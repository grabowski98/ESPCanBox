# Przyciski i radio

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Profil auta mówi, **które bity CAN to który przycisk**. Protokół radia mówi, **jaki kod dostaje
radio dla tego przycisku**. Pomiędzy nimi jest lista logicznych nazw przycisków, `KeyId`. Ta
strona objaśnia te trzy części i pokazuje, jak zmienić działanie przycisku w radiu, z ponowną
kompilacją albo bez niej.

## Trzy tabele {#three-tables}

```text
CAN frame ──► car profile ──► logical button ──► radio protocol ──► key code ──► remapping ──► radio
              keys / knobs      KeyId             keyCode()          e.g. 14       map FROM TO
```

| Część | Plik | Zależy od |
|---|---|---|
| bity CAN -> `KeyId` | twój profil, tabele `keys` i `knobs` ([Pisanie profilu auta](write-a-profile.md)) | auta |
| `KeyId` -> kod przycisku | protokół radia, `firmware/lib/radio_xp/protocol_xp.cpp` (`xp::keyCode`) | protokołu radia |
| kod przycisku -> inny kod przycisku | tabela zamian, w czasie pracy, zapisana w ustawieniach urządzenia | twojego gustu |

Profil auta nie zna kodów radia, a protokół radia nie zna ID ramek CAN.
Sprawdza to `tools/check_layering.py`.

## Dostępne protokoły radia {#radio-protocols-available}

| Flaga kompilacji | Protokół | Co robi |
|---|---|---|
| `-DRADIO_PROTOCOL=XP` (domyślnie) | `xp` | protokół skrzynki CAN Simple Soft („XP”) do radia Android na platformie FYT, UART 38400 8N1. Używane są tylko polecenia zaobserwowane na przewodzie: [Protokół XP](../reference/xp-protocol.md) |
| `-DRADIO_PROTOCOL=NONE` | `none` | bez radia: UART nie startuje, żaden przycisk nie ma kodu, nic nigdy nie jest wysyłane |
| `-DRADIO_PROTOCOL=CUSTOM` | twój | własny protokół w `firmware/lib/radio_<nazwa>`, podany przez `RADIO_PROTOCOL_CUSTOM_HEADER` i `RADIO_PROTOCOL_CUSTOM_OBJECT` (wzór: `firmware/lib/radio_example`). Na CAN tylko nasłuch: kompilacja z nadawaniem kończy się błędem |

`listen_generic_wifi` używa `none`. Twój własny profil w środowisku `listen_mycar` używa `xp`, ale
w trybie „obok skrzynki” i tak nic nie jest wysyłane do radia.

> [!NOTE]
> **Własny protokół radia**
>
> Wszystko, co dotyczy jednego radia, jest w jednej strukturze `const`, `RadioProtocol`
> (`firmware/lib/core/radio_protocol.h`): prędkość UART, ramkowanie ramki do bajtów
> (`encodeFrame`), parser bajtów od radia (`rxParser`), bajt potwierdzenia, odstęp między ramkami,
> ramki zasilania, przycisków, czujników parkowania, przeszkody i wersji, kodery danych auta dla
> ekranu radia, dekodowanie własnych ramek radia oraz dwie tabele: przyciski z zatrzaskiem
> (`latchKeys`) i ramki stanu z ograniczeniem częstości (`rateLimits`). Jeśli twoje radio ma jeden
> numer ramki „dane auta” ze stroną lub podtypem w `DATA[0]`, ustaw `stateKeyBytes = 1`, żeby strony
> nie nadpisywały się w sesji. Część wspólna
> (`firmware/lib/core/radio_session.h`: kolejka, wysyłanie przy zmianie, powtórzenia, odstęp)
> i dekoder nie znają numeru ramki żadnego protokołu, więc twój protokół dostaje na linii do radia
> własne ramkowanie; sprawdza to test hosta `radio_example` na wzorze
> `firmware/lib/radio_example` (radio wymyślone, kompilowane tylko w testach hosta). Skopiuj wzór do
> `lib/radio_<nazwa>`, zmień nazwy plików i obiektu, zastąp ramkowanie i ramki ramkami swojego
> radia i zbuduj z `-DRADIO_PROTOCOL=CUSTOM` (przykładowa sekcja
> w `firmware/platformio_override.sample.ini`). Polecenia z radia (`decodeCommand`) są tylko
> zgłaszane: jeśli protokół nie ustawia `commandsUseVsetNumbers` (ustawia je tylko XP), dekoder nie
> interpretuje numerów ustawień, klimatyzacji ani dźwięku i wypisuje `[radio] command ... not
> supported`. Protokół `CUSTOM` nie kompiluje się z nadawaniem na CAN, bo zmiana ustawień pojazdu
> idzie według numerów ustawień XP. Domyślne zamiany przycisków profilu auta działają tylko
> z protokołem wskazanym w jego `keyRemapProtocol` (profil wzorcowy: `xp`). Nadal po stronie XP,
> niezależnie od protokołu: panel WWW (nazwy ramek i ramki VIN profilu PSA, źródło radia pokazane
> kodami źródeł XP, wiersz ustawień „ramka 38”). Nowy protokół trzeba napisać z własnych pomiarów
> łącza z radiem, nie z dekompilowanych aplikacji ani firmware skrzynek.

