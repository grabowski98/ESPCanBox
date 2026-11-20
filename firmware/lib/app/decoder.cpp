#include "decoder.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void Decoder::begin(const DecoderConfig& cfg, LogFn log) {
    cfg_ = cfg;
    log_ = log;
    // Profile and protocol. car = nullptr: the generic profile (listen only, decodes nothing);
    // radio = nullptr: the default protocol XP. A default DecoderConfig points to the reference pair.
    if (!cfg_.car) cfg_.car = &kCarGeneric;
    if (!cfg_.radio) cfg_.radio = &kRadioXp;
    // PSA AEE2010 frame set (CarModel: car state for the settings change and the transmit conditions): decided
    // by a property of the profile, not by which object it is. A profile that decodes ignition with the PSA
    // AEE2010 decoder (frame 0F6) uses the PSA AEE2010 frames, so a copy of the reference profile (another key
    // map, another allow list) gets the same. Any other profile: only its tables and decode functions are used.
    psaCar_ = cfg_.car->decodeBody == &psa::decodeBodyState;
    // The radio gets frames only when it has a UART and a framing of its own (RadioProtocol::encodeFrame).
    radioOn_ = cfg_.radio->uartBaud != 0 && cfg_.radio->encodeFrame != nullptr;
    // Common session (queue, state slots, timing): every frame in it is built by this protocol.
    radio_.setProtocol(cfg_.radio);
    keys_.configure(cfg_.car, cfg_.radio->keyCode);
    // Default radio key remaps of the car profile (PSA AEE2010: 9F->9B, 21->1F; generic: none), only for the
    // radio protocol whose codes they are (CarProfile::keyRemapProtocol; nullptr = any). A table stored in the
    // settings replaces them later (src/main.cpp, parseMap).
    const char* remapFor = cfg_.car->keyRemapProtocol;
    const bool remapOk = remapFor == nullptr || (cfg_.radio->id != nullptr && strcmp(remapFor, cfg_.radio->id) == 0);
    if (remapOk) keyCustom_.setDefaults(cfg_.car->keyRemapDefaults, cfg_.car->keyRemapDefaultCount);
    else keyCustom_.setDefaults(nullptr, 0);
    // Keys with a latch (press = on, next press = off): a property of the radio protocol (XP: A4, moon).
    keyCustom_.setLatchKeys(cfg_.radio->latchKeys, cfg_.radio->latchKeyCount);
    if (!cfg_.settingsFromCan && cfg_.radio == &kRadioXp) {
        // Test only (golden_stream --without 38,3C): constant frames 38 from version 0.7.0.
        // They sit in the data bytes 1..5 of the car state frame; the settings are never decoded then, so the
        // encoder keeps them (RadioProtocol::encodeCarState). XP bytes: only with the XP protocol (review: they
        // used to reach any protocol's encoder as the "previous content").
        static const uint8_t LEGACY_38[5] = {0x02, 0xE1, 0xA9, 0x98, 0x40};
        for (uint8_t i = 0; i < 5; i++) carStateFrame_.data[1 + i] = LEGACY_38[i];
    }
    // CAN transmit gate without hardware (port = nullptr): state NOT_BUILT, every command 80 = refusal.
    gate_.begin(nullptr, cfg_.car);
    changer_.begin(&gate_, vset::ENABLED_DEFAULT);
    parkDiag_.begin(nullptr, false);   // own RAM record; the device hands over its RTC record (attachParkDiag)
}

// Program with transmission: the gate gets the hardware. The item mask stays (set by setSettingsMask).
void Decoder::attachCanTx(CanTxPort* port, bool armed) {
    if (!port) return;
    const uint32_t mask = changer_.enabledMask();
    gate_.begin(port, cfg_.car);       // transmitter disabled, state DISARMED; allow list of the car profile
    gate_.setArmed(armed);
    changer_.begin(&gate_, mask);
    lastFault_ = TxFault::NONE;
}

void Decoder::setCanTxArmed(bool on) { gate_.setArmed(on); }
void Decoder::canTxForceClose() { gate_.forceClose(); }
void Decoder::canTxClearFault() {
    gate_.clearFault();
    lastFault_ = TxFault::NONE;
}

void Decoder::requestSetting(uint8_t id, uint8_t value, uint32_t nowMs) {
    changer_.onRadioSetting(id, value, car_.state(), txConditions(nowMs), nowMs);
    pumpSettings();
}

