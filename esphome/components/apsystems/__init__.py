import re
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_RESTORE
from esphome import automation

CODEOWNERS = ["@derrohrbach"]

DEPENDENCIES = ["uart"]

CONF_INVERTERS = "inverters"
CONF_SERIAL = "serial"
CONF_PAIR_ID = "pair_id"
CONF_COORDINATOR_RESET_PIN = "coordinator_reset_pin"

apsystems_ns = cg.esphome_ns.namespace("apsystems")
Apsystems = apsystems_ns.class_("Apsystems", cg.Component, uart.UARTDevice)
Inverter = apsystems_ns.class_("Inverter")
ApsystemsPairInverterAction = apsystems_ns.class_(
    "ApsystemsPairInverterAction", automation.Action
)
ApsystemsPollInverterAction = apsystems_ns.class_(
    "ApsystemsPollInverterAction", automation.Action
)
ApsystemsRebootInverterAction = apsystems_ns.class_(
    "ApsystemsRebootInverterAction", automation.Action
)
MULTI_CONF = True


def inverter_id(value):
    value = cv.string(value)
    match = re.match(r"^\d{12}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid APsystems inverter serial")
    return value


def pair_id(value):
    value = cv.string(value)
    match = re.match(r"^[0-F]{4}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid APsystems inverter pair id")
    return value


INVERTER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Inverter),
        cv.Required(CONF_SERIAL): inverter_id,
        cv.Optional(CONF_PAIR_ID): pair_id,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Apsystems),
        cv.Required(CONF_INVERTERS): cv.ensure_list(INVERTER_SCHEMA),
        cv.Optional(CONF_RESTORE, False): cv.boolean,
        cv.Required(CONF_COORDINATOR_RESET_PIN): pins.gpio_output_pin_schema,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_restore(config[CONF_RESTORE]))

    reset_pin = await cg.gpio_pin_expression(config[CONF_COORDINATOR_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))

    for inverter in config[CONF_INVERTERS]:
        inv = cg.new_Pvariable(inverter[CONF_ID])
        cg.add(inv.set_serial(inverter[CONF_SERIAL]))
        if CONF_PAIR_ID in inverter:
            cg.add(inv.set_id(inverter[CONF_PAIR_ID]))
        cg.add(var.add_inverter(inv))


INVERTER_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Apsystems),
        cv.Required(CONF_SERIAL): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "apsystems.pair_inverter", ApsystemsPairInverterAction, INVERTER_ACTION_SCHEMA
)
@automation.register_action(
    "apsystems.poll_inverter", ApsystemsPollInverterAction, INVERTER_ACTION_SCHEMA
)
@automation.register_action(
    "apsystems.reboot_inverter", ApsystemsRebootInverterAction, INVERTER_ACTION_SCHEMA
)
async def actions_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_SERIAL], args, cg.std_string)
    cg.add(var.set_serial(template_))
    return var
