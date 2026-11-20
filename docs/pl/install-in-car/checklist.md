# Lista kontrolna montażu

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona to jedna lista sprawdzeń przed montażem, w trakcie i po nim. Każdy punkt to sprawdzenie,
które zrobisz miernikiem, próbnikiem ciągłości („brzęczykiem”) i konsolą USB. Strony tej części
opisują każdy krok:

1. [Zasilanie i ACC](power-and-acc.md): skąd urządzenie bierze 12 V i co się dzieje, gdy auto
   stoi.
2. [Połączenie z CAN](can-connection.md): jak podłączyć się do magistrali auta, nie przeszkadzając
   jej.
3. [Połączenie z radiem](radio-connection.md): linie danych UART i wyjścia ACC, ILL i REV.
4. [Pierwsza jazda](first-drive.md): co sprawdzić w aucie i w jakiej kolejności.

Jeśli jeszcze tego nie zrobiłeś, najpierw przeczytaj [Bezpieczeństwo](../getting-started/safety.md).

> [!CAUTION]
> **Podczas instalacji urządzenie może tylko słuchać**
>
> Instaluj z wersją, która na magistrali CAN auta tylko słucha: `listen`, `listen_wifi`,
> `listen_generic_wifi`, `decoder` albo `decoder_wifi` (wersje dekodera rozmawiają z radiem,
> nigdy z autem). Nigdy nie instaluj pierwszy raz z `decoder_tx_wifi`. Na prototypie v1 zostaw
> zworkę JP1 otwartą: wtedy transceiver CAN nie może nadawać, bez względu na to, jaki program
> działa. Zobacz [Nadawanie na magistralę CAN](../safety/transmit.md).

