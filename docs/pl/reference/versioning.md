# Wersjonowanie

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Inne programy i ludzie zależą od kilku stałych formatów tego projektu: narzędzia, które czytają
konsolę USB, panel WWW, makieta urządzenia, aktualizacja programu przez Wi-Fi. Ta strona wymienia te
formaty (**umowy**), numery wersji, które je chronią, i to, co liczy się jako zmiana niezgodna
wstecz.

## Numery wersji {#version-numbers}

| Numer | Gdzie | Teraz | Znaczenie |
|---|---|---|---|
| wersja programu | `FW_VERSION` w `firmware/src/version.h` | `2.0.0` | [Semantic Versioning](https://semver.org/spec/v2.0.0.html) `MAJOR.MINOR.PATCH`, z opcjonalną częścią przedwydania (`-beta.N`, `-rc.N`); najwyżej 15 znaków, tylko cyfry, litery, `.` i `-` |
| wersja API panelu | `FW_API_VERSION` w `firmware/src/version.h`, pole `api` odpowiedzi `GET /api/hello` | `1` | jedna liczba całkowita; podnoszona tylko przy zmianie, która psuje klientów |
| napis kompilacji | `__DATE__ " " __TIME__` z `firmware/src/version.cpp` | np. `Nov 18 2026 01:33:28` | odróżnia kompilacje; jest z nim związane uzbrojenie nadawania na CAN |
| skrót obrazu | pole `elf` w `/api/status` i `/api/ota/status` | 16 cyfr szesnastkowych | pierwsze 8 bajtów SHA-256 pliku ELF; inny dla każdego zbudowanego obrazu |

Numer wersji jest w `version.h`, a nie w `platformio.ini`: zmiana `platformio.ini` przebudowuje
wszystko, zmiana `version.h` tylko `src/`.

`firmware/tools/build_time.py` kompiluje ponownie `version.cpp` zawsze, gdy coś się zmieniło w
`src/`, `lib/` albo `web/`, więc napis kompilacji to czas ostatniej kompilacji ze zmienionymi
źródłami. Do odróżnienia dwóch obrazów tej samej wersji używaj skrótu obrazu, a nie napisu
kompilacji.

> [!NOTE]
> **Pierwsze publiczne wydanie to 2.0.0**
>
> Wersje programu do `1.0.0` to własna historia autora w aucie wzorcowym (prototyp, zanim projekt
> dostał nazwę). Pierwsze publiczne wydanie to `2.0.0`: nowy MAJOR, bo to drzewo zmieniło umowy
> prototypu (zobacz `CHANGELOG.md`, format:
> [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)). Przygotowano je jako kandydata do
> wydania `2.0.0-rc.1`.

Część przedwydania (na przykład `2.0.0-rc.1`) widać wszędzie, gdzie jest wersja: linia konsoli
`[ver] <product> 2.0.0-rc.1 listen, built ...`, znaczek wersji i karta System w panelu, komentarz
startowy `# version: ...` i nazwy plików wydania (`<environment>-2.0.0-rc.1.bin`, tworzone przez
`.github/workflows/release.yml`, który wymaga też znacznika `v2.0.0-rc.1`). Nic w programie, panelu
ani makiecie nie porównuje dwóch wersji, więc część przedwydania nie wymaga osobnej obsługi.

## Umowy {#the-contracts}

### 1. Format linii konsoli USB {#1-usb-console-line-format}

Czytają go panel (zakładka Konsola), `firmware/tools/record.py` (zapisuje linie konsoli przez
`/api/log`) i każdy, kto zapisuje port USB.

- surowa ramka CAN: `(seconds.milliseconds) can0 ID#DATA`, sekundy bez zer wiodących, milisekundy
  zawsze 3 cyfry, ID 3 cyfry szesnastkowe, dane 0..16 cyfr szesnastkowych;
- komentarz: linia zaczynająca się od `#`;
- linia ze znacznikiem: `[tag] ...`, na przykład `[stat]`, `[park]`, `[vset]`, `[cantx]`.

Moduł konsoli (`firmware/src/console.h`) niczego do tekstu nie dodaje i niczego nie zmienia. Nie są
w nim mieszane makra `ESP_LOGx`: linie błędów bibliotek ESP-IDF są przechwytywane i wypisywane jako
komentarze `# idf: ...`.

Ten sam format `(s.mmm) can0 ID#DATA` mają nagrania w `firmware/test_data/captures/`,
`/api/trace.log` i nagranie z panelu. Linie komentarza w tych plikach: `# masked`,
`# lost N frames`, `# device restart`.

Teksty linii ze znacznikiem są przypięte testami: wzorce strumienia w `firmware/test_host/golden/`
zawierają linie konsoli, które logika wypisuje przy odtwarzaniu nagrań (`*.console.txt`). Zmieniona
linia pokazuje się jako błąd testu; zobacz [Pliki wzorcowe](../testing/golden-files.md).

Nazwy poleceń też są częścią umowy. Stare (polskie) nazwy zostają jako aliasy: `ust`,
`wifi haslo`, `wifi nowehaslo` (zobacz [Konsola USB](console.md#old-command-names)). Polecenie
`climate` i jego alias `klima` usunięto w 2.0.0 razem z próbą klimatyzacji.

### 2. API panelu {#2-panel-api}

Opisane na stronie [API panelu](panel-api.md). Zasady:

- dodanie pola do odpowiedzi, nowego słowa błędu albo nowego zapytania jest **zgodne**: wersja API
  się nie zmienia. Klient musi pomijać pola, których nie zna, a nieznane słowa pokazywać bez zmian;
- usunięcie pola, zmiana jego nazwy albo nadanie mu innego znaczenia **psuje** klientów: podnieś
  `FW_API_VERSION`;
- pole dodane później jest na stronie API oznaczone wersją, która je wprowadziła, a klient musi
  traktować brakujące pole tak, jak tam opisano (zwykle jako `false` albo „brak danych”).

`/api/hello` zwraca `api`. Panel tylko pokazuje je na ekranie logowania i sprawdza nazwę
produktu; ani panel, ani makieta nie porównują jeszcze `api` ze znaną wersją.

Pola dodane do tej pory, żeby klient mógł rozmawiać ze starszym programem:

| Program | Dodane |
|---|---|
| 0.7.0 | bajt `canTx` w znaczniku programu; `sys.canTx`; `mode_change` także przy różnicy `canTx` |
| 0.8.0 | `car.settings`; panel sprawdza `canTx` pliku, zanim go wyśle; `radio.txGated`, `radio.slotsFull` |
| 0.9.0 | `can.tx` (brama nadawania) |
| 1.0.0 | `sys.clientAp`, `can.tx.armNext`, `car.settings.mask`, `car.settings.last.frame`; środowisko `decoder_tx_wifi`; kontrole `cantx` i `vset on/off` w `/api/cmd` |
| 2.0.0-beta.1 | zmieniona nazwa: `other.centralka` w `GET /api/ota/status` to teraz `other.known` (klient prototypu i tak się nie połączy: zmieniła się nazwa produktu). `api` zostaje `1`, pierwsze API tego produktu |

### 3. Znacznik programu (FwIdent) {#3-firmware-marker-fwident}

Struktura o długości 72 bajtów w każdym obrazie (`firmware/lib/core/fw_ident.h`, układ na stronie
[API panelu](panel-api.md#the-device-marker-in-the-file)). Aktualizacja przez Wi-Fi szuka jej we
wgrywanym pliku i odrzuca plik bez niej albo z innym produktem.

- Ciąg początkowy to bajt `7F`, 9 znaków `fwid_magic`, bajt `7F`, bajt `00`.
- Produkt to wartość `fw_product` (najwyżej 15 znaków).
- W obrazie może być dokładnie jedna kopia ciągu początkowego.
- Nowe pola mogą tylko wykorzystać bajt zarezerwowany albo powiększyć strukturę na końcu;
  przesunięcia istniejących pól zostają bez zmian.

### 4. Tożsamość projektu {#4-project-identity}

`project.json` to jedyne miejsce na nazwę projektu i wartości z niej wyprowadzone. Część z nich to
**wartości zgodności**:

| Wartość | Co od niej zależy |
|---|---|
| `fw_product`, `fwid_magic` | aktualizacja przez Wi-Fi: urządzenie odrzuca obraz z innym produktem albo innym magic, więc po zmianie pierwsza aktualizacja musi pójść przez USB |
| `nvs_namespace` | zapisane ustawienia: inna przestrzeń nazw startuje z pustymi ustawieniami (nowe hasło, brak sieci domowej, brak zamian przycisków) |
| `web_session_key` | tylko zalogowana karta przeglądarki (użytkownik loguje się ponownie) |
| `ap_ssid_prefix`, `mdns_host` | nazwa sieci urządzenia i adres `.local` |

Zmieniaj je tylko przez `tools/set_project_name.py` (przepisuje też
`firmware/include/project_identity.h` i linię tożsamości w `firmware/web/app.js`). Jego `--dry-run`
wymienia wszystkie pozostałe miejsca, w których nadal są stare wartości, łącznie z dokumentacją.
Logo i pozostałe grafiki pokazują nazwę jako rysunek: narzędzie ich nie zmienia
([Logo i ikona](../contributing/brand.md)).

### 5. Zapisane ustawienia {#5-stored-settings}

Klucze NVS wymienione na stronie
[Konfiguracja i piny](config-and-pins.md#settings-stored-on-the-device). Nowa wersja musi czytać
klucze starszej albo bezpiecznie je pomijać. Nieprawidłowa zapisana wartość wraca do domyślnej (na
przykład nieprawidłowa tabela przycisków daje wbudowane zamiany i linię
`# stored key swap table is invalid: the fixed entries apply`).

Uzbrojenie nadawania na CAN jest zapisywane jako napis kompilacji uzbrojonego programu. Jest ważne
tylko dla tej konkretnej kompilacji: każdy nowo wgrany program startuje rozbrojony. Tak jest celowo
i to nie może się zmienić.

### 6. Środowiska kompilacji {#6-build-environments}

Nazwy środowisk (`listen`, `decoder`, `listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`,
`listen_generic_wifi`) występują w dokumentacji, skryptach i ścieżce pliku
`.pio/build/<environment>/firmware.bin`. Zmieniaj nazwę środowiska tylko w wersji głównej (MAJOR) i
zostaw starą nazwę w komentarzu w `platformio.ini`. Środowisko domyślne musi pozostać środowiskiem
tylko do nasłuchu.

## Co idzie do której części wersji {#what-goes-into-which-part-of-the-version}

| Zmiana | Część |
|---|---|
| psuje się jedna z umów wyżej (linia konsoli czytana przez narzędzia zmienia postać, pole API zostaje usunięte, zmienia się układ znacznika, zapisany klucz jest czytany inaczej) | MAJOR (a dla API także `FW_API_VERSION`) |
| nowa funkcja, polecenie, pole API albo profil auta, które zachowują umowy | MINOR |
| poprawka, która zachowuje zachowanie i formaty | PATCH |

Zmiana tego, co urządzenie **wysyła** (do radia, na wyjściach 12 V albo na CAN), nigdy nie jest
poprawką PATCH: wymaga wpisu w `CHANGELOG.md`, a dla CAN także wpisu na stronie
[Nadawanie na magistralę CAN](../safety/transmit.md).
