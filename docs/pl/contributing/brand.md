# Logo i ikona

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](translations.md#send-your-work).

Ta strona mówi, jakie pliki logo i ikony istnieją, gdzie używać każdego z nich i czego nie
zmieniać. Autor znaków: grabowski98. Licencja: **CC BY 4.0** (zobacz [Licencja](#licence)).

![Logo projektu: wyświetlacz z kropek](../../assets/brand/logo-transparent.svg)

## Znaki {#marks}

- **Logo główne:** wyświetlacz z kropek, który pokazuje nazwę projektu w trzech członach: „ESP”,
  „Can” i „Box”. Człon „Can” jest odwrócony: ciemny blok, a litery w kolorze szkła.
- **Ikona:** kostka złącza 2 × 4 piny z zatrzaskiem (2 × 8 w dużych rozmiarach). To osobny znak
  do małych rozmiarów: favicon, awatar, nagłówek panelu WWW urządzenia.

Logo i ikona to dwa znaki. Nie łącz ich w jedną poziomą sygnaturę.

## Kolory {#colours}

| Nazwa | HEX | Do czego |
|---|---|---|
| Szkło (podświetlenie) | `#B2C246` | tło wyświetlacza, litery w bloku „Can”, otwory ikony |
| Kropka | `#283020` | zapalone kropki, blok „Can”, obudowa ikony |
| Kropka niezapalona | `#A6B640` | siatka kropek w tle |
| Ramka | `#151617` | ramka modułu |
| Krawędź ramki | `#45494E` | cienki pierścień ramki |
| Jasne tło | `#F4F3EE` | jasne strony i grafiki |
| Ciemne tło | `#0E0F10` | ciemne grafiki |

- Kontrast kropki do szkła wynosi 7,0 : 1.
- Litery w bloku „Can” mają **dokładnie** kolor szkła. Trzeciego odcienia nie ma.
- Kolor szkła na białej stronie ma kontrast tylko 2,0 : 1. Nie używaj go do tekstu na jasnym
  tle; strona dokumentacji używa do linków ciemniejszej oliwki (`docs/assets/brand/site.css`).

## Geometria {#geometry}

- **Siatka:** jedna siatka kwadratowych kropek. Kropka ma 0,867 rastra, więc szczelina to 13 %.
  Wszystko leży na siatce.
- **Litery:** 5 × 7 kropek, w stylu zestawu znaków popularnych modułów LCD znakowych. 1 kropka
  między literami, 3 kropki między trzema członami nazwy.
- **Blok „Can”:** 1 kropka zapasu wokół liter.
- **Okno:** 61 × 13 kropek. Marginesy mają 2 kropki; lewy równa się prawemu, a górny dolnemu.
- **Ramka:** 2 rastry z każdej strony.
- **Ikona:** siatka 16 × 16 (2 × 4 piny) i siatka 32 × 32 (2 × 8 pinów), symetryczna względem
  osi pionowej, z równymi marginesami.

## Pole ochronne i minimalne rozmiary {#clear-space-and-minimum-sizes}

- **Pole ochronne:** 7 rastrów (wysokość wielkiej litery) wokół ramki. `logo.svg` i
  `logo-dark.svg` już je zawierają.
- **Logo na ekranie:** co najmniej **160 px** szerokości. Mniejszych rozmiarów nie sprawdzano.
  Najlepiej użyj SVG i szerokości, która daje całkowity raster kropki, wtedy każda kropka jest
  narysowana tak samo: `logo-transparent.svg` ma szerokość 65,87 rastra, więc 527 px daje raster
  8 px, a 329 px raster 5 px. Pliki README i ta strona używają tych szerokości
  (`docs/assets/brand/site.css`).
- **Ikona na ekranie:** wersja 2 × 4 od 16 px. Skaluj ją tylko o całkowitą wielokrotność 16
  (16, 32, 48, 64 ...). Pliki PNG o innych rozmiarach (180, 192) mają ikonę w największej
  całkowitej skali, wyśrodkowaną.
- **Nagłówek panelu WWW:** `panel-header-icon.svg` ma własną siatkę 24 × 24, więc jest ostry
  w 24 px i 48 px. Panel rysuje go w kolorze tekstu, z wyciętymi otworami pinów
  (`firmware/web/index.html`).
- **Sitodruk na płytce:** logo od **10 mm** szerokości, ikona 2 × 4 od 4 mm, ikona 2 × 8 od
  8 mm. Tabela rozmiarów elementów jest w `hardware/brand/README.md`.

## Czego nie robić {#donts}

- Nie zmieniaj kolorów znaków i nie dodawaj trzeciego odcienia w bloku „Can”.
- Nie rozciągaj logo i nie obracaj go. Nie dodawaj cieni, poświaty ani gradientów.
- Nie przesuwaj liter z siatki i nie zmieniaj odstępów.
- Nie skaluj ikony o współczynnik, który nie jest całkowitą wielokrotnością jej siatki: rozmywa
  się.
- Nie umieszczaj logo w kolorze na tle o jasności zbliżonej do szkła. Użyj tam `logo-dark` albo
  wersji jednokolorowej.

## Pliki {#files}

Wszystko jest w `docs/assets/brand/`, poza plikami sitodruku w `hardware/brand/`.

| Plik | Do czego |
|---|---|
| `logo.svg`, `logo-600.png`, `logo-1200.png` | logo na jasnym tle, z polem ochronnym |
| `logo-dark.svg`, `logo-dark-600.png`, `logo-dark-1200.png` | logo na ciemnym tle |
| `logo-transparent.svg`, `logo-transparent-600.png`, `logo-transparent-1200.png` | sam wyświetlacz, bez tła |
| `logo-mono.svg`, `logo-mono-white.svg` | jeden kolor: grawer, naklejki, druk |
| `icon.svg`, `icon-dark.svg`, `icon-mono.svg`, `icon-2x8.svg`, `icon-2x8-dark.svg` | ikona |
| `icon-32.png` ... `icon-512.png`, `icon-dark-32.png` ... `icon-dark-512.png`, `favicon.ico` | ikona w PNG i favicon (16, 32 i 48 px) |
| `panel-header-icon.svg` | ikona 24 px do nagłówka panelu WWW urządzenia |
| `readme-banner-light.png`, `readme-banner-dark.png` | baner rastrowy 1600 × 400 z wypełnionym tłem, do miejsc, które nie pokazują SVG; pliki README używają `logo-transparent.svg` |
| `github-social-preview.png`, `github-avatar.png` | podgląd repozytorium 1280 × 640 i awatar 500 × 500 |
| `espcanbox-intro.gif` | animacja 1280 × 360, 2,85 s: nazwa wpisywana litera po literze, na końcu zapala się blok „Can” |
| `site.css` | kolory strony dokumentacji, wzięte z tabeli powyżej |
| `hardware/brand/pcb-*.svg`, `hardware/brand/pcb-*.png` | sitodruk, jeden kolor; pliki PNG mają 1 bit i 1200 dpi |

Pliki SVG są wzorcami: same ścieżki, bez fontów i filtrów. Skryptu, który je rysuje, nie ma
jeszcze w repozytorium.

Nazwa projektu jest częścią rysunków: kropki logo, banery, podgląd, animacja, `<title>` w plikach
SVG i nazwa pliku `espcanbox-intro.gif`. `tools/set_project_name.py` ich nie przepisuje, a jego
`--check` ich nie sprawdza. Po zmianie nazwy projektu wymień te pliki ręcznie.

## Licencja {#licence}

Logo, ikona i wszystkie pliki wymienione powyżej są autorstwa grabowski98 i mają licencję
CC BY 4.0, tak jak dokumentacja (`LICENSE-docs`). Możesz ich używać i je przerabiać, jeśli
podasz autora. Nazwa projektu i znaki nie oznaczają, że ten projekt jest powiązany z jakimkolwiek
producentem aut lub radia.
