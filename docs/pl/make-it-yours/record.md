# Nagrywanie magistrali

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Nagranie (zapis, ang. capture) to plik tekstowy ze wszystkimi ramkami CAN, które zobaczyło
urządzenie, po kolei i z czasem. Robisz je, gdy wykonujesz czynności w aucie: wciskasz przycisk,
otwierasz drzwi, włączasz wsteczny. Potem szukasz bitów, które zmieniły się w tych chwilach
([Dekodowanie](decode.md)).

Najpierw potrzebujesz faktów ze strony [Znajdź swoją magistralę](find-your-bus.md): przewodów,
rodzaju magistrali i prędkości transmisji.

> [!CAUTION]
> **Nagrywaj programem tylko do nasłuchu**
>
> Użyj `listen_generic_wifi` albo osobnego sniffera z `extras/sniffer/`. Oba tylko słuchają.
> Nie nagrywaj nieznanego auta programem, który umie nadawać.

## Format {#the-format}

Jedno zdarzenie w jednej linii, ten sam format w całym projekcie:

```
(416.094276) can0 532#0800000000000000      CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117                UART bytes (only the sniffer's recorder variant)
# STEP 3 19:06:41.149 Volume + three times   comment line: a marker
# stat rx=30529 ...                          comment line: statistics
```

Linie zaczynające się od `#` to komentarze. Część komentarzy to **znaczniki** (ang. markers):
linie, które dodajesz w trakcie nagrywania, w chwili, gdy zaczynasz czynność. To dzięki
znacznikom nagranie łatwo zdekodować. Każde narzędzie zapisuje własny rodzaj:

| Narzędzie | Linia znacznika |
|---|---|
| `record.py` | `# MARKER n (time) computer clock HH:MM:SS: text` |
| `record_serial.py` (sniffer) | `# STEP n HH:MM:SS.mmm text` (Enter), `# MARK d HH:MM:SS.mmm` (cyfra `1` do `9` + Enter) |

Więcej o formacie: [Nagrania z auta](../testing/captures.md).

## Wybierz narzędzie {#choose-a-tool}

- Użyj `firmware/tools/record.py`, jeśli masz urządzenie z `listen_generic_wifi` i laptop w jego
  sieci Wi-Fi. Plik trafia do katalogu `logs` obok katalogu repozytorium.
- Użyj `extras/sniffer/record_serial.py`, jeśli masz osobną płytkę sniffera na USB. Plik trafia
  do katalogu podanego w `--out`.
- Użyj `dump` w konsoli USB tylko do szybkiego podglądu: nie zapisuje znaczników.

## Najpierw zaplanuj kroki {#plan-the-steps-first}

Spisz kroki, zanim wsiądziesz do auta. Dobry plan:

- **Zacznij od kilku sekund bez niczego**, żeby nagranie pokazało auto w spoczynku.
- **Jedna czynność na krok.** „Głośność w górę” i „głośność w dół” to dwa kroki.
- **Każdą czynność powtórz trzy razy**, z przerwą 2 do 3 sekund. Bit, który zmienia się trzy
  razy, to twój sygnał. Bit, który zmienia się cały czas, to licznik albo szum.
- **Wstaw znacznik tuż przed czynnością**, nie po niej.
- **Zapisz stan auta** na początku każdego kroku: zapłon, silnik, drzwi, co pokazuje radio.

Przykładowy plan po angielsku to `extras/sniffer/scenarios/example_buttons.txt`. Zaczyna się tak:

```
START: ignition off, key out, radio off, wait 10 s
Insert the key, ignition on (engine off), wait until the radio has started, then 10 s
Steering wheel: volume + three times, every 2 s
Steering wheel: volume - three times, every 2 s
```

Nagrania auta wzorcowego w `firmware/test_data/captures/` powstały według takich planów. Ich
linie kroków są po polsku (`# KROK n` = „step n”) i zostały takie, jak je nagrano.

## Nagrywanie przez Wi-Fi skryptem `record.py` {#record-over-wi-fi-with-recordpy}

1. Zbuduj i wgraj `listen_generic_wifi` z ustaloną prędkością transmisji: z folderu `firmware`,
   `uvx platformio run -e listen_generic_wifi -t upload` ([Wgraj program](../quick-start/flash.md)).
2. Podłącz urządzenie do auta ([Połączenie z CAN](../install-in-car/can-connection.md)).
3. Połącz laptop z siecią Wi-Fi urządzenia ([Pierwsze uruchomienie](../quick-start/first-boot.md)).
   Użyj laptopa na baterii albo zasilanego z auta.
