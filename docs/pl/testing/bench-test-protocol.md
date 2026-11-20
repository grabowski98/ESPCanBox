# Protokół testu na stole

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Krótkie sprawdzenie zmontowanej płytki **na biurku, nie w aucie**. Zrób je po zmontowaniu płytki i
po każdej wersji programu, którą chcesz wgrać do auta. Obejmuje to, czego nie sięgają
[testy na komputerze](host-tests.md): sterownik CAN, UART, wyjścia 12 V, Wi-Fi i aktualizacje.

Wydrukuj tę stronę albo skopiuj tabele i wypełnij ostatnią kolumnę: `pass`, `fail` albo to, co
zobaczyłeś.

> [!CAUTION]
> **Nie podłączaj płytki do auta w trakcie tego protokołu**
>
> Każdy krok odbywa się na biurku. Zostaw zaciski CAN wolne albo podłączone tylko do magistrali
> testowej, którą sam zbudowałeś. Na żadnym kroku nie uzbrajaj nadawania (`cantx on`).

## Co jest potrzebne {#what-you-need}

- zmontowana płytka i kabel USB-C do transmisji danych;
- komputer z narzędziami z [Wgraj program](../quick-start/flash.md);
- terminal szeregowy 115200 bodów (monitor PlatformIO wystarczy);
- multimetr;
- do kroków z 12 V: zasilacz laboratoryjny ustawiony na 12 V z ograniczeniem prądu (zacznij nisko,
  na przykład 0,3 A);
- do kroków z panelem: telefon albo laptop z Wi-Fi;
- opcjonalnie: przejściówka USB-UART 3,3 V oraz magistrala CAN z co najmniej jednym innym
  działającym węzłem.

## Karta wyników {#record-sheet}

| Pozycja | Wartość |
|---|---|
| Płytka (nazwa, wersja) | |
| Wersja programu (z `ver`) | |
| Czas budowania (z `ver`) | |
| Kto testował | |
| Wynik ogólny | |

## 1. Wgranie przez USB i start {#1-flash-over-usb-and-start}

Wgraj domyślne środowisko `listen` (tylko nasłuch, bez sieci, bez kodu nadawania):

