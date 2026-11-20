# Architektura

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Jak program jest podzielony na części, jak przechodzi przez niego jedna ramka CAN i dlaczego jest
zbudowany właśnie tak. Przeczytaj to, zanim napiszesz profil auta albo zmienisz kod. Ścieżki są
względne wobec `firmware/` w repozytorium.

## Ogólny obraz {#the-big-picture}

Program ma dwa rodzaje kodu:

- **logika** w `lib/`: zwykłe C++ bez żadnego nagłówka Arduino ani ESP-IDF. Dostaje ramki i czas w
  milisekundach, a zwraca decyzje (bajty dla radia, stany wyjść 12 V, dźwięk). Te same pliki są
  kompilowane na komputerze i testowane na nagraniach z auta
  ([Testy na komputerze](../testing/host-tests.md));
- **sprzęt** w `src/`: kontroler CAN, UART do radia, wyjścia 12 V, dźwięk, ustawienia we flash
  (NVS), konsola USB, Wi-Fi i panel WWW.

Dwa wybory zapadają przy kompilacji, nigdy w czasie pracy:

- **profil auta** (`-DCAR_PROFILE=...`): co znaczą ramki CAN twojego auta;
- **protokół radia** (`-DRADIO_PROTOCOL=...`): jak rozmawiać z twoim radiem.

Piny pochodzą z trzeciego wyboru, **nagłówka płytki** (`-DBOARD=...`, pliki w `boards/`). Zobacz
[Konfiguracja i piny](config-and-pins.md).

## Biblioteki w lib/ {#libraries-in-lib}

Każda biblioteka to jeden katalog. Jej nazwa określa warstwę.

| Biblioteka | Warstwa | Co jest w środku |
|---|---|---|
| `lib/core` | core | logika niezależna od auta i radia: typ ramki CAN, parser candump, neutralne typy sygnałów (`vehicle_signals.h`), struktury profilu i protokołu (`car_profile.h`, `radio_protocol.h`), sesja z radiem wspólna dla wszystkich protokołów (`radio_session.*`: kolejka, wysyłanie przy zmianie, powtórzenia), generator piknięć czujników parkowania, zasady zasilania radia (ACC, bieg wsteczny), brama nadawania CAN i uzbrajanie (`can_tx_gate.*`, `can_tx_arm.*`), znacznik tożsamości programu, pomocnicze funkcje panelu WWW (zapis JSON, logowanie, bufor linii, ślad ramek), drobne zasady pętli i sieci |
| `lib/car_psa_aee2010` | car | profil wzorcowy (Peugeot 508 SW 2015): ID CAN, dekodowanie ramek, dane auta dla ekranu radia (`psa_signals.*`), stan auta, ustawienia pojazdu (`260`, `15B`, `361`), sekwencja zmiany ustawień, lista ramek dozwolonych do nadawania |
| `lib/car_generic` | car | punkt wyjścia dla nieznanego auta: nic nie dekoduje, brak przycisków, pusta lista dozwolonych ramek, wyjścia 12 V nigdy włączone |
| `lib/car_example` | car | skomentowany szablon z **wymyślonymi** ramkami. Kompilowany tylko przez testy na komputerze, nigdy do programu urządzenia |
| `lib/radio_xp` | radio | protokół XP: ramkowanie i parser (`xp_protocol.*`), obiekt protokołu `kRadioXp` z ramkami zasilania, przycisków, przeszkody i wersji, dekodowaniem ramek radia i tabelami protokołu (`protocol_xp.*`), ramka czujników parkowania, tabela kodów przycisków, kodery danych auta dla ekranu radia (`xp_signals.*`), tabela zamiany przycisków, nazwy poleceń radia, `RadioLink` (wspólna sesja z XP, dla wzorca 0.6.0 i testów) |
| `lib/radio_none` | radio | bez radia: bez UART, nic nigdy nie jest wysyłane |
| `lib/radio_example` | radio | skomentowany szablon protokołu z **wymyślonym** ramkowaniem i ramkami. Kompilowany tylko przez testy na komputerze, nigdy do programu urządzenia |
| `lib/app` | app | spoiwo: obiekt `Decoder` oraz części, które potrzebują obu stron (tabela przycisków w `xp_keys.*`). `xp_car.*` i `xp_settings.*` to wcześniejsze funkcje PSA -> XP, zostawione dla zamrożonego wzorca 0.6.0 i testów na komputerze |

