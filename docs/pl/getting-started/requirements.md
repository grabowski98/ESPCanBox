# Czego potrzebujesz

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Możesz zacząć od małego. Każdy poziom niżej dokłada coś do poprzedniego.

## Poziom 0: tylko komputer {#level-0-only-a-computer}

Wystarczy, żeby skompilować program, uruchomić testy i wypróbować panel WWW bez żadnej płytki.

- Komputer z Windows, Linuksem albo macOS i dostęp do internetu przy pierwszej kompilacji.
- [uv](https://docs.astral.sh/uv/), małe narzędzie do uruchamiania programów. Samo pobiera
  PlatformIO, kompilator do testów i pakiety Pythona, gdy są potrzebne.
- Ręcznie zainstaluj tylko:
    - [Python 3](https://www.python.org/downloads/), do makiety panelu i skryptów pomocniczych
      w `firmware/tools/` i `tools/` (używają tylko biblioteki standardowej). W Windows zaznacz
      w instalatorze **Add python.exe to PATH**. W Linuksie i macOS polecenie nazywa się
      `python3`.
    - W Windows, do testów na komputerze: **Git Bash** (jest w pakiecie
      [Git for Windows](https://git-scm.com/download/win)). Skrypty testów są w bashu.
- Przeglądarka internetowa.

Z samym komputerem zrobisz kroki 1-3 strony [Wgraj program](../quick-start/flash.md) i użyjesz
makiety panelu ze strony [Pierwsze uruchomienie](../quick-start/first-boot.md).

## Poziom 1: płytka ESP32-S3 na biurku {#level-1-the-esp32-s3-board-on-your-desk}

- **Płytka deweloperska ESP32-S3 z 16 MB flash i 8 MB PSRAM** (moduł ESP32-S3-WROOM-1-N16R8).
  Szukaj oznaczenia **N16R8** w nazwie modułu, na jego metalowej osłonie albo w opisie oferty.
  Projekt używa Waveshare ESP32-S3-DEV-KIT-N16R8. Płytka z mniejszą pamięcią flash nie jest
  obsługiwana bez zmian. Inna płytka N16R8 może działać, jeśli zgadzają się nóżki, ale nie była
  sprawdzana.
- Kabel USB, który przenosi dane (nie kabel tylko do ładowania). Konsola działa przez natywne USB
  układu ESP32-S3.

Z samą płytką możesz wgrywać program, używać konsoli i panelu WWW. Zasady przydziału nóżek
w programie są na stronie [Konfiguracja i piny](../reference/config-and-pins.md).

## Poziom 2: całe urządzenie {#level-2-the-full-device}

Płytka bazowa łączy płytkę ESP32-S3 z autem. Główne części
[prototypu v1](../hardware/prototype-pcb.md):

| Część | Użyta w prototypie | Do czego służy |
|---|---|---|
| moduł transceivera CAN | Adafruit CAN Pal (TJA1051T/3), przełącznik terminatora OFF | łączy się z CANH / CANL |
| moduł wzmacniacza I2S | MAX98357A | napędza głośnik tonu czujników parkowania |
| głośnik | 4-8 Ω, 2-3 W (autor użył głośnika 8 Ω / 0,5 W z zestawu skrzynki CAN i ogranicza głośność w programie) | ton czujników parkowania |
| przetwornica 12 V na 5 V | Traco TSR 1-2450 | zasilanie z auta |
| zabezpieczenie wejścia | bezpiecznik polimerowy, dioda od odwrotnej polaryzacji, transil SMBJ18A | chroni płytkę na 12 V auta |
| trzy klucze P-MOSFET | AO3407 ze sterownikami BC847 | wyjścia 12 V ACC, ILL, REV dla radia |

Pełna lista z wartościami jest na stronie [Lista elementów](../hardware/bom.md). Płytka to
dwuwarstwowa PCB trawiona w domu; plików do zamówienia płytki w fabryce jeszcze nie ma.

## Narzędzia {#tools}

- Multimetr z brzęczykiem do sprawdzania ciągłości. Służy do szukania pinów w wiązce i do
  sprawdzenia płytki przed pierwszym zasileniem.
- Lutownica (prototyp ma elementy SMD w obudowach 0805, SOT-23 i SMA/SMB).
- Zasilacz laboratoryjny z ograniczeniem prądu, ustawionym na ok. 100 mA przy pierwszym zasileniu.
- Przy nowym aucie bardzo zalecane: oscyloskop albo analizator logiczny, żeby przed podłączeniem
  zobaczyć typ i prędkość magistrali. Zobacz [Znajdź swoją magistralę](../make-it-yours/find-your-bus.md).

## Auto i radio {#the-car-and-the-radio}

| Co | Stan |
|---|---|
| Peugeot 508 SW 2015 (PSA AEE2010), profil `psa_aee2010` | zmierzone (measured) w jednym aucie; zobacz [Peugeot 508 SW](../reference/vehicles/peugeot-508.md) |
| każde inne auto, profil `generic` | tylko nasłuch: zapisuje ramki CAN, nic nie wysyła do radia i trzyma wyjścia 12 V wyłączone |
| radio z Androidem na platformie FYT, ustawione na profil CAN „XP Simple Soft -> Peugeot 508 -> Type 2”, protokół `XP` | zmierzone z jednym radiem (Junsun V3 Plus); zobacz [Protokół XP](../reference/xp-protocol.md) |

Inne auta PSA tej samej generacji prawdopodobnie używają wielu tych samych ramek, ale nikt ich nie
mierzył z tym projektem. Traktuj je jako **niezweryfikowane** (unverified).

## Umiejętności {#skills}

Nie musisz być programistą. Musisz umieć:

- wpisywać polecenia w terminalu (wystarczy kopiuj-wklej);
- używać multimetru i czytać rozkład pinów;
- lutować, jeśli budujesz płytkę bazową.

Napisanie profilu dla nowego auta to edycja małej tabeli w C++; strona
[Pisanie profilu auta](../make-it-yours/write-a-profile.md) prowadzi cię krok po kroku.

## Dalej {#next}

[Wgraj program](../quick-start/flash.md).
