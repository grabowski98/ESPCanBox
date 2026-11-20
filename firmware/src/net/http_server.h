// Web server of the service panel: start-up, login and shared tools for the request handlers.
// Description of the requests and responses (contract with the panel and with the Python mock): docs/PANEL_API.md.
//
// The server is esp_http_server from ESP-IDF (no extra libraries). It runs in ONE task of its own:
// requests are handled one after another, and a firmware upload blocks all others until it ends.
// This gives the rules for every request handler ("handler"):
//   1. It runs in the web task. It does not touch CAN, the UART, sound, outputs, the WiFi library or the logic
//      objects of main.cpp. It takes data from diag_hub (copies) and from settings (copies), and orders commands
//      via diagPostRequest(). It does not write to USB: only conLogf().
//   2. First httpRequireAuth() (everything under /api/ except /api/hello), only then anything else.
//      Request headers must be read BEFORE the first part of the response is sent: the server discards them afterwards.
//   3. No heap and no large local variables: the task stack has HTTP_TASK_STACK bytes, a handler
//      may use at most approx. 512 bytes. Larger buffers are static (sizes below);
//      this is allowed because there is a single task and requests do not overlap.
//   4. Everything that came from the network has its length checked before it reaches a buffer: request body
//      (Content-Length), URL parameters, form fields, headers. Values from the URL and the form
//      are decoded by webauth::formValue (always with the output buffer size).
//   5. httpd_resp_set_hdr / _set_type / _set_status store POINTERS, not copies. The values must
//      be constant texts or static buffers that do not change until sending has finished.
//   6. The server NEVER sends the WWW-Authenticate header. The browser then does not show its own
//      login window and does not remember the password, so a foreign page cannot send a request with the password.
//      For the same reason there are no Access-Control-* headers (no access from other origins).
//   7. A handler returns ESP_OK when the connection may stay open, and ESP_FAIL when the server should
//      close it (unread request body, send or receive error). Helper functions
//      with the 'ret' parameter say themselves what to return.
//   8. Every loop that may take longer than one portion (upload, discarding the body, the
//      trace.log file, scanning the other firmware slot) checks httpStopRequested() on every iteration
//      and then ends immediately (for an upload esp_ota_abort, return ESP_FAIL).
//      Every body read also has a TOTAL deadline (not only the limit of a single receive): a client
//      that sends one byte every few seconds must not occupy the only server task for long.
//   9. Time: take millis() for computing the age (ageMs) AFTER copying the data from diag_hub; a negative
//      difference = 0 (see net.h, "Time").
//  10. Text coming from a client that has NOT passed the login (e.g. the user name from a header)
//      never reaches the line ring or USB.
//
// Internal file of src/net; included only from files compiled with NET_ENABLED = 1.
// Do NOT include together with <WebServer.h> or <HTTPUpdateServer.h> (the same symbol HTTP_ANY).
// Implementation: src/net/http_server.cpp (server, login, page, tools),
// src/net/api_handlers.cpp (/api/... requests except the update), src/net/ota.cpp (update).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <esp_http_server.h>

#include "frame_trace.h"     // FrameTable::MAX_IDS, TraceRecord
#include "json_writer.h"
#include "line_log.h"

// ---------- server parameters (httpd_config_t cfg = HTTPD_DEFAULT_CONFIG(); then individual fields) ----------

