// Recorder: car CAN bus + both data lines between an aftermarket CAN box and the radio.
// Everything LISTEN ONLY. Output over USB, one clock for all lines:
//   (seconds.microseconds) can0 0E1#0102030405060708     CAN frame (same format as the sniffer)
//   (seconds.microseconds) uartA 2E20021401C8              burst of bytes from line A (hex)
//   (seconds.microseconds) uartB ...                      burst of bytes from line B
// Lines starting with '#' are comments and statistics.
//
// SAFETY:
//  - CAN: TWAI controller in LISTEN_ONLY mode (no transmission, no ACK), the TX wire of the CAN
//    module is not connected, as in the sniffer;
//  - UART: only RX inputs are used. No TX output is assigned to any pin, so the ESP32 cannot
//    send anything to the radio or to the box.

#include <Arduino.h>
#include "driver/twai.h"
#include "driver/gpio.h"
#include <math.h>
#include "board_pins.h"

static const twai_timing_config_t CAN_TIMING = TWAI_TIMING_CONFIG_125KBITS();

// UART bit rate of the box <-> radio lines. 38400 8N1 is the measured rate of the "XP" (Simple
// Soft) protocol on the reference car. Other CAN boxes use other rates (19200 is common):
// check with the "scan" command, change at run time with "baud 19200".
static uint32_t uartBaud = 38400;

static const uint32_t BURST_GAP_US = 4000;   // 4 ms of silence ends a burst (one byte at 19200 lasts 0.52 ms)

struct UartTap {
  HardwareSerial* port;
  const char* name;
  uint8_t buf[96];
  size_t len;
  int64_t tFirst;      // time of the first byte of the burst
  int64_t tLast;       // time of the last byte
  uint32_t bytes;      // bytes since start
};

static UartTap tapA = {&Serial1, "uartA", {0}, 0, 0, 0, 0};
static UartTap tapB = {&Serial2, "uartB", {0}, 0, 0, 0, 0};

static uint32_t rxCount = 0;
static uint32_t lastStatMs = 0;

static bool canStart() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(PIN_CAN_TX, PIN_CAN_RX, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 512;
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &CAN_TIMING, &f) != ESP_OK) return false;
  return twai_start() == ESP_OK;
}

static void uartStart() {
  // Third and fourth parameter: RX pin and TX pin. TX = -1: no output.
  Serial1.end();
  Serial2.end();
  Serial1.begin(uartBaud, SERIAL_8N1, PIN_UART_A, -1);
  Serial2.begin(uartBaud, SERIAL_8N1, PIN_UART_B, -1);
  // Hand over every byte as soon as it arrives (more precise burst time).
  Serial1.setRxFIFOFull(1);
  Serial2.setRxFIFOFull(1);
}

// --- Automatic UART bit rate detection --------------------------------------
// An interrupt on every edge of both lines measures the shortest time between edges, i.e. the
// length of one bit: 104 us = 9600, 52 us = 19200, 26 us = 38400, 8.7 us = 115200.
// The interrupt only reads the pin; it does not touch the UART settings.
struct EdgeMeter {
  volatile int64_t lastEdge;
  volatile uint32_t edges;
  volatile uint32_t hist[5];   // pulses one bit long for: 9600, 19200, 38400, 57600, 115200
};
static EdgeMeter meterA = {0, 0, {0, 0, 0, 0, 0}};
static EdgeMeter meterB = {0, 0, {0, 0, 0, 0, 0}};
// Automatic detection is OFF: with the box switched off the open line picks up noise and the
// result was wrong. Use the "scan" command instead.
static bool baudLocked = true;
static const uint32_t AUTO_RATES[5] = {9600, 19200, 38400, 57600, 115200};

// A pulse counts for a rate when it lasts one bit of that rate, with 20 % tolerance.
// Single spikes (1-5 us) fit no range and are ignored.
static void IRAM_ATTR edgeIsr(void* arg) {
  EdgeMeter* m = (EdgeMeter*)arg;
  int64_t now = esp_timer_get_time();
  uint32_t w = (uint32_t)(now - m->lastEdge);
  m->lastEdge = now;
  m->edges = m->edges + 1;
  if (w >= 84 && w <= 125) m->hist[0] = m->hist[0] + 1;        // 104 us
  else if (w >= 42 && w <= 62) m->hist[1] = m->hist[1] + 1;    // 52 us
  else if (w >= 21 && w <= 31) m->hist[2] = m->hist[2] + 1;    // 26 us
  else if (w >= 14 && w <= 20) m->hist[3] = m->hist[3] + 1;    // 17 us
  else if (w >= 7 && w <= 10) m->hist[4] = m->hist[4] + 1;     // 8.7 us
}

