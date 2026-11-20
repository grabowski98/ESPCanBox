# Znajdź swoją magistralę CAN

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Zanim cokolwiek nagrasz albo zdekodujesz, potrzebujesz trzech faktów o swoim aucie:

1. które dwa przewody przy radiu to magistrala CAN,
2. jaki to rodzaj magistrali (high-speed czy low-speed fault-tolerant),
3. jej prędkość transmisji (w kbit/s).

Ta strona pokazuje, jak je ustalić multimetrem i urządzeniem w trybie tylko nasłuchu.
Nic na tej stronie nie wysyła niczego do auta.

Cała droga, od pomiaru magistrali do udostępnienia auta, jest na jednej stronie:
[Lista kontrolna przeniesienia](porting-checklist.md). Trzymaj ją otwartą i odhaczaj kolejne wiersze.

> [!CAUTION]
> **Na tej stronie używaj tylko programu do nasłuchu**
>
> Kroki 1 i 2 wymagają tylko multimetru. Od kroku 3 używaj wyłącznie programu tylko do
> nasłuchu: `listen_generic_wifi` albo osobnego sniffera z `extras/sniffer/`. Nigdy nie używaj
> środowiska z nadawaniem (`decoder_tx_wifi`) w aucie, którego jeszcze nie znasz. W programie
> tylko do nasłuchu kontroler CAN nie wysyła ramek, bitów potwierdzenia (ACK) ani ramek błędu,
> więc auto go nie widzi, nawet przy złej prędkości. Zanim zaczniesz, przeczytaj
> [Najpierw bezpieczeństwo](../getting-started/safety.md).

## Czego szukasz {#what-you-are-looking-for}

Nowoczesne auto ma zwykle kilka magistral CAN. Każda łączy grupę modułów i ma własną prędkość.
Złącze radia daje dostęp do jednej z nich i tylko do tej jednej. Która to magistrala i jakie
sygnały przenosi, zależy od auta: dla twojego auta nie wiadomo tego, dopóki tego nie zmierzysz.

W aucie wzorcowym (Peugeot 508 SW, 2015) magistrala przy radiu przenosi zapłon, bieg
wsteczny, światła, przyciski przy kierownicy, drzwi i czujniki parkowania (zmierzone (measured):
nagrania w `firmware/test_data/captures/`). Inne auto może mieć część tych sygnałów na magistrali,
której radio nie widzi.

Zbieraj fakty w tabeli szablonu pojazdu, `vehicles/_template/README.md`, sekcja
„Bus at the radio”. Wypełniaj wiersz dopiero wtedy, gdy go zmierzysz.

## Krok 1: znajdź dwa przewody {#step-1-find-the-two-wires}

Jeśli w aucie jest skrzynka CAN z zestawu radia, najłatwiej zacząć od jej wiązki: para CAN idzie
od złącza auta do wtyczki skrzynki CAN. [Połączenie z CAN](../install-in-car/can-connection.md)
(krok 1) pokazuje, jak ją znaleźć testerem ciągłości, a [Szukanie i sprawdzanie przewodów](../hardware/wiring.md)
pokazuje, jak rozpisać całą wiązkę.

Zapisz oba końce każdego przewodu i stronę, z której patrzysz (od strony przewodów albo od strony
styków, gdzie jest zatrzask). Numer pinu bez opisu widoku łatwo odczytać odwrotnie.

Przykład z auta wzorcowego: Quadlock pin 10 = CANH, pin 13 = CANL (zmierzone: ciągłość od
transceivera skrzynki CAN, potem odbiór bez błędów przy takim przypisaniu). Numery pinów obudowy
Quadlock to własna numeracja autora; zgodność z numeracją producenta jest niezweryfikowana
(unverified). Szczegóły: `vehicles/peugeot-508-sw-2015/wiring.md`.

### Sprawdź terminator starej skrzynki {#check-the-terminator-of-the-old-box}

Magistrala auta ma już swoje rezystory terminujące. Jeśli urządzenie zastępuje skrzynkę CAN,
sprawdź, że skrzynka żadnego nie miała. Wtedy wiesz, że urządzenie też nie może go mieć:

