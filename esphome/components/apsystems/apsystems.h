#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "zigbee_coordinator.h"
#include "inverter.h"

namespace esphome {
namespace apsystems {

class Apsystems : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;
  void add_inverter(Inverter *inverter);
  void pair_inverter(std::string serial);
  void poll_inverter(std::string serial);
  void reboot_inverter(std::string serial);
  void set_reset_pin(GPIOPin *pin);
  void set_restore(bool restore);

 protected:
  void run_coordinator();
  ZigbeeCoordinator coordinator_;
  std::vector<Inverter*> inverters_{};
  GPIOPin *reset_pin_;
  bool restore_{false};
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
