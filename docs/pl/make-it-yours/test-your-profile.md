# Testowanie profilu

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Zanim twój profil trafi do auta, uruchom go na komputerze. **Test na komputerze** (ang. host test)
przepuszcza nagranie, ramka po ramce, przez ten sam kod dekodera, który działa w urządzeniu,
i wypisuje, co zrobiłoby urządzenie: linie konsoli, ramki dla radia oraz każdą zmianę wyjść 12 V
i pikania. Czytasz ten wynik raz, porównujesz z tym, co robiłeś w aucie, i zapisujesz jako wzorzec.
Od tej chwili test powie ci, gdy jakaś zmiana coś zepsuje.

Nic nie jest wgrywane i żaden port nie jest otwierany. Nie potrzebujesz płytki ani auta.

Ta strona kontynuuje samouczek ze strony [Pisanie profilu auta](write-a-profile.md) z wymyślonym
profilem `mine`. Każde polecenie uruchomiono na kopii repozytorium. Wszystkie polecenia
uruchamiasz z katalogu `firmware`, w Git Bash.

> [!TIP]
> **Najpierw próba na sucho**
>
> Zanim wpiszesz do `car_mine` własne ramki, możesz sprawdzić kopię i zmiany nazw. Zrób kroki
> 2 do 4 z wartościami szablonu wciąż w `car_mine` i z
> `run_file mine_car test_host/synthetic/example_car.log` w kroku 3. Wynik musi kończyć się
> na `example profile works on every axis` i `### code=0`. Potem zmień profil na swoje auto
> i wpisz własne nagranie do linii `run_file`.

## Czego potrzebujesz {#what-you-need}

- Git Bash na Windowsie albo powłoki na Linuksie lub macOS;
- `uv`: skrypt testów pobiera przez niego kompilator C++ (pakiet `ziglang`; pierwsze uruchomienie
  go ściąga, więc raz potrzebuje internetu); możesz też ustawić własny przez
  `CXX=g++`. Szczegóły: [Testy na komputerze](../testing/host-tests.md);
- twojego profilu w `firmware/lib/car_<name>/`;
- nagrania twojego auta ze znacznikami ([Nagrywanie magistrali](record.md)), oczyszczonego
  poleceniem `python -I ../tools/sanitize_capture.py`, jeśli zamierzasz je udostępnić
  ([Udostępnij swoje auto](contribute.md)).

## Krok 1: umieść nagranie na miejscu {#step-1-put-the-recording-in-place}

| Nagranie | Gdzie |
|---|---|
| prawdziwe nagranie twojego auta | `firmware/test_data/captures/`, z jednym wierszem w tamtejszym `README.md` (co, jak nagrane, który test go używa) |
| ramki napisane ręcznie, żeby sprawdzić jakiś przypadek | `firmware/test_host/synthetic/`, z komentarzem na górze, że ramki są wymyślone |

Format jest zwykły, `(seconds) can0 ID#DATA`. Linie, które nie są ramkami CAN (komentarze, linie
UART), test pomija. Ręcznie pisane pliki do testów profilu trafiają do `test_host/synthetic/`,
nie do `test_data/synthetic/`: tamten katalog jest dla `can_diff.py`.

Plik napisany ręcznie to dobry początek, bo wiesz dokładnie, co powinno się stać. Plik użyty
w samouczku, `test_host/synthetic/mine_car.log`, zaczynał się tak (auto z samouczka ze strony
[Pisanie profilu auta](write-a-profile.md); tego pliku nie ma w repozytorium, więc napisz własny):

```text
# Invented frames for the "mine" profile (tutorial). Not a real car.
# 2C0 = ignition / reverse, 1F0 = buttons + knob, 350 = rear parking, 2C8 = lights
(0.000000) can0 2C0#00000000
(0.200000) can0 2C0#00010000
(0.400000) can0 2C0#00020000
(0.600000) can0 1F0#01000000
(0.700000) can0 1F0#01000000
```

## Krok 2: skopiuj przykładowy test {#step-2-copy-the-example-test}

Profil szablonu ma własny test, `firmware/test_host/example_car.cpp`. Skopiuj go, z katalogu
`firmware`:

```bash
cp test_host/example_car.cpp test_host/mine_car.cpp
```

Potem zmień cztery rzeczy w `mine_car.cpp`:

1. `#include "profile_example.h"` zamień na `#include "profile_mine.h"`;
2. każde `kCarExample` zamień na `kCarMine` (sześć razy, w czterech liniach; najłatwiej przez
   „znajdź i zamień”);
3. tekst użycia `example_car <capture.log>` zamień na `mine_car <capture.log>` (w komentarzu na
   górze i w linii z `fprintf`);
