import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, font, number, switch
from esphome.const import CONF_ID, CONF_TIMEOUT, CONF_TRIGGER_ID

DEPENDENCIES = ["display"]
AUTO_LOAD = ["binary_sensor", "number", "switch"]

CONF_MINUS_BUTTON = "minus_button"
CONF_PLUS_BUTTON = "plus_button"
CONF_FONT = "font"
CONF_SMALL_FONT = "small_font"
CONF_VALUE_FONT = "value_font"
CONF_VISIBLE_AREA = "visible_area"
CONF_LEFT = "left"
CONF_TOP = "top"
CONF_RIGHT = "right"
CONF_BOTTOM = "bottom"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_HOME_LONG_MINUS_TIME = "home_long_minus_time"
CONF_ON_HOME_MINUS = "on_home_minus"
CONF_ON_HOME_PLUS = "on_home_plus"
CONF_ON_HOME_LONG_MINUS = "on_home_long_minus"
CONF_PAGES = "pages"
CONF_TITLE = "title"
CONF_ITEMS = "items"
CONF_LABEL = "label"
CONF_NUMBER = "number"
CONF_SWITCH = "switch"
CONF_TEXT = "text"
CONF_ACTION = "action"
CONF_STEP = "step"
CONF_FORMAT = "format"
CONF_CONFIRM = "confirm"

ns = cg.esphome_ns.namespace("open_lcc_menu")
OpenLCCMenu = ns.class_("OpenLCCMenu", cg.Component)


def _validate_item(value):
    kinds = [k for k in (CONF_NUMBER, CONF_SWITCH, CONF_TEXT, CONF_ACTION) if k in value]
    if len(kinds) != 1:
        raise cv.Invalid("Each menu item needs exactly one of number, switch, text or action")
    return value


ITEM_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_LABEL): cv.string,
            cv.Optional(CONF_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_STEP): cv.positive_float,
            cv.Optional(CONF_FORMAT): cv.string,
            cv.Optional(CONF_SWITCH): cv.use_id(switch.Switch),
            cv.Optional(CONF_TEXT): cv.returning_lambda,
            cv.Optional(CONF_ACTION): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(automation.Trigger.template())},
                single=True,
            ),
            cv.Optional(CONF_CONFIRM, default=False): cv.boolean,
        }
    ),
    _validate_item,
)

PAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TITLE): cv.string,
        cv.Required(CONF_ITEMS): cv.ensure_list(ITEM_SCHEMA),
    }
)

HOME_TRIGGER_SCHEMA = automation.validate_automation(
    {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(automation.Trigger.template())}
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OpenLCCMenu),
        cv.Required(CONF_MINUS_BUTTON): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_PLUS_BUTTON): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_FONT): cv.use_id(font.Font),
        cv.Required(CONF_SMALL_FONT): cv.use_id(font.Font),
        cv.Required(CONF_VALUE_FONT): cv.use_id(font.Font),
        # Pixels hidden behind the front panel on each side (display coordinates after rotation)
        cv.Optional(CONF_VISIBLE_AREA, default={}): cv.Schema(
            {
                cv.Optional(CONF_LEFT, default=36): cv.int_range(min=0, max=200),
                cv.Optional(CONF_TOP, default=20): cv.int_range(min=0, max=200),
                cv.Optional(CONF_RIGHT, default=30): cv.int_range(min=0, max=200),
                cv.Optional(CONF_BOTTOM, default=16): cv.int_range(min=0, max=200),
            }
        ),
        cv.Optional(CONF_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LONG_PRESS_TIME, default="1s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HOME_LONG_MINUS_TIME, default="3s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ON_HOME_MINUS): HOME_TRIGGER_SCHEMA,
        cv.Optional(CONF_ON_HOME_PLUS): HOME_TRIGGER_SCHEMA,
        cv.Optional(CONF_ON_HOME_LONG_MINUS): HOME_TRIGGER_SCHEMA,
        cv.Required(CONF_PAGES): cv.ensure_list(PAGE_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    minus = await cg.get_variable(config[CONF_MINUS_BUTTON])
    plus = await cg.get_variable(config[CONF_PLUS_BUTTON])
    cg.add(var.set_buttons(minus, plus))

    font_ = await cg.get_variable(config[CONF_FONT])
    small_font = await cg.get_variable(config[CONF_SMALL_FONT])
    value_font = await cg.get_variable(config[CONF_VALUE_FONT])
    cg.add(var.set_fonts(font_, small_font, value_font))

    area = config[CONF_VISIBLE_AREA]
    cg.add(var.set_margins(area[CONF_LEFT], area[CONF_TOP], area[CONF_RIGHT], area[CONF_BOTTOM]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_long_press_time(config[CONF_LONG_PRESS_TIME]))
    cg.add(var.set_home_long_minus_time(config[CONF_HOME_LONG_MINUS_TIME]))

    for key, getter in (
        (CONF_ON_HOME_MINUS, var.get_home_minus_trigger()),
        (CONF_ON_HOME_PLUS, var.get_home_plus_trigger()),
        (CONF_ON_HOME_LONG_MINUS, var.get_home_long_minus_trigger()),
    ):
        for conf in config.get(key, []):
            await automation.build_automation(getter, [], conf)

    # Pages are added in order, so the page index is the position in the list
    for page_var, page_conf in enumerate(config[CONF_PAGES]):
        cg.add(var.add_page(page_conf[CONF_TITLE]))
        for item in page_conf[CONF_ITEMS]:
            label = item[CONF_LABEL]
            if CONF_NUMBER in item:
                num = await cg.get_variable(item[CONF_NUMBER])
                cg.add(
                    var.add_number_item(
                        page_var, label, num, item.get(CONF_STEP, 0.0), item.get(CONF_FORMAT, "")
                    )
                )
            elif CONF_SWITCH in item:
                sw = await cg.get_variable(item[CONF_SWITCH])
                cg.add(var.add_switch_item(page_var, label, sw))
            elif CONF_TEXT in item:
                text = await cg.process_lambda(item[CONF_TEXT], [], return_type=cg.std_string)
                cg.add(var.add_text_item(page_var, label, text))
            else:
                action_conf = item[CONF_ACTION]
                trigger = cg.new_Pvariable(action_conf[CONF_TRIGGER_ID])
                await automation.build_automation(trigger, [], action_conf)
                cg.add(var.add_action_item(page_var, label, trigger, item[CONF_CONFIRM]))