// Console line: formatted into a buffer on the stack (no allocation), handed over through LogFn.
// The longest line ("[park] ...") has about 80 characters.
void Decoder::logf(const char* fmt, ...) {
    if (!log_) return;
    char line[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    log_(line);
}

void Decoder::formatParkingOf(const psa::ParkingAid& pa, char* buf, size_t n) {
    snprintf(buf, n, "[park] rear=%u front=%u sound=%u %s interval=0x%02X zones rear %u%u%u front %u%u%u\n",
             pa.rearStatus, pa.frontStatus, pa.soundRequest, pa.soundFront ? "front" : "rear",
             pa.intervalRaw, pa.rearLeft, pa.rearCenter, pa.rearRight,
             pa.frontLeft, pa.frontCenter, pa.frontRight);
}

void Decoder::formatParking(char* buf, size_t n) const { formatParkingOf(beeper_.last(), buf, n); }

bool Decoder::carBusy(uint32_t nowMs) const {
    return powerOut_.reverse || (beeper_.frameFresh(nowMs) && beeper_.last().anyActive());
}

bool Decoder::busAlive(uint32_t nowMs) const {
    const CarState& c = car_.state();
    return c.haveAnyFrame && nowMs - c.msAnyFrame < 1000;
}

bool Decoder::frame0F6Fresh(uint32_t nowMs) const {
    return fresh(car_.state().f0F6, nowMs, FRAME_0F6_FRESH_MS);
}

// Ignition for the console, the panel and the network policy. PSA AEE2010: from CarModel (frame 0F6), exactly
// as before. Any other profile: CarModel gets no frames, so the last frame accepted by the profile's decodeBody
// is used. The CAN transmit conditions (txConditions) do NOT use this: they stay PSA AEE2010 only.
bool Decoder::ignitionFresh(uint32_t nowMs) const {
    if (psaCar_) return frame0F6Fresh(nowMs);
    return haveBody_ && (uint32_t)(nowMs - bodyMs_) < FRAME_0F6_FRESH_MS;
}

uint8_t Decoder::ignitionRaw() const {
    return psaCar_ ? car_.state().body.ignitionRaw : body_.ignitionRaw;
}

// ---------- CAN frame (main.cpp 0.6.0: handleFrame) ----------
void Decoder::onCanFrame(const CanFrame& f, uint32_t now, bool fromBus) {
    // Car state (bus traffic, 0F6, doors, raw settings frames): alongside the encoders below,
    // sends nothing. Reverse from before this frame is remembered for the "[car] reverse" line.
    const vehicle::CarProfile& car = *cfg_.car;
    // Other car profiles: only bus traffic counts; CarModel (car state for the settings change) is PSA AEE2010
    // frames and run only with that profile, exactly as before.
    bool reverseBefore = psaCar_ ? car_.state().body.reverseGear : body_.reverseGear;
    if (!psaCar_) car_.onBusTraffic(now, fromBus);
    if (psaCar_) {
        car_.onCanFrame(f, now, fromBus);
        // Personalization reference state: first complete frame 260 since startup (the "vset" command shows the difference).
        if (f.id == psa::ID_PERSONALIZATION && !start260_.seen && car_.state().f260.len == 7) start260_ = car_.state().f260;
        // Parking sensors state for the overnight record: the first stable 126 after a wake-up is compared with
        // the one stored before sleep (park_diag.h). Only frames from the bus.
        uint8_t was = 0, st = 0;
        if (f.id == psa::ID_PARKING_COMMAND && fromBus && parkDiag_.on126(car_.state(), now, was, st))
            logf("[park] sensors state changed while asleep: was %s, now %s\n", was ? "OFF" : "ON", st ? "OFF" : "ON");
    }
    keys_.onCanFrame(f, now);              // buttons from the profile key map (PSA: frames 21F, 122, 0A2, 221, 036)

    onCanFrameCarData(f, now);             // car data for the radio screen: steering angle ... vehicle settings

    // Signals through the car profile (PSA AEE2010: frames 036, 0E1, 0F6; the same functions as before).
    bool lights;
    if (car.decodeLights && car.decodeLights(f, lights)) power_.onLights(lights, now);

    psa::ParkingAid pa;
    if (car.decodeParking && car.decodeParking(f, pa)) {
        const psa::ParkingAid& old = beeper_.last();
        bool changed = pa.soundRequest != old.soundRequest || pa.intervalRaw != old.intervalRaw ||
                       pa.rearStatus != old.rearStatus || pa.frontStatus != old.frontStatus ||
                       pa.soundFront != old.soundFront ||
                       pa.nearestFront() != old.nearestFront() || pa.nearestRear() != old.nearestRear();
        beeper_.onFrame(pa, now);
        radio_.onParking(pa, true, now);
        RadioFrame of;
        const RadioProtocol& rp = *cfg_.radio;
        if (rp.encodeObstacle && rp.encodeObstacle(pa.anyActive() && pa.nearestZone() < 7, of)) radio_.onState(of, now);
        if (changed && log_) {
            char line[160];
            formatParkingOf(pa, line, sizeof(line));
            log_(line);
        }
        // No return here: decodeBody gets every frame too (a car may carry reverse or ignition in its
        // parking frame). PSA AEE2010: decodeBodyState accepts only 0F6, so nothing changes for 0E1.
    }
    psa::BodyState b;
    if (car.decodeBody && car.decodeBody(f, b)) {
        if (b.reverseGear != reverseBefore) logf("[car] reverse=%u\n", b.reverseGear);
        body_ = b;
        haveBody_ = true;
        bodyMs_ = now;                     // freshness of the ignition for profiles other than PSA AEE2010
        power_.onBodyState(b, now);
        // The content and time of this frame (freshness, ignition, outside temperature) are kept by CarModel:
        // car().f0F6 and car().body.
    }
}

// Car data for the radio screen: the car profile decodes the frame into a car-neutral type, the radio
// protocol encodes that type into a radio frame, the radio session (RadioSession) sends it only when the content changes.
// Neither side is named here: a nullptr on either side means "this car / this radio has no such data".
// The order of the calls is the order of the former PSA AEE2010 -> XP glue (steering angle, trip computer,
// outside temperature, economy mode, climate control, doors, vehicle settings); with the reference profile
// every CAN frame matches at most one of them, so the radio frames go out in the same order as before.
void Decoder::onCanFrameCarData(const CanFrame& f, uint32_t now) {
    const vehicle::CarProfile& car = *cfg_.car;
    const RadioProtocol& rp = *cfg_.radio;
    RadioFrame sf;
    if (car.decodeSteering && car.decodeSteering(f, steering_) && rp.encodeSteering &&
        rp.encodeSteering(steering_, sf))
        radio_.onState(sf, now);
    if (car.decodeTrip) {
        const uint8_t pages = car.decodeTrip(f, trip_);
        static const uint8_t PAGES[3] = {vehicle::TRIP_NOW, vehicle::TRIP_1, vehicle::TRIP_2};
        for (uint8_t i = 0; i < 3; i++)
            if ((pages & PAGES[i]) && rp.encodeTrip && rp.encodeTrip(trip_, PAGES[i], sf)) radio_.onState(sf, now);
    }
    if (car.decodeOutsideTemp && car.decodeOutsideTemp(f, outsideTemp_) && rp.encodeOutsideTemp &&
        rp.encodeOutsideTemp(outsideTemp_, sf))
        radio_.onState(sf, now);
    if (car.decodeEcoMode && car.decodeEcoMode(f, eco_) && rp.encodeEcoMode && rp.encodeEcoMode(eco_, sf))
        radio_.onState(sf, now);
    if (car.decodeClimate && car.decodeClimate(f, climate_) && rp.encodeClimate && rp.encodeClimate(climate_, sf))
        radio_.onState(sf, now);
    const uint8_t doorsBefore = doorState_.open;
    if (car.decodeDoors && car.decodeDoors(f, doorState_)) {
        if (rp.encodeCarState && rp.encodeCarState(doorState_, settings_, doorSwap_, carStateFrame_)) {
            haveCarStateFrame_ = true;
            radio_.onState(carStateFrame_, now);
        }
        if (doorState_.open != doorsBefore) logf("[car] doors=0x%02X\n", doorState_.open);
    }
    // Vehicle settings: values -> the car state frame again (doors + settings), "which items the car has" ->
    // its own frame. Read only: nothing goes to CAN. cfg_.settingsFromCan = false only in the test of
    // version 0.7.0 behaviour (constant setting bytes, see begin()).
    if (cfg_.settingsFromCan && car.decodeSettings) {
        const uint8_t what = car.decodeSettings(f, settings_);
        if ((what & vehicle::SETTINGS_VALUES) && rp.encodeCarState &&
            rp.encodeCarState(doorState_, settings_, doorSwap_, carStateFrame_)) {
            haveCarStateFrame_ = true;
            radio_.onState(carStateFrame_, now);
        }
        if ((what & vehicle::SETTINGS_PRESENT) && rp.encodeSettingsPresent && rp.encodeSettingsPresent(settings_, sf)) {
            presentCmd_ = sf.cmd;
            havePresent_ = true;
            radio_.onState(sf, now);
        }
    }
}

// ---------- frame from the radio ----------
void Decoder::onRadioFrame(const RadioFrame& f, uint32_t now) {
    radio_.onRadioFrame(f, now);
    // Commands only through the protocol: a radio without decodeCommand never reaches the CAN side.
    RadioCmd c;                               // frame too short or not a command = NONE: we do nothing
    if (!cfg_.radio->decodeCommand || !cfg_.radio->decodeCommand(f, c)) c = RadioCmd();
    // The radio reports "on" (XP: 81 01, after startup 4 times about every 1 s): the box answers with the pair
    // car state + settings present (XP 38 + 3C) with the current content (measured 11.11.2026,
    // vehicles/peugeot-508-sw-2015/settings-map.md). We do the same, in this order; the second frame is not sent
    // until the car has reported which settings it has (PSA AEE2010: frame 361). The frame numbers are the ones
    // the protocol's encoders gave.
    // A (re)started radio has the screen on: forget the latch keys (XP: moon A4), so that the next press blanks.
    if (c.kind == RadioCmdKind::RADIO_ON) keyCustom_.onRadioOn();
    if (cfg_.settingsFromCan && c.kind == RadioCmdKind::RADIO_ON) {
        if (haveCarStateFrame_) radio_.resendState(carStateFrame_.cmd);
        if (havePresent_) radio_.resendState(presentCmd_);
    }
    // SETTING / AIR / SOUND carry the numbers of the reference command table (vset) only when the protocol says
    // so (RadioProtocol::commandsUseVsetNumbers; XP: yes). Any other protocol: there is no car-neutral command
    // path yet, so the numbers are not interpreted at all (review: a foreign "setting 12" was taken for the XP
    // item 0C and printed with XP command bytes).
    const bool commandKind = c.kind == RadioCmdKind::SETTING || c.kind == RadioCmdKind::AIR || c.kind == RadioCmdKind::SOUND;
    if (commandKind && !cfg_.radio->commandsUseVsetNumbers) {
        if (cfg_.logRadioCommands)
            logf("[radio] command %s %u = %u: not supported with this radio protocol (no action)\n",
                 c.kind == RadioCmdKind::SETTING ? "setting" : c.kind == RadioCmdKind::AIR ? "climate" : "sound", c.id,
                 c.value);
        return;
    }
    // Command 80: through SettingsChanger. The gate has no hardware (NOT_BUILT), so the refusal report
    // is produced immediately, in the same call; nothing is transmitted.
    if (c.kind == RadioCmdKind::SETTING) {
        changer_.onRadioSetting(c.id, c.value, car_.state(), txConditions(now), now);
        pumpSettings();
    }
    if (cfg_.logRadioCommands) logRadioCommand(c);
}

// Conditions for the transmit gate: fresh 0F6 (younger than 1 s) with ignition (88) or running
// engine (8E), bus traffic, no flash write and no program update session,
// no pending settings write (both from setTxBlock; without the call = false).
// Frame 0F6 is read by CarModel, which runs only for the PSA AEE2010 frame set (psaCar_): with any other
// profile ignitionOk stays false and the gate refuses every frame (such a profile needs its own condition first).
TxConditions Decoder::txConditions(uint32_t nowMs) const {
    TxConditions c;
    const vehicle::Ignition ign = car_.state().body.ignition;     // PSA AEE2010: 8E = ENGINE, 88 = ON
    c.ignitionOk = frame0F6Fresh(nowMs) && !car_.state().f0F6.pasted &&
                   (ign == vehicle::Ignition::ENGINE || ign == vehicle::Ignition::ON);
    c.busAlive = busAlive(nowMs);
    c.flashBusy = flashBusy_ || otaBusy_;
    c.nvsPending = nvsPending_;
    return c;
}

// Results from SettingsChanger and the gate to the console. Called after every onRadioSetting and tick.
// In the program without transmission there are no send or latch events: only the report remains, as before.
void Decoder::pumpSettings() {
    vset::TxEvent ev;
    const bool haveEv = changer_.takeTxEvent(ev);
    // Count only a 1A9 press that was handed to the controller (not one the gate ended before send()).
    if (haveEv && ev.canId == vset::ID_BUTTONS && ev.handedOver) parkDiag_.notePress();
    if (haveEv && cfg_.logRadioCommands) {
        const uint8_t* b = ev.frame;
        if (ev.acked)
            logf("[vset] %03X %02X %02X %02X %02X %02X %02X %02X %02X sent (acked after %u ms)\n", ev.canId, b[0], b[1],
                 b[2], b[3], b[4], b[5], b[6], b[7], ev.ms);
        else
            logf("[vset] %03X %02X %02X %02X %02X %02X %02X %02X %02X NOT ACKED (after %u ms)\n", ev.canId, b[0], b[1], b[2],
                 b[3], b[4], b[5], b[6], b[7], ev.ms);
    }
    // Release frame after a 1A9 press (only with -DCAN_TX_1A9_RELEASE=1; never produced otherwise).
    vset::ReleaseEvent re;
    if (changer_.takeReleaseEvent(re)) {
        if (re.handedOver) parkDiag_.noteRelease();
        if (cfg_.logRadioCommands) {
            const uint8_t* b = re.frame;
            if (re.sent && re.acked)
                logf("[vset] 1A9 %02X %02X %02X %02X %02X %02X %02X %02X release sent (acked after %u ms, %u ms after the press)\n",
                     b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], re.ms, re.afterPressMs);
            else if (re.sent)
                logf("[vset] 1A9 %02X %02X %02X %02X %02X %02X %02X %02X release NOT ACKED (after %u ms)\n", b[0], b[1],
                     b[2], b[3], b[4], b[5], b[6], b[7], re.ms);
            else
                logf("[vset] 1A9 %02X %02X %02X %02X %02X %02X %02X %02X release not sent (%s)\n", b[0], b[1], b[2], b[3],
                     b[4], b[5], b[6], b[7], re.refuse == TxRefuse::BUSY ? "gate still busy after 1 s" : txRefuseName(re.refuse));
        }
    }
    vset::ChangeReport rep;
    if (changer_.takeReport(rep)) onSettingReport(rep);
    const TxFault fault = gate_.stats().fault;
    if (fault != lastFault_) {
        lastFault_ = fault;
        if (fault != TxFault::NONE)
            logf("[cantx] FAULT: %s, transmit locked until \"cantx reset\"\n", txFaultName(fault));
    }
}

