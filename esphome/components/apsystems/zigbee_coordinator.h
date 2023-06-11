#pragma once

#include <string>
#include <vector>
#include "inverter.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace apsystems {


enum ZigbeeCoordinatorState {
  CS_STOPPED = 0,
  CS_CHECK_1 = 1,
  CS_CHECK_2 = 2,
  CS_HARD_RESET_COORDINATOR = 10,
  CS_INITIALIZE_COORDINATOR = 11,
  CS_ENTER_NORMAL_OPERATION = 12,
  CS_IDLE = 20,
  CS_POLL_INVERTER = 21,
  CS_PAIR_INVERTER = 22,
  CS_REBOOT_INVERTER = 23
};

enum DataReadState { DS_IDLE = 0, DS_WAITING = 1, DS_READING = 2 };

enum AsyncBoolResult { AB_INCOMPLETE = 0, AB_SUCCESS = 1, AB_FAIL = 2 };

class ZigbeeCoordinator {
 public:
  void add_inverter(Inverter *inverter);
  void set_reset_pin(GPIOPin *pin);
  void set_uart_device(uart::UARTDevice *uart);
  void restart(bool hard);
  void run();
  bool start_pair_inverter(const char *serial);
  bool start_poll_inverter(const char *serial);
  bool start_reboot_inverter(const char *serial);
  int get_delay_to_next_execution();

 protected:
  AsyncBoolResult zb_reboot_inverter(Inverter *inverter);
  AsyncBoolResult zb_check();
  AsyncBoolResult zb_ping();
  AsyncBoolResult zb_poll(Inverter *inverter);
  int zb_decode_poll_response(const char * msg, int bytes_read, Inverter *inverter);
  AsyncBoolResult zb_pair(Inverter *inverter);
  bool zb_check_pair_response(const char * msg, int bytes_read, Inverter *inverter);
  AsyncBoolResult zb_initialize();
  void zb_hardreset();
  AsyncBoolResult zb_enter_normal_operation();
  AsyncBoolResult zb_read(char buf[], int &bytes_read);
  void zb_send(char printString[]);
  void set_delay_to_next_execution(int delay_ms);
  void set_state(ZigbeeCoordinatorState state);
  ZigbeeCoordinatorState state_ = ZigbeeCoordinatorState::CS_STOPPED;
  DataReadState data_state_ = DataReadState::DS_IDLE;
  bool pair_all_mode_ = false;
  bool poll_all_mode_ = false;
  Inverter *pairing_inverter_ = nullptr;
  Inverter *polling_inverter_ = nullptr;
  Inverter *rebooting_inverter_ = nullptr;
  int healthcheck_idle_counter_ = 0;
  int state_tries_ = 0;
  int data_state_tries_ = 0;
  int delay_to_next_execution_ = 0;
  char ecu_id_[13] = "D8A3011B9780";
  char ecu_id_reverse_[13];
  std::vector<Inverter *> inverters_{};
  GPIOPin *reset_pin_;
  uart::UARTDevice *uart_;
};

}  // namespace apsystems
}  // namespace esphome
