# Testy na komputerze

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Większość logiki programu nie dotyka sprzętu: dekodowanie ramek CAN, składanie ramek dla radia,
rytm piknięć czujników parkowania, brama nadawania. Ta logika jest w `firmware/lib/` i kompiluje
się także **na twoim komputerze**. Testy na komputerze (host tests) karmią ją prawdziwymi
nagraniami z auta i porównują cały wynik z zapisanymi wzorcami.

Nic nie jest wgrywane i żaden port nie jest otwierany. Nie potrzebujesz płytki ani auta.

## Co jest potrzebne {#what-you-need}

- **Git Bash** w Windows (przychodzi z Git for Windows) albo dowolna powłoka w Linuksie i macOS.
- **uv** (narzędzie do uruchamiania pakietów Pythona). Zainstaluj je tak, jak opisuje
  [Wgraj program](../quick-start/flash.md#1-install-uv); w Windows:
  `winget install --id=astral-sh.uv -e`. Skrypt testów pobiera przez uv przenośny kompilator C++
  (pakiet `ziglang`), więc nie musisz instalować kompilatora. Pierwsze uruchomienie go pobiera,
  wymaga internetu i trwa dłużej; kolejne korzystają z pamięci podręcznej `uv`.
- Albo własny kompilator: `CXX=g++` lub `CXX=clang++` przed poleceniem.

## Uruchom wszystkie testy {#run-all-tests}

Otwórz Git Bash (albo inną powłokę) w głównym katalogu repozytorium i wpisz:

```bash
cd firmware
bash tools/run_host_tests.sh
```

To działa wszędzie. Jeśli masz `make` (Linux, macOS; Git Bash go nie zawiera), to samo z głównego
katalogu:

```bash
make test
```

Z własnym kompilatorem:

```bash
cd firmware
CXX=g++ bash tools/run_host_tests.sh
```

Skrypt buduje każdy test w `firmware/.build_host/`, uruchamia go i porównuje wynik ze wzorcem.
Wypisuje jedną linię na test:

```text
OK     conformance
OK     replay_log
...
All tests match the references.
```

Prawdziwy wynik ma więcej linii, między innymi `compiler: ...` na początku i kilka linii
`OK     golden ...`. Liczy się ostatnia linia: `All tests match the references.`

Jeśli któraś linia nie jest `OK`, skrypt zatrzymuje się na pierwszym problemie z niezerowym kodem
wyjścia:

| Linia | Znaczenie |
|---|---|
| `OK     name` | wynik identyczny ze wzorcem |
| `DIFF   name: reference ..., result ...` | wynik się różni; dalej pierwsze 40 linii różnicy |
| `FAIL   name: compilation ...` | kompilator wypisał błąd **albo ostrzeżenie**. Zasada: zero ostrzeżeń |
| `FAIL   name: no capture ...` | brakuje nagrania z `firmware/test_data/captures/` |

Inny katalog z nagraniami podasz przez `LOGS=/path/to/captures`.

## Co sprawdzają testy {#what-the-tests-check}

Krótka mapa. Sam skrypt (`firmware/tools/run_host_tests.sh`) ma komentarz nad każdym testem.

| Test | Co sprawdza | Wejście |
|---|---|---|
| `conformance` | przyciski, czujniki parkowania i czasy zasilania względem tego, co prawdziwa skrzynka CAN wysłała do radia w tej samej chwili | dwa nagrania z liniami CAN i UART |
| `replay_log` | co zagrałby głośnik urządzenia | nagrania |
| `beeper_scale` | tempo piknięć dla każdej strefy czujnika, ton ciągły, cisza po 400 ms bez ramek | ramki syntetyczne |
| `power_loss` | ACC i zapłon, gdy giną ramki `0F6` w trakcie zapisu do pamięci flash | nagranie |
| `custom_keys_doors` | zamiana przycisków i stany drzwi | nagranie |
| `car_state` | stan auta z ramek CAN, ramki krótkie i długie, ramki wklejone | dwa nagrania |
| `xp_codec` | ramki protokołu radia: suma kontrolna, limit długości, limit czasu, śmieci | bajty syntetyczne |
| `settings_38` | ustawienia pojazdu pokazywane radiu (ramki UART `38`, `3C`) względem skrzynki CAN | trzy nagrania |
| `radio_commands`, `settings_15b` | polecenia radia i bajty ramki `15B` (tylko obliczane, nic nie jest wysyłane) | wektory, losowe ramki |
| `radio_link`, `loop_policy` | kolejka i czasy ramek do radia, odroczone zapisy do pamięci flash | syntetyczne |
| `can_tx_gate`, `can_tx_gate_default` | brama nadawania z atrapą zamiast sprzętu; budowana z `-DCAN_TX_EXPERIMENTAL=1` i z `=0` | syntetyczne |
| `settings_change`, `can_tx_arm`, `tx_path`, `tx_path_default` | cała ścieżka polecenie radia -> brama -> atrapa portu, uzbrajanie powiązane z wersją budowania | syntetyczne i jedno nagranie |
| `key_tables`, `radio_none`, `profile_tables`, `example_car`, `radio_example` | profile aut i protokoły radia, w tym profil `generic`, profil wzorcowy i wzorcowy protokół radia (`radio_example`: tylko własne ramkowanie, żadnej ramki XP na linii do radia) | nagrania i syntetyczny zapis |
| `net_policy`, `net_rings`, `net_util` | kiedy działa Wi-Fi, bufory panelu, hasło i znacznik programu | syntetyczne, jeden plik z danymi testowymi |
| `golden_stream` (trzy przebiegi) | cały strumień do radia, wyjścia i konsola, bajt po bajcie | pięć nagrań i cztery warianty, patrz [Pliki wzorcowe](golden-files.md) |

> [!NOTE]
> **Czego nie obejmują testy na komputerze**
>
> Kod w `firmware/src/` rozmawia ze sprzętem: sterownik CAN, UART, wyjścia 12 V, uśpienie,
> Wi-Fi, port nadawania. Na komputerze jest tylko kompilowany, nie testowany. Sprawdza się go
> na stole [protokołem testu na stole](bench-test-protocol.md).

## Inne sprawdzenia {#other-checks}

One też nie potrzebują płytki:

| Polecenie (z `firmware/`) | Co sprawdza |
|---|---|
| `python -I tools/check_layering.py` | `lib/core` nie dołącza kodu auta ani radia, oraz inne zasady warstw |
| `bash tools/check_no_can_tx.sh` | po zbudowaniu wszystkich środowisk: `twai_transmit` istnieje tylko w `decoder_tx_wifi` i tylko w jednym pliku źródłowym. Używa narzędzia `nm`, które PlatformIO instaluje razem z kompilatorem ESP32-S3; żeby użyć innego, ustaw `NM=<path to xtensa-esp32s3-elf-nm>` |
| `bash tools/check_tx_profile_lock.sh` | wersja z nadawaniem nie kompiluje się z profilem `GENERIC`, `EXAMPLE` ani `CUSTOM` (przypadek `CUSTOM` buduje się z nagłówkiem i obiektem profilu generic, więc dochodzi do `#error` w `src/config.h`) |
| `python -I tools/check_i18n.py` | słowniki panelu mają te same klucze |

`make check` w głównym katalogu repozytorium (po `make build`) uruchamia sprawdzenie nadawania,
sprawdzenie tożsamości projektu, skan danych prywatnych, sprawdzenie plików wykluczonych,
sprawdzenie metadanych obrazów i sprawdzenie pochodzenia stałych.

## Dodaj test {#add-a-test}

Zasada projektu: nowa funkcja = logika w `firmware/lib/` + test na nagraniu.

1. Napisz logikę we właściwej bibliotece (`lib/core` dla kodu niezależnego od auta,
   `lib/car_<profile>` dla jednego auta, `lib/radio_<protocol>` dla jednego radia). Bez
   `Arduino.h`.
2. Napisz `firmware/test_host/<name>.cpp`. Wypisuje, co robi logika, po jednej linii na zdarzenie,
   i zwraca 0. `test_host/replay_frames.h` pomaga odtwarzać nagranie ramka po ramce.
3. Dodaj jedną linię do `firmware/tools/run_host_tests.sh`, obok podobnych testów:

    ```bash
    test_one my_test "my_logic.cpp psa_frames.cpp" can_20261111_191045.log
    ```

    Drugi argument to lista plików bibliotek do dołączenia; każda nazwa musi istnieć w dokładnie
    jednym katalogu `lib/*/`. Bez nazwy nagrania test uruchamia się raz, bez argumentów.
4. Zapisz wzorzec: `bash tools/run_host_tests.sh --record-golden`.
5. Przeczytaj nowy `test_host/expected/my_test.txt` linia po linii. Sprawdź, czy to jest to, co
   zrobiło auto.

> [!WARNING]
> **Zapis wzorców nadpisuje wszystkie**
>
> `--record-golden` zapisuje od nowa **każdy** wzorzec, nie tylko nowy. Przed commitem obejrzyj
> w Git różnicę w `firmware/test_host/`. Każda zmiana w pliku, którego nie chciałeś zmieniać, to
> zmiana działania: wyjaśnij ją w pull requeście albo ją cofnij.

Dalej: jak przetestować nowy profil auta na własnych nagraniach:
[Testowanie profilu](../make-it-yours/test-your-profile.md). Jak robić nagrania:
[Nagrania z auta](captures.md).
