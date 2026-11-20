# Nagrania z auta

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Nagranie (capture) to plik tekstowy z każdą ramką CAN, którą widziało urządzenie, w kolejności i
z czasem. Testy na komputerze odtwarzają nagrania z auta wzorcowego. Własne robisz tak samo.

## Format {#format}

Jedno zdarzenie w linii, w stylu `candump -L` (narzędzia CAN w Linuksie):

```text
(13.497421) can0 0F6#805C1E23225D5C20     CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117              bytes seen on one UART line between a CAN box and the radio
# STEP 3 19:06:41.149 press volume up     comment: a marker
```

- `ID` to trzy cyfry szesnastkowe dla ramki standardowej, osiem dla rozszerzonej. `DATA` to bajty
  danych szesnastkowo, bez spacji.
- Linie zaczynające się od `#` to komentarze. Parser ramek (`firmware/lib/core/candump.cpp`) je
  pomija, więc znaczniki nigdy nie psują testu.
- Linie `uartA` / `uartB` są tylko w nagraniach z rejestratora UART (patrz niżej).

Taką samą linię `can0 ID#DATA` urządzenie wypisuje na USB po poleceniu `dump`. Nie zmieniaj tego
formatu: czytają go narzędzia.

## Nagrania w repozytorium {#the-recordings-in-the-repository}

Wszystkie w `firmware/test_data/captures/`, wszystkie z auta wzorcowego (Peugeot 508 SW, PSA
AEE2010, magistrala przy radiu, 125 kbit/s), wszystkie nagrane w trybie tylko do nasłuchu.

| Plik | Czym nagrany | Scenariusz |
|---|---|---|
| `can_20261110_190421.log` | sniffer (tylko CAN), podłączona skrzynka CAN z rynku wtórnego | długa sesja: zapłon, silnik, przyciski przy kierownicy, światła, drzwi, klimatyzacja z panelu auta i z radia, czujniki parkowania przód i tył, próby z tonem czujników, uśpienie |
| `can_20261111_190551.log` | rejestrator (CAN + obie linie UART) | zapłon wyłączony -> włączony, trzy razy głośność w górę |
| `can_20261111_190817.log` | rejestrator (CAN + UART) | przyciski przy kierownicy, przycisk komputera pokładowego, drzwi, bagażnik |
| `can_20261111_191045.log` | rejestrator (CAN + UART) | drzwi, światła, silnik, wsteczny, czujniki parkowania, kierownica, klimatyzacja, ekrany radia, ustawienia pojazdu z radia, przyciski panelu środkowego, zgaszenie silnika |
| piąty plik (patrz README katalogu) | ten program w trybie dekoder, bez skrzynki CAN | około 5 minut samego CAN, czujniki nieaktywne, jedna przerwa ze zgubionymi ramkami |

Znaczniki kroków w tych plikach są po polsku (`# KROK` = step), tak jak zostały nagrane.

## Jak zanonimizowano nagrania {#how-the-recordings-were-anonymised}

W tym aucie ramki `336`, `3B6` i `2B6` niosą VIN (numer identyfikacyjny pojazdu) jako tekst.
Przed dodaniem plików:

- dane tych trzech ramek nadpisano zerami tej samej długości; czas, ID i długość zostały;
- linię nagłówka z nazwą portu szeregowego komputera, na którym nagrywano, zastąpiono `# capture`.

Nic więcej się nie zmieniło. Wyniki testów na komputerze są identyczne dla plików oryginalnych i
zanonimizowanych.

> [!WARNING]
> **Zamaskuj VIN, zanim udostępnisz nagranie**
>
> Przepuść każdy plik przez program czyszczący, zanim dołączysz go do zgłoszenia (issue) albo
> pull requestu:
>
> ```bash
> python -I tools/sanitize_capture.py my_car.log my_car_clean.log
> python -I tools/sanitize_capture.py --check my_car_clean.log
> ```
>
> `--check` kończy się kodem 1, jeśli została prywatna ramka albo nagłówek z portem. W innym
> aucie VIN może być w innych ramkach: najpierw je znajdź i podaj przez `--ids 3B6,2B6,...`.

## Zrób nagranie {#make-a-recording}

Trzy sposoby, wszystkie w trybie tylko do nasłuchu:

| Sposób | Narzędzie | Uwagi |
|---|---|---|
| konsola USB urządzenia | wpisz `dump` w terminalu szeregowym, zapisz wynik | urządzenie wypisuje każdą ramkę jako `can0 ID#DATA`; ponowne `dump` zatrzymuje |
| panel Wi-Fi urządzenia | `python -I firmware/tools/record.py` na komputerze połączonym z siecią urządzenia | zapisuje ramki CAN i linie konsoli do dwóch plików, poza repozytorium; pyta o hasło urządzenia bez wyświetlania go; ramki z VIN zapisuje jako `# masked`, a przerwy jako `# lost N frames` |
| osobny sniffer lub rejestrator | `extras/sniffer/` i jego `record_serial.py` | wariant z rejestratorem zapisuje też linie UART między skrzynką CAN a radiem |

`record.py` i `record_serial.py` pozwalają dodawać znaczniki w trakcie nagrywania; znacznik trafia
do pliku jako linia `#`. W `record.py` wpisujesz tekst i naciskasz Enter: linia ma postać
`# MARKER n ... text`, n liczone od 1. W `record_serial.py` pusty Enter zapisuje kolejny krok z
pliku scenariusza (`# STEP n HH:MM:SS.mmm text`), a cyfra od `1` do `9` + Enter zapisuje
`# MARK d HH:MM:SS.mmm` (d = wpisana cyfra). Szczegóły: [Nagrywanie](../make-it-yours/record.md).

> [!NOTE]
> **Nagrywanie w panelu w przeglądarce gubi ramki**
>
> Widok CAN w panelu nagrywa tylko wtedy, gdy jego karta jest widoczna. Gdy autor przełączył się
> na inną kartę w trakcie nagrania, zginęły dziesiątki tysięcy ramek (zmierzone (measured): jedno
> nagranie z panelu). Do wszystkiego dłuższego niż szybki podgląd używaj `record.py`.

## Zrób nagranie, które łatwo zdekodować {#make-a-recording-that-is-easy-to-decode}

- **Jedna czynność na krok.** Najpierw spisz kroki (patrz `extras/sniffer/scenarios/example_buttons.txt`).
- **Powtórz każdą czynność trzy razy**, z kilkusekundową przerwą. Bit, który zmienia się trzy razy
  przy znacznikach, to twój sygnał; bit, który zmienia się cały czas, to licznik.
- **Przed każdą czynnością postaw znacznik.**
- Zacznij od kilku sekund bez żadnej czynności, żeby zobaczyć stan spoczynkowy.
- Zapisz stan auta: zapłon, silnik, drzwi, co pokazywało radio.

Jak znaleźć sygnał w nagraniu: [Dekodowanie](../make-it-yours/decode.md).
Jak zrobić z niego test: [Testowanie profilu](../make-it-yours/test-your-profile.md).
