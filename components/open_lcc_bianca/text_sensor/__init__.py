import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import open_lcc_bianca_ns, OpenLCCBianca

CONF_OPEN_LCC_BIANCA_ID = "open_lcc_bianca_id"

CONF_STATUS = "status"
CONF_RP2040_FIRMWARE_VERSION = "rp2040_firmware_version"
CONF_BAIL_REASON = "bail_reason"
CONF_CONTROLLER_STATE = "controller_state"

OpenLCCBiancaSensor = open_lcc_bianca_ns.class_(
    "OpenLCCBiancaTextSensor",
    cg.Component,
    cg.Parented.template(OpenLCCBianca),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(OpenLCCBiancaSensor),
        cv.GenerateID(CONF_OPEN_LCC_BIANCA_ID): cv.use_id(OpenLCCBianca),
        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:information-outline",
        ),
        cv.Optional(CONF_BAIL_REASON): text_sensor.text_sensor_schema(
            icon="mdi:alert-octagon",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_CONTROLLER_STATE): text_sensor.text_sensor_schema(
            icon="mdi:state-machine",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_RP2040_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category="diagnostic",
        ),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_OPEN_LCC_BIANCA_ID])

    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status(sens))

    if version_config := config.get(CONF_RP2040_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(var.set_rp2040_firmware_version(sens))

    for key in (CONF_BAIL_REASON, CONF_CONTROLLER_STATE):
        if diag_conf := config.get(key):
            sens = await text_sensor.new_text_sensor(diag_conf)
            cg.add(getattr(var, f"set_{key}")(sens))
