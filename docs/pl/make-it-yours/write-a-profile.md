# Pisanie profilu auta

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

**Profil auta** mówi programowi, jak czytać jedno auto: prędkość magistrali, które bity to
przyciski przy kierownicy, gdzie są zapłon, bieg wsteczny, światła i czujniki parkowania. To jedna
stała struktura w C++ (`vehicle::CarProfile`, zdefiniowana w `firmware/lib/core/car_profile.h`),
wkompilowana w program. Profil wybierasz przy budowaniu programu; nie da się go zmienić w trakcie
pracy urządzenia.

Ta strona prowadzi od zdekodowanych ramek do profilu, który się kompiluje. Następna strona,
[Testowanie profilu](test-your-profile.md), sprawdza go na nagraniu na twoim komputerze.

Każdy krok tej strony wypróbowano na kopii repozytorium z wymyślonym autem. Tam, gdzie coś poszło
źle, strona o tym mówi i pokazuje komunikat, który zobaczysz.

> [!CAUTION]
> **Własny profil na magistrali auta tylko słucha**
>
> Profil zbudowany z `-DCAR_PROFILE=CUSTOM` nigdy nie może nadawać na magistralę CAN auta.
> Środowisko z nadawaniem `decoder_tx_wifi` odmawia kompilacji z takim profilem i zatrzymuje się
> z komunikatem
> `CAN transmit is not allowed with CAR_PROFILE=CUSTOM (listen-only; add a named profile in src/config.h)`.
> Nie próbuj tego obchodzić. Nadawanie dla nowego auta wymaga zmierzonych ramek, sprawdzania
> treści zapisanego w kodzie i testów: zob. [Nadawanie na magistralę CAN](../safety/transmit.md).

## Co obejmuje profil {#what-a-profile-covers}

Wyjaśnij to sobie, zanim zaczniesz. Profil niesie tylko część tego, co urządzenie robi w aucie
wzorcowym (Peugeot 508 SW 2015, profil `psa_aee2010`).

| Funkcja | Przez twój profil | Uwagi |
|---|---|---|
| Prędkość magistrali | tak | `canBitrate` |
| Przyciski przy kierownicy i na panelu, pokrętła | tak | tabele `keys` i `knobs` |
| Stan zapłonu i silnika, bieg wsteczny | tak | funkcja `decodeBody`; steruje ACC i wyjściem wstecznego |
| Światła włączone | tak | funkcja `decodeLights`; steruje wyjściem podświetlenia |
| Czujniki parkowania i pikanie z głośnika urządzenia | tak | funkcja `decodeParking` |
| Lista dozwolonych ramek | **nie**: pole zostaje puste (`nullptr, 0`) | profil `CUSTOM` nie może nadawać (zob. wyżej) |
| Ramki, które niosą VIN | wpisane, tylko informacyjnie | `privacyIds`; program z tego nie korzysta |
| Drzwi, bagażnik i maska | tak | funkcja `decodeDoors` -> `vehicle::DoorState` |
| Komputer pokładowy | tak, z ograniczeniem | funkcja `decodeTrip` -> `vehicle::TripData`. Wartości zachowują kodowanie auta wzorcowego (ich znaczenie nie jest sprawdzone): wypełniaj je tylko wtedy, gdy twoje auto używa tego samego |
| Temperatura zewnętrzna, tryb ekonomiczny | tak | funkcje `decodeOutsideTemp`, `decodeEcoMode` |
| Stan klimatyzacji | tak | funkcja `decodeClimate` -> `vehicle::ClimateState` |
| Kąt skrętu kierownicy | tak, z ograniczeniem | funkcja `decodeSteering`; skala niesprawdzona w aucie wzorcowym |
| Ustawienia pojazdu (to, co pokazuje radio) | tak | funkcja `decodeSettings` -> `vehicle::VehicleSettings` (pozycje z neutralnymi identyfikatorami) |
| Zmiana ustawienia pojazdu z radia | **nie** | tylko profil wzorcowy `psa_aee2010`, za bramą nadawania |

