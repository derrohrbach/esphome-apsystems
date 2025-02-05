#include "inverter.h"
#include "esphome/core/log.h"

static const char *const TAG = "apsystems.inverter";

namespace esphome {
namespace apsystems {

void Inverter::set_panel_connected(int i, bool connected) {
  if (i >= 0 && i <= 3)
    connnected_panels_[i] = connected;
}

void Inverter::set_serial(std::string serial) { serial.copy(serial_, 12, 0); }

void Inverter::set_id(std::string id) {
  id.copy(id_, 4, 0);
  save_preferences();
}

void Inverter::set_type(InverterType type) { type_ = type; };

InverterType Inverter::get_type() { return type_; }

const char *Inverter::get_serial() { return serial_; }

const char *Inverter::get_id() { return id_; }

int Inverter::get_unsuccessfull_polls() { return unsuccessfull_polls_; }
void Inverter::set_unsuccessfull_polls(int amount) { unsuccessfull_polls_ = amount; }

void Inverter::set_panel_energy_sensor(int i, sensor::Sensor *inst) { panel_sensors_[i].energy = inst; }
void Inverter::set_panel_ac_power_sensor(int i, sensor::Sensor *inst) { panel_sensors_[i].ac_power = inst; }
void Inverter::set_panel_dc_power_sensor(int i, sensor::Sensor *inst) { panel_sensors_[i].dc_power = inst; }
void Inverter::set_panel_dc_voltage_sensor(int i, sensor::Sensor *inst) { panel_sensors_[i].dc_voltage = inst; }
void Inverter::set_panel_dc_current_sensor(int i, sensor::Sensor *inst) { panel_sensors_[i].dc_current = inst; }

void Inverter::set_energy_sensor(sensor::Sensor *inst) { energy_sensor_ = inst; }
void Inverter::set_temperature_sensor(sensor::Sensor *inst) { temperature_sensor_ = inst; }
void Inverter::set_ac_voltage_sensor(sensor::Sensor *inst) { ac_voltage_sensor_ = inst; }
void Inverter::set_ac_frequency_sensor(sensor::Sensor *inst) { ac_frequency_sensor_ = inst; }
void Inverter::set_signal_quality_sensor(sensor::Sensor *inst) { signal_quality_sensor_ = inst; }
void Inverter::set_dc_power_sensor(sensor::Sensor *inst) { dc_power_sensor_ = inst; }
void Inverter::set_ac_power_sensor(sensor::Sensor *inst) { ac_power_sensor_ = inst; }

InverterData Inverter::get_data() { return data_; }

void publish_state(sensor::Sensor *sensor, float state, bool nanIs0) {
  if (sensor == nullptr)
    return;
  if (std::isnan(state) && nanIs0) {
    sensor->publish_state(0);
  } else {
    sensor->publish_state(state);
  }
}

void Inverter::set_data(InverterData data) {
  data_ = data;
  save_preferences();
  for (int i = 0; i < 4; i++) {
    if (is_panel_connected(i)) {
      publish_state(panel_sensors_[i].energy, data_.energy_today[i], false);
      publish_state(panel_sensors_[i].ac_power, data_.ac_power[i], true);
      publish_state(panel_sensors_[i].dc_power, data_.dc_power[i], true);
      publish_state(panel_sensors_[i].dc_voltage, data_.dc_voltage[i], false);
      publish_state(panel_sensors_[i].dc_current, data_.dc_current[i], false);
    }
  }
  publish_state(energy_sensor_, data_.energy_today[4], false);
  publish_state(temperature_sensor_, data_.temperature, false);
  publish_state(ac_voltage_sensor_, data_.ac_voltage, false);
  publish_state(ac_frequency_sensor_, data_.ac_frequency, false);
  publish_state(signal_quality_sensor_, data_.signal_quality, false);
  publish_state(dc_power_sensor_, data_.dc_power[4], true);
  publish_state(ac_power_sensor_, data_.ac_power[4], true);
}

void Inverter::save_preferences() {
  if (restore_) {
    InverterPreference pref_data;
    for (int i = 0; i < 4; i++)
      pref_data.energy_today[i] = data_.energy_today[i];
    for (int i = 0; i < 4; i++)
      pref_data.energy_since_last_reset[i] = data_.energy_since_last_reset[i];
    pref_data.last_poll_timestamp = data_.poll_timestamp;
    strcpy(pref_data.pair_id, id_);
    this->pref_.save(&pref_data);
  }
}

void Inverter::enable_restore() {
  InverterPreference pref_data{};
  this->pref_ = global_preferences->make_preference<InverterPreference>(fnv1_hash(std::string("inv_") + get_serial()));
  this->pref_.load(&pref_data);
  for (int i = 0; i < 4; i++)
    data_.energy_today[4] += data_.energy_today[i] = pref_data.energy_today[i];
  for (int i = 0; i < 4; i++)
    data_.energy_since_last_reset[4] += data_.energy_since_last_reset[i] = pref_data.energy_since_last_reset[i];
  data_.poll_timestamp = pref_data.last_poll_timestamp;
  if (!is_paired())
    strcpy(id_, pref_data.pair_id);
  restore_ = true;
}

bool Inverter::is_panel_connected(int i) {
  if (i < 0 || i > 3)
    return false;
  return connnected_panels_[i];
}

bool Inverter::is_paired() { return id_[0] != '\0'; }

}  // namespace apsystems
}  // namespace esphome