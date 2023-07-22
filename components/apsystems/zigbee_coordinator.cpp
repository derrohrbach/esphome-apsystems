#include "zigbee_coordinator.h"
#include "esphome/core/log.h"

static const char *const TAG = "apsystems.zigbee_coordinator";
#define CC2530_MAX_MSG_SIZE (230 + 1)  // null char
namespace esphome {
namespace apsystems {

// **************************************************************************
//                               data converters
// **************************************************************************
// convert a char to Hex ******************************************************
int StrToHex(char str[]) { return (int) strtol(str, 0, 16); }

// reverse the ecu id **********************************************************
std::string ECU_REVERSE(std::string ecu_id) {
  std::string reverse = ecu_id.substr(10, 12) + ecu_id.substr(8, 10) + ecu_id.substr(6, 8) + ecu_id.substr(4, 6) +
                        ecu_id.substr(2, 4) + ecu_id.substr(0, 2);
  return reverse;
}

char *split(const char *str, const char *delim) {
  char *p = strstr(str, delim);

  if (p == NULL)
    return NULL;             // delimiter not found

  *p = '\0';                 // terminate string after head
  return p + strlen(delim);  // return tail substring
}

std::string checkSumString(const char *command) {
  char bufferCRC[254] = {0};
  char bufferCRC_2[254] = {0};

  strncpy(bufferCRC, command, 2);  // as starting point perhaps called "seed" use the first two chars from "command"
  delayMicroseconds(250);          // give memset a little bit of time to empty all the buffers

  for (uint8_t i = 1; i <= (strlen(command) / 2 - 1); i++) {
    strncpy(bufferCRC_2, command + i * 2, 2);  // use every iteration the next two chars starting with char 2+3
    delayMicroseconds(250);                    // give memset a little bit of time to empty all the buffers
    sprintf(bufferCRC, "%02X", StrToHex(bufferCRC) ^ StrToHex(bufferCRC_2));
    delayMicroseconds(250);                    // give memset a little bit of time to empty all the buffers
  }
  return std::string(bufferCRC);
}

void ZigbeeCoordinator::add_inverter(Inverter *inverter) { this->inverters_.push_back(inverter); }
void ZigbeeCoordinator::set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
void ZigbeeCoordinator::set_uart_device(uart::UARTDevice *uart) { uart_ = uart; }

void ZigbeeCoordinator::set_delay_to_next_execution(int delay_ms) { delay_to_next_execution_ = delay_ms; }
int ZigbeeCoordinator::get_delay_to_next_execution() { return delay_to_next_execution_; }

void ZigbeeCoordinator::restart(std::string ecu_id, bool hard) {
  ecu_id.copy(ecu_id_, 12, 0);
  ECU_REVERSE(ecu_id).copy(ecu_id_reverse_, 12, 0);
  reset_pin_->digital_write(true);
  if (hard)
    set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
  else
    set_state(ZigbeeCoordinatorState::CS_CHECK_1);
}

void ZigbeeCoordinator::set_state(ZigbeeCoordinatorState state) {
  switch (state_) {
    case ZigbeeCoordinatorState::CS_CHECK_1:
    case ZigbeeCoordinatorState::CS_CHECK_2:
      if (state == state_)
        set_delay_to_next_execution(700);  // wait until next try
      else
        set_delay_to_next_execution(100);  // Can continue quickly
      break;
    case ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR:
      set_delay_to_next_execution(2500);  // wait for cc2530 to reboot
      break;
    case ZigbeeCoordinatorState::CS_INITIALIZE_COORDINATOR:
      set_delay_to_next_execution(1000);  // wait for cc2530 to initialize
      break;
    case ZigbeeCoordinatorState::CS_ENTER_NORMAL_OPERATION:
      set_delay_to_next_execution(500);  // wait for start of normal operation
      break;
    case ZigbeeCoordinatorState::CS_PAIR_INVERTER:
    case ZigbeeCoordinatorState::CS_POLL_INVERTER:
    case ZigbeeCoordinatorState::CS_IDLE:
      set_delay_to_next_execution(100);  // Can continue quickly
      break;
    case ZigbeeCoordinatorState::CS_REBOOT_INVERTER:
      set_delay_to_next_execution(2000);  // Wait for reboot until we read the response
      break;
    case ZigbeeCoordinatorState::CS_STOPPED:
      break;
  }
  if (state == ZigbeeCoordinatorState::CS_IDLE)
    set_delay_to_next_execution(1000);
  if (state_ != state) {
    state_ = state;
    state_tries_ = 0;
  }
}

void ZigbeeCoordinator::run() {
  if (ecu_id_[0] == '\0')  // Not initialized yet
    return;
  ZigbeeCoordinatorState oldState = state_;
  if (state_ != ZigbeeCoordinatorState::CS_IDLE)
    ESP_LOGVV(TAG, "coordinator run starting (state %i:%i - data state %i:%i)", state_, state_tries_, data_state_,
              data_state_tries_);
  AsyncBoolResult cmdResult;
  bool found_current_inverter;
  switch (state_) {
    case ZigbeeCoordinatorState::CS_CHECK_1:
      if ((cmdResult = zb_ping())) {
        if (cmdResult == AsyncBoolResult::AB_SUCCESS) {
          // Ping successfoll -> go to second check
          set_state(ZigbeeCoordinatorState::CS_CHECK_2);
        } else if (state_tries_ < 3) {
          // Zigbee Coordinator not responding -> try again
          set_state(ZigbeeCoordinatorState::CS_CHECK_1);
        } else {
          // Zigbee Coordinator not working -> initialize
          set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
        }
      }
      break;
    case ZigbeeCoordinatorState::CS_CHECK_2:
      if ((cmdResult = zb_check())) {
        if (cmdResult == AsyncBoolResult::AB_SUCCESS) {
          // Ping successfull -> go to IDLE
          if (pairing_inverter_ != nullptr)
            set_state(ZigbeeCoordinatorState::CS_PAIR_INVERTER);  // start pairing
          else
            set_state(ZigbeeCoordinatorState::CS_IDLE);
        } else if (state_tries_ < 3) {
          // Zigbee Coordinator not responding -> try again
          set_state(ZigbeeCoordinatorState::CS_CHECK_2);
        } else {
          // Zigbee Coordinator not working -> initialize
          set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
        }
      }
      break;
    case ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR:
      zb_hardreset();
      set_state(ZigbeeCoordinatorState::CS_INITIALIZE_COORDINATOR);
      break;
    case ZigbeeCoordinatorState::CS_INITIALIZE_COORDINATOR:
      if ((cmdResult = zb_initialize())) {
        if (cmdResult == AsyncBoolResult::AB_SUCCESS) {
          if (pairing_inverter_ != nullptr)
            set_state(ZigbeeCoordinatorState::CS_CHECK_1);  // We are pairing, skip entering NO
          else
            set_state(ZigbeeCoordinatorState::CS_ENTER_NORMAL_OPERATION);
        } else
          set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
      }
      break;
    case ZigbeeCoordinatorState::CS_ENTER_NORMAL_OPERATION:
      if ((cmdResult = zb_enter_normal_operation())) {
        if (cmdResult == AsyncBoolResult::AB_SUCCESS)
          set_state(ZigbeeCoordinatorState::CS_CHECK_1);
        else
          set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
      }
      break;
    case ZigbeeCoordinatorState::CS_IDLE:
      // Check connection about every 30 seconds
      healthcheck_idle_counter_++;
      if (healthcheck_idle_counter_ > 30) {
        healthcheck_idle_counter_ = 0;
        set_state(ZigbeeCoordinatorState::CS_CHECK_1);
      } else if (pairing_inverter_ != nullptr) {
        restart(ecu_id_, true);
      } else if (rebooting_inverter_ != nullptr) {
        set_state(ZigbeeCoordinatorState::CS_REBOOT_INVERTER);
      } else if (polling_inverter_ != nullptr) {
        set_state(ZigbeeCoordinatorState::CS_POLL_INVERTER);
      }
      break;
    case ZigbeeCoordinatorState::CS_PAIR_INVERTER:
      if ((cmdResult = zb_pair(pairing_inverter_))) {
        auto paired_inverter = pairing_inverter_;
        pairing_inverter_ = nullptr;
        if (pair_all_mode_) {
          pair_all_mode_ = false;
          found_current_inverter = false;
          for (auto inv : inverters_) {
            if (!found_current_inverter) {
              if (inv == paired_inverter)
                found_current_inverter = true;
            } else if (!inv->is_paired()) {
              pairing_inverter_ = inv;
              pair_all_mode_ = true;
              break;
            }
          }
        }
        if (pair_all_mode_) {
          state_tries_ = -1;  // restart pair with new inverter
        } else if (cmdResult == AsyncBoolResult::AB_SUCCESS) {
          set_state(ZigbeeCoordinatorState::CS_ENTER_NORMAL_OPERATION);
        } else {
          set_state(ZigbeeCoordinatorState::CS_HARD_RESET_COORDINATOR);
        }
      }
      break;
    case ZigbeeCoordinatorState::CS_POLL_INVERTER:
      if ((cmdResult = zb_poll(polling_inverter_))) {
        auto polled_inverter = polling_inverter_;
        polling_inverter_ = nullptr;
        if (poll_all_mode_) {
          poll_all_mode_ = false;
          found_current_inverter = false;
          for (auto inv : inverters_) {
            if (!found_current_inverter) {
              if (inv == polled_inverter)
                found_current_inverter = true;
            } else if (inv->is_paired()) {
              polling_inverter_ = inv;
              poll_all_mode_ = true;
              break;
            }
          }
        }
        if (poll_all_mode_) {
          state_tries_ = -1;  // restart poll with new inverter
        } else {
          set_state(ZigbeeCoordinatorState::CS_IDLE);
        }
      }
      break;
    case ZigbeeCoordinatorState::CS_REBOOT_INVERTER:
      if ((cmdResult = zb_reboot_inverter(rebooting_inverter_))) {
        rebooting_inverter_ = nullptr;
        set_state(ZigbeeCoordinatorState::CS_IDLE);
      }
      break;
    case ZigbeeCoordinatorState::CS_STOPPED:
      break;
  }
  if (data_state_ == DataReadState::DS_IDLE && oldState == state_) {
    set_state(state_);  // Resets the time to next execution, after reading
    state_tries_++;
  }
}

AsyncBoolResult ZigbeeCoordinator::zb_initialize() {
  if (data_state_ == DataReadState::DS_IDLE) {
    /*
     * init the coordinator takes the following procedure
     *  0 Sent=FE03260503010321
     *  Received=FE0166050062
     *  1 Sent=FE0141000040
     *  Received=FE064180020202020702C2
     *  2 Sent=FE0A26050108FFFF80971B01A3D856
     *  Received=FE0166050062
     *  3 Sent=FE032605870100A6
     *  Received=FE0166050062
     *  4 Sent=FE 04 26058302 D8A3 DD  should be ecu_id the fst 2 bytes
     *  Received=FE0166050062
     *  5 Sent=FE062605840400000100A4
     *  Received=FE0166050062
     *  6 Sent=FE0D240014050F0001010002000015000020
     *  Received=FE0164000065
     *  7 Sent=FE00260026
     *  8 Sent=FE00670067
     *  Received=FE0145C0098D
     *  received FE00660066 FE0145C0088C FE0145C0098D F0F8FE0E670000FFFF80971B01A3D8000007090011
     *  now we can pair if we want to or else an extra command for retrieving data (normal operation)
     *  9 for normal operation we send cmd 9
     *  Finished. Heap=26712
     *
     */
    if (state_tries_ == 0) {
      ESP_LOGD(TAG, "init zb coordinator");
    }
    // yield();
    char initCmd[254] = {0};
    // commands for setting up coordinater
    char initBaseCommand[][254] = {
        "2605030103",                      // ok   this is a ZB_WRITE_CONFIGURATION CMD //changed to 01
        "410000",                          // ok   ZB_SYS_RESET_REQ
        "26050108FFFF",                    // + ecu_id_reverse,  this is a ZB_WRITE_CONFIGURATION CMD
        "2605870100",                      // ok
        "26058302",                        // + ecu_id.substring(0,2) + ecu_id.substring(2,4),
        "2605840400000100",                // ok
        "240014050F00010100020000150000",  // AF_REGISTER register an application’s endpoint description
        "2600",                            // ok ZB_START_REQUEST
    };

    // construct some of the commands
    // ***************************** command 2 ********************************************
    // command 2 this is 26050108FFFF we add ecu_id reversed
    strncat(initBaseCommand[2], ecu_id_reverse_, 12);
    // DebugPrintln("initBaseCmd 2 constructed = " + String(initBaseCommand[2]));  // ok

    // ***************************** command 4 ********************************************
    // command 4 this is 26058302 + ecu_id_short
    strncat(initBaseCommand[4], ecu_id_, 2);
    strncat(initBaseCommand[4], ecu_id_ + 2, 2);

    ESP_LOGVV(TAG, "init send cmd %i", state_tries_);

    zb_send(initBaseCommand[state_tries_]);
  }
  int bytesRead = 0;
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};  // provide a buffer for the call to readZB
  // check if anything was received
  if (!zb_read(s_d, bytesRead))  // we read but flush the answer
    return AsyncBoolResult::AB_INCOMPLETE;
  if (state_tries_ < 7)
    return AsyncBoolResult::AB_INCOMPLETE;
  return AsyncBoolResult::AB_SUCCESS;
}

// *************************************************************************
//                          hard reset the cc25xx
// *************************************************************************
void ZigbeeCoordinator::zb_hardreset() {
  reset_pin_->digital_write(false);
  delay(50);
  reset_pin_->digital_write(true);
  ESP_LOGV(TAG, "zb module hard reset");
}

// **************************************************************************************
//                the extra command for normal operations
// **************************************************************************************
AsyncBoolResult ZigbeeCoordinator::zb_enter_normal_operation() {
  if (data_state_ == DataReadState::DS_IDLE) {
    char noCmd[100] = {0};  //  this buffer must have the right length

    snprintf(noCmd, sizeof(noCmd), "2401FFFF1414060001000F1E%sFBFB1100000D6030FBD3000000000000000004010281FEFE",
             ecu_id_reverse_);
    // lenth=36+12+1
    // Serial.println("noCmd = " + String(noCmd));

    // add sln at the beginning
    // char comMand[100];
    // sprintf(comMand, "%02X", (strlen(noCmd) / 2 - 2));
    // strcat(comMand, noCmd);

    // add the CRC at the end of the command is done by sendZigbee
    ESP_LOGVV(TAG, "send normal ops initCmd = %s", noCmd);
    zb_send(noCmd);
  }

  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};  // provide a buffer for the call to readZB
  int bytesRead = 0;
  // check if anything was received
  // waitSerial2Available();
  if (!zb_read(s_d, bytesRead))
    return AsyncBoolResult::AB_INCOMPLETE;

