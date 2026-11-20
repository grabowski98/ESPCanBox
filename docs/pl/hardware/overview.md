# Przegląd sprzętu

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Urządzenie to płytka z ESP32-S3, która stoi między autem a radiem na Androidzie, w miejscu
skrzynki CAN z zestawu radia. Ta część opisuje jedyny sprzęt, jaki na razie istnieje:
**prototyp v1**, wytrawioną w domu płytkę bazową na gotowe moduły. Płytka do zamówienia w fabryce
PCB jest planowana, ale jeszcze nie zaprojektowana (zobacz [Przyszłe płytki](future-boards.md)).

> [!WARNING]
> **Dowód koncepcji, nie produkt**
>
> Powstała jedna płytka prototypu: wytrawiona, zmontowana i użyta w aucie wzorcowym (Peugeot 508 SW).
> Publikujemy ją, żebyś zobaczył działający układ i mógł go skopiować na własne ryzyko. Nie ma
> plików Gerber ani plików produkcyjnych. Jeśli ją budujesz, sprawdzaj każdy krok miernikiem.

## Co jest na płytce {#what-is-on-the-board}

Prototyp to płytka bazowa: trudne części to moduły wkładane w gniazda.

| Blok | Element na prototypie | Do czego służy |
|---|---|---|
| Procesor | płytka deweloperska ESP32-S3 z ESP32-S3-WROOM-1-N16R8 (autor użył Waveshare ESP32-S3-DEV-KIT-N16R8), w dwóch gniazdach 1x22 | uruchamia program, USB, Wi-Fi |
| CAN | moduł Adafruit CAN Pal (transceiver TJA1051T/3), gniazdo 1x7 | zamienia parę przewodów CAN na sygnały logiczne dla ESP32 |
| Dźwięk | moduł wzmacniacza I2S MAX98357A, gniazdo 1x7 | piknięcia czujników parkowania z małego głośnika |
| Zasilanie | bezpiecznik polimerowy, dioda przed odwrotną polaryzacją, ogranicznik przepięć (TVS), moduł DC/DC Traco TSR 1-2450 (12 V na 5 V) | zasila płytkę ze stałego 12 V auta |
| Napięcie akumulatora | dzielnik 470k / 100k na nóżkę ADC | pokazuje napięcie zasilania w konsoli i w panelu |
| UART do radia | dwa rezystory szeregowe | łącze szeregowe z radiem (tryb dekoder) |
| Wyjścia 12 V | trzy klucze P-MOSFET po stronie plusa: ACC, podświetlenie (ILL), wsteczny (REV) | załączają wejścia ACC, ILL i REVERSE radia (tryb dekoder) |

3,3 V dla modułu CAN daje stabilizator na płytce deweloperskiej ESP32. Na płytce prototypu nie ma
osobnego stabilizatora 3,3 V.

Płytka ma 100 x 77 mm, dwie warstwy miedzi, ścieżki 0,8 mm i odstępy 0,4 mm, pod metodę
termotransferu. Szczegóły: [Płytka prototypu](prototype-pcb.md).

## Co jest tylko na schemacie {#what-is-only-in-the-schematic}

Schemat opisuje **pełniejszą** płytkę niż ta, którą wytrawiono. Te elementy są narysowane, ale nie
ma ich na płytce prototypu:

- złącze rozszerzeń i złącze I2C;
- przycisk awaryjny;
- czwarte wyjście 12 V;
- zapasowy stabilizator 3,3 V;
- zworki wyboru `JP_S` (źródło SLNT), `JP_UART` (zamiana TX/RX radia), `JP_3V3` (źródło 3,3 V).

Zobacz [Schemat](schematic.md).

## Sprzętowo tylko nasłuch {#listen-only-in-hardware}

Zbuduj płytkę tak, jak jest narysowana: JP1 pusta, bez przewodu do GPIO6. Wtedy moduł CAN nie może
nadawać: jego wejście SLNT (tryb cichy) jest podciągnięte do plusa rezystorem 10 kOhm (R3), a linia
nadawania idzie przez otwartą zworkę JP1. Płytkę autora przerobiono później do prób nadawania (JP1
zwarta, przewód z SLNT do GPIO6); ty tej przeróbki nie potrzebujesz. Na takiej płytce tylko program
decyduje, czy urządzenie nadaje.

> [!CAUTION]
> **Przerobiona płytka nie ma sprzętowej blokady nadawania**
>
> Przy zwartej JP1 i przewodzie SLNT do GPIO6 nic w sprzęcie nie zatrzymuje nadawania. Zostaw
> JP1 otwartą i nie dokładaj przewodu SLNT, chyba że celowo budujesz program z nadawaniem.
> Najpierw przeczytaj [Nadawanie na magistralę CAN](../safety/transmit.md).

Program domyślnie tylko słucha: w każdym środowisku poza `decoder_tx_wifi` kontroler TWAI pracuje
w trybie tylko do nasłuchu, a GPIO6 zostaje w stanie wysokim. Więcej na stronie
[Płytka prototypu](prototype-pcb.md#transmit-path).

## Piny {#pins}

Numery pinów prototypu są w jednym pliku, `firmware/boards/board_proto_v1.h`. Ten plik jest
jedynym źródłem: jeśli ta strona i plik się różnią, rację ma plik.

| Sygnał | GPIO | Na płytce prototypu |
|---|---|---|
| CAN RX (z modułu) | 4 | tak |
| CAN TX (do modułu) | 5 | przez zworkę JP1 |
| CAN SLNT (tryb cichy) | 6 | **nie**: tylko na przerobionej płytce autora, dołożonym przewodem |
| Napięcie akumulatora (ADC) | 7 | tak, dzielnik 470k / 100k |
| I2C SDA / SCL | 8 / 9 | nie (tylko schemat) |
| UART do radia, urządzenie nadaje | 10 | tak, pole J4 „radio RX” |
| UART z radia, urządzenie odbiera | 11 | tak, pole J4 „radio TX” |
| Wyjście 12 V ACC | 12 | tak |
| Wyjście 12 V ILL | 13 | tak |
| Wyjście 12 V REV | 14 | tak |
| I2S BCLK / LRC / DIN | 15 / 16 / 17 | tak |
| SD wzmacniacza (stan niski = wyłączony) | 18 | tak |
| Zapasowe wyjście 12 V | 21 | nie (tylko schemat) |
| Przycisk awaryjny (do masy) | 3 | nie (tylko schemat) |
| Dioda stanu (RGB na płytce deweloperskiej) | 38 | na płytce deweloperskiej |

GPIO 35-37 (pamięć modułu N16R8) i 19/20 (USB) nie są nigdy używane. Kompilacja to sprawdza.
Jak opisać własną płytkę: [Konfiguracja i piny](../reference/config-and-pins.md).

## Strony w tej części {#pages-in-this-section}

- [Lista elementów](bom.md): elementy prototypu.
- [Schemat](schematic.md): arkusze, format plików, jak go otworzyć.
- [Płytka prototypu](prototype-pcb.md): wykonanie płytki, pierwsze włączenie, co jest zmostkowane albo puste.
- [Obudowa](enclosure.md): obudowa z drukarki 3D.
- [Szukanie i sprawdzanie przewodów](wiring.md): jak płytka łączy się z wiązką i jak zmierzyć własną.
- [Przyszłe płytki](future-boards.md): co jest planowane.

## Licencja {#licence}

Pliki sprzętu (KiCad, PDF, STL, `bom.csv`) są na licencji CERN-OHL-S-2.0 (`LICENSE-hardware`
w repozytorium).

Dalej: [Lista elementów](bom.md).
