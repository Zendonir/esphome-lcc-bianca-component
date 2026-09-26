import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import http_request, text_sensor, uart
from esphome.const import CONF_ID, CONF_URL

from ..open_lcc_bianca import OpenLCCBianca

DEPENDENCIES = ["uart", "http_request", "open_lcc_bianca"]
AUTO_LOAD = ["text_sensor"]

CONF_HTTP_REQUEST_ID = "http_request_id"
CONF_OPEN_LCC_BIANCA_ID = "open_lcc_bianca_id"
CONF_STATUS = "status"

DEFAULT_URL = "https://github.com/Zendonir/open-lcc-rp2040-bianca/releases/latest/download/smart_lcc_app.bin"

ns = cg.esphome_ns.namespace("open_lcc_rp2040_updater")
OpenLCCRp2040Updater = ns.class_("OpenLCCRp2040Updater", cg.Component, uart.UARTDevice)
DownloadAction = ns.class_("DownloadAction", automation.Action)
FlashAction = ns.class_("FlashAction", automation.Action)
IsDownloadedCondition = ns.class_("IsDownloadedCondition", automation.Condition)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenLCCRp2040Updater),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(http_request.HttpRequestComponent),
            cv.GenerateID(CONF_OPEN_LCC_BIANCA_ID): cv.use_id(OpenLCCBianca),
            cv.Optional(CONF_URL, default=DEFAULT_URL): cv.url,
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(icon="mdi:chip"),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    http = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    cg.add(var.set_http_request(http))
    bianca = await cg.get_variable(config[CONF_OPEN_LCC_BIANCA_ID])
    cg.add(var.set_bianca(bianca))
    cg.add(var.set_url(config[CONF_URL]))

    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_sensor(sens))


ACTION_SCHEMA = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(OpenLCCRp2040Updater)})


@automation.register_action("open_lcc_rp2040_updater.download", DownloadAction, ACTION_SCHEMA)
@automation.register_action("open_lcc_rp2040_updater.flash", FlashAction, ACTION_SCHEMA)
async def updater_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_condition("open_lcc_rp2040_updater.is_downloaded", IsDownloadedCondition, ACTION_SCHEMA)
async def updater_condition_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