const uint16_t HTTP_PORT = 80;
// Server task stack in bytes. Default 4096; doubled because the update handler calls
// the flash write (esp_ota_write) directly from this task. The headroom is shown by sys.stackHttpFree.
const size_t HTTP_TASK_STACK = 8192;
// Core 0: that is where Wi-Fi and the network stack run. The main loop (core 1) loses no time handling requests.
const int HTTP_TASK_CORE = 0;
// Priority below the sound task (5), so that building responses does not break up the beeping; above
// the idle task and mDNS. Wi-Fi (23) and the network stack (18) are higher and take care of themselves.
const unsigned HTTP_TASK_PRIO = 4;
// Connections open at the same time. The whole program has 16 sockets, the server reserves 3 for itself. A browser
// opens up to 6 connections; when full, the server closes the least recently used one (lru_purge_enable = true).
const uint16_t HTTP_MAX_SOCKETS = 7;
// Number of addresses: 25 used (2 + one per separate panel dictionary, 5 today, in http_server.cpp,
// 15 in api_handlers.cpp, 3 in ota.cpp) and spare. The default 8 is not enough: registering the ninth one fails.
// Each slot costs one pointer; a registered address a few dozen bytes of heap, once at server start.
const uint16_t HTTP_MAX_ROUTES = 32;
// Server buffer for the request headers (default 1024 bytes). When exceeded the server answers 431
// and the panel does not work, and the browser adds its own headers that we have no control over (User-Agent,
// Accept-Language, Referer; plus our Authorization, approx. 110 bytes). The actual length on the
// radio and the phone has not been measured, hence the double margin.
const size_t HTTP_REQ_HDR_MAX = 2048;
// Response headers set via httpd_resp_set_hdr: at most 7 per response (main page,
// including Content-Security-Policy); the server's default limit of 8 stays.
const uint16_t HTTP_MAX_RESP_HEADERS = 8;
// Timeout of a single receive / send on the socket, in seconds (default 5; shortened so that
// a dead connection frees the only server task sooner).
const uint16_t HTTP_RECV_TIMEOUT_S = 3;
const uint16_t HTTP_SEND_TIMEOUT_S = 3;
// TOTAL deadline for reading (and discarding) a form body and the body after a failed login.
const uint32_t HTTP_DRAIN_DEADLINE_MS = 1000;
// TOTAL deadline for receiving the request line and the headers (and for discarding an unread body
// by the server after the handler). The HTTP_RECV_TIMEOUT_S limit counts from every byte, so a client
// sending the headers one byte every 2.9 s would hold the only server task forever (measurement 17.11.2026:
// slow-drip test client, the panel was stalled for the whole drip). Worst case: this deadline + one
// receive (HTTP_RECV_TIMEOUT_S). A browser sends the headers in a single packet, so 3 s is a large margin.
const uint32_t HTTP_HDR_DEADLINE_MS = 3000;
// Pause without reception on the socket after which the next receive outside a handler counts as the
// start of a NEW request (keep-alive connection after idling). A dripping client will not make such a pause:
// the server calls the next receive immediately after the previous one.
const uint32_t HTTP_HDR_IDLE_RESET_MS = 500;
// TOTAL deadline for sending one response, counted from its first send (sessSend, lib/core/send_deadline.h).
// HTTP_SEND_TIMEOUT_S limits only one send, so a client reading a few bytes every 2 s held the only server
// task for as long as it liked (code review 19.11.2026: slow-read, also on GET / without login). The largest
// responses: the page (about 45 kB gzip) and /api/trace.log (at most 2048 lines of up to 41 characters, about
// 84 kB): 20 s is enough down to about 4 kB/s. Worst case: this deadline + one send (HTTP_SEND_TIMEOUT_S).
const uint32_t HTTP_SEND_DEADLINE_MS = 20000;

// ---------- buffers (all static, used only in the web task) ----------

// Shared working buffer of the web task: building JSON and text responses, and during a firmware
// upload the receive buffer. 16 KB holds the largest response: 400 frames of 38 characters.
const size_t HTTP_BUF_SIZE = 16384;
// Longest accepted form body (POST). The largest valid form is a console line:
// "line=" and 127 characters, each encoded as %XX, 386 bytes in total.
const size_t HTTP_FORM_MAX = 1024;
// A body longer than HTTP_FORM_MAX, but not longer than this, is read and discarded (response 413,
// the connection stays), provided it completes within HTTP_DRAIN_DEADLINE_MS. Even longer or
// slower: response 413 and the connection is closed.
const size_t HTTP_FORM_DRAIN_MAX = 65536;
// Buffers on the handler stack: URL parameters ("c=4294967295&id=7FF&max=400" is 27 characters)
// and the Authorization header ("Basic " + 92 characters for the longest password).
const size_t HTTP_QUERY_MAX = 128;
const size_t HTTP_AUTH_HDR_MAX = 160;
// Working buffers of api_handlers.cpp for copying data from under the diag_hub lock.
const size_t HTTP_FRAME_BATCH = 64;          // TraceRecord[64] = 1 KB; frames read in portions
const size_t HTTP_LOG_ENTRIES = 64;          // LineLog::Entry[64] = 512 B
const size_t HTTP_LOG_TEXT = 4096;           // text of one portion of lines
// ID table: TableEntry[FrameTable::MAX_IDS] = approx. 3 KB. State snapshot: one copy of StatusSnapshot.

