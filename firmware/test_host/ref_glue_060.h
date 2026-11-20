// REFERENCE TO VERSION 0.6.0. DO NOT CHANGE.
//
// Literal transcription of the glue logic from firmware 0.6.0 onto the lib/core classes of that version:
//   - src/main.cpp:93-96 and :105-145   handling of one CAN frame (handleFrame without the network view and dump),
//   - src/main.cpp:479-511            middle of the main loop: buttons, radio power, RadioLink,
//   - src/main.cpp:515, :524          sensor sound and "sensors active",
//   - src/main.cpp:76-78, :148, :532, :547-549   derived states (bus alive, car busy, engine),
//   - src/radio_uart.cpp:46           frame from the radio -> RadioLink,
//   - src/radio_uart.cpp:71-84        transmission: emptying the queue, 10 ms interval, encode.
// This produces the golden files test_host/golden/. golden_stream --decoder compares the Decoder class with them:
// two independent transcriptions must give the same files byte for byte.
//
// What is NOT here (hardware and network, they stay in src): netOnCanFrame, frame dump "dump", console,
// 12 V outputs, amplifier, test beep "test", LED, acknowledgement byte F3, view "[radio_rx]" ("from radio").
// The console lines have the same texts as conPrintf in main.cpp; they go to the log function (without the \n character).
// (Line numbers refer to the 0.6.0 sources.)
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "can_frame.h"
#include "parking_beeper.h"
#include "power_policy.h"
#include "psa_frames.h"
#include "radio_link.h"
#include "xp_car.h"
#include "xp_keys.h"
#include "xp_keys_custom.h"
#include "xp_protocol.h"

struct RefGlue060 {
    typedef void (*LogFn)(const char* line);

    // ---------- logic (main.cpp:43-49) ----------
    ParkingBeeper beeper;
    PowerPolicy power;
    xp::KeyEncoder keys;
    RadioLink radio;
    xp::ClimateEncoder climate;
    xp::DoorEncoder doors;
    xp::KeyCustomizer keyCustom;

    // ---------- program state (main.cpp:52-68, only what the logic uses) ----------
    psa::BodyState body;
    PowerOutputs powerOut;
    uint32_t lastAnyFrameMs = 0;
    bool haveAnyFrame = false;
    uint32_t last0F6Ms = 0;
    bool have0F6 = false;
    bool haveTempByte = false;
    uint8_t tempRaw = 0;
    bool haveDoors = false;
    bool logKeys = true;
    bool haveLastKey = false;
    xp::KeyEvent lastKey;
    uint32_t lastKeyMs = 0;

    // ---------- radio_uart.cpp:14-22 ----------
    uint32_t framesSent = 0, framesSuppressed = 0;
    uint32_t lastTxMs = 0;

    LogFn log = nullptr;

    static constexpr uint32_t FRAME_0F6_FRESH_MS = 1000;      // main.cpp:71
    static constexpr uint8_t IGNITION_ENGINE_RUNNING = 0x8E;  // main.cpp:72
    static constexpr uint32_t TX_GAP_MS = 10;                 // radio_uart.cpp:22

    void conPrintf(const char* fmt, ...) {
        char line[160];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(line, sizeof(line), fmt, ap);
        va_end(ap);
        if (log) log(line);
    }

    // main.cpp:76-78
    bool carBusyNow(uint32_t now) const {
        return powerOut.reverse || (beeper.frameFresh(now) && beeper.last().anyActive());
    }

    // main.cpp:81-86 (no \n at the end: the line receiver appends it)
    void printParking(const psa::ParkingAid& pa) {
        conPrintf("[park] rear=%u front=%u sound=%u %s interval=0x%02X zones rear %u%u%u front %u%u%u",
                  pa.rearStatus, pa.frontStatus, pa.soundRequest, pa.soundFront ? "front" : "rear",
                  pa.intervalRaw, pa.rearLeft, pa.rearCenter, pa.rearRight,
                  pa.frontLeft, pa.frontCenter, pa.frontRight);
    }