  // do nothing with the returned value
  // if(readCounter == 0) Serial.println("no answer");

  ESP_LOGVV(TAG, "zb initializing ready, now check running");
  return AsyncBoolResult::AB_SUCCESS;
}

AsyncBoolResult ZigbeeCoordinator::zb_check() {
  if (data_state_ == DataReadState::DS_IDLE) {
    // We entered for the first time, so send the 2700 command
    //  the answer can mean that the coordinator is up, not yet started or no answer
    //  we evaluate that
    //  first empty serial2, comming from coordinator this is necessary;
    //  empty_serial2(); is done in the loop

    // the response = 67 00, status 1 bt, IEEEAddr 8bt, ShortAddr 2bt, DeviceType 1bt, Device State 1bt
    //  FE0E 67 00 00 FFFF 80971B01A3D8 0000 0709001
    // status = 00 means succes, IEEEAddress= FFFF80971B01A3D8, ShortAdr = 0000, devicetype=07 bits 0 to 2

    // Device State 09 started as zigbeecoordinator

    ESP_LOGV(TAG, "check zb radio");
    // if(log) Update_Log("zigbee", "checking zb module");
    // check the radio, send FE00670067
    //  when ok the returned string = FE0E670000FFFF + ECU_ID REVERSE + 00000709001
    //  so we check if we have this

    char checkCommand[10];  // we send 2700 to the zb
    strncpy(checkCommand, "2700", 5);

    char received[254] = {0};  // a buffer for the received message
    zb_send(checkCommand);
  }

  char *tail;
  int bytes_read = 0;
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};
  // now read the answer if there is one
  if (!zb_read(s_d, bytes_read))
    return AsyncBoolResult::AB_INCOMPLETE;

  // if ( waitSerial2Available() ) { readZigbee(); } else { readCounter = 0;} // when nothing available we don't read
  // ESP_LOGD(TAG, "cc inMessage = " + String(inMessage) + " rc = " + String(readCounter))

  // we get this : FE0E670000 FFFF80971B01A3D8 0000 07090011 or
  //    received : FE0E670000 FFFF80971B01A3D6 0000 0709001F when ok

  // check if ecu_id_reverse is in the string, then split it there + 2 bytes
  if (strstr(s_d, ecu_id_reverse_)) {
    tail = split(s_d, ecu_id_reverse_ + 4);
    // the tail should contain 0709
    if (strstr(tail, "0709")) {
      ESP_LOGVV(TAG, "check ok");
      return AsyncBoolResult::AB_SUCCESS;
    }
  }

  ESP_LOGVV(TAG, "check failed");
  return AsyncBoolResult::AB_FAIL;
}

