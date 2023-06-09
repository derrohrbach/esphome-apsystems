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

InverterData Inverter::get_data() { return data_; }

void Inverter::set_data(InverterData data) {
  data_ = data;
  save_preferences();
}

void Inverter::save_preferences() {
  if (restore_) {
    InverterPreference pref_data;
    for (int i = 0; i < 5; i++)
      pref_data.energy_today[i] = data_.energy_today[i];
    pref_data.last_poll_timestamp = data_.poll_timestamp;
    strcpy(pref_data.pair_id, id_);
    this->pref_.save(&pref_data);
  }
}

void Inverter::enable_restore() {
  InverterPreference pref_data;
  this->pref_ = global_preferences->make_preference<InverterPreference>(fnv1_hash(get_serial()));
  this->pref_.load(&pref_data);
  for (int i = 0; i < 5; i++)
    data_.energy_today[i] = pref_data.energy_today[i];
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