    // main.cpp:91-146 without line 92 (network) and 97-103 (dump)
    void handleFrame(const CanFrame& f, uint32_t now, bool fromBus) {
        if (fromBus) {
            lastAnyFrameMs = now;
            haveAnyFrame = true;
        }

        keys.onCanFrame(f, now);

        xp::Frame sf;
        if (xp::buildSteering(f, sf) || xp::buildTrip(f, sf) || xp::buildOutsideTemp(f, sf)) radio.onState(sf, now);
        if (xp::buildEcoMode(f, sf)) radio.onState(sf, now);
        if (climate.onCanFrame(f, sf)) radio.onState(sf, now);
        uint8_t doorsBefore = doors.doors();
        if (doors.onCanFrame(f, sf)) {
            haveDoors = true;
            radio.onState(sf, now);
            if (doors.doors() != doorsBefore) conPrintf("[car] doors=0x%02X", doors.doors());
        }
        bool lights;
        if (xp::decodeLightsOn(f, lights)) power.onLights(lights, now);

        psa::ParkingAid pa;
        if (psa::decodeParkingAid(f, pa)) {
            const psa::ParkingAid& old = beeper.last();
            bool changed = pa.soundRequest != old.soundRequest || pa.intervalRaw != old.intervalRaw ||
                           pa.rearStatus != old.rearStatus || pa.frontStatus != old.frontStatus ||
                           pa.soundFront != old.soundFront ||
                           pa.nearestFront() != old.nearestFront() || pa.nearestRear() != old.nearestRear();
            beeper.onFrame(pa, now);
            radio.onParking(pa, true, now);
            radio.onState(xp::buildObstacleFlag(pa.anyActive() && pa.nearestZone() < 7), now);
            if (changed) printParking(pa);
            return;
        }
        psa::BodyState b;
        if (psa::decodeBodyState(f, b)) {
            if (b.reverseGear != body.reverseGear) conPrintf("[car] reverse=%u", b.reverseGear);
            body = b;
            power.onBodyState(b, now);
            last0F6Ms = now;
            have0F6 = true;
            haveTempByte = f.len >= 7;
            if (haveTempByte) tempRaw = f.data[6];
        }
    }

    // main.cpp:148
    bool frame0F6Fresh(uint32_t now) const { return have0F6 && (uint32_t)(now - last0F6Ms) < FRAME_0F6_FRESH_MS; }

    // main.cpp:479-508 (without the 12 V outputs from lines 509-510). Returns the state of the radio power outputs.
    PowerOutputs loopStep(uint32_t now, bool flashBusy) {
        power.setLossTolerant(flashBusy);

        keys.update(now);
        xp::KeyEvent ke;
        while (keys.pop(ke)) {
            haveLastKey = true;
            lastKey = ke;
            lastKeyMs = now;
            xp::KeyEvent tx;
            bool send = keyCustom.onEvent(ke, tx);
            if (send) radio.onKey(tx);
            if (logKeys) {
                if (!send) conPrintf("[key] code=%02X state=%u (skipped)", ke.code, ke.state);
                else if (tx.code != ke.code || tx.state != ke.state)
                    conPrintf("[key] code=%02X state=%u -> to radio %02X state=%u", ke.code, ke.state, tx.code, tx.state);
                else conPrintf("[key] code=%02X state=%u", ke.code, ke.state);
            }
        }

        PowerOutputs po = power.update(now);
        if (po.acc != powerOut.acc) conPrintf("[car] acc=%u", po.acc);
        if (!po.acc && powerOut.acc && keyCustom.onIgnitionOff(ke)) radio.onKey(ke);
        powerOut = po;
        radio.onPower(po.acc, now);
        if (!beeper.frameFresh(now)) radio.onParking(beeper.last(), false, now);
        radio.update(now);
        return po;
    }

    // radio_uart.cpp:46
    void onRadioFrame(const xp::Frame& f, uint32_t now) { radio.onRadioFrame(f, now); }

    // radio_uart.cpp:71-84. Returns the number of bytes to send (0 = nothing).
    size_t nextRadioTx(uint32_t nowMs, bool txEnabled, uint8_t* buf, size_t cap) {
        xp::Frame out;
        if (!txEnabled) {
            while (radio.popTx(out)) framesSuppressed++;
            return 0;
        }
        if ((uint32_t)(nowMs - lastTxMs) < TX_GAP_MS) return 0;
        if (!radio.popTx(out)) return 0;
        size_t n = xp::encode(out, buf, cap);
        if (n == 0) return 0;
        framesSent++;
        lastTxMs = nowMs;
        return n;
    }

    // main.cpp:515
    BeeperOutput beeperStep(uint32_t now) { return beeper.update(now); }

    // main.cpp:524
    bool sensorsOn(uint32_t now) const { return beeper.frameFresh(now) && beeper.last().anyActive(); }
    // main.cpp:532
    bool busAlive(uint32_t now) const { return haveAnyFrame && now - lastAnyFrameMs < 1000; }
    // main.cpp:547
    bool accForNet(uint32_t now) const {
        return powerOut.acc && haveAnyFrame && (uint32_t)(now - lastAnyFrameMs) < 5000;
    }
    // main.cpp:549
    bool engineRunning(uint32_t now) const {
        return frame0F6Fresh(now) && body.ignitionRaw == IGNITION_ENGINE_RUNNING;
    }
};