AsyncBoolResult ZigbeeCoordinator::zb_ping() {
  if (data_state_ == DataReadState::DS_IDLE) {
    // if the ping command failed then we have to restart the coordinator
    // Update_Log("zigbee", "check serial loopback");
    // these commands already have the length 00 and checksum 20 resp 26
    char pingCmd[5] = {"2101"};  // ping
    ESP_LOGVV(TAG, "send zb ping");

    zb_send(pingCmd);  // answer should be FE02 6101 79 07 1C
  }
  char received[254] = {0};
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};
  int bytes_read = 0;
  if (!zb_read(s_d, bytes_read))  // READ INCOMPLETE
    return AsyncBoolResult::AB_INCOMPLETE;

  if (strstr(s_d, "FE026101") == NULL) {
    ESP_LOGVV(TAG, "pinging zigbee coordinator failed");
    return AsyncBoolResult::AB_FAIL;
  } else {
    ESP_LOGVV(TAG, "ping ok");
    return AsyncBoolResult::AB_SUCCESS;
  }
}

// *****************************************************************************
//                            read zigbee
// *****************************************************************************

AsyncBoolResult ZigbeeCoordinator::zb_read(char buf[], int &bytes_read) {
  bytes_read = 0;
  buf[0] = '\0';

  if (data_state_ == DataReadState::DS_IDLE) {
    if (!uart_->available()) {
      {
        data_state_tries_ = 0;
        data_state_ = DataReadState::DS_WAITING;
      }
    }
  }

  if (data_state_ == DataReadState::DS_WAITING) {
    data_state_tries_++;
    if (!uart_->available()) {
      if (data_state_tries_ > 20) {
        data_state_ = DataReadState::DS_IDLE;
        return AsyncBoolResult::AB_FAIL;
      } else {
        return AsyncBoolResult::AB_INCOMPLETE;
      }
    }
  }

  if (data_state_ == DataReadState::DS_READING &&
      (uart_->available() == data_state_tries_ || uart_->available() >= CC2530_MAX_MSG_SIZE - 1)) {
    char oneChar[10] = {0};
    // fullIncomingMessage[0] = '\0'; //terminate otherwise all is appended
    // memset( &inMessage, 0, sizeof(inMessage) ); //zero out the
    // delayMicroseconds(250);

    while (uart_->available()) {
      // Serial.print("#");
      //  here we have the danger that when readcounter reaches 512, there are 1024 bytes processed
      //  the length of a poll answer is usually not more than 223
      if (bytes_read < CC2530_MAX_MSG_SIZE - 1) {
        sprintf(oneChar, "%02X", uart_->read());  // always uppercase
        strncat(buf, oneChar, 2);                 // append
        bytes_read += 1;
      } else {
        while (uart_->available()) {
          uart_->read();
        }  // remove all excess data in the buffer at once
      }
    }
    // with swaps we get F8 sometimes, this removes it
    if (buf[0] == 'F' && buf[1] == '8') {
      ESP_LOGVV(TAG, "found F8");
      strcpy(buf, &buf[2]);
    }

    ESP_LOGVV(TAG, "  read zb %s  rc=%i", buf, bytes_read);
    data_state_ = DataReadState::DS_IDLE;
    data_state_tries_ = 0;
    return AsyncBoolResult::AB_SUCCESS;
  }

  data_state_ = DataReadState::DS_READING;
  data_state_tries_ = uart_->available();
  set_delay_to_next_execution(120);
  return AsyncBoolResult::AB_INCOMPLETE;
}