// Limits of the frame responses (GET /api/frames).
const uint32_t HTTP_FRAMES_DEFAULT = 256;    // when there is no max parameter
const uint32_t HTTP_FRAMES_MAX = 400;        // a larger max is clipped; 400 x 38 characters fit in the buffer

// User name for Basic login. The password is the device password from the settings module.
#define HTTP_AUTH_USER "admin"

// ---------- life cycle: called ONLY by the net_wifi task (src/net/wifi_mgr.cpp), NEVER by the main loop ----------

// Starts the server and registers all addresses (own ones, apiRegisterRoutes, otaRegisterRoutes).
// Call after the own Wi-Fi network has started. Blocks for a few ms. The server itself allocates memory
// for its task stack and the connections (that is ESP-IDF memory, not our code's).
// false = the start or the registration of some address failed; the server is then stopped.
// Calling again with the server running does nothing and returns true.
bool httpServerStart();

// Stops the server. Call BEFORE MDNS.end() and WiFi.mode(WIFI_OFF). First action: setting
// the httpStopRequested() flag, so that long handler loops end at the next portion.
// Then httpd_stop, which waits for the running request to end: usually ms; with a slow client
// up to a few seconds (limits of a single receive/send HTTP_*_TIMEOUT_S and the total deadlines; sending one
// response to a slow reader is bounded by HTTP_SEND_DEADLINE_MS). That is why only
// the net_wifi task calls this. With the server stopped it does nothing.
void httpServerStop();

bool httpServerRunning();          // any task
// true from the start of httpServerStop() until the next httpServerStart(). Any task (volatile variable).
bool httpStopRequested();

// ---------- address registration ----------

// Registers one address (httpd_uri_t u = {}; fields uri, method, handler; httpd_register_uri_handler).
// The handler goes into user_ctx, and the server calls the trampoline from http_server.cpp (header deadline
// HTTP_HDR_DEADLINE_MS); the field req->user_ctx is therefore taken and handlers do not use it.
// 'uri' must be a constant text (the server keeps the pointer). false = registration error (printed via
// conLogf). The uri_match_fn field in the server configuration stays empty (NULL): the server then compares
// the address without the part after the '?' character. That the comparison is exact, and not "starts with", follows only
// indirectly from the CameraWebServer example shipped with the core ("/" registered before "/control");
// the server code is not available. Therefore the REGISTRATION ORDER applies: a longer address before
// a shorter one with the same beginning ("/api/ota/rollback" and "/api/ota/status" before "/api/ota"),
// and "/" last of all. Then both ways of comparing give the same result.
bool httpRegister(httpd_handle_t server, const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*));

// src/net/api_handlers.cpp: registers (via httpRegister) GET /api/hello, /api/status, /api/log,
// /api/frames, /api/table, /api/keymap, /api/settings, /api/trace.log and POST /api/cmd, /api/keymap,
// /api/lang, /api/settings, /api/wifi, /api/password, /api/restart. false = one of the registrations failed.
// Called by httpServerStart(), in the main loop task, before the server accepts the first request.
bool apiRegisterRoutes(httpd_handle_t server);
// The three update addresses are registered by otaRegisterRoutes() from ota.h.
// http_server.cpp itself registers: GET /i18n/<lang>.json (one address per row of WEB_I18N), GET /favicon.ico
// (response 204) and, at the very end, GET / (the panel page).
// Order in httpServerStart(): otaRegisterRoutes, apiRegisterRoutes, /i18n/..., /favicon.ico, /.

// The panel page comes from the file web_assets.h, GENERATED before compilation by tools/embed_web.py
// into the directory $BUILD_DIR/generated (added to the compiler include paths). Only http_server.cpp includes it.
// Contract for the content of that file:
//     static const uint8_t WEB_INDEX_GZ[] = { ... };            // the page in a single file, gzip-compressed
//     static const size_t  WEB_INDEX_GZ_LEN = ...;              // number of bytes
//     static const char    WEB_INDEX_ETAG[] = "\"0123456789abcdef\"";   // including the quotes: the first
//                                         // 16 hex digits (lowercase) of the SHA-256 of the compressed bytes
//     static const char    WEB_INDEX_CSP[] = "default-src 'none'; script-src 'sha256-<base64>'; ...";
//                                         // full value of the Content-Security-Policy header (docs/PANEL_API.md,
//                                         // GET /); the SHA-256 hash (base64) computed from the content of the ONLY
//                                         // <script> tag of the page after app.js is inserted
//     struct WebI18nAsset { const char* uri; const uint8_t* gz; size_t len; const char* etag; };
//     static const WebI18nAsset WEB_I18N[] = { {"/i18n/pl.json", <gzip array>, <bytes>, "\"<16 hex>\""}, ...,
//                                              {nullptr, nullptr, 0, nullptr} };   // end row
//     static const size_t WEB_I18N_COUNT = ...;                 // rows without the end row; 0 for the fallback page
//                                         // every panel dictionary except English (which is inside the page)
// When web/index.html does not exist, the script generates a placeholder page (one paragraph "the panel
// was not built", no script) and prints a warning: the firmware can be built and tested without the panel.

