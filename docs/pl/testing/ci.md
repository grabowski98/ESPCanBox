# Ciągła integracja

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

> [!NOTE]
> **Jeszcze nie uruchomione na GitHubie**
>
> Opisane niżej pliki procesów (workflow) istnieją i przechodzą `actionlint`, a
> `tools/release_check.sh` przechodzi na komputerze z Windows (Git Bash). Nie działały jeszcze na
> maszynie GitHuba (runner): stanie się to przy pierwszym pushu. W szczególności testy na
> komputerze z `g++` są na razie tylko zaplanowane.

Repozytorium ma trzy pliki dla GitHuba:

| Plik | Kiedy działa | Co robi |
|---|---|---|
| `.github/workflows/ci.yml` | przy każdym pushu i pull requeście albo ręcznie | wszystkie sprawdzenia (jedno polecenie) i testy na komputerze z drugim kompilatorem |
| `.github/workflows/release.yml` | ręcznie albo przy znaczniku wersji `v<x.y.z>` | buduje obrazy wydania; tylko obrazy bez nadawania na CAN |
| `.github/dependabot.yml` | co miesiąc | pull request, gdy przypięta akcja GitHuba ma nową wersję |

Oba procesy działają na `ubuntu-24.04`, instalują tylko `uv` (przypięta wersja), a każde inne
narzędzie pobierają przez `uvx` w przypiętych wersjach (PlatformIO, MkDocs, actionlint). Użyte
akcje są przypięte do skrótów commitów. Nie używają sekretów i dostają token tylko do odczytu
(`permissions: contents: read`). Żaden z nich nie łączy się z płytką.

## Jedno polecenie: `tools/release_check.sh` {#one-command-toolsrelease_checksh}

CI uruchamia ten sam skrypt, który uruchamiasz w domu, z głównego katalogu repozytorium:

```bash
bash tools/release_check.sh                 # every check; exit code 0 only when all passed
bash tools/release_check.sh --skip-build    # quick run without the PlatformIO builds (not a release check)
make release-check                          # the same as the first line
```

Każdy krok działa także po błędzie, a podsumowanie na końcu wymienia kroki zaliczone, pominięte i
nieudane. Kod wyjścia to 1, gdy którykolwiek krok się nie udał.

| Krok | Polecenie | Po co |
|---|---|---|
| `host_tests` | `bash tools/run_host_tests.sh` (w `firmware/`) | logika `lib/` względem wzorców testowych; kompilator z `CXX`, domyślnie zig przez `uvx` |
| `can_diff_selftest` | `python -I tools/can_diff.py test_data/synthetic/marker_demo.log --selftest` (w `firmware/`) | wyszukiwarka bitów ze strony [Dekodowanie ramek](../make-it-yours/decode.md) nadal znajduje znane bity |
| `build_envs` | `uvx platformio run -e …` dla wszystkich sześciu środowisk (w `firmware/`) | każde środowisko się kompiluje; nic nie jest wgrywane |
| `check_no_can_tx` | `bash tools/check_no_can_tx.sh` (w `firmware/`, po budowaniu) | `twai_transmit` istnieje tylko w `decoder_tx_wifi` |
| `check_tx_profile_lock` | `bash tools/check_tx_profile_lock.sh` (w `firmware/`) | `decoder_tx_wifi` nie może się zbudować z profilem auta `GENERIC`, `EXAMPLE` ani `CUSTOM` |
| `check_layering` | `python -I tools/check_layering.py` (w `firmware/`) | `lib/core` pozostaje niezależne od auta |
| `check_i18n` | `python -I tools/check_i18n.py --langs en,pl,de,es,ru,uk` (w `firmware/`) | każdy język panelu ma każdy klucz i te same symbole zastępcze |
| `project_identity` | `python -I tools/set_project_name.py --check` | nazwa w `project.json` zgadza się z programem i panelem |
| `scan_private` | `python -I tools/scan_private.py .` | brak VIN, e-maila, adresu MAC, nazwy sieci i nazwiska autora |
| `check_excluded` | `python -I tools/check_excluded.py .` | brak plików z listy wykluczeń |
| `strip_exif` | `python -I tools/strip_exif.py --check .` | brak danych EXIF (na przykład pozycji GPS) w obrazach |
| `check_provenance` | `python -I tools/check_provenance.py --root .` | każda stała protokołu w kodzie ma wiersz i źródło w `radios/xp-simple-soft/provenance.md` |
| `check_doc_links` | `python -I tools/check_doc_links.py` | brak martwych linków względnych i kotwic |
| `check_translations` | `python -I tools/check_translations.py` | każda przetłumaczona strona ma stronę angielską i baner; tyle samo nagłówków, bloków kodu i wierszy tabel co po angielsku |
| `mkdocs_strict` | `mkdocs build --strict` (mkdocs-material, mkdocs-static-i18n) do katalogu tymczasowego | strona dokumentacji buduje się bez ostrzeżeń; nie zostaje katalog `site/` |
| `actionlint` | `actionlint .github/workflows/*.yml` (z shellcheck) | same pliki procesów są poprawne |