`Decoder` (`lib/app/decoder.h`) zawiera całą logikę jednego działającego urządzenia. `src/main.cpp`
tworzy jeden `Decoder`, przekazuje mu wybrany profil i protokół i karmi go danymi.

### Dwie struktury {#the-two-structures}

Profil auta to jedna struktura `const`, `vehicle::CarProfile` (`lib/core/car_profile.h`),
wkompilowana do pamięci flash:

| Pole | Znaczenie |
|---|---|
| `id` | krótka nazwa, na przykład `"psa_aee2010"`, wypisywana na konsoli |
| `canBitrate` | prędkość magistrali w bit/s. `src/can_bus.cpp` zna 50, 100, 125, 250, 500 i 1000 kbit/s; każda inna wartość oznacza, że kontroler nie jest uruchamiany (stan bezpieczny) |
| `physicalLayer` | high-speed albo low-speed fault-tolerant, tylko informacyjnie. Program nie przełącza transceiverów. Oba dostarczone profile mają `UNKNOWN` |
| `keys`, `knobs` | przyciski (ramka, bajt, maska, wartość -> przycisk logiczny) i pokrętła (licznik 8-bitowy -> dwa przyciski). Kolejność wierszy = kolejność dopasowania |
| `txAllow` | ramki, które brama nadawania może wysłać, każda z długością i kontrolą treści napisaną w C++. Pusta dla profilu tylko do nasłuchu |
| `privacyIds` | ramki, które identyfikują auto (na przykład VIN), maskowane przed udostępnieniem nagrania |
| `decodeParking`, `decodeBody`, `decodeLights` | funkcje, które zamieniają ramkę w neutralny sygnał: czujniki parkowania, zapłon i bieg wsteczny, światła. `nullptr` = to auto tego nie dostarcza |
| `powerOutputs` | `true` = zapłon jest dekodowany na tyle dobrze, że można sterować wyjściami 12 V w trybie dekoder |
| `keyRemapDefaults` | wpisy początkowe tabeli zamian przycisków radia (kody XP), przywracane przez `map reset`. Ma je tylko profil wzorcowy |
| `decodeSteering`, `decodeTrip`, `decodeOutsideTemp`, `decodeEcoMode`, `decodeClimate`, `decodeDoors`, `decodeSettings` | dane auta dla ekranu radia: funkcje, które zamieniają ramkę w typ neutralny (`SteeringAngle`, `TripData`, `OutsideTemp`, `EcoMode`, `ClimateState`, `DoorState`, `VehicleSettings` w `vehicle_signals.h`). Każdy typ ma znacznik ważności. Funkcje z argumentem `io` dostają stan przechowywany przez `Decoder` i zmieniają tylko to, co mówi ramka. `nullptr` = to auto tego nie podaje |

Protokół radia to jedna struktura `const`, `RadioProtocol` (`lib/core/radio_protocol.h`): `id`,
`uartBaud` (0 = w ogóle bez UART), `keyCode`, tabela z przycisku logicznego na kod, którego
oczekuje radio, oraz kodery danych auta (`encodeSteering`, `encodeTrip`, `encodeOutsideTemp`,
`encodeEcoMode`, `encodeClimate`, `encodeCarState`, `encodeSettingsPresent`). Koder dostaje typ
neutralny i zapisuje jedną ramkę dla radia; nigdy nie widzi ramki CAN. `nullptr` = radio nie
pokazuje tych danych. `encodeCarState` dostaje z powrotem swoją poprzednią ramkę, więc pole, którego
auto nie podało, zachowuje ostatnią wartość. Reszta struktury to łącze i sesja: `encodeFrame` (jedna
ramka do bajtów, z nagłówkiem, długością i sumą kontrolną protokołu; `nullptr` = nic nie jest
wysyłane), `rxParser` (parser bajtów od radia), `sendAck` i `ackByte` (bajt odsyłany po każdej
poprawnej ramce radia), `txGapMs`, ramki zasilania, przycisków, czujników parkowania
(z `parkingRepeatMs`), przeszkody i wersji, `decodeRadioFrame` (stan radia dla konsoli i panelu oraz
prośba „wyślij wszystko”), `decodeCommand` (polecenia z radia; `nullptr` = żadne nie dociera do
strony CAN) oraz dwie tabele `const`: `latchKeys` (przyciski z zatrzaskiem, XP: `A4`) i `rateLimits`
(ramki stanu wysyłane najwyżej raz na podany czas, XP: kąt skrętu `29`, 150 ms). Wspólna sesja
(`lib/core/radio_session.h`) i `Decoder` używają tylko tych pól.

