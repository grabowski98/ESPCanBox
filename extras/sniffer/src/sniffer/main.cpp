// CAN sniffer: LISTEN ONLY.
// Receives every frame on the bus and prints it over USB in candump format:
//   (seconds.microseconds) can0 0E1#0102030405060708
// Lines starting with '#' are comments and statistics.
//
// SAFETY:
//  - the TWAI controller runs in LISTEN_ONLY mode: it transmits no frames, no ACK bits and
//    no error frames;
//  - the TX wire to the CAN module is NOT connected (the module TX input has an internal
//    pull-up = recessive), so transmitting anything is physically impossible.

#include <Arduino.h>
#include "driver/twai.h"
#include "board_pins.h"

// Bus bit rate. 125 kbit/s is the measured rate of the INFO-DIV bus of the reference car
// (Peugeot 508, PSA AEE2010). If bus_err keeps growing and no frames arrive, the rate is
// wrong: change it here (TWAI_TIMING_CONFIG_250KBITS, _500KBITS, ...).
static const twai_timing_config_t CAN_TIMING = TWAI_TIMING_CONFIG_125KBITS();

static uint32_t rxCount = 0;       // frames received since start
static uint32_t lastStatMs = 0;    // time of the last statistics line

static bool canStart() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(PIN_CAN_TX, PIN_CAN_RX, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 512;                                         // room for bursts of frames
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();     // no filter: all IDs
  if (twai_driver_install(&g, &CAN_TIMING, &f) != ESP_OK) return false;
  return twai_start() == ESP_OK;
}

void setup() {
  Serial.begin(115200);          // the rate does not matter on native USB
  Serial.setTxTimeoutMs(0);      // do not block CAN reception when the PC is not reading
  delay(1500);
  Serial.println("# sniffer v1 LISTEN-ONLY 125kbit/s RX=GPIO4");
  // On a driver error the program only reports it: nothing else is started, nothing is sent.
  if (!canStart()) Serial.println("# ERROR: TWAI driver did not start");
}

void loop() {
  twai_message_t m;
  // Drain the receive queue (wait at most 10 ms for the first frame).
  while (twai_receive(&m, pdMS_TO_TICKS(10)) == ESP_OK) {
    int64_t t = esp_timer_get_time();   // microseconds since ESP32 start
    char line[64];
    int n = snprintf(line, sizeof(line), "(%lld.%06lld) can0 ",
                     (long long)(t / 1000000), (long long)(t % 1000000));
    // ID: 3 hex digits for standard frames (11 bit), 8 digits for extended frames (29 bit)
    n += snprintf(line + n, sizeof(line) - n, m.extd ? "%08lX#" : "%03lX#", (unsigned long)m.identifier);
    if (m.rtr) {
      n += snprintf(line + n, sizeof(line) - n, "R");
    } else {
      for (int i = 0; i < m.data_length_code && i < 8; i++)
        n += snprintf(line + n, sizeof(line) - n, "%02X", m.data[i]);
    }
    Serial.println(line);
    rxCount++;
  }

  // Once a second a statistics line: shows at once whether the bit rate is right.
  uint32_t now = millis();
  if (now - lastStatMs >= 1000) {
    lastStatMs = now;
    twai_status_info_t s;
    if (twai_get_status_info(&s) == ESP_OK) {
      // rx = frames received, lost = receive queue overflow, bus_err = errors seen on the bus
      Serial.printf("# stat rx=%lu lost=%lu bus_err=%lu queued=%lu\n",
                    (unsigned long)rxCount, (unsigned long)s.rx_missed_count,
                    (unsigned long)s.bus_error_count, (unsigned long)s.msgs_to_rx);
    }
  }
}
