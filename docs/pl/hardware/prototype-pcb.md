# Płytka prototypu

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Prototyp v1 to dwustronna płytka do wykonania w domu metodą termotransferu. Zbudowano jedną
płytkę i pracuje ona w aucie wzorcowym. Ta strona opisuje, jak ją wykonać, jak włączyć ją pierwszy
raz i co jest inne na płytce, która istnieje.

Wszystkie pliki są w `hardware/prototype-v1/`. Przeczytaj tam też `hardware/prototype-v1/STATUS.md`.

## Pliki {#files}

| Plik | Zawartość |
|---|---|
| `kicad/proto/centralka_proto.kicad_pcb` | płytka ze ścieżkami i wylewką masy (KiCad 10) |
| `print/1_bottom_copper_toner_transfer.pdf` | miedź od spodu, **bez lustrzanego odbicia** |
| `print/2_top_copper_mirrored_toner_transfer.pdf` | miedź od góry, **w lustrzanym odbiciu** |
| `print/3_assembly_top_view.pdf`, `print/4_assembly_bottom_view.pdf` | rozmieszczenie elementów w skali 2:1, tylko do oglądania |
| `images/assembly_map_bottom_view.png` | co gdzie przylutować, polaryzacja, widok od strony lutowania |
| `images/drill_map_bottom_view.png` | otwory w kolorach według średnicy wiertła, widok od strony lutowania |
| `images/esp32_socket_test_map_bottom_view.png` | sprawdzanie miernikiem pustych gniazd ESP32 |
| `images/ground_wire_vias.png` | gdzie idą przelotki masy z drutu |
| `images/preview_*.png`, `images/render_*.png` | podglądy i rendery 3D |

Opisy na obrazkach są po polsku. `hardware/prototype-v1/README.md` je tłumaczy.

Nie ma plików Gerber ani plików wierceń, nie ma też modelu STEP. Płytka nigdy nie była robiona
w fabryce PCB.

## Płytka w liczbach {#the-board-in-numbers}

- 100 x 77 mm, dwie warstwy.
- Ścieżki 0,8 mm, odstęp 0,4 mm (najmniejsza przerwa występuje w około 17 miejscach).
- 7 przelotek z drutu: 5 łączy masę od spodu z masą od góry, 2 to końce jednej zworki 5 V od
  góry.
- Sprawdzenie reguł projektowych KiCada (DRC): 0 błędów miedzi, 0 niepołączonych elementów.
  Zostają tylko ostrzeżenia o nachodzeniu opisu i obrysów elementów.

## Wykonanie płytki {#making-the-board}

1. Wydrukuj arkusze 1 i 2 w skali 100 %. Wyłącz „dopasuj do strony”. Ramka na arkuszu pozwala
   sprawdzić skalę linijką.
2. Sprawdź odbicie: spód **nie** jest odbity, góra **jest** odbita, bo toner zawsze leży na
   miedzi. Napisy „SPOD v1” (spód) i „GORA v1” (góra) mają być odbite na papierze i czytelne na
   miedzi.
3. Najpierw wywierć trzy otwory M3 (u góry z lewej przy J5, prawy górny róg, prawy dolny róg)
   i na nich wyrównaj obie strony. Masa od góry trzyma 1 mm od każdego otworu, więc przesunięcie do
   około 1 mm nie zwiera pinu gniazda.
4. Przenieś toner (żelazkiem albo laminatorem) i wytraw płytkę tak, jak zwykle to robisz; ta
   strona nie opisuje metody.
5. Po wytrawieniu obejrzyj płytkę pod lupą.

### Wiercenie {#drilling}

Środki otworów to małe kropki na miedzi.

| Wiertło | Liczba | Do czego |
|---|---|---|
| 0,8 mm | 13 | 7 przelotek z drutu, 3 diody Zenera |
| 1,0 mm | 63 | gniazda, moduł TRACO, JP1 |
| 1,2 mm | 5 | pola na przewody J4 |
| 1,3 mm | 4 | zaciski J5 |
| 3,2 mm | 3 | otwory M3 |