static void edgeMetersStart() {
  gpio_install_isr_service(0);       // returns an error when Arduino already installed it: harmless
  gpio_set_intr_type((gpio_num_t)PIN_UART_A, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type((gpio_num_t)PIN_UART_B, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add((gpio_num_t)PIN_UART_A, edgeIsr, &meterA);
  gpio_isr_handler_add((gpio_num_t)PIN_UART_B, edgeIsr, &meterB);
}

static void edgeMetersStop() {
  gpio_isr_handler_remove((gpio_num_t)PIN_UART_A);
  gpio_isr_handler_remove((gpio_num_t)PIN_UART_B);
}

// Called every 0.5 s while the rate is not fixed.
// Picks the FASTEST rate whose pulses are at least 10 % of all counted pulses
// (at 38400 there are also 52 and 104 us pulses: two and four equal bits in a row).
static void autoBaudPoll() {
  if (baudLocked) return;
  EdgeMeter* m = meterA.edges >= meterB.edges ? &meterA : &meterB;
  uint32_t h[5], sum = 0;
  for (int i = 0; i < 5; i++) { h[i] = m->hist[i]; sum += h[i]; }
  if (sum < 300) return;
  int pick = -1;
  for (int i = 4; i >= 0; i--) {
    if (h[i] * 10 >= sum) { pick = i; break; }
  }
  if (pick < 0) return;
  edgeMetersStop();
  baudLocked = true;
  Serial.printf("# auto: line %c, pulses 104us=%lu 52us=%lu 26us=%lu 17us=%lu 9us=%lu, edges=%lu -> UART rate %lu\n",
                m == &meterA ? 'A' : 'B', (unsigned long)h[0], (unsigned long)h[1], (unsigned long)h[2],
                (unsigned long)h[3], (unsigned long)h[4], (unsigned long)m->edges, (unsigned long)AUTO_RATES[pick]);
  if (AUTO_RATES[pick] != uartBaud) {
    uartBaud = AUTO_RATES[pick];
    uartStart();
  }
}

static void printStamp(int64_t t, const char* name) {
  Serial.printf("(%lld.%06lld) %s ", (long long)(t / 1000000), (long long)(t % 1000000), name);
}

static void flushTap(UartTap& u) {
  if (u.len == 0) return;
  printStamp(u.tFirst, u.name);
  for (size_t i = 0; i < u.len; i++) Serial.printf("%02X", u.buf[i]);
  Serial.println();
  u.len = 0;
}

static void pollTap(UartTap& u) {
  int64_t now = esp_timer_get_time();
  while (u.port->available()) {
    if (u.len == 0) u.tFirst = now;
    u.buf[u.len++] = (uint8_t)u.port->read();
    u.tLast = now;
    u.bytes++;
    if (u.len == sizeof(u.buf)) flushTap(u);
  }
  if (u.len && now - u.tLast > BURST_GAP_US) flushTap(u);
}

// Shortest pulse on both lines: gives the bit rate and the share of time the line is high.
// Reception pauses for the 1.5 s of the measurement; CAN frames wait in the driver queue.
static void scanLines() {
  const int pins[2] = {PIN_UART_A, PIN_UART_B};
  uint32_t minPulse[2] = {0xFFFFFFFF, 0xFFFFFFFF};
  uint32_t edges[2] = {0, 0};
  uint32_t highSamples[2] = {0, 0}, samples = 0;
  int last[2] = {digitalRead(pins[0]), digitalRead(pins[1])};
  int64_t lastEdge[2] = {esp_timer_get_time(), esp_timer_get_time()};
  int64_t end = esp_timer_get_time() + 1500000;
  while (esp_timer_get_time() < end) {
    int64_t now = esp_timer_get_time();
    samples++;
    for (int i = 0; i < 2; i++) {
      int v = digitalRead(pins[i]);
      if (v) highSamples[i]++;
      if (v != last[i]) {
        uint32_t w = (uint32_t)(now - lastEdge[i]);
        if (edges[i] > 0 && w < minPulse[i]) minPulse[i] = w;
        lastEdge[i] = now;
        last[i] = v;
        edges[i]++;
      }
    }
  }
  for (int i = 0; i < 2; i++) {
    Serial.printf("# scan line %c (GPIO%d): edges=%lu, high %lu%% of the time",
                  'A' + i, pins[i], (unsigned long)edges[i], (unsigned long)(100UL * highSamples[i] / (samples ? samples : 1)));
    if (edges[i] > 2) Serial.printf(", shortest pulse=%lu us -> about %lu bit/s",
                                    (unsigned long)minPulse[i], (unsigned long)(1000000UL / (minPulse[i] ? minPulse[i] : 1)));
    Serial.println();
  }
  Serial.println("# scan: common rates: 104 us = 9600, 52 us = 19200, 26 us = 38400, 9 us = 115200");
}

static void pollConsole() {
  static char line[32];
  static size_t n = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      line[n] = 0;
      if (strncmp(line, "baud ", 5) == 0) {
        uint32_t b = strtoul(line + 5, nullptr, 10);
        if (b >= 1200 && b <= 921600) {
          uartBaud = b;
          baudLocked = true;           // a manual setting switches automatic detection off
          uartStart();
          Serial.printf("# uart rate=%lu\n", (unsigned long)uartBaud);
        }
      } else if (strcmp(line, "scan") == 0) {
        scanLines();
      } else if (n > 0) {
        Serial.println("# commands: scan | baud <rate>");
      }
      n = 0;
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);      // do not block reception when the PC is not reading
  delay(1500);
  Serial.printf("# recorder v1 LISTEN-ONLY can=125kbit/s RX=GPIO4 uartA=GPIO%d uartB=GPIO%d %lu 8N1\n",
                PIN_UART_A, PIN_UART_B, (unsigned long)uartBaud);
  // On a driver error the program only reports it: nothing is sent.
  if (!canStart()) Serial.println("# ERROR: TWAI driver did not start");
  uartStart();
  edgeMetersStart();
}

void loop() {
  twai_message_t m;
  // Wait at most 1 ms for a CAN frame, so the UART lines are polled often.
  while (twai_receive(&m, pdMS_TO_TICKS(1)) == ESP_OK) {
    int64_t t = esp_timer_get_time();
    char line[64];
    int n = snprintf(line, sizeof(line), "(%lld.%06lld) can0 ",
                     (long long)(t / 1000000), (long long)(t % 1000000));
    n += snprintf(line + n, sizeof(line) - n, m.extd ? "%08lX#" : "%03lX#", (unsigned long)m.identifier);
    if (m.rtr) {
      n += snprintf(line + n, sizeof(line) - n, "R");
    } else {
      for (int i = 0; i < m.data_length_code && i < 8; i++)
        n += snprintf(line + n, sizeof(line) - n, "%02X", m.data[i]);
    }
    Serial.println(line);
    rxCount++;
    pollTap(tapA);
    pollTap(tapB);
  }
  pollTap(tapA);
  pollTap(tapB);
  pollConsole();

  uint32_t now = millis();
  static uint32_t lastAutoMs = 0;
  if (now - lastAutoMs >= 500) {
    lastAutoMs = now;
    autoBaudPoll();
  }
  if (now - lastStatMs >= 1000) {
    lastStatMs = now;
    twai_status_info_t s;
    if (twai_get_status_info(&s) == ESP_OK) {
      Serial.printf("# stat rx=%lu lost=%lu bus_err=%lu queued=%lu uartA=%lu uartB=%lu baud=%lu%s\n",
                    (unsigned long)rxCount, (unsigned long)s.rx_missed_count,
                    (unsigned long)s.bus_error_count, (unsigned long)s.msgs_to_rx,
                    (unsigned long)tapA.bytes, (unsigned long)tapB.bytes,
                    (unsigned long)uartBaud, baudLocked ? "" : "?");
    }
  }
}
