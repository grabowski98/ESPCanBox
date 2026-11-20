// Wi-Fi handling: the device's own network, home network, mDNS, start and stop of the web server.
// Description of the steps, rules and timings: wifi_mgr.h. Here all calls into the WiFi and ESPmDNS libraries
// and the server are executed by a single task "net_wifi" (core 0); the main loop only sets the requested state
// and reads a copy of the state, it never waits for anything.
#include "config.h"

#if NET_ENABLED

#include "wifi_mgr.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "console.h"
#include "http_server.h"
#include "settings.h"
#include "web_auth.h"

// ---------- task ----------
// Static stack and task descriptor. On this port the stack is counted in BYTES (StackType_t = uint8_t).
static StackType_t s_stack[WIFI_TASK_STACK];
static StaticTask_t s_tcb;
static TaskHandle_t s_task = nullptr;
static bool s_begun = false;

// ---------- requested state: written by the main loop, read by the task (volatile variables, no locks) ----------
static volatile bool s_wantOn = false;
static volatile bool s_carBusy = false;
static volatile bool s_engine = false;
static volatile bool s_staReq = false;        // staAttempt pulse; cleared by the task
static volatile bool s_reconfigReq = false;   // wifiMgrReconfigure
static volatile bool s_clearFailReq = false;  // wifiMgrClearFailures ("wifi on")
static volatile bool s_stopNowReq = false;    // wifiMgrRequestStopNow (before sleep)

// Password at the first start (step 1). The task turns on the radio and sets s_pwNeed; the main loop
// generates and saves the password (settings are written only from the main loop), stores the result and clears s_pwNeed.
static volatile bool s_pwNeed = false;
static volatile uint8_t s_pwResult = 0;       // 0 none, 1 saved, 2 failed

// ---------- task state seen from outside ----------
static volatile bool s_radioOff = true;       // the last WiFi.mode(WIFI_OFF) succeeded (or the radio was never started)
static volatile bool s_serving = false;       // phase RUNNING

// Wi-Fi event callbacks (task "arduino_events", core 1, priority 19): only store the code.
static volatile uint8_t s_evDiscReason = 0;

// Task report: Wi-Fi state and events for the USB messages. The task builds it in s_work
// (only the task touches it) and copies it to s_pub under a short critical section.
struct WifiReport {
    WifiStatus st;
    uint32_t warnSeq = 0;              // step longer than WIFI_STEP_WARN_MS: counter, name, time
    const char* warnName = "";
    uint16_t warnMs = 0;
    uint32_t failSeq = 0;              // failed start: counter, step, pause until the next attempt
    const char* failName = "";
    uint32_t retryInS = 0;
};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static WifiReport s_pub;               // under s_mux
static WifiReport s_work;              // task only (and wifiMgrBegin before the task is created)

// ---------- internal task state ----------
static uint8_t s_step = 0;             // 0 = no start-up in progress, 1..6 = next start-up step
static bool s_staOnByStep1 = false;    // step 1 enabled client mode only to generate the password
static bool s_mdnsOn = false;
static bool s_staBegun = false;        // WiFi.begin(name, password) has been called since the radio started
static uint32_t s_retryAtMs = 0;
static uint32_t s_lastRefreshMs = 0;
// Password buffers: static, wiped right after use.
static char s_pass[webauth::PASSWORD_MAX + 1];
static char s_staSsid[SET_SSID_MAX + 1];
static char s_staPass[SET_STA_PASS_MAX + 1];

// Wiping a password buffer; written through volatile so the compiler does not optimise it away.
static void wipe(char* p, size_t n) {
    volatile char* v = p;
    while (n--) *v++ = 0;
}

static void publish() {
    s_work.st.stackFree = (uint16_t)uxTaskGetStackHighWaterMark(nullptr);   // bytes
    portENTER_CRITICAL(&s_mux);   // copy of approx. 130 bytes
    s_pub = s_work;
    portEXIT_CRITICAL(&s_mux);
}