4. ostatnie sprawdzenie, linię, która zaczyna się od `const bool ok =`. Wymaga ono, żeby **każde**
   wyjście coś zrobiło. Poszczególne części: `r.accMs` = ACC, `r.illMs` = światła, `r.revMs` =
   wsteczny, `r.toneMs` = pikanie, `r.uartFrames` = ramki dla radia, `d.haveLastKey()` = co
   najmniej jeden przycisk. Usuń części, których twój profil nie ma. Profil bez czujników
   parkowania nie przejdzie warunku `r.toneMs > 0` i test zakończy się wtedy kodem wyjścia 1.
   Gotowe linie:
    - tylko zapłon i przyciski:
      `const bool ok = r.accMs > 0 && r.uartFrames > 0 && d.haveLastKey();`
    - tylko zapłon: `const bool ok = r.accMs > 0;`

    Tekst `example profile works on every axis` może zostać: to tylko etykieta.

## Krok 3: dodaj test do skryptu testów {#step-3-add-the-test-to-the-test-script}

Otwórz `firmware/tools/run_host_tests.sh` i znajdź blok, który kończy się na `finish example_car`.
Dodaj swój test zaraz po nim:

```bash
# My car profile
# shellcheck disable=SC2086
build mine_car $S38_LIBS profile_mine.cpp
rm -f "$OUT/mine_car.actual.txt"
run_file mine_car test_host/synthetic/mine_car.log
finish mine_car
```

`build` kompiluje twój test razem z dekoderem i jego bibliotekami oraz plikiem twojego profilu.
`$S38_LIBS` to lista plików dekodera, których test potrzebuje; zostaw ją bez zmian. `run_file` uruchamia go na podanym pliku, ze ścieżką względną do `firmware/`. Dla
nagrania w `test_data/captures/` podaj tę ścieżkę.

## Krok 4: pierwsze uruchomienie, przeczytaj wynik, zapisz wzorzec {#step-4-first-run-read-the-output-store-the-reference}

Uruchom wszystkie testy na komputerze (`cd firmware` jest potrzebne tylko wtedy, gdy jesteś
w katalogu repozytorium):

```bash
cd firmware
bash tools/run_host_tests.sh
```

Oczekiwany wynik: wszystkie istniejące testy wypisują `OK`. Twój zatrzymuje skrypt, bo nie ma jeszcze wzorca:

```text
OK     example_car
FAIL   mine_car: no reference test_host/expected/mine_car.txt
```

Wynik jest teraz w `firmware/.build_host/mine_car.actual.txt`. **Przeczytaj go** i porównaj z tym,
co robiłeś w aucie (albo co wpisałeś do pliku). To jest właściwy test; zapisany plik tylko go
później powtarza. Fragment z przebiegu samouczka:

```text
### mine_car test_host/synthetic/mine_car.log
profile mine: 500000 bit/s, 4 key rows, 1 knob(s), txAllow 0
     0 outputs acc=0 ill=0 rev=0 tone=0 continuous=0
console [car] acc=1
   200 radio 2E EA 02 01 0A 08
   200 outputs acc=1 ill=0 rev=0 tone=0 continuous=0
console [key] code=14 state=1
   600 radio 2E 20 02 14 01 C8
console [key] code=14 state=2
   800 radio 2E 20 02 14 02 C7
...
console [park] rear=4 front=0 sound=1 rear interval=0x00 zones rear 707 front 777
  1600 radio 2E 32 07 02 05 00 05 05 05 05 AB
  1600 outputs acc=1 ill=1 rev=1 tone=1 continuous=1
...
frames=21 uart_bytes=106 uart_frames=13 acc_ms=3101 ill_ms=1001 rev_ms=800 tone_ms=390 keys_dropped=0
example profile works on every axis
### code=0
```

Jak to czytać:

| Linia | Znaczenie |
|---|---|
| `profile mine: ...` | co zawiera twój profil: prędkość, liczba wierszy przycisków i pokręteł, lista nadawania (dla własnego profilu zawsze 0) |
| `console ...` | linia, którą urządzenie wypisałoby na konsoli USB |
| `200 radio 2E EA ...` | ramka dla radia w 200 ms (czas według zegara nagrania). `EA` = zasilanie włączone, `20` = przycisk, `32` = czujniki parkowania, `3E` = znacznik przeszkody. Zob. [Protokół XP](../reference/xp-protocol.md) |
| `outputs acc=1 ill=0 rev=0 tone=0 continuous=0` | zmiana wyjść 12 V (ACC, podświetlenie, wsteczny) albo pikania |
| `[key] code=14 state=1` | przycisk z kodem radia `14` (głośność +): 1 wciśnięty, 2 przytrzymany, 0 puszczony |
| ostatnie linie | sumy w milisekundach i twoje sprawdzenie `ok` |

Co warto sprawdzić we własnym wyniku:

- ACC włącza się przy twojej ramce „zapłon włączony” i wyłącza się 2,5 s po wyłączeniu zapłonu.
  We fragmencie nigdy się nie wyłącza, bo odtwarzanie kończy się 1 s po ostatniej ramce.
