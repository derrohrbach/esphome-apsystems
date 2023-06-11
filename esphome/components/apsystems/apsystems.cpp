#include "apsystems.h"
#include "esphome/core/log.h"

namespace esphome {
namespace apsystems {

static const char *const TAG = "apsystems";

float Apsystems::get_setup_priority() const { return setup_priority::DATA; }

void Apsystems::setup() {
  reset_pin_->setup();
  reset_pin_->pin_mode(gpio::Flags::FLAG_OUTPUT);
  coordinator_.set_reset_pin(reset_pin_);
  coordinator_.set_uart_device(this);
  bool needs_pairing = false;
  for (auto inv : inverters_) {
    if (restore_)
      inv->enable_restore();
    if (!inv->is_paired())
      needs_pairing = true;
    coordinator_.add_inverter(inv);
  }
  coordinator_.restart(false);
  if (auto_pair_ && needs_pairing)
    coordinator_.start_pair_inverter("*");
  run_coordinator();
}

void Apsystems::update() { coordinator_.start_poll_inverter("*"); }

void Apsystems::loop() {
  auto t = time_->now();
  if (!t.is_valid())
    return;

  if (last_day_of_year_ == 0) {
    last_day_of_year_ = t.day_of_year;
    return;
  }

  if (t.day_of_year != last_day_of_year_) {
    last_day_of_year_ = t.day_of_year;
    for (auto inv : inverters_) {
      InverterData data = inv->get_data();
      for (int i = 0; i < 5; i++)
        data.energy_today[i] = 0;
      inv->set_data(data);
    }
  }
}

void Apsystems::run_coordinator() {
  coordinator_.run();
  set_timeout(coordinator_.get_delay_to_next_execution(), [&]() { run_coordinator(); });
}

void Apsystems::pair_inverter(std::string serial) { coordinator_.start_pair_inverter(serial.c_str()); }
void Apsystems::poll_inverter(std::string serial) { coordinator_.start_poll_inverter(serial.c_str()); }
void Apsystems::reboot_inverter(std::string serial) { coordinator_.start_reboot_inverter(serial.c_str()); }
void Apsystems::set_restore(bool restore) { restore_ = restore; }
void Apsystems::set_auto_pair(bool auto_pair) { auto_pair_ = auto_pair; }

void Apsystems::add_inverter(Inverter *inverter) { this->inverters_.push_back(inverter); }
void Apsystems::set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
void Apsystems::dump_config() {
  ESP_LOGCONFIG(TAG, "APsystems:");
  this->check_uart_settings(115200, 1, uart::UART_CONFIG_PARITY_NONE, 8);
  LOG_PIN("  Reset Pin: ", reset_pin_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Configured inverters:");
  for (auto inv : inverters_) {
    ESP_LOGCONFIG(TAG, "    Serial: %s", inv->get_serial());
    ESP_LOGCONFIG(TAG, "    Pair-ID: %s", inv->get_id()[0] == '\0' ? "unpaired" : inv->get_id());
  }
}

}  // namespace apsystems
}  // namespace esphome
