# Nadawanie na magistralę CAN

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Urządzenie **tylko słucha**, chyba że celowo zbudujesz jedyne środowisko z nadawaniem i je
uzbroisz. Przeczytaj tę stronę, zanim to zrobisz: mówi, jak włączyć i wyłączyć nadawanie, co to
środowisko może wysłać i co je zatrzymuje.

> [!CAUTION]
> **Nadawanie zmienia stan auta**
>
> Ramka wysłana na magistralę auta może zmienić ustawienia w module nadwozia (BSI). Kompiluj
> środowisko z nadawaniem tylko wtedy, gdy rozumiesz tę stronę, tylko dla auta wzorcowego
> i nigdy w czasie jazdy. To projekt hobbystyczny, udostępniony w obecnym stanie, i nie nadaje
> się do żadnego układu bezpieczeństwa.

## Włącz nadawanie krok po kroku {#switch-transmitting-on-step-by-step}

Kroki 1 do 7 wykonaj na stole, nie w aucie.

> [!CAUTION]
> **Tryb dekoder: najpierw wypnij skrzynkę CAN**
>
> `decoder_tx_wifi` to środowisko dekodera. Wysyła ramki do radia i przełącza wyjścia 12 V.
> Najpierw wypnij skrzynkę CAN z rynku wtórnego: dwa nadajniki na linii radia i dwa źródła ACC
> zwierają wyjścia. Na stole nie podłączaj niczego do wyjść 12 V.