4. Z katalogu repozytorium uruchom:

    ```
    python -I firmware/tools/record.py
    ```

    Domyślny adres to `10.50.8.1`. Inny adres podaj jako pierwszy argument.
5. Gdy skrypt zapyta, wpisz hasło urządzenia. To samo hasło jest kluczem Wi-Fi; pokazuje je
   `wifi showpass` w konsoli USB ([Pierwsze uruchomienie](../quick-start/first-boot.md)). Nie
   jest pokazywane na ekranie ani zapisywane.
6. Poczekaj na `[ok] connected, recording`. Skrypt wypisze nazwy dwóch plików: jednego z ramkami
   CAN, drugiego z liniami konsoli urządzenia.
7. Przy każdym kroku: wpisz krótki tekst (na przykład `volume up 3x`), naciśnij Enter, potem wykonaj
   czynność.
8. Wpisz `q` i Enter, żeby zakończyć.

`record.py` tylko czyta z urządzenia. Nie wysyła poleceń i niczego nie zmienia. Ramki `336`,
`3B6` i `2B6` są zapisywane jako `# masked`, bo w aucie wzorcowym niosą VIN. W twoim aucie
VIN może być w innych ramkach: znajdź je, zanim cokolwiek udostępnisz ([Udostępnij swoje auto](contribute.md)).

> [!WARNING]
> **`record.py` zawsze ukrywa `336`, `3B6` i `2B6`**
>
> W innym aucie te trzy ID mogą nieść zwykłe sygnały, a skrypt nie ma opcji, żeby je zostawić.
> Jeśli nie możesz znaleźć sygnału, zrób jedno nagranie snifferem albo przez `dump`: oba
> zachowują każdą ramkę, więc przed udostępnieniem wyczyść taki plik.

Gdy połączenie Wi-Fi się zerwie, skrypt próbuje dalej. Ramki, których urządzenie nie zdążyło
przekazać, są zapisywane jako `# lost N frames`. Urządzenie trzyma w pamięci około 9 s ramek, więc
krótkie przerwy niczego nie gubią.

> [!NOTE]
> **Nie nagrywaj w panelu w przeglądarce**
>
> Widok CAN panelu WWW nagrywa tylko wtedy, gdy jego karta przeglądarki jest widoczna. Gdy autor
> przełączył się na inną kartę w trakcie nagrywania w panelu, zgubiły się 26 593 ramki
> (zmierzone (measured): linia `# zgubiono 26593 ramek`, po polsku „lost 26593 frames”,
> w `centralka_can_20261117_184411.log`). Używaj `record.py`.

## Nagrywanie przez USB snifferem {#record-over-usb-with-the-sniffer}

Sniffer to mały, osobny program dla płytki ESP32-S3 z modułem transceivera CAN, w ogóle bez
przewodu nadawania. Budowa, połączenia i polecenie nagrywania są w `extras/sniffer/README.md`.
W skrócie:

```
uv run --with pyserial python -I extras/sniffer/record_serial.py COM7 --scenario extras/sniffer/scenarios/example_buttons.txt
```

Uruchom go z katalogu repozytorium. Zamiast `COM7` wpisz swój port: pokaże go
`uvx platformio device list`. Skrypt pokazuje następny krok scenariusza. Naciśnij Enter, gdy go
zaczynasz.

## Skąd znaczniki biorą czas {#how-markers-get-their-time}

Linia komentarza nie ma własnego czasu. Narzędzie do dekodowania ([Dekodowanie](decode.md)) daje
każdemu znacznikowi czas pierwszej ramki CAN po nim. Znacznik z `record.py` ma w nawiasie czas
urządzenia i wtedy używany jest ten czas.

Ma to znaczenie, gdy magistrala śpi. W nagraniu z auta wzorcowego `can_20261110_190421.log`
magistrala milczała przez pierwsze dwa kroki („start, czekaj 10 s” i „zapłon włączony”). Oba
znaczniki dostały ten sam czas, chwilę obudzenia się magistrali:

```
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
```

Dlatego dla „zapłon włączony” patrz na pierwsze ramki nagrania, a nie na zmianę w okolicy
znacznika.

## Po nagraniu {#after-the-recording}

- Sprawdź liczby, które skrypt wypisuje na końcu: ramki, znaczniki, zgubione ramki.
- Otwórz plik w edytorze tekstu i przeczytaj linie komentarzy. Teksty znaczników trafiają do
  pliku bez zmian: nie wpisuj w nie niczego prywatnego.
- Zachowaj oryginalny plik. Pracuj na kopii.
- Zanim komukolwiek udostępnisz nagranie, usuń z niego VIN i sprawdź plik
  ([Udostępnij swoje auto](contribute.md)).

Dalej: [Dekodowanie ramek](decode.md).