// ---------- tools for request handlers (web task ONLY) ----------

enum class HttpStatus : uint8_t {
    OK = 0,               // "200 OK"
    BAD_REQUEST,          // "400 Bad Request"
    UNAUTHORIZED,         // "401 Unauthorized"
    FORBIDDEN,            // "403 Forbidden"
    CONFLICT,             // "409 Conflict"
    LENGTH_REQUIRED,      // "411 Length Required"
    TOO_LARGE,            // "413 Content Too Large"
    TOO_MANY,             // "429 Too Many Requests"
    INTERNAL,             // "500 Internal Server Error"
    UNAVAILABLE,          // "503 Service Unavailable"
};

// Constant status line text for httpd_resp_set_status (e.g. "409 Conflict").
const char* httpStatusText(HttpStatus status);

// Shared working buffer of size HTTP_BUF_SIZE. Valid only inside one handler;
// the content at entry is random. It is destroyed by: httpRequireAuth (when it discards the request body),
// httpReadForm (when it discards a too long body) and httpDiscardBody. Not destroyed by: httpSendJson, httpSendError,
// httpQueryValue, httpQueryU32.
char* httpBuf();

// Kind of request for httpRequireAuth: whether it changes the device state.
enum class HttpAccess : uint8_t {
    READ = 0,     // read only (state, log, frames, table, keys, update state, trace.log)
    WRITE,        // change: /api/cmd, POST /api/keymap, /api/wifi, /api/password, /api/restart,
                  // /api/ota, /api/ota/rollback
};

// Checks the login: header "Authorization: Basic base64(admin:<device password>)".
// Returns true = the request may be handled; it then records client activity (diagTouchClient).
// Returns false = the response has already been sent; the handler must return 'ret' immediately.
// Order of the checks:
//   0. settingsGetDevicePassword() returns false or the password does not satisfy webauth::passwordAcceptable:
//      503 {"error":"not_ready"}, without comparison and without changing LoginGuard. An empty text is NEVER
//      a valid password (the "no password" state is closed, not open);
//   1. client outside the own network (httpPeerIsAp() == false) and a lockout after wrong passwords is in force
//      (webauth::LoginGuard, one instance in http_server.cpp): 429 {"error":"locked","retryInS":N},
//      also for a correct password. Clients of the device's own network are NOT subject to the lockout (they already
//      know the password, because it is the WPA2 key of that network), so the owner in the car is never locked out;
//   2. no Authorization header: 401 {"error":"auth"}; does NOT count as a failed attempt;
//   3. unreadable header, a user name other than HTTP_AUTH_USER or a wrong password
//      (comparison webauth::constantTimeEquals, always performed, also with a wrong name):
//      401 {"error":"auth"}; for a client outside the own network LoginGuard::onFailure, UNLESS the given
//      password is identical to the password of the previous failed attempt (static 64 B buffer in
//      http_server.cpp, comparison constantTimeEquals): an old browser tab with the old password cannot
//      lock the panel by itself. Line via conLogf: a constant text and the client IP address, NEVER
//      the given name or password;
//   4. correct password, access == WRITE, client outside the own network and settingsLanAdmin() == false:
//      403 {"error":"ap_only"} (changes only from the device's own network, unless "wifi lan on" on USB);
//   5. correct: LoginGuard::onSuccess, diagTouchClient.
// When the request has a body (POST) and the login failed: a body up to HTTP_FORM_MAX is read
// and discarded within HTTP_DRAIN_DEADLINE_MS before the response (ret = ESP_OK); a longer or
// slower one is not read further, and after the response the connection is closed (ret = ESP_FAIL).
// Buffers on the stack: header HTTP_AUTH_HDR_MAX, name 16, two passwords of webauth::PASSWORD_MAX + 1 each.
bool httpRequireAuth(httpd_req_t* req, esp_err_t& ret, HttpAccess access);

