# Udostępnij swoje auto

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Zmierzyłeś auto, którego nie ma w `vehicles/SUPPORTED.md`. Twoje notatki pomogą następnej osobie
z tym samym autem, nawet bez profilu w programie. Ta strona mówi, co udostępnić, jak to najpierw
oczyścić i gdzie trafia każdy plik. Ogólne zasady wkładu są w `CONTRIBUTING.md` w głównym katalogu
repozytorium.

To projekt hobbystyczny z jednym opiekunem. Nie ma obietnicy odpowiedzi ani przeglądu w żadnym
terminie.

## Co możesz udostępnić {#what-you-can-share}

| Wkład | Gdzie trafia |
|---|---|
| Opis auta i jego magistrali | `vehicles/<make>-<model>-<year>/README.md`, na podstawie `vehicles/_template/README.md` |
| Ramki i sygnały | `vehicles/<make>-<model>-<year>/<car>.dbc`, każdy komentarz zaczyna się od statusu |
| Widoki złączy i tabele pinów | `vehicles/<make>-<model>-<year>/wiring.md`, ze stroną, z której patrzyłeś |
| Nagrania używane przez test | `firmware/test_data/captures/`, z jednym wierszem w tamtejszym `README.md` |
| Profil w programie | `firmware/lib/car_<name>/` ([Pisanie profilu auta](write-a-profile.md)) |
| Jego test na komputerze | `firmware/test_host/<name>_car.cpp`, wzorzec w `firmware/test_host/expected/`, jeden blok w `firmware/tools/run_host_tests.sh` ([Testowanie profilu](test-your-profile.md)) |
| Co twoje radio robi z każdym kodem przycisku | twoja strona pojazdu, sekcja „Radio side” ([Przyciski i radio](radio-mapping.md)) |
| Wiersz na listę obsługiwanych aut | `vehicles/SUPPORTED.md`: pojazd, profil, status, co działa, link do twojej strony |

Pull request z samą stroną pojazdu jest mile widziany. Tak samo zgłoszenie (issue) z szablonem
„New vehicle”, jeśli nie używasz Gita.

## Zasady dla faktów {#the-rules-for-facts}

- Każdy fakt ma status: **zmierzone** (measured; jak: multimetr, nagranie, konsola urządzenia; dla
  nagrania plik i krok) albo **niezweryfikowane** (unverified; źródło: publiczna baza danych, forum,
  rysunek sprzedawcy).
- Wniosek to nie pomiar. „Napięcia wyglądają na CAN low-speed fault-tolerant” to wniosek; napisz,
  co by go potwierdziło.
- Pisz tylko to, co widziałeś. „Nie zmierzono” to dobry wpis.
- Tylko twoja własna praca: twoje nagrania, zdjęcia i słowa. Nie wysyłaj zdekompilowanego kodu,
  programów ani aplikacji producentów skrzynek czy radiów, tabel z nich wziętych, kart katalogowych
  ani kodu skopiowanego z projektów na niezgodnej licencji. Zamiast kopiować, podaj link do
  publicznego źródła.

## Najpierw oczyść nagrania {#clean-your-recordings-first}

Nagranie magistrali może zawierać VIN twojego auta i pokazuje, kiedy i jak jeździłeś. Oczyść każdy
plik, zanim opuści twój komputer.

### 1. Znajdź ramki, które niosą VIN {#1-find-the-frames-that-carry-the-vin}

W aucie wzorcowym VIN jest w ramkach `336`, `3B6` i `2B6` (zmierzone: nagrania auta
wzorcowego). Te trzy ID to wartości domyślne narzędzia do czyszczenia i `record.py`. **W innej
marce VIN może być w innych ramkach albo w ogóle nie na tej magistrali.** Sprawdź to, zanim
cokolwiek udostępnisz:

1. Weź VIN z dowodu rejestracyjnego. Nie wpisuj go do żadnego pliku, który udostępnisz.
2. VIN ma 17 liter i cyfr. Zamień pierwsze trzy znaki na szesnastkowe kody ASCII: na przykład
   `V` = `56`, `F` = `46`, `3` = `33`.
3. Wyszukaj te bajty w nagraniu w edytorze tekstu, a potem kody szesnastkowe pozostałych znaków.
   Każda ramka, której dane układają się w część VIN, to ramka VIN. VIN zapisany jako tekst (17 znaków)
   nie zmieści się w jednej 8-bajtowej ramce, więc spodziewaj się więcej niż jednej. Jeśli nic nie
   pasuje, auto może nie wysyłać VIN jako tekstu na tej magistrali; napisz to na stronie pojazdu.