// *****************************************************************************
//                 send to zigbee radio
// *****************************************************************************
void ZigbeeCoordinator::zb_send(char printString[]) {
  char bufferSend[254] = {0};
  char byteSend[3];                                            // never more than 2 bytes
  sprintf(bufferSend, "%02X", (strlen(printString) / 2 - 2));  // now contains a hex representation of the length
  // first add length and the checksum
  strcat(bufferSend, printString);

  strcat(bufferSend, checkSumString(bufferSend).c_str());

  // Clear read buffer
  while (uart_->available())
    uart_->read();

  uart_->write(0xFE);  // we have to send "FE" at start of each command
  for (uint8_t i = 0; i <= strlen(bufferSend) / 2 - 1; i++) {
    // we use 2 characters to make a byte
    strncpy(byteSend, bufferSend + i * 2, 2);

    uart_->write(StrToHex(byteSend));  // turn the two chars to a byte and send this
  }

  uart_->flush();  // wait till the full command was sent

  ESP_LOGVV(TAG, "  send zb FE%s", bufferSend);

  // else if (diagNose == 2) ws.textAll("sendZB FE" + String(bufferSend));
}

// ******************************************************************************
//                   reboot an inverter
// *******************************************************************************
AsyncBoolResult ZigbeeCoordinator::zb_reboot_inverter(Inverter *inverter) {
  if (data_state_ == DataReadState::DS_IDLE && state_tries_ == 0) {
    char rebootCmd[57];

    char command[][30] = {
        "2401",
        "1414060001000F13",
        "FBFB06C1000000000000A6FEFE",
    };
    // length = 46 + 4 + 12 + 1= 53

    // construct the command
    strncpy(rebootCmd, command[0], 4);
    strncat(rebootCmd, inverter->get_id(), 4);  // add inv_id
    strncat(rebootCmd, command[1], 16);
    strncat(rebootCmd, ecu_id_reverse_, 12);
    strncat(rebootCmd, command[2], 26);
    // String term = "the rebootCmd = " + String(rebootCmd);
    // should be 2401 3A10 1414060001000F13 80971B01B3D7 FBFB06C1000000000000A6FEFE

    //} else

    // should be 2401 103A 1414060001000F13 80 97 1B 01 A3 D6 FBFB06C1000000000000A6FEFE
    //           2401 A310 1414060001000F13 80 97 1B 01 A3 D6 FBFB06C1000000000000A6FEFE
    //           2401 3A10 1414060001000F13 80 97 1B 01 B3 D7 FBFB06C1000000000000A6FEFE

    // put in the CRC at the end of the command in sendZigbee

    zb_send(rebootCmd);
    return AsyncBoolResult::AB_INCOMPLETE;  // WAIT A BIT
  }
  int bytesRead = 0;
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};
  if (!zb_read(s_d, bytesRead))
    return AsyncBoolResult::AB_INCOMPLETE;
  return AsyncBoolResult::AB_SUCCESS;
}

