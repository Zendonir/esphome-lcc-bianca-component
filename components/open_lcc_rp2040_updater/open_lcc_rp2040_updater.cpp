#include "open_lcc_rp2040_updater.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace open_lcc_rp2040_updater {

static const char *const TAG = "rp2040_updater";

// Plenty for the ~110 KiB firmware, protects against downloading something unexpected
static constexpr size_t MAX_IMAGE_SIZE = 1024 * 1024;

void OpenLCCRp2040Updater::dump_config() {
  ESP_LOGCONFIG(TAG, "Open LCC RP2040 updater:");
  ESP_LOGCONFIG(TAG, "  URL: %s", this->url_.c_str());
}

void OpenLCCRp2040Updater::publish_status_(const std::string &status) {
  ESP_LOGI(TAG, "%s", status.c_str());
  if (this->status_ != nullptr)
    this->status_->publish_state(status);
}

void OpenLCCRp2040Updater::release_image() {
  if (this->image_ != nullptr) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->image_, this->image_len_);
  }
  this->image_ = nullptr;
  this->image_len_ = 0;
}

bool OpenLCCRp2040Updater::download() {
  this->release_image();
  this->publish_status_("Lade Firmware herunter");

  auto container = this->http_request_->get(this->url_);
  if (container == nullptr || container->status_code != 200) {
    int code = container == nullptr ? -1 : container->status_code;
    if (container != nullptr)
      container->end();
    this->publish_status_(str_sprintf("Download fehlgeschlagen (HTTP %d)", code));
    return false;
  }

  size_t len = container->content_length;
  if (len == 0 || len > MAX_IMAGE_SIZE) {
    container->end();
    this->publish_status_(str_sprintf("Download fehlgeschlagen (Groesse %u)", (unsigned) len));
    return false;
  }

  RAMAllocator<uint8_t> allocator;
  uint8_t *buffer = allocator.allocate(len);
  if (buffer == nullptr) {
    container->end();
    this->publish_status_("Download fehlgeschlagen (kein Speicher)");
    return false;
  }

  auto result = http_request::http_read_fully(container.get(), buffer, len, 1024, this->http_request_->get_timeout());
  size_t read = container->get_bytes_read();
  container->end();

  if (result.status != http_request::HttpReadStatus::OK || read != len) {
    allocator.deallocate(buffer, len);
    this->publish_status_(str_sprintf("Download abgebrochen (%u von %u Bytes)", (unsigned) read, (unsigned) len));
    return false;
  }

  if (!rp2040_image_looks_valid(buffer, len)) {
    allocator.deallocate(buffer, len);
    this->publish_status_("Download ist keine gueltige RP2040-Firmware");
    return false;
  }

  this->image_ = buffer;
  this->image_len_ = len;
  this->publish_status_(str_sprintf("Firmware geladen (%u Bytes)", (unsigned) len));
  return true;
}

void OpenLCCRp2040Updater::flush_input_() {
  uint8_t byte;
  while (this->available() > 0)
    this->read_byte(&byte);
}

bool OpenLCCRp2040Updater::read_exact_(uint8_t *data, size_t len, uint32_t timeout_ms) {
  uint32_t start = millis();
  size_t received = 0;
  while (received < len) {
    int available = this->available();
    if (available > 0) {
      size_t n = std::min(static_cast<size_t>(available), len - received);
      if (!this->read_array(data + received, n))
        return false;
      received += n;
    } else {
      if (millis() - start > timeout_ms)
        return false;
      App.feed_wdt();
      delay(1);
    }
  }
  return true;
}

bool OpenLCCRp2040Updater::flash() {
  if (!this->is_downloaded()) {
    this->publish_status_("Keine Firmware geladen");
    return false;
  }

  this->publish_status_("Flashe RP2040");

  // The normal protocol handler must not read the bootloader answers
  if (this->bianca_ != nullptr)
    this->bianca_->disable();

  Rp2040Flasher flasher(
      [this](const uint8_t *data, size_t len) {
        this->write_array(data, len);
        this->flush();
      },
      [this](uint8_t *data, size_t len, uint32_t timeout_ms) { return this->read_exact_(data, len, timeout_ms); },
      [this]() { this->flush_input_(); });

  int last_logged = -10;
  const char *error = flasher.flash(this->image_, this->image_len_, [&last_logged](int percent) {
    App.feed_wdt();
    if (percent >= last_logged + 10) {
      ESP_LOGI(TAG, "Flashing %d%%", percent);
      last_logged = percent;
    }
  });

  this->flush_input_();
  if (this->bianca_ != nullptr)
    this->bianca_->enable();

  if (error != nullptr) {
    // Nothing is started without a sealed image, the RP2040 stays in the bootloader and it can be retried
    this->publish_status_(std::string("Fehler: ") + error + ". Bitte erneut versuchen");
    return false;
  }

  this->release_image();
  this->publish_status_("Aktualisiert");
  return true;
}

}  // namespace open_lcc_rp2040_updater
}  // namespace esphome
