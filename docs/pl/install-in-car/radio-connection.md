# Połączenie z radiem

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona pokazuje, jak znaleźć i podłączyć przewody między urządzeniem a radiem. Między skrzynką
CAN z zestawu radia a radiem na Androidzie są zwykle dwa rodzaje przewodów:

- **dwa przewody danych** (szeregowa linia UART, po jednym na kierunek) oraz
- **załączane wyjścia 12 V** skrzynki, które przekazują radiu stan auta: ACC (włącz się), ILL
  (podświetlenie, światła włączone) i REV (bieg wsteczny).

W trybie dekoder urządzenie zajmuje miejsce skrzynki i steruje nimi wszystkimi. W trybie „obok
skrzynki” tylko słucha.

Tryb dekoder działa tylko z radiem ustawionym na protokół skrzynki CAN Simple Soft (XP); zmierzono
go na jednym radiu. Przy każdym innym protokole radia używaj trybu „obok skrzynki”. Wersja
`listen_generic_wifi` nic nie wysyła do radia. Opcje kompilacji:
[Przyciski i radio](../make-it-yours/radio-mapping.md). Projekt nie jest związany z Simple Soft ani
z żadnym producentem radia.

## Linia danych (UART) {#the-data-line-uart}

| Właściwość | Wartość | Status |
|---|---|---|
| format | 38400 bodów, 8 bitów danych, bez parzystości, 1 bit stopu (8N1) | zmierzone (measured): rejestrator na obu liniach, `firmware/test_data/captures/can_20261111_*.log` |
| poziom logiczny | 3,3 V | zmierzone: napięcie spoczynkowe przy włączonym radiu, linia skrzynki CAN 3,3 V, linia radia 2,9 V |
| masa | mała wtyczka danych radia ma tylko dwa przewody danych (zmierzone: widać we wtyczce); wspólna masa przychodzi przez wtyczkę zasilania radia | niezweryfikowane (unverified): opis wiązki od autora, bez pomiaru ciągłości |

W tej dokumentacji oba przewody są nazwane z punktu widzenia radia:

- **radio TX**: radio nadaje, skrzynka (albo urządzenie) odbiera;
- **radio RX**: skrzynka (albo urządzenie) nadaje, radio odbiera.

Na prototypie v1 pola na przewody J4 to, od strony J5: **radio RX, radio TX, ACC, ILL, REV**.
„Radio RX” to pole, na którym nadaje urządzenie. Płytka ma rezystory szeregowe w obu liniach
danych.

### Ustal kierunek {#find-the-direction}

1. Rozpisz oba przewody danych próbnikiem ciągłości, od wtyczki skrzynki CAN do wtyczki danych
   radia.
2. Radio włączone, miernik do masy: zapisz napięcie spoczynkowe każdego przewodu. W aucie
   wzorcowym linia skrzynki miała w spoczynku 3,3 V, a linia radia 2,9 V; mała różnica to
   wskazówka, nie dowód.
3. Ustal, który przewód niesie polecenia radia. Ten przewód to radio TX.
    - **Sposób pewny:** nagranie obu linii tylko do nasłuchu rejestratorem (`extras/sniffer`,
      środowisko `recorder`). Potrzebuje drugiej płytki ESP32-S3 i modułu CAN; zobacz
      `extras/sniffer/README.md`.
    - **Urządzeniem** (wersja `listen` albo `listen_wifi`): podłącz pole J4-2 „radio TX” do jednego
      z dwóch przewodów. To pole jest tylko wejściem, więc każdy z przewodów jest bezpieczny. Radio
      włączone, wpisz `radio`. `[radio] the radio has not spoken yet` znaczy, że na tym przewodzie
      nie przyszła żadna poprawna ramka. Samo urządzenie nie odróżni obu linii na pewno: poprawna
      ramka od skrzynki też zwiększa `received frames=`.

