# Szukanie i sprawdzanie przewodów

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Płytka prototypu nie ma własnej wtyczki wiązki. Łączysz ją przewodami: cztery do zacisku
śrubowego, pięć do pól lutowniczych. Ta strona pokazuje, co gdzie idzie na płytce, jak autor
połączył ją z wiązką auta wzorcowego i jak znaleźć właściwe przewody w **twojej** wiązce.

> [!WARNING]
> **Rysunek to tylko wskazówka, dopóki brzęczyk nie zapiszczy**
>
> Pinouty wtyczek skrzynek CAN różnią się między producentami i wiązkami. Nie ufaj rysunkowi
> sprzedawcy, wpisowi na forum ani zdjęciu z opisami. Sprawdź każdy przewód próbnikiem
> ciągłości, zanim cokolwiek zasilisz.

## Połączenia na płytce {#connections-on-the-board}

**J5, zaciski śrubowe** (od góry, od strony J5 płytki):

| Zacisk | Sygnał |
|---|---|
| 1 | CANH |
| 2 | CANL |
| 3 | GND |
| 4 | +12V (stały plus akumulatora) |

**J4, pola na przewody** (od góry, od strony J5):

| Pole | Sygnał | GPIO |
|---|---|---|
| 1 „radio RX” | dane **urządzenie -> radio** (radio je odbiera) | 10 |
| 2 „radio TX” | dane **radio -> urządzenie** (radio je nadaje) | 11 |
| 3 | wyjście ACC do radia | 12 |
| 4 | wyjście ILL (podświetlenie) do radia | 13 |
| 5 | wyjście REV (wsteczny) do radia | 14 |

Nazwy pól są z punktu widzenia radia: „radio RX” to linia, na której nadaje płytka. Piny pochodzą
z `firmware/boards/board_proto_v1.h`.

> [!NOTE]
> **Zamienione przewody danych niczego nie psują, ale nic nie działa**
>
> Przy zamienionych dwóch przewodach danych radio ani nie odbiera, ani nie odpowiada (zmierzone
> (measured) w aucie wzorcowym). Zamień je i spróbuj znowu.

W środowiskach `listen`, `listen_wifi` i `listen_generic_wifi` urządzenie nic nie wysyła do radia,
a wyjścia 12 V zostają wyłączone. ACC, ILL, REV i linia „radio RX” mają znaczenie tylko w trybie
dekoder. Zobacz [Połączenie z radiem](../install-in-car/radio-connection.md).

## Przykład: auto wzorcowe {#example-the-reference-car}

W aucie wzorcowym (Peugeot 508 SW) płytka zastępuje skrzynkę CAN z zestawu radia. Autor wylutował
gniazdo 2x8 ze starej skrzynki CAN i połączył je z J5 i J4 krótkimi przewodami. Wtyczka wiązki
pasuje wtedy do gniazda jak wcześniej.

Pozycje liczone są na wtyczce wiązki (2x8, raster około 2,0 mm, oszacowany bez suwmiarki), widok
od strony przewodów, zatrzask u góry, 1-8 od lewej. Pełna mapa, z kolorami przewodów i pinami
Quadlocka, jest w `vehicles/peugeot-508-sw-2015/wiring.md` i na stronie
[Peugeot 508 SW](../reference/vehicles/peugeot-508.md).

| Pozycja we wtyczce | Sygnał | Płytka | Status |
|---|---|---|---|
| górny 1 | masa | J5-3 GND | zmierzone: ciągłość do masy skrzynki i do Quadlocka |
| dolny 1 | +12 V (zakładany stały) | J5-4 +12V | zmierzone: ciągłość do bezpiecznika skrzynki i do Quadlocka; napięcia w aucie nie zmierzono |
| górny 2 | CANH | J5-1 CANH | zmierzone: ciągłość do transceivera skrzynki, potwierdzone odbiorem bez błędów |
| dolny 2 | CANL | J5-2 CANL | zmierzone: potwierdzone odbiorem |
| dolny 8 | dane urządzenie -> radio | J4 „radio RX” | zmierzone rejestratorem tylko do nasłuchu |
| górny 7 | dane radio -> urządzenie | J4 „radio TX” | zmierzone rejestratorem: na tej linii tylko polecenia radia |
| dolny 4 | wsteczny -> radio | J4 REV | zmierzone: ciągłość w jednej z dwóch wiązek; w drugiej tylko kolor |
| dolny 3 | ACC -> radio | J4 ACC | niezweryfikowane (unverified): tylko kolor przewodu i przebieg wiązki; ciągłości nie zapisano |
| górny 3 | podświetlenie -> radio | J4 ILL | zmierzone: ciągłość w jednej z dwóch wiązek; w drugiej tylko kolor |
| górny 4 | „BRAKE” (hamulec ręczny) | niepodłączony, zaizolowany | zmierzone: luźny przewód w wiązce |
| górny 6, dolny 6 | głośnik skrzynki CAN | niepodłączony (mógłby nieść wyjście wzmacniacza) | tylko opis przewodu |