bool ZigbeeCoordinator::start_pair_inverter(const char *serial) {
  for (auto inv : inverters_) {
    if (serial[0] == '*') {
      if (!inv->is_paired()) {
        pairing_inverter_ = inv;
        pair_all_mode_ = true;
        break;
      }
    } else {
      if (strcmp(serial, inv->get_serial()) == 0) {
        pairing_inverter_ = inv;
        break;
      }
    }
  }

  if (pairing_inverter_ == nullptr) {
    if (serial[0] == '*')
      ESP_LOGI(TAG, "pairing skipped: all inverters are already paired");
    else
      ESP_LOGE(TAG, "pairing failed: inverter with serial %s is not configured", serial);
  } else if (state_ != ZigbeeCoordinatorState::CS_IDLE) {
    ESP_LOGI(TAG, "pairing defered: coordinator busy or not configured", serial);
  } else {
    if (pair_all_mode_)
      ESP_LOGI(TAG, "pairing all unpaired inverters");
    else
      ESP_LOGI(TAG, "pairing inverter %s", pairing_inverter_->get_serial());
    restart(ecu_id_, true);
    return true;
  }
  return false;
}

AsyncBoolResult ZigbeeCoordinator::zb_pair(Inverter *inverter) {
  static bool success;
  if (data_state_ == DataReadState::DS_IDLE) {
    // the pairing process consistst of 4 commands sent to the coordinator
    char pairCmd[254] = {0};

    char ecu_short[5] = {0};
    strncat(ecu_short, ecu_id_ + 2, 2);  // D8A3011B9780 should be A3D8
    strncat(ecu_short, ecu_id_, 2);

    switch (state_tries_) {
      case 0:
        success = false;
        ESP_LOGI(TAG, "starting pair of %s", inverter->get_serial());
        // command 0
        // build command 0 this is "24020FFFFFFFFFFFFFFFFF14FFFF14" + "0D0200000F1100" + String(invSerial) +
        // "FFFF10FFFF" + ecu_id_reverse
        snprintf(pairCmd, sizeof(pairCmd), "24020FFFFFFFFFFFFFFFFF14FFFF140D0200000F1100%sFFFF10FFFF%s",
                 inverter->get_serial(), ecu_id_reverse_);
        break;
      case 1:
        // build command 1 this is "24020FFFFFFFFFFFFFFFFF14FFFF14" + "0C0201000F0600"  + inv serial,
        snprintf(pairCmd, sizeof(pairCmd), "24020FFFFFFFFFFFFFFFFF14FFFF140C0201000F0600%s", inverter->get_serial());
        break;
      case 2:
        // build command 2 this is "24020FFFFFFFFFFFFFFFFF14FFFF14" + "0F0102000F1100"  + invSerial + short
        // ecu_id_reverse, + 10FFF + ecu_id_reverse

        snprintf(pairCmd, sizeof(pairCmd), "24020FFFFFFFFFFFFFFFFF14FFFF140F0102000F1100%s%s10FFFF%s",
                 inverter->get_serial(), ecu_short, ecu_id_reverse_);
        break;
      case 3:
        // now build command 3 this is "24020FFFFFFFFFFFFFFFFF14FFFF14"  + "010103000F0600" + ecu_id_reverse,
        snprintf(pairCmd, sizeof(pairCmd), "24020FFFFFFFFFFFFFFFFF14FFFF14010103000F0600%s", ecu_id_reverse_);
    }
    // send
    ESP_LOGVV(TAG, "pair command %i = %s", state_tries_, pairCmd);
    // else if(diagNose == 2) ws.textAll(term);

    zb_send(pairCmd);
  }
  int bytesRead = 0;
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};
  if (!zb_read(s_d, bytesRead))
    return AsyncBoolResult::AB_INCOMPLETE;

  // if not y == 1 or y == 2 we waste the received message
  if (state_tries_ == 1 || state_tries_ == 2) {
    // if y 1 or 2 we catch and decode the answer
    if (zb_check_pair_response(s_d, bytesRead, inverter)) {
      success = true;  // if at least one of these 2 where true we had success
    }
  }
  if (state_tries_ == 3) {
    // now all 4 commands have been sent
    if (success) {
      ESP_LOGI(TAG, "pairing successfull! inverter %s has pair id %s", inverter->get_serial(), inverter->get_id());
      return AsyncBoolResult::AB_SUCCESS;
    } else {
      inverter->set_id("");
      ESP_LOGE(TAG, "pairing failed");
      return AsyncBoolResult::AB_FAIL;
    }  // when paired 0x103A
  }
  return AsyncBoolResult::AB_INCOMPLETE;
}

bool ZigbeeCoordinator::zb_check_pair_response(const char *msg, int bytes_read, Inverter *inverter) {
  char _CC2530_answer_string[] = "44810000";
  char _noAnswerFromInverter[32] = "FE0164010064FE034480CD14011F";
  char *result;
  char temp[13];

  // Serial.println("messageToDecode = " + String(messageToDecode));
  ESP_LOGVV(TAG, "decoding: %s", msg);
  if (bytes_read == 0 || strlen(msg) < 6)  // invalid message
  {
    ESP_LOGV(TAG, "no usable code, returning..");
    return false;
  }
  // can we conclude that a valid pair answer cannot be less than 60 bytes
  if (bytes_read > 111 || bytes_read < 60) {
    // term = "no pairing code, returning...";
    ESP_LOGV(TAG, "no valid pairing code, returning...");
    return false;
  }

  if (!strstr(msg, inverter->get_serial())) {
    ESP_LOGV(TAG, "not found serialnr, returning");
    return false;
  }

  result = split(msg, inverter->get_serial());
  ESP_LOGV(TAG, "result after 1st splitting: %s", result);

  // now we keep splitting as long as result contains the serial nr
  while (strstr(result, inverter->get_serial())) {
    result = split(result, inverter->get_serial());
  }
  ESP_LOGV(TAG, "result after splitting: %s", result);
  // result are the bytes behind the serialnr
  // now we know that it is what we expect, a string right behind the last occurence of the serialnr
  result[4] = '\0';
  inverter->set_id(result);

  ESP_LOGV(TAG, "found pair id %s", inverter->get_id());

  return true;
}

