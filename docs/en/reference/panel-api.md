# Panel API (HTTP, API version 1)

This page is the contract between three parts of the project:

- the request handlers in the firmware (`firmware/src/net/http_server.cpp`, `api_handlers.cpp`,
  `ota.cpp`),
- the panel page (`firmware/web/`),
- the device mock in Python (`firmware/tools/mock_device.py`).

A change of behaviour starts with a change here. A change that breaks older clients raises
`FW_API_VERSION` in `firmware/src/version.h` (the `api` field of `/api/hello`). See
[Versioning](versioning.md).

The API exists only in the programs built with network: environments `listen_wifi`,
`decoder_wifi`, `decoder_tx_wifi` and `listen_generic_wifi`.

> [!NOTE]
> **Names in angle brackets**
>
> `<fw_product>`, `<ap_ssid_prefix>` and `<mdns_host>` are values from `project.json`. They change
> when the project gets its final name. Look them up there.

> [!CAUTION]
> **The API cannot transmit a frame, but one build can arm transmission**
>
> No request sends a CAN frame or writes to a module of the car. A console line with a frame
> (`0E1#...`) only goes into the internal logic of the device, as on USB. Only in the program
> built with `decoder_tx_wifi` can a client on the **device network** arm or disarm CAN
> transmission (`cantx on` / `cantx off` through `POST /api/cmd`). Which frames may then be sent
> is decided by the transmit gate, not by the network code. See
> [Transmitting on the CAN bus](../safety/transmit.md).

## 1. General rules

### 1.1 Addresses

| From | Address |
|---|---|
| the device network `<ap_ssid_prefix>XXXX` | `http://10.50.8.1/` |
| mDNS name (device network and home network) | `http://<mdns_host>.local/` |
| home network | the address from the router's DHCP; shown by the USB command `wifi` and in the field `wifi.staIp` |

`XXXX` is four hex digits from the end of the chip MAC address.

Port 80, HTTP/1.1, **no encryption**. The server handles requests **one at a time** (one task).
It accepts up to 7 connections; when all are taken it closes the least recently used one. A
firmware upload (`POST /api/ota`) blocks all other requests until it ends.

The request line and the headers must arrive within 3 s of the first byte (a total deadline,
`HTTP_HDR_DEADLINE_MS`). A slower client is disconnected without an answer. So one client that
sends its headers one byte at a time holds the only server task for about 6 s at most (the
deadline plus one 3 s receive), not forever.

### 1.2 Login

- `GET /`, `GET /i18n/<lang>.json` and `GET /api/hello` are public. **Every other `/api/...` request needs the header**
  `Authorization: Basic base64("admin:" + device password)`.
- The device password is the same password that protects the device Wi-Fi network. Only the USB
  command `wifi showpass` shows it. No request returns it.
- The server **never sends a `WWW-Authenticate` header** and never sends `Access-Control-*`
  headers. So the browser does not show its own login window and does not remember the password.
  The panel has its own form, keeps the password in `sessionStorage` (under the
  `web_session_key` value from `project.json`; in `localStorage` under the same key when
  **Remember on this device** is ticked) and adds the header itself in `fetch`.
  From the command line: `curl -u admin:PASSWORD http://10.50.8.1/api/status`.
- A device without a password (a moment before the first network start, or after a memory error)
  answers every request that needs a login with `503 {"error":"not_ready"}`. This is true for
  **all** such addresses, also where the error lists below do not mention it. An empty password is
  never valid.
- No header: `401 {"error":"auth"}` (not counted as a failed attempt).
  Wrong password or wrong user name: `401 {"error":"auth"}`, and the failure counter grows (only
  for clients outside the device network, see below). Repeating **the same** wrong password as in
  the previous failed attempt does not grow the counter (an old browser tab with an old password
  does not lock the panel).
- Lockout: 5 failures within 60 s lock the login for 30 s; every next lockout doubles the time, up
  to 5 min. A correct login does not clear the failures of the current 60 s window. During a
  lockout **every** request that needs a login, also with the correct password, gets
  `429 {"error":"locked","retryInS":N}` (N = seconds left, rounded up).
  There is one lockout for all clients **from the home network**. Clients of the **device
  network** (address from `10.50.8.0/24` **and** connection to the device address `10.50.8.1`) are
  never locked out: they already know the password, because it is the key of that network. Both
  conditions are checked together: the client address alone can be faked from the home network.
  A phone on the device network that connects to the device's home network address counts as a
  home network client.
- Changes from the home network: the requests that change state (`POST /api/cmd`,
  `/api/keymap`, `/api/lang`, `/api/settings`, `/api/wifi`, `/api/password`, `/api/restart`, `/api/ota`, `/api/ota/rollback`) from
  an address outside the device network get `403 {"error":"ap_only"}` until someone types
  `wifi lan on` on the USB console (default: off). Reading works from both networks.
- **Risk of the home network:** HTTP is not encrypted, and the header with the password goes with
  every request. On the home network any device that listens to the traffic can learn the device
  password. That password is also the key of the device Wi-Fi network and the only protection of
  firmware updates. Save a home network only when you trust every device on it. The panel shows
  this warning next to the home network form.
- **Packet forwarding between the networks (known, unchanged):** the framework libraries are
  built with `CONFIG_LWIP_IP_FORWARD` on (a constant of the compiled lwIP, the program cannot turn
  it off). A device on the home network that sends a packet to the device MAC address with a
  destination `10.50.8.x` can reach a phone connected to the device network (no NAT: `NAPT` is not
  enabled). The panel does not rely on this; it matters only with a saved home network.
- The panel stops polling after `401` and shows its login form; after `429` it waits `retryInS`
  seconds; after `403 ap_only` it says "changes only from the device network" and keeps polling.

See also [Security model](../safety/security.md).

### 1.3 Format

- `/api/...` answers are JSON in UTF-8: `Content-Type: application/json`,
  `Cache-Control: no-store`, `X-Content-Type-Options: nosniff`. Exception: `/api/trace.log`
  (text).
- The order of fields in an object is not guaranteed. The order of elements in an array is part
  of the contract.
- Integers have no fractional part. Counters are unsigned 32-bit (they wrap after 4294967295).
- A field without a value is `null` (it is not left out).
- Hex codes in strings are upper case, without `0x`.
- The body of a `POST` request (except `/api/ota`) is `application/x-www-form-urlencoded`: fields
  `name=value` joined with `&`, values encoded as `%XX` (space as `+` or `%20`). The
  `Content-Type` header is not checked.