Jeśli zasilasz przewody głośnika ze wzmacniacza, pamiętaj, że jego wyjście jest mostkowe: żaden
przewód nie może dotykać masy.

Wymiarów gniazda nie zmierzono, dlatego prototyp ma pola na przewody, a nie footprint gniazda.

> [!TIP]
> **Oznacz piny gniazda, zanim zaczniesz lutować**
>
> Od strony pinów gniazdo jest lustrzanym odbiciem wtyczki. Wepnij wiązkę w wylutowane gniazdo,
> znajdź każdy pin próbnikiem ciągłości od przewodu o znanym kolorze i oznacz go, zanim
> przylutujesz jakikolwiek przewód.

## Jak znaleźć przewody w twojej wiązce {#how-to-find-the-wires-in-your-harness}

1. **Odepnij wiązkę od auta.** Ponumeruj pozycje każdej wtyczki, patrząc z jednej strony i zapisz
   widok (strona przewodów czy strona styków, gdzie jest zatrzask). Większość pomyłek w pinach
   bierze się z lustrzanego widoku.
2. **Próbnik ciągłości na stole.** Od każdego przewodu wtyczki skrzynki CAN do złącza auta i do
   wtyczek radia. Zapisz kolor, oba końce i jak to ustaliłeś.
3. **Znajdź parę CAN.** To zwykle dwa przewody, które idą tylko ze złącza auta do wtyczki skrzynki
   CAN.
4. **Znajdź zasilanie i masę.** Masa piszczy do pinu masy złącza auta. +12 V idzie do pinu
   stałego plusa akumulatora, często przez odgałęzienie do wtyczki zasilania radia.
5. **Zmierz napięcia CAN w aucie.** Zapłon włączony, miernik (DC) do masy na każdym przewodzie CAN.
   Około 2,5 V na obu liniach sugeruje CAN high-speed. Jedna linia blisko 0 V, a druga blisko 5 V
   sugeruje CAN low-speed fault-tolerant; potwierdź oscyloskopem. Więcej na stronie
   [Połączenie z CAN](../install-in-car/can-connection.md).
6. **Sprawdź, czy jest terminator.** Opór CANH-CANL skrzynki CAN bez zasilania: około 120 omów
   oznacza, że ma terminator, kiloomy, że nie ma. Magistrala auta ma już terminatory, więc
   urządzenie nie może dokładać własnego.
7. **Ustal kierunek danych.** Przy włączonym radiu zmierz napięcie spoczynkowe obu linii danych.
   W aucie wzorcowym linia skrzynki miała w spoczynku 3,3 V, a linia radia 2,9 V; mała różnica to
   wskazówka, nie dowód. Potem nagraj je rejestratorem tylko do nasłuchu (`extras/sniffer`,
   środowisko `recorder`; potrzebuje drugiej płytki ESP32-S3 i modułu CAN): linia z poleceniami
   radia to radio TX. Szczegóły:
   [Połączenie z radiem](../install-in-car/radio-connection.md#find-the-direction).
8. **Podłącz urządzenie równolegle, najpierw tylko do nasłuchu.** Zobacz
   [Znajdź swoją magistralę CAN](../make-it-yours/find-your-bus.md).

W aucie wzorcowym linie CAN miały 0,65 V (CANH) i 4,5 V (CANL) względem masy przy włączonym
zapłonie (zmierzone: miernik, średnia DC). Wygląda to na CAN low-speed fault-tolerant przy
125 kbit/s; do potwierdzenia nadal potrzebny jest oscyloskop.

## Sprawdzenia przed zasileniem płytki {#checks-before-you-power-the-board}

> [!WARNING]
> **Najpierw sprawdź bez zasilania**
>
> Po przylutowaniu przewodów, przed jakimkolwiek zasileniem przejdź
> [Listę kontrolną montażu](../install-in-car/checklist.md). Są na niej sprawdzenia
> przylutowanego gniazda: brak zwarcia między sąsiednimi pinami i między +12 V a masą, CANH-CANL
> kilkadziesiąt kOhm.

Dalej: [Lista kontrolna montażu](../install-in-car/checklist.md).
