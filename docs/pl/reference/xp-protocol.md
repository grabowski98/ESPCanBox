# Protokół radia XP

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Protokół „XP” to łącze szeregowe między skrzynką CAN z rynku wtórnego a radiem Android. Ta strona
opisuje go tak, jak został **nagrany** w aucie wzorcowym: skrzynka Simple Soft (RP5-PA-001) i radio
na platformie FYT, profil CAN w radiu „XP Simple Soft -> Peugeot 508 -> Type 2”.

> [!NOTE]
> **Bez powiązań**
>
> Ten projekt nie jest powiązany z Simple Soft, FYT, Junsun ani Peugeot. Nazwy służą tylko do
> tego, żeby powiedzieć, co było testowane.

## Skąd są fakty {#where-the-facts-come-from}

Wszystko na tej stronie jest **zmierzone** (measured), chyba że wiersz mówi inaczej. Obie linie
UART (skrzynka do radia i radio do skrzynki) oraz magistrala CAN auta zostały nagrane na jednym
zegarze rejestratorem z `extras/sniffer`. Oba nagrania są w `firmware/test_data/captures/`:

- `can_20261111_190817.log`: przyciski przy kierownicy, przycisk komputera pokładowego, drzwi,
  bagażnik;
- `can_20261111_191045.log`: drzwi, światła, silnik, bieg wsteczny, czujniki parkowania, kąt skrętu
  kierownicy, klimatyzacja, ekrany radia, ustawienia pojazdu z radia, przyciski panelu środkowego.