static void copyReport(WifiReport& out) {
    portENTER_CRITICAL(&s_mux);
    out = s_pub;
    portEXIT_CRITICAL(&s_mux);
}

// End of one step: remembers the longest step and reports a step longer than WIFI_STEP_WARN_MS.
// During this time only the net_wifi task was stalled, the main loop kept running.
static void stepDone(const char* name, uint32_t startMs) {
    uint32_t d = millis() - startMs;
    uint16_t d16 = d > 65535 ? 65535 : (uint16_t)d;
    if (d16 > s_work.st.maxStepMs) s_work.st.maxStepMs = d16;
    if (d16 > WIFI_STEP_WARN_MS) {
        s_work.warnSeq++;
        s_work.warnName = name;
        s_work.warnMs = d16;
    }
}

static void clearLinkInfo() {
    WifiStatus& st = s_work.st;
    memset(st.apIp, 0, sizeof(st.apIp));
    st.apChannel = 0;
    st.apStations = 0;
    st.staConnected = false;
    memset(st.staIp, 0, sizeof(st.staIp));
    st.staRssi = 0;
}

// Full stop: server -> mDNS -> radio (wifi_mgr.h, "Stop"). Executed regardless
// of carBusy (does not write flash). httpServerStop may wait for a running request: only this task waits.
static bool fullStop() {
    s_serving = false;
    s_work.st.phase = WifiPhase::STOPPING;
    publish();
    uint32_t t = millis();
    httpServerStop();                // does nothing when the server is already stopped
    stepDone("server stop", t);
    if (s_mdnsOn) {
        t = millis();
        MDNS.end();
        s_mdnsOn = false;
        stepDone("mdns stop", t);
    }
    t = millis();
    // Only WiFi.mode(WIFI_OFF) really turns off the radio driver (and destroys the network interfaces).
    bool ok = WiFi.mode(WIFI_OFF);
    stepDone("radio stop", t);
    s_radioOff = ok;                 // false: the next task iteration will try again
    s_step = 0;
    s_staOnByStep1 = false;
    s_staBegun = false;
    s_pwNeed = false;
    clearLinkInfo();
    return ok;
}

// Failed step 1, 2 or 6: full stop and a pause before the next attempt.
static void failStart(const char* stepName) {
    fullStop();
    WifiStatus& st = s_work.st;
    if (st.startFails < 255) st.startFails++;
    s_work.failSeq++;
    s_work.failName = stepName;
    if (st.startFails >= WIFI_START_TRIES_MAX) {
        st.gaveUp = true;            // no more attempts until "wifi on", wifiMgrReconfigure or a restart
        s_work.retryInS = 0;
    } else {
        uint8_t i = st.startFails - 1;
        if (i > 2) i = 2;
        s_retryAtMs = millis() + WIFI_RETRY_MS[i];
        s_work.retryInS = WIFI_RETRY_MS[i] / 1000;
    }
    st.phase = WifiPhase::FAILED;
}

// Device password into s_pass. false = no valid password (buffer wiped).
static bool readDevicePassword() {
    if (settingsGetDevicePassword(s_pass, sizeof(s_pass)) && webauth::passwordAcceptable(s_pass)) return true;
    wipe(s_pass, sizeof(s_pass));
    return false;
}

// Refresh of the client count, addresses, channel and signal (every WIFI_STATUS_REFRESH_MS, phase RUNNING).
static void refreshStatus() {
    WifiStatus& st = s_work.st;
    st.apStations = WiFi.softAPgetStationNum();
    IPAddress ap = WiFi.softAPIP();
    for (int i = 0; i < 4; i++) st.apIp[i] = ap[i];
    int32_t ch = WiFi.channel();
    st.apChannel = (ch > 0 && ch < 256) ? (uint8_t)ch : 0;
    st.staConnected = WiFi.isConnected();
    if (st.staConnected) {
        IPAddress ip = WiFi.localIP();
        for (int i = 0; i < 4; i++) st.staIp[i] = ip[i];
        st.staRssi = WiFi.RSSI();
    } else {
        memset(st.staIp, 0, sizeof(st.staIp));
        st.staRssi = 0;
    }
    st.staLastReason = s_evDiscReason;
}

