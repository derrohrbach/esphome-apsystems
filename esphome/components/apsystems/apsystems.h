#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/time/real_time_clock.h"
#include "zigbee_coordinator.h"
#include "inverter.h"

namespace esphome {
namespace apsystems {

class Apsystems : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;
  void set_time(time::RealTimeClock *time) { time_ = time; }
  void add_inverter(Inverter *inverter);
  void pair_inverter(std::string serial);
  void poll_inverter(std::string serial);
  void reboot_inverter(std::string serial);
  void set_reset_pin(GPIOPin *pin);
  void set_restore(bool restore);
  void set_ecu_id(std::string ecu_id);
  void set_auto_pair(bool auto_pair);
  void update();
  void loop();

 protected:
  void run_coordinator();
  time::RealTimeClock *time_;
  ZigbeeCoordinator coordinator_;
  std::vector<Inverter*> inverters_{};
  GPIOPin *reset_pin_;
  bool auto_pair_ = false;
  bool restore_ = false;
  char ecu_id_[13] = "\0";
  uint16_t last_day_of_year_ = 0;
};

template<typename... Ts> class ApsystemsPairInverterAction : public Action<Ts...> {
 public:
  ApsystemsPairInverterAction(Apsystems *aps) : apsystems_(aps) {}

  TEMPLATABLE_VALUE(std::string, serial)

  void play(Ts... x) override { this->apsystems_->pair_inverter(serial_.value(x...)); }

 protected:
  Apsystems *apsystems_;
};

template<typename... Ts> class ApsystemsPollInverterAction : public Action<Ts...> {
 public:
  ApsystemsPollInverterAction(Apsystems *aps) : apsystems_(aps) {}

  TEMPLATABLE_VALUE(std::string, serial)

  void play(Ts... x) override { this->apsystems_->poll_inverter(serial_.value(x...)); }

 protected:
  Apsystems *apsystems_;
};


template<typename... Ts> class ApsystemsRebootInverterAction : public Action<Ts...> {
 public:
  ApsystemsRebootInverterAction(Apsystems *aps) : apsystems_(aps) {}

  TEMPLATABLE_VALUE(std::string, serial)

  void play(Ts... x) override { this->apsystems_->reboot_inverter(serial_.value(x...)); }

 protected:
  Apsystems *apsystems_;
};

}  // namespace apsystems
}  // namespace esphome