Dłuższa, wiążąca wersja tej strony jest w repozytorium: `radios/xp-simple-soft/protocol.md`. Źródło
każdej stałej protokołu w programie jest wymienione w `radios/xp-simple-soft/provenance.md`.
Polecenia, których nigdy nie widziano na linii, są tylko wymienione w sekcji
[Polecenia, które program tylko nazywa](#commands-the-firmware-only-names).

Jak urządzenie używa protokołu:

- w środowiskach `listen` i `listen_wifi` urządzenie tylko **słucha** linii radio -> skrzynka.
  Nic nie wysyła do radia;
- w środowiskach `decoder` urządzenie **zastępuje** skrzynkę i samo wysyła ramki skrzynka -> radio.
  Najpierw wypnij skrzynkę: dwa nadajniki na jednej linii UART zwierają się nawzajem.

## Numeracja bajtów {#byte-numbering}

Ramki UART są liczone przez `DATA[n]`: `DATA[0]` to pierwszy bajt danych po nagłówku i długości.
Bajty CAN na tej stronie są liczone od 1 (bajt 1 = pierwszy bajt danych), jak na każdej stronie o
CAN w tym serwisie. Zobacz [słowniczek](../contributing/glossary.md).

## Transport {#transport}

| Fakt | Status |
|---|---|
| UART 38400 bodów, 8 bitów danych, bez parzystości, 1 bit stopu (8N1) | zmierzone: każda nagrana ramka ma przy tej prędkości poprawną sumę kontrolną |
| poziomy logiczne 3,3 V | zmierzone |
| skrzynka -> radio i radio -> skrzynka na dwóch osobnych przewodach | zmierzone; piny w `vehicles/peugeot-508-sw-2015/wiring.md` |
| ramka: `2E CMD LEN DATA[LEN] CHK` | zmierzone |
| `CHK = ~(CMD + LEN + sum of DATA) & 0xFF` | zmierzone |
| po każdej ramce jeden bajt potwierdzenia: radio wysyła `FF`, skrzynka wysyła `F3` (raz przy starcie `F0`) | zmierzone |
| polecenia `01`-`7F` idą ze skrzynki do radia, `80`-`FF` z radia do skrzynki; wyjątek: `EA` od skrzynki | zmierzone |
| po włączeniu zasilania skrzynka wypisuje na swojej linii tekst ASCII (wersja, data kompilacji), potem zaczynają się ramki | zmierzone |

Przykład sumy kontrolnej: `2E 29 02 1C FF B9`. Dodaj `29 + 02 + 1C + FF = 146`, zostaw młodszy bajt
`46`, odwróć go: `B9`.

Odbiornik musi pomijać każdy bajt spoza ramki (tekst startowy, bajty potwierdzenia).

Czasy skrzynki (zmierzone): 5-25 ms od zmiany na CAN do ramki dla radia; 4-10 ms od polecenia radia
do ramki CAN.

## Skrzynka -> radio {#box---radio}

| CMD | Znaczenie | Treść w nagraniu |
|---|---|---|
| `20` | przycisk | `DATA[0]` = kod przycisku (tabela niżej). `DATA[1]` = `1` wciśnięty, `2` przytrzymany (powtórzenia), `0` puszczony. Ząbki pokrętła wysyłają tylko `1` |
| `21` | stan klimatyzacji | 9 bajtów. `DATA[2]`, `DATA[3]` = temperatura lewa / prawa w krokach 0,5 °C (`2D` = 22,5 °C; wartość z CAN `350` + `23`). `DATA[1]` bity 3-0 = poziom nawiewu. Bity trybu w `DATA[0]`, `DATA[1]`, `DATA[8]` zmieniają się tylko przy czynnościach na panelu auta. Przykład: `C4 E0 2D 2D 00 81 40 00 E0`. 26 z 27 ramek zgadza się z CAN `350` + `2AD` |
| `29` | kąt skrętu kierownicy | `DATA[0]` = CAN `10B` bajt 2, `DATA[1]` = bajt 1 (155 ze 155 ramek). Skala: niezweryfikowane (unverified) |
| `32` | czujniki parkowania | `DATA[0]` = stan (`01`, `02`; `00` z `DATA[1..6]` = `FF`, gdy nieaktywne; widziano też `03` + `FF`). `DATA[1..3]` tył lewy / środkowy / prawy, `DATA[4..6]` przód lewy / środkowy / prawy; `05` daleko, `00` najbliżej. Mniej więcej raz na sekundę plus przy zmianie |
| `33` | komputer pokładowy, bieżąca strona | `DATA[0..5]` = CAN `221` bajty 2-7; `DATA[6..8]` = `00` we wszystkich ramkach |
| `34` | trasa 1 | z CAN `2A1`: bajty 4, 5, `00`, 1, 2, 3 |
| `35` | trasa 2 | z CAN `261`, ten sam układ co `34` |
| `36` | temperatura zewnętrzna | bit 7 = minus, bity 6-0 = °C. Z CAN `0F6` bajt 7 (wartość / 2 - 40, połówki zaokrąglane w górę; 21 z 21 ramek) |
| `38` | drzwi i ustawienia pojazdu | 6 bajtów, zobacz [Peugeot 508 SW](vehicles/peugeot-508.md#xp-frame-38). Wysyłana przy zmianie i po każdym `81 01` od radia |
| `3C` | które wiersze ustawień pokazać | 2 bajty; nagrana jedna wartość: `9E 80` |
| `3B` | zapamiętane prędkości (ogranicznik, tempomat) | `FC 28 3B 4F 63 82 01` = 40, 59, 79, 99, 130 km/h; te same wartości są w CAN `1E8` |
| `3D` | lista prędkości | `32 46 50 5A 6E 82` = 50 ... 130 km/h, wysyłana dwa razy; to samo co CAN `2A7` / `267` |
| `3E` | przeszkoda | `80`, gdy czujniki są aktywne i któraś strefa jest poniżej 7, w przeciwnym razie `00` (19 z 19 ramek) |
| `44` | tryb ekonomiczny | `01` przy włączonym zapłonie i wyłączonym silniku, `00` przy pracującym silniku; z CAN `036` bajt 3 bit 7 |
| `71` | wersja protokołu | `0A 05 16 10 20 56 02 19 06`; radio pokazuje ją jako „XP_PSA_2016_10_20 V2256” |
| `30` | tekst wersji skrzynki | ASCII |
| `EA` | zasilanie | `01 0A` w ciągu 30 ms po tym, jak CAN `0F6` bajt 1 przestaje mieć wartość `80` (zapłon włączony); `00 0A` 2,5 s po powrocie do `80` |
| `01`, `37`, `39` | niezdekodowane | `01`: `00 00 00`, `00 00 10` po uruchomieniu silnika. `37`, `39`: zmieniają się przy niektórych komunikatach auta w CAN `1A1` |

Strefy czujników parkowania z CAN `0E1` (0 najbliżej ... 7 daleko) zamieniają się na wartości dla
radia tak: 0->0, 1->1, 2->1, 3->2, 4->2, 5->3, 6->3, 7->5 (zgadza się 394 z 396 ramek).

Po włączeniu zapłonu skrzynka wysyła tę grupę cztery razy, w odstępach około 1 s:
`38`, `EA 01 0A`, `71`, `01`, `3B`, `3C`, `30`, `3E 00`, `36`, `33`, `34`, `35`, `21`, `3D`,
`44 01`.

### Kody przycisków {#key-codes}

Polecenie `20`, `DATA[0]`. Wszystkie wiersze zmierzone (36 z 36 naciśnięć zgadzało się z ramkami
skrzynki).

| Kod | Przycisk | Źródło na CAN |
|---|---|---|
| `14` | kierownica: głośność + | `21F` bajt 1 = `08` |
| `15` | kierownica: głośność - | `21F` bajt 1 = `04` |
| `16` | kierownica: oba przyciski głośności (wyciszenie); także wciśnięte pokrętło zasilania na panelu | `21F` bajt 1 = `0C`; `036` bajt 7 = `02` |
| `17` / `18` | kierownica: następny / poprzedni | `21F` bajt 2 licznik w dół / w górę |
| `21` | kierownica: SRC | `0A2` bajt 2 = `04` |
| `50` | kierownica: LIST | `21F` bajt 1 = `01` |
| `07` | kierownica: wciśnięcie pokrętła | `21F` bajt 3 = `80` |
| `30` | kierownica: telefon (przytrzymany: powtarza `30 02`) | `21F` bajt 1 = `02` |
| `23` | dźwignia wycieraczek: komputer pokładowy | `221` bajt 1 bit 0 |
| `22` | przycisk panelu, nierozpoznany | `122` bajt 1 = `02` |
| `81` / `82` | pokrętło głośności na panelu w prawo / w lewo | `122` bajt 7 licznik w górę / w dół |
| `9E` | panel: NAV | `122` bajt 2 = `80` |
| `A0` | panel: MEDIA | `122` bajt 2 = `20` |
| `97` | panel: przewiń w lewo | `122` bajt 2 = `01` |
| `98` | panel: przewiń w prawo | `122` bajt 3 = `80` |
| `9C` | panel: lupa + | `122` bajt 3 = `40` |
| `9D` | panel: lupa - | `122` bajt 3 = `20` |
| `9F` | panel: TEL | `122` bajt 2 = `08` |
| `99` | panel: SETUP | `122` bajt 2 = `02` |
| `9B` | panel: DRIVE | `122` bajt 2 = `10` |
| `A4` | panel: wygaszenie ekranu (księżyc) | `122` bajt 2 = `40` |
| `A1` | panel: wyciszenie mikrofonu | `122` bajt 2 = `04` |
| `A3` | panel: WEB | `122` bajt 1 = `01` |

Skrzynka **nie** przekazywała radiu lewego pokrętła kierownicy (`0A2` bajt 4, licznik; wciśnięcie w bajcie 5) ani jego
wciśnięcia (zmierzone). Skrzynka wysyłała ząbki pokrętła około 220 ms po ramce CAN (zmierzone). Inna
skrzynka (Raise) nie miała takiego opóźnienia (obserwacja autora, bez nagrania).

> [!NOTE]
> **Ustawienia domyślne tego programu trochę się różnią**
>
> W trybie dekoder program wysyła zmierzone kody z tabeli wyżej, z kilkoma zamianami, które są
> wyborem tego projektu dla auta wzorcowego (na przykład `21` -> `1F`, `9F` -> `9B`, ustawione w jego
> profilu). Tabelę przycisków możesz zmienić
> z konsoli. Zobacz [Konsola](console.md).

## Radio -> skrzynka {#radio---box}

| CMD | Znaczenie | Treść w nagraniu | Skutek na CAN (skrzynka) |
|---|---|---|---|
| `A6` | data i godzina | rok - 2000, miesiąc, dzień, godzina, minuta; co sekundę | |
| `81` | stan radia | `01` włączone, `00` wyłączanie; 4 razy w odstępach około 1 s | skrzynka odpowiada na każde `81 01` ramkami `38` + `3C` |
| `90` | żądanie ramki | `90 71 00` przy każdym `81` (wersja); `90 21 ...` przy wejściu na ekran klimatyzacji; `90 33 00 00 00` przy wejściu na ekran komputera pokładowego | |
| `C4` | głośność | 0-36, `80` = wyciszenie | `1A5` = poziom, 4-10 ms później; wyciszenie: `165` bajt 1 = `E0` |
| `C0` | źródło / odtwarzanie | 8 bajtów: `01 01 ...` radio, `0C 30 ...` inne, `08 13 ...` odtwarzacz multimediów z danymi utworu | `165` bajt 3: `10` radio, `60` odtwarzacz, `40` inne |
| `CA` | wariant profilu | `00`, przy starcie, 4 razy | |
| `CB` | tekst na wyświetlacz auta | typ (`02`, `04`) + 32 bajty, w nagraniach pusty | |
| `80` | ustawienie pojazdu | numer, wartość | jedna `15B` około 6 ms później, zobacz [Peugeot 508 SW](vehicles/peugeot-508.md#personalisation-with-15b-and-260) |
| `82` | polecenie komputera pokładowego | `82 00 00 00 40 00 00` = przycisk „wyczyść” na ekranie trasy; `82 00 00 00 00 00 00` przy wejściu na ekran | |
| `8A` | polecenie ekranu klimatyzacji | `8A nn vv`, jedna ramka na naciśnięcie: `01` AUTO, `02` A/C, `03` MAX A/C, `04` / `05` temperatura lewa / prawa (`01` plus, `02` minus), `06` na twarz, `07` na szybę, `08` na nogi, `09` wiatrak, `0A` siła nawiewu (`01` plus, `02` minus), `0B` MONO, `0C` jednostka temperatury (z ekranu ustawień); nagrane ze znacznikami właściciela 19.11.2026 | brak: auto nie zareagowało, program nic nie wysyła |
| `84` | polecenie ekranu dźwięku | `84 nn vv`: `06` wysokie tony, `05` bas, `03` tłumienie, `04` balans, `09` wybór dźwięku, `02` dźwięk przestrzenny, `0A` głośność; wartość `00` / `01`, znaczenie nieustalone; nagrane ze znacznikami właściciela 19.11.2026 | brak |

Radio wysyła **wartość docelową** ustawienia, wyliczoną z własnego obrazu auta (z ramki `38`).
Jeśli `38` jest błędna, radio wysyła wciąż tę samą wartość.

## Polecenia, które program tylko nazywa {#commands-the-firmware-only-names}

Polecenia radia `84` (wiersze ekranu dźwięku w radiu) i `8A` (przyciski ekranu klimatyzacji w radiu)
nagrano 19.11.2026 ze znacznikiem właściciela przed każdym naciśnięciem
(`firmware/test_data/captures/centralka_konsola_20261119_150048.txt`; numery i nazwy są w
`radios/xp-simple-soft/provenance.md`). Program wypisuje je z nazwami i nic nie wysyła do auta.
`C2` (tuner) jest rozpoznawane i pomijane. Ramka `C5` nie jest rozpoznawana: widziano ją tylko z
bajtem długości niezgodnym z danymi, a jej znaczenia nigdy nie zmierzono.

## Nie wiadomo {#not-known}

- Który przycisk panelu wysyła kod `22`.
- Szczegóły poleceń tunera i telefonu oraz bajt wartości w `84`.
- Reguła dla pozostałych bitów `38` oraz znaczenie `01`, `37`, `39`.
- Które ramki CAN znikają po wyjęciu skrzynki: jedno krótkie nagranie bez skrzynki
  (`firmware/test_data/captures/centralka_can_20261117_184411.log`, około 10 s ramek) nie ma żadnej
  z ramek wysyłanych przez skrzynkę, ale nie powtarza tych samych czynności.
