# Obudowa

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Dwuczęściowa obudowa z drukarki 3D na płytkę prototypu. Pliki są w
`hardware/prototype-v1/enclosure/`.

| Część | Stan |
|---|---|
| dolna część | wydrukowana, płytka pasuje |
| górna część | niesprawdzona jako wydruk (uszy na śruby wzmocniono po pierwszej wersji) |

Obrys płytki i otwory pochodzą z pliku PCB. Wysokości nad płytką i pod nią oraz grubość płytki
zmierzono suwmiarką na zbudowanym prototypie. Położenie gniazda USB i wycięć na przewody to wciąż
założenie: zobacz [listę poniżej](#measured-and-assumed-dimensions).

## Pliki {#files}

| Plik | Zawartość |
|---|---|
| `enclosure_bottom.stl` | dolna część: dno, słupki pod płytkę, ścianki do 5 mm nad płytką |
| `enclosure_top.stl` | górna część, zapisana **dachem w dół** (tak, jak się ją drukuje) |
| `preview.png` | szybki podgląd obu części |
| `gen_enclosure.py` | generator; wszystkie wymiary są na początku pliku |

Wymiary zewnętrzne: 107,8 x 84,8 x 45,0 mm, albo 121,8 x 98,8 mm z uszami na śruby.

## Jak jest zbudowana {#how-it-is-built}

- Płytka leży na trzech słupkach pod swoimi otworami M3 i na jednej podporze w rogu przy polu REV,
  z 14,5 mm miejsca pod płytką. Najwyższy kondensator pod płytką ma 13 mm.
- Obudowa dzieli się 5 mm nad płytką. Wypust 2 mm górnej części wchodzi w dolną i trzyma obie
  w osi.
- Wszystkie wycięcia leżą na linii podziału, więc przewody wkładasz od góry zamiast je
  przewlekać:
    - lewa ścianka przy J5 (CANH, CANL, GND, +12V): szerokość 22 mm;
    - lewa ścianka przy J4 (przewody do radia, też przewód głośnika): 37 mm;
    - ścianka od strony USB płytki ESP32: 27 x 12 mm, miejsce na wtyczkę z obudową.
- Cztery śruby w uszach na rogach łączą połówki. Każde ucho górnej części to pełny narożnik
  połączony z obiema ściankami na długości 14 mm i 14 mm w górę (zwykły pierścień miał za mały
  styk ze ścianką).

## Śruby {#screws}

| Gdzie | Śruba | Otwór w wydruku |
|---|---|---|
| płytka do słupków, 3 szt. | M3 x 6 do 8 mm | 2,8 mm, śruba sama nacina gwint |
| góra do dołu, 4 szt. | M3 x 10 do 12 mm, łeb do 6,5 mm | 2,8 mm w dolnej, 3,4 mm w górnej |

## Druk {#printing}

- Obie części bez podpór: dolna na dnie, górna na dachu (plik jest już obrócony).
- Warstwy 0,2 mm, 3 obrysy, wypełnienie 20 %.
- Górna część ma jeden most 27 mm nad wycięciem na USB.

> [!WARNING]
> **Nie drukuj z PLA**
>
> Użyj PETG albo ABS/ASA. PLA mięknie w aucie latem: we wnętrzu bywa ponad 60 °C.

## Wymiary zmierzone i założone {#measured-and-assumed-dimensions}

Grubość płytki: niecałe 2 mm (zmierzone, measured), w generatorze 2,0 mm.

| Parametr w `gen_enclosure.py` | Wartość | Status |
|---|---|---|
| `UNDER` | 14,5 mm | zmierzone: najwyższy kondensator pod płytką 13 mm (suwmiarka) |
| `ABOVE` | 24,5 mm | zmierzone: góra zacisku śrubowego 22 mm nad płytką (suwmiarka) |
| `USB_Z` | 5 do 17 mm nad płytką | założone: wysokość gniazd USB płytki ESP32 |
| `USB_X` | 70 do 97 mm od lewej krawędzi | założone: położenie gniazd USB wzdłuż krawędzi |
| `NOTCH_Z` | 1,5 do 9 mm nad płytką | założone: wysokość, na której przewody wychodzą z zacisków |
| `NOTCH_J4` | 36 do 73 mm od górnej krawędzi | założone: miejsce na gniazdo wiązki połączone z polami do radia |

Sprawdź też, czy zaciski J5 nie wystają więcej niż 1,5 mm poza krawędź płytki: tyle jest luzu do
ścianki.

Jeśli twoja płytka ESP32 albo twoje elementy są inne, zmień wartości na początku
`gen_enclosure.py` i wygeneruj pliki STL od nowa.

## Czego nie ma {#not-included}

- Mocowania w aucie (uchwyty, opaski).
- Miejsca na głośnik: głośnik zostaje na zewnątrz, na swoim przewodzie.
- Okienka na diodę stanu.

## Wygeneruj pliki STL {#generate-the-stl-files}

Generator potrzebuje Pythona z `manifold3d`, `numpy` i `matplotlib`. Z zainstalowanym
[uv](https://docs.astral.sh/uv/), w katalogu głównym repozytorium:

```
cd hardware/prototype-v1/enclosure
uv run --with manifold3d --with numpy --with matplotlib python -I gen_enclosure.py
```

Pliki STL w repozytorium powstały z tego skryptu.

Dalej: [Szukanie i sprawdzanie przewodów](wiring.md).