// Attempt to connect to the home network (step 4 or the staAttempt pulse). The home network password only
// in a static buffer, wiped right after the call.
static void staConnect() {
    char* ssid = s_staSsid;
    if (!settingsGetSta(ssid, sizeof(s_staSsid), s_staPass, sizeof(s_staPass))) {
        wipe(s_staPass, sizeof(s_staPass));
        return;
    }
    uint32_t t = millis();
    if (!s_staBegun) {
        // WiFi.begin enables client mode (the access point stays: AP+STA mode) and tries once.
        // The library does not retry by itself (setAutoReconnect(false) in step 0).
        wl_status_t r = WiFi.begin(ssid, s_staPass);
        s_staBegun = (r != WL_CONNECT_FAILED);
    } else {
        WiFi.reconnect();            // the same credentials that WiFi.begin remembered
    }
    stepDone("home network", t);
    wipe(s_staPass, sizeof(s_staPass));
}

// One start-up step (wifi_mgr.h, steps 1-6). With carBusy the steps wait.
static void runStep() {
    if (s_carBusy) return;
    WifiStatus& st = s_work.st;
    uint32_t t = millis();
    switch (s_step) {
        case 1: {   // device password
            if (readDevicePassword()) {
                wipe(s_pass, sizeof(s_pass));
                s_pwNeed = false;
                s_step = 2;
                break;
            }
            if (s_pwNeed) break;                       // waiting for the main loop
            if (s_pwResult == 2 || s_pwResult == 1) {  // failed (or saved, but not readable)
                s_pwResult = 0;
                failStart("password");
                break;
            }
            // The radio must be running for esp_fill_random to give true randomness.
            s_radioOff = false;
            bool ok = WiFi.mode(WIFI_STA);
            stepDone("radio (password)", t);
            if (!ok) {
                failStart("radio");
                break;
            }
            s_staOnByStep1 = true;
            s_pwNeed = true;                           // generated and saved by wifiMgrPoll in the main loop
            break;
        }
        case 2: {   // own network: NEVER without a valid password (empty password = open network)
            if (!readDevicePassword()) {
                failStart("password");
                break;
            }
            s_radioOff = false;
            // softAP first enables the interface, then writes the name and password,
            // which is why softAPConfig comes only after it.
            bool ok = WiFi.softAP(st.apSsid, s_pass, NET_AP_CHANNEL, 0, NET_AP_MAX_STATIONS);
            wipe(s_pass, sizeof(s_pass));
            if (ok) {
                IPAddress ip(NET_AP_IP[0], NET_AP_IP[1], NET_AP_IP[2], NET_AP_IP[3]);
                ok = WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
            }
            stepDone("own network", t);
            if (!ok) {
                failStart("own network");              // it calls WiFi.mode(WIFI_OFF) immediately
                break;
            }
            s_step = 3;
            break;
        }
        case 3: {   // current draw and responsiveness; works only after the radio has started, an error does not abort
            WiFi.setSleep(false);
            WiFi.setTxPower((wifi_power_t)NET_TX_POWER_QDBM);
            stepDone("radio power", t);
            s_step = 4;
            break;
        }
        case 4: {   // home network or the access point alone
            st.staConfigured = settingsGetStaSsid(st.staSsid, sizeof(st.staSsid));
            if (!st.staConfigured) {
                if (s_staOnByStep1) {
                    WiFi.mode(WIFI_AP);                // client mode was only for generating the password
                    stepDone("AP mode", t);
                }
            } else if (!s_engine) {
                staConnect();                          // an error does not abort: next attempt on staAttempt
            }
            s_step = 5;
            break;
        }
        case 5: {   // name espcanbox.local; an error does not abort (the panel works at the IP address)
            s_mdnsOn = MDNS.begin(NET_HOSTNAME);
            if (s_mdnsOn) MDNS.addService("http", "tcp", HTTP_PORT);
            stepDone("mdns", t);
            s_step = 6;
            break;
        }
        case 6: {   // web server
            bool ok = httpServerStart();
            stepDone("server", t);
            if (!ok) {
                failStart("server");
                break;
            }
            s_step = 0;
            st.phase = WifiPhase::RUNNING;
            st.startFails = 0;
            st.gaveUp = false;
            s_lastRefreshMs = millis();
            refreshStatus();
            s_serving = true;
            break;
        }
        default:
            s_step = 0;
            break;
    }
}