1. Wypnij skrzynkę CAN i zostaw ją bez zasilania.
2. Zmierz multimetrem opór między jej pinami CANH i CANL.
3. Zapisz wynik. Dziesiątki kiloomów = w skrzynce nie ma terminatora. Około 120 omów = skrzynka
   terminuje magistralę; zapisz to i zapytaj w zgłoszeniu (issue), zanim pójdziesz dalej.

Przykład z auta wzorcowego: dwie skrzynki z zestawów radia, 48 kOhm i 59 kOhm (zmierzone:
omomierz, bez zasilania), więc żadna z nich nie terminuje magistrali.

## Krok 2: rozpoznaj rodzaj magistrali {#step-2-tell-the-kind-of-bus}

Zapłon włączony, multimetr na napięciu stałym, minus na masie, plus kolejno na każdym przewodzie
CAN. Tabela w [Połączenie z CAN, krok 2](../install-in-car/can-connection.md#step-2-measure-the-voltages)
mówi, co znaczą odczyty. W skrócie:

- oba przewody w okolicy 2,5 V: prawdopodobnie **CAN high-speed**;
- jeden przewód w okolicy 0 V, drugi w okolicy 5 V: prawdopodobnie **CAN low-speed
  fault-tolerant**; przewód w okolicy 0 V to CANH;
- około 12 V: to nie jest przewód CAN, przerwij.

Multimetr pokazuje wartość średnią, więc to wskazówka, a nie dowód. Prawdziwe poziomy spoczynkowe
i dominujące pokaże oscyloskop.

Przykład z auta wzorcowego: CANH 0,65 V, CANL 4,5 V względem masy, zapłon włączony, tak samo
z podłączonym urządzeniem i bez niego (zmierzone: multimetr). Te poziomy wyglądają na CAN
low-speed fault-tolerant; to wniosek z napięć i nadal wymaga potwierdzenia oscyloskopem.

> [!NOTE]
> **Urządzenie ma transceiver high-speed**
>
> Transceiver urządzenia to typ high-speed na 3,3 V. W aucie wzorcowym odbierał magistralę
> bez błędów, chociaż napięcia wyglądają na CAN low-speed fault-tolerant (zmierzone: wszystkie
> nagrania w `firmware/test_data/captures/`). Czy odbierze magistralę twojego auta, nie wiadomo,
> dopóki nie spróbujesz. Co zrobiłby nadajnik high-speed na takiej magistrali, nie zmierzono:
> to jeszcze jeden powód, żeby zostać przy samym nasłuchu.

## Krok 3: ustal prędkość transmisji {#step-3-find-the-bit-rate}

### Oscyloskopem {#with-an-oscilloscope}

Gdy płyną ramki, znajdź najkrótszy impuls na CANH albo CANL. Ten impuls to jeden bit:

| Najkrótszy impuls | Prędkość transmisji |
|---|---|
| 2 µs | 500 kbit/s |
| 4 µs | 250 kbit/s |
| 8 µs | 125 kbit/s |
| 10 µs | 100 kbit/s |
| 20 µs | 50 kbit/s |

### Urządzeniem, metodą prób {#with-the-device-by-trial}

Prędkość transmisji pochodzi z profilu auta. Dla nieznanego auta używasz profilu `generic`: nic
nie dekoduje, nigdy nie nadaje i nigdy nie włącza wyjść 12 V. Jego prędkość to jedna liczba
w `firmware/lib/car_generic/profile_generic.cpp`:

```cpp
    125000,
```

125 kbit/s to tylko wartość domyślna, bo taka jest prędkość auta wzorcowego. To nie jest fakt
o twoim aucie.

1. Podłącz urządzenie do dwóch przewodów z kroku 1
   ([Połączenie z CAN](../install-in-car/can-connection.md)).
2. Wybierz pierwszą próbę na podstawie kroku 2. Magistrala low-speed fault-tolerant pracuje
   z prędkością 125 kbit/s albo mniejszą; magistrala high-speed często z 500 kbit/s, czasem
   250 kbit/s (ogólne cechy obu standardów, nie fakty o twoim aucie). Próbuj w tej kolejności:
   low-speed `125000`, potem `100000`, `50000`; high-speed `500000`, potem `250000`, `125000`.
3. Wpisz prędkość w bitach na sekundę do `profile_generic.cpp`. Program zna tylko te wartości:
   `50000`, `100000`, `125000`, `250000`, `500000`, `1000000` (lista jest w `firmware/src/can_bus.cpp`).
   Przy każdej innej wartości kontroler CAN nie wystartuje, a urządzenie zostanie w stanie
   bezpiecznym.
4. Zbuduj i wgraj, z folderu `firmware`: `uvx platformio run -e listen_generic_wifi -t upload`.
   Oczekiwany wynik: `[SUCCESS]`. Jeśli wgrywanie nie rusza, zobacz
   [Wgraj program](../quick-start/flash.md).
5. Włącz zapłon. Otwórz konsolę USB poleceniem `uvx platformio device monitor` i wpisz kilka
   razy `stat` ([Konsola](../reference/console.md)).
6. Odczytaj linię, która zaczyna się od `[stat] rx=`:

| Co widzisz | Co to znaczy |
|---|---|
| `rx=` rośnie o setki na sekundę, `errors=` stoi w miejscu | dobra prędkość |
| `rx=` stoi na 0, `errors=` rośnie | zła prędkość albo zamienione CANH i CANL |
| `rx=` stoi na 0, `errors=` stoi na 0 | brak ruchu: magistrala śpi, złe przewody albo brak masy |

Kilka błędów w chwili budzenia się magistrali może być normalne. W jednym nagraniu z auta wzorcowego
licznik błędów sniffera wzrósł ze 136 do 192, gdy magistrala się budziła, a potem stał na 192
przez resztę 14 minut (zmierzone: linie `# stat` w `can_20261110_190421.log`).

Jeśli prędkość jest zła, zmień liczbę, wgraj program ponownie i powtórz. Następną wartość próbuj
dopiero po sprawdzeniu przewodów.

Osobny sniffer z `extras/sniffer/` ma prędkość ustawioną na stałe w źródle: `CAN_TIMING`,
125 kbit/s (`extras/sniffer/README.md`). Polecenie `scan` jego wersji rejestratora mierzy dwie
linie UART do radia, a nie magistralę CAN.

## Krok 4: kiedy magistrala jest aktywna {#step-4-when-is-the-bus-awake}

Ustal, kiedy twoja magistrala milczy. Potrzebujesz tego do planowania nagrań: znacznik wstawiony,
gdy magistrala śpi, dostaje czas chwili, w której magistrala się budzi ([Nagrywanie](record.md)).

Przykład z auta wzorcowego: przy wyjętym kluczyku magistrala milczy. Po wyłączeniu zapłonu
przez około 100 s jest pełny ruch, potem przez 14 s tylko jedna ramka (`036`), potem cisza
(zmierzone: nagrania tego auta; `vehicles/peugeot-508-sw-2015/wiring.md`).

## Krok 5: pierwsze spojrzenie {#step-5-a-first-look}

Przy dobrej prędkości wpisz `dump` w konsoli USB. Urządzenie wypisuje każdą ramkę w jednej linii:

```
(140.548) can0 0F6#8E501E33BE5D5D24
(140.569) can0 128#C0B0010200411400
(140.573) can0 0E1#B4003FFCFCFCC200
```

Kolejno: czas w sekundach, interfejs, ID ramki, `#` i bajty danych szesnastkowo. Wpisz `dump`
jeszcze raz, żeby to zatrzymać. Linie powyżej pochodzą z `centralka_can_20261117_184411.log`.

Dla skali: auto wzorcowe przy włączonym zapłonie wysyła około 218 ramek na sekundę
o 70 różnych ID (zmierzone: `can_20261110_190421.log`). Twoje auto może wysyłać więcej albo mniej.

Zapisz, co ustaliłeś (złącze, piny i widok, rodzaj magistrali, prędkość transmisji, kiedy
magistrala śpi).

Dalej: [Nagrywanie magistrali](record.md).