1. Przygotuj płytkę. Otwarta zworka JP1 oznacza, że moduł CAN nie może nadawać, niezależnie od
   programu: do codziennej pracy zostaw ją otwartą. Do nadawania prototyp v1 potrzebuje zwartej
   JP1 **i** przewodu z SLNT do GPIO6 ([Płytka prototypu](../hardware/prototype-pcb.md#transmit-path)).
   W prototypie autora JP1 jest zwarta. Sprawdź przewód SLNT multimetrem (zobacz
   [Dwa stany kontrolera](#two-states-of-the-controller)).
2. Otwórz terminal w głównym katalogu repozytorium i wgraj środowisko z nadawaniem:

    ```bash
    cd firmware
    uvx platformio run -e decoder_tx_wifi -t upload
    ```

    Oczekiwane: PlatformIO kończy na `SUCCESS`. Jak zainstalować narzędzia:
    [Wgraj program](../quick-start/flash.md).
3. Otwórz [konsolę USB](../reference/console.md) i naciśnij RESET na płytce. Oczekiwane linie
   startowe: `TRANSMIT 15B: built, armed=0` i `# CAN controller: LISTEN ONLY (transmit disarmed)`.
   Nowy program zawsze startuje rozbrojony.
4. Wykonaj sekcję 5 [protokołu testu na stole](../testing/bench-test-protocol.md). Nie idź dalej,
   jeśli któryś krok się nie uda.
5. Wpisz `cantx on`. Oczekiwane: `[cantx] arming stored. Takes effect AFTER A RESTART ...`. Jeśli
   odpowiedź to `[cantx] skipped: the car needs the device ...`, poczekaj, aż bieg wsteczny
   i czujniki parkowania będą wyłączone, i wpisz to jeszcze raz.
6. Zrestartuj urządzenie: naciśnij RESET na płytce, wyłącz i włącz jego zasilanie albo naciśnij
   **Restart urządzenia** w panelu (zakładka System).
7. Wpisz `cantx`. Oczekiwane: `[cantx] built=1 armed=1 after_restart=1 controller=NORMAL ...`.
   Linie startowe mówią teraz `armed=1` i `# CAN controller: NORMAL, ...`. Jeśli nadal widzisz
   `armed=0`, poszukaj w liniach startowych `# CAN transmit arming stored by another program: cleared`
   albo linii `# ERROR:`.
8. W aucie, przy włączonym zapłonie, wyślij ustawienie z ekranu radia (albo `set NN VV` przez
   USB). Domyślnie włączona jest tylko pozycja `01` (asystent parkowania). Jeśli brama odmawia,
   zobacz [Kiedy brama odmawia](#when-the-gate-refuses).
9. Po teście wpisz `cantx off`. Oczekiwane: `[cantx] DISARMED immediately`. Kontroler wraca do
   trybu tylko nasłuchu przy następnym restarcie. Do codziennej pracy wgraj z powrotem środowisko
   tylko do nasłuchu i otwórz JP1.

## Które kompilacje mogą nadawać {#which-builds-can-transmit}

| Środowisko | Czy może nadawać na CAN? |
|---|---|
| `listen`, `decoder`, `listen_wifi`, `decoder_wifi`, `listen_generic_wifi` | **Nie.** Katalog `src/can_tx/` (jedyny kod, który wywołuje funkcję nadawania `twai_transmit`) nie jest kompilowany. Kontroler CAN zawsze startuje w trybie tylko do nasłuchu: nie wysyła ramek, bitów potwierdzenia (ACK) ani ramek błędu |
| `decoder_tx_wifi` | Tylko ramki z listy dozwolonych profilu auta, tylko po uzbrojeniu i restarcie. Bez uzbrojenia działa jak `decoder_wifi` |

`tools/check_no_can_tx.sh` sprawdza skompilowane programy: pięć obrazów tylko do nasłuchu nie może
zawierać `twai_transmit`, obraz `decoder_tx_wifi` musi go zawierać, a w źródłach wywoływać go może
tylko `src/can_tx/can_tx_port.cpp`.

Reszta strony pokazuje, jak brama działa w środku. Każda liczba jest przepisana z kodu:
`lib/core/can_tx_gate.*`, `lib/core/can_tx_arm.*`, `src/can_tx/*`,
`lib/car_psa_aee2010/tx_allowlist.cpp` i `src/config.h` (ścieżki względem `firmware/`).

## Blokady przy kompilacji {#locks-when-you-build}

`decoder_tx_wifi` ustawia `-DCAN_TX_MODE=CAN_TX_SETTINGS` i `-DCAN_TX_EXPERIMENTAL=0`. Kompilacja
zatrzymuje się z błędem, gdy:

- `CAN_TX_MODE` jest ustawione bez trybu dekoder (`WORK_MODE` musi być `MODE_DECODER`);
- profil auta to `GENERIC` (profil tylko do nasłuchu) albo `CUSTOM` (twój własny profil: jego
  listy dozwolonych nie da się sprawdzić przy kompilacji);
- profil auta to `EXAMPLE` (to nigdy nie jest dozwolone, z nadawaniem ani bez);
- lista dozwolonych profilu jest pusta (`static_assert` w `src/config.h`);
- `src/can_tx/` jest kompilowany bez `CAN_TX_MODE = CAN_TX_SETTINGS`;
- `CAN_TX_EXPERIMENTAL` ma wartość inną niż 0 albo 1.

`tools/check_tx_profile_lock.sh` kompiluje `decoder_tx_wifi` z `GENERIC` i z `EXAMPLE`
w katalogu tymczasowym i przechodzi tylko wtedy, gdy obie kompilacje kończą się oczekiwanym
komunikatem błędu.

Obraz niesie też znacznik `canTx` (1 tylko w `decoder_tx_wifi`). Aktualizacja przez Wi-Fi do
obrazu z inną wartością `canTx` wymaga osobnej zgody w panelu.

## Co wolno wysłać {#what-may-be-sent}

Lista dozwolonych profilu PSA (`lib/car_psa_aee2010/tx_allowlist.cpp`) to dane `const` w pamięci
flash. Nic nie może dodać wiersza, gdy urządzenie pracuje.

| Ramka | Kompilacja | Długość | Sprawdzanie treści |
|---|---|---|---|
| `15B` żądanie personalizacji | domyślna (`CAN_TX_EXPERIMENTAL=0`) i eksperymentalna | 8 | **brak: tylko identyfikator i długość.** Bajty są liczone z bieżącej ramki `260` (`vset::build15B`). Sprawdzanie treści to osobna, otwarta decyzja |
| `1A9` przycisk ekranu | tylko z `-DCAN_TX_EXPERIMENTAL=1` | 8 | dokładnie jedna z dwóch treści, obie nagrane ze skrzynki z rynku wtórnego razem z reakcją auta: `00 7F FF 04 FF 00 10 00` (czujniki parkowania), `00 7F FF 20 FF 00 10 00` (martwe pole) |

Brama sprawdza ramkę w tej kolejności: identyfikator na liście (pierwszy pasujący wiersz), potem
długość (`BAD_LEN`), potem treść. Warstwa sprzętowa sprawdza każdą ramkę **drugi raz** według tej
samej listy, zanim przekaże ją kontrolerowi.

> [!CAUTION]
> **Kompilacja eksperymentalna**
>
> `-DCAN_TX_EXPERIMENTAL=1` dodaje `1A9`. Ustawiaj to tylko we własnym `platformio_override.ini`,
> świadomie. W aucie wzorcowym `1A9` została przyjęta tylko częściowo (zobacz
> [niżej](#what-was-tried-in-the-car)). Żadna wersja nie ma ramki klimatyzacji (`364`) ani treści
> `1A9` dla Stop and Start: obie usunięto 19.11.2026, bo nigdy nie zmierzono ich skutku.
> Tej kompilacji nie ma w CI: jej logikę budują testy na komputerze, a w PlatformIO skompilowano ją
> raz ręcznie (19.11.2026, bez ostrzeżeń); z tego drzewa źródeł nigdy jej nie wgrywano.

Profil `generic` nie ma listy dozwolonych: jego brama odrzuca każdą ramkę.

## Uzbrajanie {#arming}

Uzbrojenie jest zapisane w NVS (obszarze ustawień w pamięci flash) jako **napis kompilacji**
programu, w którym je włączyłeś (data i godzina kompilacji, w postaci `Mmm dd yyyy hh:mm:ss`).
Przy starcie zapisany napis musi być równy napisowi działającego programu; porównywane są
najwyżej 23 znaki. Inny, brakujący, pusty albo za długi = rozbrojone, a zapisana wartość jest
kasowana. Dlatego każdy nowo wgrany program startuje rozbrojony, także gdy wgrasz z powrotem stary.

> [!WARNING]
> **Najpierw uzbrajaj na stole**
>
> Uzbrajaj i testuj na stole, zanim pójdziesz do auta. Wpisz `cantx off`, gdy tylko test się
> skończy, żeby urządzenie nie było uzbrojone po następnym restarcie.

| Polecenie | Co robi | Skąd |
|---|---|---|
| `cantx` | wypisuje stan | skądkolwiek |
| `cantx on` | zapisuje uzbrojenie; działa **po restarcie** | USB albo panel WWW przez własną sieć Wi-Fi urządzenia |
| `cantx off` | rozbraja **od razu** i od razu kasuje zapisane uzbrojenie | USB albo panel przez własną sieć urządzenia |
| `cantx reset` | zdejmuje [blokadę po błędzie](#the-fault-latch) | tylko USB |
| `vset on NN`, `vset off NN` | włącza albo wyłącza jedną pozycję ustawień (zapisane w NVS) | USB albo panel przez własną sieć urządzenia |
| `set NN VV` | ta sama ścieżka co polecenie radia `80 NN VV`, do testu bez radia | tylko USB |

Z panelu przez sieć domową te polecenia są odrzucane. `cantx on` i `vset on|off` są też odrzucane,
gdy auto potrzebuje urządzenia (bieg wsteczny albo aktywne czujniki parkowania).

## Dwa stany kontrolera {#two-states-of-the-controller}

Tryb kontrolera jest wybierany raz, przy starcie (`canBegin()` w `src/can_bus.cpp`):

| Stan | Kontroler CAN | Nóżka SLNT transceivera |
|---|---|---|
| rozbrojony (i każda kompilacja tylko do nasłuchu) | tylko nasłuch | wysoki: nadajnik wyłączony |
| uzbrojony | tryb normalny, bez kolejki nadawania | wysoki, poza oknem jednej ramki |

Nóżka SLNT („silent”, wyciszenie) transceivera przełącza jego nadajnik: wysoki = nadajnik
wyłączony, odbiornik działa. Program ustawia ją w stan wysoki jako pierwszą instrukcję `setup()`
i jeszcze raz przed startem sterownika CAN. Tylko `src/can_tx/can_tx_port.cpp` ustawia ją w stan
niski.

Warstwa sprzętowa potrzebuje też sprzętowego timera (`esp_timer`). Jeśli nie da się go utworzyć,
urządzenie zostaje rozbrojone i wypisuje linię z błędem.

> [!CAUTION]
> **SLNT to jedyna bariera sprzętowa**
>
> Otwarta JP1 = nadawanie niemożliwe; do codziennej pracy zostaw ją otwartą. W prototypie autora
> JP1 jest zwarta, a SLNT jest podłączona do GPIO6. Transceiver (TJA1051T/3 w module CAN,
> według pliku nagłówkowego płytki) ściąga swoją nóżkę S **w stan niski wewnątrz układu**, więc
> przerwany przewód SLNT oznacza aktywny nadajnik. Rezystor 10 kΩ (R3) do 3,3 V trzyma ją
> w stanie wysokim, gdy program nią nie steruje. Sprawdź ten przewód multimetrem, zanim
> podłączysz urządzenie do auta. Szczegóły: `boards/board_proto_v1.h`.

## Kiedy brama odmawia {#when-the-gate-refuses}

Żądanie przechodzi przez te sprawdzenia w tej kolejności. Pierwsze, które nie przejdzie, daje
odmowę:

1. nadawanie nie wkompilowane (`NOT_BUILT`), rozbrojone (`DISARMED`), zablokowane (`FAULT`);
2. ramki nie ma na liście albo ma złą treść (`ID_NOT_ALLOWED`), zła długość (`BAD_LEN`);
3. wysyłanie albo przerwa po nim jeszcze trwa (`BUSY`);
4. brak zapłonu (`IGNITION`): musi być ramka nadwozia młodsza niż 1 s, odebrana z magistrali
   (nie wklejona), z włączonym zapłonem albo pracującym silnikiem (PSA: `0F6` bajt 1 = `88` albo
   `8E`);
5. brak ruchu na magistrali w ostatniej sekundzie (`BUS_QUIET`);
6. sesja aktualizacji przez Wi-Fi, zapis do pamięci flash albo oczekujący zapis ustawień
   (`FLASH_BUSY`);
7. stan kontrolera: bus-off albo licznik błędów nadawania (TEC) równy 32 lub więcej blokuje
   bramę. Kontroler, który nie działa, licznik błędów odbioru (REC) powyżej 0 albo błąd magistrali
   sprzed mniej niż 100 ms dają chwilową odmowę (`CONTROLLER`);
8. limit ramek (`RATE`): najwyżej **10 ramek w dowolnych 60 s**.

Konsola wypisuje odmowę jako tekst. Co zrobić:

| Odmowa | Tekst w konsoli | Co zrobić |
|---|---|---|
| `NOT_BUILT` | `no CAN transmit in this build` | wgraj `decoder_tx_wifi` (krok 2 wyżej) |
| `DISARMED` | `transmit disarmed` | wpisz `cantx on`, zrestartuj, sprawdź przez `cantx` (kroki 5 do 7) |
| `FAULT` | `transmit locked after a fault` | zapisz przyczynę, którą wypisuje `cantx` ([Blokada po błędzie](#the-fault-latch)), potem wpisz `cantx reset` przez USB |
| `ID_NOT_ALLOWED` | `frame not on the list` | ta kompilacja nie pozwala na tę ramkę; w czasie pracy nic tego nie zmieni |
| `BAD_LEN` | `wrong frame length` | to samo: ramka nie jest dozwolona |
| `BUSY` | `previous send in progress` | chwilowe: zmiana ustawienia ponawia sama |
| `IGNITION` | `no ignition` | włącz zapłon albo uruchom silnik |
| `BUS_QUIET` | `bus is quiet` | auto śpi: otwórz je i włącz zapłon |
| `FLASH_BUSY` | `flash write in progress` | poczekaj na koniec aktualizacji albo zapisu ustawień i wyślij jeszcze raz |
| `CONTROLLER` | `CAN controller not ready` | chwilowe: zmiana ustawienia ponawia sama. Jeśli się powtarza, zobacz [Zgubione ramki albo błędy magistrali](../troubleshooting/index.md#frames-lost-or-bus-errors) |
| `RATE` | `frame count limit` | poczekaj: najwyżej 10 ramek w dowolnych 60 s |

## Okno nadajnika {#the-transmitter-window}

Gdy żądanie przejdzie, brama otwiera nadajnik na **jedną ramkę**:

| Krok | Wartość |
|---|---|
| start sprzętowego timera, potem SLNT w stan niski | timer `CAN_TX_HW_WINDOW_US` = **25 ms**; gdy zadziała, ustawia SLNT w stan wysoki, niezależnie od tego, co robi pętla główna |
| czekanie, zanim ramka trafi do kontrolera | **2 tyknięcia** licznika milisekund (co najmniej 1 ms), żeby transceiver zdążył wyjść z trybu wyciszenia. Rzeczywistego czasu przełączenia nie ma w karcie katalogowej: nie zmierzono |
| wysłanie | tylko jedna próba („single shot”): kontroler sam nie ponawia |
| przegrany arbitraż | to nie błąd: do **2** kolejnych prób w tym samym oknie |
| limit okna w bramie | **20 ms**; dłużej = blokada |
| SLNT znowu w stanie wysokim | gdy tylko wynik jest znany (potwierdzona, nieudana, przekroczony czas) |
| przerwa przed następną ramką | **500 ms** |

Gdy okno jest otwarte, pętla główna nie robi niczego, co mogłoby zapisywać do pamięci flash: żadnych
poleceń z USB, żadnych odłożonych zapisów ustawień, żadnych zapytań z panelu. Przed uśpieniem,
restartem albo aktualizacją brama zamyka nadajnik (`forceClose`).

> [!NOTE]
> **W czasie okna urządzenie potwierdza inne ramki**
>
> W trybie normalnym, przy otwartym nadajniku, kontroler przez te kilka milisekund potwierdza
> także ramki innych węzłów.

## Blokada po błędzie {#the-fault-latch}

Blokada po błędzie sprawia, że brama odrzuca każdą ramkę, dopóki jej nie zdejmiesz. Brama
blokuje się sama (stan `fault`) po dowolnym z tych zdarzeń:

| Przyczyna | Tekst w konsoli |
|---|---|
| 3 nieudane wysłania z rzędu | `3 failed sends` |
| licznik błędów nadawania 32 lub więcej | `transmit error counter` |
| bus-off | `bus-off` |
| nadajnik otwarty dłużej niż 20 ms | `transmitter window exceeded` |
| zdarzenie kontrolera (alarm bus-off, ponowiona ramka, reset peryferium albo zadziałał sprzętowy timer) | `controller event` |
| po zmianie ustawienia zmieniły się też inne bity `260` | `foreign bits of 260 changed` |
| inna przyczyna spoza bramy | `external lock` |

Blokada trwa do `cantx reset` (tylko USB) albo restartu. Rozbrojenie jej nie zdejmuje.

## Jedna zmiana ustawienia {#one-settings-change}

Polecenie radia `80 nn vv` (albo `set NN VV` przez USB) przechodzi przez moduł zmiany ustawień:

1. Pozycja musi być w tabeli, możliwa do wysłania, włączona w masce pozycji i obecna w aucie
   (`361`). Domyślnie maska włącza **tylko pozycję `01`** (asystent parkowania). Pozycje bez
   zmierzonej ramki są odrzucane na stałe i nie da się ich włączyć.
2. `15B` jest liczona z bieżącej `260`.
3. Jeśli `260` już pokazuje żądany stan, nic nie jest wysyłane.
4. Brama dostaje ramkę. Chwilowe odmowy są ponawiane; każda inna odmowa kończy polecenie.
5. Po potwierdzeniu moduł czeka do **1300 ms** na nową `260`: przyjęte albo nieprzyjęte. Jeśli
   zmieniły się też bity poza żądanym polem, brama się blokuje.

`15B` nigdy nie jest powtarzana. Drugie polecenie, które przyjdzie w trakcie pierwszego, czeka
w jednym miejscu (wygrywa ostatnie).

Pozycje przycisków `05`, `0C` i `0D` używają zamiast tego `1A9` (tylko kompilacja
eksperymentalna). Potrzebują świeżej ramki stanu z magistrali (`227` albo `3E1`), nic nie wysyłają,
gdy auto już jest w żądanym stanie, i czekają do **2500 ms** na zmianę stanu.

## Co wypróbowano w aucie {#what-was-tried-in-the-car}

Wszystkie wyniki poniżej pochodzą z konsoli wcześniejszej wersji tego programu z nadawaniem,
uruchomionej w aucie wzorcowym (log nieopublikowany). Szczegóły: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md).

| Ramka | Wynik | Stan |
|---|---|---|
| `15B`, pozycje `01`, `02`, `06`, `07`, `09`, `0F` | każda ramka potwierdzona na magistrali; BSI pokazało nowy stan w `260` po 140-572 ms; każda pozycja w obie strony | zmierzone (measured). **Widocznego skutku w aucie nie sprawdzono** |
| `1A9` czujniki parkowania | przyjęta w obie strony | zmierzone |
| `1A9` martwe pole | wyłączenie przyjęte; włączenie nieprzyjęte (jedna próba) | zmierzone |
| `1A9` Stop and Start | nieprzyjęta (dwie próby); treści tej ramki nigdy nie nagrano z innego urządzenia | zmierzone |
| `364` klimatyzacja, 4 i 6 bajtów | potwierdzona na magistrali, **bez wpływu na klimatyzację** | zmierzone |

W tym drzewie źródeł domyślne `decoder_tx_wifi` odrzuca `1A9`, a każda wersja odrzuca `364` i treść
`1A9` dla Stop and Start (usunięte po tych próbach). Kod sprzętowy w `src/` nie
ma testu na PC; sprawdza się go według [protokołu testu na stole](../testing/bench-test-protocol.md).

## Jeszcze nieznane {#not-known-yet}

- Ile czasu transceiver potrzebuje, żeby wyjść z trybu wyciszenia (oscyloskop).
- Jak późno może zadziałać sprzętowy timer w czasie zapisu do pamięci flash (działa w zadaniu
  systemowym, nie w przerwaniu). Dlatego brama odmawia przez całą sesję aktualizacji.
- Czy zostanie dodane sprawdzanie treści `15B`.

Dalej: [Model zabezpieczeń](security.md): panel WWW i aktualizacje przez Wi-Fi.