static void taskMain(void*) {
    for (;;) {
        // Only a 1 ms pause between start-up steps, otherwise wake-up every WIFI_TASK_TICK_MS
        // or by a notification from the main loop.
        bool stepping = s_step != 0 && !s_carBusy && !s_pwNeed;
        ulTaskNotifyTake(pdTRUE, stepping ? 1 : pdMS_TO_TICKS(WIFI_TASK_TICK_MS));
        uint32_t now = millis();
        WifiStatus& st = s_work.st;

        if (s_clearFailReq) {
            s_clearFailReq = false;
            st.startFails = 0;
            st.gaveUp = false;
        }
        bool want = s_wantOn && !s_stopNowReq;
        if (s_reconfigReq) {
            s_reconfigReq = false;
            st.startFails = 0;
            st.gaveUp = false;
            // Stop immediately; start with the new settings by the usual rules (waits for !carBusy).
            if (!s_radioOff || s_step != 0 || st.phase == WifiPhase::RUNNING) fullStop();
            st.phase = WifiPhase::OFF;
            // Home network name into the state immediately (the "wifi" command shows the new one even with
            // the radio off). Settings may be read from any task.
            st.staConfigured = settingsGetStaSsid(st.staSsid, sizeof(st.staSsid));
        }

        if (!want) {
            if (!s_radioOff || s_step != 0 || st.phase == WifiPhase::RUNNING || st.phase == WifiPhase::STOPPING) {
                fullStop();
            }
            if (s_radioOff) st.phase = WifiPhase::OFF;
            s_staReq = false;
        } else if (st.phase == WifiPhase::RUNNING) {
            if (s_staReq) {
                s_staReq = false;
                // Home network: only when saved, not connected, the car does not need the device
                // and the engine is not running (net_policy gives a pulse every 30 s / 5 min).
                if (st.staConfigured && !s_carBusy && !s_engine && !WiFi.isConnected()) staConnect();
            }
            if ((uint32_t)(now - s_lastRefreshMs) >= WIFI_STATUS_REFRESH_MS) {
                s_lastRefreshMs = now;
                refreshStatus();
            }
        } else if (s_step != 0) {
            runStep();
        } else if (!st.gaveUp && !s_carBusy &&
                   (st.startFails == 0 || (int32_t)(now - s_retryAtMs) >= 0)) {
            // Phase OFF or FAILED after the pause: new start from step 1.
            s_pwResult = 0;
            s_step = 1;
            st.phase = WifiPhase::STARTING;
        }
        publish();
    }
}

// Event callback: runs in the "arduino_events" task under the library lock. ONLY stores the code
// into a volatile variable; no WiFi.*, conPrintf, diag*, settings* (wifi_mgr.h).
static void onWifiEvent(arduino_event_t* e) {
    if (e == nullptr) return;
    if (e->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) s_evDiscReason = e->event_info.wifi_sta_disconnected.reason;
}

// ---------- main loop ----------

