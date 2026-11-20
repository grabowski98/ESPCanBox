# Lista elementów

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

To lista elementów **prototypu v1**, wzięta z płytki prototypu. Kopia do odczytu maszynowego to
`hardware/prototype-v1/bom.csv`. Oznaczenia (R1, Q2, ...) zgadzają się z płytką i mapami montażu.

Lista podaje typowe elementy i zamienniki, żebyś mógł użyć tego, co dostaniesz. Nie poleca
sklepów. Typy oznaczone „niesprawdzone” nie były próbowane na płytce.

> [!WARNING]
> **Niektóre elementy są obowiązkowe przed autem**
>
> D2 (ogranicznik przepięć) i C1 na 35 V lub więcej muszą być wlutowane, zanim podłączysz
> płytkę do auta. Na stole możesz pominąć D2. Wszystkie sprawdzenia przed autem:
> [Lista kontrolna montażu](../install-in-car/checklist.md).

## Moduły {#modules}

| Ozn. | Ilość | Element | Uwagi |
|---|---|---|---|
| M1 + M2 (dwa gniazda jednej płytki deweloperskiej) | 1 | płytka deweloperska ESP32-S3 z ESP32-S3-WROOM-1-N16R8, rozstaw rzędów 22,86 mm (autor użył Waveshare ESP32-S3-DEV-KIT-N16R8) | wkładana w dwie listwy żeńskie 1x22. Własny stabilizator 3,3 V płytki deweloperskiej zasila też moduł CAN |
| M3 | 1 | Adafruit CAN Pal (TJA1051T/3) | gniazdo 1x7. Kolejność pinów od dołu: 1 CANL, 2 CANH, 3 SLNT, 4 TX, 5 RX, 6 GND, 7 VCC (3,3 V). **Przełącznik terminatora na OFF** |
| M4 | 1 | moduł wzmacniacza I2S MAX98357A | gniazdo 1x7. Kolejność pinów od góry: 1 LRC, 2 BCLK, 3 DIN, 4 GAIN, 5 SD, 6 GND, 7 VIN (5 V). Głośnik 4-8 omów, 2-3 W, na zacisk modułu |
| U1 | 1 | Traco TSR 1-2450 (wejście 6,5-36 V, wyjście 5 V / 1 A) | powinien pasować każdy moduł impulsowy 5 V / 1 A z wyprowadzeniami jak 7805 (niesprawdzone) |

Zanim wytrawisz płytkę, sprawdź kolejność pinów nadrukowaną na **twoim** module. Klony mogą się
różnić.

## Zasilanie i zabezpieczenia {#power-and-protection}

| Ozn. | Ilość | Wartość | Typowy element | Funkcja | Uwagi |
|---|---|---|---|---|---|
| F1 | 1 | PTC 1,1 A / 30 V | polimerowy bezpiecznik samoresetujący, 1812 | bezpiecznik wejściowy | na 30 V lub więcej |
| F2, F3, F4 | 3 | PTC 0,5 A / 30 V | polimerowy bezpiecznik samoresetujący, 1812 | bezpieczniki wyjść 12 V | na 30 V lub więcej |
| D1, D3 | 2 | SS34 | SS34 (zamienniki SS36, B340A, SS24) | D1 odwrotna polaryzacja; D3 oddziela przetwornicę od 5 V z USB | |
| D2 | 1 | SMBJ18A | SMBJ18A (zamienniki SMAJ18A, P6SMB18A) | ogranicznik przepięć na wejściu, za F1 | **obowiązkowa przed autem**; pasek katody do +12 V |
| C1 | 1 | 100 uF / 35 V | elektrolityczny, low ESR, 105 °C, 8x10 mm | kondensator wejściowy | 35 V lub więcej w aucie; ma 10 mm wysokości, płytka potrzebuje dystansów 12 mm |
| C2 | 1 | 100 nF / 50 V | ceramiczny X7R, 0805 | odsprzęganie wejścia | 25 V lub więcej |
| C4 | 1 | 470 uF / 16 V | elektrolityczny, 8x10 mm | kondensator na 5 V (szczyty prądu wzmacniacza i Wi-Fi) | |
| C3, C5, C6, C10 | 4 | 10 uF | tantalowy 10 uF 10 V, 3528 (albo ceramiczny) | odsprzęganie 5 V / 3,3 V | |
| C7, C8 | 2 | 100 nF | ceramiczny, 0805 | odsprzęganie / filtr ADC | |