## Przyciski logiczne i kody XP {#logical-buttons-and-xp-codes}

Nazwy opisują przyciski auta wzorcowego (Peugeot 508 SW 2015). Dla swojego auta użyj nazwy
o najbliższym znaczeniu. Kod XP to to, co radio dostaje w ramce przycisku `20`
(DATA[0] = kod, DATA[1] = 1 wciśnięty, 2 przytrzymany, 0 puszczony). To ramka UART: jej bajty
danych liczy się od DATA[0], jak na stronie [Protokół radia XP](../reference/xp-protocol.md).
Bajty CAN na pozostałych stronach liczy się od bajtu 1.

| `KeyId` | Przycisk w aucie wzorcowym | Kod XP |
|---|---|---|
| `VOLUME_UP` | głośność + na kierownicy | `14` |
| `VOLUME_DOWN` | głośność - na kierownicy | `15` |
| `MUTE` | oba przyciski głośności naraz | `16` |
| `PHONE` | przycisk telefonu na kierownicy | `30` |
| `LIST` | LIST na kierownicy | `50` |
| `WHEEL_KNOB_PRESS` | wciśnięcie pokrętła kierownicy | `07` |
| `SOURCE` | SRC | `21` |
| `TRIP` | przycisk komputera pokładowego na dźwigni wycieraczek | `23` |
| `NAV` | panel: NAV | `9E` |
| `MEDIA` | panel: MEDIA | `A0` |
| `SCROLL_LEFT` | panel: przewiń w lewo | `97` |
| `ZOOM_IN` | panel: lupa + | `9C` |
| `SCROLL_RIGHT` | panel: przewiń w prawo | `98` |
| `TEL` | panel: TEL | `9F` |
| `SETUP` | panel: SETUP | `99` |
| `DRIVE` | panel: DRIVE | `9B` |
| `MOON` | panel: wygaszenie ekranu (księżyc) | `A4` |
| `ZOOM_OUT` | panel: lupa - | `9D` |
| `MIC_MUTE` | panel: wyciszenie mikrofonu | `A1` |
| `WEB` | panel: WEB | `A3` |
| `PANEL_EXTRA` | przycisk panelu, nierozpoznany | `22` |
| `PANEL_KNOB_PRESS` | wciśnięcie pokrętła zasilania na panelu | `16` (ten sam kod co `MUTE`) |
| `NEXT` / `PREV` | pokrętło kierownicy, jeden ząbek | `17` / `18` |
| `PANEL_KNOB_UP` / `PANEL_KNOB_DOWN` | pokrętło panelu, jeden ząbek w prawo / w lewo | `81` / `82` |

Status: **zmierzone** (measured). Kody to to, co skrzynka CAN Simple Soft wysyłała radiu dla
każdego przycisku, nagrane razem z magistralą CAN na wspólnym zegarze (nagrania
`can_20261111_190817.log` i `can_20261111_191045.log` w `firmware/test_data/captures/`; źródło
każdej stałej w `radios/xp-simple-soft/provenance.md`). Przycisk, który wysyła ramkę `0A2`
(`SOURCE`), jest w notatkach auta wzorcowego opisany na dwa sposoby („SRC” i „przycisk między
SRC a LIST”); trzeba to jeszcze raz sprawdzić w aucie.

Jeśli twoje auto ma przycisk bez pasującej nazwy, nową nazwę można dodać do `enum class KeyId`
w `firmware/lib/core/vehicle_signals.h`, **tylko na końcu listy**, żeby numery istniejących nazw
się nie zmieniły. Potrzebuje ona też kodu w protokole radia, inaczej nigdy nie zostanie wysłana.

## Domyślne zamiany pochodzą z profilu auta {#default-remappings-come-from-the-car-profile}

Tuż przed wysłaniem ramki przycisku do radia program stosuje małą tabelę zamian
(`firmware/lib/radio_xp/xp_keys_custom.h`). Jej wpisy początkowe pochodzą z profilu auta
(pola `keyRemapDefaults` i `keyRemapDefaultCount`, zobacz [Pisanie profilu auta](write-a-profile.md)).
Profil wzorcowy `psa_aee2010` ma dwa, dobrane dla auta wzorcowego i jego radia:

| Z | Na | Dlaczego (zmierzone: próba autora w aucie, jedno radio) |
|---|---|---|
| `9F` (`TEL`) | `9B` | otwiera ekran telefonu; `9F` bez podłączonego telefonu nic nie robił |
| `21` (`SOURCE`) | `1F` | asystent głosowy (Siri w CarPlay) |

To preferencje właściciela, nie fakty o magistrali: inne auto może nie mieć tych przycisków.
Profile `generic` i `example` nie mają domyślnych wpisów, podobnie profil skopiowany z szablonu,
więc każdy przycisk dociera do radia z własnym kodem. Pokazuje to test szablonu:
`[key] code=21 state=1` (SRC idzie jako `21`).

