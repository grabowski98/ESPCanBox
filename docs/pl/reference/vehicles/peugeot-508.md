# Peugeot 508 SW (2015)

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../../contributing/translations.md#send-your-work).

Auto wzorcowe tego projektu: auto, na którym program powstał i był testowany. Profil programu:
`psa_aee2010` (flaga kompilacji `-DCAR_PROFILE=PSA_AEE2010`, domyślna).

> [!NOTE]
> **Bez powiązań**
>
> Ten projekt nie jest powiązany z Peugeot, Stellantis, Simple Soft, Raise, Junsun ani FYT. Nazwy
> służą tylko do tego, żeby powiedzieć, co było testowane.

Szczegółowe, wiążące pliki tego auta są w repozytorium w `vehicles/peugeot-508-sw-2015/`:

- `README.md`: lista sygnałów ze statusem;
- `508.dbc`: ramki i sygnały z komentarzem o statusie przy każdym (otwórz w SavvyCAN albo dowolnym
  narzędziu DBC);
- `settings-map.md`: ustawienia pojazdu z ekranu radia;
- `wiring.md`: piny Quadlocka, wtyk skrzynki CAN, wtyki radia i jak zmierzyć własne.

## Słowa statusu {#status-words}

- **zmierzone** (measured): widziane w tym aucie, w nagraniu (z nazwą pliku) albo w konsoli
  urządzenia. Nagrania używane przez testy są w `firmware/test_data/captures/`; ich ramki z VIN są
  nadpisane.
- **niezweryfikowane** (unverified): z publicznego opisu, głównie PSA-RE
  (github.com/prototux/PSA-RE, `buses/AEE2010.full/LS.INFO-DIV`, commit 67f7581e, Apache-2.0),
  niepotwierdzone w tym aucie. PSA-RE opisuje całą rodzinę aut. Trzy jego pola nie pasują do tego
  auta: `2E1`, `236` bajt 8 i `3E1` 4.5 nigdy się tu nie zmieniają.

Numeracja bajtów i bitów: bajty CAN od 1 (bajt 1 = pierwszy bajt danych), bity 7..0, krótki zapis
`bajt.bit` (`5.7` = bajt 5 bit 7). Bajty UART: `DATA[0]` = pierwszy bajt danych. Zobacz
[słowniczek](../../contributing/glossary.md).

## Auto {#the-car}

| Pozycja | Wartość | Status |
|---|---|---|
| Model | Peugeot 508 SW, początek gwarancji 31 marca 2015, 1.6 e-HDi, ręczna skrzynia biegów | dokumenty auta (dokument VIN, narzędzie diagnostyczne) |
| Architektura elektryczna | PSA AEE2010 | narzędzie diagnostyczne serwisu wymienia moduły BSI2010, COM2008P, ESP81; 66 z 70 ID ramek przy radiu zgadza się nazwą i okresem z PSA-RE dla AEE2010 |
| Radio fabryczne | zastąpione radiem Android z rynku wtórnego (platforma FYT). Fabrycznie: radio z nawigacją, bez wzmacniacza, 8 głośników | autor |
| Wyposażenie ważne tutaj | czujniki parkowania przód i tył, wyświetlacz przezierny (HUD), Stop and Start z przyciskiem ECO OFF, monitorowanie martwego pola | autor, narzędzie serwisowe |
| Wypróbowane skrzynki CAN z rynku wtórnego | Simple Soft RP5-PA-001 (protokół „XP”), Raise RZ-PSA15 | autor |

BSI to moduł nadwozia auta. Odbiera żądania personalizacji opisane niżej.

## Magistrala przy radiu {#the-bus-at-the-radio}

