# Wgraj program

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona prowadzi od świeżego komputera do płytki, na której działa program i jego panel WWW.
Kroki 1-3 działają bez płytki.

> [!NOTE]
> **Stan tego drzewa programu**
>
> Ten program kompiluje się i przechodzi testy na PC (wszystkie środowiska się kompilują, testy
> na komputerze zgadzają się ze wzorcami). Wersję 2.0.0 sprawdzono na stole na płytce ESP32-S3-WROOM-1-N16R8
> ([protokół testu na stole](../testing/bench-test-protocol.md)) i wypróbowano w jednym aucie 20.11.2026.

**Gdzie wpisywać polecenia.** W Windows wpisuj wszystkie polecenia z tej strony w **PowerShell**
(albo w Terminalu Windows). Git Bash nie jest potrzebny do wgrania programu, tylko do testów na
komputerze i do stron działu [Dostosuj do siebie](../make-it-yours/porting-checklist.md). W Linuksie i macOS używaj zwykłego terminala.

## Najszybciej: gotowy obraz w przeglądarce {#ready-image-in-the-browser}

To zalecany sposób na pierwszą instalację. Nie potrzeba PlatformIO, pakietu Pythona do
wgrywania ani zwykle dodatkowego sterownika: tylko Chrome albo Edge i plik obrazu. Działa też tam,
gdzie `pip install esptool` się nie udaje (na bardzo nowej wersji Pythona instalacja potrafi
przerwać się błędem kompilacji jednej z zależności).