> [!WARNING]
> **Zamienione przewody danych: brak komunikacji**
>
> Jeśli dwa przewody danych są zamienione, radio ani nie odbiera, ani nie odpowiada (zmierzone
> w aucie wzorcowym). Zanim podłączysz pin nadawania urządzenia, upewnij się, że idzie do linii
> radio RX, nigdy do linii radio TX: dwa wyjścia na jednym przewodzie walczą ze sobą.

### Przykład: Peugeot 508 SW (auto wzorcowe) {#example-peugeot-508-sw-reference-car}

Wtyczka skrzynki CAN 2x8, widok od strony przewodów, zatrzask u góry, pozycje 1-8 od lewej.

| Przewód | Sygnał | Status |
|---|---|---|
| górny 7 (szaro-biały) | radio TX, do wtyczki danych radia, dolny rząd poz. 4 | zmierzone: rejestrator, na tej linii tylko polecenia radia |
| dolny 8 (biały) | radio RX, do wtyczki danych radia, górny rząd poz. 4 | zmierzone: rejestrator i próbnik ciągłości |

Więcej o tej wiązce: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md). Co znaczą ramki:
[Protokół radia XP](../reference/xp-protocol.md).

## Wyjścia 12 V: ACC, ILL, REV {#the-12-v-outputs-acc-ill-rev}

Na prototypie v1 każde wyjście to klucz P-MOSFET po stronie plusa, sterowany tranzystorem z pinu
GPIO. Stan wysoki na pinie daje 12 V na wyjściu. Przy starcie program ustawia wszystkie wyjścia
w stan niski, **zanim** przełączy piny na wyjścia, więc nie ma impulsu. Po błędzie sterownika przy
starcie wyjścia zostają wyłączone.

| Wyjście | Kiedy jest włączone (tryb dekoder) | Źródło w profilu auta | Status w aucie wzorcowym |
|---|---|---|---|
| ACC | zapłon włączony; wyłącza się 2,5 s po wyłączeniu zapłonu albo około 3,5 s po zaniku ramki zapłonu (1 s na uznanie go za wyłączony, potem 2,5 s) | ramka zapłonu (`0F6` bajt 1 w PSA AEE2010: `80` wyłączony, `88` włączony, `8E` silnik pracuje) | zmierzone: moment przełączenia zgadza się ze skrzynką CAN z dokładnością do 40 ms na nagraniach (testy na komputerze) |
| ILL | światła włączone, i tylko przy włączonym zapłonie | ramka świateł (`036` 4.5 w PSA AEE2010) | zmierzone: bit 4.5 idzie za światłami na nagraniach; co skrzynka CAN robiła na swoim przewodzie ILL, **nie zmierzono** |
| REV | włączony bieg wsteczny | ramka zapłonu/nadwozia (`0F6` w PSA AEE2010) | zmierzone: ramka `0F6` idzie za biegiem wstecznym na nagraniach (`can_20261111_191045.log`) |
| zapasowe (OUT 4) | program nigdy go nie załącza | brak | w wiązce wzorcowej ta pozycja to nieużywany przewód „BRAKE” |

Piny wyjść ustawia plik nagłówkowy płytki (`boards/board_proto_v1.h`); zobacz
[Konfiguracja i piny](../reference/config-and-pins.md).

> [!WARNING]
> **Niesprawdzone na stole jako komplet**
>
> Na prototypie v1 nie ma osobnego raportu ze stołu dla wyjść ACC, ILL i REV
> (niezweryfikowane). Przed autem: zasil płytkę z zasilacza laboratoryjnego, daj małe obciążenie
> (np. żarówkę 12 V albo rezystor) na każde wyjście i sprawdź miernikiem, że po starcie wszystkie
> wyjścia mają 0 V, w obu trybach.

### Przykład: Peugeot 508 SW (auto wzorcowe) {#example-peugeot-508-sw-reference-car_1}