- Wyjście podświetlenia wyłącza się 1 s po ostatniej ramce świateł (w przebiegu samouczka: ramka
  świateł w 1100 ms, `ill=0` w 2101 ms). Jeśli twoje auto powtarza ramkę świateł, tak się nie
  dzieje.
- Każdy przycisk daje `state=1`, potem co 200 ms `state=2`, dopóki jest przytrzymany, potem
  `state=0`.
- Pikanie zaczyna się od pierwszej strefy poniżej 7 i przechodzi w ton ciągły tylko przy odstępie
  `0x00`.
- Przycisk pokazuje inny kod, niż się spodziewasz: zob. [Przyciski i radio](radio-mapping.md).
  Domyślne zamiany przycisków pochodzą z profilu auta; profil przykładowy nie ma żadnej, więc
  przykładowy test pokazuje SRC jako `code=21 state=1`, wysłany bez zmiany.

> [!WARNING]
> **Zapisany `### code=1` przechodzi jako OK**
>
> Skrypt porównuje wynik tylko z zapisanym plikiem. Nie zatrzymuje się na zapisanym
> `### code=1`. Sprawdź ostatnie linie (punkt 1 niżej), zanim skopiujesz plik, bo inaczej test
> przejdzie, choć się nie udał.

Gdy wynik jest dobry, zapisz go jako wzorzec:

1. Sprawdź dwie ostatnie linie `.build_host/mine_car.actual.txt`. Muszą to być
   `example profile works on every axis` i `### code=0`.
2. Jeśli widzisz `FAIL: an axis of the example profile did nothing` i `### code=1`, popraw linię
   `const bool ok =` (krok 2, punkt 4) i uruchom skrypt ponownie.
3. Skopiuj plik i uruchom skrypt ponownie:

```bash
cp .build_host/mine_car.actual.txt test_host/expected/mine_car.txt
bash tools/run_host_tests.sh
```

Oczekiwany wynik: `OK     mine_car` i `All tests match the references.`

> [!WARNING]
> **Nie używaj do tego `--record-golden`**
>
> `bash tools/run_host_tests.sh --record-golden` nagrywa od nowa wzorce **wszystkich** testów,
> także pliki golden auta wzorcowego. Używaj go tylko wtedy, gdy celowo zmieniłeś działanie
> i sprawdziłeś każdą różnicę. Dla jednego nowego testu skopiuj jego plik, jak pokazano wyżej.

## Szybsza pętla dla jednego testu {#a-faster-loop-for-one-test}

Pełny skrypt trwał w przebiegu samouczka od 30 do 50 sekund. Gdy pracujesz nad profilem, możesz
kompilować i uruchamiać tylko swój test. Najpierw raz uruchom pełny skrypt testów: pobiera on
kompilator do pamięci podręcznej `uv`, a polecenie poniżej korzysta z tej pamięci (`--offline`).
Z katalogu `firmware`, w Git Bash:

```bash
LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp profile_mine.cpp vehicle_signals.cpp"
SRCS=""; for f in $LIBS; do SRCS="$SRCS $(ls lib/*/$f)"; done
INC=""; for d in lib/*/; do INC="$INC -I ${d%/}"; done
mkdir -p .build_host
uvx --offline --from ziglang python -I -m ziglang c++ -std=c++17 -O1 -Wall $INC -I include \
    test_host/mine_car.cpp $SRCS -o .build_host/mine_car.exe < /dev/null
./.build_host/mine_car.exe test_host/synthetic/mine_car.log
```

Lista plików jest taka sama jak `$S38_LIBS` w skrypcie testów, plus twój profil
i `vehicle_signals.cpp`. Jeśli lista w skrypcie się zmieni, skopiuj ją stamtąd. Zanim cokolwiek
udostępnisz, uruchom pełny skrypt.

## Uruchom też sprawdzenie warstw {#also-run-the-layering-check}

```bash
python -I tools/check_layering.py
```

Sprawdza, czy twój profil nie dołącza niczego z protokołu radia, z `src/` ani z frameworka Arduino
i czy żadna nazwa nagłówka nie występuje dwa razy. W przebiegu samouczka wypisało `car_mine` wśród
bibliotek i zakończyło się na `Layering OK.`

## Czego ten test nie obejmuje {#what-this-test-does-not-cover}

- Kod w `firmware/src/` (sterownik CAN, UART, wyjścia 12 V, dźwięk) nie działa na komputerze.
  Pierwsze sprawdzenie prawdziwego sprzętu to [test na stole](../quick-start/bench-test.md).
- Czasy na prawdziwej magistrali: odtwarzanie idzie krokami co 1 ms i nie gubi ramek.
- Czy twoje dekodowanie jest dobre dla auta. Test pokazuje tylko, co twój profil robi z ramkami.
  Dlatego czytasz wynik, porównując go ze znacznikami, zanim go zapiszesz.

Dalej: [Przyciski i radio](radio-mapping.md).