`--binaries DIR` dodaje jeszcze jeden krok: `scan_private.py` na obrazach w `DIR`.

Zmienne: `PIO` (domyślnie `uvx platformio`), `PYTHON` (domyślnie `python`), `CXX` (testy na
komputerze), `MKDOCS` i `ACTIONLINT` (inne polecenia dla tych dwóch narzędzi).

## `ci.yml` {#ciyml}

1. Pobranie repozytorium, instalacja Pythona 3.13, `pip install uv==<pinned>`.
2. `bash tools/release_check.sh` (wszystkie kroki powyżej, z kompilatorem zig dla testów na
   komputerze).
3. `CXX=g++ bash tools/run_host_tests.sh` w `firmware/`: testy na komputerze drugi raz, z `g++`
   maszyny GitHuba. Każde ostrzeżenie kompilatora oblewa test, więc wychodzą ostrzeżenia, które
   zgłasza tylko jeden z kompilatorów.

## `release.yml` {#releaseyml}

> [!WARNING]
> **Test na stole przed znacznikiem wersji**
>
> Przed znacznikiem wersji przejdź [protokół testu na stole](bench-test-protocol.md) z obrazem z
> ręcznego uruchomienia tego procesu i wypchnij znacznik dopiero po zaliczeniu. Ręczne uruchomienie z wejściem
> `bench_tested` pozostawionym na `false` nadaje każdemu obrazowi nazwę `…-NOT-BENCH-TESTED`.

1. Czyta `FW_VERSION` z `firmware/src/version.h`; przy znaczniku musi on być równy `v` + ta wersja.
2. Ustawia `PLATFORMIO_BUILD_FLAGS` na opcje `-ffile-prefix-map`, żeby kod projektu w obrazach
   zawierał `.` i `~/.platformio` zamiast ścieżek maszyny GitHuba. Obrazy są budowane **tylko w
   CI**: lokalne budowanie zawiera ścieżki twojego komputera.
3. `bash tools/release_check.sh`: każde sprawdzenie i budowanie wszystkich sześciu środowisk.
   Wersja z nadawaniem `decoder_tx_wifi` jest budowana tylko po to, by `check_no_can_tx.sh` mógł
   potwierdzić, że to jedyny program z `twai_transmit`.
4. Pakuje pięć środowisk **bez nadawania na CAN** (`listen`, `decoder`, `listen_wifi`,
   `decoder_wifi`, `listen_generic_wifi`) do `dist/`:
    - `<env>-<version>.bin`: obraz aplikacji (aktualizacja przez Wi-Fi albo esptool pod `0x10000`);
    - `<env>-<version>-merged.bin`: bootloader, tablica partycji, `boot_app0` i aplikacja w jednym
      pliku zapisywanym pod adresem `0` (ESP Web Tools albo jedno polecenie esptool). Adresy
      pochodzą z PlatformIO (`pio project metadata`), łączy je `firmware/tools/merge_image.py`,
      który odrzuca każde środowisko z nadawaniem.
5. Sprawdza `dist/`: żadna nazwa pliku nie zawiera `tx`, `scan_private.py dist`, potem zapisuje
   `SHA256SUMS`.
6. Wysyła `dist/` jako artefakt procesu (przechowywany 30 dni). Zrobienie z niego wydania na
   GitHubie to krok ręczny.

Żaden obraz, który może nadawać na CAN, nigdy nie jest pakowany. Kto chce nadawać, buduje
`decoder_tx_wifi` celowo (patrz [Nadawanie na magistralę CAN](../safety/transmit.md)).

## Zanim otworzysz pull request {#before-you-open-a-pull-request}

```bash
bash tools/release_check.sh
```

Bez kompilatora C++ na komputerze testy na komputerze używają zig przez `uvx`; CI dokłada
uruchomienie z `g++`.