Bez wiertła 0,8 mm użyj 1,0 mm (zostaje pierścień miedzi około 0,65 mm).

> [!WARNING]
> **Nie wierć gniazd większym wiertłem niż 1,0 mm**
>
> Wzdłuż rzędu gniazda pierścień miedzi ma tylko 0,52 mm. Większe wiertło go usunie.

### Lutowanie {#soldering}

1. Lutuj w tej kolejności:
    1. Elementy SMD od spodu.
    2. 7 przelotek z drutu: drut przez otwór, lut z obu stron. Dwie z nich są 1,6 mm od listwy
       ESP32: zrób je teraz, przed gniazdami.
    3. Moduł TRACO.
    4. Zaciski i diody Zenera.
    5. Na końcu gniazda.

    JP1 zostaje pusta.
2. Pola elementów przewlekanych lutuj **tylko od spodu**. Pierścienie wokół otworów od góry to
   martwa miedź.
3. Nie pomijaj 5 przelotek masy z drutu. To jedyne połączenie masy od góry z masą od spodu. Bez
   nich części masy od spodu wiszą w powietrzu (np. masa przetwornicy, modułu CAN i tranzystorów
   wyjściowych).
4. Piny masy gniazd modułów łączą się z wylewką pełną miedzią (bez odciążenia termicznego):
   przytrzymaj grot trochę dłużej. Pola gniazd są 0,5 mm od siebie: sprawdź, czy sąsiednie piny
   nie są zwarte.
5. C1 i C4 mają 10 mm wysokości i są od spodu: płytka potrzebuje dystansów co najmniej 12 mm.
   [Obudowa](enclosure.md) je ma.

## Pierwsze włączenie {#first-power-up}

> [!WARNING]
> **Użyj zasilacza z ograniczeniem prądu**
>
> Zasil gołą płytkę z zasilacza laboratoryjnego z ograniczeniem do około 100 mA. Jeśli dojdzie
> do ograniczenia, od razu wyłącz. Wyłącz zasilacz za każdym razem, zanim włożysz moduł.

Jak znaleźć piny: na `images/esp32_socket_test_map_bottom_view.png` (widok od strony lutowania)
M1 to wewnętrzne gniazdo 1x22, obok gniazda modułu CAN M3, a M2 to zewnętrzne, przy krawędzi
płytki. Pin 1 obu gniazd jest od strony J5, pin 22 na drugim końcu.

1. Jeszcze bez modułów. Sprawdź miernikiem: brak zwarcia +12V-GND, +5V-GND, +3V3-GND (pola przy
   J5, C4 i C6).
2. Sprawdź puste gniazda ESP32 próbnikiem ciągłości, bez zasilania, jak na
   `images/esp32_socket_test_map_bottom_view.png`: sąsiednie piny nie piszczą (poza M1 piny 1-2
   i M2 piny 21-22); piny masy piszczą do GND na J5. Piny 7 i 11 M1 mają 100k / 10k do masy, więc
   tam cisza jest prawidłowa (omomierz pokazuje około 85k / 10k).
3. Podłącz 12 V do J5 (zacisk 4 = +12V, zacisk 3 = GND). Minus miernika na GND J5. Spodziewaj się
   4,6-4,8 V na pinie 21 M1 (5 V za diodą D3), 0 V na pinach 1 i 2 M1 (3,3 V przyjdzie później
   z płytki ESP32), około 1,9-2,0 V na pinie 7 M1 (dzielnik napięcia akumulatora) i prądu kilku mA.