| Fakt | Status |
|---|---|
| CAN 125 kbit/s, ID 11-bitowe | zmierzone: odbiór bez błędów przy tej prędkości (`can_20261110_190421.log`) |
| około 218 ramek/s przy włączonym zapłonie, 70 ID, z tego 58 okresowych | zmierzone (`can_20261110_190421.log`) |
| 66 z 70 ID ma nazwę i okres podane przez PSA-RE dla INFO-DIV; nieznane: `2E3`, `323`, `363` | zmierzone, porównane z PSA-RE |
| Quadlock pin 10 = CANH, pin 13 = CANL | zmierzone: ciągłość od transceivera skrzynki CAN do Quadlocka oraz bezbłędny odbiór (17 169 ramek) przy takim przypisaniu |
| CANH 0,65 V, CANL 4,5 V średnio względem masy, zapłon włączony, multimetr DC | zmierzone; tak samo z podłączonym urządzeniem i bez niego |
| typ magistrali | **niezweryfikowane.** Napięcia wyglądają na CAN low-speed fault-tolerant (ISO 11898-3), a nie high-speed CAN (obie linie w spoczynku około 2,5 V). Do potwierdzenia potrzebny jest oscyloskop |
| transceiver high-speed (3,3 V) mimo to odbiera ją bez błędów | zmierzone |
| kluczyk wyjęty: brak ramek | zmierzone |
| po wyłączeniu: pełny ruch przez około 100 s, potem przez 14 s sama ramka `036`, potem cisza aż do otwarcia auta | zmierzone |
| ramki `336`, `3B6`, `2B6` niosą VIN w ASCII | zmierzone. **Zamaskuj je, zanim udostępnisz nagranie** (`tools/sanitize_capture.py`) |

Jak podłączyć się do tej magistrali: [Połączenie z CAN](../../install-in-car/can-connection.md).

## Sygnały używane przez program {#signals-used-by-the-firmware}

Wszystkie wiersze są zmierzone w tym aucie, chyba że wiersz mówi inaczej.