// SettingsChanger report: data for the panel (car.settings.last, counters) and the "[vset]" console line.
// In the program without transmission (txRefuse = NOT_BUILT) the text depends only on the kind of table item,
// not on the car state: the same three endings as in earlier versions.
void Decoder::onSettingReport(const vset::ChangeReport& r) {
    const vset::SettingDef* def = vset::find(r.id);
    const char* name = def ? def->name : "unknown setting";
    const char* text = vset::changeResultName(r.result);
    SettingCmdResult res = SettingCmdResult::REFUSED;
    switch (r.result) {
        case vset::ChangeResult::ACCEPTED: res = SettingCmdResult::ACCEPTED; setCmd_.accepted++; break;
        case vset::ChangeResult::NOT_ACCEPTED: res = SettingCmdResult::NOT_ACCEPTED; setCmd_.notAccepted++; break;
        case vset::ChangeResult::ALREADY: res = SettingCmdResult::ALREADY; break;
        case vset::ChangeResult::TX_FAILED: res = SettingCmdResult::TX_FAILED; break;
        case vset::ChangeResult::SIDE_EFFECT: res = SettingCmdResult::SIDE_EFFECT; break;
        default:                                                                 // REFUSED
            setCmd_.refused++;
            if (r.refuse == vset::Refuse::UNKNOWN_ID || r.refuse == vset::Refuse::NO_FRAME_FOR_ID) {
                res = SettingCmdResult::NO_FRAME;                                // group E and numbers outside the table
                text = "no known frame";
            } else if (r.refuse == vset::Refuse::BLOCKED_OTHER_FRAME) {
                res = SettingCmdResult::BLOCKED;
                text = "known frame (1A9), transmit blocked until measured";
            } else if (r.txRefuse == TxRefuse::NOT_BUILT) {
                text = "REFUSED (no CAN transmit in this build)";
            } else {
                text = r.refuse != vset::Refuse::OK ? vset::refuseName(r.refuse) : txRefuseName(r.txRefuse);
            }
            break;
    }
    setCmd_.haveLast = true;
    setCmd_.lastId = r.id;
    setCmd_.lastValue = r.value;
    setCmd_.lastResult = res;
    setCmd_.lastMs = r.ms;
    setCmd_.lastTookMs = r.tookMs;
    setCmd_.lastHaveFrame = r.haveFrame;
    for (int i = 0; i < 8; i++) setCmd_.lastFrame[i] = r.frame[i];
    if (!cfg_.logRadioCommands) return;
    // Results possible only in the program with transmission. Refusals still have
    // the form "[vset] radio: 80 ..." (also for the "set NN VV" command, which takes the same path).
    const uint8_t* ob = r.otherBits;
    switch (r.result) {
        case vset::ChangeResult::ACCEPTED:
            logf("[vset] %s = %u: ACCEPTED by BSI after %u ms\n", name, r.value, r.tookMs);
            return;
        case vset::ChangeResult::NOT_ACCEPTED:
            logf("[vset] %s = %u: NOT ACCEPTED by BSI (%s unchanged after %u ms)\n", name, r.value,
                 r.canId == vset::ID_BUTTONS ? "state" : "260", r.tookMs);
            return;
        case vset::ChangeResult::ALREADY:
            logf("[vset] %s = %u: unchanged (the car already has this state), nothing sent\n", name, r.value);
            return;
        case vset::ChangeResult::TX_FAILED:
            logf("[vset] %s = %u: frame %03X not acked on the bus, car state unchanged\n", name, r.value, r.canId);
            return;
        case vset::ChangeResult::SIDE_EFFECT:
            logf("[vset] %s = %u: %s, but other bits of 260 changed too (%02X %02X %02X %02X %02X): transmit locked\n",
                 name, r.value, vset::accepted(car_.state().f260, r.id, r.value) ? "ACCEPTED" : "NOT ACCEPTED", ob[0], ob[1],
                 ob[2], ob[3], ob[4]);
            return;
        default:
            break;
    }
    if (res == SettingCmdResult::REFUSED && r.txRefuse != TxRefuse::NOT_BUILT)
        logf("[vset] radio: 80 %02X %02X %s = %u: REFUSED (%s)\n", r.id, r.value, name, r.value, text);
    else
        logf("[vset] radio: 80 %02X %02X %s = %u: %s\n", r.id, r.value, name, r.value, text);
}

