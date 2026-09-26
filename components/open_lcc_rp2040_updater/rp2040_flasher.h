#pragma once

// Serial bootloader protocol of lib/rp2040-serial-bootloader in open-lcc-rp2040-bianca.
// Kept free of ESPHome dependencies so it can be tested on a host.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

namespace esphome {
namespace open_lcc_rp2040_updater {

static constexpr uint32_t RP2040_APP_ADDRESS = 0x10008000;
static constexpr uint32_t RP2040_SRAM_BASE = 0x20000000;
static constexpr uint32_t RP2040_SRAM_END = 0x20042000;

// Standard CRC32 (IEEE 802.3, same as zlib and the bootloader), can be fed incrementally
inline uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

// Same checks the bootloader does before it starts an image, so a broken download never gets flashed
inline bool rp2040_image_looks_valid(const uint8_t *image, size_t len) {
  if (len < 8 || len > 2 * 1024 * 1024)
    return false;
  uint32_t stack_pointer, reset_vector;
  memcpy(&stack_pointer, image, 4);
  memcpy(&reset_vector, image + 4, 4);
  if (stack_pointer < RP2040_SRAM_BASE || stack_pointer > RP2040_SRAM_END)
    return false;
  if (reset_vector < RP2040_APP_ADDRESS || reset_vector > RP2040_APP_ADDRESS + len || !(reset_vector & 1))
    return false;
  return true;
}

class Rp2040Flasher {
 public:
  // write(data, len), read(data, len, timeout_ms) -> true when all bytes arrived, flush_input()
  using WriteFn = std::function<void(const uint8_t *, size_t)>;
  using ReadFn = std::function<bool(uint8_t *, size_t, uint32_t)>;
  using FlushFn = std::function<void()>;
  using ProgressFn = std::function<void(int percent)>;

  Rp2040Flasher(WriteFn write, ReadFn read, FlushFn flush) : write_(write), read_(read), flush_(flush) {}

  // Returns nullptr on success, otherwise a short error description
  const char *flash(const uint8_t *image, size_t len, const ProgressFn &progress) {
    if (!this->sync_())
      return "Bootloader antwortet nicht";

    uint32_t info[5];
    if (!this->command_(op("INFO"), nullptr, 0, nullptr, 0, info, 5, 2000))
      return "INFO fehlgeschlagen";
    const uint32_t write_min = info[0], flash_size = info[1], erase_size = info[2], page_size = info[3],
                   max_data = info[4];
    if (page_size == 0 || erase_size == 0 || max_data < page_size)
      return "Ungueltige Bootloader-Info";
    if (RP2040_APP_ADDRESS < write_min)
      return "App-Adresse ausserhalb des Flash";

    const size_t padded_len = (len + page_size - 1) / page_size * page_size;
    if (RP2040_APP_ADDRESS + padded_len >= write_min + flash_size)
      return "Firmware zu gross";

    const uint32_t erase_len = (padded_len + erase_size - 1) / erase_size * erase_size;
    uint32_t erase_args[2] = {RP2040_APP_ADDRESS, erase_len};
    if (!this->command_(op("ERAS"), erase_args, 2, nullptr, 0, nullptr, 0, 30000))
      return "Loeschen fehlgeschlagen";

    const size_t chunk_size = max_data - max_data % page_size;
    uint8_t chunk[1024];
    const size_t use_chunk = chunk_size < sizeof(chunk) ? chunk_size : sizeof(chunk);
    uint32_t image_crc = 0;

    for (size_t offset = 0; offset < padded_len; offset += use_chunk) {
      size_t n = padded_len - offset < use_chunk ? padded_len - offset : use_chunk;
      size_t from_image = offset >= len ? 0 : (len - offset < n ? len - offset : n);
      memcpy(chunk, image + offset, from_image);
      memset(chunk + from_image, 0xff, n - from_image);  // pad like erased flash
      image_crc = crc32_update(image_crc, chunk, n);

      uint32_t write_args[2] = {static_cast<uint32_t>(RP2040_APP_ADDRESS + offset), static_cast<uint32_t>(n)};
      uint32_t remote_crc;
      if (!this->command_(op("WRIT"), write_args, 2, chunk, n, &remote_crc, 1, 3000))
        return "Schreiben fehlgeschlagen";
      if (remote_crc != crc32_update(0, chunk, n))
        return "Pruefsumme falsch";

      progress(static_cast<int>((offset + n) * 100 / padded_len));
    }

    uint32_t seal_args[3] = {RP2040_APP_ADDRESS, static_cast<uint32_t>(padded_len), image_crc};
    if (!this->command_(op("SEAL"), seal_args, 3, nullptr, 0, nullptr, 0, 5000))
      return "Versiegeln fehlgeschlagen";

    // GOGO has no response
    uint8_t go[8];
    uint32_t go_op = op("GOGO"), go_addr = RP2040_APP_ADDRESS;
    memcpy(go, &go_op, 4);
    memcpy(go + 4, &go_addr, 4);
    this->write_(go, sizeof(go));
    return nullptr;
  }

 protected:
  static uint32_t op(const char *text) {
    uint32_t value;
    memcpy(&value, text, 4);
    return value;
  }

  bool sync_() {
    for (int attempt = 0; attempt < 20; attempt++) {
      this->flush_();
      uint32_t sync = op("SYNC");
      this->write_(reinterpret_cast<const uint8_t *>(&sync), 4);
      uint32_t response;
      if (this->read_(reinterpret_cast<uint8_t *>(&response), 4, 500) && response == op("PICO"))
        return true;
    }
    return false;
  }

  bool command_(uint32_t opcode, const uint32_t *args, size_t nargs, const uint8_t *data, size_t data_len,
                uint32_t *resp_args, size_t resp_nargs, uint32_t timeout_ms) {
    this->write_(reinterpret_cast<const uint8_t *>(&opcode), 4);
    if (nargs)
      this->write_(reinterpret_cast<const uint8_t *>(args), nargs * 4);
    if (data_len)
      this->write_(data, data_len);

    uint32_t status;
    if (!this->read_(reinterpret_cast<uint8_t *>(&status), 4, timeout_ms) || status != op("OKOK"))
      return false;
    if (resp_nargs && !this->read_(reinterpret_cast<uint8_t *>(resp_args), resp_nargs * 4, timeout_ms))
      return false;
    return true;
  }

  WriteFn write_;
  ReadFn read_;
  FlushFn flush_;
};

}  // namespace open_lcc_rp2040_updater
}  // namespace esphome