bool wifiMgrBegin() {
    if (s_begun) return true;
    // Step 0. Network credentials in RAM only (no driver write to NVS), hostname before enabling
    // client mode, no automatic retrying (retrying without pauses breaks the own network).
    WiFi.persistent(false);
    WiFi.setHostname(NET_HOSTNAME);   // written to a library buffer; error = default name, no effect on the panel
    WiFi.setAutoReconnect(false);
    if (WiFi.onEvent(onWifiEvent) == 0) return false;

    // Own network name: bytes 4 and 5 of the MAC address from eFuse (byte 0 = lowest byte of the number).
    // WiFi.macAddress() with the access point alone returns zeros.
    uint64_t mac = ESP.getEfuseMac();
    snprintf(s_work.st.apSsid, sizeof(s_work.st.apSsid), "%s%02X%02X", NET_AP_SSID_PREFIX,
             (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));
    s_work.st.staConfigured = settingsStaConfigured();
    if (s_work.st.staConfigured) settingsGetStaSsid(s_work.st.staSsid, sizeof(s_work.st.staSsid));
    portENTER_CRITICAL(&s_mux);
    s_pub = s_work;
    portEXIT_CRITICAL(&s_mux);

    s_task = xTaskCreateStaticPinnedToCore(taskMain, "net_wifi", WIFI_TASK_STACK, nullptr, WIFI_TASK_PRIO, s_stack,
                                           &s_tcb, WIFI_TASK_CORE);
    if (s_task == nullptr) return false;
    s_begun = true;
    return true;
}

static const char* phaseName(WifiPhase p) {
    switch (p) {
        case WifiPhase::OFF: return "off";
        case WifiPhase::STARTING: return "starting";
        case WifiPhase::RUNNING: return "running";
        case WifiPhase::STOPPING: return "stopping";
        case WifiPhase::FAILED: return "start failed";
    }
    return "?";
}

void wifiMgrPoll(bool wantOn, bool staAttempt, bool carBusy, bool engineRunning, uint32_t nowMs) {
    (void)nowMs;
    if (!s_begun) return;
    bool wake = false;
    if (wantOn != s_wantOn) {
        s_wantOn = wantOn;
        wake = true;
    }
    if (wantOn && s_stopNowReq) {       // the pre-sleep request stays in force until wantOn again
        s_stopNowReq = false;
        wake = true;
    }
    if (carBusy != s_carBusy) {         // also the end of the state: a held-back step resumes immediately
        s_carBusy = carBusy;
        wake = true;
    }
    s_engine = engineRunning;
    if (staAttempt) {
        s_staReq = true;
        wake = true;
    }

    // Start-up step 1: generating and saving the password (settings are written only from the main loop). An NVS write
    // stops CAN reception for a few ms, so not while the car needs the device.
    if (s_pwNeed && !carBusy) {
        char pw[webauth::PASSWORD_MAX + 1];
        bool ok = settingsGeneratePassword(pw, sizeof(pw)) && settingsSetDevicePassword(pw) == SetResult::OK;
        wipe(pw, sizeof(pw));
        s_pwResult = ok ? 1 : 2;
        s_pwNeed = false;
        wake = true;
        conPrintln(ok ? "[wifi] device password generated; the command wifi showpass shows it"
                      : "[wifi] could not store the device password");
    }
    if (wake) xTaskNotifyGive(s_task);

    // Change messages (the net_wifi task itself does not write to USB).
    static WifiReport now;              // static: approx. 130 bytes, not on the loop stack
    static WifiReport prev;
    static bool havePrev = false;
    copyReport(now);
    if (!havePrev) {
        prev = now;
        havePrev = true;
        return;
    }
    const WifiStatus& a = prev.st;
    const WifiStatus& b = now.st;
    if (now.warnSeq != prev.warnSeq) conPrintf("[wifi] step %s took %u ms\n", now.warnName, (unsigned)now.warnMs);
    if (now.failSeq != prev.failSeq) {
        if (b.gaveUp) {
            conPrintf("[wifi] start failed %u times, network off until restart\n", (unsigned)b.startFails);
        } else {
            conPrintf("[wifi] start failed (step: %s), next try in %lu s\n", now.failName,
                      (unsigned long)now.retryInS);
        }
    }
    if (b.phase != a.phase) {
        if (b.phase == WifiPhase::RUNNING) {
            conPrintf("[wifi] network %s running: http://%u.%u.%u.%u (http://%s.local)\n", b.apSsid, b.apIp[0], b.apIp[1],
                      b.apIp[2], b.apIp[3], NET_HOSTNAME);
        } else if (b.phase == WifiPhase::OFF && a.phase != WifiPhase::OFF) {
            conPrintln("[wifi] disabled");
        }
    }
    if (b.staConnected && !a.staConnected) {
        conPrintf("[wifi] connected to the home network: http://%u.%u.%u.%u\n", b.staIp[0], b.staIp[1], b.staIp[2],
                  b.staIp[3]);
    } else if (!b.staConnected && a.staConnected) {
        conPrintf("[wifi] disconnected from the home network (code %u)\n", b.staLastReason);
    }
    if (b.apStations != a.apStations && b.phase == WifiPhase::RUNNING) {
        conPrintf("[wifi] stations on the device network: %u\n", b.apStations);
    }
    prev = now;
}