const char* settingCmdResultName(SettingCmdResult r) {
    switch (r) {
        case SettingCmdResult::REFUSED: return "refused";
        case SettingCmdResult::NO_FRAME: return "no_frame";
        case SettingCmdResult::BLOCKED: return "blocked";
        case SettingCmdResult::ALREADY: return "unchanged";
        case SettingCmdResult::ACCEPTED: return "accepted";
        case SettingCmdResult::NOT_ACCEPTED: return "not_accepted";
        case SettingCmdResult::TX_FAILED: return "tx_failed";
        case SettingCmdResult::SIDE_EFFECT: return "side_effect";
    }
    return "refused";
}

// Radio commands 8A (climate control) and 84 (sound), which we do not execute: only a console
// line with the name. The "[vset]" line for command 80 is printed by onSettingReport (SettingsChanger report).
// Nothing is sent to the car for 8A: single 364 frames did not change the climate control of the reference
// car (vehicles/peugeot-508-sw-2015/settings-map.md), so the program has no climate transmit path.
void Decoder::logRadioCommand(const RadioCmd& c) {
    if (c.kind == RadioCmdKind::AIR) {
        const char* name = airCommandName(c.id);
        if (!name)
            logf("[climate] radio: 8A %02X %02X unknown no. %u = %u (no action)\n", c.id, c.value, c.id, c.value);
        else if (airIsPlusMinus(c.id) && (c.value == 1 || c.value == 2))        // 1 = plus, 2 = minus
            logf("[climate] radio: 8A %02X %02X %s %s (no action)\n", c.id, c.value, name,
                 c.value == 1 ? "plus" : "minus");
        else
            logf("[climate] radio: 8A %02X %02X %s = %u (no action)\n", c.id, c.value, name, c.value);
    } else if (c.kind == RadioCmdKind::SOUND) {
        const char* name = soundCommandName(c.id);
        if (!name)
            logf("[sound] radio: 84 %02X %02X unknown no. %u = %u (no action)\n", c.id, c.value, c.id, c.value);
        else
            logf("[sound] radio: 84 %02X %02X %s = %u (no action)\n", c.id, c.value, name, c.value);
    }
}