- An error from a handler: the right HTTP status and the body `{"error":"<word>"}`, sometimes with
  an extra field `"detail":"<text>"`. The words are in [1.6](#16-error-words).
- Errors from the server itself (unknown address `404`, wrong method `405`, address too long
  `414`, headers too long `431`) have a text body, not JSON. A client should look at the status.

### 1.4 Limits

| What | Limit | When exceeded |
|---|---|---|
| address with parameters | about 496 bytes (measured on the bench: 496 passes, 497 gives `414`; request line limit of esp_http_server, `CONFIG_HTTPD_MAX_URI_LEN` = 512) | `414` (server) |
| part of the address after `?` | 127 bytes | the whole query is ignored (as if there were no parameters) |
| request headers | 2048 bytes | `431` (server) |
| time for the request line and headers | 3 s from the first byte (in total, not per byte) | disconnect, no answer |
| form body | 1024 bytes, received within 1 s | `413 {"error":"too_large"}`; when the rest cannot be thrown away within 1 s (or is longer than 64 KB), the connection is closed as well |
| firmware file | `slotSize` from `/api/ota/status` (3 342 336 bytes) | `413 {"error":"too_large"}` and the connection is closed |
| console line | 127 characters | `400 bad_request` |
| frames in one answer | 400 | cut |
| lines in one `/api/log` answer | 64 lines and 4096 bytes of text | the rest in the next answer (`next < head`) |
| a whole firmware upload | 180 s | write aborted, connection closed |
| JSON answer | 16 KB | data is split (cursors), never cut in the middle |

A body sent in chunks (`Transfer-Encoding: chunked`) is not supported: a request with a body must
have `Content-Length`.

### 1.5 Time, boot number, cursors

- All `...Ms` times are milliseconds since the device started (`millis()`). They wrap after
  49.7 days.
- `boot` is a random number picked at every start of the device. When a client sees a different
  value than before, the device has restarted: drop the cursors and the remembered counters.
- **Cursor.** Every console line and every CAN frame gets the next number (lines and frames are
  counted separately, from 0 after the start). The client asks "give me everything from number
  `c`" and gets:
    - `next`: the number to send in the next request (the first record it has not received yet or,
      with an ID filter, has not been scanned yet);
    - `head`: the number the next new record will get. `next < head` means more records are waiting
      (the answer was full), so ask again at once;
    - `dropped`: how many records from number `c` had already fallen out of the device memory before
      the client asked.
- Without `c`: from the oldest record still in memory, with `dropped` = 0.
- A `c` "from the future" is treated as `head` (nothing to return, `next` = `head`). The numbers
  wrap after 2³², so they are compared "on a circle": `head + 1 .. head + 2³¹` is the future, and a
  value below the oldest record (also, for example, `4294967295` with a small `head`, that is "just
  before 0") is the past: the answer starts at the oldest record with `dropped` > 0.
- To start "from now", take `log.head` / `trace.head` from `/api/status`.
- The device keeps no state per client: several browsers at once and reconnections are fine. The
  memory holds about 200 recent lines (8 KB) and 2048 recent frames (about 9 s of the reference
  car bus).

### 1.6 Error words

| HTTP | `error` | Meaning |
|---|---|---|
| 400 | `bad_request` | a required field is missing, wrong form, characters not allowed, too long; `detail` = name of the field. `detail` = `usb_only` for `wifi ...` commands and for `cantx reset` and `set NN VV` sent through `/api/cmd` |
| 400 | `weak_password` | the new password breaks the rules (10..63 printable ASCII characters, no spaces) |
| 400 | `bad_image` | the file is not a program for this chip or is damaged; `detail`: `short`, `header`, `verify` |
| 400 | `wrong_product` | the file has no marker of this device, or it is another product |
| 401 | `auth` | missing or wrong login |
| 403 | `wrong_password` | the field `old` does not match the current password |
| 403 | `ap_only` | a change sent from the home network without the consent `wifi lan on` (1.2). Also, from a client outside the device network **whatever `wifi lan` says**: the CAN transmit commands (`cantx on`, `cantx off`, `vset on NN`, `vset off NN`) in `/api/cmd`, and an upload or rollback that changes the operating mode or `canTx` |
| 409 | `car_busy` | reverse gear engaged or parking sensors active: try again later |
| 409 | `ota_pending` | the running program waits for its confirmation after an update (up to about 20 s after the start) |
| 409 | `restart_pending` | a restart is already scheduled, or another operation on the second program slot is running (rollback, upload) |
| 409 | `no_net` | the image has no network panel; repeat with `force=1` if you mean it |
| 409 | `mode_change` | the image has another operating mode than the running one, or another CAN transmit ability (`canTx`); repeat with `mode=1` if you mean it |
| 409 | `no_rollback` | there is no program to go back to |
| 411 | `length_required` | a request with a body but without `Content-Length`, or with an empty body |
| 413 | `too_large` | the body is longer than the limit |
| 429 | `locked` | lockout after wrong passwords; extra field `retryInS` |
| 500 | `flash` | flash write error; `detail` = ESP-IDF error name |
| 500 | `overflow` | the answer did not fit in the buffer (a firmware bug: please report it) |
| 503 | `busy` | the command queue is full or the data is busy for a moment; try again shortly |
| 503 | `not_ready` | the device has just started and has no state snapshot yet (the first 0.2 s), or it has no device password yet |

## 2. Public requests

### GET /

The panel page: one HTML file with CSS and JS inside, packed with gzip. It holds no secrets.

Answer headers: `Content-Type: text/html; charset=utf-8`, `Content-Encoding: gzip`,
`Cache-Control: no-cache`, `ETag: "<16 hex digits>"`, `X-Frame-Options: DENY`,
`X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer`,
`Content-Security-Policy: default-src 'none'; script-src 'sha256-<hash>'; style-src 'unsafe-inline'; connect-src 'self'; img-src data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'`.

The hash is the base64 SHA-256 of the content of the only `<script>` tag of the page;
`firmware/tools/embed_web.py` computes it (constant `WEB_INDEX_CSP`). So the page has exactly one
inline script, no `onclick=` in the HTML and no `eval`.
With `If-None-Match` equal to the current `ETag`: `304` without a body.
The body is always packed, also when the client sent no `Accept-Encoding` (with `curl` use
`--compressed`).

**Panel languages.** The page contains only the English dictionary `firmware/web/i18n/en.json`,
the list of languages of the image (code and name) and the car profile data
`firmware/web/profiles/psa_aee2010.json`: `embed_web.py` puts them into the only script (constants
`I18N`, `LANGS` and `PROFILE`) before it computes the CSP hash. Every other dictionary is a separate
resource, `GET /i18n/<lang>.json`, fetched only when that language is chosen; until it arrives, and
when the request fails, the panel shows English. The default language is English. The choice (login screen
and System tab) is stored **in the device** (`POST /api/lang`, field `lang` of `/api/hello`), so
every browser opens in it, the login screen too; the browser keeps only a cache in `localStorage`
under `ui_lang`, and the device value wins. The device translates nothing: the API, the error
words, the console lines and the JSON keys are the same for every language. Dictionary rules: `firmware/web/i18n/README.md`; check:
`firmware/tools/check_i18n.py`; sizes: `python -I tools/embed_web.py --report` (run in
`firmware/`; budget 56 KB gzip for the page and 16 KB gzip for each separate dictionary, a build over
a budget fails).

**`GET /i18n/<lang>.json`** (no login): one panel dictionary, for every language of
`WEB_UI_LANGS` except `en` (which is in the page); another address answers `404`. It holds only the
texts of `firmware/web/i18n/<lang>.json` (no device data) and never changes until the next
firmware. Answer headers: `Content-Type: application/json; charset=utf-8`, `Content-Encoding: gzip`
(always, as for `GET /`), `Cache-Control: no-cache`, `ETag: "<16 hex digits>"`,
`X-Content-Type-Options: nosniff`; with a matching `If-None-Match`: `304` without a body. The
dictionaries are `static const` arrays in flash (`WEB_I18N` in the generated `web_assets.h`), sent
from there without a copy in RAM.

### GET /favicon.ico

`204` without a body. The panel page carries its tab icon itself, as a `data:` address in a
`<link rel="icon">` tag (allowed by `img-src data:` of the CSP), so a browser that reads the page
does not need this address.

### GET /api/hello

No login. Used to recognise the device, to show a lockout and to notice a restart after an
update. On purpose it does not give the version, hash or mode (those are in `/api/status`, after
login).

```json
{"product":"<fw_product>","api":1,"boot":2893471123,"locked":0,"lang":"pl"}
```

| Field | Type | Meaning |
|---|---|---|
| `product` | string | always the `fw_product` value of `project.json` |
| `api` | number | version of this description |
| `boot` | number | boot number (1.5) |
| `locked` | number | seconds left of the login lockout for home network clients; 0 = none. A client of the device network always gets 0 (the lockout does not apply to it, 1.2). The panel treats this field as information only; only a real `429` stops a login |
| `lang` | string or null | panel language stored in the device (`POST /api/lang`); `null` = nothing stored or a code this image has no dictionary for: English. Public on purpose, so the login screen opens in it |

## 3. State

### GET /api/status

One object with everything the dashboard shows. The car data comes from a state snapshot that
the main loop takes every 200 ms; polling more often than every 250 ms gives no new data.
Errors: `401`, `429`, `503 not_ready`, `503 busy`.

```json
{
 "sys": {"version":"2.0.0-beta.1","build":"Nov 18 2026 01:33:28","elf":"0F3A6C21D49B7E58","mode":"listen","canTx":false,
         "boot":2893471123,"uptimeMs":734512,"resetReason":"deepsleep",
         "heapFree":182344,"heapMin":151208,"heapLargest":110580,
         "loopBusyPct":7,"cpu0Pct":12,"loopMaxUs":1840,"loopsPerSec":992,
         "stackLoopFree":5120,"stackHttpFree":4368,"stackWifiFree":2210,"tempC":41.5,
         "otaState":"none","otaBusy":false,"restartPending":false,"hubDrops":0,"clientAp":true},
 "can": {"ok":true,"rx":160131,"missed":0,"overrun":0,"busErrors":0,"framesPerSec":218,"busAlive":true,"listenOnly":true,
         "tx":{"built":false,"armed":false,"armNext":false,"state":"none","fault":null,"sent":0,"acked":0,"failed":0,"refused":0,"tec":0,"rec":0}},
 "car": {"ignition":true,"ignitionRaw":142,"acc":true,"reverse":false,"lights":true,"doors":64,
         "batteryV":13.21,"outsideTempC":6.5,
         "settings":{"raw260":"A41C8FBBC55000","ageMs":212,"state38":"0290A9B840",
                     "last":{"id":1,"value":1,"result":"refused","tookMs":0,"ageMs":5300,"frame":null},
                     "accepted":0,"notAccepted":0,"refused":1,"mask":2}},
 "park": {"fresh":true,"rearStatus":5,"frontStatus":4,"sound":true,"soundFront":true,"interval":24,
          "rear":[7,7,7],"front":[5,3,7],"beeping":true,"continuous":false},
 "radio": {"ok":true,"txEnabled":false,"heard":true,"ageMs":310,"on":true,"volume":12,"muted":false,
           "source":[1,0,0,0,0,0,0,0],"time":"2026-11-16 21:14",
           "rxFrames":812,"rxAcks":0,"rxBadChecksum":0,"rxBadLength":0,"rxTimeouts":0,"rxStray":3,
           "txFrames":0,"txAcks":0,"txSuppressed":455,"txQueue":0,"txQueueCap":24,"txDropped":0,
           "keysDropped":0,"txGated":37,"slotsFull":0,
           "settingCount":0,"lastSetting":[0,0],"airCount":0,"lastAir":[0,0],"unknownCount":0},
 "audio": {"ok":true,"volume":40,"volumeMax":80},
 "wifi": {"on":true,"phase":"running","apSsid":"<ap_ssid_prefix>A1B2","apIp":"10.50.8.1","apChannel":1,
          "apStations":1,"staConfigured":true,"staSsid":"home","staConnected":true,"staIp":"192.168.1.37",
          "staRssi":-61,"staLastReason":0,"hostname":"<mdns_host>","maxStepMs":180,"startFails":0,
          "gaveUp":false,"blocked":false,"lanAdmin":false},
 "log": {"head":532,"oldest":341},
 "trace": {"head":160131,"oldest":158083}
}
```

**`sys`**

| Field | Type | Meaning |
|---|---|---|
| `version` | string | `FW_VERSION`, form `x.y.z` |
| `build` | string | compile time of the version file, `Mmm dd yyyy hh:mm:ss`. `firmware/tools/build_time.py` recompiles that file at every change in `src/`, `lib/` or `web/`, so this is the time of the last build with changed sources. To tell two images apart use `elf` |
| `elf` | string | 16 upper case hex digits: the first 8 bytes of the SHA-256 of the ELF file of the running program; different for every built image. Compare without regard to case |
| `mode` | string | `listen` = the device beside the CAN box (sends nothing to the radio); `decoder` = instead of the box |
| `canTx` | boolean | `true` = the running program can transmit on the car CAN (byte `canTx` of the marker is not 0). Programs older than 0.7.0 do not send this field: treat a missing field as `false` |
| `boot` | number | boot number (1.5) |
| `uptimeMs` | number | time since the start, at the moment of the answer |
| `resetReason` | string | cause of the last start: `poweron`, `ext`, `sw` (software restart, also after an update), `panic`, `int_wdt`, `task_wdt`, `wdt`, `deepsleep` (wake-up from sleep: the normal case), `brownout`, `sdio`, `usb`, `jtag`, `efuse`, `pwr_glitch`, `cpu_lockup`, `unknown` |
| `heapFree`, `heapMin`, `heapLargest` | number, bytes | free internal memory now, lowest since start, largest free block |
| `loopBusyPct` | number 0..100 | load of core 1 (main loop) in the last full second |
| `cpu0Pct` | number 0..100 | load of core 0 (Wi-Fi, sound, web server) |
| `loopMaxUs` | number, µs | longest main loop pass in the last full second, including the 1 ms pause; at rest about 1000..2000 |
| `loopsPerSec` | number | main loop passes in the last full second (at rest about 1000) |
| `stackLoopFree`, `stackHttpFree`, `stackWifiFree` | number, bytes | smallest stack headroom of the main loop, the web task and the Wi-Fi task since start |
| `tempC` | number with 1 decimal or `null` | ESP32 chip temperature; `null` when the reading failed (the sensor covers 10..50 °C) |
| `otaState` | string | state of the running program: `none` (flashed by cable), `valid` (flashed over Wi-Fi and confirmed), `pending` (waits for confirmation) |
| `otaBusy` | boolean | a firmware upload is running. The server handles one request at a time, so an HTTP client in practice always sees `false`; the field is there for completeness |
| `restartPending` | boolean | a restart is scheduled (after 1.5 s; while the car needs the device or the radio has ACC, only after that state ends) |
| `hubDrops` | number | preview records (lines, frames, snapshots) lost inside the device since start; 0 in a healthy system |
| `clientAp` | boolean | the client that sent THIS request is connected to the device network (by the addresses of both ends of the connection, not by headers: the same rule as `ap_only`). Only such a client can arm or disarm CAN transmission and change the item mask from the panel. Older programs do not send it: treat as `false` |

**`can`** (car bus)

| Field | Type | Meaning |
|---|---|---|
| `ok` | boolean | the CAN driver started |
| `rx` | number | frames received since start |
| `missed` | number | frames lost because the driver queue was full (the main loop did not keep up) |
| `overrun` | number | frames lost in the CAN controller itself (the interrupt could not run, for example during a flash write in an update) |
| `busErrors` | number | bus errors seen by the controller |
| `framesPerSec` | number | frames accepted in the last full second (reference car: about 218, measured) |
| `busAlive` | boolean | a frame within the last second |
| `listenOnly` | boolean | the controller mode the program picked at start (a program setting, not a measurement): `true` = listen only. `false` only in the program with transmission (`sys.canTx` = `true`) after an armed start: controller in NORMAL mode, transceiver in silent mode except during the window of one frame. A hardware guarantee would need an open CAN transmitter jumper; on the author's prototype the jumper JP1 is closed and SLNT is wired to GPIO6, so only the program decides |
| `tx` | object | CAN transmit gate (`firmware/lib/core/can_tx_gate.h`); table below. Programs older than 0.9.0 do not send it |

**`can.tx`.** In a program without transmit hardware (`sys.canTx` = `false`): `built` = `false`,
`state` = `"none"`, counters 0. In the program built with `decoder_tx_wifi`: `built` = `true`.
The panel then shows the arm / disarm buttons on the CAN bus card; they work only when
`sys.clientAp` = `true`, send `cantx on` / `cantx off` through `/api/cmd`, and arming asks first
with a warning.

| Field | Type | Meaning |
|---|---|---|
| `built` | boolean | the program has transmit hardware (`false` in all environments except `decoder_tx_wifi`) |
| `armed` | boolean | transmission is armed in this start (gate in a state other than `none` and `disarmed`) |
| `armNext` | boolean | a valid arming is stored, so the next start will be armed. After `cantx on`: `armNext` = `true` with `armed` = `false` until a restart; after `cantx off` both are `false` at once |
| `state` | string | `none` (program without transmission), `disarmed`, `ready`, `opening`, `sending`, `cooldown`, `fault` (latch) |
| `fault` | string or `null` | reason of the latch: `3 failed sends`, `transmit error counter`, `bus-off`, `transmitter window exceeded`, `controller event`, `foreign bits of 260 changed`, `external lock`; `null` = none |
| `sent`, `acked`, `failed`, `refused` | number | frames given to the controller, acknowledged, failed, requests refused by the gate; 0 without transmit hardware |
| `tec`, `rec` | number | controller error counters from the last reading by the gate (at a request and at the end of the pause after a frame, not live); 0 without transmit hardware |

**`car`**

| Field | Type | Meaning |
|---|---|---|
| `ignition` | boolean | ignition on (frame `0F6`, only with fresh frames) |
| `ignitionRaw` | number 0..255 or `null` | byte 1 of frame `0F6`. Reference car: 128 (`80`) off, 136 (`88`) on, 142 (`8E`) engine running (measured: recordings in `firmware/test_data/captures/`). `null` when frame `0F6` is older than 1 s (also before the first one) |
| `acc` | boolean | the radio gets ACC (in `listen` mode: would get; the output is off then) |
| `reverse` | boolean | reverse gear (with ignition on and a fresh frame) |
| `lights` | boolean | lights on (frame `036`, with ignition on) |
| `doors` | number (bits) or `null` | `null` until a door frame has come since start (`128` with the warning lamp off, or `1A1`). Open doors: 64 (`40`) driver, 128 (`80`) passenger, 16 (`10`) rear left, 32 (`20`) rear right, 8 (`08`) boot (measured: door test in the reference car). Known limit: when one of several open doors closes, the display may stay until all are closed |
| `batteryV` | number with 2 decimals or `null` | supply voltage measured on the board BEHIND the input diode, about 0.8 V (0.7-0.9 V) below the battery (the drop is not measured exactly); `null` when the board has no divider or no 12 V is present (below 6 V; on USB power the divider gives 0..1.5 V of noise). The panel then shows "no 12 V (powered from USB)", and `stat` prints `supply=no 12V (x.xxV)` |
| `outsideTempC` | number with 1 decimal or `null` | outside temperature from frame `0F6`, step 0.5 °C; `null` without a fresh frame |
| `settings` | object | vehicle settings (personalisation), read only; table below. Programs older than 0.8.0 do not send it |

**`car.settings`** (read only. A setting changes only through the radio command `80` in an armed
program with transmission.)

| Field | Type | Meaning |
|---|---|---|
| `raw260` | string or `null` | data of the last CAN frame `260` (personalisation state in the BSI) as upper case hex without spaces; a normal frame has 7 bytes (14 characters), a shorter or longer one is given as it came (at most 8 bytes). `null` until a `260` frame has come since start. This is the last received data, also when old: see `ageMs` |
| `ageMs` | number or `null` | age of that frame at the moment of the answer (`260` comes about every 500 ms with ignition on); `null` together with `raw260` |
| `state38` | string, 10 characters | DATA[1..5] of frame `38` that the device sends to the radio (state of the items of the "Function settings" screen), computed from frames `260`, `227`, `3E1`, `217` (`firmware/lib/car_psa_aee2010/psa_signals.cpp`, `firmware/lib/radio_xp/xp_signals.cpp`, `vehicles/peugeot-508-sw-2015/settings-map.md`). `0000000000` until the first 7-byte `260` frame. Also given in `listen` mode (then nothing goes to the radio) |
| `last` | object or `null` | the last radio command `80 nn vv` (setting change on the radio screen); `null` until one has come since start |
| `last.id`, `last.value` | number 0..255 | item number and requested value |
| `last.result` | string | `refused` (the item has a field in frame `15B`, but the program does not transmit or is disarmed), `no_frame` (no known frame for this item, or a number outside the table), `blocked` (frame known, but not transmitted by this build), `unchanged`, `accepted`, `not_accepted`, `tx_failed`, `side_effect`. Show an unknown word as it is |
| `last.tookMs` | number | time from the command to the result (for `accepted`: time to the `260` frame with the new state); 0 for a refusal |
| `last.ageMs` | number | how long ago the command came |
| `last.frame` | string or `null` | the 8 bytes of the `15B` frame given to the gate (16 hex digits) for `accepted`, `not_accepted`, `tx_failed`, `side_effect`; `null` for a refusal and `unchanged`. Always `null` in a program without transmission |
| `accepted`, `notAccepted` | number | changes accepted / not accepted by the car since start; always 0 in a program without transmission |
| `refused` | number | `80` commands that ended in a refusal since start; in a program without transmission = the number of all `80` commands |
| `mask` | number | mask of enabled items, bit number = item number (default 2 = only item `01`). Change: `vset on NN` / `vset off NN`. Older programs do not send it |

"Accepted" means the BSI changed frame `260` as requested. Whether the change is visible in the car
(wiper, lights, sound) was not checked. See [Transmitting on the CAN bus](../safety/transmit.md).

**`park`** (frame `0E1`)

| Field | Type | Meaning |
|---|---|---|
| `fresh` | boolean | frame `0E1` came within the last 400 ms; with `false` the other fields hold the last known values |
| `rearStatus`, `frontStatus` | number | rear / front state: 0 no frame, 1 fault, 2 switched off by the driver, 4 active, 5 standby, 6 not working |
| `sound` | boolean | the car asks for sound |
| `soundFront` | boolean | `true` obstacle in front, `false` behind |
| `interval` | number 0..63 | beep interval field from the car: 0 continuous tone, 63 none, 57..11 closer and closer |
| `rear`, `front` | array of 3 numbers | zones `[left, centre, right]`, 0 = very close, 7 = far / no obstacle |
| `beeping` | boolean | the device is beeping (also in the gap between beeps and during the `test` beeping) |
| `continuous` | boolean | the device plays a continuous tone |

The meaning of the `0E1` fields on the reference car is on the [Peugeot 508 SW](vehicles/peugeot-508.md),
with its status.

**`radio`** (UART to the radio, XP protocol)

| Field | Type | Meaning |
|---|---|---|
| `ok` | boolean | the UART port started |
| `txEnabled` | boolean | sending to the radio is on (decoder mode only) |
| `heard` | boolean | the radio has spoken at least once since start |
| `ageMs` | number or `null` | time since the last frame from the radio; `null` with `heard` = `false` |
| `on` | boolean | the radio reported "on". With `heard` = `false` the fields `on`, `volume`, `muted`, `source` hold defaults (`false`, 0, zeros), not a reading: the panel shows "—" |
| `volume` | number 0..36 | radio volume |
| `muted` | boolean | muted |
| `source` | array of 8 numbers 0..255 | raw data of the sound source frame (`C0`) |
| `time` | string `YYYY-MM-DD hh:mm` or `null` | time given by the radio |
| `rxFrames`, `rxAcks`, `rxBadChecksum`, `rxBadLength`, `rxTimeouts`, `rxStray` | number | reception: good frames, acknowledgements, bad checksum, bad length, truncated frames, bytes outside frames |
| `txFrames`, `txAcks`, `txSuppressed` | number | frames sent, acknowledgements sent, frames prepared but not sent (`listen` mode) |
| `txQueue`, `txQueueCap` | number | frames waiting to be sent, and the queue size |
| `txDropped`, `keysDropped` | number | frames and button events lost because a queue was full |
| `txGated` | number | frames the device did not send because the radio had no ACC (buttons and sensors with the radio off, the answer to a version request); grows in normal use |
| `slotsFull` | number | state frames skipped because there was no slot for a new kind of frame (12 kinds); should be 0 |
| `settingCount`, `airCount`, `unknownCount` | number | vehicle setting commands, climate commands and unknown frames from the radio |
| `lastSetting`, `lastAir` | array `[number, value]` | the last such command |

**`audio`**: `ok` (boolean, the sound driver started), `volume` (number, percent), `volumeMax`
(number, upper volume limit that protects the speaker, now 80).

**`wifi`**

| Field | Type | Meaning |
|---|---|---|
| `on` | boolean | `phase` = `running` |
| `phase` | string | `off`, `starting`, `running`, `stopping`, `failed` |
| `apSsid` | string | name of the device network |
| `apIp` | string | address of the device in its own network |
| `apChannel` | number | channel (0 = unknown); with a home network connection this is the router channel |
| `apStations` | number | devices connected to the device network |
| `staConfigured` | boolean | a home network is saved |
| `staSsid` | string | its name; empty when none |
| `staConnected` | boolean | connected to the home network and has an address |
| `staIp` | string or `null` | address on the home network |
| `staRssi` | number or `null` | router signal strength in dBm |
| `staLastReason` | number 0..255 | code of the last disconnection from the home network (0 = none). The panel shows 201 as "network not found", 2, 15, 202, 204 as "password rejected", others as a number |
| `hostname` | string | the `mdns_host` value (address `<mdns_host>.local`) |
| `maxStepMs` | number | longest single step of Wi-Fi handling since start (a separate task does it; the main loop does not wait for it) |
| `startFails` | number | failed Wi-Fi starts in a row |
| `gaveUp` | boolean | after 3 failed starts the device stopped trying (until `wifi on` on USB or a restart) |
| `blocked` | boolean | network held after crash restarts (until `wifi on` on USB or a power cycle) |
| `lanAdmin` | boolean | changes allowed also from the home network (`wifi lan on`) |

**`log`**, **`trace`**: `head` = number of the next line / frame, `oldest` = number of the oldest
one still in memory (1.5).

## 4. Live data

### GET /api/log

Console lines (the same as on USB, without the raw frame dump and without the password line).

Parameter: `c` (optional) cursor, decimal digits only. Errors: `400 bad_request` (`detail`: `c`),
`401`, `429`, `503 not_ready`, `503 busy`.

```json
{"boot":2893471123,"next":534,"head":534,"dropped":0,
 "lines":[[734100,"[car] reverse=1"],[734312,"[park] rear=4 front=5 sound=1 rear interval=0x18 zones rear 357 front 777"]]}
```

`lines`: an array of pairs `[time ms, text]`, oldest first. The text has no line end characters
and at most 200 characters (the device cuts longer lines). One answer holds at most 64 lines and
4096 bytes of text; when more is waiting, `next < head` and the client asks again at once.
Comment lines `# idf: ...` are error messages of the ESP-IDF libraries (Wi-Fi, server).

### GET /api/frames

CAN frames from the preview (received from the bus and typed with a console command).

| Parameter | Meaning |
|---|---|
| `c` | cursor (optional) |
| `id` | filter: 1..3 hex digits, value up to `7FF` (optional) |
| `max` | at most this many frames in the answer: 1..400; default 256; a larger value is cut to 400 |

Errors: `400 bad_request` (`detail`: `c`, `id` or `max`; also `max=0`), `401`, `429`,
`503 not_ready`, `503 busy`.

```json
{"boot":2893471123,"next":160133,"head":160133,"dropped":0,
 "frames":[[734501,"0E1","B0F000FCFC1EC200"],[734503,"21F","000000"]]}
```

`frames`: an array of triples `[time ms, "ID", "DATA"]`, oldest first. The ID is always 3 hex
digits, the data 0..16 hex digits (two per byte). With the `id` filter the answer holds only
matching frames, but `next` also moves past the scanned frames of other IDs, and `dropped`
counts all frames that fell out, whatever their ID. Frames that carry the VIN (`336`, `3B6`,
`2B6`) are given here unchanged; the panel (chapter 7) and the export mask them.

### GET /api/table

The last state of every identifier. No parameters. Errors: `401`, `429`, `503 busy`.

```json
{"nowMs":734520,"overflow":0,
 "ids":[["036",7302,100,12,"0E00800F310001A8"],["0E1",7290,100,19,"B0F000FCFC1EC200"]]}
```

`ids`: an array of five-element arrays `["ID", frames since start, gap between the last two
frames in ms, age of the last frame in ms, "DATA"]`, sorted by ID, at most 128 entries. A gap of 0
means only one frame has come so far. `nowMs` is the device time at the moment of the answer (the
age is counted from it; the time is taken after the table is copied, so an age is never negative
or wrapped).
`overflow`: frames with identifiers that did not fit in the table (the reference car sends about
70 IDs, measured).

### GET /api/trace.log

The whole frame preview as a text file in the recording format.

Parameter: `mask` (optional). `mask=0` turns masking off; any other value and no parameter mean
masking. Errors before the first byte: `401`, `429`, `503 busy`. After sending has started, an
error ends with a dropped connection.

Headers: `Content-Type: text/plain; charset=utf-8`, `Content-Disposition: attachment` with a
fixed file name `<slug>_can.log` (`slug` from `project.json`; set in `firmware/src/net/api_handlers.cpp`),
`Cache-Control: no-store`. The body is sent in chunks, lines end with `\n` only:

```text
# <fw_product> 2.0.0-beta.1 listen; time in seconds since the device started
(734.501) can0 0E1#B0F000FCFC1EC200
# masked
(734.503) can0 21F#000000
```

- First line: a comment with the product name, version and mode (`listen` or `decoder`).
- A frame: `(seconds.milliseconds) can0 ID#DATA`, seconds without leading zeros, milliseconds
  always 3 digits.
- With masking every frame with ID `336`, `3B6` or `2B6` (VIN) is replaced by the line `# masked`
  (exactly this text). The panel uses the same format for its own recording in the browser.
- The file covers frames from the oldest in memory up to the number `trace.head` at the start of
  the download. When frames fall out of memory during the download, a line `# lost N frames`
  marks the place.

## 5. Commands

Commands are not run in the server task. They are put into a queue and run by the main loop
within a few milliseconds. The answer `{"queued":true}` means "accepted for running"; the result
shows up in `/api/log`, `/api/status` or `/api/keymap`. The queue has 4 places; when full:
`503 busy`. All requests of this chapter need the consent for changes from the client's network
(1.2, `ap_only`).

**While the car needs the device** (reverse gear, active sensors): requests that write flash
(key table, Wi-Fi, password, rollback) wait in the queue until that state ends (a flash write
stops CAN reception). Console lines run at once, unless they are queued behind such a request;
from the panel in this state `test`, `vol N`, `map FROM TO` and `map reset` are refused (log:
`[www] skipped: the car needs the device`).

### POST /api/cmd

One console line, as typed on USB. The commands are described on [USB console](console.md).

Body: `line=<command>`. After decoding 1..127 characters, printable ASCII only (codes 32..126).
A line whose first word is `wifi` is refused at once and reaches neither the queue nor the log
(`400 bad_request`, `detail` `usb_only`): Wi-Fi commands contain passwords.
CAN transmit commands are checked here too: `cantx reset` and `set NN VV` give `400 bad_request`,
`detail` `usb_only`; `cantx on`, `cantx off`, `vset on NN`, `vset off NN` from a client outside
the device network give `403 ap_only` (also after `wifi lan on`). A client on the device network
gets `{"queued":true}`; the main loop checks the source again and prints the result in the log.

Answer: `{"queued":true}`. Errors: `400 bad_request` (`detail`: `line` or `usb_only`), `401`,
`403 ap_only`, `411`, `413`, `429`, `503 busy`.

The log first shows the line `# www> <command>`, then the answer of the command. A `help` sent
from the panel lists the commands without the `wifi` ones.

### GET /api/keymap

The button remapping table (button code from the car → code sent to the radio) and the last
pressed button. Errors: `401`, `429`, `503 not_ready`, `503 busy`.

```json
{"max":16,"map":[["9F","9B"],["21","1F"]],"defaults":[["9F","9B"],["21","1F"]],
 "lastKey":{"code":"9F","state":1,"ageMs":420}}
```

| Field | Meaning |
|---|---|
| `max` | largest number of remappings (16) |
| `map` | remappings in use: pairs `["FROM","TO"]`, codes as two hex digits |
| `defaults` | default remappings of the car profile built in (restored by `reset=1`); empty for `generic` |
| `lastKey` | the last button event, or `null` when there was none since start: `code` = code from the car BEFORE remapping, `state` = 1 pressed / 2 held / 0 released, `ageMs` = how many ms ago |

The data comes from the state snapshot (refreshed every 200 ms).

### POST /api/keymap

Body, one of two forms:

- `map=<table>`: the whole new table as `FROM=TO;FROM=TO`, for example `9F=9B;21=1F` (in the form:
  `map=9F%3D9B%3B21%3D1F`). Two hex digits per code, any case, a `;` at the end is allowed, no
  spaces, at most 16 pairs, each `FROM` code at most once. A pair `X=X` is skipped. An empty value
  (`map=`) means no remappings.
- `reset=1`: back to the built-in remappings (the field `map`, if present, is ignored).

Answer: `{"queued":true}`. Errors: `400 bad_request` (`detail`: `map`; also when neither field is
present), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

The table is stored in flash in the form the device normalises (upper case, without `X=X` pairs).
`GET /api/keymap` shows the new state after about 300 ms (later while the car needs the device).
When the write fails, the table works until a restart and the log says
`[key] table write failed`. A remapping for code `A4` (moon) has no effect: that button works as
a screen-off switch.

### POST /api/lang

The panel language stored in the device (a later addition to API version 1: a new field and new
addresses only, nothing changed for older clients). The panel sends it when the language is changed after login, and
once after login when a language was picked on the login screen.

Body: `lang=<code>`: one of the dictionaries built into this image (`firmware/web/i18n/<code>.json`,
list `WEB_UI_LANGS` written by `embed_web.py`). An empty value (`lang=`) forgets the choice
(English).

Answer: `{"queued":true}`. Errors: `400 bad_request` (`detail` `lang`: missing field or a code that
is not built in), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

The code is stored in flash (NVS key `ui_lang`) when the request leaves the queue (later while the
car needs the device). An unchanged code writes nothing. `GET /api/hello` shows it at once after
that. USB console: `lang`, `lang <code>`, `lang default`. See [Settings](settings.md).

### GET /api/settings

Backup of the user settings, without passwords. Errors: `401`, `429`, `503 not_ready`.

```json
{"v":1,"lang":"pl","vol":40,"keys":"9F=9B;21=1F","net":true,"lan":false}
```

| Field | Meaning |
|---|---|
| `v` | format version, 1 |
| `lang` | stored panel language, `null` = nothing stored (English) |
| `vol` | stored beep volume in percent, `null` = nothing stored (the build default) |
| `keys` | stored key table (`POST /api/keymap` format), `null` = never stored (the car profile defaults), `""` = stored without remappings |
| `net`, `lan` | `wifi on/off` and `wifi lan on/off`, for information |

Not in the backup on purpose: the device password, the home network, CAN transmit arming and the
vehicle settings mask. Format and parser: `firmware/lib/core/settings_backup.h`. USB console:
`settings`.

### POST /api/settings

Import of a backup. Body: `json=<object>` (at most 255 characters after decoding). Any subset of
the fields of `GET /api/settings`: a missing field is not changed, `null` returns it to the
default. `net` and `lan` are ignored here (they can only be changed on USB).

Everything is checked before the request is queued, so a bad import changes nothing: `400
bad_request` with `detail` = `json` (missing or too long field), `syntax`, `version`,
`unknown_key`, `duplicate`, `bad_value` (parser), `lang` (not built in), `vol` (below 15, the
lowest volume the device stores) or `keys` (bad table). Other errors: `401`, `403 ap_only`, `411`,
`413`, `429`, `503 busy`.

Answer: `{"queued":true}`. The main loop writes the fields one by one (later while the car needs
the device) and logs `[settings] import: lang=ok vol=ok keys=ok` (`-` = not given, `FAILED` = flash
write error). USB console: `settings import <JSON>`.

### POST /api/wifi

The home network the device should join (its own network always runs).

Body: `ssid=<name>&pass=<password>`.

- `ssid`: 1..32 bytes, no control characters. **An empty value (`ssid=`) = forget the home
  network.** A missing `ssid` field is an error.
- `pass`: required, 8..63 printable ASCII characters or 64 hex digits (a ready WPA2 key; 64
  characters of another kind get `400 bad_request`, `detail` `pass`). An open network cannot be
  saved (the device would join any access point with that name). With `ssid=` the `pass` field is
  ignored.

Answer: `{"queued":true,"applyInMs":1000}`. Errors: `400 bad_request` (`detail`: `ssid` or
`pass`), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

The setting is stored as soon as the request leaves the queue. `applyInMs` after the last such
request the device Wi-Fi starts again: its own network disappears for about 1..3 s (not measured)
and the client has to reconnect. After joining the router, the device network runs on the
router's channel. While the car needs the device, the write and the restart wait until that state
ends. With the engine running the device does not try to join the home network.
Result: `wifi.staConfigured`, `wifi.staConnected`, `wifi.staLastReason` in `/api/status`. The home
network password cannot be read back. The panel shows the home network risk (1.2) before saving.

### POST /api/password

Change of the device password (panel login and the key of the device Wi-Fi network).

Body: `old=<current>&new=<new>`. New: 10..63 printable ASCII characters (codes 33..126), no
spaces.

Answer: `{"queued":true,"applyInMs":1000}`. Errors: `400 bad_request` (`detail`: `old` or `new`
when a field is missing), `400 weak_password`, `403 wrong_password` (does not count towards the
lockout), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`, `503 not_ready` (the device has
no password).

The new password is valid in the panel from the moment the main loop runs the request (a few ms
after the answer; later while the car needs the device). The panel uses the new password right
after the `queued` answer, and on `401` tries once more with the old one. After `applyInMs` the
device network starts again with the new password: every phone and computer has to reconnect. A
forgotten password is shown by `wifi showpass` on USB; `wifi newpass` picks a new random one.

### POST /api/restart

Restart of the device. No body (a body, if any, is ignored).

Answer: `{"queued":true,"restartInMs":1500}`. `restartInMs` is the SHORTEST time: when the car
starts to need the device meanwhile, or the radio has ACC (ignition), the restart waits until that
state ends (until the ignition is off). Errors: `409 car_busy`, `409 ota_pending` (a restart before
confirmation would roll the new program back: use `/api/ota/rollback` for that),
`409 restart_pending`, `401`, `403 ap_only`, `429`, `503 not_ready`, `503 busy`.

Before the restart the device switches off the 12 V outputs. In `decoder` mode the radio loses
ACC for the time of the start (about 1..2 s, not measured). After the restart `boot` changes.
The device network starts again: a phone or computer may not reconnect by itself. With a silent
bus and no computer on USB the device waits 60 s from the start for a client, then switches Wi-Fi
off and goes to sleep (`firmware/lib/core/net_policy.h`); CAN traffic or a power cycle wakes it.
The panel suggests reconnecting to the device network after 20 s of waiting.

## 6. Firmware update

The file to upload: `.pio/build/<environment>/firmware.bin`. A merged image that starts at address
0 is refused.

> [!WARNING]
> **Update only while parked**
>
> A flash write stops CAN reception for a moment and can make the sound stutter. The device
> refuses an update while reverse gear is engaged or the parking sensors are active, and aborts
> the upload when such a state appears during it (detection can take 1..3 s, because some frames
> get lost during the write). During the upload and for 3 s after it the device does not switch
> off ignition, ACC, illumination or reverse because of lost frames (a frame "ignition off" still
> works normally). The panel asks `/api/ota/status` and shows this warning before it sends a
> file.

**CAN transmitter jumper.** Images are not signed. A hardware guarantee that no uploaded program
transmits on the car bus would need an open CAN transmitter jumper (JP1 on the prototype). On the
author's prototype JP1 is closed and SLNT is wired to GPIO6, so the uploaded program alone decides.
The `canTx` byte of the marker and a separate consent (`mode=1`) protect against a mistake.

**Operating mode.** An image in `decoder` mode sends frames to the radio and switches the 12 V
outputs. Upload it only when the CAN box is unplugged (two transmitters on one line and two
ACC sources short the outputs). So an image in another mode than the running one needs the
parameter `mode=1`, and the panel asks a separate question first. A program uploaded with
`mode=1` confirms itself right after its first good start (no `pending` period), so that a power
loss while you rewire does not bring back the previous mode.

**CAN transmission (`canTx`).** The marker of the file says whether the program can transmit on
the car CAN. The device treats a `canTx` difference between the file and the running program like
a mode change: `409 mode_change`, consent `mode=1`, confirmation right after the start. Only the
image of environment `decoder_tx_wifi` has `canTx` = 1. After upload it starts disarmed (the
arming is tied to the build string and does not carry over to another program). This is a
protection against a mistake, not a signature. When the mode changes, the panel reports success
only at `otaState` = `valid` and shows "do not disconnect power" until then.

### The device marker in the file

Every program of this project contains, in its `firmware.bin`, one 72-byte marker, anywhere in the
file (usually among the constants). The panel and the mock look for it before they send a file.

| Offset | Length | Content |
|---|---|---|
| +0 | 12 | byte `7F`, the 9 characters of `fwid_magic` from `project.json`, byte `7F`, byte `00` |
| +12 | 16 | product, ASCII, zero-terminated: the `fw_product` value |
| +28 | 16 | version, ASCII, zero-terminated |
| +44 | 24 | build time, ASCII, zero-terminated |
| +68 | 1 | mode: 0 `listen`, 1 `decoder` |
| +69 | 1 | network: 1 = the program has the panel and the Wi-Fi update |
| +70 | 1 | `canTx`: 1 = the program can transmit on the car CAN, 0 = listen only. In images up to 0.6.0 this byte was reserved and is 0 |
| +71 | 1 | reserved (0) |

Treat the strings as untrusted (they may lack the zero). The `elf` hash of an image is the file
bytes `0xB0..0xB7` written as 16 upper case hex digits. The layout is defined in
`firmware/lib/core/fw_ident.h`.

Panel rules before sending: no marker or another product = refused in the browser, nothing sent;
`net` = 0 = a question, then `force=1`; a mode other than `sys.mode` from `/api/status` = a
separate question, then `mode=1`. The device also requires `mode=1` when the `canTx` byte of the
file differs from the running program (both ways). The panel checks this before sending: it
compares "byte +70 of the file is not 0" with `sys.canTx` (missing field = `false`) and on a
difference asks a separate question ("Firmware with transmission on the car CAN" / "Firmware
without transmission on the car CAN"), then `mode=1`; the same for a rollback (`other.canTx`). The
panel compares booleans, the device compares raw bytes: two different non-zero values pass the
panel without a question, and the device refuses them with `mode_change`. The device checks the
same after receiving the file anyway, so a browser without this check cannot get around the
protection; it only overwrites the second slot for nothing.

### POST /api/ota

Body: the raw file (no `multipart`), `Content-Length` header required.
Address parameters: `force=1` consent to an image without the network panel (after it, updates
only by cable and no automatic rollback), `mode=1` consent to an image in another operating mode
or with another `canTx`.

During the upload the server answers no other request. The browser computes the progress from the
bytes it has sent (upload progress events); the device has no progress request.

Answer after the file is accepted:

```json
{"ok":true,"bytes":1048576,"version":"2.0.0-beta.1","build":"Nov 18 2026 10:02:11","mode":"listen","canTx":false,"net":true,
 "elf":"7B1E90C4A2D35F68","restartInMs":1500}
```

`version`, `build`, `mode`, `canTx`, `net` come from the marker of the accepted file; `elf` is the
hash of the new image (file bytes `0xB0..0xB7`; the same that `sys.elf` and `running.elf` show after
the restart). After `restartInMs` (at the earliest; while the car needs the device or the radio has
ACC, only after that state ends, that is after the ignition is off) the device restarts with the new
program.

| Status | `error` | When | File data |
|---|---|---|---|
| 401 / 403 / 429 / 503 | `auth` / `ap_only` / `locked` / `not_ready` | login | not read, connection closed (the client may see a broken connection instead of an answer) |
| 411 | `length_required` | no `Content-Length` | |
| 413 | `too_large` | file larger than `slotSize` | not read, connection closed |
| 400 | `bad_image`, `detail` `short` | file shorter than 65 536 bytes | read and thrown away |
| 409 | `ota_pending`, `restart_pending`, `car_busy` | see 1.6 | read and thrown away |
| 503 | `not_ready` | no state snapshot | read and thrown away |
| 400 | `bad_image`, `detail` `header` | the first 4096 bytes are not an ESP32-S3 program (byte 0 not `E9`, bytes 12-13 not `09 00`, bytes 32..35 not `32 54 CD AB`) | the rest read and thrown away; nothing written |
| 409 | `car_busy` | reverse or sensors during the upload | write aborted, the rest thrown away |
| 500 | `flash` | write error | write aborted, the rest thrown away |
| 400 | `bad_image`, `detail` `verify` | checksum or SHA-256 of the image does not match | |
| 400 | `wrong_product` | no marker of this device, or another product (`force` does not change this) | |
| 409 | `no_net` | image without the network panel and no `force=1` | |
| 409 | `mode_change` | another operating mode or another `canTx` than the running program and no `mode=1` | |
| 403 | `ap_only` | the same change with `mode=1`, but from a client outside the device network | nothing switched |
| 409 | `car_busy` | reverse or sensors appeared during the image check | nothing switched |

When the connection is lost, after three receive gaps of 3 s in a row, after more than 180 s for
the whole upload, or when the network stops, the device aborts the write and closes the connection
without an answer. In every error case the running program stays untouched.
Note: from the moment the write starts (after the check of the first 4096 bytes) the previous
program in the second slot is overwritten; after an error later on, rollback is not possible until
the next good upload.

**After the restart.** The new program starts in the `pending` state. When the drivers come up,
the main loop has run for 20 s and the device network and the server work, the program confirms
itself (`otaState` = `valid`). The confirmation is a flash write, so while the car needs the device
it waits until that state ends (`pending` can then last longer than 20 s). When the drivers or the
network do not come up at the start, when the network does not start within 60 s (counted without
the time with ACC; the rollback waits for the ignition to go off), or when the device hangs (loop
watchdog) or restarts before the confirmation, the previous program comes back. In the `pending`
state the device does not sleep and accepts no further file. Exception: a program uploaded with
`mode=1` confirms itself at once.

The panel, after the `ok` answer, asks `GET /api/hello` (no login) every 1 s until `boot` changes,
then `GET /api/ota/status` (with the same password), and compares `running.elf` with the value from
the upload answer: equal = the new program runs; different = a rollback happened. `state` shows
`pending`, and after about 20 s `valid`.

### GET /api/ota/status

Errors: `401`, `429`, `503 not_ready`. The first call after the device starts (and the first after
every upload) can take up to about 2 s: the device then scans the second program slot.

```json
{"state":"valid","slotSize":3342336,"confirmInMs":0,"busy":false,"carBusy":false,
 "restartPending":false,"restartReason":"none","canRollback":true,
 "running":{"slot":"app1","version":"2.0.0-beta.1","build":"Nov 18 2026 01:33:28","mode":"listen","canTx":false,
            "net":true,"elf":"0F3A6C21D49B7E58"},
 "other":{"slot":"app0","image":true,"scanned":true,"known":true,"bootable":true,
          "version":"0.9.0","build":"Nov 18 2026 00:12:03","mode":"listen","canTx":false,
          "net":true,"elf":"7C21A04B9D3E6F10"},
 "last":{"result":"none","error":"","bytes":0}}
```

| Field | Type | Meaning |
|---|---|---|
| `state` | string | state of the running program: `none`, `valid`, `pending` (as `sys.otaState`) |
| `slotSize` | number, bytes | the largest file that can be uploaded |
| `confirmInMs` | number | with `pending`: time left until the earliest confirmation; otherwise 0 |
| `busy` | boolean | an upload is running (an HTTP client in practice sees `false`) |
| `carBusy` | boolean | reverse gear or active sensors: upload, rollback and restart would be refused now |
| `restartPending` | boolean | a restart is scheduled |
| `restartReason` | string | `none`, `user`, `ota`, `rollback` |
| `canRollback` | boolean | `POST /api/ota/rollback` has something to go back to |
| `running.slot` | string | slot of the running program: `app0` or `app1` |
| `running.version`, `.build`, `.mode`, `.canTx`, `.elf` | | as `sys.version`, `sys.build`, `sys.mode`, `sys.canTx`, `sys.elf` in `/api/status` |
| `running.net` | boolean | always `true` |
| `other.slot` | string | the second slot |
| `other.image` | boolean | the second slot holds some program |
| `other.scanned` | boolean | its content has been scanned; `false` = not yet (the car was busy), the other `other` fields are unknown, ask again |
| `other.known` | boolean | the firmware marker of this project was found in the second slot (an image of the prototype firmware gives `false`) |
| `other.bootable` | boolean | the bootloader allows starting it |
| `other.version`, `.build`, `.mode`, `.net` | string / string / string / boolean, or `null` | data from the marker; `null` when the marker was not found |
| `other.canTx` | boolean or `null` | the `canTx` byte of the marker in the second slot is not 0; `null` when the marker was not found. A 0.6.0 image gives `false` |
| `other.elf` | string or `null` | as `running.elf`, from the app description in the second slot; `null` when the slot is empty, overwritten or not scanned yet. Equal to `running.elf` = the same image in both slots |
| `last.result` | string | the last upload since the device started: `none`, `ok`, `error` |
| `last.error` | string | error word of the last failed upload; empty when none |
| `last.bytes` | number | bytes of the last file received |

### POST /api/ota/rollback

Go back to the previous program. No body. Address parameters: `force=1`, `mode=1` with the same
meaning as for the upload, checked against the program in the second slot (`other`).

- `pending` state: the running (new) program is rejected and the previous one comes back. `force`
  is not needed; `mode=1` is needed when the program in the second slot has another operating mode
  or another `canTx`.
- Other states: the bootloader is told to start from the second slot. Needs `canRollback` =
  `true`.

From the moment the request is accepted until the restart (or a failure) the second slot is
taken: a parallel `POST /api/ota` or a second rollback gets `409 restart_pending`.

Answer: `{"queued":true,"restartInMs":1500}`. Errors: `409 no_rollback`, `409 no_net`,
`409 mode_change`, `409 car_busy`, `409 restart_pending`, `401`, `403 ap_only`, `429`,
`503 not_ready`, `503 busy`. If running it in the main loop fails, there is no restart, the slot is
freed, and the log shows a line `[ota] rollback failed: ...`.

## 7. Hints for a client

- Polling: `/api/status` every 500 ms; `/api/log` every 1 s (Console tab: every 500 ms);
  `/api/frames` every 500 ms and `/api/table` every 1 s only with the CAN tab open (frames also
  while recording); `/api/keymap` every 1 s only with the Radio tab open. The next request of the
  same kind only after the answer to the previous one. A browser tab in the background does not
  poll.
- Client activity (requests with the correct password) keeps the device awake after the car is
  switched off: at most 60 s from the last request and 10 min in total, and only with enough
  voltage. Requests without login (`/`, `/api/hello`) do not count.
- A change of `boot` in any answer: drop the cursors (next request without `c`) and refresh
  everything.
- A broken connection or no answer is not a login error: the panel shows "no connection" and keeps
  trying `GET /api/hello`.
- Recording frames "without a limit" is done by the browser: it collects the `/api/frames` answers
  and saves a file in the `/api/trace.log` format, with VIN masking on by default.
- The "mask VIN" switch (on by default) also applies to the SCREEN: for IDs `336`, `3B6`, `2B6`
  the table and the live preview show the ID, count and period, and `••` instead of the data. A
  screenshot of the CAN tab must not reveal the VIN.
- Put every text from the device (log lines, network names, versions, `detail`) into the page only
  with `textContent` (never `innerHTML`): someone else may have set the network name or a console
  line.
- Panel texts only from the dictionary (`t()` in `firmware/web/app.js`). Words from the API
  (`error`, `can.tx.state`, `settings.last.result`, `resetReason`...) are translated by the panel; an
  unknown word is shown as it is.
- The password in `sessionStorage`, or in `localStorage` with **Remember on this device**; at start
  the panel reads `sessionStorage` first, then `localStorage`. **Log out** removes it from both, a
  `401` removes the rejected password from both. After a password change (`POST /api/password`)
  the panel replaces it at once, in the same storage.

## 8. Where the firmware takes the `/api/status` fields

For whoever edits `firmware/src/net/api_handlers.cpp`. `s` is a copy of `StatusSnapshot` from
`diagGetStatus()` (`firmware/src/net/diag_types.h`); the server task never touches the objects of
the main loop. The snapshot is filled by `firmware/src/app_status.cpp`.

| JSON field | Source |
|---|---|
| `sys.version`, `build`, `mode`, `canTx` | `fwIdent()` from `src/version.h`; mode through `fwModeName()`; `canTx` = marker byte not 0 |
| `sys.elf` | `otaRunningElf()` (`src/net/ota.h`): the first 8 bytes of `esp_app_get_description()->app_elf_sha256` as 16 upper case hex digits, computed once. NOT `esp_app_get_elf_sha256()`, which in this build returns at most 9 characters |
| `sys.uptimeMs` | `millis()` at the moment of the answer |
| `sys.resetReason` | `s.resetReason` (`esp_reset_reason_t` 0..15) as a name: 0 `unknown`, 1 `poweron`, 2 `ext`, 3 `sw`, 4 `panic`, 5 `int_wdt`, 6 `task_wdt`, 7 `wdt`, 8 `deepsleep`, 9 `brownout`, 10 `sdio`, 11 `usb`, 12 `jtag`, 13 `efuse`, 14 `pwr_glitch`, 15 `cpu_lockup` |
| `sys.heapFree`, `heapMin`, `heapLargest` | `ESP.getFreeHeap()`, `ESP.getMinFreeHeap()`, `ESP.getMaxAllocHeap()` |
| `sys.stackLoopFree`, `stackHttpFree` | `uxTaskGetStackHighWaterMark(loopTaskHandle)` and `(NULL)`; on this port the result is in bytes |
| `sys.tempC` | `temperatureRead()`; NaN gives `null` |
| `sys.otaState`, `otaBusy`, `restartPending`, `hubDrops` | `otaRunningState()`, `otaBusy()`, `otaRestartPending()`, `diagDrops()` |
| `sys.clientAp` | `httpPeerIsAp(req)` at the moment of the request (not from the snapshot) |
| `can.listenOnly` | `s.canListenOnly` (`canListenOnly()` in `src/can_bus.cpp`) |
| `can.tx.*` | `s.canTxState` as `TxState` (`built` = not `NOT_BUILT`, `armed` = not `NOT_BUILT` and not `DISARMED`, `state` = `txStateName()`), `s.canTxFault` (`txFaultName()`, 0 gives `null`), the counters, `armNext` = `s.canTxArmNext` |
| `car.ignitionRaw`, `doors`, `batteryV`, `outsideTempC` | `null` when `s.ignitionFresh`, `s.doorsKnown`, `s.batteryV >= NET_VBAT_VALID_MIN_V`, `s.haveOutsideTemp` is false |
| `car.settings.*` | `s.set260*`, `s.set38`, `s.setLast*`, `s.setAccepted`, `s.setNotAccepted`, `s.setRefused`, `s.setMask`; `last.result` = `settingCmdResultName()` (`lib/app/decoder.h`) |
| all ages (`ageMs`) | `millis()` taken AFTER copying the snapshot, minus the stamp; a negative result = 0 |
| `radio.time` | with `s.radio.haveTime`: `20YY-MM-DD hh:mm`, otherwise `null` |
| `audio.volumeMax` | constant `AUDIO_VOLUME_MAX` |
| `wifi.blocked`, `lanAdmin`, `hostname` | `s.wifi.blockedByFaults`, `settingsLanAdmin()`, constant `NET_HOSTNAME` |
| `wifi.staIp`, `staRssi` | `null` without a connection |
| `log.*`, `trace.*` | `diagHeads()` |

`GET /api/keymap`: `map` from `s.keyMap`, `defaults` from `xp::KeyCustomizer::defaultEntry()`,
`max` = `xp::KeyCustomizer::MAP_SIZE`. `GET /api/ota/status`: `running.elf` = `otaRunningElf()`, the
other fields from `otaGetStatus()`.

## 9. Requirements for the mock (`firmware/tools/mock_device.py`)

The mock replays a CAN recording and answers the same addresses in the same format, so you can
work on the panel without a board. It must keep:

- Basic login with user `admin` (`401` and `429` in the given format, no `WWW-Authenticate`;
  lockout rules from 1.2; an option `--lan` "client from the home network", where the changing
  requests get `403 ap_only`);
- a random `boot` at start, cursor numbering with `next` / `head` / `dropped` and the memory
  limits (200 lines, 2048 frames, 400 frames per answer, **64 lines and 4096 bytes of text per
  `/api/log` answer**), the order of array elements, the error words of 1.6, command queueing
  (`{"queued":true}`), refusal of `wifi ...` lines in `/api/cmd`, and a change of `boot` after
  `/api/restart` and after a file is accepted in `/api/ota`;
- option `--can-tx` (the running program has `canTx` = 1, `can.tx.built` = `true`): the mock shows
  the arm switch, accepts `cantx`, `cantx on|off`, `vset on|off NN` (with `--lan`: `403 ap_only`),
  refuses `cantx reset` and `set NN VV` (`usb_only`), arms after a simulated restart and disarms
  after another program is accepted. It transmits nothing: the `can.tx` counters stay at zero;
- **file in `/api/ota`:** the mock checks the header (byte 0 = `E9`, bytes 12..13 = `09 00`, bytes
  32..35 = `32 54 CD AB`), looks for the marker of chapter 6 and returns the same errors
  (`bad_image`, `wrong_product`, `no_net`, `mode_change`) and the same answer fields; `elf` from
  bytes `0xB0..0xB7`. After a simulated restart `sys.elf` and `running.elf` have that value, and
  `sys.mode` the mode from the marker;
- **`/api/status` fields** decoded from the replayed recording by the same rules as the firmware:
  `lib/car_psa_aee2010/psa_frames.*` (`0E1`, `0F6`, reverse, ignition, lights `036`),
  `lib/car_psa_aee2010/psa_signals.cpp` (doors `1A1`), `lib/core/power_policy.cpp` (ACC, freshness limits),
  `psa_signals.cpp` with `lib/radio_xp/xp_signals.cpp` (`car.settings.raw260`, `ageMs`, `state38`; the mock
  has no radio, so `last` = `null` and the counters are 0). These may be constant: `sys.*` except
  `uptimeMs` and `boot`, `radio.*`, `audio.*`, `wifi.*`;
- **VIN:** when it loads a recording the mock overwrites the data of frames `336`, `3B6`, `2B6` (for
  example with zeros) and never shows their real content. Recordings added to the repository as
  mock data must already be masked in the file (`tools/sanitize_capture.py`);
- **`GET /`:** the mock serves `web/index.html` with `app.css` and `app.js` put in, the same way as
  `embed_web.py` (gzip optional), with the same `Content-Security-Policy` header;
- **`GET /i18n/<lang>.json`:** the same dictionaries as the firmware, gzip, with `ETag` and `304`;
- **restart:** a scheduled restart waits only until the car no longer needs the device. The mock
  does NOT wait for ACC off (firmware: `netRestartDue`): the replayed recording has the ignition on
  nearly all the time, so the restart would never come;
- **`/api/hello`:** `locked` > 0 only in the `--lan` mode (as the firmware does for clients of the
  device network); `lang` = the language stored through `POST /api/lang`, the console `lang` or an
  import;
- **settings:** `POST /api/lang`, `GET` / `POST /api/settings` with the same checks and error
  details as the firmware (the parser is a simplified Python copy).

It does not have to reproduce lockout timing, flash writes or `car_busy` refusals.

Start it with `python -I tools/mock_device.py <recording.log>` in `firmware/` and open
`http://127.0.0.1:8080/`. It prints its password at start (or use `--password`). Other options:
`--port`, `--host`, `--speed`, `--mode decoder`, `--lan`, `--lan-admin`, `--can-tx`, `--no-loop`,
`--verbose`. Use a recording that has traffic from the beginning, for example
`test_data/captures/can_20261111_191045.log`.

## 10. Tables for the panel

Source of truth: `firmware/lib/app/xp_keys.cpp`,
`firmware/lib/car_psa_aee2010/profile_psa_aee2010.cpp` and `firmware/lib/radio_xp/protocol_xp.cpp`
(button codes) and `firmware/lib/radio_xp/xp_keys_custom.h` (radio functions). When those files change, fix this
table too. The car-side frames are the reference car's; their status is on
[Peugeot 508 SW](vehicles/peugeot-508.md).

**Car buttons** (code sent to the radio by the box, that is `FROM` in the remapping table):

| Code | Button | CAN frame |
|---|---|---|
| `14` | volume + | `21F` |
| `15` | volume − | `21F` |
| `16` | both volume buttons at once (mute) **and** press of the panel power knob | `21F` / `036` |
| `30` | phone | `21F` |
| `50` | LIST | `21F` |
| `07` | press of the steering-wheel thumbwheel | `21F` |
| `17`, `18` | steering-wheel thumbwheel: next / previous (one detent) | `21F` |
| `21` | SRC | `0A2` |
| `23` | trip computer button (wiper stalk) | `221` |
| `9E` | NAV | `122` |
| `A0` | MEDIA | `122` |
| `97`, `98` | scroll left / right | `122` |
| `9C`, `9D` | magnifier + / magnifier − | `122` |
| `9F` | TEL | `122` |
| `99` | SETUP | `122` |
| `9B` | DRIVE | `122` |
| `A4` | moon (screen off; handled on its own, remapping does not work) | `122` |
| `A1` | microphone mute | `122` |
| `A3` | WEB | `122` |
| `22` | a panel button whose name is not confirmed yet | `122` |
| `81`, `82` | panel knob: right / left (one detent) | `122` |

Code `16` belongs to two buttons: a remapping works for both at once.

Which physical button sends frame `0A2` (SRC, or the one between SRC and LIST) has two different
descriptions in the notes of the project: to be checked in the car.

**Radio functions** (`TO` in the remapping table; tried in the reference car with the radio set to
the CAN profile "XP Simple Soft → 508 → Type 2", measured by watching the radio):
`08` source change, `1F` voice assistant (Siri in CarPlay), `30` source change, `9A` screen
brightness, `9B` phone, `A5` climate control screen, `A6` back, `B5` FM/AM, `B9` and `BF` both noted
as tuning down by 0.05 MHz (one of the two to be checked again). No reaction: `B1`, `91`..`96`. The
panel also accepts any other hex code.

**Sound source** (`radio.source`, bytes 0..1, measured on the UART line of the reference radio):
`01 01` radio, `08 13` player (then: track number, number of tracks, minutes, seconds), `0C 30`
others; also seen `05 40` and `07 30` (meaning not known).

**Names of CAN identifiers** in the CAN tab: from `vehicles/peugeot-508-sw-2015/508.dbc`
(message names), copied into `firmware/web/profiles/psa_aee2010.json` (field `frames`;
descriptions as dictionary keys `psa.frame.*`). That file also has the tables of car buttons
(`car_keys`), radio functions (`radio_fn`), door bits (`doors`) and ignition byte values
(`ignition`); their texts are in the dictionaries `firmware/web/i18n/*.json`.