bool ZigbeeCoordinator::start_reboot_inverter(const char *serial) {
  for (auto inv : inverters_) {
    if (strcmp(serial, inv->get_serial()) == 0 && inv->is_paired()) {
      rebooting_inverter_ = inv;
      break;
    }
  }

  if (rebooting_inverter_ == nullptr) {
    ESP_LOGE(TAG, "rebooting failed: inverter with serial %s is not paired", serial);
  } else if (state_ != ZigbeeCoordinatorState::CS_IDLE) {
    ESP_LOGI(TAG, "rebooting defered: coordinator busy or not configured", serial);
  } else {
    ESP_LOGI(TAG, "rebooting inverter %s", rebooting_inverter_->get_serial());
    set_state(ZigbeeCoordinatorState::CS_REBOOT_INVERTER);
    return true;
  }
  return false;
}

bool ZigbeeCoordinator::start_poll_inverter(const char *serial) {
  for (auto inv : inverters_) {
    if (serial[0] == '*') {
      if (inv->is_paired()) {
        polling_inverter_ = inv;
        poll_all_mode_ = true;
        break;
      }
    } else {
      if (strcmp(serial, inv->get_serial()) == 0 && inv->is_paired()) {
        polling_inverter_ = inv;
        break;
      }
    }
  }

  if (polling_inverter_ == nullptr) {
    if (serial[0] == '*')
      ESP_LOGV(TAG, "polling skipped: no inverters are paired");
    else
      ESP_LOGE(TAG, "polling failed: inverter with serial %s is not paired", serial);
  } else if (state_ != ZigbeeCoordinatorState::CS_IDLE) {
    ESP_LOGV(TAG, "polling defered: coordinator busy or not configured", serial);
  } else {
    if (poll_all_mode_)
      ESP_LOGV(TAG, "polling all paired inverters");
    else
      ESP_LOGI(TAG, "polling inverter %s", polling_inverter_->get_serial());
    set_state(ZigbeeCoordinatorState::CS_POLL_INVERTER);
    return true;
  }
  return false;
}

AsyncBoolResult ZigbeeCoordinator::zb_poll(Inverter *inverter) {
  if (data_state_ == DataReadState::DS_IDLE) {
    char pollCommand[65] = {0};
    snprintf(pollCommand, sizeof(pollCommand), "2401%s1414060001000F13%sFBFB06BB000000000000C1FEFE", inverter->get_id(),
             ecu_id_reverse_);

    zb_send(pollCommand);
  }
  int bytesRead = 0;
  char s_d[CC2530_MAX_MSG_SIZE * 2] = {0};
  if (!zb_read(s_d, bytesRead))
    return AsyncBoolResult::AB_INCOMPLETE;

  if (zb_decode_poll_response(s_d, bytesRead, inverter)) {
    inverter->set_unsuccessfull_polls(0);
    return AsyncBoolResult::AB_SUCCESS;
  }
  inverter->set_unsuccessfull_polls(inverter->get_unsuccessfull_polls() + 1);
  if (inverter->get_unsuccessfull_polls() == 10) {
    auto data = inverter->get_data();
    data.ac_frequency = NAN;
    for (int i = 0; i < 4; i++) {
      data.dc_current[i] = NAN;
      data.dc_voltage[i] = NAN;
    }
    data.ac_voltage = NAN;
    data.signal_quality = NAN;
    data.temperature = NAN;
    for (int i = 0; i < 5; i++) {
      data.ac_power[i] = NAN;
      data.dc_power[i] = NAN;
    }
    inverter->set_data(data);
  }
  return AsyncBoolResult::AB_FAIL;
}

// *******************************************************************************************************************
//                             extract values
// *******************************************************************************************************************
float extractValue(uint8_t startPosition, uint8_t valueLength, float valueSlope, float valueOffset,
                   const char *toDecode) {
  char tempMsgBuffer[64] = {0};  // was 254
  strncpy(tempMsgBuffer, toDecode + startPosition, valueLength);
  return (valueSlope * (float) strtol(tempMsgBuffer, 0, 16)) + valueOffset;
}

