# Dekodowanie ramek

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Masz nagranie ze znacznikami ([Nagrywanie magistrali](record.md)). Teraz szukasz bitów, które
zmieniły się, gdy coś robiłeś w aucie. Ta strona robi to krok po kroku na prawdziwych nagraniach
auta wzorcowego, narzędziem `firmware/tools/can_diff.py`.

Narzędzie tylko czyta pliki. Potrzebuje Pythona 3 i niczego więcej. Wszystkie polecenia poniżej
uruchamiasz z katalogu repozytorium.

Przykłady używają nagrań z repozytorium. Twoje własne nagranie z `record.py` leży w katalogu
`logs` obok katalogu repozytorium, w pliku, którego nazwa kończy się na
`_can_YYYYMMDD_HHMMSS.log`. Podaj zamiast przykładu jego ścieżkę, na przykład
`python -I firmware/tools/can_diff.py ../logs/<nazwa pliku> --list`.

## Bajty i bity {#bytes-and-bits}

Ramka CAN ma ID (trzy cyfry szesnastkowe, na przykład `0F6`) i do 8 bajtów danych. Ten projekt
numeruje je tak:

- bajty od **1**: bajt 1 to pierwszy bajt danych (`data[0]` w kodzie C++);
- bity **7..0** w bajcie, bit 0 jest najmłodszy;
- zapis skrócony `bajt.bit`: `8.7` = bajt 8, bit 7.

Przykład: ramka `0F6#884D1E2182585828` ma bajt 1 = `88`, bajt 2 = `4D`, ... bajt 8 = `28`.

Żeby zobaczyć bity, zapisz każdą cyfrę szesnastkową jako cztery cyfry dwójkowe:

| Hex | Dwójkowo (bity 7..0) |
|---|---|
| `24` | `0010 0100` |
| `A4` | `1010 0100` |

`24` i `A4` różnią się tylko skrajnym lewym bitem, więc zmienił się bit 7.

## Co robi narzędzie {#what-the-tool-does}

Dla każdego znacznika `can_diff.py` porównuje dwa okna czasu:

- **przed**: sekundy tuż przed znacznikiem (domyślnie 3 s, `--before`);
- **po**: sekundy tuż po nim (domyślnie 5 s, `--after`).

Wypisuje każdy bit, który był stały w oknie „przed” i zmienił się w oknie „po”. Dwa filtry usuwają
większość szumu:

- **liczniki**: bajt albo połówka bajtu, która zmienia się o tyle samo prawie w każdej ramce
  całego nagrania (liczniki kroczące, zegary). Narzędzie wypisuje je raz na górze i nigdy ich nie
  zgłasza.
- **tło**: bit, który zmieniał się już w oknie „przed”, jest dla tego znacznika pomijany
  („ignored: N background bits” w wyniku).

Wypisuje też ID, które pojawiają się dopiero po znaczniku („new IDs”), i ID, które znikają.

Najpierw sprawdź, czy narzędzie działa na twoim komputerze. Plik syntetyczny to wymyślone auto
o znanych odpowiedziach:

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --selftest
```

```
selftest: 6 markers, 3216 frames, 0 errors
```

Przydatne opcje:

| Opcja | Co robi |
|---|---|
| `--list` | wypisuje znaczniki z numerami i czasami, potem kończy |
| `--marker N` | tylko znacznik N (można powtórzyć) |
| `--match TEXT` | tylko znaczniki, których tekst zawiera TEXT |
| `--after S`, `--before S` | długości okien w sekundach |
| `--id 0F6,21F` | tylko te ID |
| `--summary` | dla wybranych znaczników: przy ilu z nich zmienił się każdy bit |
| `--export OUT` | zapisuje do OUT same linie CAN, dla innych programów |

Znaczniki to linie komentarza, które zaczynają się od `# STEP`, `# MARK`, `# MARKER` albo `# KROK`
(polskie słowo użyte w nagraniach z auta wzorcowego). Inny wzorzec podajesz w `--marker-regex`.

## Znajdź znaczniki {#find-the-markers}

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --list
```

```
firmware/test_data/captures/can_20261110_190421.log: 186700 frames, 70 IDs, 73 markers, 0 broken lines skipped
counters (never reported): 0E6:8H 10B:5L 236:5 3F6:3H
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
    3  line   4078     416.094 s  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
