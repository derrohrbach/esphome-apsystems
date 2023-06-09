#pragma once

#include <string>
#include <vector>
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace apsystems {

enum InverterType { IT_YC600 = 0, IT_QS1 = 1, IT_DS3 = 2 };

struct InverterPreference {
  int last_poll_timestamp;
  float energy_today[5];
  char pair_id[5];
};

struct InverterData {
  int poll_timestamp{0};
  float frequency{0.0f};
  float signal_quality{0.0f};
  float temperature{0.0f};
  float ac_voltage{0.0f};
  float dc_current[4]{0.0f};
  float dc_voltage[4]{0.0f};
  float power[5]{0.0f};
  float energy_since_last_reset[5]{0.0f};
  float energy_today[5]{0.0f};
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
  void save_preferences();
  void enable_restore();
  InverterData get_data();
  void set_data(InverterData data);

 protected:
  bool restore_;
  ESPPreferenceObject pref_;
  char serial_[13] = "000000000000";
  char id_[5] = {0};
  InverterData data_;
  InverterType type_ = InverterType::IT_YC600;
  bool connnected_panels_[4] = {true, true, true, true};
};

}  // namespace apsystems
}  // namespace esphome