Dane auta dla ekranu radia (od drzwi do ustawień pojazdu) są opcjonalne: zostaw wskaźnik
`nullptr`, a radio nigdy nie dostanie tych danych. Każda funkcja wypełnia typ neutralny
z `firmware/lib/core/vehicle_signals.h`; protokół radia zamienia go na własną ramkę. Wylicza je
podrozdział „Dane auta dla ekranu radia” w kroku 3.

Z protokołem radia XP profil innego auta daje więc radiu: stan zasilania (ACC), wciśnięcia
przycisków, obraz czujników parkowania, znacznik przeszkody i każdą funkcję danych auta, którą
profil wypełni. Zmiana ustawień pojazdu z radia zostaje przy aucie wzorcowym.

## Zanim zaczniesz {#before-you-start}

Potrzebujesz:

- prędkości transmisji, przewodów CAN i rodzaju magistrali twojego auta
  ([Znajdź swoją magistralę](find-your-bus.md));
- nagrania ze znacznikami ([Nagrywanie magistrali](record.md));
- zdekodowanych ramek i bitów ([Dekodowanie ramek](decode.md)). Zapisz każdy z jego statusem,
  **zmierzone** (measured) albo **niezweryfikowane** (unverified), w kopii
  `vehicles/_template/README.md`;
- narzędzi do budowania ze strony [Wgraj program](../quick-start/flash.md): `uv` oraz, na
  Windowsie, Git Bash.

Nie potrzebujesz wszystkich sygnałów. Profil z samym zapłonem już się przydaje: może przełączać
ACC radia. Pomiń wszystko, czego nie zmierzyłeś.

## Krok 1: skopiuj szablon {#step-1-copy-the-template}

Szablon `firmware/lib/car_example/` opisuje wymyślone auto, z komentarzem przy każdym polu.
Kompilują go tylko testy na komputerze, nigdy nie trafia do programu urządzenia.

> [!NOTE]
> **Dwa wymyślone auta**
>
> Szablon używa ramek `100`, `200`, `300` i `400`. Przykłady na tej stronie i na stronie
> [Testowanie profilu](test-your-profile.md) pochodzą z **drugiego** wymyślonego auta, „auta
> z samouczka” (ramki `2C0`, `1F0`, `350`, `2C8`), wymyślonego przy sprawdzaniu tych stron. Jego
> nagrania nie ma w repozytorium; fragmenty pokazują tylko, jak wyglądają twoje własne wartości.
> W swojej kopii zastępujesz wartości szablonu ramkami swojego auta.

Wybierz krótką nazwę profilu, małymi literami, na przykład `mine`. Potem otwórz Git Bash
w katalogu `firmware`: w Eksploratorze otwórz ten katalog, kliknij prawym przyciskiem i wybierz
„Open Git Bash here” (w Windows 11 pod „Pokaż więcej opcji”). Uruchom:

```bash
cp -r lib/car_example lib/car_mine
cd lib/car_mine
mv profile_example.h profile_mine.h
mv profile_example.cpp profile_mine.cpp
cd ../..
```

Nazwy plików muszą być unikalne w całym `firmware/lib/`: wszystkie biblioteki mają jedną wspólną
ścieżkę dołączania, a `tools/check_layering.py` zgłasza nazwę nagłówka, która występuje dwa razy.

## Krok 2: zmień nazwy w plikach {#step-2-rename-inside-the-files}

Otwórz trzy pliki w edytorze tekstu i zmień nazwy:

| Gdzie | Z | Na |
|---|---|---|
| `profile_mine.cpp`, pierwszy `#include` | `"profile_example.h"` | `"profile_mine.h"` |
| oba pliki | `kCarExample` | `kCarMine` |
| oba pliki | `namespace example` i `example::` | `namespace mine` i `mine::` |
| `profile_mine.cpp`, pierwsze pole profilu | `"example"` | `"mine"` |
| `library.json`, pole `name` | `"car_example"` | `"car_mine"` |
| `library.json`, pole `description` | tekst szablonu | jedna linia o twoim aucie |

> [!WARNING]
> **Nie zapomnij o `library.json`**
>
> PlatformIO rozpoznaje bibliotekę po polu `name` w `library.json`, nie po nazwie katalogu.
> Środowiska `listen` pomijają bibliotekę o nazwie `car_example` (`lib_ignore`
> w `firmware/platformio.ini`). Ze starą nazwą kompilacja kończy się komunikatem
> `Warning! Could not find the package with 'car_mine' requirements`, a potem
> `fatal error: profile_mine.h: No such file or directory`.