4. Wyłącz zasilacz. Włóż płytkę deweloperską ESP32 (USB po stronie pinu 22). Włącz zasilacz
   z tym samym ograniczeniem 100 mA. Teraz piny 1 i 2 M1 oraz pin VCC gniazda CAN pokazują 3,3 V.
   Prąd: kilkadziesiąt mA. Jeśli zasilacz dojdzie do ograniczenia, od razu wyłącz.
5. Wyłącz zasilacz. Włóż moduł CAN (przełącznik terminatora na OFF) i wzmacniacz. Potem wgraj
   program ([Wgraj program](../quick-start/flash.md)) i zrób
   [test na stole](../quick-start/bench-test.md).

## Co jest na prototypie, a czego nie ma {#what-is-on-the-prototype-and-what-is-not}

Na płytce prototypu: wejście 12 V z bezpiecznikiem, diodą przed odwrotną polaryzacją
i przetwornicą DC/DC; płytka deweloperska ESP32-S3; moduł CAN Pal; wzmacniacz MAX98357A; pomiar
napięcia akumulatora; UART do radia; wyjścia 12 V ACC, ILL i REV.

Nie ma na płytce prototypu (tylko na [schemacie](schematic.md)): złącza rozszerzeń, I2C,
przycisku awaryjnego, czwartego wyjścia, zapasowego stabilizatora 3,3 V, zworek wyboru `JP_S`,
`JP_UART`, `JP_3V3`.

Na stałe zamiast do wyboru:

- **SLNT** modułu CAN jest podciągnięte do 3,3 V przez R3 (10k). Nie ma `JP_S`.
- **3,3 V** dla modułu CAN pochodzi z płytki deweloperskiej ESP32. Nie ma `JP_3V3` ani
  zapasowego stabilizatora.
- **UART** jest połączony w kierunku zmierzonym w aucie wzorcowym. Nie ma zworek zamiany
  `JP_UART`: żeby zamienić, zamień dwa przewody na J4.
- **Linie CAN** idą prosto z J5 do modułu. Rezystorów 0 omów pod koraliki ferrytowe
  i opcjonalnej ochrony linii D4 ze schematu nie ma na płytce.
- **Wzmocnienie wzmacniacza**: wejście GAIN zostaje niepodłączone (domyślne ustawienie modułu).
- **Głośnik**: przykręcony do zacisku modułu wzmacniacza. Wyjście wzmacniacza jest mostkowe:
  żaden przewód głośnika nie może dotykać masy.

## Tor nadawania {#transmit-path}

Przeczytaj to, zanim zbudujesz albo przerobisz płytkę.

Tak jak jest narysowana, płytka prototypu sprzętowo tylko słucha:

- SLNT modułu CAN jest podciągnięte przez R3, więc TJA1051 zostaje w trybie cichym: nadajnik jest
  wyłączony, odbiornik działa normalnie.
- JP1 (linia TX z GPIO5 do modułu) to miejsce na 2 piny, które **zostawiasz otwarte**. Przy
  otwartej JP1 wejście TX modułu wisi i zostaje w stanie recesywnym.
- GPIO6, którym program steruje SLNT, **nie jest połączone** z modułem.

Sama JP1 nie włącza nadawania: SLNT nadal trzyma stan wysoki przez R3. Nadawanie wymaga zarówno
zwartej JP1, **jak i** przewodu z SLNT (pin 3 M3) do GPIO6 (pin 6 M1).

Płytkę autora przerobiono w ten sposób do prób z nadawaniem: JP1 zwarta, przewód z SLNT do GPIO6,
R3 zostawiony jako podciągnięcie na czas startu.

> [!CAUTION]
> **Przerobiona płytka nie ma blokady sprzętowej**
>
> Przy zwartej JP1 i dołożonym przewodzie SLNT tylko program decyduje, czy urządzenie nadaje
> (tryb tylko do nasłuchu TWAI i GPIO6 w stanie wysokim = tylko nasłuch). Nie rób tej przeróbki,
> chyba że celowo budujesz osobny program z nadawaniem (`decoder_tx_wifi`) i akceptujesz
> ryzyko. Przeczytaj [Nadawanie na magistralę CAN](../safety/transmit.md).