// ---------- once per loop pass (main.cpp 0.6.0: middle of loop()) ----------
const DecoderOutputs& Decoder::tick(uint32_t now, bool flashBusy) {
    // A flash memory write (update over Wi-Fi) loses 0F6 frames: during that time a longer freshness
    // limit, so that a lost frame does not switch off the radio's ACC.
    power_.setLossTolerant(flashBusy);

    // --- buttons: CAN -> events -> frames for the radio ---
    keys_.update(now);
    xp::KeyEvent ke;
    while (keys_.pop(ke)) {
        haveLastKey_ = true;
        lastKey_ = ke;
        lastKeyMs_ = now;
        xp::KeyEvent tx;
        bool send = keyCustom_.onEvent(ke, tx);
        if (send) radio_.onKey(tx);
        if (logKeys_) {
            if (!send) logf("[key] code=%02X state=%u (skipped)\n", ke.code, ke.state);
            else if (tx.code != ke.code || tx.state != ke.state)
                logf("[key] code=%02X state=%u -> to radio %02X state=%u\n", ke.code, ke.state, tx.code, tx.state);
            else logf("[key] code=%02X state=%u\n", ke.code, ke.state);
        }
    }

    // --- radio power: ACC and reverse ---
    PowerOutputs po = power_.update(now);
    if (po.acc != powerOut_.acc) logf("[car] acc=%u\n", po.acc);
    // ACC goes off: every latched key (XP: the screen blanked with the moon button) first gets "released".
    if (!po.acc && powerOut_.acc)
        while (keyCustom_.onIgnitionOff(ke)) radio_.onKey(ke);
    // ACC comes on: latch keys pressed while it was off were never seen by the radio, forget them.
    if (po.acc && !powerOut_.acc) keyCustom_.onAccOn();
    powerOut_ = po;
    radio_.onPower(po.acc, now);
    // Without fresh 0E1 frames the radio must get "no data", not the last graph.
    if (!beeper_.frameFresh(now)) radio_.onParking(beeper_.last(), false, now);
    radio_.update(now);

    // --- sensor sound and derived states ---
    out_.power = po;
    out_.beep = beeper_.update(now);
    out_.sensorsOn = beeper_.frameFresh(now) && beeper_.last().anyActive();
    out_.busAlive = busAlive(now);
    // ACC counts for the network only with real bus traffic in the last 5 s
    // (frames pasted from the panel do not extend standby or Wi-Fi).
    const CarState& c = car_.state();
    out_.accForNet = powerOut_.acc && c.haveAnyFrame && (uint32_t)(now - c.msAnyFrame) < 5000;
    out_.carBusy = carBusy(now);
    const vehicle::Ignition ign = psaCar_ ? c.body.ignition : body_.ignition;
    out_.engineRunning = ignitionFresh(now) && ign == vehicle::Ignition::ENGINE;

    // --- vehicle settings change and the CAN transmit gate (logic only; gate without hardware: does nothing) ---
    flashBusy_ = flashBusy;
    const TxConditions tc = txConditions(now);
    changer_.tick(car_.state(), tc, now);
    gate_.tick(tc, now);
    pumpSettings();
    return out_;
}