// true = the client is in the device's own network: BOTH conditions (socket httpd_req_to_sockfd(req);
// lwip/sockets.h, IPv4 addresses also in the form ::ffff:a.b.c.d):
//   1. the LOCAL address of the connection (getsockname) is exactly NET_AP_IP (the connection came to the address
//      of the device's network, not to the address from the home network);
//   2. the address of the OTHER side (getpeername) lies in the subnet NET_AP_IP/24.
// Condition 2 alone is not enough: from the home network one can send a SYN to the STA address with a spoofed source
// 10.50.8.x, and lwIP (LWIP_HOOK_IP4_ROUTE_SRC) sends the reply via STA. With condition 1 the reply
// to the spoofed source goes via the AP interface and the connection is not established.
// Address read error = false (treated as a client from the home network).
bool httpPeerIsAp(httpd_req_t* req);

// How many seconds the login lockout still lasts (0 = none), rounded up. For GET /api/hello
// (shown regardless of where the client asks from).
uint32_t httpLoginLockedS();

// Sends the built JSON: type "application/json", headers "Cache-Control: no-store"
// and "X-Content-Type-Options: nosniff", status line per 'status'. When j.ok() == false (the response did not
// fit in the buffer), it sends 500 {"error":"overflow"} instead.
// The JsonWriter buffer (usually httpBuf()) must exist until the function returns. Returns the send result:
// the handler ends with "return httpSendJson(...)".
esp_err_t httpSendJson(httpd_req_t* req, const JsonWriter& j, HttpStatus status = HttpStatus::OK);

// Sends the error {"error":"<code>"} or {"error":"<code>","detail":"<detail>"} with the given status.
// 'code' is a constant word from the table in docs/PANEL_API.md (e.g. "bad_request", "busy", "car_busy");
// 'detail' optional, short (up to 64 characters, e.g. the name of the invalid parameter). Builds the response
// in a small buffer on the stack; httpBuf() stays untouched. Returns the send result.
esp_err_t httpSendError(httpd_req_t* req, HttpStatus status, const char* code, const char* detail = nullptr);

// Reads a form body (application/x-www-form-urlencoded) into its own static buffer
// of size HTTP_FORM_MAX + 1 and zero-terminates it, within HTTP_DRAIN_DEADLINE_MS.
// Call AFTER httpRequireAuth.
// true: 'body' and 'len' describe the body (valid until the end of the handler); the fields are extracted by
//       webauth::formValue(body, len, "name", buffer, size).
// false: an error response has been sent, the handler returns 'ret'. Cases:
//       no body (Content-Length 0 or missing): 411 "length_required";
//       body longer than HTTP_FORM_MAX: 413 "too_large" (discarding as in httpRequireAuth);
//       error, receive timeout or deadline passed: no response, ret = ESP_FAIL.
bool httpReadForm(httpd_req_t* req, const char*& body, size_t& len, esp_err_t& ret);

// Parameter from the URL (the part after '?'), decoded (%XX, '+'). The buffer for the whole parameter text
// (HTTP_QUERY_MAX) is on the stack of this function. false = no parameter, the parameter text longer than
// HTTP_QUERY_MAX - 1, encoding error or a value longer than cap - 1; 'out' is then empty.
bool httpQueryValue(httpd_req_t* req, const char* key, char* out, size_t cap);

// Numeric parameter: decimal digits only, at most 10, value up to 4294967295.
// 'present' = the parameter was in the URL (false also when the parameter text is too long and was
// skipped entirely). Returns true when the parameter is present and correctly written.
// (present == true and result false means an invalid notation: the handler answers 400 "bad_request".)
bool httpQueryU32(httpd_req_t* req, const char* key, uint32_t& out, bool& present);

// Reads and discards 'remaining' bytes of the remaining request body (into httpBuf()), so that the connection
// can be used for the next request. Ends early when 'deadlineMs' (millis() time) passes.
// true = everything discarded; false = error, timeout, deadline or httpStopRequested(): the
// handler must return ESP_FAIL.
// After every portion it yields the processor (vTaskDelay(1)), because the core 0 idle task is guarded by
// the watchdog (5 s).
bool httpDiscardBody(httpd_req_t* req, size_t remaining, uint32_t deadlineMs);