## Wyjścia 12 V (ACC, ILL, REV) {#12-v-outputs-acc-ill-rev}

| Ozn. | Ilość | Wartość | Typowy element | Funkcja | Uwagi |
|---|---|---|---|---|---|
| Q2, Q4, Q6 | 3 | AO3407 | AO3407A (-30 V, bramka +/-20 V), SOT-23 | klucze po stronie plusa (P-MOSFET) | **nie AO3401A**: jego bramka znosi tylko +/-12 V |
| Q1, Q3, Q5 | 3 | BC847A | BC847, BC817, MMBT3904, SOT-23 | sterowanie wyjść (NPN) | |
| D5, D7, D9 | 3 | Zener 12 V | 12 V 0,5 W przewlekana (BZX85C12 w DO-41 pasuje po rozwierceniu do 1,0 mm) | ochrona bramek Q2 / Q4 / Q6 | na stole opcjonalne, w aucie zalecane; katoda do +12 V |
| D6, D8, D10 | 3 | SS14 | SS14, SS24, 1N4148W | diody ograniczające na wyjściach | |
| R6, R10, R14, R18 | 4 | 10k | 0805 | rezystory bramka-źródło / podciągające | |
| R7, R9, R11, R13, R15, R17 | 6 | 4k7 | 0805 | rezystory bazy i kolektora | |
| R8, R12, R16 | 3 | 100k | 0805 | rezystory ściągające bazy: wyjścia wyłączone po resecie | |

## CAN, UART i pomiar {#can-uart-and-measurement}

| Ozn. | Ilość | Wartość | Funkcja | Uwagi |
|---|---|---|---|---|
| R3 | 1 | 10k, 0805 | podciągnięcie SLNT do 3,3 V | utrzymuje moduł CAN w trybie cichym po resecie |
| R1 | 1 | 470k, 0805 | dzielnik napięcia akumulatora, górny | 470k / 100k: 16 V daje 2,8 V |
| R2 | 1 | 100k, 0805 | dzielnik napięcia akumulatora, dolny | |
| R23 | 1 | 1k, 0805 | linia UART do radia (urządzenie nadaje), wg projektu | na prototypie nie sprawdzono, który rezystor jest na której linii |
| R24 | 1 | 4k7, 0805 | linia UART z radia (urządzenie odbiera) | bezpieczny także przy poziomie 5 V |
| JP1 | 1 | listwa 2-pin albo mostek z drutu | linia TX do modułu CAN | **zostaw otwartą** w wersji tylko do nasłuchu ([Płytka prototypu](prototype-pcb.md#transmit-path)) |

## Złącza i mechanika {#connectors-and-mechanics}

| Ozn. | Ilość | Element | Uwagi |
|---|---|---|---|
| J5 | 1 | zacisk śrubowy 4-torowy 5,08 mm (dwa bloki 2-torowe obok siebie) | od góry: CANH, CANL, GND, +12V |
| J4 | - | 5 pól na przewody, otwory 1,2 mm, bez elementu | od góry: radio RX, radio TX, ACC, ILL, REV |
| gniazda | 2 + 2 | listwy żeńskie 2,54 mm: 2x 1x22 (ESP32), 2x 1x7 (moduły) | można uciąć z listew 1x40 |
| H1, H2, H3 | 3 | otwory M3 | śruby M3 x 6-8 mm |
| drut | 7 kawałków | np. obcięte nóżki elementów | 7 przelotek z drutu |

Poza listą: gniazdo wiązki 2x8. Nie ma go na płytce; autor połączył gniazdo wylutowane ze starej
skrzynki CAN z J5 i J4 krótkimi przewodami ([Szukanie i sprawdzanie przewodów](wiring.md)).

## Czym różni się płytka autora {#where-the-authors-board-differs}

Płytka w aucie nie odpowiada dokładnie tej liście:

- kilka elementów pochodzi z rozebranej skrzynki CAN z zestawu radia: tranzystory P-MOSFET, diody
  wyjściowe i bezpiecznik wejściowy;
- D2 i diody Zenera D5, D7, D9 na początku nie były wlutowane.

Przed autem wlutuj D2 i użyj C1 na 35 V lub więcej, niezależnie od tego, skąd bierzesz elementy.

Dalej: [Schemat](schematic.md).
