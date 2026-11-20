#include "sleep_ctrl.h"

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "app_state.h"
#include "audio_out.h"
#include "config.h"
#include "console.h"
#include "net/net.h"
#include "outputs.h"

static uint32_t lastActiveMs = 0;
static bool netStopAsked = false;      // the network was asked to stop before sleep
static uint32_t netStopAskedMs = 0;    // when (first request)

void sleepPoll(bool busAlive, bool accOn, bool holdAwake, uint32_t nowMs) {
#if SLEEP_ENABLED
    // We do not fall asleep when: the bus is alive, the radio still has ACC, the network asks to stay awake, or a computer
    // is connected to USB (then bench work or logging is in progress and the console must work).
    if (busAlive || accOn || holdAwake || HWCDC::isPlugged()) {
        lastActiveMs = nowMs;
        netStopAsked = false;      // falling asleep interrupted: next time we count the time from scratch
        return;
    }
    if ((uint32_t)(nowMs - lastActiveMs) < SLEEP_AFTER_MS) return;

    // --- network: switching off the Wi-Fi radio before sleep (without network netPrepareSleep() returns true at once) ---
    // netPrepareSleep() does not wait: it asks the net_wifi task to stop and says whether the radio has already stopped.
    // Until it has stopped, we return (the loop keeps running, the car may wake up in the meantime). After
    // NET_STOP_WAIT_MS we fall asleep anyway: deep sleep cuts the radio off anyway, and staying awake drains the battery.
    if (!netPrepareSleep()) {
        if (!netStopAsked) {
            netStopAsked = true;
            netStopAskedMs = nowMs;
            return;
        }
        if ((uint32_t)(nowMs - netStopAskedMs) < NET_STOP_WAIT_MS) return;
        conPrintln("[sleep] network did not stop, sleeping anyway");
    }

    // --- safe state before sleep ---
    decoder.canTxForceClose();     // CAN transmitter closed, SLNT high (does nothing without transmission)
    audioSetAmp(false);            // amplifier asleep (on the board additionally pulled down by a 10k resistor)
    outputsAllOff();               // 12 V outputs off (in sleep they are held by resistors to ground)
    statusLed(0, 0, 0);            // the WS2812 LED remembers its colour, so we turn it off explicitly
    // The only delay() outside setup(), left deliberately: a margin to finish sending the colour to the
    // WS2812 LED (24 bits of 1.25 µs each) before deep sleep stops the chip. Whether rgbLedWrite itself waits for
    // the end of the transmission: not verified, hence the margin. The loop loses nothing by this: these are the last milliseconds
    // before sleep, the bus has been silent for SLEEP_AFTER_MS, ACC and the 12 V outputs are already off.
    delay(5);                      // time to send the data to the LED

    // --- wake-up: the first dominant bit on the bus ---
    // The RX output of the CAN module is high at rest, with traffic on the bus it drops to zero.
    // GPIO4 belongs to the RTC domain, so it can wake from deep sleep.
    // The pull-up keeps the high level also when the CAN module is not plugged in.
    rtc_gpio_pullup_en(PIN_CAN_RX);
    rtc_gpio_pulldown_dis(PIN_CAN_RX);
    if (esp_sleep_enable_ext0_wakeup(PIN_CAN_RX, 0) != ESP_OK) {
        lastActiveMs = nowMs;      // failed to set up the wake-up: we stay awake
        netStopAsked = false;
        return;
    }
    // The SLNT pin of the CAN module is in high impedance during sleep; the 10k resistor on the board
    // pulls it up to 3.3 V, so the transmitter stays disabled. Note: pin S of the TJA1051 itself
    // has an internal pull-down to ground (NXP datasheet sect. 7.2.2), so without R3 this would mean normal mode.
    // The TX line (GPIO5) is also in high impedance during sleep: the chip's TXD has a pull-up to VIO (recessive).
    esp_deep_sleep_start();        // there is no return from here: after wake-up the program starts from scratch
#else
    (void)busAlive;
    (void)accOn;
    (void)holdAwake;
    (void)nowMs;
    (void)lastActiveMs;
    (void)netStopAsked;
    (void)netStopAskedMs;
#endif
}

bool sleepWokeFromBus() { return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0; }