## Krok 3: wypełnij profil {#step-3-fill-in-the-profile}

Pracuj w tej kolejności:

1. W `profile_mine.h` ustaw cztery ID ramek `ID_BODY`, `ID_KEYS`, `ID_PARKING` i `ID_LIGHTS`
   na ramki swojego auta. ID, którego funkcję ustawisz na `nullptr` (punkt 4), nie jest czytane.
2. W `profile_mine.cpp` przepisz funkcje `decodeBody`, `decodeParking` i `decodeLights`
   (sekcje poniżej).
3. W tym samym pliku wypełnij tabele `KEYS` i `KNOBS` (sekcje poniżej).
4. Na końcu pliku wypełnij stałą profilu (tabela poniżej). Jej pola nie mają w kodzie nazw,
   liczy się tylko kolejność, więc zostaw komentarz przy każdej linii.

Chcesz sprawdzić kopię i zmiany nazw, zanim zmienisz jakąkolwiek wartość? Najpierw zrób próbę
na sucho ze strony [Testowanie profilu](test-your-profile.md).

Pola stałej profilu, po kolei:

| Pole | Co tam wpisać |
|---|---|
| `id` | krótka nazwa ASCII, pokazywana w konsoli (`# profile: car=mine radio=xp`) i w panelu |
| `canBitrate` | zmierzona prędkość transmisji w bit/s |
| `physicalLayer` | `vehicle::PhysicalLayer::UNKNOWN`, `HIGH_SPEED` albo `LOW_SPEED_FT`. Tylko informacyjnie: o transceiverze decyduje płytka. Zostaw `UNKNOWN`, dopóki tego nie zmierzysz |
| `keys`, `keyCount` | tabela przycisków (niżej); `nullptr, 0`, gdy brak |
| `knobs`, `knobCount` | tabela pokręteł (niżej); `nullptr, 0`, gdy brak |
| `txAllow`, `txAllowCount` | `nullptr, 0`. Profil `CUSTOM` nigdy nie nadaje |
| `privacyIds`, `privacyCount` | ID ramek, które w twoim aucie niosą VIN, jeśli je znasz; w przeciwnym razie `nullptr, 0` |
| `decodeParking` | twoja funkcja czujników parkowania albo `nullptr` |
| `decodeBody` | twoja funkcja zapłonu i wstecznego albo `nullptr` |
| `decodeLights` | twoja funkcja świateł albo `nullptr` |
| `powerOutputs` | `true` tylko wtedy, gdy `decodeBody` niezawodnie czyta zapłon; `false` trzyma wszystkie wyjścia 12 V wyłączone |
| `keyRemapDefaults`, `keyRemapDefaultCount` | `nullptr, 0`: każdy przycisk dociera do radia z własnym kodem. Wiersze `{from, to}` (kody protokołu radia, z którym budujesz: kody XP przy protokole domyślnym, twoje kody przy `-DRADIO_PROTOCOL=CUSTOM`) tylko dla zamian, które chcesz mieć domyślnie; zobacz [Przyciski i radio](radio-mapping.md#default-remappings-come-from-the-car-profile) |

`nullptr` jest zawsze dozwolony: ten sygnał nigdy wtedy nie powstaje.

### Numery bajtów w kodzie zaczynają się od 0 {#byte-numbers-in-code-start-at-0}

Dokumentacja i strony pojazdów liczą bajty CAN od **1** (bajt 1 = pierwszy bajt danych). W C++
pierwszy bajt to `f.data[0]`, a pole `byteIndex` w tabelach to ten sam indeks. Przycisk zapisany
jako „bajt 1, bit 2” to więc `byteIndex 0, mask 0x04`. Sprawdź to dwa razy: to najłatwiejszy błąd.

### Prędkość transmisji {#bit-rate}

`src/can_bus.cpp` zna te prędkości: 50000, 100000, 125000, 250000, 500000 i 1000000 bit/s. Każda
inna wartość się skompiluje, ale kontroler CAN nie wystartuje: konsola pokaże `# can=FAIL`, dioda
stanu zaświeci na czerwono, a wyjścia 12 V zostaną wyłączone. To stan bezpieczny, nie awaria.

### Zapłon i wsteczny: `decodeBody` {#ignition-and-reverse-decodebody}

Funkcja dostaje każdą odebraną ramkę. Zwraca `false` dla każdej ramki, która nie jest jej, albo
jest za krótka, i `true`, gdy wypełni `out`. Zawsze najpierw sprawdź ID i długość; nigdy nie czytaj
poza długością.

Program nie używa bezpośrednio bajtu zapłonu twojego auta. Zamieniasz go na niezależny od auta
typ `vehicle::Ignition` (`firmware/lib/core/vehicle_signals.h`):

| Wartość | Znaczenie |
|---|---|
| `OFF` | zapłon wyłączony albo jeszcze nic nie zdekodowano |
| `ON` | zapłon włączony, silnik nie pracuje |
| `ENGINE` | silnik pracuje |
| `TRANSITION` | wszystko inne, co nie jest wyłączeniem: rozruch, wyłączanie, wartość, której nie znasz |

Surową wartość z twojego auta zachowaj w `ignitionRaw`. Jest pokazywana tylko w konsoli i w panelu.

Przykład z wymyślonego auta tego samouczka (ramka `2C0`: bajt 2 = zapłon, bajt 4 bit 6 =
wsteczny):

```cpp
bool decodeBody(const CanFrame& f, vehicle::BodyState& out) {
    if (f.id != ID_BODY || f.len < 4) return false;   // our frame, long enough?
    out.ignitionRaw = f.data[1];                       // byte 2
    switch (f.data[1]) {
        case 0: out.ignition = vehicle::Ignition::OFF; break;
        case 1: out.ignition = vehicle::Ignition::ON; break;
        case 2: out.ignition = vehicle::Ignition::ENGINE; break;
        default: out.ignition = vehicle::Ignition::TRANSITION; break;
    }
    out.reverseGear = (f.data[3] & 0x40) != 0;         // byte 4 bit 6
    return true;
}
```

Jak używa tego wspólna logika (`firmware/lib/core/power_policy.h`):

- ACC jest włączone w każdym stanie poza `OFF`, więc rozruch nie wyłącza radia.
- ACC wyłącza się 2,5 s po przejściu zapłonu na `OFF`.
- Ramka musi przychodzić stale. Jeśli nie ma jej dłużej niż 1 s, zapłon liczy się jako wyłączony
  ([jak zmierzyć okres](decode.md#how-often-a-frame-comes)).
  W aucie wzorcowym ramka zapłonu `0F6` przychodzi co 500 ms (zmierzone: nagranie
  `can_20261111_191045.log`). Jeśli twoja ramka zapłonu przychodzi rzadziej niż raz na sekundę,
  wyjścia 12 V będą się wyłączać.
- Wyjście wstecznego jest włączone tylko przy włączonym zapłonie i świeżej ramce.

### Światła: `decodeLights` {#lights-decodelights}

Ten sam wzór: sprawdź ID i długość, ustaw `lightsOn`, zwróć `true`. Wyjście podświetlenia jest
włączone tylko wtedy, gdy zapłon jest włączony **i** ramka świateł przyszła w ciągu ostatniej
sekundy ([jak zmierzyć okres](decode.md#how-often-a-frame-comes)). Auto, które wysyła stan świateł tylko przy zmianie, straci wyjście podświetlenia po
sekundzie.

### Czujniki parkowania: `decodeParking` {#parking-sensors-decodeparking}

Wypełnij `vehicle::ParkingState` z tym samym znaczeniem co w aucie wzorcowym:

| Pole | Znaczenie |
|---|---|
| `rearStatus`, `frontStatus` | `4` (`PARK_ACTIVE`) = działa. Pikanie używa tylko strony ze stanem 4. Inne wartości: 1 usterka, 2 wyłączone przez kierowcę, 5 oczekiwanie, 6 niesprawne |
| `rearLeft` ... `frontRight` | strefy 0 do 7: 0 = bardzo blisko, 7 = daleko albo brak przeszkody. Ogranicz swoje wartości do 0..7 |
| `soundRequest`, `soundFront` | auto prosi o dźwięk; z przodu (`true`) albo z tyłu |
| `intervalRaw` | `0x00` = ton ciągły, `0x3F` = bez piknięć, wartości pośrednie = coraz bliżej |

Zacznij od `out = vehicle::ParkingState();`, żeby każde pole, którego nie wypełnisz, miało swoją
wartość „nic” (strefy 7, stan 0).

Co urządzenie z tym robi (`firmware/lib/core/parking_beeper.h`):

- pika od pierwszej strefy poniżej 7 na aktywnej stronie, nawet jeśli auto nie prosi o dźwięk;
  rytm wynika z najbliższej strefy;
- gra ton ciągły **tylko** wtedy, gdy ustawione jest `soundRequest`, a `intervalRaw` wynosi `0x00`.
  Jeśli twoje auto nie podaje odstępu, wylicz go ze strefy, tak jak robi to szablon;
- milknie, gdy przez 400 ms nie przyszła żadna ramka czujników. W aucie wzorcowym ramka
  przychodzi co 100 ms.

### Dane auta dla ekranu radia {#car-data-for-the-radio-screen}

Siedem funkcji opcjonalnych, wszystkie według tych samych zasad: najpierw sprawdź ID i długość,
wypełnij typ neutralny, ustaw jego znacznik `valid`, zwróć `true` (albo bity strony lub zmiany).
`firmware/lib/car_example/profile_example.cpp` pokazuje `decodeDoors` i `decodeTrip`.

- `decodeDoors` -> `DoorState`: `open` zawiera bity `DOOR_DRIVER`, `DOOR_PASSENGER`,
  `DOOR_REAR_LEFT`, `DOOR_REAR_RIGHT`, `DOOR_BOOT`, `DOOR_BONNET`; `anyOpen` = ostrzeżenie auta
  o otwartych drzwiach. Zwróć `true` tylko wtedy, gdy ramka mówi coś o drzwiach.
- `decodeTrip` -> `TripData`: zwróć `TRIP_NOW`, `TRIP_1` albo `TRIP_2` dla strony, którą ramka
  zaktualizowała.
- `decodeOutsideTemp` -> `OutsideTemp`, w połówkach stopnia Celsjusza.
- `decodeEcoMode` -> `EcoMode`: auto za chwilę odetnie akcesoria.
- `decodeClimate` -> `ClimateState`: temperatury w połówkach stopnia (0 = brak wartości), nawiew
  0..15, znaczniki `AIR_*`.
- `decodeSteering` -> `SteeringAngle`: surowa wartość w kodowaniu auta wzorcowego.
- `decodeSettings` -> `VehicleSettings`: dla każdej pozycji `at(SettingId::...)` wartość `value`,
  `known` (twoja ostatnia ramka ją podała) oraz `present` / `presentKnown` (auto ma tę pozycję).
  Zwróć `SETTINGS_VALUES` i / lub `SETTINGS_PRESENT`.

Funkcje z argumentem `io` (komputer pokładowy, klimatyzacja, drzwi, ustawienia) dostają stan,
który dekoder przechowuje między ramkami. Zmieniaj tylko to, co mówi twoja ramka, żeby kilka ramek
mogło złożyć jeden stan.

### Przyciski: tabela `keys` {#buttons-the-keys-table}

Jeden wiersz to jeden przycisk, który jest „wciśnięty, dopóki występuje wzór bitów”:

```cpp
{canId, byteIndex, mask, value, KeyId::...}
```

Przycisk liczy się jako wciśnięty, dopóki w ramce `canId` zachodzi `(data[byteIndex] & mask) == value`.
Zasady z kodu (`firmware/lib/core/car_profile.h`, `firmware/lib/app/xp_keys.h`):

- kolejność wierszy to kolejność dopasowania i kolejność zdarzeń dla radia;
- używanych jest najwyżej 32 wierszy;
- ramka krótsza niż `byteIndex + 1` jest dla tego wiersza pomijana;
- wiersz, którego wzór występuje, gdy nic nie jest wciśnięte, zgłasza przycisk przytrzymany cały
  czas. Przy `value` = `0x00` łatwo o to; daj każdemu wierszowi wzór, który powstaje tylko przy
  wciśnięciu;
- wciśnięcie = jedno zdarzenie od razu, potem co 200 ms „przytrzymany”, dopóki wzór trwa, potem
  „puszczony”.

Dwa sposoby kodowania przycisków przez auto, oba spotykane w praktyce:

```cpp
// one bit per button (template car: byte 1 bit 0 = volume +, bit 1 = volume -)
{0x200, 0, 0x01, 0x01, KeyId::VOLUME_UP},
{0x200, 0, 0x02, 0x02, KeyId::VOLUME_DOWN},

// a button number in a group of bits (tutorial car: byte 1 bits 3-0 = 1, 2, 3, ...)
{0x1F0, 0, 0x0F, 0x01, KeyId::VOLUME_UP},
{0x1F0, 0, 0x0F, 0x02, KeyId::VOLUME_DOWN},
```

Profil wzorcowy też używa drugiego sposobu: w ramce `21F` bajt 1 maska `0C` daje głośność +
(`08`), głośność - (`04`) i oba naraz, wyciszenie (`0C`) (zmierzone: nagrania
`can_20261111_190817.log` i `can_20261111_191045.log`, CAN i linia skrzynki CAN do radia na
wspólnym zegarze).

Nazwy (`KeyId`) to przyciski logiczne. Jaki kod dostaje radio dla każdej nazwy, to zadanie
protokołu radia: zob. [Przyciski i radio](radio-mapping.md).

### Pokrętła: tabela `knobs` {#rotary-knobs-the-knobs-table}

Pokrętło to 8-bitowy licznik w jednym bajcie ramki:

```cpp
{canId, byteIndex, onIncrease, onDecrease}
```

Krok licznika +1 to jeden ząbek `onIncrease`, -1 to jeden ząbek `onDecrease`. Na jedną ramkę
zgłaszane są najwyżej 4 ząbki i używane są najwyżej 4 wiersze pokręteł. Pierwsza ramka tylko
ustawia wartość początkową. Jeśli twoje pokrętło liczy w dół, gdy kręcisz je „do przodu”, zamień
obie nazwy, tak jak robi profil wzorcowy dla ramki `21F`.

## Krok 4: zbuduj {#step-4-build-it}

Twój profil wybierają trzy flagi kompilacji, bez zmieniania czegokolwiek w `src/`. Plik
`firmware/platformio_override.sample.ini` ma już środowisko dla profilu o nazwie `mine`:

1. Skopiuj `firmware/platformio_override.sample.ini` do `firmware/platformio_override.ini`. Nowy
   plik nie jest śledzony przez Git; `platformio.ini` wczytuje go automatycznie.
2. Jeśli twój profil nie nazywa się `mine`, zmień sekcję `listen_mycar` tak, żeby wyglądała jak
   niżej, ze swoją nazwą zamiast `<name>` i `<Name>`:

    ```ini
    [env:listen_mycar]
    extends = env:listen_wifi
    lib_deps = car_<name>
    build_flags =
        ${env:listen_wifi.build_flags}
        -DCAR_PROFILE=CUSTOM
        '-DCAR_PROFILE_CUSTOM_HEADER="profile_<name>.h"'
        -DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>
    ```

    `lib_deps` jest potrzebne: PlatformIO nie podąża za `#include`, który przychodzi przez makro.
    Sekcja `listen_mine` w tym samym pliku dotyczy twojej własnej płytki, nie auta; zostaw ją.
3. Zbuduj, z katalogu `firmware`:

    ```bash
    uvx platformio run -e listen_mycar
    ```

    Oczekiwany wynik: kompilacja kończy się `SUCCESS`, a lista zależności u góry pokazuje
    `|-- car_mine @ 1.0.0`. Inne komunikaty: zobacz [Typowe błędy](#common-mistakes).
4. Wgraj program i otwórz konsolę:

    ```bash
    uvx platformio run -e listen_mycar -t upload
    uvx platformio device monitor
    ```

    Oczekiwany wynik: przy starcie `# profile: car=mine radio=xp`. Port i tryb bootloadera:
    [Wgraj program](../quick-start/flash.md).

`listen_mycar` to program tylko do nasłuchu, w trybie „obok skrzynki”: dekoduje twoje auto, pika
czujnikami parkowania z głośnika urządzenia i wypisuje linie konsoli, ale nic nie wysyła do radia
i trzyma wyjścia 12 V wyłączone. W konsoli wpisz `keys`, żeby wypisywać każde zdarzenie
przycisku, `dump`, żeby wypisywać każdą ramkę CAN.

### Tryb dekoder z twoim profilem {#decoder-mode-with-your-profile}

Żeby sterować radiem (ramki UART, ACC, wyjścia podświetlenia i wstecznego), dodaj drugą sekcję do
`platformio_override.ini`. Tę zbudowano z powodzeniem w przebiegu samouczka:

```ini
[env:decoder_mycar]
extends = env:decoder_wifi
lib_deps = car_mine
build_flags =
    ${env:decoder_wifi.build_flags}
    -DCAR_PROFILE=CUSTOM
    '-DCAR_PROFILE_CUSTOM_HEADER="profile_mine.h"'
    -DCAR_PROFILE_CUSTOM_OBJECT=kCarMine
```

> [!CAUTION]
> **Tryb dekoder steruje radiem i wyjściami 12 V**
>
> Wgraj program w trybie dekoder dopiero wtedy, gdy profil przeszedł test na komputerze i na
> stole, i tylko przy wypiętej skrzynce CAN: dwa nadajniki na jednej linii UART i dwa
> źródła ACC to wyjścia pracujące przeciwko sobie. Najpierw przeczytaj
> [Połączenie z radiem](../install-in-car/radio-connection.md) i
> [Pierwsza jazda](../install-in-car/first-drive.md). Na magistrali CAN auta ten program nadal
> tylko słucha.

### Co celowo się nie kompiluje {#what-does-not-build-on-purpose}

| Próba | Wynik |
|---|---|
| `decoder_tx_wifi` z `-DCAR_PROFILE=CUSTOM` | `#error "CAN transmit is not allowed with CAR_PROFILE=CUSTOM ..."` (wypróbowane w przebiegu samouczka) |
| `decoder_tx_wifi` z `-DCAR_PROFILE=GENERIC` | `#error "CAN transmit is not allowed with CAR_PROFILE=GENERIC ..."` (sprawdza `tools/check_tx_profile_lock.sh`) |
| `-DCAR_PROFILE=EXAMPLE` w dowolnym środowisku | `#error "CAR_PROFILE=EXAMPLE is a template for the host tests only ..."` |
| `CUSTOM` bez flagi nagłówka albo obiektu | `#error "CAR_PROFILE=CUSTOM needs ..."` |

## Nie zaczynaj od profilu wzorcowego {#do-not-start-from-the-reference-profile}

Zamiast szablonu mógłbyś skopiować `firmware/lib/car_psa_aee2010/`. Wiedz, co się wtedy stanie:
jeśli twój profil zachowa funkcje wzorcowe, dekoduje ID ramek auta wzorcowego (drzwi, ustawienia,
komputer pokładowy, klimatyzacja). Jeśli zachowa też `psa::decodeBodyState` dla zapłonu, program
potraktuje twoje auto jako auto PSA AEE2010 także przy zmianie ustawień i warunkach nadawania
(`firmware/lib/app/decoder.cpp`, `Decoder::begin`). To jest dobre tylko dla auta, o którym pomiar wykazał, że używa tych samych
ramek. Każdą ramkę innego auta traktuj jako **niezweryfikowaną**, dopóki jej nie nagrasz, nawet
w modelu blisko spokrewnionym z autem wzorcowym.

## Typowe błędy {#common-mistakes}

| Objaw | Przyczyna |
|---|---|
| `fatal error: profile_mine.h: No such file or directory` | niezmienione `name` w `library.json` albo brak `lib_deps` |
| `# can=FAIL` przy starcie, czerwona dioda | `canBitrate` nie jest jedną z sześciu obsługiwanych prędkości |
| przycisk zgłaszany jako przytrzymany cały czas | jego wzór `mask` / `value` występuje w spoczynku |
| przyciski na złym bicie | `byteIndex` liczony od 1 zamiast od 0 |
| wyjście podświetlenia gaśnie po 1 s | ramka świateł nie powtarza się co najmniej raz na sekundę |
| brak pikania, choć strefy się zmieniają | stan strony nie wynosi `4` (`PARK_ACTIVE`) |

Dalej: [Testowanie profilu](test-your-profile.md).