1. Pobierz obraz. Gotowe obrazy są na stronie projektu w dziale **Releases**, po parze na każde
   środowisko bez nadawania na CAN (`listen`, `decoder`, `listen_wifi`, `decoder_wifi`,
   `listen_generic_wifi`; przeczytaj [Które środowisko](#which-environment)). Obrazu z nadawaniem
   nigdy nie ma w gotowej postaci. Wybierz plik według tego, co robisz:

    | Plik | Adres | Do czego |
    |---|---|---|
    | `<environment>-<version>-merged.bin` | `0x0` | pierwsza instalacja; kasuje całą pamięć flash razem z zapisanymi ustawieniami (urządzenie wygeneruje potem nowe hasło Wi-Fi) |
    | `<environment>-<version>.bin` | `0x10000` | aktualizacja urządzenia, na którym już działa ten program; zapisane ustawienia zostają |

    Własna kompilacja (kroki 1-3 niżej) daje obraz aplikacji
    `.pio/build/<environment>/firmware.bin`: tak jak plik bez `-merged` idzie pod `0x10000` i tylko
    aktualizuje urządzenie, na którym już działa ten program.

2. Podłącz płytkę do komputera kablem USB do danych, do natywnego gniazda USB układu ESP32-S3
   (zobacz [Podłącz płytkę](#4-connect-the-board)).
3. Otwórz <https://espressif.github.io/esptool-js/> w Chrome albo Edge.
4. Kliknij **Connect** i wybierz port płytki z listy, którą pokaże przeglądarka.
5. Wpisz adres z tabeli w pole adresu, obok wybierz plik obrazu i kliknij **Program**. Przyciski
   na module nie są potrzebne: płytka resetuje się sama.
6. Oczekiwane: dziennik na stronie kończy się linią `Hash of data verified.` Jeśli nowy program nie
   ruszy, naciśnij RESET albo podłącz kabel ponownie.

Konsola na tamtej stronie tylko pokazuje, co wypisuje urządzenie; nie da się w niej wpisywać
poleceń. Do wpisywania poleceń zainstaluj raz mały terminal szeregowy Pythona i uruchom go
(w Linuksie i macOS wpisz `python3` zamiast `python`):

```text
python -m pip install pyserial
python -m serial.tools.miniterm - 115200
```

`-` sprawia, że wypisze listę portów; wybierz płytkę. Wpisywanych znaków nie widać: wpisz
polecenie na ślepo i zakończ je Enterem. Z terminala wychodzi się przez Ctrl+].

Po pierwszej instalacji `ver` pokazuje wersję. Kompilacja z siecią otwiera wtedy własną sieć Wi-Fi
`<project name>-XXXX` z panelem pod `http://10.50.8.1`, najwyżej dla czterech urządzeń naraz.
Dalej przejdź do [Pierwsze uruchomienie](first-boot.md#2-read-the-password-from-the-usb-console),
żeby odczytać hasło.

<details class="note" markdown="1">
<summary>Inna droga: esptool w wierszu poleceń</summary>

Jeśli `esptool` instaluje się na Twoim komputerze, zapisze te same pliki pod te same adresy.
Użyj jednego z dwóch poleceń zapisu:

```text
python -m pip install esptool
python -m esptool --chip esp32s3 write-flash 0x0 <environment>-<version>-merged.bin
python -m esptool --chip esp32s3 write-flash 0x10000 <environment>-<version>.bin
```

</details>

**Własna kompilacja.** Kroki z numerami poniżej kompilują program ze źródeł w PlatformIO
i wgrywają go przez USB. Użyj ich, gdy nie ma gotowego obrazu do tego, czego potrzebujesz, albo gdy
zmieniasz kod.

## 1. Zainstaluj uv {#1-install-uv}

Program kompiluje się w [PlatformIO](https://platformio.org/). Nie instalujesz go sam:
`uvx platformio ...` pobiera je i uruchamia przez [uv](https://docs.astral.sh/uv/), małe narzędzie
do uruchamiania programów.

### Windows {#windows}

1. Otwórz PowerShell i uruchom:

    ```powershell
    winget install --id=astral-sh.uv -e
    ```

2. Zamknij PowerShell i otwórz go ponownie, żeby znalazł `uv`.

Niepotrzebne do wgrania programu; potrzebne do testów na komputerze i do działu „Dostosuj do
siebie”: [Git for Windows](https://git-scm.com/download/win). W pakiecie jest **Git Bash**, który
uruchamia skrypty testów i polecenia z tych stron.

### Linux {#linux}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Żeby używać portu USB bez roota, dodaj siebie do grupy portów szeregowych (`dialout` w Debianie
i Ubuntu, `uucp` w Archu) i zainstaluj reguły udev PlatformIO, jak opisuje
[dokumentacja PlatformIO](https://docs.platformio.org/en/latest/core/installation/udev-rules.html).
Wyloguj się i zaloguj ponownie.

### macOS {#macos}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

albo, z Homebrew, `brew install uv`.

### Sprawdź {#check-it}

Otwórz nowy terminal i uruchom:

```text
uvx platformio --version
```

Oczekiwany wynik: jedna linia z wersją PlatformIO, na przykład `PlatformIO Core, version 6.2.0`.
Pierwsze wywołanie pobiera PlatformIO i trwa chwilę; kolejne startują od razu.
Jeśli widzisz „command not found” albo „not recognized”, zamknij terminal, otwórz nowy i spróbuj
jeszcze raz.

Jeśli wolisz edytor, działa też rozszerzenie PlatformIO do VS Code: otwórz folder `firmware`
i wybierz środowisko na pasku stanu. Polecenia poniżej to te same, które rozszerzenie uruchamia za
ciebie.

## 2. Pobierz kod {#2-get-the-code}

1. Otwórz stronę projektu (repozytorium, w którym znalazłeś ten poradnik) i wybierz
   **Code → Download ZIP**. Jeśli używasz Gita, działa też `git clone`.
2. Rozpakuj ZIP. W Windows: prawy klik na pliku → **Wyodrębnij wszystkie**.
3. Otwórz rozpakowany folder, w którym widać obok siebie `firmware`, `docs` i `README.md`. Po
   rozpakowaniu bywa on o jeden poziom głębiej, w folderze o tej samej nazwie.
4. Otwórz terminal w folderze `firmware` w jego środku:
    - **Windows:** otwórz `firmware` w Eksploratorze plików, kliknij pasek adresu, wpisz
      `powershell` i naciśnij Enter. (W Windows 11 działa też prawy klik na pustym miejscu →
      **Otwórz w terminalu**.)
    - **Linux i macOS:** otwórz terminal, wpisz `cd ` (ze spacją), przeciągnij folder `firmware`
      do okna i naciśnij Enter.

Wszystkie polecenia poniżej uruchamiasz w tym terminalu, w folderze `firmware`.

## 3. Skompiluj {#3-build}

```text
uvx platformio run -e listen_wifi
```

Ten program tylko słucha auta i ma panel WWW. Bez panelu użyj tu i w kroku 5 `-e listen`.

Pierwsza kompilacja pobiera platformę ESP32 (`espressif32@8.0.0`, przypiętą w `platformio.ini`)
i kompilator. To dużo danych i może potrwać wiele minut; nie przerywaj. Kolejne kompilacje używają
ich ponownie.

Oczekiwany wynik: na końcu PlatformIO wypisuje `SUCCESS`. Program jest w
`.pio/build/listen_wifi/firmware.bin`. Jeśli zamiast tego widzisz `Error`, przeczytaj pierwszą
linię błędu: błąd sieci oznacza przerwane pobieranie, więc uruchom polecenie jeszcze raz.

### Które środowisko {#which-environment}

**Środowisko** to nazwany zestaw opcji kompilacji w `firmware/platformio.ini`. Decyduje, co
urządzeniu wolno robić. Na biurko wystarczy `listen_wifi`. Do innego auta niż wzorcowe użyj
później `listen_generic_wifi`.

<details class="note" markdown="1">
<summary>Wszystkie sześć środowisk i kompilacja wszystkich naraz</summary>

Nigdy nie zmieniaj trybu przez edycję `src/config.h`; zamiast tego wybierz środowisko.

| Środowisko | Tryb | Panel Wi-Fi i aktualizacje przez Wi-Fi | Nadaje na magistralę auta | Do czego |
|---|---|---|---|---|
| `listen` (domyślne) | obok skrzynki | nie | nie (w programie nie ma kodu nadawania) | pierwsze wgranie, stół, słuchanie obok skrzynki CAN |
| `decoder` | dekoder | nie | nie | zastąpienie skrzynki CAN, bez sieci |
| `listen_wifi` | obok skrzynki | tak | nie | słuchanie z panelem WWW |
| `decoder_wifi` | dekoder | tak | nie | zastąpienie skrzynki CAN, z panelem WWW |
| `decoder_tx_wifi` | dekoder | tak | **tylko ramkę `15B`, po `cantx on` i restarcie** | zmiany personalizacji z ekranu radia; najpierw przeczytaj [Nadawanie na magistralę CAN](../safety/transmit.md) |
| `listen_generic_wifi` | obok skrzynki | tak | nie | inne auto: profil `generic`, radio `none`, nagrywa magistralę i nic nie wysyła do radia |

„Obok skrzynki” to tryb tylko nasłuchu: urządzenie pracuje obok skrzynki CAN i nic nie
wysyła. Konsola nazywa ten sam tryb `alongside the box`.

Samo `uvx platformio run` (bez `-e`) kompiluje `listen`, bezpieczny tryb bez sieci.

Żeby skompilować wszystkie sześć środowisk naraz, uruchom `make build` w głównym katalogu
(potrzebny jest `make`, którego Git Bash nie zawiera; zwykłe polecenia
`uvx platformio run -e ...` działają zawsze).

Opcjonalne ustawienia, które zostają na twoim komputerze, wpisuje się do
`firmware/platformio_override.ini` (skopiuj `platformio_override.sample.ini`). Przykład
pokazuje własną płytkę (`listen_mine`) i własny profil auta (`listen_mycar`). Ramka próbna
`1A9` istnieje tylko wtedy, gdy celowo dodasz tam `-DCAN_TX_EXPERIMENTAL=1` do
kompilacji z nadawaniem.

</details>

> [!WARNING]
> **Tryb dekoder tylko bez skrzynki CAN**
>
> Wgrywaj środowisko `decoder*` tylko wtedy, gdy dokupiona skrzynka CAN jest wypięta, albo na
> stole. Dwa urządzenia na linii danych radia i dwa źródła na przewodzie ACC zwierają
> nawzajem swoje wyjścia.

## 4. Podłącz płytkę {#4-connect-the-board}

1. Podłącz płytkę do komputera kablem USB, który przenosi dane (nie kablem tylko do ładowania).
   Na płytkach z dwoma gniazdami USB użyj tego, które jest połączone z samym ESP32-S3 (często
   opisane `USB`, nie `UART`): ten program komunikuje się przez natywne USB układu.
2. Wypisz porty:

    ```text
    uvx platformio device list
    ```

Oczekiwany wynik: po podłączeniu płytki pojawia się nowa pozycja. Windows 10 i 11 zwykle nie
potrzebują dodatkowego sterownika do tego portu.

| System | Port wygląda tak |
|---|---|
| Windows | `COM5` (dowolny numer) |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem1101` (dowolny numer) |

Nic nowego na liście? Spróbuj innego kabla. Jeśli dalej nic, uruchom bootloader ręcznie, patrz
[Jeśli wgrywanie nie rusza](#if-the-upload-does-not-start-bootloader-mode).

## 5. Wgraj {#5-upload}

```text
uvx platformio run -e listen_wifi -t upload
```

PlatformIO samo znajduje port. Przy kilku podłączonych płytkach podaj go:
`uvx platformio run -e listen_wifi -t upload --upload-port COM5`.

Oczekiwany wynik: wgrywanie kończy się `[SUCCESS]`.

- „could not open port” albo „Access is denied”: port jest zajęty. Zamknij monitor (Ctrl+C)
  w drugim oknie i uruchom wgrywanie jeszcze raz.
- Port nie znaleziony: spróbuj innego kabla, potem trybu bootloadera poniżej.

Wgranie przez USB zachowuje zapisane ustawienia (głośność, tabelę przycisków, hasło Wi-Fi).

### Jeśli wgrywanie nie rusza: tryb bootloadera {#if-the-upload-does-not-start-bootloader-mode}

Zwykle narzędzie do wgrywania samo restartuje ESP32-S3 do wbudowanego bootloadera przez USB. Jeśli
port się nie pojawia albo wgrywanie kończy się błędem połączenia (na przykład po programie, który
pada przy starcie), uruchom bootloader ręcznie:

1. Przytrzymaj przycisk **BOOT** (na niektórych płytkach opisany `0` albo `IO0`).
2. Wciśnij i puść **RESET** (`RST` albo `EN`) albo podłącz kabel USB, trzymając BOOT.
3. Puść BOOT. Port pojawia się znowu; uruchom polecenie wgrywania.
4. Po wgraniu wciśnij raz RESET, żeby nowy program wystartował.

To są przyciski płytki deweloperskiej ESP32-S3, nie płytki bazowej.

## 6. Obserwuj konsolę {#6-watch-the-console}

```text
uvx platformio device monitor
```

Prędkość (115200) pochodzi z `platformio.ini`. Wciśnij RESET na płytce, żeby zobaczyć linie
startowe. Wyglądają tak (pierwsza linia zaczyna się od nazwy projektu; `listen` wypisuje
`network=no`, `listen_wifi` `network=yes`):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0, built <date and time>, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: <reset reason>
```

RESET na chwilę rozłącza też port USB. Jeśli monitor się zatrzyma albo nie zobaczysz linii
startowych, uruchom monitor ponownie i wpisz `ver`.

> [!NOTE]
> **Wpisywanych liter nie widać**
>
> Litery, które wpisujesz, nie pojawiają się na ekranie. To normalne: program ich nie odsyła.
> Wpisz polecenie na ślepo i naciśnij Enter; odpowiedź pojawi się w nowej linii.

Wpisz `help` i Enter, żeby zobaczyć listę poleceń, `ver`, żeby zobaczyć wersję, `stat`, żeby
zobaczyć stan. Z monitora wychodzisz przez Ctrl+C.

`FAIL` w linii `can=... audio=... radio=...` oznacza, że jakiś sterownik nie wystartował.
Urządzenie zostaje wtedy w stanie bezpiecznym (wyjścia 12 V wyłączone, nic nie wysyła), a dioda
LED na płytce świeci na czerwono. Nie restartuje się w kółko.

## Kasowanie wszystkiego {#erasing-everything}

`uvx platformio run -e listen -t erase` kasuje całą pamięć flash, razem z zapisanymi ustawieniami.
Przy następnym starcie sieci urządzenie wygeneruje nowe hasło Wi-Fi. Potem wgraj program
ponownie.

## Dalej {#next}

- [Pierwsze uruchomienie](first-boot.md): odczytaj hasło i otwórz panel WWW.
- [Test na stole](bench-test.md): sprawdź płytkę przed autem.