void wifiMgrReconfigure() {
    if (!s_begun) return;
    s_reconfigReq = true;
    xTaskNotifyGive(s_task);
}

void wifiMgrClearFailures() {
    if (!s_begun) return;
    s_clearFailReq = true;
    xTaskNotifyGive(s_task);
}

bool wifiMgrRequestStopNow() {
    if (!s_begun) return true;
    if (!s_stopNowReq) {
        s_stopNowReq = true;
        xTaskNotifyGive(s_task);
    }
    return s_radioOff && !s_serving;
}

// ---------- any task ----------

void wifiMgrGetStatus(WifiStatus& out) {
    portENTER_CRITICAL(&s_mux);
    out = s_pub.st;
    portEXIT_CRITICAL(&s_mux);
}

bool wifiMgrServing() { return s_serving; }

// ---------- console (main loop) ----------

void wifiMgrPrintInfo() {
    static WifiReport r;
    copyReport(r);
    const WifiStatus& s = r.st;
    conPrintf("[wifi] state: %s\n", phaseName(s.phase));
    conPrintf("[wifi] device network: %s, address http://%u.%u.%u.%u", s.apSsid, NET_AP_IP[0], NET_AP_IP[1],
              NET_AP_IP[2], NET_AP_IP[3]);
    if (s.phase == WifiPhase::RUNNING) conPrintf(", channel %u, stations %u", s.apChannel, s.apStations);
    conPrintln();
    if (!s.staConfigured) {
        conPrintln("[wifi] home network: none");
    } else if (s.staConnected) {
        conPrintf("[wifi] home network: %s, connected, http://%u.%u.%u.%u, signal %d dBm\n", s.staSsid, s.staIp[0],
                  s.staIp[1], s.staIp[2], s.staIp[3], (int)s.staRssi);
    } else {
        conPrintf("[wifi] home network: %s, not connected (last code %u)\n", s.staSsid, s.staLastReason);
    }
    conPrintf("[wifi] name: http://%s.local\n", NET_HOSTNAME);
    conPrintf("[wifi] changes from the home network: %s\n", settingsLanAdmin() ? "allowed (wifi lan on)" : "blocked (wifi lan off)");
    if (s.gaveUp) conPrintf("[wifi] start failed %u times; wifi on tries again\n", (unsigned)s.startFails);
    conPrintf("[wifi] longest step: %u ms, stack headroom: %u B\n", (unsigned)s.maxStepMs, (unsigned)s.stackFree);
    conPrintln("[wifi] password: command wifi showpass");
}

void wifiMgrPrintPassword() {
    char pw[webauth::PASSWORD_MAX + 1];
    // USB only (conUsbPrintf): the password must not reach the ring read by the panel.
    if (settingsGetDevicePassword(pw, sizeof(pw))) conUsbPrintf("[wifi] password: %s\n", pw);
    else conUsbPrintln("[wifi] password: (not generated yet)");
    wipe(pw, sizeof(pw));
}

#endif  // NET_ENABLED
