// State snapshot for the web panel (description in app_status.h). Code cut out of main.cpp (17.11.2026)
// without changing the fields. The whole content only in the program with network.
#include "app_status.h"

#if NET_ENABLED
#include <Arduino.h>
#include <string.h>

#include "app_state.h"
#include "audio_out.h"
#include "can_bus.h"
#include "net/net.h"
#include "outputs.h"
#include "radio_uart.h"

// Fields from canOk to lastKeyMs (diag_types.h). Called only when netStatusDue() returns true,
// i.e. every 200 ms.
void appPublishStatus(uint32_t now, bool busAlive, const BeeperOutput& out) {
    static StatusSnapshot snap;    // approx. 300 bytes: static (net.h)
    snap.canOk = app.canOk;
    snap.audioOk = app.audioOk;
    snap.radioOk = app.radioOk;
    snap.can = canStats();
    snap.busAlive = busAlive;
    snap.canListenOnly = canListenOnly();
    snap.canTxArmNext = app.txArmNext;
    // CAN transmit gate: in the program without transmit hardware state NOT_BUILT and all zeros.
    const TxStats& tx = decoder.canTxStats();
    snap.canTxState = (uint8_t)decoder.canTxState();
    snap.canTxFault = (uint8_t)tx.fault;
    snap.canTxTec = tx.tec;
    snap.canTxRec = tx.rec;
    snap.canTxSent = tx.sent;
    snap.canTxAcked = tx.acked;
    snap.canTxFailed = tx.failed;
    snap.canTxRefused = tx.refused;
    const PowerOutputs& po = decoder.power();
    snap.ignition = po.ignition;
    snap.ignitionRaw = decoder.ignitionRaw();
    snap.ignitionFresh = decoder.ignitionFresh(now);  // an older ignition byte is shown by the panel as "no frame"
    snap.doorsKnown = decoder.doorsKnown();
    snap.acc = po.acc;
    snap.reverse = po.reverse;
    snap.lights = po.illumination;
    snap.doors = decoder.doorState().open;    // vehicle::DOOR_* bits (same values as byte 4 of frame 1A1)
#if HAS_VBAT_DIVIDER
    snap.batteryV = batteryVolts();          // one ADC measurement per snapshot
#else
    snap.batteryV = 0.0f;
#endif
    snap.haveOutsideTemp = decoder.frame0F6Fresh(now) && decoder.haveTempByte();
    snap.outsideTempC = decoder.tempRaw() / 2.0f - 40.0f;
    // Vehicle settings: last frame 260, state sent to the radio in frame 38, last command 80.
    const RawFrame& f260 = decoder.car().f260;
    snap.set260Seen = f260.seen;
    snap.set260Len = f260.len;
    memcpy(snap.set260, f260.d, sizeof(snap.set260));
    snap.set260Ms = f260.ms;
    memcpy(snap.set38, decoder.settings38(), sizeof(snap.set38));
    const SettingCmdStats& sc = decoder.settingCmds();
    snap.setHaveLast = sc.haveLast;
    snap.setLastId = sc.lastId;
    snap.setLastValue = sc.lastValue;
    snap.setLastResult = (uint8_t)sc.lastResult;
    snap.setLastMs = sc.lastMs;
    snap.setLastTookMs = sc.lastTookMs;
    snap.setLastHaveFrame = sc.lastHaveFrame;
    memcpy(snap.setLastFrame, sc.lastFrame, sizeof(snap.setLastFrame));
    snap.setMask = decoder.settingsMask();
    snap.setAccepted = sc.accepted;
    snap.setNotAccepted = sc.notAccepted;
    snap.setRefused = sc.refused;
    snap.park = decoder.beeper().last();
    snap.parkFresh = decoder.beeper().frameFresh(now);
    // Test beeping ("test") also counts as "the device is playing".
    snap.beeping = out.active || app.test.running(now);
    snap.continuous = out.continuous;
    snap.radioTxEnabled = radioUartTxEnabled();
    snap.radio = decoder.link().info();
    snap.radioRx = radioUartParserStats();
    snap.radioTx = radioUartStats();
    snap.txQueue = decoder.link().txQueued();
    snap.txQueueCap = RadioSession::txCapacity();
    snap.txDropped = decoder.link().txDropped();
    snap.txGated = decoder.link().txGated();
    snap.slotsFull = decoder.link().slotsFull();
    snap.keysDropped = decoder.keys().dropped();
    snap.audioVolume = audioVolume();
    uint8_t n = 0;
    for (uint8_t i = 0; i < xp::KeyCustomizer::MAP_SIZE; i++) {
        const xp::KeyCustomizer::Entry& e = decoder.keyMap().entry(i);
        if (!e.used) continue;
        snap.keyMap[n].from = e.from;
        snap.keyMap[n].to = e.to;
        n++;
    }
    snap.keyMapCount = n;
    snap.haveLastKey = decoder.haveLastKey();
    snap.lastKeyCode = decoder.lastKey().code;
    snap.lastKeyState = decoder.lastKey().state;
    snap.lastKeyMs = decoder.lastKeyMs();
    netPublishStatus(snap);
}
#endif  // NET_ENABLED
