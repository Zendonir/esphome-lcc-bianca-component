#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../open_lcc_bianca/OpenLCCBianca.h"
#include "rp2040_flasher.h"

namespace esphome {
namespace open_lcc_rp2040_updater {

// Downloads RP2040 firmware (smart_lcc_app.bin) over HTTP(S) and flashes it through the serial bootloader.
// Downloading is a separate step, so the RP2040 is only touched once a valid image is in memory.
class OpenLCCRp2040Updater : public Component, public uart::UARTDevice {
 public:
  void set_http_request(http_request::HttpRequestComponent *http_request) { this->http_request_ = http_request; }
  void set_bianca(open_lcc_bianca::OpenLCCBianca *bianca) { this->bianca_ = bianca; }
  void set_url(const std::string &url) { this->url_ = url; }
  void set_status_sensor(text_sensor::TextSensor *status) { this->status_ = status; }

  void setup() override { this->publish_status_("Bereit"); }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Blocking, a few seconds
  bool download();
  // Blocking, around 15 seconds. The RP2040 has to be in the serial bootloader already.
  bool flash();

  bool is_downloaded() const { return this->image_ != nullptr; }
  void release_image();

 protected:
  void publish_status_(const std::string &status);
  bool read_exact_(uint8_t *data, size_t len, uint32_t timeout_ms);
  void flush_input_();

  http_request::HttpRequestComponent *http_request_{nullptr};
  open_lcc_bianca::OpenLCCBianca *bianca_{nullptr};
  text_sensor::TextSensor *status_{nullptr};
  std::string url_;

  uint8_t *image_{nullptr};
  size_t image_len_{0};
};

template<typename... Ts> class DownloadAction : public Action<Ts...>, public Parented<OpenLCCRp2040Updater> {
 public:
  void play(Ts... x) override { this->parent_->download(); }
};

template<typename... Ts> class FlashAction : public Action<Ts...>, public Parented<OpenLCCRp2040Updater> {
 public:
  void play(Ts... x) override { this->parent_->flash(); }
};

template<typename... Ts> class IsDownloadedCondition : public Condition<Ts...>, public Parented<OpenLCCRp2040Updater> {
 public:
  bool check(Ts... x) override { return this->parent_->is_downloaded(); }
};

}  // namespace open_lcc_rp2040_updater
}  // namespace esphome
