# Schemat

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona mówi, gdzie jest schemat prototypu v1, jak go otworzyć i które szczegóły projektu
warto znać. Pliki są w `hardware/prototype-v1/`:

| Plik | Zawartość |
|---|---|
| `pdf/schematic.pdf` | wszystkie arkusze w PDF: otwórz go, jeśli nie używasz KiCada |
| `kicad/centralka.kicad_pro` | projekt KiCada |
| `kicad/*.kicad_sch` | arkusze schematu |
| `kicad/centralka.kicad_sym` | biblioteka symboli projektu |

Nazwy plików i biblioteki KiCada (`centralka*`) to robocza nazwa prototypu, sprzed nadania nazwy
projektowi. Zostały, żeby pliki projektu zachowały swoje powiązania.

Sprawdzenie reguł elektrycznych (ERC) KiCada zgłasza 0 naruszeń.

> [!NOTE]
> **Tylko KiCad 10**
>
> Pliki KiCada zapisała wersja rozwojowa KiCada 10 (wersja formatu pliku 20250610, widoczna jako
> „9.99”). Otwierają się w KiCadzie 10. **Starsze wersje KiCada ich nie otworzą.** Wszyscy inni
> mogą użyć PDF.

Schemat i płytkę wygenerowały skrypty, których nie ma w repozytorium. Edytuj bezpośrednio pliki
KiCada.

## Schemat jest pełniejszy niż płytka {#the-schematic-is-fuller-than-the-board}

Schemat opisuje **pełną** płytkę bazową. Płytka prototypu realizuje jej okrojoną część.
Narysowane na schemacie, ale nie ma ich na płytce prototypu:

- złącze rozszerzeń (`EXP1`-`EXP6`) i złącze I2C;
- przycisk awaryjny na GPIO3;
- czwarte wyjście 12 V (GPIO21);
- zapasowy stabilizator 3,3 V (AMS1117-3.3, oznaczony „nie montować”);
- zworki wyboru `JP_S`, `JP_UART`, `JP_3V3`;
- główne złącze 2x8 i 4-torowe złącze pomocnicze (na prototypie zastępują je zacisk śrubowy J5
  i pola na przewody J4).

Co naprawdę ma wytrawiona płytka: [Płytka prototypu](prototype-pcb.md).

## Arkusze {#sheets}

Pliki arkuszy mają oryginalne polskie nazwy. To identyfikatory wspólne dla schematu i płytki,
więc nie zmieniano ich nazw. Notatki i tytuły na arkuszach są po angielsku.

| Plik arkusza | Znaczenie nazwy | Zawartość |
|---|---|---|
| `centralka.kicad_sch` | (projekt) | arkusz główny z arkuszami bloków |
| `zasilanie.kicad_sch` | zasilanie | wejście 12 V, bezpiecznik F1, dioda przed odwrotną polaryzacją D1, TVS D2, moduł DC/DC U1, dioda D3 od 5 V z USB, dzielnik napięcia akumulatora, zworka źródła 3,3 V, punkty pomiarowe |
| `can.kicad_sch` | CAN | gniazdo CAN Pal, dwie blokady nadawania, rezystory 0 omów pod koraliki ferrytowe, opcjonalna ochrona linii D4; alternatywny footprint pod moduł SN65HVD230 |
| `procesor.kicad_sch` | procesor | dwa gniazda 1x22 płytki deweloperskiej ESP32-S3, przycisk awaryjny |
| `dzwiek.kicad_sch` | dźwięk | gniazdo MAX98357A, rezystor ściągający SD, zworka wzmocnienia |
| `uart.kicad_sch` | UART | łącze szeregowe z radiem, zworki zamiany |
| `wyjscia.kicad_sch` | wyjścia | wyjścia 12 V ACC, ILL, REV i jedno zapasowe |
| `zlacza.kicad_sch` | złącza | główne złącze 2x8, złącze pomocnicze, I2C, złącze rozszerzeń, otwory montażowe |