...
```

Linię liczników czytasz tak: w `0E6` liczy starsza połówka (`H`) bajtu 8, w `10B` młodsza połówka
(`L`) bajtu 5, w `236` cały bajt 5, w `3F6` starsza połówka bajtu 3. Narzędzie nigdy ich nie
zgłosi.

## Przykład 1: bieg wsteczny (`0F6`) {#example-1-reverse-gear-0f6}

Krok 17 tego nagrania mówi po polsku: „wsteczny: włącz 3 s, wyłącz, 3 razy (bez przeszkody)”. Krok
trwał około 27 s, więc użyj długiego okna „po”:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25
```

Bez `--id` narzędzie wypisuje 13 ID: `0A1 0B6 0E1 0E6 0F6 10B 128 161 227 236 2AD 3E7 3F6`.
Ramka czujników parkowania `0E1` też się zmienia, a wolne wartości dryfują w ciągu 25 s. Szukaj
bitu, który zmienia się **tyle razy, ile razy wykonałeś czynność**. Tutaj jest w `0F6`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25 --id 0F6
```

```
==============================================================================
marker 17, line 43732, at 597.094 s
  # KROK 17 19:08:20.249 Wsteczny: wlacz 3 s, wylacz, 3 razy (bez przeszkody)
  window: 3.0 s before, 25.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 2: 51 52
      bits: 2.1 0->1 x1 @+23.87s, 2.0 1->0 x1 @+23.87s
    byte 8: 24 A4 24 A4 24 A4 24 A4
      bits: 8.7 0->1 x7 @+8.67s
```

Jak to czytać:

- `byte 8: 24 A4 24 ...` to kolejne wartości, które przyjął bajt, bez powtórzeń;
- `8.7 0->1 x7 @+8.67s`: bit 8.7 był 0 przed znacznikiem, zmienił się 7 razy, pierwszy raz 8,67 s
  po znaczniku.

Bit 8.7 przechodził na 1 i z powrotem raz za razem, w rytm czynności. Bajt 2 zmienił się raz,
pod koniec: to nie jest ta czynność.

Plan mówił o trzech razach; w oknie bit przeszedł na 1 cztery razy. Nagranie nie mówi dlaczego.
Ufaj temu, co pokazuje plik, porównaj to ze swoimi notatkami i potwierdź sygnał na drugim
nagraniu.

Wynik: bieg wsteczny = `0F6` 8.7 (zmierzone (measured): `can_20261110_190421.log`, krok 17; ten sam
bit jest w `vehicles/peugeot-508-sw-2015/508.dbc`).

## Przykład 2: zapłon i silnik (bajt 1 ramki `0F6`) {#example-2-ignition-and-engine-0f6-byte-1}

Niektóre sygnały to nie jeden bit, tylko cały bajt z kilkoma wartościami. Krok 3 mówi „odpal
silnik, 10 s bez ruchu”:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 3 --after 10 --id 0F6
```

```
==============================================================================
marker 3, line 4078, at 416.094 s
  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
  window: 3.0 s before, 10.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 1: 88 90 91 89 8A 8E
      bits: 1.4 0->1 x2 @+3.30s, 1.3 1->0 x2 @+3.30s, 1.2 0->1 x1 @+5.02s, 1.1 0->1 x1 @+4.03s, 1.0 0->1 x2 @+3.46s
    byte 8: 20 24 64
      bits: 8.6 0->1 x1 @+6.12s, 8.2 0->1 x1 @+4.07s
```

Tutaj lista bitów jest myląca, ale **wartości bajtu** mówią wszystko: `88` przed, potem
`90 91 89 8A`, gdy kręci rozrusznik, potem `8E` przy pracującym silniku. Z większej liczby nagrań
wyłączenia zapłonu, włączenia i odpalenia auto wzorcowe daje `80` wyłączony, `88` włączony,
`90`/`91`/`89`/`8A` rozruch, `8E` silnik pracuje, `86`/`84` wyłączanie (zmierzone: nagrania
z krokami zapłonu i silnika, `508.dbc`).