> [!NOTE]
> **Jak otworzyć konsolę USB**
>
> Konsola USB jest potrzebna do większości sprawdzeń z tej listy.
>
> 1. Połącz urządzenie z komputerem kablem USB.
> 2. W folderze repozytorium uruchom:
>
>     ```bash
>     uvx platformio device monitor -d firmware
>     ```
>
>     Zobaczysz linie, które wypisuje urządzenie. Jeśli okno zostaje puste, urządzenie
>     wystartowało przed monitorem: wpisz `ver` i naciśnij Enter, odpowie jedną linią `[ver]`.
>
> 3. Wpisz polecenie, na przykład `stat`, i naciśnij Enter.
> 4. Monitor zamkniesz klawiszami Ctrl+C.
>
> Szczegóły: [Konsola USB](../reference/console.md#connect-to-the-console).

## Zanim pójdziesz do auta {#before-you-go-to-the-car}

1. Komputer z konsolą USB współpracuje z urządzeniem (ramka wyżej).
2. Urządzenie przeszło [test na stole](../quick-start/bench-test.md): `stat` pokazuje `can=ok
   audio=ok radio=ok`, a linia startowa mówi `LISTEN ONLY`.
3. Wiesz, która wersja jest wgrana: linia konsoli `# profile: car=... radio=...` i `ver`.
   Dla nieznanego auta użyj profilu `generic` (`listen_generic_wifi`): tylko słucha, nic nie
   wysyła do radia i nigdy nie załącza wyjść 12 V.
4. Prototyp v1: dioda ochronna D2 (SMBJ18A) jest wlutowana, a C1 jest na 35 V lub więcej. Diody
   Zenera D5, D7, D9 są zalecane.
5. Prototyp v1: JP1 jest otwarta (sprzętowy tryb tylko nasłuchu).
6. Bez zasilania: brak zwarcia między +12 V a masą i między sąsiednimi pinami gniazd.
7. Na urządzeniu nie ma terminatora 120 omów. Na module CAN Pal przełącznik terminatora jest na
   OFF. Opór CANH-CANL urządzenia bez zasilania: kilkadziesiąt kiloomów.
8. Masz mapę pinów **twojej** wiązki, zrobioną próbnikiem ciągłości (metoda w
   [Szukanie i sprawdzanie przewodów](../hardware/wiring.md)). Rysunek sprzedawcy to tylko
   wskazówka.
9. Wybrałeś tryb pracy:
    - **obok skrzynki** (konsola wypisuje `alongside the box`): oryginalna skrzynka CAN zostaje,
      urządzenie tylko słucha auta i radia. Domyślny (`listen`, `listen_wifi`).
    - **dekoder**: skrzynka jest wypięta, urządzenie rozmawia z radiem i załącza ACC, ILL i REV
      (`decoder`, `decoder_wifi`).
10. Tylko tryb dekoder: na stole, z małym obciążeniem (np. żarówką 12 V) na każdym wyjściu, ACC,
    ILL i REV pokazują 0 V po starcie, w obu trybach. Zobacz
    [Połączenie z radiem](radio-connection.md#the-12-v-outputs-acc-ill-rev).

## W aucie, zanim cokolwiek wepniesz {#in-the-car-before-you-plug-anything-in}

Zrób to w tej kolejności.

1. Zapisz stan auta przed zmianą: czy pilot otwiera auto po godzinie czy dwóch postoju? Czy ton
   czujników parkowania zachowuje się normalnie? Potrzebujesz tego do późniejszego porównania.
2. Zapłon włączony: zmierz parę CAN. Który przewód to CANH, który CANL, high-speed czy low-speed
   fault-tolerant ([Połączenie z CAN](can-connection.md#step-2-measure-the-voltages)).
3. Zmierz przewód zasilania przy włączonym zapłonie, przy wyłączonym i przy zamkniętym aucie:
   +12 V stałe czy załączane
   ([Zasilanie i ACC](power-and-acc.md#step-1-find-out-what-your-supply-wire-is)).
4. Zapłon wyłączony, kluczyk wyjęty, żadnej przeszkody przed autem ani za nim (czujniki
   parkowania muszą milczeć; dlaczego, zobacz [Pierwsza jazda](first-drive.md#before-you-start)).
   Dopiero teraz cokolwiek wpinaj.

## Podłączanie {#connecting}

„Równolegle” znaczy: oryginalny przewód zostaje cały i dalej idzie do skrzynki CAN; dokładasz do
niego odgałęzienie do urządzenia. Użyj rozgałęźnika (T-adaptera) albo zapasowej wtyczki. Jeśli nie
masz żadnego z nich, zdejmij kawałek izolacji, przylutuj przewód odgałęzienia i osłoń połączenie
koszulką termokurczliwą. Nigdy nie przecinaj oryginalnego przewodu.

1. Najpierw masa, potem +12 V, potem CANH i CANL **równolegle** z istniejącym okablowaniem.
2. W trybie „obok skrzynki” podłącz tylko: masę, +12 V, CANH, CANL i linię danych radio TX
   (równolegle, do RX urządzenia). ACC, ILL, REV urządzenia i jego wyjście UART TX zostaw
   niepodłączone.
3. W trybie dekoder wypnij skrzynkę CAN, **zanim** urządzenie zajmie jej miejsce. Dwie skrzynki na
   jednej linii UART i dwa źródła ACC zwarłyby swoje wyjścia.
4. Otwórz konsolę USB, zanim włączysz zapłon, żeby widzieć ją od pierwszej sekundy.

## Po pierwszej jeździe {#after-the-first-drive}

1. `stat`: `lost=0`, `errors=0` i `bus=active` przy włączonym zapłonie. Kilka błędów w chwili
   budzenia się magistrali może być normalne; licznik, który ciągle rośnie, nie jest: zobacz
   [Połączenie z CAN](can-connection.md#step-4-connect).
2. `can-tx=none` w ostatniej linii `[stat]` (w programie nie ma kodu nadawania).
3. Radio włącza się i wyłącza razem z zapłonem (tryb dekoder).
4. Pilot nadal otwiera auto po długim postoju.
5. Zapisałeś, co widziałeś. Krótka notatka z wersją, trybem i wynikiem pomoże później tobie
   i innym ([Udostępnij swoje auto](../make-it-yours/contribute.md)).

Dalej: [Zasilanie i ACC](power-and-acc.md).