// ---------- transmission to the radio (radio_uart.cpp 0.6.0) ----------
size_t Decoder::nextRadioTx(uint32_t nowMs, bool txEnabled, uint8_t* buf, size_t cap) {
    RadioFrame out;
    if (!txEnabled || !radioOn_) {             // radio "none" or without framing: never anything to send
        // Transmission disabled: we empty the queue so that it does not overflow.
        while (radio_.popTx(out)) framesSuppressed_++;
        return 0;
    }
    if ((uint32_t)(nowMs - lastTxMs_) < cfg_.radio->txGapMs) return 0;
    if (!radio_.popTx(out)) return 0;
    size_t n = cfg_.radio->encodeFrame(out, buf, cap);   // the protocol's own framing
    if (n == 0) {                            // frame does not fit in the buffer: skipped, but counted
        framesTooLong_++;
        return 0;
    }
    framesSent_++;
    lastTxMs_ = nowMs;
    return n;
}

// ---------- console commands ----------
// Frame bytes as "A4 1C 8F" or "none" when the frame has not arrived yet.
static void hexOf(const RawFrame& r, char* buf, size_t n) {
    if (n == 0) return;
    buf[0] = 0;
    if (!r.seen) {
        snprintf(buf, n, "none");
        return;
    }
    size_t pos = 0;
    for (uint8_t i = 0; i < r.len && i < 8 && pos + 4 < n; i++)
        pos += (size_t)snprintf(buf + pos, n - pos, i ? " %02X" : "%02X", r.d[i]);
}