```bash
cd firmware
uvx platformio run -e listen -t upload
uvx platformio device monitor -b 115200
```

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 1.1 | Wgrywanie się kończy | PlatformIO kończy na `SUCCESS` | |
| 1.2 | Linie startowe na USB | linia z `LISTEN ONLY. Mode: alongside the box (sends nothing).` | |
| 1.3 | Linia profilu | `# profile: car=psa_aee2010 radio=xp` | |
| 1.4 | Linia sterowników | `# can=ok audio=ok radio=ok` (jakikolwiek `FAIL` = stop, patrz [Rozwiązywanie problemów](../troubleshooting/index.md#the-device-reports-fail-at-start)) | |
| 1.5 | Wpisz `ver` | `[ver] <product> <version> listen, built <date time>` | |
| 1.6 | Wpisz `help` | lista poleceń | |
| 1.7 | Wpisz `test` | 1,5 s piknięć z głośnika | |

## 2. Stan i odbiór {#2-status-and-reception}

Wpisz `stat`. Pierwsze linie wyglądają tak:

```text
[stat] mode=alongside the box can=ok audio=ok radio=ok
[stat] rx=0 lost=0 errors=0 bus=quiet volume=40% ...
```

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 2.1 | `stat`, nic na CAN | `rx=0 lost=0 errors=0 bus=quiet`, dioda zgaszona | |
| 2.2 | Ostatnia linia `stat` | `[stat] can-tx=none sent=0 acked=0 failed=0 refused=0` | |
| 2.3 | Wklej `0E1#B0F000FCFC1EC200` i naciśnij Enter | logika przyjmuje to jak ramkę z auta; linia `[park]` następnego `stat` się zmienia | |
| 2.4 | Opcjonalnie, z magistralą CAN, na której jest inny działający węzeł z właściwą prędkością: podłącz CANH i CANL, odczekaj 10 s, `stat` | `rx` rośnie, `lost=0`, `errors=0`, `bus=active`, dioda zielona | |
| 2.5 | Opcjonalnie: wpisz `dump`, potem jeszcze raz `dump` | ramki wypisywane jako `(s.ms) can0 ID#DATA`, potem koniec | |

> [!NOTE]
> **Urządzenie w trybie tylko do nasłuchu nigdy nie potwierdza ramki**
>
> Na magistrali testowej, na której jest tylko jeden nadajnik i to urządzenie, nikt nie
> potwierdza ramek i nadajnik powtarza je z błędami. Potrzebny jest drugi węzeł, który potwierdza,
> tak jak w aucie.

## 3. UART do radia jest wyłączony w `listen` {#3-uart-to-the-radio-is-off-in-listen}

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 3.1 | Wpisz `radio` | pierwsza linia zawiera `transmit=no` | |
| 3.2 | Wpisz `key A5` | `[key] trial code A5: not sent (listen mode)` | |
| 3.3 | Opcjonalnie: RX przejściówki na polu idącym do radia (w prototypie v1: J4 „radio RX”), 38400 8N1, wspólna masa | żadnych bajtów, także po 3.2 | |

W `listen` program nie przydziela pinu wyjściu UART, więc linia nie może być wysterowana (z kodu,
`firmware/src/radio_uart.cpp`).

## 4. Wyjścia 12 V zostają wyłączone w `listen` {#4-the-12-v-outputs-stay-off-in-listen}

> [!WARNING]
> **Użyj zasilacza z ograniczeniem prądu**
>
> Podłącz zasilacz laboratoryjny do zacisków +12 V i GND z ustawionym ograniczeniem prądu. Jeśli
> prąd skoczy do ograniczenia, wyłącz zasilanie i poszukaj zwarcia, zanim pójdziesz dalej.

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 4.1 | 12 V włączone, USB nadal podłączone | prąd zasilania stabilny, linie startowe jak w kroku 1 | |
| 4.2 | Zmierz wyjścia ACC, ILL i REV względem GND | 0 V na każdym | |
| 4.3 | Wklej ramkę włączonego zapłonu `0F6#885C1E23225D5C20` | 0 V na każdym wyjściu, niezależnie od tego, co pokazuje `stat` | |
| 4.4 | `stat` | wartość `supply=` bliska napięciu zasilacza minus spadek na diodzie wejściowej | |

W `listen` pętla główna zawsze wyłącza wyjścia (z kodu, `firmware/src/main.cpp`).

> [!NOTE]
> **Wyjścia w trybie `decoder`**
>
> Raportu z testu na stole, w którym wyjścia **włączają się** w `decoder`, jeszcze nie ma. Jeśli
> to testujesz, wgraj `decoder`, wklej ramkę zapłonu z 4.3 i zapisz, które wyjścia się włączają
> i na jak długo. Nigdy nie rób tego ze skrzynką CAN podłączoną do tych samych wyjść.

## 5. Nadawanie nie jest uzbrojone {#5-transmission-is-not-armed}

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 5.1 | W `listen`: wpisz `cantx` | `[cantx] built=0: no CAN transmit in this build (CAN listen only)` | |
| 5.2 | Wpisz `cantx on` | `[cantx] REFUSED: this program cannot transmit on CAN` | |

Tylko jeśli celowo budujesz środowisko z nadawaniem (najpierw przeczytaj [Nadawanie na magistralę CAN](../safety/transmit.md)).

> [!CAUTION]
> **decoder_tx_wifi to środowisko dekodera**
>
> Może włączyć wyjścia 12 V i nadawać po UART do radia. W trakcie tych kroków nie podłączaj
> niczego do wyjść ani skrzynki CAN.

Wgraj je:

```bash
uvx platformio run -e decoder_tx_wifi -t upload
```

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 5.3 | Wgraj `decoder_tx_wifi`; linia startowa | `TRANSMIT 15B: built, armed=0` | |
| 5.4 | Następna linia startowa | `# CAN controller: LISTEN ONLY (transmit disarmed)` | |
| 5.5 | Wpisz `cantx` | `[cantx] built=1 armed=0 after_restart=0 controller=listen ...` | |
| 5.6 | Wpisz `set 01 01` | linia `[vset]` z `REFUSED`; nic nie zostało wysłane | |

Przed sekcją 6 wgraj z powrotem `listen_wifi`.

## 6. Panel WWW w dwóch językach {#6-web-panel-in-two-languages}

Wgraj `listen_wifi`:

```bash
uvx platformio run -e listen_wifi -t upload
```

Nie odłączaj USB: z komputerem na USB urządzenie trzyma Wi-Fi włączone nawet bez ruchu na
magistrali.

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 6.1 | Wpisz `wifi` | `[wifi] device network: <name>, address http://10.50.8.1`; bez hasła w tym wyniku | |
| 6.2 | Wpisz `wifi showpass` | hasło urządzenia. Nie wklejaj tej linii do żadnego zapisu ani zgłoszenia | |
| 6.3 | Połącz telefon z siecią urządzenia, otwórz `http://10.50.8.1` | panel się wczytuje i pyta o hasło | |
| 6.4 | Zaloguj się | karty stanu się wypełniają; liczniki CAN jak w `stat` | |
| 6.5 | Wybór języka: English | wszystkie etykiety po angielsku | |
| 6.6 | Wybór języka: Polski | wszystkie etykiety po polsku, bez angielskich resztek, bez surowych kluczy w rodzaju `ota.ask.ok` | |
| 6.7 | Odśwież stronę | wybrany język zostaje (jest zapamiętany tylko w tej przeglądarce) | |
| 6.8 | Dowolny inny język | informacja, że tłumaczenie jest maszynowe i wymaga sprawdzenia | |
| 6.9 | Karta konsoli: wpisz `stat` | ten sam wynik co na USB | |

## 7. Aktualizacja programu przez Wi-Fi {#7-firmware-update-over-wi-fi}

Zbuduj nowy obraz tego samego środowiska (każde ponowne budowanie zmienia czas budowania):

```bash
uvx platformio run -e listen_wifi
```

Plik do wysłania to `firmware/.pio/build/listen_wifi/firmware.bin`, nie obraz połączony (merged).

| # | Krok | Oczekiwane | Wynik |
|---|---|---|---|
| 7.1 | Panel: aktualizacja, wybierz plik | przed wysłaniem widać wersję, czas budowania i tryb pliku | |
| 7.2 | Wyślij | postęp, restart, panel wraca; „Gotowe” po około 20 s | |
| 7.3 | `ver` na USB | nowy czas budowania | |
| 7.4 | Panel: naciśnij **Wróć do poprzedniego** | poprzedni czas budowania w `ver` | |
| 7.5 | Wybierz plik, który nie jest programem (dowolny inny plik) | odrzucony przed wysłaniem: brak znacznika tego urządzenia | |
| 7.6 | Wybierz obraz `decoder_wifi` | osobne ostrzeżenie o zmianie trybu; anuluj je | |
| 7.7 | Po 7.2 i 7.4: linie startowe na USB | `# can=ok audio=ok radio=ok` i linia `# start:` z przyczyną | |

> [!WARNING]
> **Nowa nazwa projektu blokuje aktualizacje ze starszych obrazów**
>
> Aktualizacja sprawdza nazwę produktu w obrazie. Obraz z inną nazwą produktu jest odrzucany
> (`wrong_product`). Po zmianie nazwy projektu pierwsze wgranie z nową nazwą musi pójść przez USB.

## 8. Po protokole {#8-after-the-protocol}

- Wgraj środowisko, którego chcesz używać w aucie (zwykle najpierw `listen_wifi`).
- Zachowaj wypełnioną kartę razem z wersją programu. Dołącz ją do pull requestu, jeśli zmieniłeś
  cokolwiek w `firmware/src/` albo `firmware/boards/`.

Dalej: zanim pójdziesz do auta, zobacz [Lista kontrolna montażu](../install-in-car/checklist.md).