Inne polskie słowa, które spotkasz: `masa` = masa, `przewody z wiazki` = przewody z wiązki,
`sruba M3` = śruba M3, `PUNKTY POMIAROWE` = punkty pomiarowe, `ROZSZERZENIA` = rozszerzenia.

## Warto wiedzieć o projekcie {#design-notes-worth-knowing}

### CAN i dwie blokady nadawania {#can-and-the-two-transmit-locks}

Schemat ma dwie sprzętowe blokady nadawania:

- `JP_TX` (2-pin) w linii TX z GPIO5 do modułu. Otwarta = wejście TX modułu wisi, a transceiver
  zostaje w stanie recesywnym (nie steruje magistralą).
- `JP_S` (3-pin) na wejściu SLNT. Pozycja 1-2 łączy SLNT z 3,3 V: nadajnik wyłączony, odbiornik
  działa. Pozycja 2-3 oddaje SLNT do GPIO6, więc tryb ustawia program (stan wysoki = tylko
  nasłuch).

Rezystor podciągający 10 kOhm (R3) trzyma SLNT w stanie wysokim po resecie. To ważne, bo TJA1051
ma na tej nóżce wewnętrzny rezystor ściągający: niepodłączone SLNT oznacza tryb normalny
z włączonym nadajnikiem (karta katalogowa NXP TJA1051, rozdział 7.2.2).

Na płytce **nie ma terminatora 120 omów**, a przełącznik na CAN Pal musi być na OFF: magistrala
auta jest już zakończona terminatorami.

Alternatywny moduł SN65HVD230 nie ma wejścia trybu cichego. Z nim jedynymi blokadami są `JP_TX`
i tryb tylko do nasłuchu kontrolera.

### Zasilanie {#power}

- F1 (około 1,1 A, 30 V lub więcej), potem D1, potem D2. D2 stoi za bezpiecznikiem, więc długie
  przepięcie wyzwala F1.
- D3 oddziela moduł DC/DC od szyny 5 V, gdy płytka pracuje z USB płytki deweloperskiej.
- Dzielnik 470k / 100k na GPIO7: 16 V daje 2,8 V na ADC, prąd około 21 uA.

### Wyjścia 12 V {#12-v-outputs}

Każde wyjście to klucz P-MOSFET po stronie plusa, sterowany tranzystorem NPN. Po resecie rezystor
100 kOhm trzyma NPN wyłączony, więc wyjście jest wyłączone. Dioda Zenera 12 V chroni bramkę
MOSFET-a. Klucz działa od około 5 V, więc ACC ma się utrzymać podczas kręcenia rozrusznikiem.
W aucie wzorcowym BSI (moduł nadwozia) zgłosiło 7,16 V przy rozruchu (zmierzone (measured):
odczyt BSI w DiagBoxie). Czy wyjścia naprawdę utrzymują się przy rozruchu: niezmierzone.

### UART {#uart}

Według projektu: 1 kOhm w linii nadawania, 4,7 kOhm w linii odbioru (bezpieczne także przy
poziomie 5 V z radia). Na prototypie obie wartości zmierzono, ale nie sprawdzono jeszcze
miernikiem, która jest na której linii. Zworki lutowane `JP_UART` zamieniają nadawanie z odbiorem,
dla wiązki połączonej odwrotnie.

### Dźwięk {#sound}

Wejście SD wzmacniacza jest ściągnięte do masy przez 10 kOhm: cisza do startu programu. Zworka
wzmocnienia `JP_GAIN`: otwarta = 9 dB, do masy = 12 dB, do 5 V = 6 dB (niezweryfikowane
(unverified): z opisu modułu Adafruit; sprawdź na klonie).

## Piny {#pins}

Schemat i `firmware/boards/board_proto_v1.h` używają tych samych pinów. Wiążącą listą jest plik
nagłówkowy: [Przegląd sprzętu](overview.md#pins).

Dalej: [Płytka prototypu](prototype-pcb.md).