void Decoder::formatSettingsHeader(uint32_t nowMs, char* buf, size_t n) const {
    const CarState& c = car_.state();
    char h260[32], h361[32];
    hexOf(c.f260, h260, sizeof(h260));
    hexOf(c.f361, h361, sizeof(h361));
    if (gate_.state() == TxState::NOT_BUILT)
        snprintf(buf, n, "[vset] 260=%s age=%lu ms, 361=%s, mask=0x%08lX, no CAN transmit in this build\n", h260,
                 c.f260.seen ? (unsigned long)(uint32_t)(nowMs - c.f260.ms) : 0UL, h361,
                 (unsigned long)changer_.enabledMask());
    else
        snprintf(buf, n, "[vset] 260=%s age=%lu ms, 361=%s, mask=0x%08lX, transmit 15B: %s\n", h260,
                 c.f260.seen ? (unsigned long)(uint32_t)(nowMs - c.f260.ms) : 0UL, h361,
                 (unsigned long)changer_.enabledMask(), txStateName(gate_.state()));
}

// Difference of the current frame 260 against the first complete frame 260 since startup (bytes 2-6): shows
// what changed in the personalization since the program started, also without the unit's involvement.
void Decoder::formatSettingsDiff(char* buf, size_t n) const {
    const RawFrame& cur = car_.state().f260;
    if (!start260_.seen || !cur.seen || cur.len != 7) {
        snprintf(buf, n, "[vset] 260 at startup=none\n");
        return;
    }
    char h[32];
    hexOf(start260_, h, sizeof(h));
    uint8_t x[5];
    for (int i = 0; i < 5; i++) x[i] = (uint8_t)(start260_.d[1 + i] ^ cur.d[1 + i]);
    snprintf(buf, n, "[vset] 260 at startup=%s changed bits (bytes 2-6)=%02X %02X %02X %02X %02X\n", h, x[0], x[1],
             x[2], x[3], x[4]);
}

