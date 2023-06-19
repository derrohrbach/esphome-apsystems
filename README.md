# ESPHome APsystems ECU Component

This repository adds support for inverters from APsystems to [ESPHome](https://esphome.io). For the communication with the inverters you need to connect a cc2530 or a cc2531 zigbee module with a custom firmware.

## Thanks

First of all I need to thank some people for their work in making this project possible:
- [@patience4711](https://github.com/patience4711) for their work on various projects related to communication with APsystems inverters. Especially on [ESP32-read-APS-inverters](https://github.com/patience4711/ESP32-read-APS-inverters) which served as a base for this project
- [@mampfes](https://github.com/mampfes) for their excellent esphome [d0_obis](https://github.com/mampfes/esphome_obis_d0) custom_component, which also served as a base for this project.

## Configuration

This component can easily be added as an [external component](https://esphome.io/components/external_components.html) to your ESPHome configuration.

Communication with the zigbee coordinator is done using UART, so you need to configure the [UART bus](https://esphome.io/components/uart.html#uart). You also need to configure the time component, which is used to reset the daily energy at midnight.

Additionally you need to flash a custom firmware to the zigbee coordinator. For details see the next chapters.

```yaml
external_components:
  - source: github://derrohrbach/esphome-apsystems

time:
  - platform: homeassistant

uart:
  rx_pin: 16
  tx_pin: 17
  baud_rate: 115200

apsystems:
  id: aps1
  coordinator_reset_pin: 5
  restore: true
  update_interval: 15s
  #auto_pair: false

sensor:
  - platform: apsystems
    serial: "[YOUR INVERTER SERIAL]"
    type: yc600
    #pair_id: "3CA1" #can be used if restore is set to false. pair id will be logged when pairing the inverter
    panels: 
      connected: [true, true] #can have up to 4 entries
      power: 
        name: "Solar Leistung Panel"
      energy:
        name: "Solar Energie Panel"
    power: 
      name: "Solar Leistung Gesamt"
    dc_power: 
      name: "Solar DC Leistung Gesamt"
    energy: 
      name: "Solar Energie Gesamt"
    temperature:
      name: "Solar Inverter Temperatur"

# Enable Home Assistant API
api:
  services:
    - service: pair_inverter    
      variables:
        serial: string
      then:
        - apsystems.pair_inverter:
            id: aps1
            serial: !lambda 'return serial;'
    - service: poll_inverter    
      variables:
        serial: string
      then:
        - apsystems.poll_inverter:
            id: aps1
            serial: !lambda 'return serial;'
```

## Configuration Variables

### APsystems platform

- **id** (Optional, [ID](https://esphome.io/guides/configuration-types.html#config-id)): Manually specify the ID used for code generation.
- **uart_id** (Optional, [ID](https://esphome.io/guides/configuration-types.html#config-id)): ID of the [UART Component](https://esphome.io/components/uart.html#uart) if you want to use multiple UART buses.
- **update_interval** (Optional, string): How often the inverters should be polled
- **coordinator_reset_pin** (Required, Pin): Pin which is connected to the reset pin of the zigbee coordinator
- **restore** (Optional, bool): Specifies whether the daily energy production and inverter pair ids should be saved to the esp storage
- **auto_pair** (Optional, bool): Specified if unpaired inverter should be automaticcally paired on first boot. Otherwise use the apsystems.pair_inverter command

### Sensor

- **serial** (Required, string): Serial number of your inverter (12 digits 0-9)
- **type** (Required, string): Type of your inverter. Can be: "yc600", "qs1", "ds3"
- **pair_id** (Optional, string): Pairing code of your inverter. Can be found in the log after pairing the inverter. Not neccessary if **restore** is on, since it is automatically saved to flash
- **panels** (Required, object): Connected panels and per panel sensors
  - **connected** (Required, bool[]): Array of booleans. `[true, false, true, false]` means panels 1 and 3 are connected.
  - **energy** (Optional, Sensor): Configuration of ac energy sensor
  - **power** (Optional, Sensor): Configuration of ac power sensor
  - **dc_power** (Optional, Sensor): Configuration of dc power sensor
  - **dc_voltage** (Optional, Sensor): Configuration of dc voltage sensor
  - **dc_current** (Optional, Sensor): Configuration of dc current sensor
- **energy** (Optional, Sensor): Configuration of ac energy sensor
- **power** (Optional, Sensor): Configuration of ac power sensor
- **voltage** (Optional, Sensor): Configuration of ac voltage sensor
- **frequency** (Optional, Sensor): Configuration of ac frequency sensor
- **signal_strength** (Optional, Sensor): Configuration of rf signal strength percent sensor
- **dc_power** (Optional, Sensor): Configuration of dc power sensor
---

## Hardware

A zigbee coordinator with custom firmware is required to use this component. Please refer to the documentation of [ESP32-read-APS-inverters](https://github.com/patience4711/ESP32-read-APS-inverters) on how to flash this coordinator and connect it to your esp.

## Note!
 
This was only tested with the following setups. If you can get it running on other setups then please let me know!

- @derrohrbach
  - 1x YC600
  - ESP32 and ESP8266
  - cc2531 ZigBee Module flashed using ccloader
