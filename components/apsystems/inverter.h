#pragma once

#include <string>
#include <vector>
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace apsystems {

enum InverterType { INVERTER_TYPE_YC600 = 0, INVERTER_TYPE_QS1 = 1, INVERTER_TYPE_DS3 = 2 };

struct InverterPreference {
  int last_poll_timestamp;
  float energy_today[4];
  char pair_id[5];
  float energy_since_last_reset[4];
};

struct InverterData {
  int poll_timestamp{0};
  float ac_frequency{0.0f};
  float signal_quality{0.0f};
  float temperature{0.0f};
  float ac_voltage{0.0f};
  float dc_current[4]{0.0f};
  float dc_voltage[4]{0.0f};
  float dc_power[5]{0.0f};
  float ac_power[5]{0.0f};
  float energy_since_last_reset[5]{0.0f};
  float energy_today[5]{0.0f};
};

struct PanelSensors {
  sensor::Sensor *energy;
  sensor::Sensor *ac_power;
  sensor::Sensor *dc_power;
  sensor::Sensor *dc_voltage;
  sensor::Sensor *dc_current;
};

class Inverter {
 public:
  const char *get_serial();
  bool is_panel_connected(int i);
  const char *get_id();
  InverterType get_type();
  bool is_paired();
  void set_serial(std::string serial);
  void set_panel_connected(int i, bool connected);
  void set_id(std::string id);
  void set_type(InverterType type);
  void set_panel_energy_sensor(int i, sensor::Sensor *inst);
  void set_panel_ac_power_sensor(int i, sensor::Sensor *inst);
  void set_panel_dc_power_sensor(int i, sensor::Sensor *inst);
  void set_panel_dc_voltage_sensor(int i, sensor::Sensor *inst);
  void set_panel_dc_current_sensor(int i, sensor::Sensor *inst);
  void set_energy_sensor(sensor::Sensor *inst);
  void set_temperature_sensor(sensor::Sensor *inst);
  void set_ac_voltage_sensor(sensor::Sensor *inst);
  void set_ac_frequency_sensor(sensor::Sensor *inst);
  void set_signal_quality_sensor(sensor::Sensor *inst);
  void set_dc_power_sensor(sensor::Sensor *inst);
  void set_ac_power_sensor(sensor::Sensor *inst);
  int get_unsuccessfull_polls();
  void set_unsuccessfull_polls(int amount);
  void save_preferences();
  void enable_restore();
  InverterData get_data();
  void set_data(InverterData data);

 protected:
  bool restore_;
  ESPPreferenceObject pref_;
  int unsuccessfull_polls_ = 0;
  char serial_[13] = "000000000000";
  char id_[5] {0};
  InverterData data_{};
  InverterType type_ = InverterType::INVERTER_TYPE_YC600;

  PanelSensors panel_sensors_[4];

  sensor::Sensor *energy_sensor_;
  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *ac_voltage_sensor_;
  sensor::Sensor *ac_frequency_sensor_;
  sensor::Sensor *signal_quality_sensor_;
  sensor::Sensor *dc_power_sensor_;
  sensor::Sensor *ac_power_sensor_;

  bool connnected_panels_[4] = {false, false, false, false};
};

}  // namespace apsystems
}  // namespace esphome