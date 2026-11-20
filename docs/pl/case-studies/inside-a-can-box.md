# Wnętrze skrzynki CAN z rynku wtórnego

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Radio Android rozmawia z autem przez małą „skrzynkę CAN”: słucha ona magistrali auta i wysyła do
radia prostszy protokół szeregowy. W trakcie projektu otwarto dwie skrzynki: **Simple Soft
RP5-PA-001** i **Raise RZ-PSA15**. Ta strona opisuje tylko to, co widać na płytkach i co zmierzono
multimetrem albo nagraniem CAN. Nie opisuje ich oprogramowania.

Projekt nie jest powiązany z Simple Soft ani z Raise. Zdjęcia płytek zrobił autor; nie są
publikowane w tej dokumentacji.

## Po co w ogóle otwierać skrzynkę {#why-open-a-box-at-all}

Skrzynka Simple Soft była głównym podejrzanym dwóch usterek w aucie wzorcowym (patrz
[Zawieszony ton czujników parkowania](parking-tone-bug.md)). Żeby zrozumieć usterki, a później
zastąpić skrzynkę, autor musiał wiedzieć trzy rzeczy: jaki ma transceiver CAN, czy terminuje
magistralę i który pin jej złącza do czego służy.

## Jak odczytano płytki {#how-the-boards-were-read}

- **Film, klatka po klatce.** Płytki sfilmowano w trybie makro, a oznaczenia elementów odczytano
  z pojedynczych klatek. Część oznaczeń potwierdzono lupą.
- **Multimetr.** Ciągłość od pinów złącza do elementów, rezystancja między liniami CAN przy
  skrzynce bez zasilania.
- **Nagrania.** Sniffer w trybie tylko do nasłuchu na magistrali, przy działającej skrzynce i radiu.

Oznaczenie odczytane z płytki to fakt o płytce. To, czym jest dany element, wynika z karty
katalogowej producenta dla tego oznaczenia. Tam, gdzie oznaczenia nie znaleziono w żadnej karcie
katalogowej, tabela to mówi.

## Simple Soft RP5-PA-001 {#simple-soft-rp5-pa-001}

### Elementy {#parts}

| Pozycja na płytce | Odczytane oznaczenie | Co to jest | Status |
|---|---|---|---|
| U1 | ARTERY AT32F415CBT7, LQFP48 | mikrokontroler (Cortex-M4) z jednym kontrolerem CAN | odczytane; typ z karty katalogowej producenta |
| B1 | ZGC8.000 | kwarc 8 MHz | odczytane |
| obok U10 | CSC1040, SO-8 | transceiver CAN **high-speed**, rozkład pinów jak w TJA1040 (pin 8 = czuwanie) | odczytane; typ z karty katalogowej producenta |
| U10 | rodzina BY25D40ES, SO-8 | pamięć flash SPI 4 Mbit | odczytane; typ z karty katalogowej producenta |
| U15 | LM4890S | wzmacniacz audio 1 W, prawdopodobnie do zewnętrznego głośnika z zestawu | odczytane; zastosowanie to wniosek |
| — | 78D05 (TO-252) | stabilizator 5 V | odczytane |
| U7, U9 | 6118A, 6203A | stabilizatory; napięcia wyjściowe **niezmierzone** | odczytane; typ niezweryfikowany (unverified) |
| U3, U4, U5, U12 | 3407 (SOT-23) | tranzystory MOSFET z kanałem P | odczytane; typ niezweryfikowany |
| F1 | JK 110 | bezpiecznik polimerowy | odczytane |
| D10 | SS14 | dioda Schottky'ego | odczytane |
| B2 | puste pole z oznaczeniem „+” | miejsce na brzęczyk, **nieobsadzone** | odczytane |

Zasilanie 5 V na 3,3 V działa (zmierzone: multimetr).

### Pomiary {#measurements}

| Co | Wynik | Status |
|---|---|---|
| Rezystancja CANH-CANL przy transceiverze, skrzynka bez zasilania | 48 kΩ: **brak rezystora terminującego** | zmierzone |
| Pola obok linii CAN | dwie pary pustych pól: nieobsadzona terminacja | odczytane ze zdjęć; zgodne z 48 kΩ |
| Pin czuwania transceivera (pin 8) | połączony z pinem 2 mikrokontrolera | zmierzone: ciągłość |
| Element szeregowy na każdej linii CAN między transceiverem a złączem | około 9 Ω, elementu nie zidentyfikowano | zmierzone |
| Złącze 2x8, CAN | górny rząd pozycja 2 = CANH, dolny rząd pozycja 2 = CANL | zmierzone: ciągłość od pinów 7 i 6 transceivera |
| Złącze 2x8, zasilanie | dolny rząd pozycja 1 = +12 V (przez F1), górny rząd pozycja 1 = GND | zmierzone: ciągłość |
| 5-otworowe złącze J3 | GND, dwa piny mikrokontrolera, VBAT, zasilanie: **to nie jest złącze debugowania (SWD)** | zmierzone: ciągłość |
| 6-otworowe złącze J2 | GND, piny mikrokontrolera, VDD, jeden przewód do złącza | zmierzone: ciągłość |

„Górny rząd” i „dolny rząd” oznaczają: patrząc w gniazdo od strony wtyczki, płytka na dole;
górny rząd jest dalej od płytki.

### Odczytane ze zdjęć, niezmierzone {#read-from-the-photos-not-measured}

- Trzy tranzystory P-MOSFET (U3, U4, U5), każdy sterowany małym tranzystorem: najpewniej trzy
  przełączane wyjścia 12 V. Czwarty P-MOSFET (U12) stoi na wejściu zasilania za bezpiecznikiem i
  diodą.