void Decoder::formatCanTxStatus(char* buf, size_t n) const {
    const TxStats& t = gate_.stats();
    snprintf(buf, n, "[cantx] state=%s sent=%lu acked=%lu failed=%lu refused=%lu tec=%u rec=%u fault=%s\n",
             txStateName(gate_.state()), (unsigned long)t.sent, (unsigned long)t.acked, (unsigned long)t.failed,
             (unsigned long)t.refused, t.tec, t.rec, txFaultName(t.fault));
}

void Decoder::formatCanTx(char* buf, size_t n) const {
    const TxStats& t = gate_.stats();
    const char* st = "ready";                       // IDLE, OPENING, SENDING, COOLDOWN
    if (gate_.state() == TxState::NOT_BUILT) st = "none";
    else if (gate_.state() == TxState::DISARMED) st = "disarmed";
    else if (gate_.state() == TxState::FAULT) st = "FAIL";
    snprintf(buf, n, "[stat] can-tx=%s sent=%lu acked=%lu failed=%lu refused=%lu\n", st,
             (unsigned long)t.sent, (unsigned long)t.acked, (unsigned long)t.failed, (unsigned long)t.refused);
}

void Decoder::formatParkDiag(char* buf, size_t n) const {
    const psa::ParkDiagRecord& r = parkDiag_.record();
    snprintf(buf, n, "[diag] wakes=%lu park-flips-asleep=%lu 1A9 press=%lu release=%lu\n", (unsigned long)r.wakes,
             (unsigned long)r.flipsAsleep, (unsigned long)r.presses, (unsigned long)r.releases);
}

void Decoder::formatSettingRow(uint8_t i, uint32_t nowMs, char* buf, size_t n) const {
    if (i >= vset::count()) {
        if (n > 0) buf[0] = 0;
        return;
    }
    const vset::SettingDef& d = vset::at(i);
    const CarState& c = car_.state();
    uint8_t state = 0;
    const bool button = d.kind == vset::Kind::KNOWN_BLOCKED;
    const bool haveState = button ? vset::buttonState(c, nowMs, d.id, state) : vset::stateOf(c.f260, d.id, state);
    char st[8];
    if (haveState) snprintf(st, sizeof(st), "%u", state);
    else snprintf(st, sizeof(st), "-");
    // Result of the pure logic for the current state (frame 15B without a field change), with the full mask:
    // shows refusals other than the mask (function missing per 361, stale 260, group D / E). The frame is not
    // sent anywhere: this is the only call of build15B outside the tests.
    uint8_t frame[8];
    vset::Refuse r = button ? vset::buildButton(c, nowMs, d.id, haveState ? state : d.minValue, 0xFFFFFFFFUL, frame)
                            : vset::build15B(c.f260, c.f361, nowMs, d.id, haveState ? state : d.minValue, 0xFFFFFFFFUL, frame);
    const char* mask = "-";
    if (d.kind == vset::Kind::SENDABLE || button) mask = (changer_.enabledMask() & (1UL << d.id)) ? "enabled" : "disabled";
    snprintf(buf, n, "[vset] %02X %-26s state=%s basis=%s mask=%s logic=%s\n", d.id, d.name, st, d.note, mask,
             vset::refuseName(r));
}

void Decoder::injectKey(uint8_t code, uint8_t state) {
    RadioKeyEvent e;
    e.code = code;
    e.state = state;
    radio_.onKey(e);
}

void Decoder::toggleDoorSwap(uint32_t nowMs) {
    doorSwap_ = !doorSwap_;
    const RadioProtocol& rp = *cfg_.radio;
    if (rp.encodeCarState && rp.encodeCarState(doorState_, settings_, doorSwap_, carStateFrame_)) {
        haveCarStateFrame_ = true;
        radio_.onState(carStateFrame_, nowMs);
    }
}
