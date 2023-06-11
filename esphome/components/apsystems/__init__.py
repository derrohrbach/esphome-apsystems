from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, time
from esphome.const import CONF_ID, CONF_RESTORE, CONF_TIME_ID
from esphome import automation

CODEOWNERS = ["@derrohrbach"]

DEPENDENCIES = ["uart", "time"]

CONF_AUTO_PAIR = "auto_pair"
CONF_COORDINATOR_RESET_PIN = "coordinator_reset_pin"
CONF_SERIAL = "serial"

apsystems_ns = cg.esphome_ns.namespace("apsystems")
Apsystems = apsystems_ns.class_("Apsystems", cg.Component, uart.UARTDevice)
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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Apsystems),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_RESTORE, False): cv.boolean,
            cv.Optional(CONF_AUTO_PAIR, True): cv.boolean,
            cv.Required(CONF_COORDINATOR_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("5min"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_restore(config[CONF_RESTORE]))
    cg.add(var.set_auto_pair(config[CONF_AUTO_PAIR]))

    reset_pin = await cg.gpio_pin_expression(config[CONF_COORDINATOR_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))


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
