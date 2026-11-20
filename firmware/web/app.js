// Service panel of the device. Contract with the device: docs/PANEL_API.md.
// Rules (chapter 7 of the contract):
//  - every text from the device reaches the page ONLY via textContent (the network name and console
//    lines may have been set by someone else); there is no innerHTML in this file;
//  - the password in sessionStorage, or in localStorage when "Remember on this device" is ticked; we add the Authorization header ourselves (the device does not send
//    WWW-Authenticate, so the browser does not show its own window);
//  - the next request of the same kind only after the response to the previous one; a background tab does not poll;
//  - this script is the ONLY script of the page (the CSP header allows it by its SHA-256 hash), which is why
//    we attach the events here and not in HTML attributes;
//  - no user-visible text in this file: every text comes from the dictionaries web/i18n/<lang>.json through
//    t() / tp() (rules in web/i18n/README.md, check: tools/check_i18n.py). tools/embed_web.py inserts the
//    English dictionary (I18N), the language list (LANGS) and the vehicle profile (PROFILE) in place of the
//    three marked {} below; every other dictionary is fetched from /i18n/<lang>.json when it is chosen.
(function () {
  'use strict';

  // ---------- constants from the contract ----------
  // Dictionaries {lang: {key: text}} (only "en" in the page, the others are added by loadLang), the
  // languages of this image {code: name} and the vehicle profile; tools/embed_web.py replaces the marked {}.
  const I18N = /*@I18N@*/{};
  const LANGS = /*@LANGS@*/{};
  const PROFILE = /*@PROFILE@*/{};
  // Project identity: the only copy in the panel, the line is written by tools/set_project_name.py from project.json.
  const IDENT = Object.freeze({ name: 'ESPCanBox', slug: 'espcanbox', product: 'espcanbox', magic: 'ESPC-FWID', apSsidPrefix: 'ESPCanBox-', mdnsHost: 'espcanbox', nvsNamespace: 'espcanbox', sessionKey: 'espcanboxpw' });  // from project.json (tools/set_project_name.py)
  // FwIdent marker: 0x7F, 9 characters of IDENT.magic, 0x7F, zero = 12 bytes (lib/core/fw_ident.h).
  const MARKER = [0x7F].concat(Array.from(IDENT.magic, function (c) { return c.charCodeAt(0); }), [0x7F, 0x00]);
  const PRODUCT = IDENT.product;
  const MIN_IMAGE = 65536;                                     // the device rejects a shorter file (short)

  // Vehicle-specific tables from the profile (web/profiles/<profile>.json): frames carrying the VIN
  // (src/net/diag_types.h), frame names (DBC message names + i18n key of the description), car keys and
  // radio functions (docs/PANEL_API.md, chapter 10), door bits of the top view, ignition byte values.
  const VIN_IDS = {};
  (PROFILE.vin_ids || []).forEach(function (id) { VIN_IDS[id] = 1; });
  const CAN_NAMES = PROFILE.frames || {};
  const CAR_KEYS = PROFILE.car_keys || [];
  const RADIO_FN = PROFILE.radio_fn || [];
  const DOORS = PROFILE.doors || [];
  const IGN = PROFILE.ignition || { label: 'car.chip.ign', values: {} };

  // Error words of the API (PANEL_API.md 1.6) -> dictionary keys; an unknown word is shown unchanged.
  const ERRORS = {
    bad_request: 'err.bad_request', weak_password: 'err.weak_password', bad_image: 'err.bad_image',
    wrong_product: 'err.wrong_product', auth: 'err.auth', wrong_password: 'err.wrong_password', ap_only: 'err.ap_only',
    car_busy: 'err.car_busy', ota_pending: 'err.ota_pending', restart_pending: 'err.restart_pending',
    no_net: 'err.no_net', mode_change: 'err.mode_change', no_rollback: 'err.no_rollback',
    length_required: 'err.length_required', too_large: 'err.too_large', locked: 'err.locked', flash: 'err.flash',
    overflow: 'err.overflow', busy: 'err.busy', not_ready: 'err.not_ready'
  };
  const PARK_STATUS = { 0: 'park.status.no_frame', 1: 'park.status.fault', 2: 'park.status.off', 4: 'park.status.active',
    5: 'park.status.waiting', 6: 'park.status.failed' };
  const KEY_STATE = { 0: 'keymap.state.released', 1: 'keymap.state.pressed', 2: 'keymap.state.held' };

  // ---------- translations ----------
  // t(key, params): text in the chosen language -> English -> the key itself; {name} is replaced by params.name.
  // tp(key, n, params): plural form key_one / _few / _many / _other chosen by Intl.PluralRules (without Intl,
  // e.g. in an old radio browser: _other). Language: device (/api/hello, POST /api/lang), localStorage = cache.
  const LANG_KEY = 'ui_lang';
  let LANG = 'en';
  const has = Object.prototype.hasOwnProperty;
  function lookup(lang, key) {
    const d = I18N[lang];
    return d && has.call(d, key) ? d[key] : undefined;
  }
  function fill(s, p) {
    if (!p) return s;
    return s.replace(/\{([A-Za-z0-9_]+)\}/g, function (m, k) { return has.call(p, k) ? String(p[k]) : m; });
  }
  function t(key, p) {
    let s = lookup(LANG, key);
    if (s === undefined) s = lookup('en', key);
    if (s === undefined) s = key;
    return fill(s, p);
  }
  const pluralRules = {};
  function pluralCat(lang, n) {
    try {
      if (!pluralRules[lang]) pluralRules[lang] = new Intl.PluralRules(lang);
      return pluralRules[lang].select(n);
    } catch (e) { return 'other'; }
  }
  function tp(key, n, p) {
    const q = {};
    if (p) Object.keys(p).forEach(function (k) { q[k] = p[k]; });
    q.n = n;
    let s = lookup(LANG, key + '_' + pluralCat(LANG, n));
    if (s === undefined) s = lookup(LANG, key + '_other');
    if (s === undefined) s = lookup('en', key + '_' + pluralCat('en', n));
    if (s === undefined) s = lookup('en', key + '_other');
    if (s === undefined) s = key;
    return fill(s, q);
  }
  // Word from the API -> text through a {word: key} map; an unknown word stays unchanged.
  function tw(map, w) { return has.call(map, w) ? t(map[w]) : w; }
  function langGet() { try { return localStorage.getItem(LANG_KEY); } catch (e) { return null; } }
  function langSet(v) { try { localStorage.setItem(LANG_KEY, v); } catch (e) { /* no storage: the choice lasts until reload */ } }
  function langList() {
    return Object.keys(LANGS).sort(function (a, b) { return a === 'en' ? -1 : b === 'en' ? 1 : (a < b ? -1 : a > b ? 1 : 0); });
  }

  // ---------- DOM tools ----------
  const $ = function (id) { return document.getElementById(id); };
  function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined && text !== null) e.textContent = String(text);
    return e;
  }
  function clear(n) { while (n.firstChild) n.removeChild(n.firstChild); }
  function setText(n, v) {
    const s = (v === null || v === undefined || v === '') ? '—' : String(v);
    if (n.textContent !== s) n.textContent = s;
  }
  function show(n, on) { n.hidden = !on; }
  function hex2(n) { return ('0' + (n & 255).toString(16).toUpperCase()).slice(-2); }
  function pad(n, w) { let s = String(n); while (s.length < w) s = '0' + s; return s; }
  // Number formatting of the chosen language: thousands separator (fmt.group), decimal separator
  // (fmt.decimal) and the typographic minus sign.
  function num(n) {
    if (typeof n !== 'number') return '—';
    const g = t('fmt.group');
    return String(n).replace(/\B(?=(\d{3})+(?!\d))/g, function () { return g; }).replace(/^-/, '\u2212');
  }
  function dec(x, digits) {
    if (typeof x !== 'number') return '—';
    return x.toFixed(digits).replace('.', t('fmt.decimal')).replace(/^-/, '\u2212');
  }
  function yesNo(b) { return b === true ? t('common.yes') : (b === false ? t('common.no') : '—'); }
  function duration(ms) {
    if (typeof ms !== 'number') return '—';
    let s = Math.floor(ms / 1000);
    const d = Math.floor(s / 86400); s -= d * 86400;
    const h = Math.floor(s / 3600); s -= h * 3600;
    const m = Math.floor(s / 60); s -= m * 60;
    return (d ? t('fmt.days', { n: d }) + ' ' : '') + pad(h, 2) + ':' + pad(m, 2) + ':' + pad(s, 2);
  }
  // Age: below 10 s in milliseconds, above that as a duration.
  function fmtAge(ms) { return t('fmt.ago', { t: ms < 10000 ? ms + ' ms' : duration(ms) }); }
  function devTime(ms) { return Math.floor(ms / 1000) + '.' + pad(ms % 1000, 3); }
  function errText(r) {
    if (!r) return t('err.no_response');
    if (r.status === 0) return t('err.offline');
    const d = r.data;
    if (d && typeof d.error === 'string') {
      // A command the panel must not run (Wi-Fi passwords, "set NN VV", "cantx reset"): say so instead of "Invalid data".
      if (d.error === 'bad_request' && d.detail === 'usb_only') return t('err.usb_only');
      let m = tw(ERRORS, d.error);
      if (typeof d.detail === 'string' && d.detail) m += ' (' + d.detail + ')';
      if (d.error === 'locked' && typeof d.retryInS === 'number') m = t('err.locked_retry', { msg: m, s: d.retryInS });
      return m;
    }
    return t('err.http', { status: r.status });
  }
  function stamp() {
    const d = new Date();
    return d.getFullYear() + pad(d.getMonth() + 1, 2) + pad(d.getDate(), 2) + '_' +
      pad(d.getHours(), 2) + pad(d.getMinutes(), 2) + pad(d.getSeconds(), 2);
  }
  function saveFile(name, blob) {
    const url = URL.createObjectURL(blob);
    const a = el('a');
    a.href = url; a.download = name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 5000);
  }
  function sleep(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }

  // ---------- state ----------
  const S = {
    // lockUntil: lockout confirmed by a 429 response (holds back requests and login).
    // helloLockUntil: lockout reported in /api/hello, only for the label (does not hold back a login attempt).
    pw: null, prevPw: null, remember: false, authed: false, offline: false, lockUntil: 0, helloLockUntil: 0,
    boot: null, status: null, tab: 'pulpit',
    logCur: null, frCur: null, apOnlyUntil: 0,
    keymap: null, editorLoaded: false, otaBusy: false, otaLoaded: false
  };
  // Saved password: sessionStorage (this tab only, the default) or, with "Remember on this device",
  // localStorage (every tab, survives a browser restart). Same key in both; read session first, then local.
  // Every access in try/catch: a private window may refuse storage, then the password lives only in S.pw.
  function storeRead(st) { try { return st.getItem(IDENT.sessionKey); } catch (e) { return null; } }
  function storeWrite(st, v) { try { if (v === null) st.removeItem(IDENT.sessionKey); else st.setItem(IDENT.sessionKey, v); } catch (e) { /* no storage */ } }
  function storeGet() {
    const s = storeRead(sessionStorage0());
    if (s !== null) { S.remember = false; return s; }
    const l = storeRead(localStorage0());
    if (l !== null) S.remember = true;
    return l;
  }
  // v === null: forget the password in BOTH places (logout, 401). Otherwise only in the chosen one.
  function storeSet(v, remember) {
    if (v === null) { storeWrite(sessionStorage0(), null); storeWrite(localStorage0(), null); return; }
    storeWrite(remember ? localStorage0() : sessionStorage0(), v);
    storeWrite(remember ? sessionStorage0() : localStorage0(), null);
  }
  // Forget the password pw wherever that exact value is saved (both storages).
  function storeForget(pw) {
    [sessionStorage0(), localStorage0()].forEach(function (st) { if (pw !== null && storeRead(st) === pw) storeWrite(st, null); });
  }
  // Even reading window.localStorage may throw (storage blocked); a null store makes storeRead/Write no-ops.
  function sessionStorage0() { try { return window.sessionStorage; } catch (e) { return null; } }
  function localStorage0() { try { return window.localStorage; } catch (e) { return null; } }

  // ---------- messages ----------
  let toastTimer = null;
  function toast(text, bad) {
    const box = $('toast');
    box.textContent = text;
    box.className = 'toast show' + (bad ? ' bad' : '');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { box.className = 'toast'; }, 4000);
  }
  function msg(node, text, kind) {
    node.textContent = text || '';
    node.className = 'msg' + (kind ? ' ' + kind : '');
  }
  function updateBanner() {
    const b = $('banner');
    let m = '', cls = 'banner';
    const now = Date.now();
    if (S.offline) { m = t('banner.offline'); cls += ' bad'; }
    else if (now < Math.max(S.lockUntil, S.helloLockUntil)) { m = t('banner.locked', { s: Math.ceil((Math.max(S.lockUntil, S.helloLockUntil) - now) / 1000) }); cls += ' bad'; }
    else if (now < S.apOnlyUntil) { m = t('err.ap_only'); cls += ' warn'; }
    if (b.textContent !== m) b.textContent = m;
    b.className = cls;
    show(b, m !== '');
  }

  // ---------- question dialog (instead of confirm(): works the same in the radio's browser) ----------
  function ask(title, text, okLabel, danger) {
    return new Promise(function (resolve) {
      const m = $('modal');
      $('modal-title').textContent = title;
      const body = $('modal-text');
      clear(body);
      (Array.isArray(text) ? text : [text]).forEach(function (p) { body.appendChild(el('p', null, p)); });
      const ok = $('modal-ok'), cancel = $('modal-cancel');
      ok.textContent = okLabel || t('modal.ok');
      ok.className = 'btn ' + (danger ? 'danger' : 'primary');
      function done(v) {
        m.hidden = true;
        ok.removeEventListener('click', yes);
        cancel.removeEventListener('click', no);
        resolve(v);
      }
      function yes() { done(true); }
      function no() { done(false); }
      ok.addEventListener('click', yes);
      cancel.addEventListener('click', no);
      m.hidden = false;
      cancel.focus();
    });
  }

  // ---------- requests ----------
  async function rawFetch(method, path, opt, pw) {
    const headers = {};
    if (!opt.noAuth) headers.Authorization = 'Basic ' + btoa('admin:' + pw);
    let body;
    if (opt.form) {
      body = new URLSearchParams(opt.form).toString();
      headers['Content-Type'] = 'application/x-www-form-urlencoded';
    }
    const ctl = new AbortController();
    const tm = setTimeout(function () { ctl.abort(); }, opt.timeout || 8000);
    try {
      const res = await fetch(path, { method: method, headers: headers, body: body, cache: 'no-store',
        credentials: 'omit', signal: ctl.signal });
      const r = { status: res.status, data: null, text: null, blob: null };
      const ct = res.headers.get('Content-Type') || '';
      if (opt.blob && res.status === 200) r.blob = await res.blob();
      else if (ct.indexOf('application/json') >= 0) { try { r.data = await res.json(); } catch (e) { r.data = null; } }
      else r.text = await res.text();
      return r;
    } catch (e) {
      return { status: 0, data: null };
    } finally {
      clearTimeout(tm);
    }
  }

  async function api(method, path, opt) {
    opt = opt || {};
    if (!opt.noAuth && S.pw === null) return { status: 401, data: { error: 'auth' } };
    let r = await rawFetch(method, path, opt, S.pw);
    // After a password change we use the new one right away; the device accepts it only after the request
    // has been executed by the main loop (longer when the car needs the device). On 401 we retry once with the old one.
    if (r.status === 401 && !opt.noAuth && S.prevPw !== null) {
      const r2 = await rawFetch(method, path, opt, S.prevPw);
      if (r2.status !== 401) return after(r2, opt);
      S.prevPw = null;
    }
    if (r.status === 200 && !opt.noAuth && S.prevPw !== null) S.prevPw = null;
    return after(r, opt);
  }
  function after(r, opt) {
    if (r.status === 0) { if (!opt.quiet) setOffline(true); return r; }
    setOffline(false);
    const d = r.data;
    if (r.status === 401 && !opt.noAuth && !opt.loginTry) authLost(t('login.lost'), true);
    if (r.status === 429 && d && typeof d.retryInS === 'number') { S.lockUntil = Date.now() + d.retryInS * 1000; updateBanner(); }
    if (r.status === 403 && d && d.error === 'ap_only') { S.apOnlyUntil = Date.now() + 15000; updateBanner(); }
    if (d && typeof d.boot === 'number') checkBoot(d.boot);
    return r;
  }
  function setOffline(v) {
    if (S.offline === v) return;
    S.offline = v;
    updateBanner();
    $('conn').className = 'dot ' + (v ? 'bad' : 'ok');
  }
  function checkBoot(b) {
    if (S.boot !== null && b !== S.boot) onReboot();
    S.boot = b;
  }
  // Boot number changed: the device has restarted, the cursors are invalid (contract 1.5).
  function onReboot() {
    S.logCur = null; S.frCur = null;
    addLogLines([[null, t('log.restarted')]], true);
    if (rec.on) rec.lines.push('# device restart');
    S.editorLoaded = false;
    S.otaLoaded = false;
  }

  // ---------- polling loops ----------
  // Each loop asks only after the response to the previous request. When the condition is not met
  // (background tab, another tab, not logged in, lockout, no connection), the loop only waits, without requests.
  function loop(fn, delay, cond) {
    async function tick() {
      let again = false;
      if (S.authed && !S.offline && !document.hidden && Date.now() >= S.lockUntil && cond()) {
        try { again = await fn(); } catch (e) { again = false; }
      }
      setTimeout(tick, again ? 30 : (typeof delay === 'function' ? delay() : delay));
    }
    setTimeout(tick, 0);
  }
  function always() { return true; }

  // /api/hello: no login. Only when there is no connection or no login (shows the lockout).
  async function helloLoop() {
    for (;;) {
      if (!document.hidden && (S.offline || !S.authed)) {
        const r = await api('GET', '/api/hello', { noAuth: true, quiet: true });
        if (r.status === 200 && r.data) {
          setOffline(false);
          const d = r.data;
          setText($('login-info'), d.product === PRODUCT ? t('login.info', { name: IDENT.name, api: d.api }) : t('login.unknown_device'));
          S.helloLockUntil = d.locked > 0 ? Date.now() + d.locked * 1000 : 0;
          const dl = has.call(LANGS, d.lang) ? d.lang : 'en';
          if (dl !== S.devLang) { S.devLang = dl; setLang(dl, true); }
          updateBanner();
        } else if (r.status === 0) {
          setOffline(true);
        }
      }
      await sleep(2000);
    }
  }

  // ---------- login ----------
  // rejected = true after a 401: forget only the saved copies of the rejected password (another tab may
  // have saved a newer one after a password change); logout (rejected false) forgets both storages.
  function authLost(text, rejected) {
    S.authed = false;
    if (rejected) storeForget(S.pw); else storeSet(null);
    S.pw = null; S.prevPw = null;
    S.remember = false;
    $('login-remember').checked = false;
    showLogin(text);
  }
  function showLogin(text) {
    show($('login'), true);
    show($('app'), false);
    msg($('login-msg'), text || '', text ? 'bad' : '');
    $('login-pw').value = '';
    $('login-pw').focus();
  }
  async function tryLogin(pw, remember) {
    S.pw = pw;
    S.remember = !!remember;
    $('login-remember').checked = S.remember;
    const r = await api('GET', '/api/status', { loginTry: true });
    if (r.status === 200) {
      S.authed = true;
      storeSet(pw, S.remember);
      show($('login'), false);
      show($('app'), true);
      renderStatus(r.data);
      if (S.devLang && LANG !== S.devLang) saveLang();
      return true;
    }
    S.pw = null;
    // We clear the saved password (both storages) only when the device rejected it; with no connection,
    // a lockout or the device starting up it stays (retrying the login is enough).
    if (r.status === 401) storeForget(pw);
    showLogin(r.status === 401 ? t('err.auth') : errText(r));
    if (r.status !== 401 && storeGet()) $('login-pw').value = pw;
    return false;
  }

  // ---------- tabs ----------
  function selectTab(name) {
    S.tab = name;
    document.querySelectorAll('.tabpane').forEach(function (p) { show(p, p.id === 'tab-' + name); });
    document.querySelectorAll('.tabbar button').forEach(function (b) {
      b.classList.toggle('on', b.dataset.tab === name);
      b.setAttribute('aria-selected', b.dataset.tab === name ? 'true' : 'false');
    });
    if (name === 'system' && !S.otaLoaded) loadOtaStatus();
    if (name === 'konsola') { const v = $('log-view'); v.scrollTop = v.scrollHeight; }
  }

  // ---------- label / value pairs ----------
  // Labels are dictionary keys (data-i18n on the dt, refreshed by applyI18n); values are inserted via setText.
  function kvGrid(container, defs) {
    const out = {};
    defs.forEach(function (d) {
      const dt = el('dt', null, t(d[1]));
      dt.setAttribute('data-i18n', d[1]);
      container.appendChild(dt);
      const v = el('dd');
      v.textContent = '—';
      container.appendChild(v);
      out[d[0]] = v;
    });
    return out;
  }

  // ---------- /api/status ----------
  let K = {};   // value cells
  function initGrids() {
    K.car = kvGrid($('kv-car'), [['bat', 'car.kv.battery'], ['temp', 'car.kv.outside_temp'], ['doors', 'car.kv.doors'], ['ign', IGN.label], ['set', PROFILE.settings_label || 'link.kv.settings']]);
    K.park = kvGrid($('kv-park'), [['front', 'park.kv.front'], ['rear', 'park.kv.rear'], ['snd', 'park.kv.sound'], ['int', 'park.kv.interval'], ['beep', 'park.kv.beeper']]);
    K.radioDash = kvGrid($('kv-radio-dash'), [['on', 'radio.kv.radio'], ['vol', 'radio.kv.volume'], ['src', 'radio.kv.source'], ['time', 'radio.kv.time']]);
    K.can = kvGrid($('kv-can'), [['fps', 'can.kv.fps'], ['rx', 'can.kv.rx'], ['missed', 'can.kv.missed'], ['overrun', 'can.kv.overrun'], ['err', 'can.kv.bus_errors'], ['alive', 'can.kv.alive'], ['tx', 'can.kv.tx']]);
    K.radio = kvGrid($('kv-radio'), [['ok', 'radio.kv.uart'], ['tx', 'radio.kv.tx'], ['heard', 'radio.kv.heard'], ['age', 'radio.kv.age'], ['on', 'radio.kv.on'], ['vol', 'radio.kv.volume'], ['muted', 'radio.kv.muted'], ['src', 'radio.kv.source'], ['srcraw', 'radio.kv.source_raw'], ['time', 'radio.kv.time']]);
    K.link = kvGrid($('kv-link'), [['rxFrames', 'link.kv.rx_frames'], ['rxAcks', 'link.kv.rx_acks'], ['rxBadChecksum', 'link.kv.rx_bad_checksum'], ['rxBadLength', 'link.kv.rx_bad_length'], ['rxTimeouts', 'link.kv.rx_timeouts'], ['rxStray', 'link.kv.rx_stray'], ['txFrames', 'link.kv.tx_frames'], ['txAcks', 'link.kv.tx_acks'], ['txSuppressed', 'link.kv.tx_suppressed'], ['txQueue', 'link.kv.tx_queue'], ['txDropped', 'link.kv.tx_dropped'], ['keysDropped', 'link.kv.keys_dropped'], ['txGated', 'link.kv.tx_gated'], ['slotsFull', 'link.kv.slots_full'], ['settings', 'link.kv.settings'], ['air', 'link.kv.air'], ['unknownCount', 'link.kv.unknown']]);
    K.sys = kvGrid($('kv-sys'), [['version', 'sys.kv.version'], ['build', 'sys.kv.build'], ['elf', 'sys.kv.elf'], ['mode', 'sys.kv.mode'], ['uptime', 'sys.kv.uptime'], ['reset', 'sys.kv.reset'], ['ota', 'sys.kv.ota'], ['temp', 'sys.kv.temp'], ['audio', 'sys.kv.audio']]);
    K.load = kvGrid($('kv-load'), [['loop', 'load.kv.loop'], ['loopMax', 'load.kv.loop_max'], ['loops', 'load.kv.loops'], ['cpu0', 'load.kv.cpu0'], ['heap', 'load.kv.heap'], ['heapMin', 'load.kv.heap_min'], ['heapLargest', 'load.kv.heap_largest'], ['stacks', 'load.kv.stacks'], ['hubDrops', 'load.kv.hub_drops']]);
    K.wifi = kvGrid($('kv-wifi'), [['phase', 'wifi.kv.phase'], ['ap', 'wifi.kv.ap'], ['apIp', 'wifi.kv.ap_ip'], ['chan', 'wifi.kv.channel'], ['stations', 'wifi.kv.stations'], ['sta', 'wifi.kv.sta'], ['staState', 'wifi.kv.sta_state'], ['staIp', 'wifi.kv.sta_ip'], ['rssi', 'wifi.kv.rssi'], ['reason', 'wifi.kv.reason'], ['host', 'wifi.kv.host'], ['step', 'wifi.kv.step'], ['fails', 'wifi.kv.fails'], ['lan', 'wifi.kv.lan']]);
  }

  function decodeSource(src) {
    if (!Array.isArray(src) || src.length < 2) return null;
    const a = src[0], b = src[1];
    if (a === 0x01 && b === 0x01) return t('radio.src.radio');
    if (a === 0x08 && b === 0x13) {
      return t('radio.src.player', { track: src[2], total: src[3], time: pad(src[4], 2) + ':' + pad(src[5], 2) });
    }
    if (a === 0x0C && b === 0x30) return t('radio.src.other');
    return t('radio.src.unknown', { code: hex2(a) + ' ' + hex2(b) });
  }
  // Words from the API (PANEL_API.md) -> dictionary keys; an unknown word stays unchanged (tw()).
  const RESET_WORDS = { unknown: 'sys.reset.unknown', poweron: 'sys.reset.poweron', ext: 'sys.reset.ext', sw: 'sys.reset.sw',
    panic: 'sys.reset.panic', int_wdt: 'sys.reset.int_wdt', task_wdt: 'sys.reset.task_wdt', wdt: 'sys.reset.wdt',
    deepsleep: 'sys.reset.deepsleep', brownout: 'sys.reset.brownout', sdio: 'sys.reset.sdio', usb: 'sys.reset.usb',
    jtag: 'sys.reset.jtag', efuse: 'sys.reset.efuse', pwr_glitch: 'sys.reset.pwr_glitch', cpu_lockup: 'sys.reset.cpu_lockup' };
  const OTA_WORDS = { none: 'ota.state.none', valid: 'ota.state.valid', pending: 'ota.state.pending' };
  // Operating mode of /api/status, /api/ota and the firmware marker: the same names as the header badge.
  const MODE_WORDS = { listen: 'app.mode_name.listen', decoder: 'app.mode_name.decoder' };
  function modeWord(m) { return tw(MODE_WORDS, m); }
  // Field of a refused settings import (detail of bad_request, POST /api/settings) -> hint what to fix.
  const IMPORT_WORDS = { json: 'sys.backup.err.json', syntax: 'sys.backup.err.syntax', version: 'sys.backup.err.version',
    unknown_key: 'sys.backup.err.unknown_key', duplicate: 'sys.backup.err.duplicate', bad_value: 'sys.backup.err.bad_value',
    lang: 'sys.backup.err.lang', vol: 'sys.backup.err.vol', keys: 'sys.backup.err.keys' };
  const PHASE_WORDS = { off: 'wifi.phase.off', starting: 'wifi.phase.starting', running: 'wifi.phase.running',
    stopping: 'wifi.phase.stopping', failed: 'wifi.phase.failed' };
  // CAN transmit gate: state and fault codes of /api/status (can.tx.state, can.tx.fault) -> keys.
  const TX_STATE_WORDS = { none: 'cantx.state.none', disarmed: 'cantx.state.disarmed', ready: 'cantx.state.ready',
    opening: 'cantx.state.opening', sending: 'cantx.state.sending', cooldown: 'cantx.state.cooldown', fault: 'cantx.state.fault' };
  const TX_FAULT_WORDS = { none: 'cantx.fault.none', '3 failed sends': 'cantx.fault.failed_sends',
    'transmit error counter': 'cantx.fault.tec', 'bus-off': 'cantx.fault.bus_off',
    'transmitter window exceeded': 'cantx.fault.window', 'controller event': 'cantx.fault.controller',
    'foreign bits of 260 changed': 'cantx.fault.foreign_bits', 'external lock': 'cantx.fault.external' };
  const SET_RESULT = { refused: 'set.result.refused', no_frame: 'set.result.no_frame', blocked: 'set.result.blocked',
    unchanged: 'set.result.unchanged', accepted: 'set.result.accepted', not_accepted: 'set.result.not_accepted',
    tx_failed: 'set.result.tx_failed', side_effect: 'set.result.side_effect' };
  function reasonText(r) {
    if (r === 0) return t('wifi.reason.none');
    if (r === 201) return t('wifi.reason.not_found', { code: r });
    if (r === 2 || r === 15 || r === 202 || r === 204) return t('wifi.reason.auth', { code: r });
    return t('wifi.reason.code', { code: r });
  }
  // Row "Vehicle settings": last frame 260 from the car, state sent to the radio in frame 38 (DATA[1..5])
  // and the last radio command 80. Read only; fields since version 0.8.0 (older firmware does not report them).
  function hexSpaced(h) { return h.replace(/(..)(?=.)/g, '$1 '); }
  function settingsText(st) {
    if (!st) return t('set.no_data');
    const parts = [(st.raw260 === null ? t('set.no_260') : '260: ' + hexSpaced(st.raw260) + ' (' + fmtAge(st.ageMs) + ')') +
      ' \u2192 38: ' + hexSpaced(st.state38)];
    if (st.last) {
      parts.push(t('set.last', { cmd: '80 ' + hex2(st.last.id) + ' ' + hex2(st.last.value), result: tw(SET_RESULT, st.last.result),
        age: fmtAge(st.last.ageMs) }));
    } else {
      parts.push(t('set.no_cmds'));
    }
    if (st.refused || st.accepted || st.notAccepted) parts.push(t('set.counts', { refused: st.refused, accepted: st.accepted, notAccepted: st.notAccepted }));
    return parts.join('; ');
  }
  // Row "CAN transmission": state of the transmit gate (field can.tx since version 0.9.0). Firmware without
  // transmit hardware reports built = false. Arming switch: renderCanTxBox below.
  function canTxText(tx) {
    if (!tx) return t('cantx.no_data');
    if (!tx.built) return t('cantx.not_built');
    let m = t('cantx.summary', { arm: t(tx.armed ? 'cantx.armed' : 'cantx.disarmed'), state: tw(TX_STATE_WORDS, tx.state) });
    if (typeof tx.armNext === 'boolean' && tx.armNext !== tx.armed) m += ' ' + t('cantx.after_restart', { arm: t(tx.armNext ? 'cantx.armed' : 'cantx.disarmed') });
    if (tx.fault) m += ', ' + t('cantx.fault_label', { fault: tw(TX_FAULT_WORDS, tx.fault) });
    m += '; ' + t('cantx.counters', { sent: tx.sent, acked: tx.acked, failed: tx.failed, refused: tx.refused, tec: tx.tec, rec: tx.rec });
    return m;
  }
  // Arming switch for 15B transmission: only in the transmit-capable build (can.tx.built). The device
  // accepts "cantx on" / "cantx off" from the panel only from a client in the device's own network
  // (sys.clientAp); via the home network it answers 403 ap_only. Arming takes effect after a restart.
  function renderCanTxBox(s) {
    const tx = s.can ? s.can.tx : null;
    const box = $('cantx-box');
    if (!tx || !tx.built) { show(box, false); return; }
    show(box, true);
    const viaAp = !!(s.sys && s.sys.clientAp);
    const next = typeof tx.armNext === 'boolean' ? tx.armNext : tx.armed;
    $('btn-cantx-on').disabled = !viaAp || next;
    $('btn-cantx-off').disabled = !viaAp || (!next && !tx.armed);
    const parts = [t('cantx.box.now', { state: t(tx.armed ? 'cantx.box.armed_long' : 'cantx.box.disarmed_long') }),
      t('cantx.box.next', { arm: t(next ? 'cantx.armed' : 'cantx.disarmed') }),
      tx.fault ? t('cantx.box.gate_fault', { state: tw(TX_STATE_WORDS, tx.state), fault: tw(TX_FAULT_WORDS, tx.fault) })
        : t('cantx.box.gate', { state: tw(TX_STATE_WORDS, tx.state) })];
    if (!viaAp) parts.push(t('cantx.box.ap_only'));
    if (s.car && s.car.settings && typeof s.car.settings.mask === 'number') {
      const ids = [];
      for (let i = 0; i < 32; i++) if (s.car.settings.mask & (1 << i)) ids.push((i < 16 ? '0' : '') + i.toString(16).toUpperCase());
      parts.push(t('cantx.box.positions', { list: ids.length ? ids.join(', ') : t('cantx.box.none') }));
    }
    setText($('cantx-info'), parts.join(' '));
  }
  async function canTxArm(on) {
    if (on) {
      if (!await ask(t('cantx.ask.title'), [t('cantx.ask.p1'), t('cantx.ask.p2'), t('cantx.ask.p3'), t('cantx.ask.p4')],
        t('cantx.ask.ok'), true)) return;
    }
    if (await sendCmd(on ? 'cantx on' : 'cantx off')) {
      toast(t(on ? 'cantx.toast_armed' : 'cantx.toast_disarmed'));
    }
  }
  function doorsText(d) {
    if (typeof d !== 'number') return t('common.no_frame');   // null: no door frame has arrived since start
    const names = [];
    DOORS.forEach(function (x) { if (d & parseInt(x[0], 16)) names.push(t(x[1])); });
    return names.length ? names.join(', ') : t('car.doors_all_closed');
  }
  function chip(id, on, label) {
    const c = $(id);
    c.classList.toggle('on', !!on);
    if (label !== undefined) setText(c, label);
  }

  function renderStatus(s) {
    if (!s || !s.sys) return;
    S.status = s;
    if (typeof s.sys.boot === 'number') checkBoot(s.sys.boot);
    const sys = s.sys, can = s.can, car = s.car, park = s.park, radio = s.radio, wifi = s.wifi, audio = s.audio;

    setText($('badge-ver'), 'v' + sys.version);
    setText($('badge-mode'), t(sys.mode === 'decoder' ? 'app.mode.decoder' : 'app.mode.listen'));
    $('badge-mode').classList.toggle('warn', sys.mode === 'decoder');
    const txOn = can.tx && can.tx.armed;   // armed transmit build: no "listen only"
    setText($('badge-can'), txOn ? 'CAN: ' + t('cantx.armed') : t('app.listen_badge'));
    $('badge-can').className = 'badge ' + (txOn ? 'warn' : 'listen');

    // Dashboard
    // ignitionRaw = null: no fresh 0F6 frame (a stale byte is not a reading).
    chip('chip-ign', car.ignition, t(car.ignition && car.ignitionRaw === 142 ? 'car.chip.engine' : 'car.chip.ign'));
    chip('chip-acc', car.acc);
    chip('chip-rev', car.reverse);
    chip('chip-lights', car.lights);
    setText(K.car.bat, car.batteryV === null ? t('car.no_12v') : dec(car.batteryV, 2) + ' V');
    setText(K.car.temp, car.outsideTempC === null ? t('common.no_frame') : dec(car.outsideTempC, 1) + ' \u00B0C');
    setText(K.car.doors, doorsText(car.doors));
    setText(K.car.ign, car.ignitionRaw === null ? t('common.no_frame') : hex2(car.ignitionRaw) +
      (IGN.values && has.call(IGN.values, String(car.ignitionRaw)) ? ' ' + t(IGN.values[String(car.ignitionRaw)]) : ''));
    setText(K.car.set, settingsText(car.settings));
    renderCar(park, car.doors);
    setText(K.park.front, t('park.side', { status: tw(PARK_STATUS, park.frontStatus), zones: park.front.join(' ') }));
    setText(K.park.rear, t('park.side', { status: tw(PARK_STATUS, park.rearStatus), zones: park.rear.join(' ') }));
    setText(K.park.snd, park.sound ? t(park.soundFront ? 'park.sound_front' : 'park.sound_rear') : t('common.no'));
    setText(K.park.int, park.interval === 0 ? t('park.interval_cont') : park.interval === 63 ? t('park.interval_none') : String(park.interval));
    setText(K.park.beep, t(park.continuous ? 'park.beep.continuous' : park.beeping ? 'park.beep.beeping' : 'park.beep.silent'));
    $('park-stale').hidden = !!park.fresh;
    // Volume slider: while it is being moved (and until the response) we do not overwrite it with the device state.
    const vr = $('vol-range');
    if (!volDrag && audio.ok) {
      if (String(vr.max) !== String(audio.volumeMax)) vr.max = audio.volumeMax;
      if (String(vr.value) !== String(audio.volume)) vr.value = audio.volume;
      setText($('vol-val'), audio.volume + ' %');
    }
    vr.disabled = !audio.ok;
    setText(K.radioDash.on, !radio.heard ? t('radio.not_heard') : t(radio.on ? 'common.on' : 'common.off'));
    // The radio has not responded: the fields hold default values (0), not a reading.
    setText(K.radioDash.vol, radio.heard ? (radio.muted ? t('radio.vol_muted', { v: radio.volume }) : radio.volume) : null);
    setText(K.radioDash.src, radio.heard ? decodeSource(radio.source) : null);
    setText(K.radioDash.time, radio.time);
    setText(K.can.fps, can.framesPerSec);
    setText(K.can.rx, num(can.rx));
    setText(K.can.missed, num(can.missed));
    setText(K.can.overrun, num(can.overrun));
    setText(K.can.err, num(can.busErrors));
    setText(K.can.alive, can.busAlive ? t('common.yes') : t('can.silent'));
    setText(K.can.tx, canTxText(can.tx));
    renderCanTxBox(s);
    K.can.tx.classList.toggle('bad', !!(can.tx && can.tx.fault));
    K.can.missed.classList.toggle('bad', can.missed > 0);
    K.can.overrun.classList.toggle('bad', can.overrun > 0);
    $('can-state').className = 'dot ' + (can.ok && can.busAlive ? 'ok' : can.ok ? 'idle' : 'bad');

    // Radio
    setText(K.radio.ok, t(radio.ok ? 'radio.uart_ok' : 'radio.uart_fail'));
    setText(K.radio.tx, t(radio.txEnabled ? 'radio.tx_on' : 'radio.tx_off'));
    setText(K.radio.heard, yesNo(radio.heard));
    setText(K.radio.age, radio.ageMs === null ? null : fmtAge(radio.ageMs));
    setText(K.radio.on, radio.heard ? yesNo(radio.on) : null);
    setText(K.radio.vol, radio.heard ? radio.volume : null);
    setText(K.radio.muted, radio.heard ? yesNo(radio.muted) : null);
    setText(K.radio.src, radio.heard ? decodeSource(radio.source) : null);
    setText(K.radio.srcraw, radio.heard && Array.isArray(radio.source) ? radio.source.map(hex2).join(' ') : null);
    setText(K.radio.time, radio.time);
    ['rxFrames', 'rxAcks', 'rxBadChecksum', 'rxBadLength', 'rxTimeouts', 'rxStray', 'txFrames', 'txAcks',
      'txSuppressed', 'txDropped', 'keysDropped', 'txGated', 'slotsFull', 'unknownCount'].forEach(function (k) { setText(K.link[k], num(radio[k])); });
    setText(K.link.txQueue, radio.txQueue + ' / ' + radio.txQueueCap);
    setText(K.link.settings, radio.settingCount ? t('link.last', { n: radio.settingCount, v: radio.lastSetting.join(' = ') }) : String(radio.settingCount));
    setText(K.link.air, radio.airCount ? t('link.last', { n: radio.airCount, v: radio.lastAir.join(' = ') }) : String(radio.airCount));
    ['rxBadChecksum', 'txDropped', 'keysDropped', 'slotsFull'].forEach(function (k) { K.link[k].classList.toggle('bad', radio[k] > 0); });
    $('key-mode-hint').hidden = radio.txEnabled;

    // System
    setText(K.sys.version, sys.version);
    setText(K.sys.build, sys.build);
    setText(K.sys.elf, sys.elf);
    setText(K.sys.mode, modeWord(sys.mode));
    setText(K.sys.uptime, duration(sys.uptimeMs));
    setText(K.sys.reset, tw(RESET_WORDS, sys.resetReason));
    setText(K.sys.ota, sys.restartPending ? t('sys.ota_restart', { state: tw(OTA_WORDS, sys.otaState) }) : tw(OTA_WORDS, sys.otaState));
    setText(K.sys.temp, sys.tempC === null ? null : dec(sys.tempC, 1) + ' \u00B0C');
    setText(K.sys.audio, audio.ok ? t('sys.audio_ok', { v: audio.volume, max: audio.volumeMax }) : t('sys.audio_fail'));
    setText(K.load.loop, sys.loopBusyPct + ' %');
    setText(K.load.loopMax, num(sys.loopMaxUs) + ' \u00B5s');
    setText(K.load.loops, num(sys.loopsPerSec));
    setText(K.load.cpu0, sys.cpu0Pct + ' %');
    setText(K.load.heap, num(sys.heapFree) + ' B');
    setText(K.load.heapMin, num(sys.heapMin) + ' B');
    setText(K.load.heapLargest, num(sys.heapLargest) + ' B');
    setText(K.load.stacks, sys.stackLoopFree + ' / ' + sys.stackHttpFree + ' / ' + sys.stackWifiFree + ' B');
    setText(K.load.hubDrops, num(sys.hubDrops));
    const ph = [tw(PHASE_WORDS, wifi.phase)];
    if (wifi.gaveUp) ph.push(t('wifi.gave_up'));
    if (wifi.blocked) ph.push(t('wifi.blocked'));
    setText(K.wifi.phase, ph.join(', '));
    setText(K.wifi.ap, wifi.apSsid);
    setText(K.wifi.apIp, wifi.apIp);
    setText(K.wifi.chan, wifi.apChannel || t('wifi.chan_unknown'));
    setText(K.wifi.stations, wifi.apStations);
    setText(K.wifi.sta, wifi.staConfigured ? wifi.staSsid : t('wifi.sta_none'));
    setText(K.wifi.staState, t(wifi.staConnected ? 'wifi.connected' : 'wifi.not_connected'));
    setText(K.wifi.staIp, wifi.staIp);
    setText(K.wifi.rssi, wifi.staRssi === null ? null : wifi.staRssi + ' dBm');
    setText(K.wifi.reason, reasonText(wifi.staLastReason));
    setText(K.wifi.host, wifi.hostname + '.local');
    setText(K.wifi.step, wifi.maxStepMs + ' ms');
    setText(K.wifi.fails, wifi.startFails);
    setText(K.wifi.lan, t(wifi.lanAdmin ? 'wifi.lan_on' : 'wifi.lan_off'));
  }

  // Top view of the car: sensor zones (0 = very close, 7 = no obstacle) and open doors.
  function zoneClass(z, active) {
    if (!active) return 'zone off';
    if (z >= 7) return 'zone z7';
    if (z >= 5) return 'zone z5';
    if (z >= 3) return 'zone z3';
    if (z >= 1) return 'zone z1';
    return 'zone z0';
  }
  function renderCar(park, doors) {
    const fa = park.fresh && park.frontStatus === 4, ra = park.fresh && park.rearStatus === 4;
    for (let i = 0; i < 3; i++) {
      $('zf' + i).setAttribute('class', zoneClass(park.front[i], fa));
      $('zr' + i).setAttribute('class', zoneClass(park.rear[i], ra));
    }
    DOORS.forEach(function (d) {
      const n = $('d' + d[0]);      // door rectangles of the top view: id "d" + bit in hex (index.html)
      if (n) n.setAttribute('class', 'door' + ((doors & parseInt(d[0], 16)) ? ' open' : ''));
    });
  }

  async function pollStatus() {
    const r = await api('GET', '/api/status');
    if (r.status === 200) renderStatus(r.data);
    return false;
  }

  // ---------- console: /api/log ----------
  const LOG_KEEP = 1500;
  const logLines = [];   // [time ms | null, text]
  function addLogLines(lines, local) {
    const view = $('log-view');
    const atEnd = view.scrollTop + view.clientHeight >= view.scrollHeight - 8;
    const f = $('log-filter').value.toLowerCase();
    lines.forEach(function (l) {
      logLines.push(l);
      if (!f || String(l[1]).toLowerCase().indexOf(f) >= 0) view.appendChild(logRow(l, local));
    });
    while (logLines.length > LOG_KEEP) logLines.shift();
    while (view.childNodes.length > LOG_KEEP) view.removeChild(view.firstChild);
    if (atEnd) view.scrollTop = view.scrollHeight;
  }
  function logRow(l, local) {
    const row = el('div', 'logline' + (local ? ' local' : ''));
    row.appendChild(el('span', 'ts', l[0] === null ? '' : devTime(l[0])));
    row.appendChild(el('span', null, l[1]));
    return row;
  }
  function rerenderLog() {
    const view = $('log-view');
    clear(view);
    const f = $('log-filter').value.toLowerCase();
    logLines.forEach(function (l) {
      if (!f || String(l[1]).toLowerCase().indexOf(f) >= 0) view.appendChild(logRow(l, l[0] === null));
    });
    view.scrollTop = view.scrollHeight;
  }
  async function pollLog() {
    const q = S.logCur === null ? '' : '?c=' + S.logCur;
    const b0 = S.boot;
    const r = await api('GET', '/api/log' + q);
    if (r.status !== 200 || !r.data) return false;
    const d = r.data;
    // A response with a new boot number to a cursor from the old one: the cursor is invalid, we ask from the beginning.
    if (q && b0 !== null && d.boot !== b0) { S.logCur = null; return true; }
    if (d.dropped > 0) addLogLines([[null, tp('log.dropped', d.dropped)]], true);
    if (Array.isArray(d.lines) && d.lines.length) addLogLines(d.lines, false);
    S.logCur = d.next;
    return d.next < d.head;
  }

  // Beep volume: the "vol N" command only after the slider is released (change event), because every
  // command from 15 % upwards is a write to the device's flash memory.
  let volDrag = false;
  async function sendVolume() {
    const v = parseInt($('vol-range').value, 10);
    if (isNaN(v)) { volDrag = false; return; }
    await sendCmd('vol ' + v);
    if (v < 15) toast(t('park.vol_low', { v: v }));
    // The device state controls the slider again after the next state snapshot (every 200 ms).
    setTimeout(function () { volDrag = false; }, 600);
  }

  const hist = [];
  let histPos = 0;
  async function sendCmd(line) {
    line = line.trim();
    if (!line) return;
    if (!/^[\x20-\x7E]{1,127}$/.test(line)) { toast(t('console.bad_cmd'), true); return; }
    if (/^wifi(\s|$)/i.test(line)) { toast(t('console.no_wifi'), true); return; }
    if (hist[hist.length - 1] !== line) hist.push(line);
    if (hist.length > 50) hist.shift();
    histPos = hist.length;
    const r = await api('POST', '/api/cmd', { form: { line: line } });
    if (r.status === 200) return true;
    toast(t('console.cmd_error', { cmd: line, err: errText(r) }), true);
    return false;
  }

  // ---------- CAN: /api/frames and /api/table ----------
  const TRACE_KEEP = 300;
  const trace = [];            // latest frames for the preview: [ms, id, data]
  const rec = { on: false, lines: [], frames: 0, started: 0, header: '' };
  let canPaused = false;
  function masked(id) { return $('mask-vin').checked && VIN_IDS[id] === 1; }
  function canFilter() { return $('can-filter').value.trim().toUpperCase(); }
  function traceRow(f) {
    const row = el('div', 'trow');
    row.appendChild(el('span', 'ts', devTime(f[0])));
    row.appendChild(el('span', 'id', f[1]));
    row.appendChild(el('span', 'data', masked(f[1]) ? '\u2022\u2022' : f[2].replace(/(..)(?!$)/g, '$1 ')));
    return row;
  }
  function rerenderTrace() {
    const v = $('trace-view');
    clear(v);
    const flt = canFilter();
    trace.forEach(function (f) { if (!flt || f[1] === flt) v.appendChild(traceRow(f)); });
    v.scrollTop = v.scrollHeight;
  }
  async function pollFrames() {
    const hadCur = S.frCur !== null;
    const q = '?max=400' + (hadCur ? '&c=' + S.frCur : '');
    const b0 = S.boot;
    const r = await api('GET', '/api/frames' + q);
    if (r.status !== 200 || !r.data) return false;
    const d = r.data;
    if (hadCur && b0 !== null && d.boot !== b0) { S.frCur = null; return true; }
    if (rec.on && d.dropped > 0) rec.lines.push('# lost ' + d.dropped + ' frames');   // same comment as /api/trace.log
    const v = $('trace-view');
    const atEnd = v.scrollTop + v.clientHeight >= v.scrollHeight - 8;
    const flt = canFilter();
    (d.frames || []).forEach(function (f) {
      if (rec.on) { rec.lines.push('(' + devTime(f[0]) + ') can0 ' + f[1] + '#' + f[2]); rec.frames++; }
      trace.push(f);
      if (!canPaused && (!flt || f[1] === flt)) v.appendChild(traceRow(f));
    });
    while (trace.length > TRACE_KEEP) trace.shift();
    while (v.childNodes.length > TRACE_KEEP) v.removeChild(v.firstChild);
    if (atEnd && !canPaused) v.scrollTop = v.scrollHeight;
    S.frCur = d.next;
    updateRec();
    return d.next < d.head;
  }

  const tableRows = {};       // id -> {row, cells, data, changed[]}
  async function pollTable() {
    if (canPaused) return false;
    const r = await api('GET', '/api/table');
    if (r.status !== 200 || !r.data) return false;
    const d = r.data, body = $('can-table'), now = Date.now(), flt = canFilter();
    const seen = {};
    (d.ids || []).forEach(function (e, idx) {
      const id = e[0];
      seen[id] = 1;
      let tr = tableRows[id];
      if (!tr) {
        const row = el('tr');
        const name = CAN_NAMES[id];
        const cells = [el('td', 'id', id), el('td', 'name', name ? name[0] : ''), el('td', 'num'), el('td', 'num'), el('td', 'num'), el('td', 'data')];
        if (name) cells[1].title = t(name[1]);
        cells.forEach(function (c) { row.appendChild(c); });
        row.addEventListener('click', function () { setCanFilter(canFilter() === id ? '' : id); });
        tr = tableRows[id] = { row: row, cells: cells, data: '', changed: [] };
      }
      // rows in the order from the device (ascending by ID)
      if (body.childNodes[idx] !== tr.row) body.insertBefore(tr.row, body.childNodes[idx] || null);
      tr.row.classList.toggle('sel', flt === id);
      setText(tr.cells[2], e[2] ? e[2] + ' ms' : '—');
      setText(tr.cells[3], num(e[1]));
      setText(tr.cells[4], e[3] + ' ms');
      tr.cells[4].classList.toggle('dim', e[3] > 2000);
      renderBytes(tr, e[4], masked(id), now);
    });
    Object.keys(tableRows).forEach(function (id) {
      if (!seen[id]) { tableRows[id].row.remove(); delete tableRows[id]; }
    });
    setText($('can-overflow'), d.overflow ? t('can.overflow', { n: d.overflow }) : '');
    return false;
  }
  // Bytes with change highlighting (1.5 s from the change). For VIN frames with masking: dots only.
  function renderBytes(tr, data, isMasked, now) {
    const cell = tr.cells[5];
    if (isMasked) {
      if (tr.maskedShown) return;
      clear(cell); cell.appendChild(el('span', 'byte', '\u2022\u2022'));
      tr.maskedShown = true; tr.data = ''; tr.changed = [];
      return;
    }
    const n = data.length / 2;
    const old = tr.data;
    for (let i = 0; i < n; i++) {
      const b = data.substr(i * 2, 2);
      if (old && old.substr(i * 2, 2) !== b) tr.changed[i] = now;
    }
    tr.data = data;
    if (tr.maskedShown || cell.childNodes.length !== n) {
      clear(cell);
      for (let i = 0; i < n; i++) cell.appendChild(el('span', 'byte'));
      tr.maskedShown = false;
    }
    for (let i = 0; i < n; i++) {
      const s = cell.childNodes[i];
      setText(s, data.substr(i * 2, 2));
      s.className = 'byte' + (tr.changed[i] && now - tr.changed[i] < 1500 ? ' chg' : '');
    }
  }
  function setCanFilter(id) {
    $('can-filter').value = id;
    Object.keys(tableRows).forEach(function (k) { tableRows[k].row.classList.toggle('sel', k === id); });
    rerenderTrace();
  }
  function updateRec() {
    const b = $('btn-rec');
    b.textContent = t(rec.on ? 'can.rec.stop' : 'can.rec.start');
    b.classList.toggle('danger', rec.on);
    setText($('rec-info'), rec.on ? t('can.rec.status', { frames: tp('can.rec.frames', rec.frames), t: duration(Date.now() - rec.started) })
      : rec.lines.length ? tp('can.rec.frames', rec.frames) : (rec.started ? t('can.rec.empty') : ''));
    $('btn-rec-save').disabled = rec.lines.length === 0;
  }
  function recHeader() {
    const s = S.status;
    // the same header comment as /api/trace.log (file format, not translated)
    return '# ' + PRODUCT + ' ' + (s ? s.sys.version + ' ' + s.sys.mode : '') + '; time in seconds since the device started';
  }
  function saveRecording() {
    const mask = $('mask-vin').checked;
    const out = [rec.header];
    rec.lines.forEach(function (l) {
      if (mask && l.charAt(0) === '(') {
        const p = l.indexOf(' can0 ');
        const id = l.substr(p + 6, 3);
        if (VIN_IDS[id] === 1) { out.push('# masked'); return; }
      }
      out.push(l);
    });
    saveFile(IDENT.slug + '_can_' + stamp() + '.log', new Blob([out.join('\n') + '\n'], { type: 'text/plain' }));
  }
  async function downloadDeviceTrace() {
    const mask = $('mask-vin').checked;
    const r = await api('GET', '/api/trace.log' + (mask ? '' : '?mask=0'), { blob: true, timeout: 30000 });
    if (r.status === 200 && r.blob) saveFile(IDENT.slug + '_can_' + stamp() + '.log', r.blob);
    else toast(t('can.dl_error', { err: errText(r) }), true);
  }

  // ---------- keys: /api/keymap ----------
  function keyName(list, code) {
    for (let i = 0; i < list.length; i++) if (list[i][0] === code) return t(list[i][1]);
    return null;
  }
  // Code picker: a list of known codes + a field for any hex code. Texts are set by relabel() (also after
  // a language change).
  function codeSelect(list, labelKey) {
    const wrap = el('div', 'codepick');
    const sel = el('select');
    list.forEach(function (k) { const o = el('option'); o.value = k[0]; sel.appendChild(o); });
    const other = el('option'); other.value = ''; sel.appendChild(other);
    const inp = el('input', 'hex');
    inp.maxLength = 2;
    function relabel() {
      sel.setAttribute('aria-label', t(labelKey));
      list.forEach(function (k, i) { sel.options[i].textContent = k[0] + ' ' + t(k[1]); });
      other.textContent = t('keymap.other_code');
      inp.placeholder = t('keymap.code_placeholder');
      inp.setAttribute('aria-label', t('keymap.code_aria', { label: t(labelKey) }));
    }
    relabel();
    inp.autocapitalize = 'characters'; inp.spellcheck = false;
    sel.addEventListener('change', function () { if (sel.value) inp.value = sel.value; else inp.focus(); markDirty(); });
    inp.addEventListener('input', function () {
      const v = inp.value.trim().toUpperCase();
      sel.value = keyName(list, v) !== null ? v : '';
      markDirty();
    });
    wrap.appendChild(sel); wrap.appendChild(inp);
    return { wrap: wrap, relabel: relabel, set: function (code) { inp.value = code; sel.value = keyName(list, code) !== null ? code : ''; },
      get: function () { return inp.value.trim().toUpperCase(); } };
  }
  const editorRows = [];
  function addEditorRow(from, to) {
    if (editorRows.length >= 16) { toast(t('keymap.max'), true); return; }
    const row = el('div', 'maprow');
    const a = codeSelect(CAR_KEYS, 'keymap.car_key'), b = codeSelect(RADIO_FN, 'keymap.radio_fn');
    a.set(from || ''); b.set(to || '');
    const del = el('button', 'btn ghost small', t('keymap.remove'));
    del.type = 'button';
    del.setAttribute('data-i18n', 'keymap.remove');
    const entry = { row: row, a: a, b: b };
    del.addEventListener('click', function () {
      row.remove();
      editorRows.splice(editorRows.indexOf(entry), 1);
      markDirty();
    });
    row.appendChild(a.wrap); row.appendChild(el('span', 'arrow', '\u2192')); row.appendChild(b.wrap); row.appendChild(del);
    $('map-rows').appendChild(row);
    editorRows.push(entry);
  }
  function loadEditor(map) {
    clear($('map-rows'));
    editorRows.length = 0;
    map.forEach(function (p) { addEditorRow(p[0], p[1]); });
    S.editorLoaded = true;
    $('map-dirty').hidden = true;
  }
  function markDirty() { $('map-dirty').hidden = false; }
  function mapText(map) { return map.length ? map.map(function (p) { return p[0] + '=' + p[1]; }).join(';') : t('keymap.none'); }
  async function pollKeymap() {
    const r = await api('GET', '/api/keymap');
    if (r.status !== 200 || !r.data) return false;
    const d = r.data;
    S.keymap = d;
    setText($('map-current'), mapText(d.map));
    setText($('map-defaults'), mapText(d.defaults));
    if (!S.editorLoaded) loadEditor(d.map);
    const lk = d.lastKey;
    if (lk) {
      const n = keyName(CAR_KEYS, lk.code);
      setText($('last-key'), t('keymap.last_key', { key: lk.code + (n ? ' ' + n : ''), state: tw(KEY_STATE, lk.state),
        age: t('fmt.ago', { t: lk.ageMs < 60000 ? dec(lk.ageMs / 1000, 1) + ' s' : duration(lk.ageMs) }) }));
    } else setText($('last-key'), t('keymap.no_key'));
    $('btn-learn').disabled = !lk;
    return false;
  }
  async function saveKeymap() {
    const pairs = [], seen = {};
    for (let i = 0; i < editorRows.length; i++) {
      const z = editorRows[i].a.get(), na = editorRows[i].b.get();
      if (!/^[0-9A-F]{2}$/.test(z) || !/^[0-9A-F]{2}$/.test(na)) { toast(t('keymap.row_bad', { n: i + 1 }), true); return; }
      if (seen[z]) { toast(t('keymap.dup', { code: z }), true); return; }
      seen[z] = 1;
      if (z !== na) pairs.push(z + '=' + na);
    }
    const r = await api('POST', '/api/keymap', { form: { map: pairs.join(';') } });
    if (r.status === 200) {
      toast(t('keymap.saved'));
      S.editorLoaded = false;
      setTimeout(pollKeymap, 400);
    } else toast(t('keymap.save_error', { err: errText(r) }), true);
  }
  async function resetKeymap() {
    if (!await ask(t('keymap.builtin'), t('keymap.reset_ask'), t('keymap.reset_ok'))) return;
    const r = await api('POST', '/api/keymap', { form: { reset: '1' } });
    if (r.status === 200) { toast(t('keymap.reset_done')); S.editorLoaded = false; setTimeout(pollKeymap, 400); }
    else toast(errText(r), true);
  }
  async function sendKey(cmd) {
    const code = $('key-code').value.trim().toUpperCase();
    if (!/^[0-9A-F]{2}$/.test(code)) { toast(t('radio.keytest.bad_code'), true); return; }
    const tx = S.status && S.status.radio ? S.status.radio.txEnabled : true;
    if (await sendCmd(cmd + ' ' + code)) toast(t(tx ? 'radio.keytest.sent' : 'radio.keytest.beside', { cmd: cmd + ' ' + code }), !tx);
  }

  // ---------- Wi-Fi and password ----------
  async function saveSta(forget) {
    const out = $('sta-msg');
    let form;
    if (forget) {
      if (!await ask(t('sta.title'), t('sta.forget_ask'), t('sta.forget_ok'))) return;
      form = { ssid: '' };
    } else {
      const ssid = $('sta-ssid').value, pass = $('sta-pass').value;
      const len = new TextEncoder().encode(ssid).length;
      if (len < 1 || len > 32 || /[\x00-\x1F\x7F]/.test(ssid)) { msg(out, t('sta.err_ssid'), 'bad'); return; }
      if (!/^[\x20-\x7E]{8,64}$/.test(pass)) { msg(out, t('sta.err_pass'), 'bad'); return; }
      if (pass.length === 64 && !/^[0-9A-Fa-f]{64}$/.test(pass)) { msg(out, t('sta.err_pass64'), 'bad'); return; }
      if (!await ask(t('sta.title'), [t('sta.ask.p1'), t('sta.ask.p2'), t('sta.ask.p3')], t('sta.ask.ok'))) return;
      form = { ssid: ssid, pass: pass };
    }
    const r = await api('POST', '/api/wifi', { form: form });
    if (r.status === 200) {
      msg(out, t('sta.accepted', { s: Math.round((r.data.applyInMs || 1000) / 1000) }), 'ok');
      $('sta-pass').value = '';
    } else msg(out, errText(r), 'bad');
  }
  async function changePassword() {
    const out = $('pw-msg');
    const old = $('pw-old').value, nw = $('pw-new').value, nw2 = $('pw-new2').value;
    if (!/^[\x21-\x7E]{10,63}$/.test(nw)) { msg(out, t('err.weak_password'), 'bad'); return; }
    if (nw !== nw2) { msg(out, t('pw.mismatch'), 'bad'); return; }
    if (!await ask(t('pw.ask_title'), t('pw.ask_text'), t('pw.ask_ok'))) return;
    const r = await api('POST', '/api/password', { form: { old: old, new: nw } });
    if (r.status === 200) {
      // The new password right away; the old one stays until the first successful response (see api()).
      // Saved where the old one was saved (this tab only, or this device).
      S.prevPw = S.pw; S.pw = nw; storeSet(nw, S.remember);
      ['pw-old', 'pw-new', 'pw-new2'].forEach(function (id) { $(id).value = ''; });
      msg(out, t('pw.changed'), 'ok');
    } else msg(out, errText(r), 'bad');
  }

  // ---------- settings backup (GET / POST /api/settings) ----------
  // The file holds the language, the beep volume and the key table; never passwords, the home network,
  // CAN transmission or the vehicle settings mask (docs: reference/settings.md). The device checks
  // the whole import before it changes anything.
  async function exportSettings() {
    const out = $('set-msg');
    const r = await api('GET', '/api/settings');
    if (r.status !== 200 || !r.data) { msg(out, errText(r), 'bad'); return; }
    const name = IDENT.slug + '_settings_' + stamp() + '.json';
    saveFile(name, new Blob([JSON.stringify(r.data)], { type: 'application/json' }));
    msg(out, t('sys.backup.saved', { name: name }), 'ok');
  }
  async function importSettings() {
    const out = $('set-msg');
    const inp = $('set-file');
    const f = inp.files[0];
    inp.value = '';
    if (!f) return;
    let text = '', obj = null;
    try { text = (await f.text()).trim(); obj = JSON.parse(text); } catch (e) { obj = null; }
    if (!obj || typeof obj !== 'object' || Array.isArray(obj) || f.size > 4096) { msg(out, t('sys.backup.bad_file'), 'bad'); return; }
    // The device takes at most 255 characters: a file saved with indentation is sent without it.
    if (text.length > 255) text = JSON.stringify(obj);
    if (!await ask(t('sys.backup.import'), t('sys.backup.ask', { name: f.name }), t('sys.backup.ask_ok'))) return;
    msg(out, t('sys.backup.sending'), '');
    const r = await api('POST', '/api/settings', { form: { json: text } });
    if (r.status !== 200) {
      const d = r.data;
      const hint = d && d.error === 'bad_request' && typeof d.detail === 'string' && has.call(IMPORT_WORDS, d.detail);
      msg(out, hint ? t(IMPORT_WORDS[d.detail]) : errText(r), 'bad');
      return;
    }
    // language first, so the message below already comes in the language from the file
    if (has.call(obj, 'lang')) { setLang(has.call(LANGS, obj.lang) ? obj.lang : 'en', true); S.devLang = LANG; }
    msg(out, t('sys.backup.done'), 'ok');
  }

  // ---------- firmware update ----------
  // Reading the marker from the firmware.bin file (docs/PANEL_API.md, chapter 6). Untrusted texts: only
  // printable ASCII up to the first zero.
  function cstr(b, off, len) {
    let s = '';
    for (let i = 0; i < len; i++) {
      const c = b[off + i];
      if (c === 0) break;
      s += (c >= 32 && c < 127) ? String.fromCharCode(c) : '?';
    }
    return s;
  }
  function findMarker(b) {
    const first = MARKER[0];
    outer: for (let i = 0; i + 72 <= b.length; i++) {
      if (b[i] !== first) continue;
      for (let j = 1; j < 12; j++) if (b[i + j] !== MARKER[j]) continue outer;
      return {
        product: cstr(b, i + 12, 16), version: cstr(b, i + 28, 16), build: cstr(b, i + 44, 24),
        mode: b[i + 68] === 1 ? 'decoder' : 'listen', net: b[i + 69] === 1,
        canTx: b[i + 70] !== 0     // since 0.7.0: the firmware can transmit on the car's CAN (up to 0.6.0 reserved byte = 0)
      };
    }
    return null;
  }
  // Update wizard (System tab): 1 choose the file, 2 the panel checks it and asks for an explicit
  // acknowledgement of every risky difference, 3 sending, restart and result. The device checks the file
  // again on its own (product, mode_change); the acknowledgements here only decide whether we ask it for
  // consent mode=1 / force=1, never more.
  let image = null;     // {buf, ident, elf, size, acks: [{need, box}]}
  function otaReady() { return !!image && image.acks.every(function (a) { return a.box.checked; }); }
  function otaStep() {
    const ready = otaReady();
    $('ota-s1').className = 'step ' + (image ? 'done' : 'next');
    $('ota-s2').className = 'step' + (image ? (ready ? ' done' : ' next') : '');
    $('ota-s3').className = 'step' + (ready ? ' next' : '');
    $('btn-ota').disabled = !ready || S.otaBusy;
  }
  // One acknowledgement box: title, explanation, checkbox "I understand"; need = 'mode' | 'cantx' | 'nonet'.
  function ackBox(need, title, text, label) {
    const d = el('div', 'ack');
    d.appendChild(el('strong', null, title));
    d.appendChild(el('p', 'small', text));
    const l = el('label');
    const box = el('input');
    box.type = 'checkbox';
    box.addEventListener('change', otaStep);
    l.appendChild(box);
    l.appendChild(el('span', null, label));
    d.appendChild(l);
    $('ota-acks').appendChild(d);
    return { need: need, box: box };
  }
  async function onFilePicked() {
    const f = $('ota-file').files[0];
    const info = $('ota-file-info');
    image = null;
    clear(info);
    clear($('ota-acks'));
    otaLine('');
    show($('ota-s2-empty'), !f);
    otaStep();
    if (!f) return;
    const buf = new Uint8Array(await f.arrayBuffer());
    const g = el('dl', 'kv');
    function row(k, v, cls) { g.appendChild(el('dt', null, t(k))); g.appendChild(el('dd', cls || null, v)); }
    function line(txt, cls) { info.appendChild(el('div', cls || null, txt)); }
    row('ota.chk.file', f.name + ', ' + num(f.size) + ' B');
    info.appendChild(g);
    const slot = S.ota && S.ota.slotSize ? S.ota.slotSize : 3342336;
    if (buf.length < MIN_IMAGE) { line(t('ota.too_short'), 'bad'); return; }
    if (buf.length > slot) { line(t('ota.too_big', { max: num(slot) }), 'bad'); return; }
    if (buf[0] !== 0xE9 || buf[12] !== 0x09 || buf[13] !== 0x00 || buf[32] !== 0x32 || buf[33] !== 0x54 || buf[34] !== 0xCD || buf[35] !== 0xAB) {
      line(t('ota.not_app'), 'bad'); return;
    }
    const ident = findMarker(buf);
    if (!ident || ident.product !== PRODUCT) { line(t('ota.no_marker'), 'bad'); return; }
    let elf = '';
    for (let i = 0xB0; i < 0xB8; i++) elf += hex2(buf[i]);
    const cur = S.status && S.status.sys;
    row('ota.chk.product', ident.product);
    row('ota.chk.version', ident.version + (cur && cur.version ? ' (' + t('ota.chk.now', { v: cur.version }) + ')' : ''));
    row('ota.chk.build', ident.build);
    row('ota.chk.mode', modeWord(ident.mode), cur && ident.mode !== cur.mode ? 'bad' : null);
    row('ota.chk.cantx', t(ident.canTx ? 'ota.chk.cantx_yes' : 'ota.chk.cantx_no'), cur && ident.canTx !== !!cur.canTx ? 'bad' : null);
    row('ota.chk.net', yesNo(ident.net), ident.net ? null : 'bad');
    row('ota.kv.hash', elf);
    if (cur && cur.elf && cur.elf.toUpperCase() === elf) line(t('ota.same'), 'warn');
    image = { buf: buf, ident: ident, elf: elf, size: buf.length, acks: [] };
    // Each risky difference: its own explicit acknowledgement (the device requires consent mode=1 for a
    // different operating mode or a different ability to transmit on CAN, and force=1 without the network panel).
    if (cur && ident.mode !== cur.mode) {
      image.acks.push(ackBox('mode', t('ota.mode.title', { from: modeWord(cur.mode), to: modeWord(ident.mode) }),
        t(ident.mode === 'decoder' ? 'ota.mode.to_decoder' : 'ota.mode.to_listen'), t('ota.ack.mode')));
    }
    if (cur && ident.canTx !== !!cur.canTx) {
      image.acks.push(ackBox('cantx', t(ident.canTx ? 'ota.cantx.title_yes' : 'ota.cantx.title_no'),
        t(ident.canTx ? 'ota.cantx.text_yes' : 'ota.cantx.text_no'), t('ota.ack.cantx')));
    }
    if (!ident.net) image.acks.push(ackBox('nonet', t('ota.nonet.title'), t('ota.no_net_warn'), t('ota.ack.nonet')));
    line(t(image.acks.length ? 'ota.chk.ack' : 'ota.chk.ok'), image.acks.length ? 'warn' : 'ok');
    otaStep();
  }

  // A failure also shows the general "what to do" hint under the message.
  function otaLine(text, kind) { msg($('ota-msg'), text, kind); show($('ota-help'), kind === 'bad'); }
  function progress(frac) {
    $('ota-bar').hidden = frac === null;
    if (frac !== null) $('ota-bar-fill').style.width = Math.round(frac * 100) + '%';
  }

  // Waits until /api/hello shows a boot number different from oldBoot (every 1 s, at most limitMs).
  // After a restart the device's network starts afresh and a phone / computer does not always reconnect by itself.
  // With a silent bus and no computer on USB the device waits 60 s for a client, then turns Wi-Fi off
  // and falls asleep (lib/core/net_policy.h); it is then woken only by CAN traffic or power being reapplied.
  function rejoinHint() { return t('ota.rejoin'); }
  async function waitReboot(oldBoot, limitMs, label) {
    const t0 = Date.now();
    while (Date.now() - t0 < limitMs) {
      await sleep(1000);
      const sec = Math.round((Date.now() - t0) / 1000);
      otaLine(t('ota.wait', { label: label, s: sec }) + (sec >= 20 ? '. ' + rejoinHint() : ''), 'warn');
      const r = await rawFetch('GET', '/api/hello', { noAuth: true, timeout: 2500 }, null);
      if (r.status === 200 && r.data && typeof r.data.boot === 'number' && r.data.boot !== oldBoot) {
        setOffline(false);
        checkBoot(r.data.boot);
        return true;
      }
    }
    return false;
  }
  // After the restart: /api/ota/status (with retries, because right after start it may be 503 not_ready).
  async function otaStatusRetry(limitMs) {
    const t0 = Date.now();
    while (Date.now() - t0 < limitMs) {
      const r = await api('GET', '/api/ota/status', { timeout: 6000, quiet: true });
      if (r.status === 200 && r.data) { renderOta(r.data); return r.data; }
      if (r.status === 401 || r.status === 429) return null;
      await sleep(1500);
    }
    return null;
  }
  // Result after the restart: comparison of running.elf; with a mode change success only at valid.
  async function afterRestart(expectElf, modeChange, what) {
    let st = await otaStatusRetry(30000);
    if (!st) { otaLine(t('ota.state_unreadable'), 'bad'); return; }
    if (expectElf && st.running.elf.toUpperCase() !== expectElf.toUpperCase()) {
      otaLine(t('ota.rolled_back', { version: st.running.version, what: what }), 'bad');
      return;
    }
    const t0 = Date.now();
    while (st.state === 'pending' && Date.now() - t0 < 600000) {
      otaLine((modeChange ? t('ota.no_power') + ' ' : '') + t('ota.pending_wait', { version: st.running.version }), 'warn');
      await sleep(2000);
      const n = await otaStatusRetry(20000);
      if (!n) break;
      st = n;
      if (expectElf && st.running.elf.toUpperCase() !== expectElf.toUpperCase()) {
        otaLine(t('ota.rolled_back_short', { version: st.running.version }), 'bad');
        return;
      }
    }
    if (st.state === 'valid' || (!modeChange && st.state === 'none')) {
      otaLine(t('ota.done', { version: st.running.version, mode: modeWord(st.running.mode), state: tw(OTA_WORDS, st.state) }), 'ok');
      return true;
    } else if (modeChange) {
      otaLine(t('ota.not_confirmed', { version: st.running.version, state: tw(OTA_WORDS, st.state) }), 'bad');
    } else {
      otaLine(t('ota.running_state', { version: st.running.version, state: tw(OTA_WORDS, st.state) }), 'warn');
    }
  }

  function uploadXhr(query) {
    return new Promise(function (resolve) {
      const x = new XMLHttpRequest();
      x.open('POST', '/api/ota' + query);
      x.setRequestHeader('Authorization', 'Basic ' + btoa('admin:' + S.pw));
      x.setRequestHeader('Content-Type', 'application/octet-stream');
      x.timeout = 200000;                    // the device aborts after 180 s
      x.upload.onprogress = function (e) { if (e.lengthComputable) progress(e.loaded / e.total); };
      x.onload = function () {
        let d = null;
        try { d = JSON.parse(x.responseText); } catch (e) { d = null; }
        resolve({ status: x.status, data: d });
      };
      x.onerror = function () { resolve({ status: 0, data: null }); };
      x.ontimeout = function () { resolve({ status: 0, data: null }); };
      x.send(image.buf);
    });
  }

  async function startOta() {
    if (!otaReady() || S.otaBusy) return;
    S.otaBusy = true;
    otaStep();
    try {
      otaLine(t('ota.checking'));
      const r0 = await api('GET', '/api/ota/status', { timeout: 6000 });
      if (r0.status !== 200 || !r0.data) { otaLine(errText(r0), 'bad'); return; }
      const st = r0.data;
      renderOta(st);
      if (st.carBusy) { otaLine(t('err.car_busy'), 'bad'); return; }
      if (st.state === 'pending') { otaLine(t('err.ota_pending'), 'bad'); return; }
      if (st.restartPending) { otaLine(t('err.restart_pending'), 'bad'); return; }
      if (image.size > st.slotSize) { otaLine(t('ota.too_big_slot'), 'bad'); return; }
      const cur = S.status ? S.status.sys : null;
      if (!await ask(t('ota.ask.title'), [t('ota.ask.p1', { version: image.ident.version, mode: modeWord(image.ident.mode) }),
        t('ota.ask.p2'), t('ota.ask.p3'), t('ota.ask.p4')], t('ota.ask.ok'))) { otaLine(''); return; }
      // Consents come only from the acknowledgements of step 2 (each one ticked by the user). The device
      // requires mode=1 for a different operating mode or CAN transmission, force=1 without the network panel.
      // The running firmware may have changed since step 2 (e.g. a rollback): then step 2 again.
      const need = {};
      image.acks.forEach(function (a) { if (a.box.checked) need[a.need] = true; });
      const now = { mode: !!(cur && image.ident.mode !== cur.mode), cantx: !!(cur && image.ident.canTx !== !!cur.canTx),
        nonet: !image.ident.net };
      if (Object.keys(now).some(function (k) { return now[k] && !need[k]; })) { otaLine(t('ota.chk.changed'), 'bad'); return; }
      const q = [];
      if (now.nonet) q.push('force=1');
      const modeChange = now.mode || now.cantx;
      if (modeChange) q.push('mode=1');
      const oldBoot = S.boot;
      otaLine(t('ota.sending'), 'warn');
      progress(0);
      let r = await uploadXhr(q.length ? '?' + q.join('&') : '');
      // The device checks the marker once more (no_net, mode_change, wrong_product): then the usual
      // refusal message below; we never give consents without asking.
      progress(null);
      if (r.status === 0) { otaLine(t('ota.conn_lost'), 'bad'); return; }
      if (r.status === 401) { otaLine(t('err.auth'), 'bad'); authLost(t('login.lost_short'), true); return; }
      if (r.status !== 200 || !r.data || !r.data.ok) { otaLine(t('ota.rejected', { err: errText(r) }), 'bad'); return; }
      const d = r.data;
      otaLine(t('ota.accepted', { bytes: num(d.bytes), version: d.version, mode: modeWord(d.mode), s: dec(d.restartInMs / 1000, d.restartInMs % 1000 ? 1 : 0) }), 'warn');
      if (!await waitReboot(oldBoot, 120000, (modeChange ? t('ota.no_power') + ' ' : '') + t('ota.wait_restart'))) {
        otaLine(t('ota.no_return'), 'bad');
        return;
      }
      if (await afterRestart(d.elf, modeChange, t('ota.what_new'))) clearFile();
    } finally {
      S.otaBusy = false;
      otaStep();
    }
  }
  function clearFile() {
    $('ota-file').value = ''; image = null;
    clear($('ota-file-info')); clear($('ota-acks')); show($('ota-s2-empty'), true);
    otaStep();
  }

  async function rollback() {
    if (S.otaBusy) return;
    const st = S.ota;
    const cur = S.status ? S.status.sys : null;
    if (!st) { await loadOtaStatus(); return; }
    const o = st.other;
    const dp = [o && o.known ? t('ota.rb.desc', { version: o.version, mode: modeWord(o.mode), build: o.build }) : t('ota.rb.unknown')];
    if (o && o.elf) dp.push(t('ota.rb.hash', { elf: o.elf }));
    const desc = dp.join(', ') + (o && o.elf && o.elf === st.running.elf ? ' ' + t('ota.rb.same') : '');
    if (!await ask(t('ota.rb.title'), t('ota.rb.text', { desc: desc }), t('ota.rb.ok'), true)) return;
    const q = [];
    if (st.state !== 'pending' && o && o.known && o.net === false) {
      if (!await ask(t('ota.rb.nonet_title'), t('ota.rb.nonet_text'), t('common.yes_btn'), true)) return;
      q.push('force=1');
    }
    // As for the upload: mode=1 with a different operating mode or a different ability to transmit on CAN (canTx).
    const workChange = !!(cur && o && o.known && o.mode && o.mode !== cur.mode);
    const canTxChange = !!(cur && o && o.known && !!o.canTx !== !!cur.canTx);
    const modeChange = workChange || canTxChange;
    if (workChange) {
      // Back to BESIDE mode: the same warning as the wizard (the radio loses ACC without the factory box).
      if (!await ask(t('ota.mode.title', { from: modeWord(cur.mode), to: modeWord(o.mode) }),
        t(o.mode === 'listen' ? 'ota.mode.to_listen' : 'ota.rb.mode_text'), t('ota.mode.ok'), true)) return;
    }
    if (canTxChange) {
      if (!await ask(t(o.canTx ? 'ota.cantx.title_yes' : 'ota.cantx.title_no'),
        t(o.canTx ? 'ota.rb.cantx_yes' : 'ota.rb.cantx_no'), t('ota.rb.anyway'), true)) return;
    }
    if (modeChange) q.push('mode=1');
    S.otaBusy = true;
    try {
      const oldBoot = S.boot;
      const r = await api('POST', '/api/ota/rollback' + (q.length ? '?' + q.join('&') : ''));
      if (r.status !== 200) { otaLine(t('ota.rb.error', { err: errText(r) }), 'bad'); return; }
      if (!await waitReboot(oldBoot, 120000, t('ota.wait_restart'))) { otaLine(t('ota.no_return_short'), 'bad'); return; }
      await afterRestart(null, modeChange, t('ota.what_prev'));
    } finally { S.otaBusy = false; }
  }
  async function restart() {
    if (!await ask(t('ota.restart_title'), t('ota.restart_ask'), t('ota.restart_ok'), true)) return;
    const oldBoot = S.boot;
    const r = await api('POST', '/api/restart');
    if (r.status !== 200) { toast(t('ota.restart_error', { err: errText(r) }), true); return; }
    toast(t('ota.restart_scheduled'));
    if (await waitReboot(oldBoot, 120000, t('ota.wait_restart_short'))) otaLine(t('ota.restarted'), 'ok');
    else otaLine(t('ota.no_return_wait') + ' ' + rejoinHint(), 'bad');
  }

  function renderOta(st) {
    S.ota = st;
    S.otaLoaded = true;
    const box = $('ota-state');
    clear(box);
    const g = el('dl', 'kv');
    function row(k, v) { g.appendChild(el('dt', null, k)); g.appendChild(el('dd', null, v === null || v === undefined || v === '' ? '—' : String(v))); }
    row(t('ota.kv.state'), st.state === 'pending' ? t('ota.confirm_in', { state: tw(OTA_WORDS, st.state), s: Math.ceil(st.confirmInMs / 1000) }) : tw(OTA_WORDS, st.state));
    row(t('ota.kv.running'), st.running.slot + ': ' + st.running.version + ' (' + modeWord(st.running.mode) + '), ' + st.running.build);
    row(t('ota.kv.hash'), st.running.elf);
    const o = st.other;
    let od;
    if (!o.image) od = t('ota.other.empty');
    else if (!o.scanned) od = t('ota.other.not_scanned');
    else if (!o.known) od = t('ota.other.no_marker');
    else od = o.version + ' (' + modeWord(o.mode) + (o.net ? '' : ', ' + t('ota.other.no_net')) + '), ' + o.build + (o.bootable ? '' : ', ' + t('ota.other.not_bootable'));
    row(t('ota.kv.other', { slot: o.slot }), od);
    if (o.image && o.scanned) row(t('ota.kv.other_hash'), o.elf ? o.elf + (o.elf === st.running.elf ? ' ' + t('ota.same_as_running') : '') : null);
    row(t('ota.kv.last'), st.last.result === 'none' ? t('ota.last_none') : st.last.result + (st.last.error ? ': ' + tw(ERRORS, st.last.error) : '') + ', ' + num(st.last.bytes) + ' B');
    row(t('ota.kv.max'), num(st.slotSize) + ' B');
    if (st.carBusy) row(t('ota.kv.note'), t('ota.car_busy_note'));
    if (st.restartPending) row(t('ota.kv.restart'), t('ota.restart_planned', { reason: st.restartReason }));
    box.appendChild(g);
    $('btn-rollback').disabled = !st.canRollback;
  }
  async function loadOtaStatus() {
    S.otaLoaded = true;
    const r = await api('GET', '/api/ota/status', { timeout: 6000 });
    if (r.status === 200 && r.data) renderOta(r.data);
    else { S.otaLoaded = false; toast(t('ota.status_error', { err: errText(r) }), true); }
  }

  // ---------- language ----------
  // Text with `code` fragments (backticks in the dictionary): plain parts as text, the rest as <code>.
  // Everything goes in through textContent, never as HTML.
  function setRich(n, s) {
    if (s.indexOf('`') < 0) { if (n.textContent !== s) n.textContent = s; return; }
    clear(n);
    s.split('`').forEach(function (part, i) {
      if (i % 2) n.appendChild(el('code', null, part));
      else if (part) n.appendChild(document.createTextNode(part));
    });
  }
  // Fills every element marked in index.html (and the labels of the kv grids), then redraws what
  // was drawn from data, so that a language change does not wait for the next answer of the device.
  function applyI18n() {
    document.documentElement.lang = I18N[LANG] ? LANG : 'en';
    document.querySelectorAll('[data-i18n]').forEach(function (n) { setRich(n, t(n.getAttribute('data-i18n'))); });
    document.querySelectorAll('[data-i18n-title]').forEach(function (n) { n.title = t(n.getAttribute('data-i18n-title')); });
    document.querySelectorAll('[data-i18n-placeholder]').forEach(function (n) { n.placeholder = t(n.getAttribute('data-i18n-placeholder')); });
    document.querySelectorAll('[data-i18n-aria]').forEach(function (n) { n.setAttribute('aria-label', t(n.getAttribute('data-i18n-aria'))); });
    const meta = I18N[LANG] && I18N[LANG]._meta;
    show($('lang-note'), !!(meta && typeof meta.status === 'string' && meta.status.indexOf('machine') === 0));
    ['login-lang', 'sys-lang'].forEach(function (id) { if ($(id).value !== LANG) $(id).value = LANG; });
    $('btn-can-pause').textContent = t(canPaused ? 'can.resume' : 'can.pause');
    updateRec();
    editorRows.forEach(function (r) { r.a.relabel(); r.b.relabel(); });
    Object.keys(tableRows).forEach(function (id) { const nm = CAN_NAMES[id]; if (nm) tableRows[id].cells[1].title = t(nm[1]); });
    if (S.status) renderStatus(S.status);
    if (S.ota) renderOta(S.ota);
    if (!S.otaBusy && (image || $('ota-file').files.length)) onFilePicked();
    if (S.keymap) { setText($('map-current'), mapText(S.keymap.map)); setText($('map-defaults'), mapText(S.keymap.defaults)); }
    updateBanner();
  }
  async function saveLang() {
    const r = await api('POST', '/api/lang', { form: { lang: LANG } });
    if (r.status === 200) S.devLang = LANG; else toast(errText(r), true);
  }
  // A dictionary other than English comes from /i18n/<code>.json (no login, gzip, cached by the browser with
  // ETag). Until it arrives, and when the request fails, t() falls back to English; a later choice retries.
  const langLoading = {};
  function loadLang(code) {
    if (I18N[code] || langLoading[code]) return;
    langLoading[code] = true;
    fetch('/i18n/' + code + '.json', { credentials: 'same-origin' })
      .then(function (r) { return r.ok ? r.json() : null; })
      .catch(function () { return null; })
      .then(function (d) {
        langLoading[code] = false;
        if (!d || typeof d !== 'object' || Array.isArray(d)) return;
        I18N[code] = d;
        if (LANG === code) applyI18n();
      });
  }
  function setLang(code, save) {
    if (!has.call(LANGS, code)) code = 'en';
    LANG = code;
    if (save) langSet(code);
    loadLang(code);
    applyI18n();
  }
  function initLang() {
    ['login-lang', 'sys-lang'].forEach(function (id) {
      const sel = $(id);
      langList().forEach(function (code) {
        const o = el('option', null, LANGS[code] || lookup(code, 'lang.name') || code);
        o.value = code;
        sel.appendChild(o);
      });
      sel.addEventListener('change', function () { setLang(sel.value, true); if (S.authed) saveLang(); });
    });
    const saved = langGet();
    LANG = saved && has.call(LANGS, saved) ? saved : 'en';
    loadLang(LANG);
  }

  // ---------- event wiring ----------
  function on(id, ev, fn) { $(id).addEventListener(ev, fn); }
  function init() {
    initLang();
    initGrids();
    document.querySelectorAll('.tabbar button').forEach(function (b) {
      b.addEventListener('click', function () { selectTab(b.dataset.tab); });
    });
    on('login-form', 'submit', function (e) {
      e.preventDefault();
      const pw = $('login-pw').value;
      if (!pw) return;
      if (Date.now() < S.lockUntil) { msg($('login-msg'), t('login.wait_lock'), 'bad'); return; }
      tryLogin(pw, $('login-remember').checked);
    });
    on('btn-logout', 'click', function () { authLost(''); });
    // Console
    on('cmd-form', 'submit', async function (e) {
      e.preventDefault();
      const i = $('cmd-line');
      if (await sendCmd(i.value)) i.value = '';
    });
    on('cmd-line', 'keydown', function (e) {
      if (e.key === 'ArrowUp' && histPos > 0) { histPos--; e.target.value = hist[histPos]; e.preventDefault(); }
      else if (e.key === 'ArrowDown') {
        if (histPos < hist.length - 1) { histPos++; e.target.value = hist[histPos]; } else { histPos = hist.length; e.target.value = ''; }
        e.preventDefault();
      }
    });
    document.querySelectorAll('[data-cmd]').forEach(function (b) {
      b.addEventListener('click', function () { sendCmd(b.dataset.cmd); });
    });
    on('vol-range', 'input', function () { volDrag = true; setText($('vol-val'), $('vol-range').value + ' %'); });
    on('vol-range', 'change', sendVolume);
    on('btn-test', 'click', function () { sendCmd('test'); });
    on('btn-cantx-on', 'click', function () { canTxArm(true); });
    on('btn-cantx-off', 'click', function () { canTxArm(false); });
    on('log-filter', 'input', rerenderLog);
    on('btn-log-clear', 'click', function () { logLines.length = 0; clear($('log-view')); });
    // CAN
    on('can-filter', 'input', function () { setCanFilter(canFilter()); });
    on('btn-can-pause', 'click', function () {
      canPaused = !canPaused;
      $('btn-can-pause').textContent = t(canPaused ? 'can.resume' : 'can.pause');
      if (!canPaused) rerenderTrace();
    });
    on('btn-rec', 'click', function () {
      if (!rec.on) {
        rec.lines = []; rec.frames = 0; rec.started = Date.now(); rec.header = recHeader(); rec.on = true;
      } else rec.on = false;
      updateRec();
    });
    on('btn-rec-save', 'click', saveRecording);
    on('btn-trace-dl', 'click', downloadDeviceTrace);
    on('mask-vin', 'change', function () {
      Object.keys(tableRows).forEach(function (k) { tableRows[k].maskedShown = false; tableRows[k].data = ''; clear(tableRows[k].cells[5]); });
      rerenderTrace();
    });
    // Radio
    on('btn-key', 'click', function () { sendKey('key'); });
    on('btn-kd', 'click', function () { sendKey('kd'); });
    on('btn-ku', 'click', function () { sendKey('ku'); });
    on('btn-map-add', 'click', function () { addEditorRow('', ''); markDirty(); });
    on('btn-map-save', 'click', saveKeymap);
    on('btn-map-reset', 'click', resetKeymap);
    on('btn-map-reload', 'click', function () { if (S.keymap) loadEditor(S.keymap.map); });
    on('btn-learn', 'click', function () {
      const lk = S.keymap && S.keymap.lastKey;
      if (!lk) return;
      for (let i = 0; i < editorRows.length; i++) if (editorRows[i].a.get() === lk.code) { toast(t('keymap.already', { code: lk.code })); return; }
      addEditorRow(lk.code, ''); markDirty();
    });
    // System
    on('sta-form', 'submit', function (e) { e.preventDefault(); saveSta(false); });
    on('btn-sta-forget', 'click', function () { saveSta(true); });
    on('pw-form', 'submit', function (e) { e.preventDefault(); changePassword(); });
    on('btn-set-export', 'click', exportSettings);
    on('btn-set-import', 'click', function () { $('set-file').click(); });
    on('set-file', 'change', importSettings);
    on('ota-file', 'change', onFilePicked);
    on('btn-ota', 'click', startOta);
    on('btn-ota-refresh', 'click', loadOtaStatus);
    on('btn-rollback', 'click', rollback);
    on('btn-restart', 'click', restart);
    otaStep();     // step 1 of the wizard is marked as the one to do from the start
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape' && !$('modal').hidden) $('modal-cancel').click(); });
    setInterval(function () { updateBanner(); if (rec.on) updateRec(); }, 1000);

    // Polling (contract, chapter 7)
    loop(pollStatus, 500, always);
    loop(pollLog, function () { return S.tab === 'konsola' ? 500 : 1000; }, always);
    loop(pollFrames, 500, function () { return S.tab === 'can' || rec.on; });
    loop(pollTable, 1000, function () { return S.tab === 'can'; });
    loop(pollKeymap, 1000, function () { return S.tab === 'radio'; });
    helloLoop();

    applyI18n();
    selectTab('pulpit');
    const saved = storeGet();
    if (saved) tryLogin(saved, S.remember);
    else showLogin('');
  }
  init();
})();
