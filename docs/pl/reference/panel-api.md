# API panelu (HTTP, wersja API 1)

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona to umowa między trzema częściami projektu:

- obsługą zapytań w programie urządzenia (`firmware/src/net/http_server.cpp`, `api_handlers.cpp`,
  `ota.cpp`),
- stroną panelu (`firmware/web/`),
- makietą urządzenia w Pythonie (`firmware/tools/mock_device.py`).

Zmiana działania zaczyna się od zmiany tutaj. Zmiana, która psuje starszych klientów, podnosi
`FW_API_VERSION` w `firmware/src/version.h` (pole `api` w `/api/hello`). Zobacz
[Wersjonowanie](versioning.md).

API istnieje tylko w programach zbudowanych z siecią: środowiska `listen_wifi`,
`decoder_wifi`, `decoder_tx_wifi` i `listen_generic_wifi`.

> [!NOTE]
> **Nazwy w nawiasach ostrych**
>
> `<fw_product>`, `<ap_ssid_prefix>` i `<mdns_host>` to wartości z `project.json`. Zmienią się,
> gdy projekt dostanie ostateczną nazwę. Sprawdź je tam.

> [!CAUTION]
> **API nie nada ramki, ale jedna wersja umie uzbroić nadawanie**
>
> Żadne zapytanie nie wysyła ramki CAN ani nie zapisuje niczego do modułu auta. Linia konsoli
> z ramką (`0E1#...`) trafia tylko do wewnętrznej logiki urządzenia, tak jak z USB. Tylko
> w programie zbudowanym z `decoder_tx_wifi` klient w **sieci urządzenia** może uzbroić albo
> rozbroić nadawanie na CAN (`cantx on` / `cantx off` przez `POST /api/cmd`). O tym, które ramki
> wolno wtedy wysłać, decyduje brama nadawania, a nie kod sieciowy. Zobacz
> [Nadawanie na magistralę CAN](../safety/transmit.md).

## 1. Zasady ogólne {#1-general-rules}

### 1.1 Adresy {#11-addresses}

| Skąd | Adres |
|---|---|
| sieć urządzenia `<ap_ssid_prefix>XXXX` | `http://10.50.8.1/` |
| nazwa mDNS (sieć urządzenia i sieć domowa) | `http://<mdns_host>.local/` |
| sieć domowa | adres z DHCP routera; pokazuje go polecenie USB `wifi` i pole `wifi.staIp` |

`XXXX` to cztery cyfry szesnastkowe z końca adresu MAC układu.

Port 80, HTTP/1.1, **bez szyfrowania**. Serwer obsługuje zapytania **po jednym naraz** (jedno
zadanie). Przyjmuje do 7 połączeń; gdy wszystkie są zajęte, zamyka najdawniej używane. Wgrywanie
programu (`POST /api/ota`) blokuje wszystkie inne zapytania aż do końca.

Linia zapytania i nagłówki muszą przyjść w ciągu 3 s od pierwszego bajtu (termin łączny,
`HTTP_HDR_DEADLINE_MS`). Wolniejszy klient jest rozłączany bez odpowiedzi. Dlatego jeden klient,
który wysyła nagłówki po jednym bajcie, zajmuje jedyne zadanie serwera najwyżej na ok. 6 s (termin
plus jeden odbiór 3 s), a nie w nieskończoność.

### 1.2 Logowanie {#12-login}

- `GET /`, `GET /i18n/<lang>.json` i `GET /api/hello` są publiczne. **Każde inne zapytanie `/api/...` wymaga nagłówka**
  `Authorization: Basic base64("admin:" + device password)`.
- Hasło urządzenia to to samo hasło, które chroni sieć Wi-Fi urządzenia. Pokazuje je tylko
  polecenie USB `wifi showpass`. Żadne zapytanie go nie zwraca.
- Serwer **nigdy nie wysyła nagłówka `WWW-Authenticate`** ani nagłówków `Access-Control-*`. Dzięki
  temu przeglądarka nie pokazuje własnego okna logowania i nie zapamiętuje hasła. Panel ma własny
  formularz, trzyma hasło w `sessionStorage` (pod wartością `web_session_key` z `project.json`;
  w `localStorage` pod tym samym kluczem, gdy zaznaczono **Zapamiętaj na tym urządzeniu**)
  i sam dodaje nagłówek w `fetch`.
  Z wiersza poleceń: `curl -u admin:PASSWORD http://10.50.8.1/api/status`.
- Urządzenie bez hasła (chwilę przed pierwszym startem sieci albo po błędzie pamięci) odpowiada na
  każde zapytanie wymagające logowania `503 {"error":"not_ready"}`. Dotyczy to **wszystkich** takich
  adresów, także tam, gdzie listy błędów niżej o tym nie wspominają. Puste hasło nigdy nie jest
  ważne.
- Brak nagłówka: `401 {"error":"auth"}` (nie liczy się jako nieudana próba).
  Złe hasło albo zła nazwa użytkownika: `401 {"error":"auth"}` i licznik nieudanych prób rośnie
  (tylko dla klientów spoza sieci urządzenia, zobacz niżej). Powtórzenie **tego samego** złego hasła
  co w poprzedniej nieudanej próbie nie zwiększa licznika (stara zakładka przeglądarki ze starym
  hasłem nie blokuje panelu).
- Blokada: 5 nieudanych prób w ciągu 60 s blokuje logowanie na 30 s; każda kolejna blokada
  podwaja czas, do 5 min. Poprawne logowanie nie kasuje nieudanych prób z bieżącego okna 60 s.
  W czasie blokady **każde** zapytanie wymagające logowania, także z poprawnym hasłem, dostaje
  `429 {"error":"locked","retryInS":N}` (N = pozostałe sekundy, zaokrąglone w górę).
  Jest jedna blokada dla wszystkich klientów **z sieci domowej**. Klienci **sieci urządzenia**
  (adres z `10.50.8.0/24` **i** połączenie z adresem urządzenia `10.50.8.1`) nigdy nie są
  blokowani: znają już hasło, bo jest kluczem tej sieci. Oba warunki są sprawdzane razem: sam adres
  klienta da się podrobić z sieci domowej. Telefon w sieci urządzenia, który łączy się z adresem
  urządzenia w sieci domowej, liczy się jako klient sieci domowej.
- Zmiany z sieci domowej: zapytania zmieniające stan (`POST /api/cmd`, `/api/keymap`,
  `/api/lang`, `/api/settings`, `/api/wifi`, `/api/password`, `/api/restart`, `/api/ota`, `/api/ota/rollback`) z adresu spoza
  sieci urządzenia dostają `403 {"error":"ap_only"}`, dopóki ktoś nie wpisze `wifi lan on`
  w konsoli USB (domyślnie: wyłączone). Odczyt działa z obu sieci.
- **Ryzyko sieci domowej:** HTTP nie jest szyfrowane, a nagłówek z hasłem idzie z każdym
  zapytaniem. W sieci domowej każde urządzenie, które podsłuchuje ruch, może poznać hasło
  urządzenia. To hasło jest też kluczem sieci Wi-Fi urządzenia i jedynym zabezpieczeniem
  aktualizacji programu. Zapisuj sieć domową tylko wtedy, gdy ufasz każdemu urządzeniu w niej.
  Panel pokazuje to ostrzeżenie obok formularza sieci domowej.
- **Przekazywanie pakietów między sieciami (znane, bez zmian):** biblioteki frameworka są
  zbudowane z włączonym `CONFIG_LWIP_IP_FORWARD` (stała skompilowanego lwIP, program nie może jej
  wyłączyć). Urządzenie w sieci domowej, które wyśle pakiet na adres MAC urządzenia z adresem
  docelowym `10.50.8.x`, może dotrzeć do telefonu podłączonego do sieci urządzenia (bez NAT:
  `NAPT` nie jest włączone). Panel na tym nie polega; ma to znaczenie tylko przy zapisanej sieci
  domowej.
- Panel przestaje odpytywać po `401` i pokazuje formularz logowania; po `429` czeka `retryInS`
  sekund; po `403 ap_only` mówi „zmiany tylko z sieci urządzenia” i odpytuje dalej.

Zobacz też [Model zabezpieczeń](../safety/security.md).

### 1.3 Format {#13-format}

- Odpowiedzi `/api/...` to JSON w UTF-8: `Content-Type: application/json`,
  `Cache-Control: no-store`, `X-Content-Type-Options: nosniff`. Wyjątek: `/api/trace.log`
  (tekst).
- Kolejność pól w obiekcie nie jest gwarantowana. Kolejność elementów w tablicy jest częścią
  umowy.
- Liczby całkowite nie mają części ułamkowej. Liczniki są 32-bitowe bez znaku (przekręcają się po
  4294967295).
- Pole bez wartości to `null` (nie jest pomijane).
- Kody szesnastkowe w napisach są wielkimi literami, bez `0x`.
- Treść zapytania `POST` (oprócz `/api/ota`) to `application/x-www-form-urlencoded`: pola
  `name=value` połączone znakiem `&`, wartości zakodowane jako `%XX` (spacja jako `+` albo `%20`).
  Nagłówek `Content-Type` nie jest sprawdzany.