Żeby mieć domyślną zamianę we własnym profilu, dopisz wiersz do jego tabeli `keyRemapDefaults`.
Żeby usunąć zamianę w działającym urządzeniu, użyj `map 21 21` w konsoli USB albo sekcji zamiany
przycisków w panelu WWW.

Przycisk księżyca (`A4`) jest obsługiwany poza tabelą: jest w tabeli przycisków z zatrzaskiem
protokołu XP (`RadioProtocol::latchKeys`), więc pierwsze naciśnięcie wysyła tylko „wciśnięty” (radio
wygasza ekran), drugie tylko „puszczony”, a przy wyłączeniu zapłonu zatrzaśnięty przycisk dostaje
„puszczony”. Zamiana dla `A4` nie działa. Inny protokół podaje własne przyciski z zatrzaskiem albo
żadnych.

## Zmiana przypisania bez ponownej kompilacji {#change-a-mapping-without-rebuilding}

Polecenia konsoli USB ([Konsola USB](../reference/console.md)):

| Polecenie | Co robi |
|---|---|
| `map` | wypisuje tabelę, na przykład `[key] swap 9F -> 9B` |
| `map FROM TO` | kod przycisku `FROM` wysyła do radia `TO`, oba szesnastkowo: `map 9B 1F`. `map 9B 9B` usuwa wpis |
| `map reset` | powrót do domyślnych wpisów profilu auta (w `generic` brak) |
| `keys` | włącza albo wyłącza wypisywanie każdego zdarzenia przycisku (`[key] printing=1` / `0`) |

Tabela mieści najwyżej 16 wpisów i jest zapisana w ustawieniach urządzenia, więc przetrwa
restart. Panel WWW edytuje tę samą tabelę.

`FROM` to kod, który protokół radia nadaje przyciskowi (trzecia kolumna wyżej), a nie wartość
z CAN.

## Sprawdź, co kod robi w twoim radiu {#find-out-what-a-code-does-in-your-radio}

W programie w trybie dekoder możesz z konsoli USB wysłać do radia dowolny kod przycisku, bez
udziału auta:

| Polecenie | Co wysyła |
|---|---|
| `key 1F` | kod `1F` wciśnięty, potem puszczony |
| `kd A4` | tylko „wciśnięty” |
| `ku A4` | tylko „puszczony” |

W programie tylko do nasłuchu konsola odpowiada `: not sent (listen mode)` i nic nie wychodzi.

Co niektóre kody robiły w radiu auta wzorcowego, sprawdzone poleceniem `key` (zmierzone:
próba autora, jedno radio, profil CAN „XP Simple Soft -> Peugeot 508 -> Type 2”):

| Kod | Reakcja |
|---|---|
| `08` | zmiana źródła |
| `1F` | asystent głosowy (Siri w CarPlay) |
| `9A` | jasność ekranu |
| `9B` | telefon |
| `A5` | ekran klimatyzacji |
| `A6` | wstecz |
| `B5` | FM / AM |
| `B9`, `BF` | oba: strojenie w dół o 0,05 MHz (dwa kody z tą samą notatką; do ponownego sprawdzenia) |
| `B1`, `91` do `96` | brak reakcji |

Inne radio albo inny profil CAN w ustawieniach radia może reagować inaczej. Wypróbuj każdy kod na
swoim radiu i zapisz, co zrobił.

## Co jeszcze trafia do radia z twoim profilem {#what-else-goes-to-the-radio-with-your-profile}

Z protokołem XP i profilem innym niż wzorcowy radio dostaje tylko te ramki (zmierzone: test na
komputerze profilu szablonu, `firmware/test_host/expected/example_car.txt`):

| Polecenie XP | Treść | Z twojego profilu |
|---|---|---|
| `EA` | stan zasilania (ACC) | `decodeBody` (zapłon) |
| `20` | przycisk | `keys`, `knobs` |
| `32` | obraz czujników parkowania | `decodeParking` |
| `3E` | znacznik przeszkody | `decodeParking` |

Dane auta dla ekranu radia pochodzą z opcjonalnych funkcji twojego profilu:

- `38` drzwi i wartości ustawień: `decodeDoors`, `decodeSettings`;
- `3C` które wiersze ustawień pokazać: `decodeSettings`;
- `33`, `34`, `35` strony komputera pokładowego: `decodeTrip`;
- `36` temperatura zewnętrzna: `decodeOutsideTemp`;
- `44` tryb ekonomiczny: `decodeEcoMode`;
- `21` stan klimatyzacji: `decodeClimate`;
- `29` kąt skrętu kierownicy: `decodeSteering`.

Ramka, której funkcja w twoim profilu to `nullptr`, nigdy nie jest wysyłana. Kodery XP są
w `firmware/lib/radio_xp/xp_signals.h`. Zmiana ustawień pojazdu z radia (polecenie `80`) działa
tylko z profilem wzorcowym.

Dalej: [Lista kontrolna przeniesienia](porting-checklist.md).
