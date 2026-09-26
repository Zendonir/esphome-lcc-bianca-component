//
// Created by Magnus Nordlander on 2023-10-10.
//

#ifndef SMART_LCC_OPENLCCBIANCATEXTSENSOR_H
#define SMART_LCC_OPENLCCBIANCATEXTSENSOR_H

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include <cstring>
#include <string>
#include "../esp-protocol.h"
#include "../OpenLCCBianca.h"

namespace esphome {
    namespace open_lcc_bianca {
        class OpenLCCBiancaTextSensor
                : public esphome::Component, public esphome::Parented<OpenLCCBianca> {
        public:
            void setup() {
                get_parent()->add_on_status_update_callback([this](ESPSystemStatusMessage msg) { this->handleStatus(msg); });
            }

            void handleStatus(ESPSystemStatusMessage message) {
                auto stateString = prettifyCoalescedStateString(message.coalescedState);
                if (status_ != nullptr && (!status_->has_state() || status_->state != stateString)) {
                    status_->publish_state(stateString);
                }

                if (rp2040_firmware_version_ != nullptr) {
                    // Firmware older than v1.0.2 does not send a version, the field is then all zero
                    std::string version(message.firmwareVersion,
                                        strnlen(message.firmwareVersion, sizeof(message.firmwareVersion)));
                    if (version.empty()) {
                        version = "älter als v1.0.2";
                    }
                    if (!rp2040_firmware_version_->has_state() || rp2040_firmware_version_->state != version) {
                        rp2040_firmware_version_->publish_state(version);
                    }
                }
            }

            std::string prettifyCoalescedStateString(ESPSystemCoalescedState state) {
                switch (state) {
                    case ESP_SYSTEM_COALESCED_STATE_UNDETERMINED:
                        return "Undetermined";
                    case ESP_SYSTEM_COALESCED_STATE_HEATUP:
                        return "Aufheizen";
                    case ESP_SYSTEM_COALESCED_STATE_TEMPS_NORMALIZING:
                        return "Normalisieren";
                    case ESP_SYSTEM_COALESCED_STATE_WARM:
                        return "Betriebsbereit";
                    case ESP_SYSTEM_COALESCED_STATE_SLEEPING:
                        return "Schlafmodus";
                    case ESP_SYSTEM_COALESCED_STATE_STANDBY:
                        return "Standby";
                    case ESP_SYSTEM_COALESCED_STATE_BAILED:
                        return "Fehler";
                    case ESP_SYSTEM_COALESCED_STATE_FIRST_RUN:
                        return "Erststart";
                }

                return "Unknown";
            }

            void set_status(esphome::text_sensor::TextSensor *status) { status_ = status; }
            void set_rp2040_firmware_version(esphome::text_sensor::TextSensor *sens) { rp2040_firmware_version_ = sens; }
        protected:
            esphome::text_sensor::TextSensor *status_{nullptr};
            esphome::text_sensor::TextSensor *rp2040_firmware_version_{nullptr};
        };
    }
}

#endif //SMART_LCC_OPENLCCBIANCATEXTSENSOR_H