// ******************************************************************
//                    decode polling answer
// ******************************************************************
bool ZigbeeCoordinator::zb_decode_poll_response(const char *msg, int bytes_read, Inverter *inv) {
  uint8_t message_start_offset = 0;

  InverterData old_data = inv->get_data();
  InverterData new_data{};
  bool new_data_valid = true;

  if (bytes_read == 0) {
    ESP_LOGE(TAG, "no answer to poll request for inverter %s", inv->get_serial());

    return false;  // no answer
  }

  ESP_LOGV(TAG, "decode poll response for inverter %s", inv->get_serial());

  char *tail;

  if (strstr(msg, "FE01640100") == NULL) {  // answer to AF_DATA_REQUEST 00=success
    ESP_LOGE(TAG, "AF_DATA_REQUEST failed while polling inverter %s", inv->get_serial());
    return false;
  } else if (strstr(msg, "FE03448000") == NULL) {  //  AF_DATA_CONFIRM the 00 byte = success
    ESP_LOGD(TAG, "did not receive AF_DATA_CONFIRM while polling inverter %s", inv->get_serial());
    return false;
  } else if (strstr(msg, "4481") == NULL) {
    ESP_LOGD(TAG, "did not receive AF_INCOMING_MSG while polling inverter %s", inv->get_serial());
    return false;
  }

  if (strlen(msg) < 223)  // this message is not long enough to be valid inverter data
  {
    ESP_LOGD(TAG, "received message was too short while polling inverter %s", inv->get_serial());
    return false;
  }

  // shorten the message by removing everything before 4481
  tail = split(msg, "44810000");  // remove the 0000 as well

  new_data.signal_quality = extractValue(14, 2, 1, 0, tail) * 100 / 255;

  char s_d[CC2530_MAX_MSG_SIZE * 2];
  strncpy(s_d, tail + 30, strlen(tail));

  if (inv->get_type() == InverterType::INVERTER_TYPE_DS3) {
    ESP_LOGV(TAG, "decoding DS3 inverter");

    // ACV offset 34
    new_data.ac_voltage = extractValue(68, 4, 1, 0, s_d) / 3.8;

    // FREQ offset 36
    new_data.ac_frequency = extractValue(72, 4, 1, 0, s_d) / 100;

    // TEMP offset 48
    new_data.temperature = extractValue(96, 4, 1, 0, s_d) * 0.0198 - 23.84;

    // ******************  dc voltage   *****************************************
    // voltage ch1 offset 28
    new_data.dc_voltage[0] = extractValue(52, 4, 1, 0, s_d) / 48.0f;
    // voltage ch2 offset 26
    new_data.dc_voltage[1] = extractValue(56, 4, 1, 0, s_d) / 48.0f;
    // ******************  current   *****************************************
    // current ch1 offset 30
    new_data.dc_current[0] = extractValue(60, 4, 1, 0, s_d) * 0.0125f;
    // current ch2 offset 34
    new_data.dc_current[1] = extractValue(64, 4, 1, 0, s_d) * 0.0125f;

  } else {
    if (inv->get_type() == InverterType::INVERTER_TYPE_YC600) {
      ESP_LOGV(TAG, "decoding YC600 inverter");
    } else if (inv->get_type() == InverterType::INVERTER_TYPE_QS1) {
      ESP_LOGV(TAG, "decoding QS1 inverter");
    } else {
      ESP_LOGV(TAG, "decoding unspecified inverter (trying like QS1)");
    }

    // yc600 or QS1
    //  frequency ac voltage and temperature
    //  ACV offset 28
    new_data.ac_voltage = extractValue(56, 4, 1, 0, s_d) / 5.3108f;
    // FREQ offset 12
    new_data.ac_frequency = 50000000 / extractValue(24, 6, 1, 0, s_d);
    // TEMP offset 10
    new_data.temperature = extractValue(20, 4, 0.2752f, -258.7f, s_d);

    // ******************  dc voltage   *****************************************
    // voltage ch1 offset 24
    new_data.dc_voltage[0] = (extractValue(48, 2, 16, 0, s_d) + extractValue(46, 1, 1, 0, s_d)) * 82.5f / 4096.0f;
    // voltage ch2 offset 27
    new_data.dc_voltage[1] = (extractValue(54, 2, 16, 0, s_d) + extractValue(52, 1, 1, 0, s_d)) * 82.5f / 4096.0f;

    // ******************  current   *****************************************
    // current ch1 offset 22
    new_data.dc_current[0] = (extractValue(47, 1, 256, 0, s_d) + extractValue(44, 2, 1, 0, s_d)) * 27.5f / 4096.0f;
    // current ch2 offset 25
    new_data.dc_current[1] = (extractValue(53, 1, 256, 0, s_d) + extractValue(50, 2, 1, 0, s_d)) * 27.5f / 4096.0f;

    //********************************************************************************************
    //                                     QS1
    //********************************************************************************************
    if (inv->get_type() == InverterType::INVERTER_TYPE_QS1) {
      ESP_LOGV(TAG, "extracting dc values for panels 3 and 4");

      // ******************  dc voltage   *****************************************
      // voltage ch3 offset 21
      new_data.dc_voltage[2] = (extractValue(42, 2, 16, 0, s_d) + extractValue(40, 1, 1, 0, s_d)) * 82.5f / 4096.0f;
      // voltage ch4 offset 18
      new_data.dc_voltage[3] = (extractValue(36, 2, 16, 0, s_d) + extractValue(34, 1, 1, 0, s_d)) * 82.5f / 4096.0f;

      // ******************  current   *****************************************
      // current ch3 offset 19
      new_data.dc_current[2] = (extractValue(41, 1, 256, 0, s_d) + extractValue(38, 2, 1, 0, s_d)) * 27.5f / 4096.0f;
      // current ch4 offset 16
      new_data.dc_current[3] = (extractValue(35, 1, 256, 0, s_d) + extractValue(32, 2, 1, 0, s_d)) * 27.5f / 4096.0f;
    }
  }

  // we extract a value out of the inverter answer: en_extr
  // We have a value from the last poll: en_saved --> en_old
  // save the new enerrgy value en_extr to en_saved
  // We subtract these to get the increase en_saved -/- en_old
  // So en_saved is the value of total energy from the inverter
  // We keep stacking the increases so we have also en_inc_total
  // **********************************************************************
  //               calculation of the power per panel
  // **********************************************************************
  ESP_LOGI(TAG, "successfully polled inverter %s", inv->get_serial());

  // 1st the time period
  // at the start of this we have a value of the t_new[which] of the former poll
  // if this is 0 there was no former poll
  switch (inv->get_type()) {
    case InverterType::INVERTER_TYPE_YC600:                      // yc600
      new_data.poll_timestamp = extractValue(34, 4, 1, 0, s_d);  // dataframe timestamp
      break;
    case InverterType::INVERTER_TYPE_QS1:                        // qs1
      new_data.poll_timestamp = extractValue(60, 4, 1, 0, s_d);  // dataframe timestamp
      break;
    case InverterType::INVERTER_TYPE_DS3:                        // ds3 offset 38
      new_data.poll_timestamp = extractValue(76, 4, 1, 0, s_d);  // dataframe timestamp ds3
      break;
  }

  // if the inverter had a reset, time new would be smaller than time old
  // t_saved is globally defined so we remember the last. With the new we can calculate the timeperiod
  if (new_data.poll_timestamp < old_data.poll_timestamp || old_data.poll_timestamp == 0) {  // there has been a reset
    old_data.poll_timestamp = 0;
  }

  int time_since_last_poll = new_data.poll_timestamp - old_data.poll_timestamp;

  // now for each channel
  int increment = 10;  // offset to the next energy value
  int btc = 6;         // amount of bytes
  int offst = 74;      // this is incremented with 10
  if (inv->get_type() == InverterType::INVERTER_TYPE_DS3) {
    // for the DS3 we have different offset/increment
    offst = 100;
    increment = 8;
    btc = 8;
  }
  // for every panel of inverter which we go through this loop

  for (int x = 0; x < 4; x++) {
    if (inv->is_panel_connected(x)) {  // is this panel connected ? otherwise skip

      ESP_LOGV(TAG, "decoding panel %i", x);

      // now we extract a new energy_new[which][x]
      int extracted_energy_value =
          extractValue(offst + x * increment, btc, 1, 0, s_d);  // offset 74 todays module energy channel 0

      // we calculate a new energy value for this panel and remember it
      if (inv->get_type() == InverterType::INVERTER_TYPE_DS3) {
        new_data.energy_since_last_reset[x] = extracted_energy_value / 100000.0f * 1.66f;  //[Wh]
      } else {
        new_data.energy_since_last_reset[x] = extracted_energy_value * 8.311F / 3600.0f;   //[Wh]
      }

      float energy_increase = new_data.energy_since_last_reset[x];
      if (old_data.poll_timestamp != 0) {
        energy_increase -= old_data.energy_since_last_reset[x];
      }

      new_data.energy_today[x] =
          old_data.energy_today[x] + energy_increase;  // totalize the energy increase for this poll

      // calculate the power for this panel
      new_data.dc_power[x] = new_data.dc_voltage[x] * new_data.dc_current[x];
      new_data.ac_power[x] = energy_increase / (time_since_last_poll / 3600.0f);  //[W]

      // reject invalid value ranges
      if (inv->get_type() == InverterType::INVERTER_TYPE_YC600) {
        if (new_data.dc_power[x] > 450 || new_data.ac_power[x] > 450) {
          new_data_valid = false;
        }
      }
      if (inv->get_type() == InverterType::INVERTER_TYPE_QS1) {
        if (new_data.dc_power[x] > 480 || new_data.ac_power[x] > 480) {
          new_data_valid = false;
        }
      }
      if (inv->get_type() == InverterType::INVERTER_TYPE_DS3) {
        if (new_data.dc_power[x] > 750 || new_data.ac_power[x] > 750) {
          new_data_valid = false;
        }
      }
      if (new_data.dc_power[x] < 0 || new_data.ac_power[x] < 0 || new_data.dc_current[x] < 0 ||
          new_data.dc_voltage[x] < 0 || new_data.ac_frequency < 30 || new_data.ac_frequency > 80 ||
          new_data.ac_voltage < 80 || new_data.ac_voltage > 290 || new_data.signal_quality < 0 ||
          new_data.signal_quality > 100) {
        new_data_valid = false;
      }

      new_data.dc_power[4] += new_data.dc_power[x];
      new_data.ac_power[4] += new_data.ac_power[x];
      new_data.energy_since_last_reset[4] += new_data.energy_since_last_reset[x];
      new_data.energy_today[4] += new_data.energy_today[x];
    }
  }  // stack the increase

  ESP_LOGV(TAG, "done parsing poll response");
  if (new_data_valid) {
    inv->set_data(new_data);
  } else {
    ESP_LOGW(TAG, "ignoring invalid data from inverter!");
  }

  yield();
  ESP_LOGV(TAG, "inverter data: %s", inv->get_serial());
  ESP_LOGV(TAG, "                   time = %i", new_data.poll_timestamp);
  ESP_LOGV(TAG, "               timespan = %i", time_since_last_poll);
  ESP_LOGV(TAG, "            temperature = %.2f", new_data.temperature);
  ESP_LOGV(TAG, "         signal_quality = %.2f", new_data.signal_quality);
  ESP_LOGV(TAG, "             ac_voltage = %.2f", new_data.ac_voltage);
  ESP_LOGV(TAG, "              frequency = %.2f", new_data.ac_frequency);
  ESP_LOGV(TAG, "             dc_voltage = %5.2f %5.2f %5.2f %5.2f", new_data.dc_voltage[0], new_data.dc_voltage[1],
           new_data.dc_voltage[2], new_data.dc_voltage[3]);
  ESP_LOGV(TAG, "             dc_current = %5.2f %5.2f %5.2f %5.2f", new_data.dc_current[0], new_data.dc_current[1],
           new_data.dc_current[2], new_data.dc_current[3]);
  ESP_LOGV(TAG, "               dc_power = %8.2f +%8.2f +%8.2f +%8.2f =%9.2f", new_data.dc_power[0],
           new_data.dc_power[1], new_data.dc_power[2], new_data.dc_power[3], new_data.dc_power[4]);
  ESP_LOGV(TAG, "energy_since_last_reset = %8.2f +%8.2f +%8.2f +%8.2f =%9.2f", new_data.energy_since_last_reset[0],
           new_data.energy_since_last_reset[1], new_data.energy_since_last_reset[2],
           new_data.energy_since_last_reset[3], new_data.energy_since_last_reset[4]);
  ESP_LOGV(TAG, "                  power = %8.2f +%8.2f +%8.2f +%8.2f =%9.2f", new_data.ac_power[0],
           new_data.ac_power[1], new_data.ac_power[2], new_data.ac_power[3], new_data.ac_power[4]);
  ESP_LOGV(TAG, "           energy_today = %8.2f +%8.2f +%8.2f +%8.2f =%9.2f", new_data.energy_today[0],
           new_data.energy_today[1], new_data.energy_today[2], new_data.energy_today[3], new_data.energy_today[4]);

  return true;
}
}  // namespace apsystems
}  // namespace esphome