| Sygnał | Ramka i pole | Jak potwierdzono |
|---|---|---|
| zapłon / silnik | `0F6` bajt 1: `80` wyłączony, `88` włączony, `8E` silnik pracuje (`90`, `91`, `89`, `8A` rozruch; `86`, `84` wyłączanie) | nagrania z kolejnymi krokami zapłonu i silnika |
| bieg wsteczny | `0F6` 8.7 | wsteczny włączony 3 razy w nagraniu |
| kierunkowskazy | `0F6` 8.1-8.0 | krok w nagraniu |
| temperatura zewnętrzna | `0F6` bajt 7: wartość / 2 - 40 °C | zgadza się z ramką skrzynki do radia w 21 z 21 ramek |
| światła włączone | `036` 4.5 | kroki włączenia / wyłączenia świateł |
| tryb ekonomiczny (zapłon włączony, silnik wyłączony) | `036` 3.7 | 10 z 10 ramek |
| wciśnięte pokrętło zasilania na panelu | `036` bajt 7: `01` spoczynek, `02` wciśnięte | nagranie CAN + UART |
| czujniki parkowania | `0E1`, zobacz [niżej](#parking-sensors-0e1) | wiele podjazdów przodem i tyłem; ramka skrzynki do radia zgadza się w 394 z 396 ramek |
| czujniki zablokowane | `126` bajt 1 = `18` | 4 naciśnięcia przycisku na ekranie |
| przyciski przy kierownicy | `21F` (głośność, telefon, LIST, pokrętło i jego wciśnięcie), `0A2` (SRC, lewe pokrętło kierownicy) | 36 z 36 naciśnięć zgadzało się z ramkami skrzynki do radia |
| przyciski panelu środkowego | `122` (NAV, MEDIA, TEL, SETUP, DRIVE, przewijanie, lupa, wygaszenie ekranu, wyciszenie mikrofonu, WEB, pokrętło głośności) | każdy przycisk naciśnięty w nagraniu; jeden przycisk (kod `22`) nierozpoznany |
| komputer pokładowy | `221` (bieżąca strona, przycisk na dźwigni), `2A1` trasa 1, `261` trasa 2 | skopiowane bajt po bajcie przez skrzynkę (wszystkie 9-11 ramek każdej); znaczenie wartości niezweryfikowane |
| kąt skrętu kierownicy | `10B` bajty 1-2 | skopiowany przez skrzynkę, 155 ze 155 ramek; skala niezweryfikowana |
| stan klimatyzacji | `350` + `2AD` (ogrzewanie tylnej szyby) | zgadzało się 26 z 27 ramek skrzynki |
| komunikat o otwartych drzwiach | `1A1` bajt 2 = `DE`, bity drzwi w bajcie 4 | każde drzwi w nagraniu |
| stan Stop and Start | `3E1` bajt 4 | zobacz [Przyciski ECO OFF i ASR](#eco-off-and-asr-buttons) |
| ASR | `217` 3.6 naciśnięcie, `227` 1.4 stan | zobacz [Przyciski ECO OFF i ASR](#eco-off-and-asr-buttons) |
| personalizacja | `260` stan, `15B` żądanie | zobacz [niżej](#personalisation-with-15b-and-260) |

Pełne położenia bitów są w `vehicles/peugeot-508-sw-2015/508.dbc`.

Fizyczny panel klimatyzacji auta nie wysyła na tę magistralę ramek przycisków: zmienia się tylko
ramka stanu `350` (zmierzone).

### Czujniki parkowania 0E1 {#parking-sensors-0e1}

Ramka `0E1` przychodzi z modułu czujników parkowania co 100 ms (zmierzone:
`can_20261110_190421.log`, `can_20261111_191045.log`).

| Pole | Treść | Status |
|---|---|---|
| stan tyłu, stan przodu | 4 aktywne, 5 oczekiwanie, 2 wyłączone przez kierowcę, 6 po wyłączeniu zapłonu | zmierzone |
| żądanie dźwięku | 1, gdy auto pika | zmierzone |
| przód / tył | 1 = przeszkoda z przodu, 0 = z tyłu | zmierzone (nagranie CAN + UART) |
| odstęp piknięć | `3F` bez dźwięku, `39` ... `0B` pikanie przerywane, `00` ton ciągły. Po `0B` następna wartość to `00`. Jednostka nieustalona | zmierzone |
| strefy sześciu czujników (tył L/Ś/P, przód L/Ś/P) | 0 najbliżej ... 7 daleko | zmierzone |
| żądanie wyświetlenia | 1 razem z przeszkodą | zmierzone |

Przednie czujniki zaczynają żądać dźwięku przy odstępie `18` (zmierzone tym urządzeniem w aucie).

## Personalizacja przez 15B i 260 {#personalisation-with-15b-and-260}

Ekran „ustawienia pojazdu” w radiu zmienia pozycje personalizacji w BSI.

| Ramka | Nadawca, okres | Rola |
|---|---|---|
| `260` | BSI, 500 ms, 7 bajtów | **stan** personalizacji |
| `15B` | radio / telematyka, przy zmianie, 8 bajtów | **żądanie** personalizacji do BSI |
| `361` | BSI, 500 ms, 6 bajtów | które funkcje ma auto (radio ukrywa wiersze funkcji, których nie ma) |

Jak przebiega jedna zmiana (zmierzone ze skrzynką z rynku wtórnego i z tym urządzeniem): radio
wysyła `80 nn vv` -> około 6 ms później idzie jedna `15B` -> BSI pokazuje nowy stan w następnej
`260`, 0,15-0,55 s później. Powtórzonej `15B` nie widziano.

`15B` jest zbudowana tak: bajt 1 = `00`; bajty 2-5 = bajty 2-5 ostatniej `260` z jednym zmienionym
polem; bajt 6 = bajt 6 z `260` z wyzerowanymi bitami 3, 1 i 0; bajty 7-8 = `00`. Bit 6.3 to w
PSA-RE żądanie kalibracji ciśnienia w oponach (niezweryfikowane) i zawsze jest wysyłany jako 0.

**BSI przyjmuje pojedynczą `15B` od tego urządzenia**, bez ramek okresowych, które normalnie
wysyła moduł telematyki (zmierzone: konsola wcześniejszej kompilacji tego programu z nadawaniem, w
aucie; zapis nieopublikowany). Każda pozycja niżej została przyjęta w obie strony:

| Polecenie radia | Pozycja | Pole w `260` / `15B` | Wysłana `15B` (przed: `260` = `A4 1C 8F BB C5 50 00`) | Status |
|---|---|---|---|---|
| `80 01 v` | asystent parkowania | 5.7 | `80 01 00`: `00 1C 8F BB 45 50 00 00`; `80 01 01`: `00 1C 8F BB C5 50 00 00` | zmierzone: przyjęte po 555 ms i 367 ms; także w nagraniu skrzynki z rynku wtórnego (4 polecenia, wszystkie przyjęte) |
| `80 02 v` | tylna wycieraczka przy cofaniu | 5.5 | `80 02 01`: `00 1C 8F BB E5 50 00 00` | zmierzone: przyjęte |
| `80 06 v` | oświetlenie „prowadź mnie do domu”, v = 0 wyłączone, 1-3 = czas | 4.7 włączone, 4.6-4.5 = v - 1 | `80 06 03`: `00 1C 8F DB C5 50 00 00` | zmierzone: przyjęte |
| `80 07 v` | oświetlenie powitalne, jak wyżej | 4.3 włączone, 4.2-4.1 = v - 1 | `80 07 03`: `00 1C 8F BD C5 50 00 00` | zmierzone: przyjęte |
| `80 09 v` | zestaw dźwięków, v = 1-4 | 2.1-2.0 = v - 1 (2.2 musi już być 1) | `80 09 02`: `00 1D 8F BB C5 50 00 00` | zmierzone: przyjęte, bajt 1 = `00` wystarcza |
| `80 0F v` | odblokowanie drzwi, 1 = wszystkie drzwi, 0 = tylko kierowcy | 3.6 odwrócony | `80 0F 00`: `00 1C CF BB C5 50 00 00` | zmierzone: przyjęte |

W tych próbach BSI przyjmowało zmianę po 140-572 ms. Po próbach `260` wróciła do `1C 8F BB C5 50` w
bajtach 2-6.

> [!WARNING]
> **Przyjęte to nie to samo co działające**
>
> Zmierzone jest **przyjęcie** każdego żądania przez BSI: nowa wartość pojawia się w `260`.
> Widocznego skutku w aucie (wycieraczka, światła, dźwięki, odblokowanie) nie sprawdzano.

Pozostałe wiersze ekranu radia (odblokowanie bagażnika, jednostki paliwa, język, Stop and Start,
kalibracja ciśnienia w oponach, opóźnienie reflektorów, kontrola trakcji, wykrywanie zmęczenia,
kolor zestawu wskaźników, automatyczne światła, automatyczne ryglowanie, reflektory adaptacyjne i
wiersze `80 1B`-`80 1F`) **nie mają zmierzonej ramki**. Program je odrzuca. Kalibracji ciśnienia
w oponach nie wolno wysyłać nigdy. Numer polecenia każdego wiersza nagrano 19.11.2026, przechodząc
ekran wiersz po wierszu (tabela w `vehicles/peugeot-508-sw-2015/settings-map.md`); radio nie wysłało
`80 04`, `80 08` ani `80 0E`, więc program traktuje te numery jako nieznane.

## Przyciski ekranu przez 1A9 {#screen-buttons-with-1a9}

Dwa wiersze ekranu radia nie zmieniają pola w `15B`. W aucie działają jak naciśnięcie przycisku
na ekranie: jedna ramka `1A9` z jednym bitem, która **przełącza** stan. Dlatego przed naciśnięciem
trzeba odczytać stan z magistrali i nic nie wysyłać, gdy auto ma już żądany stan.

| Polecenie radia | Pozycja | Treść `1A9` | Skąd czytany stan | Nagrane ze skrzynki z rynku wtórnego | Wysłane przez to urządzenie (pojedyncza ramka) |
|---|---|---|---|---|---|
| `80 05 v` | czujniki parkowania wł. / wył. | `00 7F FF 04 FF 00 10 00` | `227` 1.6 (dioda „czujniki wyłączone”) | zmierzone: 4 naciśnięcia, 4 reakcje (`can_20261110_190421.log`) | zmierzone: **przyjęte w obie strony**; `227` zmieniła się po 286 ms i 236 ms, stan w `0E1` przeszedł 6 -> 2 -> 6 |
| `80 0C v` | monitorowanie martwego pola | `00 7F FF 20 FF 00 10 00` | `227` 3.0 | zmierzone: 4 naciśnięcia, 4 reakcje (`can_20261110_190421.log`) | zmierzone: wyłączenie **przyjęte** (76 ms); włączenie 1,2 s później **nieprzyjęte** (brak zmiany w `227` przez 2,5 s). Jedna próba, przyczyna nieznana |

Stop and Start (`80 0D`) nie jest pozycją przycisku: pojedyncza `1A9` `00 7F FF 00 FF 00 90 00`
(bit 7.7, z opisów publicznych, nigdy nienagrana w tym aucie) **nie została przyjęta** (dwie próby,
`3E1` bez zmian), a program nie ma już tej treści: odrzuca `80 0D` (brak znanej ramki).

Skrzynka z rynku wtórnego wysyłała w tle `1A9` co 204 ms. Urządzenie wysyła tylko pojedynczą ramkę
naciśnięcia. Wyniki urządzenia pochodzą z konsoli wcześniejszej kompilacji tego programu z
nadawaniem, w aucie (zapis nieopublikowany).

Bit `227` 3.0 przełącza się przyciskiem martwego pola, a zeruje się też na wstecznym i po
wyłączeniu silnika; jego dokładne znaczenie w tym aucie nie jest ustalone.

> [!CAUTION]
> **Nie ma tego w domyślnej kompilacji z nadawaniem**
>
> Program wysyła `1A9` tylko w kompilacji z `-DCAN_TX_EXPERIMENTAL=1`. Domyślna kompilacja z
> nadawaniem ją odrzuca. Zobacz [Nadawanie na magistralę CAN](../../safety/transmit.md).

## Przyciski ECO OFF i ASR {#eco-off-and-asr-buttons}

Zmierzone tym urządzeniem w trybie tylko nasłuchu, silnik pracuje, nagranie 36 s (nieopublikowane).

| Przycisk | Co zmienia się na magistrali | Status |
|---|---|---|
| ECO OFF (wyłączenie Stop and Start) | `3E1` bajt 4: `40` = Stop and Start czynny, `80` = wyłączony przyciskiem. W tym samym czasie `1A1` pokazuje `02 21 A4` (wyłączony) i `02 20 A4` (znowu włączony) | zmierzone |
| ASR (wyłączenie kontroli trakcji) | jeden impuls w `217` 3.6 na naciśnięcie (`F0 80 40 ...`, około 130-190 ms). Stan w `227` 1.4: `10 00 11 00 00`, gdy ASR jest wyłączony. Przy zmianach pojawiają się dwa komunikaty `1A1`: `82 45 A4` i `82 44 A4` | zmierzone |

W tym samym nagraniu `128` 6.4 zmienił się 8 razy, w czasie naciśnięć ECO OFF. Przyczyna nie jest
znana.

Dwa przyciski dołożone przez autora na konsoli środkowej (nie części fabryczne) nie wysyłają
na tę magistralę **nic**: podczas ich naciskania nie zmieniła się żadna ramka poza licznikami i
temperaturą (zmierzone). W tym aucie nie ma przycisku wyłączania czujników parkowania.

## Sterowanie klimatyzacją przez 364 nie działa {#climate-control-with-364-does-not-work}

Ekran klimatyzacji w radiu wysyła `8A n v`. Skrzynka CAN wysyła wtedy ramkę `364` z jednym bitem
przycisku, a około 200 ms później ramkę spoczynkową `00 00 1E 00`. **Nic z tego nie zmieniło
klimatyzacji tego auta.**

| Próba | Ramki na magistrali | Wynik | Status |
|---|---|---|---|
| skrzynka z rynku wtórnego (Simple Soft, starszy program), naciśnięcia na ekranie radia | 4-bajtowa `364`: `80 00 1E 00` x4 (temperatura lewa +), `40 00 1E 00` x3 (lewa -), `20 00 1E 00` x4 (prawa +), `08 00 1E 00` x2 (nawiew +), `04 00 1E 00` x1 (nawiew -), spoczynkowa `00 00 1E 00` co 1004 ms, po każdym naciśnięciu ramka spoczynkowa po 202 ms | `350` bez zmian | zmierzone (`can_20261110_190421.log`) |
| to urządzenie, ramka 4-bajtowa | `80 00 1E 00` (4 razy) i `40 00 1E 00` (raz), każda z ramką spoczynkową 0,5 s później; wszystkie potwierdzone na magistrali po 4-7 ms | temperatura bez zmian | zmierzone (konsola, nieopublikowane) |
| to urządzenie, ramka 6-bajtowa | `80`, `40`, `04` w bajcie 1, cztery naciśnięcia z puszczeniem, potwierdzone po 4-7 ms | brak reakcji | zmierzone (konsola, nieopublikowane) |

W próbie ze skrzynką na magistrali były też okresowe ramki radia / telematyki (`1A9`, `167`,
`165`), więc one same nie wystarczają. Autor widział, jak ta sama skrzynka z nowszym programem
sterowała klimatyzacją dzień wcześniej; tego nie nagrano.

PSA-RE opisuje `364` jako ramkę 6-bajtową, z okresem 1000 ms plus przy zmianie, i z treścią
spoczynkową `00 00 1E 78 E0 00` (niezweryfikowane: PSA-RE `LS.INFO-DIV/364.yml`). Ta postać
spoczynkowa nie była testowana w tym aucie jako ramka okresowa. Pozostałe nazwy bitów z PSA-RE (AUTO,
A/C, MAX, rozdział powietrza, MONO i inne) są niezweryfikowane.

**Wniosek: pojedyncze ramki `364` od urządzenia nie sterują klimatyzacją.** Program nie ma ścieżki
`364` od 19.11.2026 (próbę usunięto). Numery przycisków ekranu klimatyzacji w radiu (`8A 01`-`8A 0C`)
nagrano tego dnia ze znacznikami właściciela; program tylko je wypisuje.

## Ramka XP 38 {#xp-frame-38}

Radio rysuje ekran ustawień na podstawie ramki XP `38` (6 bajtów danych, skrzynka -> radio) i
ukrywa wiersze według ramki XP `3C`. Zobacz też [Protokół radia XP](../xp-protocol.md).

| `38` | Treść | Źródło na CAN | Status |
|---|---|---|---|
| `DATA[0]` bity 7, 6, 5, 4, 3 | drzwi: przednie (dwoje), tylne (dwoje), bagażnik | komunikat drzwi `1A1` | strona CAN zmierzona; położenia bitów w `38` niezweryfikowane (w `38` nie nagrano zmiany drzwi) |
| `DATA[0]` bit 2 | ustawiony przy włączonym zapłonie | | zmierzone |
| `DATA[2]` bit 4 | asystent parkowania | `260` 5.7 | zmierzone (zmieniły się razem, 2 razy) |
| `DATA[4]` bit 7 | martwe pole | `227` 3.0 | zmierzone (3 zmiany) |
| `DATA[4]` bit 6 | Stop and Start | `3E1` bajt 4 bity 7-6 | zmierzone (1 zmiana) |
| `DATA[4]` bity 3-0 | język | `260` 1.6-1.2 przez tabelę | zmierzony tylko polski (kod `260` 9 -> radio 8); innych kodów program nie tłumaczy (pole zachowuje wartość) |
| pozostałe bity | tylna wycieraczka, wykrywanie zmęczenia, kolor zestawu wskaźników, oświetlenie nastrojowe, czasy oświetlenia, światła dzienne, zestaw dźwięków, funkcja powitalna, odblokowanie drzwi, jednostki paliwa | `260`, `217`, `227` | niezweryfikowane |

Pełna reguła w programie odtwarza wszystkie 38 ramek `38` nagranych ze skrzynki, z 0 różnic (test
na komputerze `settings_38`). Dla bitów niezweryfikowanych pokazuje to tylko zgodność z wartością,
która w nagraniach się nie zmieniała.

`3C` jest budowana z `361`. Nagrano jedną wartość: `361` = `A0 66 C8 0A 00 00` daje `3C` = `9E 80`
(9 z 9 ramek). Przypisanie bitów jest niezweryfikowane.

## Ramki wysyłane przez skrzynkę CAN z rynku wtórnego {#frames-sent-by-an-aftermarket-can-box}

Skrzynka Simple Soft działała na tej magistrali jako radio fabryczne **i** jako moduł telematyki.
Widziane w nagraniach:

| Ramka | Co się zmienia | Status |
|---|---|---|
| `165` stan radia | źródło (bajt 3 bity 6-4), wyciszenie | zmierzone |
| `1A5` głośność | poziom z polecenia głośności radia, 4-10 ms później | zmierzone |
| `1E5` ustawienia dźwięku | pojedyncze bajty zmieniają się z balansem / basami w radiu | zmierzone; w tym aucie nie ma wzmacniacza, który by zareagował |
| `15B` żądanie personalizacji | po `80 nn vv` z radia, przyjęte przez BSI | zmierzone |
| `1A9` przyciski ekranu | czujniki wyłączone, martwe pole | zmierzone |
| `364` przyciski klimatyzacji | impulsy po zmianach temperatury w radiu; **auto nie zareagowało** | zmierzone |
| `2E3`, `363` | tekst / częstotliwość dla zestawu wskaźników, zmieniają się ze źródłem | zmierzone; układ niezweryfikowany |
| `3E5`, `167`, `1A3`, `323` | stałe w nagraniach | zmierzone |

Istnieje jedno krótkie nagranie bez skrzynki (`firmware/test_data/captures/centralka_can_20261117_184411.log`,
urządzenie w trybie dekoder, około 10 s ramek). Nie ma w nim żadnej ramki z tabeli, choć `167` (co
104 ms) i `1A9` (co 204 ms) pojawiłyby się w 10 s dziesiątki razy, gdyby wysyłał je inny moduł
(zmierzone). To częściowo potwierdza, że wysyła je skrzynka. Nagranie jest krótkie i nie powtarza
tych samych czynności, więc nie wiadomo, czego autu bez nich brakuje.

## Znany problem: ton czujników parkowania nie milknie {#known-issue-parking-tone-keeps-sounding}

Z zamontowaną skrzynką z rynku wtórnego ton ciągły czujników parkowania czasem grał dalej po
wyłączeniu silnika z przeszkodą w najbliższej strefie. Przy wyłączeniu moduł czujników zawsze
wysyłał `0E1` **bez** żądania dźwięku (zmierzone), więc zawieszony ton jest trzymany dalej w aucie,
a nie przez ramkę na tej magistrali. Raz ton zawiesił się bez podłączonej skrzynki. Cała historia:
[Błąd tonu czujników parkowania](../../case-studies/parking-tone-bug.md).

## Czego nie wiadomo {#what-is-not-known}

- Typ magistrali: potrzebny jest oscyloskop.
- Czego autu brakuje bez ramek skrzynki z rynku wtórnego (istnieje tylko jedno krótkie nagranie bez skrzynki).
- Widoczny skutek przyjętych żądań `15B`.
- Dlaczego włączenie monitorowania martwego pola pojedynczą `1A9` nie zostało przyjęte (jedna próba).
- Treść `1A9` dla Stop and Start i to, czy BSI w ogóle ją przyjmuje.
- Czy `364` w 6-bajtowej postaci spoczynkowej wysyłana okresowo cokolwiek zmienia.
- Znaczenie `2E3`, `323`, `363`, wartości komputera pokładowego i skali kąta skrętu kierownicy.
- Który przycisk panelu wysyła kod XP `22`.
