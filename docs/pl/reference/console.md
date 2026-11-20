# Konsola USB

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona mówi, jak podłączyć się do konsoli tekstowej urządzenia i co w niej wpisywać.
Najczęściej potrzebne polecenia to `stat` (stan urządzenia), `vol N` (głośność pikania) i `test`
(próbne piknięcie). Panel WWW ma kopię tej samej konsoli (zakładka Konsola); część poleceń działa
tylko przez USB, zobacz [Skąd może przyjść polecenie](#where-a-command-may-come-from).

## Połączenie z konsolą {#connect-to-the-console}

Potrzebujesz uv i kodu repozytorium, zobacz [Wgraj program](../quick-start/flash.md).

1. Podłącz urządzenie do komputera kablem USB. Użyj natywnego portu USB modułu ESP32-S3: na
   płytkach z dwoma gniazdami jest często opisany `USB`, a nie `UART`.
2. Otwórz terminal w głównym katalogu repozytorium (w Windows: PowerShell albo Git Bash).
3. Uruchom monitor. Prędkość, 115200 bodów, pochodzi z `platformio.ini`:

    ```bash
    uvx platformio device monitor -d firmware
    ```

4. Naciśnij Enter, wpisz `ver` i naciśnij Enter. Oczekiwane: linia zaczynająca się od `[ver]`, na
   przykład `[ver] <product> 2.0.0-beta.1 listen, built Nov 18 2026 01:33:28`.
5. Monitor zamykasz klawiszami Ctrl+C.

Jeśli monitor nie może otworzyć portu, używa go inny program (drugi monitor, wgrywanie). Zamknij
ten program i spróbuj ponownie. Jeśli żaden port się nie pojawia, wypisz porty poleceniem
`uvx platformio device list` i sprawdź kabel.

Urządzenie przyjmuje na końcu linii `\r`, `\n` albo oba. Linia polecenia może mieć najwyżej
127 znaków. Dłuższy tekst jest obcinany.

> [!NOTE]
> **Bez komputera, bez czekania**
>
> Urządzenie nie czeka na komputer. Gdy nic nie czyta portu USB, jego tekst po prostu przepada.
> Program nigdy nie zatrzymuje się z powodu konsoli.

## Co urządzenie wypisuje przy starcie {#what-the-device-prints-at-start}

Zaraz po starcie urządzenie wypisuje kilka linii komentarza, na przykład (program bez nadawania na
CAN):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0-beta.1, built Nov 18 2026 01:33:28, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: power on
```

- Pierwsza linia podaje prędkość CAN z profilu auta i tryb pracy
  (`alongside the box (sends nothing)` albo `DECODER (sends to the radio, drives ACC)`).
- Jeden tryb, trzy zapisy: konsola wypisuje `alongside the box`, panel mówi „obok skrzynki”,
  a `ver` wypisuje `listen`. Ta strona nazywa go „obok skrzynki”.
- W programie, który umie nadawać (środowisko `decoder_tx_wifi`), pierwsza linia mówi
  `TRANSMIT 15B: built, armed=0` albo `armed=1`, a druga linia mówi, czy kontroler CAN pracuje
  w trybie tylko nasłuchu, czy w trybie normalnym. Zobacz [Nadawanie na magistralę CAN](../safety/transmit.md).
- `can=FAIL`, `audio=FAIL` albo `radio=FAIL` oznacza, że sterownik nie wystartował. Urządzenie
  zostaje wtedy w stanie bezpiecznym: wyjścia 12 V wyłączone, nic nie jest wysyłane, czerwona dioda.
  Zobacz [Rozwiązywanie problemów](../troubleshooting/index.md#the-device-reports-fail-at-start).
- Inne możliwe linie: `# settings unavailable (NVS): defaults, changes will not be stored`,
  `# stored key swap table is invalid: the fixed entries apply`,
  `# CAN transmit arming stored by another program: cleared`.

Części `<project name>` i `<product>` pochodzą z `project.json` i zmienią się, gdy projekt dostanie
ostateczną nazwę.

## Polecenia {#commands}

Nazwy poleceń są angielskie. Część starych nazw nadal działa jako aliasy, zobacz
[Stare nazwy poleceń](#old-command-names).

### Stan i informacje {#state-and-information}

| Polecenie | Co robi |
|---|---|
| `stat` | Tryb, stan sterowników, liczniki CAN, zapłon, ACC, wsteczny, światła, ostatnia ramka czujników parkowania i brama nadawania CAN. Szczegóły niżej. |
| `radio` | Co przyszło z radia przez UART: liczniki ramek, błędy, ostatnia głośność, czas i źródło. |
| `ver` | Produkt, wersja, tryb i czas kompilacji, na przykład `[ver] <product> 2.0.0-beta.1 listen, built Nov 18 2026 01:33:28`. |
| `help` | Lista poleceń. Każde nieznane polecenie wypisuje `Unknown command: ...` i tę samą listę. |

`stat` wypisuje linie takie jak te:

```text
[stat] mode=alongside the box can=ok audio=ok radio=ok
[stat] rx=160131 lost=0 errors=0 bus=active volume=40% supply=12.43V (after the diode)
[stat] ignition=1 (0x8E) acc=1 reverse=0 lights=1
[park] rear=5 front=4 sound=1 front interval=0x18 zones rear 777 front 537
[stat] can-tx=none sent=0 acked=0 failed=0 refused=0
```

- `rx` = odebrane ramki, `lost` = ramki zgubione, bo kolejka odbiorcza była pełna, `errors` =
  błędy magistrali widziane przez kontroler CAN, `bus` = `active`, gdy w ostatniej sekundzie
  przyszła ramka.
- `supply` jest tylko na płytce z dzielnikiem napięcia. Mierzone jest **za** diodą wejściową, więc
  pokazuje ok. 0,8 V (0,7-0,9 V) mniej niż akumulator (spadku na diodzie nie zmierzono dokładnie). Poniżej 6 V
  linia mówi `supply=no 12V (x.xxV)`: płytka jest zasilana tylko z USB.
- `ignition` pokazuje w nawiasie surowy bajt. W aucie wzorcowym `0x80` = wyłączony, `0x88` =
  włączony, `0x8E` = silnik pracuje (zmierzone (measured): nagrania CAN
  w `firmware/test_data/captures/`).
- W programie bez nadawania ostatnia linia to zawsze `can-tx=none` z zerami.

`radio` wypisuje między innymi `[radio] dropped without ACC=N no slot for a state frame=N too long to frame=N`.
Pierwszy licznik rośnie przy normalnym używaniu (przyciski i czujniki, gdy radio jest wyłączone).
Drugi i trzeci powinny zostać na 0 (trzeci liczy ramki, których protokół radia nie zmieścił w buforze).

### Dźwięk {#sound}

| Polecenie | Co robi |
|---|---|
| `vol N` | Głośność pikania, `N` = 0 do 100 procent. Urządzenie ogranicza ją do swojej górnej granicy (80 %, `AUDIO_VOLUME_MAX` w `firmware/src/config.h`). |
| `test` | 1,5 s próbnego pikania, bez auta. |

Głośność poniżej 15 % działa tylko do następnego restartu. Po restarcie głośność wynosi co
najmniej 15 % (`AUDIO_VOLUME_MIN_SAVED`). Powód: w trybie dekoder głośnik urządzenia może być
jedynym dźwiękiem czujników parkowania w aucie, więc jedno `vol 0` nie może go wyciszyć na stałe.

### Przyciski i radio {#buttons-and-the-radio}

Te polecenia sprawdzają, co radio robi z kodem przycisku. Wymagają trybu dekoder: w trybie obok
skrzynki urządzenie nic nie wysyła do radia, a odpowiedź kończy się na
`: not sent (listen mode)`.

| Polecenie | Co robi |
|---|---|
| `key CODE` | Wysyła do radia kod przycisku `CODE` (szesnastkowo, na przykład `key A5`) jako „wciśnięty”, a potem „puszczony”. |
| `kd CODE` | Wysyła tylko „wciśnięty”. |
| `ku CODE` | Wysyła tylko „puszczony”. Razem z `kd` sprawdza przyciski, które radio traktuje inaczej, gdy są przytrzymane. |
| `keys` | Włącza albo wyłącza wypisywanie zdarzeń przycisków. Każde wywołanie przełącza: `[key] printing=1`, potem `[key] printing=0`. |
| `map` | Wypisuje tabelę zamiany przycisków: `[key] swap 9F -> 9B` (przycisk auta z kodem `9F` wysyła do radia `9B`). |
| `map FROM TO` | Dodaje zamianę, oba kody szesnastkowo: `map 9B 1F`. `map 9B 9B` ją usuwa. Zapisywane we flash. Najwyżej 16 pozycji (`[key] swap table full`). |
| `map reset` | Powrót do domyślnych zamian profilu auta (w `generic` brak). Zapisywane we flash. |
| `rxlog` | Włącza albo wyłącza wypisywanie ramek od radia (`[radio_rx] ...`). |
| `doorswap` | Zamienia lewe i prawe drzwi w ramce dla radia, do następnego restartu. |

Kody i to, co robią w radiu wzorcowym, są na stronach [Protokół XP](xp-protocol.md) i
[API panelu](panel-api.md#10-tables-for-the-panel).

### Język panelu i kopia ustawień {#panel-language-and-settings-backup}

| Polecenie | Co robi |
|---|---|
| `lang` | Pokazuje zapisany język panelu i języki wbudowane w ten obraz: `[lang] panel language=pl (stored in the device); available: de,en,es,pl,ru,uk`. |
| `lang CODE` | Zapisuje język panelu, na przykład `lang pl`. Tylko kod z listy `available`. Zapisywane we flash. |
| `lang default` | Zapomina zapisany język: panel otwiera się po angielsku. |
| `settings` | Wypisuje kopię ustawień jako jedną linię JSON bez haseł: `[settings] {"v":1,"lang":"pl","vol":40,...}`. |
| `settings import JSON` | Wczytuje dowolny podzbiór pól kopii, na przykład `settings import {"vol":40}`. Najpierw wszystko jest sprawdzane; błędne pole niczego nie zmienia. `net` i `lan` nigdy nie są wczytywane. Zapisywane we flash. |

Pola, ograniczenia i przykłady: [Ustawienia](../../en/reference/settings.md#backup-and-restore) (po angielsku).

### Ramki CAN {#can-frames}

| Polecenie | Co robi |
|---|---|
| `dump` | Włącza albo wyłącza zrzut surowych ramek (`# CAN frame dump=1`). Ramki idą **tylko na USB**, nie do panelu: przy ok. 218 ramkach na sekundę w aucie wzorcowym (zmierzone) wypchnęłyby z panelu wszystko inne. |
| `ID#DATA` | Ramka wpisana ręcznie, na przykład `0E1#B0F000FCFC1EC200`. Cała linia sniffera `(12.345) can0 0E1#B0F0...` też działa. |

> [!NOTE]
> **Wpisana ramka nigdy nie trafia do auta**
>
> Wpisana ramka trafia tylko do wewnętrznej logiki urządzenia, tak jakby przyszła z auta. Nigdy
> nie jest nadawana na magistralę CAN. Używaj jej do sprawdzania dekodowania na stole.
>
> Ramka wpisana w panelu WWW nie liczy się jako ruch na magistrali: nie utrzymuje urządzenia ani
> jego Wi-Fi w stanie czuwania.

### Ustawienia pojazdu (tylko odczyt) {#vehicle-settings-read-only}

| Polecenie | Co robi |
|---|---|
| `vset` | Wypisuje tabelę ustawień pojazdu: ostatnie ramki `260` i `361`, wiek `260`, maskę, ramkę `260` widzianą przy starcie i bity zmienione od tamtej chwili, potem po jednym wierszu na pozycję ekranu radia (numer komendy radia `80`, nazwa, stan odczytany z `260`, podstawa, maska, wynik logiki ramki `15B`). |

`vset` tylko czyta. Niczego nie nadaje. Co znaczą pozycje, opisuje
[Peugeot 508 SW](vehicles/peugeot-508.md).

### Nadawanie na CAN {#can-transmission}

Te polecenia mają znaczenie tylko w programie zbudowanym ze środowiskiem `decoder_tx_wifi`.
W każdym innym programie odpowiadają `[cantx] built=0: no CAN transmit in this build (CAN listen only)`
albo `[cantx] REFUSED: this program cannot transmit on CAN`.

> [!CAUTION]
> **Nadaje na magistralę auta**
>
> `cantx on` uzbraja program, który umie wysyłać ramki do auta. Przeczytaj
> [Nadawanie na magistralę CAN](../safety/transmit.md), zanim użyjesz któregokolwiek polecenia z tej tabeli.
> Kroki włączania są w [Włącz nadawanie krok po kroku](../safety/transmit.md#switch-transmitting-on-step-by-step).

| Polecenie | Co robi | Skąd wolno |
|---|---|---|
| `cantx` | Stan: zbudowane czy nie, uzbrojone teraz, uzbrojone po restarcie, tryb kontrolera, maska, stan bramy, liczniki, TEC / REC (liczniki błędów nadawania i odbioru), przyczyna blokady. | zewsząd |
| `cantx on` | Zapisuje uzbrojenie. Działa **po restarcie** urządzenia i tylko dla tego konkretnego programu (zapisywany jest napis kompilacji). Wgranie dowolnego innego programu rozbraja. | USB, panel w sieci urządzenia |
| `cantx off` | Rozbraja od razu i to zapisuje. Kontroler wraca do trybu tylko nasłuchu przy następnym starcie. | USB, panel w sieci urządzenia |
| `cantx reset` | Zdejmuje blokadę bramy nadawania po błędzie. | tylko USB |
| `vset on NN`, `vset off NN` | Włącza albo wyłącza pozycję `NN` (szesnastkowo) w masce ustawień. Zapisywane we flash. | USB, panel w sieci urządzenia |
| `set NN VV` | To samo co komenda radia `80 NN VV`, do prób bez radia. Te same sprawdzenia, ta sama brama. | tylko USB |

Uwagi:

- `vset on NN` przyjmuje tylko pozycje ze znaną ramką: `01`, `02`, `06`, `07`, `09`, `0F`
  (ramka `15B`) oraz `05`, `0C` (ramka `1A9`). Inne pozycje odpowiadają
  `[vset] item NN cannot be enabled: fixed refusal in the program (no measurement or no frame)`.
- Domyślna wersja wysyła tylko ramkę `15B`, sprawdzaną wyłącznie po ID i długości. Ramka `1A9`
  jest na liście dozwolonych tylko w wersji zbudowanej z `-DCAN_TX_EXPERIMENTAL=1`; w domyślnej
  wersji brama ją odrzuca, nawet gdy pozycja jest w masce. Żadna wersja nic nie wysyła na polecenia
  klimatyzacji z radia (`8A`): wypisują tylko linię `[climate]`.
- Zmiany zapisujące flash (`cantx on`, maska) są pomijane, gdy auto potrzebuje urządzenia (bieg
  wsteczny albo aktywne czujniki parkowania): `[cantx] skipped: the car needs the device ...`.
  Spróbuj ponownie na postoju. `cantx off` zawsze działa od razu.
- Liczby `NN` i `VV` są szesnastkowe, jedna albo dwie cyfry, bez `0x`. Zła forma wypisuje
  `[cantx] bad syntax. Commands: cantx | cantx on | cantx off | cantx reset | vset on NN | vset off NN | set NN VV`.

### Wi-Fi (tylko USB) {#wi-fi-usb-only}

Te polecenia istnieją tylko w programach z siecią (`listen_wifi`, `decoder_wifi`,
`decoder_tx_wifi`, `listen_generic_wifi`). Działają **tylko w konsoli USB**: część z nich pokazuje
albo ustawia hasła, a USB oznacza, że ktoś trzyma urządzenie w rękach. Z panelu odpowiedź to
`[wifi] USB console only` (sam panel odrzuca je wcześniej).

| Polecenie | Co robi |
|---|---|
| `wifi` | Stan: nazwa i adres sieci urządzenia, podłączeni klienci, sieć domowa i jej adres, nazwa `.local`. Bez hasła. |
| `wifi showpass` | Wypisuje hasło urządzenia (klucz jego sieci Wi-Fi i hasło logowania do panelu). Wypisywane tylko na USB, nigdy do panelu. |
| `wifi pass NEW` | Ustawia nowe hasło urządzenia: 10 do 63 drukowalnych znaków, bez spacji. Sieć startuje od nowa. |
| `wifi newpass` | Losuje nowe hasło. Stare przestaje działać na wszystkich telefonach i komputerach. Pokaż je przez `wifi showpass`. |
| `wifi sta NAME|PASSWORD` | Zapisuje sieć domową. Linia jest dzielona na **pierwszym** `|`. Hasło jest wymagane, 8 do 63 znaków albo 64 cyfry szesnastkowe: sieci otwarte nie są obsługiwane. |
| `wifi sta off` | Zapomina sieć domową. |
| `wifi lan on`, `wifi lan off` | Pozwala na zmiany z sieci domowej albo je blokuje (domyślnie: zablokowane, panel jest tam tylko do odczytu). Zapisywane. |
| `wifi on`, `wifi off` | Włącza albo wyłącza sieć. Zapisywane na kolejne starty. `wifi on` zeruje też liczniki nieudanych startów i restartów po awarii. |

Przykład: `wifi sta MyHome|secret-pass-123`. Kreska pionowa `|` jest częścią polecenia: oddziela
nazwę sieci od jej hasła.

> [!WARNING]
> **Nie zostawiaj hasła we wspólnych logach**
>
> Nie zostawiaj linii `[wifi] password:` w logu, który publikujesz albo komuś wysyłasz.

W czasie wgrywania programu `wifi off`, `wifi pass`, `wifi sta` i `wifi newpass` zapisują
ustawienie, ale Wi-Fi zostaje bez zmian do końca wgrywania.

## Skąd może przyjść polecenie {#where-a-command-may-come-from}

| Źródło | Dozwolone |
|---|---|
| konsola USB | wszystko |
| panel WWW, telefon w **sieci urządzenia** | wszystko oprócz `wifi ...`, `cantx reset` i `set NN VV` |
| panel WWW przez **sieć domową** | polecenia odczytu; zmiany dopiero po `wifi lan on` na USB; `cantx on/off` i `vset on/off NN` nigdy |

Gdy włączony jest bieg wsteczny albo działają czujniki parkowania:

- panel odrzuca `test`, `vol N`, `map FROM TO`, `map reset`, `lang CODE` i `settings import`
  (`[www] skipped: the car needs the device`);
- USB wykonuje `test`, `vol N`, `map FROM TO` i `map reset` od razu, a zapis do flash robi
  później. Mówi o tym linia `[audio] volume write deferred: the car needs the device`, a potem
  `[audio] deferred volume write 40%: done`. Tak samo dla tabeli przycisków (`[key] ...`);
- USB odrzuca `lang CODE` i `settings import` (`... try again shortly`).

Powód: zapis do flash zatrzymuje odbiór CAN na kilka milisekund, a próbne pikanie zagłuszyłoby
czujniki.

## Stare nazwy poleceń {#old-command-names}

Polecenia miały kiedyś polskie nazwy. Te nadal działają i robią dokładnie to samo:

| Stara nazwa | Obecna nazwa |
|---|---|
| `ust` | `vset` (także `ust on NN`, `ust off NN`) |
| `wifi haslo` | `wifi showpass` |
| `wifi nowehaslo` | `wifi newpass` |

Test `firmware/test_host/console_alias.cpp` sprawdza alias `ust`.

## Format linii {#the-line-format}

Ta część jest dla osób, które piszą narzędzia czytające konsolę. Format jest stały
(zobacz [Wersjonowanie](versioning.md)). Konsola wypisuje trzy rodzaje linii:

| Linia | Przykład | Znaczenie |
|---|---|---|
| ramka CAN | `(734.501) can0 0E1#B0F000FCFC1EC200` | surowa ramka, tylko po poleceniu `dump`. Czas w sekundach od startu urządzenia, ID w trzech cyfrach szesnastkowych, dane szesnastkowo |
| komentarz | `# version: ...` | zaczyna się od `#`. Komunikaty startowe, uwagi, błędy bibliotek ESP-IDF (`# idf: ...`) |
| linia ze znacznikiem | `[stat] rx=160131 lost=0 ...` | zaczyna się od znacznika w nawiasach kwadratowych, który mówi, która część programu się odzywa |

Znaczniki, które możesz spotkać: `[stat]`, `[radio]`, `[ver]`, `[park]`, `[car]`, `[key]`,
`[audio]`, `[vset]`, `[cantx]`, `[climate]`, `[sound]`, `[radio_rx]`, `[can]`, `[wifi]`, `[www]`,
`[ota]`, `[sleep]`.

Format nagrania z polecenia `dump` (`(s.mmm) can0 ID#DATA`) jest taki sam jak w nagraniach
w `firmware/test_data/captures/` i w pliku, który pobiera panel.

Każde polecenie i każdą linię na tej stronie sprawdzono z `firmware/src/console_cmds.cpp`,
`firmware/src/net/net.cpp` i `firmware/lib/core/can_tx_arm.cpp`. Jeśli ta strona i kod się różnią,
rację ma kod: zgłoś to (issue).

## Zobacz też {#see-also}

- [Przewodnik po panelu WWW](panel-guide.md): zakładka Konsola.
- [API panelu](panel-api.md): `POST /api/cmd` wysyła linię konsoli z panelu.
- [Konfiguracja i piny](config-and-pins.md): środowiska kompilacji.
- Coś nie działa: [Rozwiązywanie problemów](../troubleshooting/index.md).
