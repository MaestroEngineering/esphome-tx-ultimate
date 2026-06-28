import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.automation as automation
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CONF_ZONES = "zones"
CONF_ON_TAP = "on_tap"
CONF_ON_HOLD = "on_hold"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_SWIPE_LEFT = "on_swipe_left"
CONF_ON_SWIPE_RIGHT = "on_swipe_right"
CONF_ON_TWO_FINGER = "on_two_finger"

tx_ultimate_ns = cg.esphome_ns.namespace("tx_ultimate")
TxUltimate = tx_ultimate_ns.class_("TxUltimate", cg.Component, uart.UARTDevice)

ZONE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_TAP): automation.validate_automation(single=False),
        cv.Optional(CONF_ON_HOLD): automation.validate_automation(single=False),
        cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation(single=False),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TxUltimate),
            cv.Optional(CONF_ZONES): cv.All(
                cv.ensure_list, [ZONE_SCHEMA], cv.Length(min=1, max=4)
            ),
            cv.Optional(CONF_ON_SWIPE_LEFT): automation.validate_automation(),
            cv.Optional(CONF_ON_SWIPE_RIGHT): automation.validate_automation(),
            cv.Optional(CONF_ON_TWO_FINGER): automation.validate_automation(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    zones = config.get(CONF_ZONES, [])
    num_zones = len(zones) if zones else 4
    # set_num_zones allocates Trigger objects before automations are wired below
    cg.add(var.set_num_zones(num_zones))

    for i, zone_conf in enumerate(zones):
        if CONF_ON_TAP in zone_conf:
            await automation.build_automation(
                var.get_on_tap_trigger(i), [], zone_conf[CONF_ON_TAP]
            )
        if CONF_ON_HOLD in zone_conf:
            await automation.build_automation(
                var.get_on_hold_trigger(i), [], zone_conf[CONF_ON_HOLD]
            )
        if CONF_ON_DOUBLE_TAP in zone_conf:
            await automation.build_automation(
                var.get_on_double_tap_trigger(i), [], zone_conf[CONF_ON_DOUBLE_TAP]
            )

    if CONF_ON_SWIPE_LEFT in config:
        await automation.build_automation(
            var.get_on_swipe_left_trigger(), [], config[CONF_ON_SWIPE_LEFT]
        )
    if CONF_ON_SWIPE_RIGHT in config:
        await automation.build_automation(
            var.get_on_swipe_right_trigger(), [], config[CONF_ON_SWIPE_RIGHT]
        )
    if CONF_ON_TWO_FINGER in config:
        await automation.build_automation(
            var.get_on_two_finger_trigger(), [], config[CONF_ON_TWO_FINGER]
        )