- Błąd z obsługi zapytania: właściwy status HTTP i treść `{"error":"<word>"}`, czasem z dodatkowym
  polem `"detail":"<text>"`. Słowa są w [1.6](#16-error-words).
- Błędy samego serwera (nieznany adres `404`, zła metoda `405`, za długi adres `414`, za długie
  nagłówki `431`) mają treść tekstową, nie JSON. Klient powinien patrzeć na status.

### 1.4 Limity {#14-limits}

| Co | Limit | Po przekroczeniu |
|---|---|---|
| adres z parametrami | ok. 496 bajtów (zmierzone na stole: 496 przechodzi, 497 daje `414`; limit linii zapytania esp_http_server, `CONFIG_HTTPD_MAX_URI_LEN` = 512) | `414` (serwer) |
| część adresu po `?` | 127 bajtów | całe zapytanie jest pomijane (jakby nie było parametrów) |
| nagłówki zapytania | 2048 bajtów | `431` (serwer) |
| czas na linię zapytania i nagłówki | 3 s od pierwszego bajtu (łącznie, nie na bajt) | rozłączenie, bez odpowiedzi |
| treść formularza | 1024 bajty, odebrane w ciągu 1 s | `413 {"error":"too_large"}`; gdy reszty nie da się wyrzucić w ciągu 1 s (albo jest dłuższa niż 64 KB), połączenie też jest zamykane |
| plik programu | `slotSize` z `/api/ota/status` (3 342 336 bajtów) | `413 {"error":"too_large"}` i połączenie jest zamykane |
| linia konsoli | 127 znaków | `400 bad_request` |
| ramki w jednej odpowiedzi | 400 | obcięte |
| linie w jednej odpowiedzi `/api/log` | 64 linie i 4096 bajtów tekstu | reszta w następnej odpowiedzi (`next < head`) |
| całe wgrywanie programu | 180 s | zapis przerwany, połączenie zamknięte |
| odpowiedź JSON | 16 KB | dane są dzielone (kursory), nigdy cięte w połowie |

Treść wysyłana w kawałkach (`Transfer-Encoding: chunked`) nie jest obsługiwana: zapytanie z treścią
musi mieć `Content-Length`.

### 1.5 Czas, numer startu, kursory {#15-time-boot-number-cursors}

- Wszystkie czasy `...Ms` to milisekundy od startu urządzenia (`millis()`). Przekręcają się po
  49,7 dnia.
- `boot` to losowa liczba wybierana przy każdym starcie urządzenia. Gdy klient widzi inną wartość
  niż wcześniej, urządzenie się zrestartowało: odrzuć kursory i zapamiętane liczniki.
- **Kursor.** Każda linia konsoli i każda ramka CAN dostaje kolejny numer (linie i ramki są liczone
  osobno, od 0 po starcie). Klient prosi „daj mi wszystko od numeru `c`” i dostaje:
    - `next`: numer do wysłania w następnym zapytaniu (pierwszy rekord, którego jeszcze nie dostał,
      albo, z filtrem ID, którego jeszcze nie przejrzano);
    - `head`: numer, który dostanie następny nowy rekord. `next < head` oznacza, że czeka więcej
      rekordów (odpowiedź była pełna), więc zapytaj od razu jeszcze raz;
    - `dropped`: ile rekordów od numeru `c` wypadło już z pamięci urządzenia, zanim klient zapytał.
- Bez `c`: od najstarszego rekordu, który jest jeszcze w pamięci, z `dropped` = 0.
- `c` „z przyszłości” jest traktowane jak `head` (nic do zwrócenia, `next` = `head`). Numery
  przekręcają się po 2³², więc porównuje się je „na kole”: `head + 1 .. head + 2³¹` to przyszłość,
  a wartość poniżej najstarszego rekordu (także na przykład `4294967295` przy małym `head`, czyli
  „tuż przed 0”) to przeszłość: odpowiedź zaczyna się od najstarszego rekordu z `dropped` > 0.
- Żeby zacząć „od teraz”, weź `log.head` / `trace.head` z `/api/status`.
- Urządzenie nie trzyma stanu dla poszczególnych klientów: kilka przeglądarek naraz i ponowne
  połączenia są w porządku. Pamięć mieści ok. 200 ostatnich linii (8 KB) i 2048 ostatnich ramek
  (ok. 9 s magistrali auta wzorcowego).

### 1.6 Słowa błędów {#16-error-words}

| HTTP | `error` | Znaczenie |
|---|---|---|
| 400 | `bad_request` | brak wymaganego pola, zła forma, niedozwolone znaki, za długie; `detail` = nazwa pola. `detail` = `usb_only` dla poleceń `wifi ...` oraz dla `cantx reset` i `set NN VV` wysłanych przez `/api/cmd` |
| 400 | `weak_password` | nowe hasło łamie zasady (10..63 drukowalne znaki ASCII, bez spacji) |
| 400 | `bad_image` | plik nie jest programem dla tego układu albo jest uszkodzony; `detail`: `short`, `header`, `verify` |
| 400 | `wrong_product` | plik nie ma znacznika tego urządzenia albo to inny produkt |
| 401 | `auth` | brak logowania albo złe logowanie |
| 403 | `wrong_password` | pole `old` nie zgadza się z obecnym hasłem |
| 403 | `ap_only` | zmiana wysłana z sieci domowej bez zgody `wifi lan on` (1.2). Także od klienta spoza sieci urządzenia, **bez względu na `wifi lan`**: polecenia nadawania na CAN (`cantx on`, `cantx off`, `vset on NN`, `vset off NN`) w `/api/cmd` oraz wgranie albo powrót, które zmieniają tryb pracy lub `canTx` |
| 409 | `car_busy` | włączony bieg wsteczny albo aktywne czujniki parkowania: spróbuj później |
| 409 | `ota_pending` | działający program czeka na potwierdzenie po aktualizacji (do ok. 20 s po starcie) |
| 409 | `restart_pending` | restart jest już zaplanowany albo trwa inna operacja na drugim miejscu na program (cofnięcie, wgrywanie) |
| 409 | `no_net` | obraz nie ma panelu sieciowego; powtórz z `force=1`, jeśli tak chcesz |
| 409 | `mode_change` | obraz ma inny tryb pracy niż działający albo inną zdolność nadawania na CAN (`canTx`); powtórz z `mode=1`, jeśli tak chcesz |
| 409 | `no_rollback` | nie ma programu, do którego można wrócić |
| 411 | `length_required` | zapytanie z treścią, ale bez `Content-Length`, albo z pustą treścią |
| 413 | `too_large` | treść jest dłuższa niż limit |
| 429 | `locked` | blokada po złych hasłach; dodatkowe pole `retryInS` |
| 500 | `flash` | błąd zapisu do flash; `detail` = nazwa błędu ESP-IDF |
| 500 | `overflow` | odpowiedź nie zmieściła się w buforze (błąd programu: zgłoś go) |
| 503 | `busy` | kolejka poleceń jest pełna albo dane są na chwilę zajęte; spróbuj za chwilę |
| 503 | `not_ready` | urządzenie dopiero wystartowało i nie ma jeszcze migawki stanu (pierwsze 0,2 s) albo nie ma jeszcze hasła urządzenia |

## 2. Zapytania publiczne {#2-public-requests}

### GET / {#get-}

Strona panelu: jeden plik HTML z CSS i JS w środku, spakowany gzipem. Nie zawiera żadnych
tajemnic.

Nagłówki odpowiedzi: `Content-Type: text/html; charset=utf-8`, `Content-Encoding: gzip`,
`Cache-Control: no-cache`, `ETag: "<16 hex digits>"`, `X-Frame-Options: DENY`,
`X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer`,
`Content-Security-Policy: default-src 'none'; script-src 'sha256-<hash>'; style-src 'unsafe-inline'; connect-src 'self'; img-src data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'`.

Skrót to SHA-256 w base64 z treści jedynego znacznika `<script>` strony; liczy go
`firmware/tools/embed_web.py` (stała `WEB_INDEX_CSP`). Dlatego strona ma dokładnie jeden skrypt
wbudowany, żadnego `onclick=` w HTML i żadnego `eval`.
Z `If-None-Match` równym bieżącemu `ETag`: `304` bez treści.
Treść jest zawsze spakowana, także gdy klient nie wysłał `Accept-Encoding` (w `curl` użyj
`--compressed`).

**Języki panelu.** Strona zawiera tylko słownik angielski `firmware/web/i18n/en.json`, listę
języków obrazu (kod i nazwa) i dane profilu auta `firmware/web/profiles/psa_aee2010.json`:
`embed_web.py` wstawia je do jedynego skryptu (stałe `I18N`, `LANGS` i `PROFILE`), zanim policzy
skrót CSP. Każdy inny słownik to osobny zasób, `GET /i18n/<lang>.json`, pobierany dopiero po wyborze
tego języka; do czasu pobrania i przy błędzie panel pokazuje angielski. Domyślny język to angielski. Wybór (ekran logowania i zakładka System) jest zapisywany
**w urządzeniu** (`POST /api/lang`, pole `lang` w `/api/hello`), więc każda przeglądarka otwiera się
w nim, także ekran logowania; przeglądarka trzyma tylko kopię podręczną w `localStorage` pod
`ui_lang`, a wartość z urządzenia wygrywa. Urządzenie niczego nie tłumaczy: API, słowa błędów,
linie konsoli i klucze JSON są takie same dla każdego języka. Zasady słowników:
`firmware/web/i18n/README.md`; sprawdzenie: `firmware/tools/check_i18n.py`; rozmiary:
`python -I tools/embed_web.py --report` (uruchom w `firmware/`; budżet 56 KB po gzip dla strony
i 16 KB po gzip dla każdego osobnego słownika, przekroczenie zatrzymuje build).

**`GET /i18n/<lang>.json`** (bez logowania): jeden słownik panelu, dla każdego języka z
`WEB_UI_LANGS` oprócz `en` (który jest w stronie); inny adres dostaje `404`. Zawiera tylko teksty
z `firmware/web/i18n/<lang>.json` (żadnych danych urządzenia) i nie zmienia się do następnego
programu. Nagłówki odpowiedzi: `Content-Type: application/json; charset=utf-8`,
`Content-Encoding: gzip` (zawsze, jak dla `GET /`), `Cache-Control: no-cache`,
`ETag: "<16 cyfr hex>"`, `X-Content-Type-Options: nosniff`; z pasującym `If-None-Match`: `304` bez
treści. Słowniki to tablice `static const` we flash (`WEB_I18N` w generowanym `web_assets.h`),
wysyłane stamtąd bez kopii w RAM.

### GET /favicon.ico {#get-faviconico}

`204` bez treści. Strona panelu ma ikonę karty w sobie, jako adres `data:` w znaczniku
`<link rel="icon">` (dozwolony przez `img-src data:` w CSP), więc przeglądarka, która czyta stronę,
nie potrzebuje tego adresu.

### GET /api/hello {#get-apihello}

Bez logowania. Służy do rozpoznania urządzenia, pokazania blokady i zauważenia restartu po
aktualizacji. Celowo nie podaje wersji, skrótu ani trybu (te są w `/api/status`, po zalogowaniu).

```json
{"product":"<fw_product>","api":1,"boot":2893471123,"locked":0,"lang":"pl"}
```

| Pole | Typ | Znaczenie |
|---|---|---|
| `product` | napis | zawsze wartość `fw_product` z `project.json` |
| `api` | liczba | wersja tego opisu |
| `boot` | liczba | numer startu (1.5) |
| `locked` | liczba | sekundy pozostałe do końca blokady logowania dla klientów sieci domowej; 0 = brak. Klient sieci urządzenia zawsze dostaje 0 (blokada go nie dotyczy, 1.2). Panel traktuje to pole tylko jako informację; logowanie zatrzymuje dopiero prawdziwe `429` |
| `lang` | tekst albo null | język panelu zapisany w urządzeniu (`POST /api/lang`); `null` = nic nie zapisano albo kod, dla którego ten obraz nie ma słownika: angielski. Celowo publiczne, żeby ekran logowania otwierał się w tym języku |

## 3. Stan {#3-state}

### GET /api/status {#get-apistatus}

Jeden obiekt ze wszystkim, co pokazuje pulpit. Dane auta pochodzą z migawki stanu, którą pętla
główna robi co 200 ms; odpytywanie częściej niż co 250 ms nie daje nowych danych.
Błędy: `401`, `429`, `503 not_ready`, `503 busy`.

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

| Pole | Typ | Znaczenie |
|---|---|---|
| `version` | napis | `FW_VERSION`, postać `x.y.z` |
| `build` | napis | czas kompilacji pliku wersji, `Mmm dd yyyy hh:mm:ss`. `firmware/tools/build_time.py` kompiluje ten plik od nowa przy każdej zmianie w `src/`, `lib/` albo `web/`, więc to czas ostatniej kompilacji ze zmienionymi źródłami. Do odróżnienia dwóch obrazów używaj `elf` |
| `elf` | napis | 16 cyfr szesnastkowych wielkimi literami: pierwsze 8 bajtów SHA-256 pliku ELF działającego programu; inne dla każdego zbudowanego obrazu. Porównuj bez względu na wielkość liter |
| `mode` | napis | `listen` = urządzenie obok skrzynki CAN (nic nie wysyła do radia); `decoder` = zamiast skrzynki |
| `canTx` | wartość logiczna | `true` = działający program umie nadawać na CAN auta (bajt `canTx` znacznika nie jest 0). Programy starsze niż 0.7.0 nie wysyłają tego pola: brak pola traktuj jako `false` |
| `boot` | liczba | numer startu (1.5) |
| `uptimeMs` | liczba | czas od startu, w chwili odpowiedzi |
| `resetReason` | napis | przyczyna ostatniego startu: `poweron`, `ext`, `sw` (restart programowy, także po aktualizacji), `panic`, `int_wdt`, `task_wdt`, `wdt`, `deepsleep` (wybudzenie ze snu: zwykły przypadek), `brownout`, `sdio`, `usb`, `jtag`, `efuse`, `pwr_glitch`, `cpu_lockup`, `unknown` |
| `heapFree`, `heapMin`, `heapLargest` | liczba, bajty | wolna pamięć wewnętrzna teraz, najmniej od startu, największy wolny blok |
| `loopBusyPct` | liczba 0..100 | obciążenie rdzenia 1 (pętla główna) w ostatniej pełnej sekundzie |
| `cpu0Pct` | liczba 0..100 | obciążenie rdzenia 0 (Wi-Fi, dźwięk, serwer WWW) |
| `loopMaxUs` | liczba, µs | najdłuższy przebieg pętli głównej w ostatniej pełnej sekundzie, razem z przerwą 1 ms; w spoczynku ok. 1000..2000 |
| `loopsPerSec` | liczba | przebiegi pętli głównej w ostatniej pełnej sekundzie (w spoczynku ok. 1000) |
| `stackLoopFree`, `stackHttpFree`, `stackWifiFree` | liczba, bajty | najmniejszy zapas stosu pętli głównej, zadania WWW i zadania Wi-Fi od startu |
| `tempC` | liczba z 1 miejscem po przecinku albo `null` | temperatura układu ESP32; `null`, gdy odczyt się nie udał (czujnik obejmuje 10..50 °C) |
| `otaState` | napis | stan działającego programu: `none` (wgrany kablem), `valid` (wgrany przez Wi-Fi i potwierdzony), `pending` (czeka na potwierdzenie) |
| `otaBusy` | wartość logiczna | trwa wgrywanie programu. Serwer obsługuje jedno zapytanie naraz, więc klient HTTP w praktyce zawsze widzi `false`; pole jest dla kompletności |
| `restartPending` | wartość logiczna | restart jest zaplanowany (po 1,5 s; gdy auto potrzebuje urządzenia albo radio ma ACC, dopiero po końcu tego stanu) |
| `hubDrops` | liczba | rekordy podglądu (linie, ramki, migawki) zgubione wewnątrz urządzenia od startu; 0 w zdrowym systemie |
| `clientAp` | wartość logiczna | klient, który wysłał TO zapytanie, jest połączony z siecią urządzenia (według adresów obu końców połączenia, nie według nagłówków: ta sama zasada co dla `ap_only`). Tylko taki klient może z panelu uzbroić albo rozbroić nadawanie na CAN i zmienić maskę pozycji. Starsze programy go nie wysyłają: traktuj jako `false` |

**`can`** (magistrala auta)

| Pole | Typ | Znaczenie |
|---|---|---|
| `ok` | wartość logiczna | sterownik CAN wystartował |
| `rx` | liczba | ramki odebrane od startu |
| `missed` | liczba | ramki zgubione, bo kolejka sterownika była pełna (pętla główna nie nadążyła) |
| `overrun` | liczba | ramki zgubione w samym kontrolerze CAN (przerwanie nie mogło się wykonać, na przykład w czasie zapisu do flash przy aktualizacji) |
| `busErrors` | liczba | błędy magistrali widziane przez kontroler |
| `framesPerSec` | liczba | ramki przyjęte w ostatniej pełnej sekundzie (auto wzorcowe: ok. 218, zmierzone (measured)) |
| `busAlive` | wartość logiczna | ramka w ostatniej sekundzie |
| `listenOnly` | wartość logiczna | tryb kontrolera wybrany przez program przy starcie (ustawienie programu, nie pomiar): `true` = tylko nasłuch. `false` tylko w programie z nadawaniem (`sys.canTx` = `true`) po uzbrojonym starcie: kontroler w trybie NORMAL, transceiver w trybie cichym poza oknem jednej ramki. Gwarancja sprzętowa wymagałaby otwartej zworki nadajnika CAN; w prototypie autora zworka JP1 jest zamknięta, a SLNT jest podłączone do GPIO6, więc decyduje tylko program |
| `tx` | obiekt | brama nadawania CAN (`firmware/lib/core/can_tx_gate.h`); tabela niżej. Programy starsze niż 0.9.0 go nie wysyłają |

**`can.tx`.** W programie bez sprzętu do nadawania (`sys.canTx` = `false`): `built` = `false`,
`state` = `"none"`, liczniki 0. W programie zbudowanym z `decoder_tx_wifi`: `built` = `true`.
Panel pokazuje wtedy na karcie Magistrala CAN przyciski uzbrojenia / rozbrojenia; działają one
tylko przy `sys.clientAp` = `true`, wysyłają `cantx on` / `cantx off` przez `/api/cmd`,
a uzbrojenie najpierw pyta, z ostrzeżeniem.

| Pole | Typ | Znaczenie |
|---|---|---|
| `built` | wartość logiczna | program ma sprzęt do nadawania (`false` we wszystkich środowiskach oprócz `decoder_tx_wifi`) |
| `armed` | wartość logiczna | nadawanie jest uzbrojone w tym starcie (brama w stanie innym niż `none` i `disarmed`) |
| `armNext` | wartość logiczna | zapisane jest ważne uzbrojenie, więc następny start będzie uzbrojony. Po `cantx on`: `armNext` = `true` przy `armed` = `false` aż do restartu; po `cantx off` oba są od razu `false` |
| `state` | napis | `none` (program bez nadawania), `disarmed`, `ready`, `opening`, `sending`, `cooldown`, `fault` (blokada) |
| `fault` | napis albo `null` | przyczyna blokady: `3 failed sends`, `transmit error counter`, `bus-off`, `transmitter window exceeded`, `controller event`, `foreign bits of 260 changed`, `external lock`; `null` = brak |
| `sent`, `acked`, `failed`, `refused` | liczba | ramki przekazane do kontrolera, potwierdzone, nieudane, żądania odrzucone przez bramę; 0 bez sprzętu do nadawania |
| `tec`, `rec` | liczba | liczniki błędów kontrolera z ostatniego odczytu przez bramę (przy żądaniu i na końcu przerwy po ramce, nie na żywo); 0 bez sprzętu do nadawania |

**`car`**

| Pole | Typ | Znaczenie |
|---|---|---|
| `ignition` | wartość logiczna | zapłon włączony (ramka `0F6`, tylko ze świeżymi ramkami) |
| `ignitionRaw` | liczba 0..255 albo `null` | bajt 1 ramki `0F6`. Auto wzorcowe: 128 (`80`) wyłączony, 136 (`88`) włączony, 142 (`8E`) silnik pracuje (zmierzone: nagrania w `firmware/test_data/captures/`). `null`, gdy ramka `0F6` jest starsza niż 1 s (także przed pierwszą) |
| `acc` | wartość logiczna | radio dostaje ACC (w trybie `listen`: dostawałoby; wyjście jest wtedy wyłączone) |
| `reverse` | wartość logiczna | bieg wsteczny (przy włączonym zapłonie i świeżej ramce) |
| `lights` | wartość logiczna | światła włączone (ramka `036`, przy włączonym zapłonie) |
| `doors` | liczba (bity) albo `null` | `null`, dopóki od startu nie przyszła ramka drzwi (`128` ze zgaszoną lampką ostrzegawczą albo `1A1`). Otwarte drzwi: 64 (`40`) kierowcy, 128 (`80`) pasażera, 16 (`10`) tył lewe, 32 (`20`) tył prawe, 8 (`08`) bagażnik (zmierzone: próba drzwi w aucie wzorcowym). Znane ograniczenie: gdy zamkną się jedne z kilku otwartych drzwi, wskazanie może zostać, aż zamkną się wszystkie |
| `batteryV` | liczba z 2 miejscami po przecinku albo `null` | napięcie zasilania mierzone na płytce ZA diodą wejściową, ok. 0,8 V (0,7-0,9 V) poniżej akumulatora (spadku nie zmierzono dokładnie); `null`, gdy płytka nie ma dzielnika albo nie ma 12 V (poniżej 6 V; przy zasilaniu z USB dzielnik daje 0..1,5 V szumu). Panel pokazuje wtedy „brak 12 V (zasilanie z USB)”, a `stat` wypisuje `supply=no 12V (x.xxV)` |
| `outsideTempC` | liczba z 1 miejscem po przecinku albo `null` | temperatura zewnętrzna z ramki `0F6`, krok 0,5 °C; `null` bez świeżej ramki |
| `settings` | obiekt | ustawienia pojazdu (personalizacja), tylko odczyt; tabela niżej. Programy starsze niż 0.8.0 go nie wysyłają |

**`car.settings`** (tylko odczyt. Ustawienie zmienia się tylko przez komendę radia `80`
w uzbrojonym programie z nadawaniem.)

| Pole | Typ | Znaczenie |
|---|---|---|
| `raw260` | napis albo `null` | dane ostatniej ramki CAN `260` (stan personalizacji w BSI) szesnastkowo, wielkimi literami, bez spacji; zwykła ramka ma 7 bajtów (14 znaków), krótsza albo dłuższa jest podawana tak, jak przyszła (najwyżej 8 bajtów). `null`, dopóki od startu nie przyszła ramka `260`. To ostatnie odebrane dane, także gdy są stare: zobacz `ageMs` |
| `ageMs` | liczba albo `null` | wiek tej ramki w chwili odpowiedzi (`260` przychodzi mniej więcej co 500 ms przy włączonym zapłonie); `null` razem z `raw260` |
| `state38` | napis, 10 znaków | DATA[1..5] ramki `38`, którą urządzenie wysyła do radia (stan pozycji ekranu „Function settings”), liczone z ramek `260`, `227`, `3E1`, `217` (`firmware/lib/car_psa_aee2010/psa_signals.cpp`, `firmware/lib/radio_xp/xp_signals.cpp`, `vehicles/peugeot-508-sw-2015/settings-map.md`). `0000000000` do pierwszej 7-bajtowej ramki `260`. Podawane także w trybie `listen` (wtedy nic nie idzie do radia) |
| `last` | obiekt albo `null` | ostatnia komenda radia `80 nn vv` (zmiana ustawienia na ekranie radia); `null`, dopóki od startu żadna nie przyszła |
| `last.id`, `last.value` | liczba 0..255 | numer pozycji i żądana wartość |
| `last.result` | napis | `refused` (pozycja ma pole w ramce `15B`, ale program nie nadaje albo jest rozbrojony), `no_frame` (brak znanej ramki dla tej pozycji albo numer spoza tabeli), `blocked` (ramka znana, ale ta wersja jej nie nadaje), `unchanged`, `accepted`, `not_accepted`, `tx_failed`, `side_effect`. Nieznane słowo pokaż takie, jakie jest |
| `last.tookMs` | liczba | czas od komendy do wyniku (dla `accepted`: czas do ramki `260` z nowym stanem); 0 przy odmowie |
| `last.ageMs` | liczba | jak dawno przyszła komenda |
| `last.frame` | napis albo `null` | 8 bajtów ramki `15B` przekazanej do bramy (16 cyfr szesnastkowych) dla `accepted`, `not_accepted`, `tx_failed`, `side_effect`; `null` przy odmowie i dla `unchanged`. Zawsze `null` w programie bez nadawania |
| `accepted`, `notAccepted` | liczba | zmiany przyjęte / nieprzyjęte przez auto od startu; zawsze 0 w programie bez nadawania |
| `refused` | liczba | komendy `80`, które skończyły się odmową, od startu; w programie bez nadawania = liczba wszystkich komend `80` |
| `mask` | liczba | maska włączonych pozycji, numer bitu = numer pozycji (domyślnie 2 = tylko pozycja `01`). Zmiana: `vset on NN` / `vset off NN`. Starsze programy jej nie wysyłają |

„Przyjęte” oznacza, że BSI zmieniło ramkę `260` zgodnie z żądaniem. Czy zmiana jest widoczna
w aucie (wycieraczka, światła, dźwięk), nie sprawdzono. Zobacz [Nadawanie na magistralę CAN](../safety/transmit.md).

**`park`** (ramka `0E1`)

| Pole | Typ | Znaczenie |
|---|---|---|
| `fresh` | wartość logiczna | ramka `0E1` przyszła w ciągu ostatnich 400 ms; przy `false` pozostałe pola mają ostatnie znane wartości |
| `rearStatus`, `frontStatus` | liczba | stan tyłu / przodu: 0 brak ramki, 1 usterka, 2 wyłączone przez kierowcę, 4 aktywne, 5 oczekiwanie, 6 niesprawne |
| `sound` | wartość logiczna | auto prosi o dźwięk |
| `soundFront` | wartość logiczna | `true` przeszkoda z przodu, `false` z tyłu |
| `interval` | liczba 0..63 | pole odstępu piknięć z auta: 0 ton ciągły, 63 brak, 57..11 coraz bliżej |
| `rear`, `front` | tablica 3 liczb | strefy `[left, centre, right]` (lewa, środkowa, prawa), 0 = bardzo blisko, 7 = daleko / brak przeszkody |
| `beeping` | wartość logiczna | urządzenie pika (także w przerwie między piknięciami i w czasie pikania `test`) |
| `continuous` | wartość logiczna | urządzenie gra ton ciągły |

Znaczenie pól `0E1` w aucie wzorcowym, z ich statusem, opisuje [Peugeot 508 SW](vehicles/peugeot-508.md).

**`radio`** (UART do radia, protokół XP)

| Pole | Typ | Znaczenie |
|---|---|---|
| `ok` | wartość logiczna | port UART wystartował |
| `txEnabled` | wartość logiczna | nadawanie do radia jest włączone (tylko tryb dekoder) |
| `heard` | wartość logiczna | radio odezwało się co najmniej raz od startu |
| `ageMs` | liczba albo `null` | czas od ostatniej ramki z radia; `null` przy `heard` = `false` |
| `on` | wartość logiczna | radio zgłosiło „włączone”. Przy `heard` = `false` pola `on`, `volume`, `muted`, `source` mają wartości domyślne (`false`, 0, zera), a nie odczyt: panel pokazuje „—” |
| `volume` | liczba 0..36 | głośność radia |
| `muted` | wartość logiczna | wyciszone |
| `source` | tablica 8 liczb 0..255 | surowe dane ramki źródła dźwięku (`C0`) |
| `time` | napis `YYYY-MM-DD hh:mm` albo `null` | czas podany przez radio |
| `rxFrames`, `rxAcks`, `rxBadChecksum`, `rxBadLength`, `rxTimeouts`, `rxStray` | liczba | odbiór: dobre ramki, potwierdzenia, zła suma kontrolna, zła długość, urwane ramki, bajty poza ramkami |
| `txFrames`, `txAcks`, `txSuppressed` | liczba | wysłane ramki, wysłane potwierdzenia, ramki przygotowane, ale niewysłane (tryb `listen`) |
| `txQueue`, `txQueueCap` | liczba | ramki czekające na wysłanie i rozmiar kolejki |
| `txDropped`, `keysDropped` | liczba | ramki i zdarzenia przycisków zgubione, bo kolejka była pełna |
| `txGated` | liczba | ramki, których urządzenie nie wysłało, bo radio nie miało ACC (przyciski i czujniki przy wyłączonym radiu, odpowiedź na prośbę o wersję); rośnie przy normalnym używaniu |
| `slotsFull` | liczba | ramki stanu pominięte, bo nie było miejsca na nowy rodzaj ramki (12 rodzajów); powinno być 0 |
| `settingCount`, `airCount`, `unknownCount` | liczba | komendy ustawień pojazdu, komendy klimatyzacji i nieznane ramki od radia |
| `lastSetting`, `lastAir` | tablica `[number, value]` | ostatnia taka komenda |

**`audio`**: `ok` (wartość logiczna, sterownik dźwięku wystartował), `volume` (liczba, procent),
`volumeMax` (liczba, górna granica głośności chroniąca głośnik, obecnie 80).

**`wifi`**

| Pole | Typ | Znaczenie |
|---|---|---|
| `on` | wartość logiczna | `phase` = `running` |
| `phase` | napis | `off`, `starting`, `running`, `stopping`, `failed` |
| `apSsid` | napis | nazwa sieci urządzenia |
| `apIp` | napis | adres urządzenia w jego własnej sieci |
| `apChannel` | liczba | kanał (0 = nieznany); przy połączeniu z siecią domową to kanał routera |
| `apStations` | liczba | urządzenia podłączone do sieci urządzenia |
| `staConfigured` | wartość logiczna | sieć domowa jest zapisana |
| `staSsid` | napis | jej nazwa; pusty, gdy nie ma |
| `staConnected` | wartość logiczna | połączone z siecią domową i ma adres |
| `staIp` | napis albo `null` | adres w sieci domowej |
| `staRssi` | liczba albo `null` | siła sygnału routera w dBm |
| `staLastReason` | liczba 0..255 | kod ostatniego rozłączenia z siecią domową (0 = nie było). Panel pokazuje 201 jako „nie znaleziono sieci”, 2, 15, 202, 204 jako „odrzucone hasło”, inne jako liczbę |
| `hostname` | napis | wartość `mdns_host` (adres `<mdns_host>.local`) |
| `maxStepMs` | liczba | najdłuższy pojedynczy krok obsługi Wi-Fi od startu (robi to osobne zadanie; pętla główna na nie nie czeka) |
| `startFails` | liczba | nieudane starty Wi-Fi z rzędu |
| `gaveUp` | wartość logiczna | po 3 nieudanych startach urządzenie przestało próbować (do `wifi on` na USB albo restartu) |
| `blocked` | wartość logiczna | sieć wstrzymana po restartach po awarii (do `wifi on` na USB albo odłączenia i podłączenia zasilania) |
| `lanAdmin` | wartość logiczna | zmiany dozwolone także z sieci domowej (`wifi lan on`) |

**`log`**, **`trace`**: `head` = numer następnej linii / ramki, `oldest` = numer najstarszej,
która jest jeszcze w pamięci (1.5).

## 4. Dane na żywo {#4-live-data}

### GET /api/log {#get-apilog}

Linie konsoli (te same co na USB, bez zrzutu surowych ramek i bez linii z hasłem).

Parametr: `c` (opcjonalny) kursor, tylko cyfry dziesiętne. Błędy: `400 bad_request` (`detail`:
`c`), `401`, `429`, `503 not_ready`, `503 busy`.

```json
{"boot":2893471123,"next":534,"head":534,"dropped":0,
 "lines":[[734100,"[car] reverse=1"],[734312,"[park] rear=4 front=5 sound=1 rear interval=0x18 zones rear 357 front 777"]]}
```

`lines`: tablica par `[czas ms, tekst]`, od najstarszej. Tekst nie ma znaków końca linii i ma
najwyżej 200 znaków (urządzenie obcina dłuższe linie). Jedna odpowiedź mieści najwyżej 64 linie
i 4096 bajtów tekstu; gdy czeka więcej, `next < head` i klient od razu pyta jeszcze raz.
Linie komentarza `# idf: ...` to komunikaty błędów bibliotek ESP-IDF (Wi-Fi, serwer).

### GET /api/frames {#get-apiframes}

Ramki CAN z podglądu (odebrane z magistrali i wpisane poleceniem konsoli).

| Parametr | Znaczenie |
|---|---|
| `c` | kursor (opcjonalny) |
| `id` | filtr: 1..3 cyfry szesnastkowe, wartość do `7FF` (opcjonalny) |
| `max` | najwyżej tyle ramek w odpowiedzi: 1..400; domyślnie 256; większa wartość jest obcinana do 400 |

Błędy: `400 bad_request` (`detail`: `c`, `id` albo `max`; także `max=0`), `401`, `429`,
`503 not_ready`, `503 busy`.

```json
{"boot":2893471123,"next":160133,"head":160133,"dropped":0,
 "frames":[[734501,"0E1","B0F000FCFC1EC200"],[734503,"21F","000000"]]}
```

`frames`: tablica trójek `[czas ms, "ID", "DATA"]`, od najstarszej. ID to zawsze 3 cyfry
szesnastkowe, dane to 0..16 cyfr szesnastkowych (dwie na bajt). Z filtrem `id` odpowiedź zawiera
tylko pasujące ramki, ale `next` przesuwa się też za przejrzane ramki innych ID, a `dropped` liczy
wszystkie ramki, które wypadły, bez względu na ID. Ramki niosące VIN (`336`, `3B6`, `2B6`) są
tu podawane bez zmian; panel (rozdział 7) i eksport je maskują.

### GET /api/table {#get-apitable}

Ostatni stan każdego identyfikatora. Bez parametrów. Błędy: `401`, `429`, `503 busy`.

```json
{"nowMs":734520,"overflow":0,
 "ids":[["036",7302,100,12,"0E00800F310001A8"],["0E1",7290,100,19,"B0F000FCFC1EC200"]]}
```

`ids`: tablica tablic pięcioelementowych `["ID", ramki od startu, odstęp między dwiema ostatnimi
ramkami w ms, wiek ostatniej ramki w ms, "DATA"]`, posortowana po ID, najwyżej 128 pozycji.
Odstęp 0 oznacza, że na razie przyszła tylko jedna ramka. `nowMs` to czas urządzenia w chwili
odpowiedzi (od niego liczony jest wiek; czas jest brany po skopiowaniu tabeli, więc wiek nigdy nie
jest ujemny ani przekręcony).
`overflow`: ramki z identyfikatorami, które nie zmieściły się w tabeli (auto wzorcowe wysyła ok.
70 ID, zmierzone).

### GET /api/trace.log {#get-apitracelog}

Cały podgląd ramek jako plik tekstowy w formacie nagrania.

Parametr: `mask` (opcjonalny). `mask=0` wyłącza maskowanie; każda inna wartość i brak parametru
oznaczają maskowanie. Błędy przed pierwszym bajtem: `401`, `429`, `503 busy`. Po rozpoczęciu
wysyłania błąd kończy się zerwaniem połączenia.

Nagłówki: `Content-Type: text/plain; charset=utf-8`, `Content-Disposition: attachment` ze stałą
nazwą pliku `<slug>_can.log` (`slug` z `project.json`; ustawiane w
`firmware/src/net/api_handlers.cpp`), `Cache-Control: no-store`. Treść jest wysyłana w kawałkach,
linie kończą się tylko `\n`:

```text
# <fw_product> 2.0.0-beta.1 listen; time in seconds since the device started
(734.501) can0 0E1#B0F000FCFC1EC200
# masked
(734.503) can0 21F#000000
```

- Pierwsza linia: komentarz z nazwą produktu, wersją i trybem (`listen` albo `decoder`).
- Ramka: `(seconds.milliseconds) can0 ID#DATA`, sekundy bez zer wiodących, milisekundy zawsze
  3 cyfry.
- Przy maskowaniu każda ramka z ID `336`, `3B6` albo `2B6` (VIN) jest zastępowana linią `# masked`
  (dokładnie tym tekstem). Panel używa tego samego formatu dla własnego nagrania w przeglądarce.
- Plik obejmuje ramki od najstarszej w pamięci do numeru `trace.head` z chwili rozpoczęcia
  pobierania. Gdy w czasie pobierania ramki wypadają z pamięci, linia `# lost N frames` zaznacza
  to miejsce.

## 5. Polecenia {#5-commands}

Polecenia nie są wykonywane w zadaniu serwera. Trafiają do kolejki, a pętla główna wykonuje je
w ciągu kilku milisekund. Odpowiedź `{"queued":true}` oznacza „przyjęte do wykonania”; wynik
pojawia się w `/api/log`, `/api/status` albo `/api/keymap`. Kolejka ma 4 miejsca; gdy jest pełna:
`503 busy`. Wszystkie zapytania z tego rozdziału wymagają zgody na zmiany z sieci klienta
(1.2, `ap_only`).

**Gdy auto potrzebuje urządzenia** (bieg wsteczny, aktywne czujniki): zapytania zapisujące flash
(tabela przycisków, Wi-Fi, hasło, cofnięcie) czekają w kolejce do końca tego stanu (zapis do flash
zatrzymuje odbiór CAN). Linie konsoli wykonują się od razu, chyba że stoją w kolejce za takim
zapytaniem; z panelu w tym stanie `test`, `vol N`, `map FROM TO` i `map reset` są odrzucane (log:
`[www] skipped: the car needs the device`).

### POST /api/cmd {#post-apicmd}

Jedna linia konsoli, tak jak wpisana na USB. Polecenia opisuje strona [Konsola USB](console.md).

Treść: `line=<command>`. Po zdekodowaniu 1..127 znaków, tylko drukowalne ASCII (kody 32..126).
Linia, której pierwsze słowo to `wifi`, jest odrzucana od razu i nie trafia ani do kolejki, ani do
logu (`400 bad_request`, `detail` `usb_only`): polecenia Wi-Fi zawierają hasła.
Tu sprawdzane są też polecenia nadawania na CAN: `cantx reset` i `set NN VV` dają
`400 bad_request`, `detail` `usb_only`; `cantx on`, `cantx off`, `vset on NN`, `vset off NN` od
klienta spoza sieci urządzenia dają `403 ap_only` (także po `wifi lan on`). Klient w sieci
urządzenia dostaje `{"queued":true}`; pętla główna jeszcze raz sprawdza źródło i wypisuje wynik
w logu.

Odpowiedź: `{"queued":true}`. Błędy: `400 bad_request` (`detail`: `line` albo `usb_only`), `401`,
`403 ap_only`, `411`, `413`, `429`, `503 busy`.

Log najpierw pokazuje linię `# www> <command>`, potem odpowiedź polecenia. `help` wysłane z panelu
wypisuje polecenia bez poleceń `wifi`.

### GET /api/keymap {#get-apikeymap}

Tabela zamiany przycisków (kod przycisku z auta → kod wysyłany do radia) i ostatnio naciśnięty
przycisk. Błędy: `401`, `429`, `503 not_ready`, `503 busy`.

```json
{"max":16,"map":[["9F","9B"],["21","1F"]],"defaults":[["9F","9B"],["21","1F"]],
 "lastKey":{"code":"9F","state":1,"ageMs":420}}
```

| Pole | Znaczenie |
|---|---|
| `max` | największa liczba zamian (16) |
| `map` | używane zamiany: pary `["FROM","TO"]`, kody jako dwie cyfry szesnastkowe |
| `defaults` | domyślne zamiany wbudowanego profilu auta (przywracane przez `reset=1`); w `generic` puste |
| `lastKey` | ostatnie zdarzenie przycisku albo `null`, gdy od startu żadnego nie było: `code` = kod z auta PRZED zamianą, `state` = 1 wciśnięty / 2 przytrzymany / 0 puszczony, `ageMs` = ile ms temu |

Dane pochodzą z migawki stanu (odświeżanej co 200 ms).

### POST /api/keymap {#post-apikeymap}

Treść, jedna z dwóch postaci:

- `map=<table>`: cała nowa tabela jako `FROM=TO;FROM=TO`, na przykład `9F=9B;21=1F` (w formularzu:
  `map=9F%3D9B%3B21%3D1F`). Dwie cyfry szesnastkowe na kod, dowolna wielkość liter, `;` na końcu
  jest dozwolony, bez spacji, najwyżej 16 par, każdy kod `FROM` najwyżej raz. Para `X=X` jest
  pomijana. Pusta wartość (`map=`) oznacza brak zamian.
- `reset=1`: powrót do zamian wbudowanych (pole `map`, jeśli jest, jest pomijane).

Odpowiedź: `{"queued":true}`. Błędy: `400 bad_request` (`detail`: `map`; także gdy nie ma żadnego
z pól), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

Tabela jest zapisywana we flash w postaci, którą urządzenie normalizuje (wielkie litery, bez par
`X=X`). `GET /api/keymap` pokazuje nowy stan po ok. 300 ms (później, gdy auto potrzebuje
urządzenia). Gdy zapis się nie uda, tabela działa do restartu, a log mówi
`[key] table write failed`. Zamiana dla kodu `A4` (księżyc) nie działa: ten przycisk działa jako
wyłącznik ekranu.

### POST /api/lang {#post-apilang}

Język panelu zapisany w urządzeniu (późniejszy dodatek do API w wersji 1: tylko nowe pole i nowe
adresy, nic się nie zmienia dla starszych klientów). Panel wysyła go przy zmianie języka po zalogowaniu i raz po zalogowaniu, gdy język
wybrano na ekranie logowania.

Treść: `lang=<kod>`: jeden ze słowników wbudowanych w ten obraz (`firmware/web/i18n/<kod>.json`,
lista `WEB_UI_LANGS` zapisywana przez `embed_web.py`). Pusta wartość (`lang=`) zapomina wybór
(angielski).

Odpowiedź: `{"queued":true}`. Błędy: `400 bad_request` (`detail` `lang`: brak pola albo kod, którego
nie ma w obrazie), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

Kod trafia do flash (klucz NVS `ui_lang`), gdy zapytanie wyjdzie z kolejki (później, gdy auto
potrzebuje urządzenia). Ten sam kod niczego nie zapisuje. `GET /api/hello` pokazuje go od razu
potem. Konsola USB: `lang`, `lang <kod>`, `lang default`. Zobacz [Ustawienia](../../en/reference/settings.md).

### GET /api/settings {#get-apisettings}

Kopia zapasowa ustawień użytkownika, bez haseł. Błędy: `401`, `429`, `503 not_ready`.

```json
{"v":1,"lang":"pl","vol":40,"keys":"9F=9B;21=1F","net":true,"lan":false}
```

| Pole | Znaczenie |
|---|---|
| `v` | wersja formatu, 1 |
| `lang` | zapisany język panelu, `null` = nic nie zapisano (angielski) |
| `vol` | zapisana głośność piszczyka w procentach, `null` = nic nie zapisano (domyślna z kompilacji) |
| `keys` | zapisana tabela przycisków (format `POST /api/keymap`), `null` = nigdy nie zapisana (domyślne profilu auta), `""` = zapisana bez zamian |
| `net`, `lan` | `wifi on/off` i `wifi lan on/off`, informacyjnie |

Celowo poza kopią: hasło urządzenia, sieć domowa, uzbrojenie nadawania CAN i maska ustawień
pojazdu. Format i parser: `firmware/lib/core/settings_backup.h`. Konsola USB: `settings`.

### POST /api/settings {#post-apisettings}

Wczytanie kopii. Treść: `json=<obiekt>` (najwyżej 255 znaków po zdekodowaniu). Dowolny podzbiór
pól z `GET /api/settings`: brak pola = bez zmiany, `null` = powrót do domyślnej. `net` i `lan`
są tu pomijane (zmienia się je tylko przez USB).

Wszystko jest sprawdzane przed wstawieniem do kolejki, więc błędna kopia niczego nie zmienia: `400
bad_request` z `detail` = `json` (brak pola albo za długie), `syntax`, `version`, `unknown_key`,
`duplicate`, `bad_value` (parser), `lang` (brak w obrazie), `vol` (poniżej 15, najniższej głośności,
którą urządzenie zapisuje) albo `keys` (błędna tabela). Inne błędy: `401`, `403 ap_only`, `411`,
`413`, `429`, `503 busy`.

Odpowiedź: `{"queued":true}`. Pętla główna zapisuje pola po kolei (później, gdy auto potrzebuje
urządzenia) i loguje `[settings] import: lang=ok vol=ok keys=ok` (`-` = nie podano, `FAILED` = błąd
zapisu flash). Konsola USB: `settings import <JSON>`.

### POST /api/wifi {#post-apiwifi}

Sieć domowa, z którą urządzenie ma się łączyć (jego własna sieć działa zawsze).

Treść: `ssid=<name>&pass=<password>`.

- `ssid`: 1..32 bajty, bez znaków sterujących. **Pusta wartość (`ssid=`) = zapomnij sieć
  domową.** Brak pola `ssid` to błąd.
- `pass`: wymagane, 8..63 drukowalne znaki ASCII albo 64 cyfry szesnastkowe (gotowy klucz WPA2;
  64 znaki innego rodzaju dostają `400 bad_request`, `detail` `pass`). Sieci otwartej nie da się
  zapisać (urządzenie łączyłoby się z każdym punktem dostępowym o tej nazwie). Przy `ssid=` pole
  `pass` jest pomijane.

Odpowiedź: `{"queued":true,"applyInMs":1000}`. Błędy: `400 bad_request` (`detail`: `ssid` albo
`pass`), `401`, `403 ap_only`, `411`, `413`, `429`, `503 busy`.

Ustawienie jest zapisywane, gdy tylko zapytanie wyjdzie z kolejki. `applyInMs` po ostatnim takim
zapytaniu Wi-Fi urządzenia startuje od nowa: jego własna sieć znika na ok. 1..3 s (nie zmierzono)
i klient musi połączyć się ponownie. Po połączeniu z routerem sieć urządzenia działa na kanale
routera. Gdy auto potrzebuje urządzenia, zapis i restart czekają do końca tego stanu. Przy
pracującym silniku urządzenie nie próbuje łączyć się z siecią domową.
Wynik: `wifi.staConfigured`, `wifi.staConnected`, `wifi.staLastReason` w `/api/status`. Hasła sieci
domowej nie da się odczytać. Panel pokazuje ryzyko sieci domowej (1.2) przed zapisem.

### POST /api/password {#post-apipassword}

Zmiana hasła urządzenia (logowanie do panelu i klucz sieci Wi-Fi urządzenia).

Treść: `old=<current>&new=<new>`. Nowe: 10..63 drukowalne znaki ASCII (kody 33..126), bez spacji.

Odpowiedź: `{"queued":true,"applyInMs":1000}`. Błędy: `400 bad_request` (`detail`: `old` albo
`new`, gdy brak pola), `400 weak_password`, `403 wrong_password` (nie liczy się do blokady), `401`,
`403 ap_only`, `411`, `413`, `429`, `503 busy`, `503 not_ready` (urządzenie nie ma hasła).

Nowe hasło obowiązuje w panelu od chwili, gdy pętla główna wykona zapytanie (kilka ms po
odpowiedzi; później, gdy auto potrzebuje urządzenia). Panel używa nowego hasła zaraz po odpowiedzi
`queued`, a przy `401` próbuje jeszcze raz ze starym. Po `applyInMs` sieć urządzenia startuje od
nowa z nowym hasłem: każdy telefon i komputer musi połączyć się ponownie. Zapomniane hasło pokazuje
`wifi showpass` na USB; `wifi newpass` losuje nowe.

### POST /api/restart {#post-apirestart}

Restart urządzenia. Bez treści (treść, jeśli jest, jest pomijana).

Odpowiedź: `{"queued":true,"restartInMs":1500}`. `restartInMs` to NAJKRÓTSZY czas: gdy w międzyczasie
auto zacznie potrzebować urządzenia albo radio ma ACC (zapłon), restart czeka do końca tego stanu
(do wyłączenia zapłonu). Błędy: `409 car_busy`, `409 ota_pending` (restart przed potwierdzeniem
cofnąłby nowy program: do tego służy `/api/ota/rollback`), `409 restart_pending`, `401`,
`403 ap_only`, `429`, `503 not_ready`, `503 busy`.

Przed restartem urządzenie wyłącza wyjścia 12 V. W trybie `decoder` radio traci ACC na czas startu
(ok. 1..2 s, nie zmierzono). Po restarcie zmienia się `boot`. Sieć urządzenia startuje od nowa:
telefon albo komputer może nie połączyć się sam. Przy cichej magistrali i bez komputera na USB
urządzenie czeka 60 s od startu na klienta, potem wyłącza Wi-Fi i zasypia
(`firmware/lib/core/net_policy.h`); budzi je ruch na CAN albo odłączenie i podłączenie zasilania.
Panel po 20 s czekania podpowiada ponowne połączenie z siecią urządzenia.

## 6. Aktualizacja programu {#6-firmware-update}

Plik do wgrania: `.pio/build/<environment>/firmware.bin`. Scalony obraz, który zaczyna się od
adresu 0, jest odrzucany.

> [!WARNING]
> **Aktualizuj tylko na postoju**
>
> Zapis do flash na chwilę zatrzymuje odbiór CAN i może sprawić, że dźwięk będzie się rwał.
> Urządzenie odmawia aktualizacji, gdy włączony jest bieg wsteczny albo aktywne są czujniki
> parkowania, i przerywa wgrywanie, gdy taki stan pojawi się w jego trakcie (wykrycie może zająć
> 1..3 s, bo część ramek ginie w czasie zapisu). W czasie wgrywania i przez 3 s po nim urządzenie
> nie wyłącza zapłonu, ACC, podświetlenia ani wstecznego z powodu zgubionych ramek (ramka
> „zapłon wyłączony” nadal działa normalnie). Panel pyta `/api/ota/status` i pokazuje to
> ostrzeżenie, zanim wyśle plik.

**Zworka nadajnika CAN.** Obrazy nie są podpisane. Gwarancja sprzętowa, że żaden wgrany program
nie nada na magistralę auta, wymagałaby otwartej zworki nadajnika CAN (JP1 w prototypie).
W prototypie autora JP1 jest zamknięta, a SLNT jest podłączone do GPIO6, więc decyduje tylko wgrany
program. Przed pomyłką chronią bajt `canTx` znacznika i osobna zgoda (`mode=1`).

**Tryb pracy.** Obraz w trybie `decoder` wysyła ramki do radia i załącza wyjścia 12 V. Wgrywaj go
tylko przy wypiętej skrzynce CAN (dwa nadajniki na jednej linii i dwa źródła ACC to zwarcie
wyjść). Dlatego obraz w innym trybie niż działający wymaga parametru `mode=1`, a panel najpierw
zadaje osobne pytanie. Program wgrany z `mode=1` potwierdza się zaraz po pierwszym udanym starcie
(bez okresu `pending`), żeby utrata zasilania w trakcie przepinania nie przywróciła poprzedniego
trybu.

**Nadawanie na CAN (`canTx`).** Znacznik pliku mówi, czy program umie nadawać na CAN auta.
Urządzenie traktuje różnicę `canTx` między plikiem a działającym programem jak zmianę trybu:
`409 mode_change`, zgoda `mode=1`, potwierdzenie zaraz po starcie. Tylko obraz środowiska
`decoder_tx_wifi` ma `canTx` = 1. Po wgraniu startuje rozbrojony (uzbrojenie jest związane
z napisem kompilacji i nie przechodzi na inny program). To ochrona przed pomyłką, nie podpis. Gdy
zmienia się tryb, panel zgłasza sukces dopiero przy `otaState` = `valid` i do tego czasu pokazuje
„nie odłączaj zasilania”.

### Znacznik urządzenia w pliku {#the-device-marker-in-the-file}

Każdy program tego projektu zawiera w swoim `firmware.bin` jeden 72-bajtowy znacznik, w dowolnym
miejscu pliku (zwykle wśród stałych). Panel i makieta szukają go, zanim wyślą plik.

| Przesunięcie | Długość | Zawartość |
|---|---|---|
| +0 | 12 | bajt `7F`, 9 znaków `fwid_magic` z `project.json`, bajt `7F`, bajt `00` |
| +12 | 16 | produkt, ASCII, zakończony zerem: wartość `fw_product` |
| +28 | 16 | wersja, ASCII, zakończona zerem |
| +44 | 24 | czas kompilacji, ASCII, zakończony zerem |
| +68 | 1 | tryb: 0 `listen`, 1 `decoder` |
| +69 | 1 | sieć: 1 = program ma panel i aktualizację przez Wi-Fi |
| +70 | 1 | `canTx`: 1 = program umie nadawać na CAN auta, 0 = tylko nasłuch. W obrazach do 0.6.0 ten bajt był zarezerwowany i wynosi 0 |
| +71 | 1 | zarezerwowany (0) |

Traktuj napisy jako niezaufane (może im brakować zera). Skrót `elf` obrazu to bajty pliku
`0xB0..0xB7` zapisane jako 16 cyfr szesnastkowych wielkimi literami. Układ jest zdefiniowany
w `firmware/lib/core/fw_ident.h`.

Zasady panelu przed wysłaniem: brak znacznika albo inny produkt = odrzucenie w przeglądarce, nic
nie jest wysyłane; `net` = 0 = pytanie, potem `force=1`; tryb inny niż `sys.mode` z `/api/status`
= osobne pytanie, potem `mode=1`. Urządzenie wymaga `mode=1` także wtedy, gdy bajt `canTx` pliku
różni się od działającego programu (w obie strony). Panel sprawdza to przed wysłaniem: porównuje
„bajt +70 pliku nie jest 0” z `sys.canTx` (brak pola = `false`) i przy różnicy zadaje osobne
pytanie („Program z nadawaniem na CAN auta” / „Program bez nadawania na CAN auta”), potem
`mode=1`; tak samo przy cofnięciu (`other.canTx`). Panel porównuje wartości logiczne, urządzenie
porównuje surowe bajty: dwie różne wartości niezerowe przejdą przez panel bez pytania, a urządzenie
odrzuci je z `mode_change`. Urządzenie i tak sprawdza to samo po odebraniu pliku, więc przeglądarka
bez tego sprawdzenia nie obejdzie zabezpieczenia; nadpisze tylko na darmo drugie miejsce.

### POST /api/ota {#post-apiota}

Treść: surowy plik (bez `multipart`), nagłówek `Content-Length` wymagany.
Parametry adresu: `force=1` zgoda na obraz bez panelu sieciowego (po nim aktualizacje tylko kablem
i bez automatycznego cofnięcia), `mode=1` zgoda na obraz w innym trybie pracy albo z innym `canTx`.

W czasie wgrywania serwer nie odpowiada na żadne inne zapytanie. Przeglądarka liczy postęp
z bajtów, które wysłała (zdarzenia postępu wysyłania); urządzenie nie ma zapytania o postęp.

Odpowiedź po przyjęciu pliku:

```json
{"ok":true,"bytes":1048576,"version":"2.0.0-beta.1","build":"Nov 18 2026 10:02:11","mode":"listen","canTx":false,"net":true,
 "elf":"7B1E90C4A2D35F68","restartInMs":1500}
```

`version`, `build`, `mode`, `canTx`, `net` pochodzą ze znacznika przyjętego pliku; `elf` to skrót
nowego obrazu (bajty pliku `0xB0..0xB7`; ten sam, który po restarcie pokazują `sys.elf`
i `running.elf`). Po `restartInMs` (najwcześniej; gdy auto potrzebuje urządzenia albo radio ma ACC,
dopiero po końcu tego stanu, czyli po wyłączeniu zapłonu) urządzenie restartuje się z nowym
programem.

| Status | `error` | Kiedy | Dane pliku |
|---|---|---|---|
| 401 / 403 / 429 / 503 | `auth` / `ap_only` / `locked` / `not_ready` | logowanie | nieczytane, połączenie zamknięte (klient może zobaczyć zerwane połączenie zamiast odpowiedzi) |
| 411 | `length_required` | brak `Content-Length` | |
| 413 | `too_large` | plik większy niż `slotSize` | nieczytane, połączenie zamknięte |
| 400 | `bad_image`, `detail` `short` | plik krótszy niż 65 536 bajtów | przeczytane i wyrzucone |
| 409 | `ota_pending`, `restart_pending`, `car_busy` | zobacz 1.6 | przeczytane i wyrzucone |
| 503 | `not_ready` | brak migawki stanu | przeczytane i wyrzucone |
| 400 | `bad_image`, `detail` `header` | pierwsze 4096 bajtów to nie program ESP32-S3 (bajt 0 nie `E9`, bajty 12-13 nie `09 00`, bajty 32..35 nie `32 54 CD AB`) | reszta przeczytana i wyrzucona; nic nie zapisano |
| 409 | `car_busy` | wsteczny albo czujniki w czasie wgrywania | zapis przerwany, reszta wyrzucona |
| 500 | `flash` | błąd zapisu | zapis przerwany, reszta wyrzucona |
| 400 | `bad_image`, `detail` `verify` | suma kontrolna albo SHA-256 obrazu się nie zgadza | |
| 400 | `wrong_product` | brak znacznika tego urządzenia albo inny produkt (`force` tego nie zmienia) | |
| 409 | `no_net` | obraz bez panelu sieciowego i brak `force=1` | |
| 409 | `mode_change` | inny tryb pracy albo inne `canTx` niż w działającym programie i brak `mode=1` | |
| 403 | `ap_only` | ta sama zmiana z `mode=1`, ale od klienta spoza sieci urządzenia | nic nie przełączone |
| 409 | `car_busy` | wsteczny albo czujniki pojawiły się w czasie sprawdzania obrazu | nic nie przełączone |

Gdy połączenie zostanie zerwane, po trzech przerwach w odbiorze po 3 s z rzędu, po ponad 180 s
całego wgrywania albo gdy sieć się zatrzyma, urządzenie przerywa zapis i zamyka połączenie bez
odpowiedzi. W każdym przypadku błędu działający program zostaje nietknięty.
Uwaga: od chwili rozpoczęcia zapisu (po sprawdzeniu pierwszych 4096 bajtów) poprzedni program
w drugim miejscu jest nadpisywany; po późniejszym błędzie cofnięcie nie jest możliwe aż do
następnego udanego wgrania.

**Po restarcie.** Nowy program startuje w stanie `pending`. Gdy sterowniki wstaną, pętla główna
przepracuje 20 s, a sieć urządzenia i serwer działają, program sam się potwierdza
(`otaState` = `valid`). Potwierdzenie to zapis do flash, więc gdy auto potrzebuje urządzenia, czeka
do końca tego stanu (`pending` może wtedy trwać dłużej niż 20 s). Gdy przy starcie nie wstaną
sterowniki albo sieć, gdy sieć nie wystartuje w ciągu 60 s (liczone bez czasu z ACC; cofnięcie
czeka na wyłączenie zapłonu) albo gdy urządzenie się zawiesi (strażnik pętli) lub zrestartuje
przed potwierdzeniem, wraca poprzedni program. W stanie `pending` urządzenie nie zasypia i nie
przyjmuje kolejnego pliku. Wyjątek: program wgrany z `mode=1` potwierdza się od razu.

Panel po odpowiedzi `ok` pyta `GET /api/hello` (bez logowania) co 1 s, aż zmieni się `boot`, potem
`GET /api/ota/status` (z tym samym hasłem) i porównuje `running.elf` z wartością z odpowiedzi na
wgranie: równe = działa nowy program; różne = nastąpiło cofnięcie. `state` pokazuje `pending`,
a po ok. 20 s `valid`.

### GET /api/ota/status {#get-apiotastatus}

Błędy: `401`, `429`, `503 not_ready`. Pierwsze wywołanie po starcie urządzenia (i pierwsze po
każdym wgraniu) może trwać do ok. 2 s: urządzenie przegląda wtedy drugie miejsce na program.

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

| Pole | Typ | Znaczenie |
|---|---|---|
| `state` | napis | stan działającego programu: `none`, `valid`, `pending` (jak `sys.otaState`) |
| `slotSize` | liczba, bajty | największy plik, który można wgrać |
| `confirmInMs` | liczba | przy `pending`: czas do najwcześniejszego potwierdzenia; w przeciwnym razie 0 |
| `busy` | wartość logiczna | trwa wgrywanie (klient HTTP w praktyce widzi `false`) |
| `carBusy` | wartość logiczna | bieg wsteczny albo aktywne czujniki: wgrywanie, cofnięcie i restart byłyby teraz odrzucone |
| `restartPending` | wartość logiczna | restart jest zaplanowany |
| `restartReason` | napis | `none`, `user`, `ota`, `rollback` |
| `canRollback` | wartość logiczna | `POST /api/ota/rollback` ma do czego wrócić |
| `running.slot` | napis | miejsce działającego programu: `app0` albo `app1` |
| `running.version`, `.build`, `.mode`, `.canTx`, `.elf` | | jak `sys.version`, `sys.build`, `sys.mode`, `sys.canTx`, `sys.elf` w `/api/status` |
| `running.net` | wartość logiczna | zawsze `true` |
| `other.slot` | napis | drugie miejsce |
| `other.image` | wartość logiczna | w drugim miejscu jest jakiś program |
| `other.scanned` | wartość logiczna | jego zawartość została przejrzana; `false` = jeszcze nie (auto było zajęte), pozostałe pola `other` są nieznane, zapytaj ponownie |
| `other.known` | wartość logiczna | w drugim miejscu znaleziono znacznik programu tego projektu (obraz programu prototypowego daje `false`) |
| `other.bootable` | wartość logiczna | bootloader pozwala go uruchomić |
| `other.version`, `.build`, `.mode`, `.net` | napis / napis / napis / wartość logiczna, albo `null` | dane ze znacznika; `null`, gdy znacznika nie znaleziono |
| `other.canTx` | wartość logiczna albo `null` | bajt `canTx` znacznika w drugim miejscu nie jest 0; `null`, gdy znacznika nie znaleziono. Obraz 0.6.0 daje `false` |
| `other.elf` | napis albo `null` | jak `running.elf`, z opisu aplikacji w drugim miejscu; `null`, gdy miejsce jest puste, nadpisane albo jeszcze nieprzejrzane. Równe `running.elf` = ten sam obraz w obu miejscach |
| `last.result` | napis | ostatnie wgrywanie od startu urządzenia: `none`, `ok`, `error` |
| `last.error` | napis | słowo błędu ostatniego nieudanego wgrywania; puste, gdy nie było |
| `last.bytes` | liczba | bajty ostatniego odebranego pliku |

### POST /api/ota/rollback {#post-apiotarollback}

Powrót do poprzedniego programu. Bez treści. Parametry adresu: `force=1`, `mode=1` w tym samym
znaczeniu co przy wgrywaniu, sprawdzane względem programu w drugim miejscu (`other`).

- Stan `pending`: działający (nowy) program jest odrzucany i wraca poprzedni. `force` nie jest
  potrzebne; `mode=1` jest potrzebne, gdy program w drugim miejscu ma inny tryb pracy albo inne
  `canTx`.
- Inne stany: bootloader dostaje polecenie startu z drugiego miejsca. Wymaga `canRollback` =
  `true`.

Od chwili przyjęcia zapytania do restartu (albo niepowodzenia) drugie miejsce jest zajęte:
równoległe `POST /api/ota` albo drugie cofnięcie dostaje `409 restart_pending`.

Odpowiedź: `{"queued":true,"restartInMs":1500}`. Błędy: `409 no_rollback`, `409 no_net`,
`409 mode_change`, `409 car_busy`, `409 restart_pending`, `401`, `403 ap_only`, `429`,
`503 not_ready`, `503 busy`. Jeśli wykonanie w pętli głównej się nie uda, restartu nie ma, miejsce
jest zwalniane, a log pokazuje linię `[ota] rollback failed: ...`.

## 7. Wskazówki dla klienta {#7-hints-for-a-client}

- Odpytywanie: `/api/status` co 500 ms; `/api/log` co 1 s (zakładka Konsola: co 500 ms);
  `/api/frames` co 500 ms i `/api/table` co 1 s tylko przy otwartej zakładce CAN (ramki także
  w czasie nagrywania); `/api/keymap` co 1 s tylko przy otwartej zakładce Radio. Następne
  zapytanie tego samego rodzaju dopiero po odpowiedzi na poprzednie. Zakładka przeglądarki w tle
  nie odpytuje.
- Aktywność klienta (zapytania z poprawnym hasłem) utrzymuje urządzenie w czuwaniu po wyłączeniu
  auta: najwyżej 60 s od ostatniego zapytania i 10 min łącznie, i tylko przy wystarczającym
  napięciu. Zapytania bez logowania (`/`, `/api/hello`) się nie liczą.
- Zmiana `boot` w dowolnej odpowiedzi: odrzuć kursory (następne zapytanie bez `c`) i odśwież
  wszystko.
- Zerwane połączenie albo brak odpowiedzi to nie błąd logowania: panel pokazuje „brak połączenia”
  i dalej próbuje `GET /api/hello`.
- Nagrywanie ramek „bez limitu” robi przeglądarka: zbiera odpowiedzi `/api/frames` i zapisuje plik
  w formacie `/api/trace.log`, z domyślnie włączonym maskowaniem VIN.
- Przełącznik „maskuj VIN” (domyślnie włączony) dotyczy też EKRANU: dla ID `336`, `3B6`, `2B6`
  tabela i podgląd na żywo pokazują ID, liczbę i okres, a zamiast danych `••`. Zrzut ekranu
  zakładki CAN nie może ujawnić VIN.
- Każdy tekst z urządzenia (linie logu, nazwy sieci, wersje, `detail`) wstawiaj do strony tylko
  przez `textContent` (nigdy `innerHTML`): nazwę sieci albo linię konsoli mógł ustawić ktoś inny.
- Teksty panelu tylko ze słownika (`t()` w `firmware/web/app.js`). Słowa z API (`error`,
  `can.tx.state`, `settings.last.result`, `resetReason`...) tłumaczy panel; nieznane słowo jest
  pokazywane takie, jakie jest.
- Hasło w `sessionStorage` albo, z **Zapamiętaj na tym urządzeniu**, w `localStorage`; przy starcie
  panel czyta najpierw `sessionStorage`, potem `localStorage`. **Wyloguj** usuwa je z obu miejsc,
  `401` usuwa odrzucone hasło z obu miejsc. Po zmianie hasła (`POST /api/password`) panel od razu
  je podmienia, w tym samym miejscu.

## 8. Skąd program bierze pola `/api/status` {#8-where-the-firmware-takes-the-apistatus-fields}

Dla tego, kto edytuje `firmware/src/net/api_handlers.cpp`. `s` to kopia `StatusSnapshot`
z `diagGetStatus()` (`firmware/src/net/diag_types.h`); zadanie serwera nigdy nie dotyka obiektów
pętli głównej. Migawkę wypełnia `firmware/src/app_status.cpp`.

| Pole JSON | Źródło |
|---|---|
| `sys.version`, `build`, `mode`, `canTx` | `fwIdent()` z `src/version.h`; tryb przez `fwModeName()`; `canTx` = bajt znacznika różny od 0 |
| `sys.elf` | `otaRunningElf()` (`src/net/ota.h`): pierwsze 8 bajtów `esp_app_get_description()->app_elf_sha256` jako 16 cyfr szesnastkowych wielkimi literami, liczone raz. NIE `esp_app_get_elf_sha256()`, które w tej wersji zwraca najwyżej 9 znaków |
| `sys.uptimeMs` | `millis()` w chwili odpowiedzi |
| `sys.resetReason` | `s.resetReason` (`esp_reset_reason_t` 0..15) jako nazwa: 0 `unknown`, 1 `poweron`, 2 `ext`, 3 `sw`, 4 `panic`, 5 `int_wdt`, 6 `task_wdt`, 7 `wdt`, 8 `deepsleep`, 9 `brownout`, 10 `sdio`, 11 `usb`, 12 `jtag`, 13 `efuse`, 14 `pwr_glitch`, 15 `cpu_lockup` |
| `sys.heapFree`, `heapMin`, `heapLargest` | `ESP.getFreeHeap()`, `ESP.getMinFreeHeap()`, `ESP.getMaxAllocHeap()` |
| `sys.stackLoopFree`, `stackHttpFree` | `uxTaskGetStackHighWaterMark(loopTaskHandle)` i `(NULL)`; na tym porcie wynik jest w bajtach |
| `sys.tempC` | `temperatureRead()`; NaN daje `null` |
| `sys.otaState`, `otaBusy`, `restartPending`, `hubDrops` | `otaRunningState()`, `otaBusy()`, `otaRestartPending()`, `diagDrops()` |
| `sys.clientAp` | `httpPeerIsAp(req)` w chwili zapytania (nie z migawki) |
| `can.listenOnly` | `s.canListenOnly` (`canListenOnly()` w `src/can_bus.cpp`) |
| `can.tx.*` | `s.canTxState` jako `TxState` (`built` = nie `NOT_BUILT`, `armed` = nie `NOT_BUILT` i nie `DISARMED`, `state` = `txStateName()`), `s.canTxFault` (`txFaultName()`, 0 daje `null`), liczniki, `armNext` = `s.canTxArmNext` |
| `car.ignitionRaw`, `doors`, `batteryV`, `outsideTempC` | `null`, gdy `s.ignitionFresh`, `s.doorsKnown`, `s.batteryV >= NET_VBAT_VALID_MIN_V`, `s.haveOutsideTemp` jest fałszywe |
| `car.settings.*` | `s.set260*`, `s.set38`, `s.setLast*`, `s.setAccepted`, `s.setNotAccepted`, `s.setRefused`, `s.setMask`; `last.result` = `settingCmdResultName()` (`lib/app/decoder.h`) |
| wszystkie wieki (`ageMs`) | `millis()` pobrane PO skopiowaniu migawki minus znacznik czasu; wynik ujemny = 0 |
| `radio.time` | przy `s.radio.haveTime`: `20YY-MM-DD hh:mm`, w przeciwnym razie `null` |
| `audio.volumeMax` | stała `AUDIO_VOLUME_MAX` |
| `wifi.blocked`, `lanAdmin`, `hostname` | `s.wifi.blockedByFaults`, `settingsLanAdmin()`, stała `NET_HOSTNAME` |
| `wifi.staIp`, `staRssi` | `null` bez połączenia |
| `log.*`, `trace.*` | `diagHeads()` |

`GET /api/keymap`: `map` z `s.keyMap`, `defaults` z `xp::KeyCustomizer::defaultEntry()`,
`max` = `xp::KeyCustomizer::MAP_SIZE`. `GET /api/ota/status`: `running.elf` = `otaRunningElf()`,
pozostałe pola z `otaGetStatus()`.

## 9. Wymagania dla makiety (`firmware/tools/mock_device.py`) {#9-requirements-for-the-mock-firmwaretoolsmock_devicepy}

Makieta odtwarza nagranie CAN i odpowiada pod tymi samymi adresami w tym samym formacie, więc nad
panelem można pracować bez płytki. Musi zachować:

- logowanie Basic z użytkownikiem `admin` (`401` i `429` w podanym formacie, bez
  `WWW-Authenticate`; zasady blokady z 1.2; opcja `--lan` „klient z sieci domowej”, w której
  zapytania zmieniające dostają `403 ap_only`);
- losowy `boot` przy starcie, numerowanie kursorów z `next` / `head` / `dropped` i limity pamięci
  (200 linii, 2048 ramek, 400 ramek na odpowiedź, **64 linie i 4096 bajtów tekstu na odpowiedź
  `/api/log`**), kolejność elementów tablic, słowa błędów z 1.6, kolejkowanie poleceń
  (`{"queued":true}`), odrzucanie linii `wifi ...` w `/api/cmd` oraz zmianę `boot` po
  `/api/restart` i po przyjęciu pliku w `/api/ota`;
- opcję `--can-tx` (działający program ma `canTx` = 1, `can.tx.built` = `true`): makieta pokazuje
  przełącznik uzbrojenia, przyjmuje `cantx`, `cantx on|off`, `vset on|off NN` (z `--lan`:
  `403 ap_only`), odrzuca `cantx reset` i `set NN VV` (`usb_only`), uzbraja się po symulowanym
  restarcie i rozbraja po przyjęciu innego programu. Niczego nie nadaje: liczniki `can.tx` zostają
  na zerze;
- **plik w `/api/ota`:** makieta sprawdza nagłówek (bajt 0 = `E9`, bajty 12..13 = `09 00`, bajty
  32..35 = `32 54 CD AB`), szuka znacznika z rozdziału 6 i zwraca te same błędy (`bad_image`,
  `wrong_product`, `no_net`, `mode_change`) oraz te same pola odpowiedzi; `elf` z bajtów
  `0xB0..0xB7`. Po symulowanym restarcie `sys.elf` i `running.elf` mają tę wartość, a `sys.mode`
  tryb ze znacznika;
- **pola `/api/status`** dekodowane z odtwarzanego nagrania według tych samych zasad co
  w programie: `lib/car_psa_aee2010/psa_frames.*` (`0E1`, `0F6`, wsteczny, zapłon, światła `036`),
  `lib/car_psa_aee2010/psa_signals.cpp` (drzwi `1A1`), `lib/core/power_policy.cpp` (ACC, limity
  świeżości), `psa_signals.cpp` z `lib/radio_xp/xp_signals.cpp` (`car.settings.raw260`, `ageMs`,
  `state38`; makieta nie ma radia, więc `last` = `null`, a liczniki są 0). Stałe mogą być:
  `sys.*` oprócz `uptimeMs` i `boot`, `radio.*`, `audio.*`, `wifi.*`;
- **VIN:** przy wczytaniu nagrania makieta nadpisuje dane ramek `336`, `3B6`, `2B6` (na przykład
  zerami) i nigdy nie pokazuje ich prawdziwej treści. Nagrania dodawane do repozytorium jako dane
  makiety muszą być zamaskowane już w pliku (`tools/sanitize_capture.py`);
- **`GET /`:** makieta serwuje `web/index.html` z wstawionymi `app.css` i `app.js`, tak samo jak
  `embed_web.py` (gzip opcjonalny), z tym samym nagłówkiem `Content-Security-Policy`;
- **`GET /i18n/<lang>.json`:** te same słowniki co program, gzip, z `ETag` i `304`;
- **restart:** zaplanowany restart czeka tylko, aż auto przestanie potrzebować urządzenia. Makieta
  NIE czeka na wyłączenie ACC (program: `netRestartDue`): odtwarzane nagranie prawie cały czas ma
  włączony zapłon, więc restart nigdy by nie nastąpił;
- **`/api/hello`:** `locked` > 0 tylko w trybie `--lan` (tak jak program robi dla klientów sieci
  urządzenia); `lang` = język zapisany przez `POST /api/lang`, polecenie konsoli `lang` albo import;
- **ustawienia:** `POST /api/lang`, `GET` / `POST /api/settings` z tymi samymi sprawdzeniami
  i szczegółami błędów co program (parser to uproszczona kopia w Pythonie).

Nie musi odtwarzać czasów blokady, zapisów do flash ani odmów `car_busy`.

Uruchom ją poleceniem `python -I tools/mock_device.py <recording.log>` w `firmware/` i otwórz
`http://127.0.0.1:8080/`. Przy starcie wypisuje swoje hasło (albo użyj `--password`). Inne opcje:
`--port`, `--host`, `--speed`, `--mode decoder`, `--lan`, `--lan-admin`, `--can-tx`, `--no-loop`,
`--verbose`. Użyj nagrania, które ma ruch od początku, na przykład
`test_data/captures/can_20261111_191045.log`.

## 10. Tabele dla panelu {#10-tables-for-the-panel}

Źródło prawdy: `firmware/lib/app/xp_keys.cpp`,
`firmware/lib/car_psa_aee2010/profile_psa_aee2010.cpp` i `firmware/lib/radio_xp/protocol_xp.cpp`
(kody przycisków) oraz `firmware/lib/radio_xp/xp_keys_custom.h` (funkcje radia). Gdy te pliki się zmienią, popraw też tę
tabelę. Ramki po stronie auta pochodzą z auta wzorcowego; ich status opisuje
[Peugeot 508 SW](vehicles/peugeot-508.md).

**Przyciski auta** (kod wysyłany do radia przez skrzynkę, czyli `FROM` w tabeli zamian):

| Kod | Przycisk | Ramka CAN |
|---|---|---|
| `14` | głośność + | `21F` |
| `15` | głośność − | `21F` |
| `16` | oba przyciski głośności naraz (wyciszenie) **oraz** wciśnięcie pokrętła zasilania na panelu | `21F` / `036` |
| `30` | telefon | `21F` |
| `50` | LIST | `21F` |
| `07` | wciśnięcie pokrętła kierownicy | `21F` |
| `17`, `18` | pokrętło kierownicy: następny / poprzedni (jeden ząbek) | `21F` |
| `21` | SRC | `0A2` |
| `23` | przycisk komputera pokładowego (dźwignia wycieraczek) | `221` |
| `9E` | NAV | `122` |
| `A0` | MEDIA | `122` |
| `97`, `98` | przewiń w lewo / w prawo | `122` |
| `9C`, `9D` | lupa + / lupa − | `122` |
| `9F` | TEL | `122` |
| `99` | SETUP | `122` |
| `9B` | DRIVE | `122` |
| `A4` | księżyc (wyłączenie ekranu; obsługiwany osobno, zamiana nie działa) | `122` |
| `A1` | wyciszenie mikrofonu | `122` |
| `A3` | WEB | `122` |
| `22` | przycisk panelu, którego nazwy jeszcze nie potwierdzono | `122` |
| `81`, `82` | pokrętło panelu: w prawo / w lewo (jeden ząbek) | `122` |

Kod `16` należy do dwóch przycisków: zamiana działa dla obu naraz.

To, który fizyczny przycisk wysyła ramkę `0A2` (SRC albo ten między SRC a LIST), notatki projektu
opisują na dwa różne sposoby: do sprawdzenia w aucie.

**Funkcje radia** (`TO` w tabeli zamian; wypróbowane w aucie wzorcowym przy radiu ustawionym na
profil CAN „XP Simple Soft → 508 → Type 2”, zmierzone przez obserwację radia):
`08` zmiana źródła, `1F` asystent głosowy (Siri w CarPlay), `30` zmiana źródła, `9A` jasność
ekranu, `9B` telefon, `A5` ekran klimatyzacji, `A6` wstecz, `B5` FM/AM, `B9` i `BF` oba zapisane
jako strojenie w dół o 0,05 MHz (jeden z nich do ponownego sprawdzenia). Bez reakcji: `B1`,
`91`..`96`. Panel przyjmuje też każdy inny kod szesnastkowy.

**Źródło dźwięku** (`radio.source`, bajty 0..1, zmierzone na linii UART radia wzorcowego):
`01 01` radio, `08 13` odtwarzacz (dalej: numer utworu, liczba utworów, minuty, sekundy), `0C 30`
pozostałe; widziano też `05 40` i `07 30` (znaczenie nieznane).

**Nazwy identyfikatorów CAN** w zakładce CAN: z `vehicles/peugeot-508-sw-2015/508.dbc` (nazwy
komunikatów), skopiowane do `firmware/web/profiles/psa_aee2010.json` (pole `frames`; opisy jako
klucze słownika `psa.frame.*`). Ten plik ma też tabele przycisków auta (`car_keys`), funkcji
radia (`radio_fn`), bitów drzwi (`doors`) i wartości bajtu zapłonu (`ignition`); ich teksty są
w słownikach `firmware/web/i18n/*.json`.