| Wtyczka skrzynki CAN | Sygnał | Dokąd idzie | Status |
|---|---|---|---|
| dolny 3 (czerwony) | wyjście ACC | wtyczka zasilania radia, pin 15 (ACC IN) | niezweryfikowane: kolor przewodu i przebieg wiązki; ciągłości nie zapisano |
| górny 3 (pomarańczowy) | wyjście ILL | wtyczka zasilania radia, pin 07 (ILL) | zmierzone: ciągłość w jednej wiązce, w drugiej tylko kolor |
| dolny 4 (brązowy) | wyjście REV | wtyczka zasilania radia, pin 14 (REVERSE) | zmierzone: ciągłość w jednej wiązce, w drugiej tylko kolor |

Te trzy przewody idą z wtyczki skrzynki CAN do wtyczki zasilania radia, nie do złącza auta:
załącza je skrzynka. Numery pinów wtyczki radia pochodzą z rysunku sprzedawcy radia, dopasowane po
kolorze (niezweryfikowane poza wierszami ILL i REVERSE powyżej).

Zanim użyjesz trybu dekoder, sprawdź brzęczykiem przewód ACC od wtyczki skrzynki CAN do pinu 15
wtyczki zasilania radia. Nie polegaj na kolorze.

## Które przewody podłączyć w każdym trybie {#which-wires-to-connect-in-each-mode}

> [!CAUTION]
> **Nigdy dwa źródła na jednym przewodzie**
>
> W trybie dekoder skrzynka CAN musi być **wypięta**. Gdy podłączone są i skrzynka, i urządzenie,
> dwa nadajniki sterują jednym przewodem UART, a dwa wyjścia ACC jednym przewodem: wyjścia
> zwierają się nawzajem.

| Przewód | Obok skrzynki | Dekoder |
|---|---|---|
| masa, +12 V, CANH, CANL | podłącz | podłącz |
| radio TX (radio -> skrzynka) | podłącz do RX urządzenia (nasłuch) | podłącz do RX urządzenia |
| radio RX (skrzynka -> radio) | **zostaw niepodłączony** (pin nadawania urządzenia nie jest używany w tym trybie) | podłącz do TX urządzenia |
| ACC, ILL, REV | **zostaw niepodłączone** (wyjścia są w tym trybie zawsze wyłączone) | podłącz, skrzynka CAN wypięta |

W trybie „obok skrzynki” wyjścia zostają wyłączone, a pin nadawania UART nie jest przypisany, więc
na tych przewodach i tak nic by się nie działo. Zostawienie ich niepodłączonych sprawia, że zła
wersja programu nie wywoła konfliktu ze skrzynką.

## Sprawdzenie na radiu {#check-on-the-radio}

1. Tryb dekoder, skrzynka CAN wypięta, zapłon wyłączony.
2. Zapłon włączony: radio startuje. Linia `stat` w konsoli pokazuje `acc=1`.
3. `radio` w konsoli: `received frames=` rośnie. Niektóre radia wysyłają wadliwą ramkę `C5`; każda liczy się jako jedna zła suma i trzy bajty śmieci, więc powoli rosnące `bad checksum=` przy `garbage=` dokładnie trzy razy większym jest normalne. Inna proporcja oznacza błąd w przewodach albo w prędkości.
4. Światła włączone: radio przyciemnia ekran (jeśli jest ustawione na ILL). `stat` pokazuje
   `lights=1`.
5. Bieg wsteczny: radio pokazuje kamerę. `stat` pokazuje `reverse=1`.
6. Zapłon wyłączony: radio wyłącza się około 2,5 s później (plus własne opóźnienie wyłączenia, jeśli
   jest ustawione w radiu).

Jeśli któryś krok nie wychodzi, wyłącz zapłon i wepnij z powrotem skrzynkę CAN; potem zobacz
[Rozwiązywanie problemów](../troubleshooting/index.md). Polecenia konsoli opisuje strona
[Konsola USB](../reference/console.md).

Dalej: [Pierwsza jazda](first-drive.md).
