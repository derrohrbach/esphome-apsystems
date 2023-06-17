import re
from esphome.core import ID
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_ENERGY,
    CONF_NAME,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    CONF_FREQUENCY,
    CONF_SIGNAL_STRENGTH,
    CONF_POWER,
    UNIT_WATT_HOURS,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_WATT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_POWER,
    STATE_CLASS_TOTAL_INCREASING,
    STATE_CLASS_MEASUREMENT,
)
from . import CONF_SERIAL, Apsystems, apsystems_ns

DEPENDENCIES = ["apsystems"]

CONF_PAIR_ID = "pair_id"
CONF_PANELS = "panels"
CONF_CONNECTED = "connected"
CONF_DC_POWER = "dc_power"
CONF_DC_VOLTAGE = "dc_voltage"
CONF_DC_CURRENT = "dc_current"
CONF_APSYSTEMS_ID = "apsystems_id"

Inverter = apsystems_ns.class_("Inverter")

InverterType = apsystems_ns.enum("InverterType")
INVERTER_TYPES = {
    "yc600": InverterType.INVERTER_TYPE_YC600,
    "qs1": InverterType.INVERTER_TYPE_QS1,
    "ds3": InverterType.INVERTER_TYPE_DS3,
}


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


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Inverter),
        cv.GenerateID(CONF_APSYSTEMS_ID): cv.use_id(Apsystems),
        cv.Required(CONF_SERIAL): inverter_id,
        cv.Required(CONF_TYPE): cv.enum(INVERTER_TYPES, lower=True),
        cv.Required(CONF_PANELS): cv.Schema(
            {
                cv.Required(CONF_CONNECTED): cv.ensure_list(cv.boolean),
                cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT_HOURS,
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_ENERGY,
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                ),
                cv.Optional(CONF_POWER): sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_POWER,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_POWER,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
                    unit_of_measurement=UNIT_VOLT,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_DC_CURRENT): sensor.sensor_schema(
                    unit_of_measurement=UNIT_AMPERE,
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_CURRENT,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
            }
        ),
        cv.Optional(CONF_PAIR_ID): pair_id,
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


def make_panel_sensor_config(i, config):
    new_config = config.copy()
    user_idx = i + 1
    new_config[CONF_NAME] = config[CONF_NAME] + " " + str(user_idx)
    if i > 0:
        new_config[CONF_ID] = ID(
            config[CONF_ID].id + "_" + str(i),
            is_declaration=True,
            type=config[CONF_ID].type,
        )
    return new_config


async def to_code(config):
    coordinator = await cg.get_variable(config[CONF_APSYSTEMS_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_serial(config[CONF_SERIAL]))
    cg.add(var.set_type(config[CONF_TYPE]))
    panel_config = config[CONF_PANELS]
    for i, panel_state in enumerate(panel_config[CONF_CONNECTED]):
        cg.add(var.set_panel_connected(i, panel_state))
    if CONF_PAIR_ID in config:
        cg.add(var.set_id(config[CONF_PAIR_ID]))
    cg.add(coordinator.add_inverter(var))

    if CONF_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY])
        cg.add(var.set_energy_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_ac_voltage_sensor(sens))
    if CONF_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_ac_frequency_sensor(sens))
    if CONF_SIGNAL_STRENGTH in config:
        sens = await sensor.new_sensor(config[CONF_SIGNAL_STRENGTH])
        cg.add(var.set_signal_quality_sensor(sens))
    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_ac_power_sensor(sens))
    if CONF_DC_POWER in config:
        sens = await sensor.new_sensor(config[CONF_DC_POWER])
        cg.add(var.set_dc_power_sensor(sens))

    for i in range(0, 4):
        if i < len(panel_config[CONF_CONNECTED]) and panel_config[CONF_CONNECTED][i]:
            if CONF_ENERGY in panel_config:
                sens = await sensor.new_sensor(
                    make_panel_sensor_config(i, panel_config[CONF_ENERGY])
                )
                cg.add(var.set_panel_energy_sensor(i, sens))
            if CONF_POWER in panel_config:
                sens = await sensor.new_sensor(
                    make_panel_sensor_config(i, panel_config[CONF_POWER])
                )
                cg.add(var.set_panel_ac_power_sensor(i, sens))
            if CONF_DC_POWER in panel_config:
                sens = await sensor.new_sensor(
                    make_panel_sensor_config(i, panel_config[CONF_DC_POWER])
                )
                cg.add(var.set_panel_dc_power_sensor(i, sens))
            if CONF_DC_VOLTAGE in panel_config:
                sens = await sensor.new_sensor(
                    make_panel_sensor_config(i, panel_config[CONF_DC_VOLTAGE])
                )
                cg.add(var.set_panel_dc_voltage_sensor(i, sens))
            if CONF_DC_CURRENT in panel_config:
                sens = await sensor.new_sensor(
                    make_panel_sensor_config(i, panel_config[CONF_DC_CURRENT])
                )
                cg.add(var.set_panel_dc_current_sensor(i, sens))