Wybór odbywa się w `src/config.h`:

| Flaga kompilacji | Profil albo protokół |
|---|---|
| `-DCAR_PROFILE=PSA_AEE2010` (domyślnie) | `lib/car_psa_aee2010` |
| `-DCAR_PROFILE=GENERIC` | `lib/car_generic` (używany przez `listen_generic_wifi`) |
| `-DCAR_PROFILE=CUSTOM` | twój własny profil, podany przez `CAR_PROFILE_CUSTOM_HEADER` i `CAR_PROFILE_CUSTOM_OBJECT`. Tylko do nasłuchu |
| `-DRADIO_PROTOCOL=XP` (domyślnie) | `lib/radio_xp` |
| `-DRADIO_PROTOCOL=NONE` | `lib/radio_none` |
| `-DRADIO_PROTOCOL=CUSTOM` | twój własny protokół, podany przez `RADIO_PROTOCOL_CUSTOM_HEADER` i `RADIO_PROTOCOL_CUSTOM_OBJECT`. Na CAN tylko nasłuch |

`-DCAR_PROFILE=EXAMPLE` i `-DRADIO_PROTOCOL=EXAMPLE` celowo zatrzymują kompilację: przykłady to
szablony, a nie auto i radio. Jak napisać własny profil:
[Pisanie profilu auta](../make-it-yours/write-a-profile.md); własny protokół radia:
[Przyciski i radio](../make-it-yours/radio-mapping.md#radio-protocols-available).

## Jak przechodzi jedna ramka CAN {#how-one-can-frame-travels}

Jeden przebieg pętli głównej (`loop()` w `src/main.cpp`) wykonuje te kroki w tej kolejności:

1. **Odbiór.** `canReceive()` opróżnia kolejkę odbiorczą kontrolera CAN (`src/can_bus.cpp`,
   kolejka 128 ramek). Dla każdej ramki działa `handleFrame()`:
    - panel WWW dostaje kopię do podglądu na żywo (tylko kompilacje z Wi-Fi);
    - przy włączonym `dump` ramka jest wypisywana na USB jako `(time) can0 ID#DATA`;
    - `Decoder::onCanFrame()` dostaje ramkę.
2. **Dekodowanie.** W `onCanFrame()` profil zamienia ramkę w neutralne sygnały: przyciski z tabel
   przycisków i pokręteł, czujniki parkowania, zapłon i bieg wsteczny, światła oraz dane auta dla
   ekranu radia (kąt skrętu kierownicy, komputer pokładowy, temperatura zewnętrzna, tryb
   ekonomiczny, klimatyzacja, drzwi, ustawienia pojazdu). Dla danych auta `onCanFrameCarData()`
   przekazuje każdą neutralną wartość koderowi protokołu radia. Nie zna przy tym żadnego ID CAN ani
   żadnej ramki radia.
3. **Praca konsoli i panelu.** Obsługiwane są polecenia z USB, opóźnione zapisy ustawień i
   zapytania panelu WWW, z wyjątkiem chwil, gdy okno nadajnika CAN jest otwarte (zobacz
   [Nadawanie na magistralę CAN](../safety/transmit.md)).
4. **Takt.** `Decoder::tick()` decyduje w tej kolejności: zdarzenia przycisków -> zasilanie radia
   (ACC, podświetlenie, bieg wsteczny) -> ramki dla radia -> dźwięk czujników parkowania. Wynik
   zwraca do `main.cpp`.
5. **Wyjścia.** `main.cpp` ustawia wyjścia 12 V, ale tylko w trybie dekoder, przy działającym
   sterowniku CAN i z profilem, który ma `powerOutputs = true`. W przeciwnym razie wszystkie wyjścia
   są wyłączone.
6. **Radio.** `radioUartPoll()` (`src/radio_uart.cpp`) czyta bajty z radia przez parser protokołu,
   odpowiada na każdą kompletną ramkę potwierdzeniem protokołu (XP: `F3`), przekazuje ją do
   `Decoder::onRadioFrame()` i wysyła następną ramkę z `Decoder::nextRadioTx()`, w ramkowaniu
   protokołu (co najmniej `txGapMs` między ramkami, XP: 10 ms). Przy radiu `none` nic nie jest
   wysyłane.
7. **Dźwięk, dioda, sen.** Głośnik gra to, o czym zdecydował generator piknięć; urządzenie zasypia
   po 30 s ciszy na magistrali, gdy do USB nie jest podłączony komputer.

Polecenie z radia idzie w drugą stronę: `onRadioFrame()` -> `decodeCommand` protokołu (XP: parser
poleceń radia) -> dla ustawienia pojazdu (XP `80 nn vv`) moduł zmiany ustawień -> brama nadawania CAN -> port sprzętowy w
`src/can_tx/`. W każdej kompilacji bez nadawania brama nie ma portu i odmawia. Szczegóły:
[Nadawanie na magistralę CAN](../safety/transmit.md).

## Zasady warstw {#layer-rules}

`tools/check_layering.py` sprawdza linie `#include` każdego pliku źródłowego w `lib/` i `include/`
(dowolne rozszerzenie poza dokumentacją i `library.json`; najpierw usuwa komentarze). Uruchom go poleceniem
`python -I tools/check_layering.py` w `firmware/`; kod wyjścia 0 oznacza brak naruszeń.

| Warstwa | Może dołączać |
|---|---|
| `core` | tylko `core` |
| `car_*` | `core` i siebie |
| `radio_*` | `core` i siebie |
| `app` | wszystko w `lib/` |

Skrypt zgłasza błąd także wtedy, gdy:

- biblioteka radia dołącza plik auta (w szczególności `can_ids.h`) albo biblioteka auta dołącza
  plik radia lub `app`;
- plik w `lib/` dołącza nagłówek frameworka albo SDK (`Arduino.h`, `esp_*.h`, FreeRTOS ...);
  dozwolone są tylko nagłówki standardowe C oraz pliki z `lib/` i `include/`;
- plik w `lib/` dołącza plik z `src/` albo spoza programu;
- ta sama nazwa nagłówka istnieje w dwóch bibliotekach (mają wspólną ścieżkę dołączania);
- dołączenie idzie przez makro (celu nie da się sprawdzić);
- nagłówek w `include/` dołącza coś innego niż `core`, `include/` albo nagłówek standardowy C
  (podlega zasadom `core`, więc nie przekaże dalej nagłówka auta ani radia);
- kod używa nazw drugiej strony (bez komentarzy i napisów, łącznie z liniami `#define`): słowo `xp`,
  `BOX_*` albo `RADIO_*` w `lib/car_*`; słowa `psa` albo `vset`, `ID_*` albo `CanFrame`
  w `lib/radio_*`; słowa `xp`, `psa` albo `vset` w `lib/core` i `include/` (jeden sprawdzony
  wyjątek: aliasy starych nazw w `lib/core/compat/legacy_060.h`);
- tor radia w `Decoder` (`lib/app/decoder.*`) używa nazw ramkowania, parsera albo numerów ramek XP
  (`xp::encode`, `xp::Parser`, `xp::BOX_*`, `xp::RADIO_*`, `parseRadioCmd` ...): każda ramka do
  radia i od radia idzie przez `RadioProtocol`.

Skrypt nie śledzi dołączeń dalej niż o jeden krok i nie sprawdza ID CAN zapisanych jako zwykłe
liczby w `lib/radio_*`. Pilnuj ich ręcznie; przegląd kodu nadal jest potrzebny.

## Decyzje projektowe {#design-decisions}

| Decyzja | Dlaczego |
|---|---|
| Wszystko wybierane przy kompilacji, profile są `const` | w czasie pracy nie da się namówić programu na inne auto ani inną listę nadawania. Jedyna tabela zmieniana w czasie pracy to tabela zamiany przycisków radia |
| Bez pamięci dynamicznej, bez `String`, bez `delay()` w pętli | urządzenie pracuje godzinami w aucie; stałe bufory nie mogą się pofragmentować ani wyczerpać |
| Logika bez nagłówków sprzętowych | ten sam kod działa na komputerze na prawdziwych nagraniach, więc zmiana jest sprawdzona, zanim trafi do auta |
| Jedno miejsce, które może nadawać na CAN (`src/can_tx/can_tx_port.cpp`), kompilowane tylko w `decoder_tx_wifi` | pozostałe pięć środowisk w ogóle nie zawiera funkcji nadawania (`tools/check_no_can_tx.sh` sprawdza zbudowane programy) |
| Rozbrojone = kontroler tylko w trybie nasłuchu, uzbrojone = nadajnik zamknięty poza jedną ramką | urządzenie pozostaje niewidoczne dla auta poza kilkoma milisekundami wysyłania. Zobacz [Nadawanie na magistralę CAN](../safety/transmit.md) |
| Sterownik, który nie wystartuje, zostawia urządzenie w stanie bezpiecznym (wyjścia 12 V wyłączone, czerwona dioda), bez pętli restartów | radio i auto nie mogą dostawać impulsów ACC |
| Format linii USB `can0 ID#DATA` i linie komentarza zaczynające się od `#` są stałe | czytają je narzędzia; zobacz [Wersjonowanie](versioning.md) |
| Strumień XP do radia jest porównywany bajt po bajcie z nagraniami oryginalnej skrzynki | testy wzorcowe trzymają zachowanie bez zmian, gdy kod jest przebudowywany ([Pliki wzorcowe](../testing/golden-files.md)) |

## Co jeszcze nie jest ogólne {#what-is-not-generic-yet}

Podział na profile jest zrobiony ostrożnie, tak żeby strumień do radia pozostał taki sam. Podział w tej wersji:

- przez profil auta i protokół radia przechodzą przyciski, czujniki parkowania, zapłon, światła,
  prędkość magistrali, lista ramek dozwolonych do nadawania oraz dane auta dla ekranu radia (kąt
  skrętu kierownicy, komputer pokładowy, temperatura zewnętrzna, tryb ekonomiczny, klimatyzacja,
  drzwi, ustawienia pojazdu);
- wartości komputera pokładowego i kąt skrętu zachowują kodowanie auta wzorcowego: ich znaczenie i
  skala nie są sprawdzone, więc typy neutralne nie mogą jeszcze podać jednostek;
- zmiana ustawienia pojazdu nie jest ogólna: polecenie radia `80`, ramka `15B` / `1A9` i sprawdzenie
  w ramce `260` zostają kodem PSA w `Decoder` i w `settings_changer.*`, za bramką nadawania. Stan auta, który czytają (`CarModel`, ramki `260`, `361`, `0F6`), działa
  tylko z zestawem ramek PSA;
- strona radia idzie przez protokół radia (ramkowanie, parser, potwierdzenie, wszystkie ramki,
  przyciski z zatrzaskiem i ograniczenia częstości), ale znaczenie poleceń z radia nie jest ogólne:
  numery ustawień z `decodeCommand` to numery XP z tabeli ustawień pojazdu, dlatego protokół radia
  `CUSTOM` kompiluje się tylko bez nadawania na CAN (polecenia klimatyzacji i dźwięku tylko wypisują
  linię w konsoli);
- typy zdarzeń przycisków i tabela zamian przycisków mają nadal nazwy `xp::` (`xp::KeyEncoder`,
  `xp::KeyCustomizer`), choć działają z kodami każdego protokołu;
- domyślne zamiany przycisków (`21` -> `1F`, `9F` -> `9B`) są w profilu PSA (`keyRemapDefaults`) jako
  kody XP: to jedyne miejsce, w którym profil auta podaje kody radia;
- sekwencja zmiany ustawień (`settings_changer.*`) jest w bibliotece profilu PSA, a nie w `core`;
- kod sprzętowy w `src/` (CAN, port nadawania, UART radia, pętla główna) nie ma testu na
  komputerze. Jest sprawdzany na stole ([Protokół testu na stole](../testing/bench-test-protocol.md)).

Który przycisk wysyła ramkę `0A2` (SRC czy ten między SRC a LIST): notatki projektu dają dwie
różne odpowiedzi i trzeba to jeszcze sprawdzić w aucie.
