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
CONF_ON_SWIPE_UP = "on_swipe_up"
CONF_ON_SWIPE_DOWN = "on_swipe_down"
CONF_ON_TWO_FINGER = "on_two_finger"
CONF_DOUBLE_TAP_WINDOW = "double_tap_window"
CONF_HOLD_TIMEOUT = "hold_timeout"
CONF_MIN_POSITION = "min_position"
CONF_MAX_POSITION = "max_position"

# The touch strip reports finger positions 1..12 across its width.
MAX_POSITION = 12
POSITIONS_PER_ZONE = 3

tx_ultimate_ns = cg.esphome_ns.namespace("tx_ultimate")
TxUltimate = tx_ultimate_ns.class_("TxUltimate", cg.Component, uart.UARTDevice)


def _validate_zone(conf):
    """min_position must not be above max_position when both are given."""
    lo = conf.get(CONF_MIN_POSITION)
    hi = conf.get(CONF_MAX_POSITION)
    if lo is not None and hi is not None and lo > hi:
        raise cv.Invalid(
            f"{CONF_MIN_POSITION} ({lo}) must not be greater than "
            f"{CONF_MAX_POSITION} ({hi})"
        )
    return conf


ZONE_SCHEMA = cv.All(
    cv.Schema(
        {
            # Which raw finger positions (1-12) count as this zone. Defaults to
            # an even three-position split. Shift these if the touch areas do
            # not line up with the printed buttons.
            cv.Optional(CONF_MIN_POSITION): cv.int_range(min=1, max=MAX_POSITION),
            cv.Optional(CONF_MAX_POSITION): cv.int_range(min=1, max=MAX_POSITION),
            cv.Optional(CONF_ON_TAP): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_HOLD): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation(single=True),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _validate_zone,
)


def _validate_double_tap(config):
    """Warn loudly if double-tap handlers exist but detection is switched off."""
    if config[CONF_DOUBLE_TAP_WINDOW].total_milliseconds != 0:
        return config
    for i, zone in enumerate(config.get(CONF_ZONES, [])):
        if CONF_ON_DOUBLE_TAP in zone:
            raise cv.Invalid(
                f"zone {i + 1} defines {CONF_ON_DOUBLE_TAP}, but "
                f"{CONF_DOUBLE_TAP_WINDOW} is 0 so double taps are never "
                f"detected. Set a non-zero window or remove the handler."
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TxUltimate),
            cv.Optional(CONF_ZONES): cv.ensure_list(ZONE_SCHEMA),
            # 0ms disables double-tap detection. A single tap then fires on
            # release instead of waiting out the window, which removes that
            # much latency from every press.
            cv.Optional(
                CONF_DOUBLE_TAP_WINDOW, default="200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_HOLD_TIMEOUT, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_SWIPE_UP): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_SWIPE_DOWN): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_TWO_FINGER): automation.validate_automation(single=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_double_tap,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    zones = config.get(CONF_ZONES, [])
    num_zones = len(zones) if zones else 4
    # set_num_zones allocates Trigger objects and fills the default position
    # ranges, so it must run before anything below overrides them.
    cg.add(var.set_num_zones(num_zones))

    cg.add(var.set_double_tap_window(config[CONF_DOUBLE_TAP_WINDOW].total_milliseconds))
    cg.add(var.set_hold_timeout(config[CONF_HOLD_TIMEOUT].total_milliseconds))

    for i, zone_conf in enumerate(zones):
        # Only emit a call when the YAML actually overrides a bound, so the
        # C++ defaults stay the single source of truth otherwise.
        if CONF_MIN_POSITION in zone_conf or CONF_MAX_POSITION in zone_conf:
            lo = zone_conf.get(CONF_MIN_POSITION, i * POSITIONS_PER_ZONE + 1)
            hi = zone_conf.get(
                CONF_MAX_POSITION, i * POSITIONS_PER_ZONE + POSITIONS_PER_ZONE
            )
            cg.add(var.set_zone_positions(i, lo, hi))

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

    if CONF_ON_SWIPE_UP in config:
        await automation.build_automation(
            var.get_on_swipe_up_trigger(), [], config[CONF_ON_SWIPE_UP]
        )
    if CONF_ON_SWIPE_DOWN in config:
        await automation.build_automation(
            var.get_on_swipe_down_trigger(), [], config[CONF_ON_SWIPE_DOWN]
        )
    if CONF_ON_TWO_FINGER in config:
        await automation.build_automation(
            var.get_on_two_finger_trigger(), [], config[CONF_ON_TWO_FINGER]
        )