4. Zapisz ID. Jeśli żadnego nie znajdziesz, napisz na stronie pojazdu „VIN frames: none found”.

### 2. Nadpisz je {#2-overwrite-them}

Z głównego katalogu repozytorium:

```bash
python -I tools/sanitize_capture.py my_drive.log my_drive_clean.log --ids 336,3B6,2B6,ABC
python -I tools/sanitize_capture.py --check my_drive_clean.log --ids 336,3B6,2B6,ABC
```

Zastąp `ABC` ID swoich ramek VIN (zostaw trzy domyślne; nie szkodzą). Narzędzie nadpisuje dane
tych ramek zerami o tej samej długości: liczba linii, czasy, ID i długości zostają bez zmian, więc
testy dalej działają. Zastępuje też linię nagłówka z nazwą twojego portu szeregowego przez
`# capture`. `--check` musi zgłosić `0 problem(s)`.

### 3. Sam przeczytaj komentarze {#3-read-the-comments-yourself}

Narzędzie kopiuje linie komentarzy (`# ...`) bez zmian. Są w nich teksty twoich znaczników. Otwórz
plik i poszukaj nazwisk, miejsc, adresów, nazw sieci i wszystkiego innego, co prywatne.

### 4. Przeskanuj wszystko, co dodajesz {#4-scan-everything-you-add}

```bash
python -I tools/scan_private.py .
```

Szuka numerów VIN, ramek VIN z danymi, adresów e-mail, ścieżek użytkownika Windows, adresów MAC
i nazw sieci urządzenia z prawdziwym przyrostkiem. Musi zakończyć się bez błędów.

> [!WARNING]
> **Skaner zna tylko format VIN auta wzorcowego**
>
> `scan_private.py` rozpoznaje VIN grupy PSA (`VF3` + 14 znaków) i ramki VIN PSA. Nie zna VIN
> innej marki ani twoich własnych ramek VIN. Kroki 1 do 3 nadal należą do ciebie.

## Zanim otworzysz pull request {#before-you-open-a-pull-request}

Szablon pull requestu ma listę kontrolną. Dla nowego auta liczą się te części:

| Sprawdzenie | Polecenie | Oczekiwany wynik |
|---|---|---|
| Nagrania oczyszczone | `python -I tools/sanitize_capture.py --check <files> --ids ...` | `0 problem(s)` |
| Brak danych prywatnych | `python -I tools/scan_private.py .` | bez błędów |
| Testy na komputerze (z `firmware/`) | `bash tools/run_host_tests.sh` | `All tests match the references.` |
| Warstwy (z `firmware/`) | `python -I tools/check_layering.py` | `Layering OK.` |
| Twoja kompilacja (z `firmware/`) | `uvx platformio run -e listen_mycar` | `SUCCESS` |
| Linki dokumentacji, jeśli zmieniałeś `docs/` | `python -I tools/check_doc_links.py` | brak martwych linków |

Twój `platformio_override.ini` zostaje na twoim komputerze: nie jest śledzony. Napisz w pull
requeście, którego środowiska kompilacji użyłeś dla swojego auta i czy próbowałeś programu na
płytce i w aucie.

W pull requeście, który dodaje auto, nie zmieniaj istniejących wzorców testów
(`firmware/test_host/expected/`, `golden/`, `golden_060/`). Dodanie własnego pliku wzorca jest
w porządku.

## Nadawanie nie jest częścią wkładu z autem {#transmission-is-not-part-of-a-car-contribution}

Profil zbudowany z `-DCAR_PROFILE=CUSTOM` nie może nadawać, a program nie ma sposobu, żeby dodać
ramkę nadawania w czasie pracy. Ramka, którą urządzenie może wysłać, wymaga własnego pomiaru,
sprawdzenia treści napisanego w C++, testu na komputerze i nazwanego profilu
w `firmware/src/config.h`. To osobna dyskusja, nie część dodawania auta: przeczytaj
[Nadawanie na magistralę CAN](../safety/transmit.md) i najpierw otwórz zgłoszenie.

## Licencje {#licences}

Wysyłając wkład, zgadzasz się na jego publikację na licencjach tego repozytorium: kod Apache-2.0,
dokumentacja CC BY 4.0, sprzęt CERN-OHL-S-2.0. Te licencje są tymczasowe do pierwszego publicznego
wydania.

Ten projekt nie jest związany z żadnym producentem aut, radiów ani skrzynek CAN. Wymieniaj ich
tylko po to, żeby powiedzieć, co testowałeś.