Bajt 8 też się zmienił (`20 24 64`). Co znaczą te dwa bity, nie wiadomo. Nie przypisuj bitowi
znaczenia tylko dlatego, że zmienił się w tym samym czasie.

Przy pierwszym włączeniu nie ma okna „przed”: magistrala spała i każde ID jest przy znaczniku
„nowe” (zob. [Nagrywanie](record.md#how-markers-get-their-time)). Wtedy czytaj pierwsze ramki
bezpośrednio. Pierwsza ramka `0F6` tego nagrania to:

```
(398.212947) can0 0F6#884D1E2182585828
```

Bajt 1 = `88`: zapłon włączony.

## Przykład 3: przycisk przy kierownicy (`21F`) {#example-3-a-steering-wheel-button-21f}

Czysty przypadek. Krok 3 nagrania `can_20261111_190817.log` mówi „kierownica: głośność + trzy
razy, co 2 s”. Tym razem bez `--id`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_190817.log --marker 3 --after 4.5
```

```
firmware/test_data/captures/can_20261111_190817.log: 30040 frames, 62 IDs, 14 markers, 0 broken lines skipped
counters (never reported): 0E6:8H,8L 10B:5L 236:5 3F6:3H
==============================================================================
marker 3, line 6809, at 223.999 s
  # KROK 3 19:08:54.163 Kierownica: glosnosc + trzy razy, co 2 s
  window: 3.0 s before, 4.5 s after; ignored: 10 background bits, counters in 4 IDs
  21F
    byte 1: 00 08 00 08 00
      bits: 1.3 0->1 x4 @+0.78s
  new IDs (absent before, present after): 1A5(4)
```

Jeden bit: `21F` 1.3 (bajt 1 = `08`, gdy przycisk jest wciśnięty). Okno 4,5 s obejmuje dwa z trzech
wciśnięć: 4 zmiany. Następny znacznik jest 4,5 s później, więc dłuższe okno wmieszałoby następny
krok.

Nowe ID `1A5` nie pochodzi od przycisku. W tym nagraniu była podłączona skrzynka CAN z zestawu
radia, a ona wysyła `1A5` (poziom głośności) kilka milisekund po tym, jak radio zmieni głośność
(zmierzone: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md)). Inne
urządzenia na magistrali też reagują na twoją czynność. Jeśli możesz, nagraj raz bez nich.

Wynik: głośność w górę = `21F` 1.3 (zmierzone: `can_20261111_190817.log`
i `can_20261111_191045.log` razem z linią UART do radia).

## Przykład 4: czujniki parkowania (`0E1`) {#example-4-parking-sensors-0e1}

Ramka pełna pól. Krok 18 nagrania `can_20261111_191045.log` mówi „czujniki tył: wsteczny
włączony, zbliżaj przedmiot do tylnego zderzaka aż do tonu ciągłego, zabierz, wyłącz wsteczny”.
Ten plik zaczyna się od kroku 13, więc krok 18 to znacznik 6 (sprawdź przez `--list`):

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_191045.log --marker 6 --after 25 --id 0E1
```

```
==============================================================================
marker 6, line 18775, at 421.532 s
  # KROK 18 19:12:11.696 Czujniki TYL: wsteczny wlaczony, zblizaj przedmiot do tylnego zderzaka az do tonu ciaglego, zabierz, wylacz wsteczny
  window: 3.0 s before, 25.0 s after; ignored: 2 background bits, counters in 0 IDs
  0E1
    byte 1: B0 B4 90 B0
      bits: 1.2 0->1 x2 @+2.53s
    byte 2: 00 D0 00 D0 50 D0 00
      bits: 2.7 0->1 x6 @+15.31s, 2.6 0->1 x4 @+15.31s, 2.4 0->1 x4 @+15.31s
    byte 3: 3F 30 2B 3F 2E 28 25 22 ...
      bits: 3.5 1->0 x2 @+16.86s, 3.4 1->0 x6 @+15.51s, 3.3 1->0 x10 @+15.31s, 3.2 1->0 x10 @+15.31s, 3.1 1->0 x14 @+15.31s, 3.0 1->0 x12 @+15.31s
    byte 4: FC F8 F4 F8 F4 F0 EC E8 ...
      bits: 4.7 1->0 x2 @+17.37s, 4.6 1->0 x2 @+17.75s, 4.5 1->0 x4 @+17.37s, 4.4 1->0 x2 @+16.86s, 4.3 1->0 x6 @+15.51s, 4.2 1->0 x10 @+15.31s
    byte 5: FC 5C 3C 1C FC
      bits: 5.7 1->0 x2 @+17.56s, 5.6 1->0 x2 @+17.82s, 5.5 1->0 x4 @+17.56s
```

Gdy wiele sąsiednich bitów zmienia się razem, tworzą jedno pole. Podziel bajty na grupy bitów,
które poruszają się razem:

| Pole | Gdzie | Co robią tu wartości | Znaczenie w aucie wzorcowym |
|---|---|---|---|
| stan tyłu, stan przodu | bajt 1, bity 7-5 i 4-2 | `90` = `100 100 00`: tył 4, przód 4 | 4 aktywne, 5 oczekiwanie (zmierzone) |
| żądanie dźwięku | 2.4 | 1, gdy auto pika | zmierzone |
| strony | 2.7-2.6 | `D0`: obie strony | zmierzone |
| odstęp piknięć | bajt 3, bity 5-0 | `3F`, potem coraz mniejsze wartości, gdy przedmiot się zbliża | `3F` bez dźwięku, `39` ... `0B` piknięcia, `00` ton ciągły (zmierzone) |
| strefy czujników | bajty 4-6, po 3 bity | wartości maleją, gdy przedmiot się zbliża | 0 najbliżej ... 7 daleko (zmierzone) |

Znaczenia: `vehicles/peugeot-508-sw-2015/508.dbc` oraz
[Peugeot 508 SW](../reference/vehicles/peugeot-508.md#parking-sensors-0e1).

Bit 1.5 też się zmienił (`B4` na `90`), ale nie ma go na liście: ruszał się już w oknie „przed”,
które zachodzi na koniec kroku ze wstecznym. Filtr tła może ukryć prawdziwy bit. Jeśli spodziewasz
się bitu i go nie widzisz, przesuń okno: z `--before 1` ten przebieg pokazuje też `1.5 1->0 x2`.
Wartości bajtów (`B0 B4 90 B0`) są zawsze pokazywane w całości.

## Powtórz czynność, policz trafienia {#repeat-the-action-count-the-hits}

Najlepszy dowód to bit, który zmienia się przy **każdym** znaczniku tej samej czynności i przy
żadnym innym. Gdy każde powtórzenie ma swój znacznik, `--match` i `--summary` policzą to za
ciebie. Na pliku syntetycznym (wymyślone auto, trzy otwarcia drzwi i trzy wciśnięcia przycisku):

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --match door --summary
```

```
firmware/test_data/synthetic/marker_demo.log: 3216 frames, 5 IDs, 6 markers, 4 broken lines skipped
counters (never reported): 100:2,8L
==============================================================================
summary of 3 markers: 1 3 5
  3/3  300 2.5
```

`3/3  300 2.5` = w ramce `300` bit 2.5 (bajt 2, bit 5) zmienił się przy wszystkich trzech
znacznikach drzwi. Z `--match volume` odpowiedź to
`3/3  310 1.3`. 4 uszkodzone linie są celowe: narzędzie pomija linie, które wyglądają na ramki,
ale są zepsute (nieparzysta liczba cyfr szesnastkowych, więcej niż 8 bajtów, 4-cyfrowe ID).

## Jak często przychodzi ramka {#how-often-a-frame-comes}

Część sygnałów wymaga ramki, która powtarza się dość często (zobacz
[Lista kontrolna przeniesienia](porting-checklist.md)). Narzędzie nie pokazuje okresu. Odczytaj go
z pliku:

1. Otwórz nagranie w edytorze tekstu.
2. Wyszukaj `can0 0F6#` (zamiast `0F6` wpisz swoje ID).
3. Odejmij czasy dwóch kolejnych linii. W `can_20261111_191045.log`:
   336.199989 - 335.699989 = 0,5 s.
4. Sprawdź kilka par. Ramka wysyłana tylko przy zmianie ma długie, nieregularne przerwy.

W Git Bash `grep "can0 0F6#" <plik> | head -5` wypisze pierwsze pięć linii jednego ID.

## Od bitów do wartości {#from-bits-to-values}

Pole staje się liczbą dzięki **skali** i **przesunięciu**:

```
value = raw × scale + offset
```

Przykład z auta wzorcowego: temperatura zewnętrzna = `0F6` bajt 7, skala 0,5, przesunięcie
-40, czyli `value / 2 - 40` °C (zmierzone: zgodne z temperaturą, którą skrzynka CAN wysłała
radiu, w 21 z 21 ramek, `can_20261111_191045.log`). W ramce `0F6#884D1E2182585828` powyżej bajt 7
= `58` = 88, a 88 / 2 - 40 = 4 °C.

Żeby znaleźć skalę, nagraj wartość w dwóch znanych punktach (na przykład temperaturę pokazaną na
desce rozdzielczej albo głośność radia) i porównaj.

### Zapisz to w pliku DBC {#write-it-into-a-dbc-file}

Plik DBC to plik tekstowy z listą ramek i sygnałów. Czyta go wiele narzędzi CAN. Plik auta
wzorcowego to `vehicles/peugeot-508-sw-2015/508.dbc`. Jedna linia na sygnał:

```
BO_ 246 BODY_STATE: 8 CAR
 SG_ reverse : 63|1@0+ (1,0) [0|1] "" DEVICE
```

- `246` to ID dziesiętnie (`0F6` hex = 246).
- `63|1@0+`: bit startowy 63, długość 1 bit, `@0` = kolejność bajtów big-endian (zwana też
  Motorola; wybierz ją w SavvyCAN albo cantools), `+` = bez znaku.
  Przy `@0` bit startowy = (bajt - 1) × 8 + bit najstarszego bitu pola: bajt 8 bit 7 =
  7 × 8 + 7 = 63. Odstęp piknięć, bajt 3 bity 5-0, to `21|6@0+`.
- `(1,0)` = skala i przesunięcie; temperatura zewnętrzna ma `(0.5,-40)`.

Każdemu sygnałowi daj komentarz, który zaczyna się od jego statusu (`measured: ...` z nagraniem
albo `unverified: ...` ze źródłem), tak jak w `508.dbc`. Potem zamień sygnały na profil auta:
[Pisanie profilu auta](write-a-profile.md).

## SavvyCAN i inne narzędzia {#savvycan-and-other-tools}

SavvyCAN to darmowy program na komputer do analizy CAN, który czyta pliki DBC i pliki logów
w kilku formatach. Ten projekt **nie ma z nim połączenia na żywo**: program urządzenia nie ma
protokołu GVRET ani podobnego. Pracujesz na nagranych plikach. Czy twoja wersja SavvyCAN otworzy
format nagrań tego projektu bezpośrednio, nie sprawdzono; jeśli odrzuci linie komentarzy albo
linie `uartA`, najpierw wyeksportuj same linie CAN:

```
python -I firmware/tools/can_diff.py my_recording.log --export frames_only.log
```

## Ograniczenia narzędzia {#limits-of-the-tool}

- Znacznik nie ma własnego czasu: dostaje czas następnej ramki CAN. Na śpiącej magistrali to
  chwila, w której magistrala się budzi.
- Sygnał, który zmienia się wolno (temperatura, paliwo), może nie zmienić się w oknie.
- Bit, który ruszał się już przed znacznikiem, jest ukryty (zob. przykład 4).
- Filtr liczników szuka równych kroków. Licznik, który skacze, albo suma kontrolna nie zostanie
  rozpoznany i pokaże się jako ruchliwy bit w każdym oknie.
- Czytane są tylko ID 11-bitowe (trzy cyfry szesnastkowe).

Czego nie wiesz po dekodowaniu: co auto robi z wartością, której zmiany nie widziałeś. Sygnał,
który znalazłeś, jest zmierzony dla czynności, które nagrałeś, i nic ponadto.

Dalej: [Pisanie profilu auta](write-a-profile.md).