W każdym środowisku poza `decoder_tx_wifi`, a w `decoder_tx_wifi` dopóki nie jest uzbrojone,
GPIO6 zostaje w stanie wysokim, a kontroler CAN jest w trybie tylko do nasłuchu. Program z nadawaniem
ściąga GPIO6 w stan niski tylko na czas jednej ramki.

Na przerobionej płytce w aucie ramki wysłane przez program z nadawaniem zostały potwierdzone na
magistrali (zmierzone (measured): konsola urządzenia). Co program może wysyłać, opisuje strona
o nadawaniu.

> [!NOTE]
> **Niewyjaśnione: brak odbioru przy zwartej JP1**
>
> Raz, w aucie przy zwartej JP1, płytka nie odebrała żadnej ramki CAN; po otwarciu JP1 odbiór
> wrócił. Sprawdzenie ciągłości nie wykazało zwarcia między ścieżkami TX i RX. Przyczyna nie
> jest znana.

## Sprawdzone na zbudowanej płytce {#verified-on-the-built-board}

| Sprawdzenie | Status |
|---|---|
| wszystkie wartości rezystorów i kondensatorów, ciągła masa bez zwarć do ścieżek, brak zwarcia między szynami zasilania, piny TRACO, każdy pin gniazd ESP32, RX-TX | zmierzone: miernik, przed włożeniem modułów |
| start z samego USB (ESP32 + moduł CAN + wzmacniacz, bez 12 V): program zgłasza `can=ok audio=ok radio=ok`, polecenie konsoli `test` odtwarza ton z głośnika | zmierzone: konsola urządzenia |
| odbiór CAN, UART do radia, zasilanie 12 V w aucie | zmierzone w aucie, zobacz [Peugeot 508 SW](../reference/vehicles/peugeot-508.md) |
| wyjścia 12 V ACC / ILL / REV | **niezmierzone** na stole; brak osobnego raportu |

## Różnice zbudowanej płytki {#differences-of-the-built-board}

- Kilka elementów pochodzi z rozebranej skrzynki CAN z zestawu radia: tranzystory P-MOSFET
  (oznaczenie `3407`), diody wyjściowe, bezpiecznik wejściowy. D1 to 1N4001 z tej samej skrzynki
  (oznaczenie `M1`), nie SS34.
- Na początku brakowało D2 (SMBJ18A) i diod Zenera D5, D7, D9, a C1 był elementem 220 uF / 25 V
  przeznaczonym tylko na stół.
- Przelotki masy wywiercono tam, gdzie wybrał autor, bo ich znaczniki nie przetrwały trawienia.
- Odczyt napięcia akumulatora to napięcie **za** diodą D1, więc pokazuje około 0,8 V (0,7-0,9 V) mniej niż
  akumulator (uwaga w `firmware/boards/board_proto_v1.h`). Przy zasilaniu z samego USB, bez 12 V,
  odczyt to szum około 0-1,5 V; poniżej progu konsola i panel traktują go jako „brak 12 V”.

> [!WARNING]
> **Przed autem**
>
> Przejdź [Listę kontrolną montażu](../install-in-car/checklist.md): są na niej D2 i C1, otwarta
> JP1, brak terminatora i sprawdzenia bez zasilania.

## Jeśli brakuje elementu {#if-a-part-is-missing}

- Brak diod Zenera 12 V (D5, D7, D9): płytka działa bez nich; chronią tylko bramki MOSFET-ów.
- Brak bezpieczników polimerowych: mostek z drutu zamiast F1-F4 **tylko na stole**, z bezpiecznikiem
  w przewodzie +12 V zasilacza.
- Brak SMBJ18A (D2): na stole możesz ją pominąć. Wlutuj ją przed autem.

Dalej: [Obudowa](enclosure.md).
