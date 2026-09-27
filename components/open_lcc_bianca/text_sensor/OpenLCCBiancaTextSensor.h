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

                publish_if_changed_(bail_reason_, bailReasonString(message.bailReason));
                publish_if_changed_(controller_state_, controllerStateString(message.internalState, message.runState));

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

            static std::string bailReasonString(uint8_t reason) {
                // SystemControllerBailReason in the RP2040 firmware
                switch (reason) {
                    case 0: return "Kein Fehler";
                    case 1: return "Control Board antwortet nicht";
                    case 2: return "Ungültiges Paket vom Control Board";
                    case 3: return "Ungültiges Paket an das Control Board";
                    case 4: return "Heizungssteuerung ohne Daten";
                    case 5: return "Erzwungen (z. B. OTA-Update)";
                }
                return "Unbekannt (" + std::to_string(reason) + ")";
            }

            static std::string controllerStateString(ESPSystemInternalState internal, ESPSystemRunState run) {
                switch (internal) {
                    case ESP_SYSTEM_INTERNAL_STATE_NOT_STARTED_YET:
                        return "Nicht gestartet";
                    case ESP_SYSTEM_INTERNAL_STATE_SOFT_BAIL:
                        return "Sicherheitsabschaltung (wird automatisch aufgehoben)";
                    case ESP_SYSTEM_INTERNAL_STATE_HARD_BAIL:
                        return "Sicherheitsabschaltung (Neustart nötig)";
                    case ESP_SYSTEM_INTERNAL_STATE_RUNNING:
                        break;
                }
                switch (run) {
                    case ESP_SYSTEM_RUN_STATE_UNDETEMINED: return "Läuft: unbestimmt";
                    case ESP_SYSTEM_RUN_STATE_NORMAL: return "Läuft: normal";
                    case ESP_SYSTEM_RUN_STATE_HEATUP_STAGE_1: return "Läuft: Aufheizen Stufe 1";
                    case ESP_SYSTEM_RUN_STATE_HEATUP_STAGE_2: return "Läuft: Aufheizen Stufe 2";
                    case ESP_SYSTEM_RUN_STATE_FIRST_RUN: return "Läuft: Erststart";
                }
                return "Läuft";
            }

            void set_status(esphome::text_sensor::TextSensor *status) { status_ = status; }
            void set_bail_reason(esphome::text_sensor::TextSensor *sens) { bail_reason_ = sens; }
            void set_controller_state(esphome::text_sensor::TextSensor *sens) { controller_state_ = sens; }
            void set_rp2040_firmware_version(esphome::text_sensor::TextSensor *sens) { rp2040_firmware_version_ = sens; }
        protected:
            esphome::text_sensor::TextSensor *status_{nullptr};
            esphome::text_sensor::TextSensor *rp2040_firmware_version_{nullptr};
            esphome::text_sensor::TextSensor *bail_reason_{nullptr};
            esphome::text_sensor::TextSensor *controller_state_{nullptr};

            static void publish_if_changed_(esphome::text_sensor::TextSensor *sens, const std::string &value) {
                if (sens != nullptr && (!sens->has_state() || sens->state != value)) {
                    sens->publish_state(value);
                }
            }
        };
    }
}

#endif //SMART_LCC_OPENLCCBIANCATEXTSENSOR_H