- Dwa jednakowe tory (560 Ω, kondensator, 1 kΩ) przy złączu: najpewniej dwie szeregowe linie
  danych do radia. Do których pinów złącza dochodzą, nie potwierdzono.

### Co skrzynka robi na magistrali {#what-the-box-does-on-the-bus}

Sniffer przy radiu pokazał, że ta skrzynka nie tylko słucha. Wysyłała ramki tak, jakby była
fabrycznym radiem **i** modułem telematyki: stan radia, głośność, ustawienia audio, żądania zmiany
ustawień pojazdu, przyciski ekranu, przyciski klimatyzacji (zmierzone: nagrania z podłączoną
skrzynką; szczegóły na stronie pojazdu `vehicles/peugeot-508-sw-2015/README.md`). Gdy auto
zasypiało, skrzynka milkła razem z nim i na postoju nie wysyłała nic (zmierzone: nagranie na
postoju).

Linia szeregowa do radia: 38400 bodów, 8N1, poziomy 3,3 V (zmierzone: rejestrator na obu
przewodach).

### Jedna przeróbka i jej wynik {#one-modification-and-its-result}

Żeby sprawdzić pomysł, że usterki powoduje pin czuwania, pin 8 transceivera odcięto od pola
lutowniczego i połączono z masą, więc transceiver zawsze zostaje w trybie normalnym. Ton dalej się
zawieszał, tylko na krócej; pilot dalej nie działał. Patrz
[próba 2 w historii tonu czujników](parking-tone-bug.md#attempt-2-the-standby-pin).

## Raise RZ-PSA15 {#raise-rz-psa15}

Tę skrzynkę odczytano tylko z jednego filmu, a potem zmierzono przez jej wiązkę. Dalej jej nie
rozbierano.

### Elementy (odczytane z filmu) {#parts-read-from-the-video}

| Co | Odczytane oznaczenie | Status |
|---|---|---|
| Mikrokontroler, LQFP48 | własne oznaczenie („Pocket Link N12”), typ **nieznany** | odczytane |
| Kwarc | 24,000 MHz | odczytane |
| Transceiver CAN, SO-8 | P1040, ta sama rodzina 1040 co w skrzynce Simple Soft | odczytane |
| Terminacja | przy transceiverze nie widać rezystora 120 Ω | odczytane |
| Brzęczyk | **wlutowany** na płytce | odczytane |
| Wyjścia 12 V | trzy jednakowe stopnie przełączające i czwarty | odczytane |
| Przy złączu | dwa rezystory 560 Ω i diody (prawdopodobnie linie szeregowe), dzielnik 47 k / 18 k (prawdopodobnie pomiar 12 V) | odczytane; zastosowanie to wniosek |
| Złącze | 2x8, tego samego typu co w skrzynce Simple Soft | odczytane |

Pola lutownicze złącza zakrywa nadruk i obudowa, więc rozkładu pinów nie dało się odczytać z
płytki.

### Pomiary (bez zasilania, przez jej wiązkę na wtyku Quadlock auta) {#measurements-unpowered-through-its-harness-at-the-cars-quadlock-plug}

| Co | Wynik | Status |
|---|---|---|
| CANH-CANL (piny 10 i 13 Quadlocka) | 59 kΩ: **brak terminacji** | zmierzone |
| +12 V do masy (piny 12 i 16) | 2,63 MΩ: brak zwarcia | zmierzone |
| Masa wiązki (pin 16) do masy płytki | ciągłość | zmierzone |

### W aucie {#in-the-car}

Skrzynkę Raise wpięto w wiązkę Simple Soft; pozycje się zgadzały. Z profilem radia ustawionym na
Raise (zmierzone: w aucie, przez autora):

- radio się uruchomiło; przyciski przy kierownicy i głośność działały, bez opóźnienia na
  przyciskach głośności (skrzynka Simple Soft wysyłała kroki głośności około 220 ms za późno);
- czujniki parkowania pokazywały się na ekranie, a brzęczyk skrzynki pikał razem z własnymi
  piknięciami auta;
- brzęczyk ma tylko około trzech stopni, a ton ciągły zaczyna się wcześniej niż w układzie
  fabrycznym;
- klimatyzacja: brak sterowania i brak temperatury na ekranie;
- przyciski nad panelem klimatyzacji były pomieszane;
- pilot działał po każdym postoju, także po nocy.

## Co to oznaczało dla urządzenia {#what-this-meant-for-the-device}

- Żadna ze skrzynek nie terminuje magistrali, więc urządzenie też nie może. Podłącza się
  równolegle do istniejącej magistrali, bez rezystora 120 Ω.
- Skrzynka Simple Soft używa transceivera CAN high-speed (jego oznaczenie i rozkład pinów
  odczytano z układu). Transceiver w Raise ma oznaczenie `P1040`, co wygląda na tę samą rodzinę
  1040, więc prawdopodobnie też jest high-speed (niezweryfikowane: tylko oznaczenie). Napięcia
  magistrali wyglądają na CAN low-speed fault-tolerant. Obie skrzynki działają; działa też
  transceiver high-speed tego urządzenia. Dlaczego to działa, nie udowodniono: brakuje jeszcze
  pomiaru oscyloskopem.
- Rozkład pinów złącza 2x8 zmierzony na skrzynce Simple Soft (GND, +12 V, CANH, CANL) to ten,
  którego prototyp używa, żeby wpiąć się w istniejącą wiązkę, przez gniazdo wyjęte z tej skrzynki.
- Skrzynka Simple Soft udaje na magistrali fabryczne radio i moduł telematyki. Urządzenie, które ją
  zastępuje i **nie** nadaje, zostawia te ramki brakujące. Co auto robi bez nich, wiadomo tylko
  częściowo.
