# Lista kontrolna przeniesienia

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Cała droga od „auta, którego nikt nie zmierzył” do „urządzenie działa zamiast skrzynki CAN”, na
jednej stronie. Idź od góry do dołu. Każdy wiersz mówi, kiedy krok jest zrobiony. Nie pomijaj
wiersza dlatego, że inne auto „pewnie” działa tak samo: każdy fakt o twoim aucie potrzebuje
statusu, **zmierzone** (measured) albo **niezweryfikowane** (unverified).

Wyniki wpisuj na bieżąco do kopii `vehicles/_template/README.md`. Polecenia z `tools/`
uruchamiasz z folderu podanego w wierszu: z katalogu repozytorium albo z `firmware`.

> [!CAUTION]
> **Nic z tej listy nie nadaje na magistralę auta**
>
> Każdy program używany tutaj tylko słucha magistrali CAN auta. Własnego profilu w ogóle nie da
> się zbudować z nadawaniem. Nadawanie dla nowego auta to osobny projekt z własnymi zasadami:
> [Nadawanie na magistralę CAN](../safety/transmit.md).

## 1. Magistrala {#1-the-bus}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Znajdź przewody CANH i CANL przy radiu | ciągłość zmierzona od złącza do znanego punktu i odebrane na nich ramki | [Znajdź swoją magistralę](find-your-bus.md) |
| Zmierz napięcia linii względem masy, zapłon włączony | zapisane napięcia CANH i CANL | [Znajdź swoją magistralę](find-your-bus.md) |
| Rozstrzygnij: high-speed czy low-speed fault-tolerant | wniosek zapisany jako wniosek; sprawdzenie oscyloskopem odnotowane jako zrobione albo do zrobienia | [Połączenie z CAN](../install-in-car/can-connection.md) |
| Zmierz terminator starej skrzynki CAN | opór CANH-CANL, bez zasilania, zapisany | [Znajdź swoją magistralę](find-your-bus.md#check-the-terminator-of-the-old-box) |
| Zbuduj i wgraj `listen_generic_wifi` z pierwszą próbą prędkości transmisji | konsola pokazuje `# profile: car=generic radio=none` | [Wgraj program](../quick-start/flash.md) |
| Ustal prędkość transmisji | ramki odebrane bez błędów przy jednej z prędkości 50, 100, 125, 250, 500 albo 1000 kbit/s | [Znajdź swoją magistralę](find-your-bus.md) |
| Sprawdź, czy magistrala cichnie po wyjęciu kluczyka | czas od wyjęcia kluczyka do ciszy, z nagrania | [Znajdź swoją magistralę, krok 4](find-your-bus.md#step-4-when-is-the-bus-awake) |

## 2. Nagrania {#2-recordings}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Zaplanuj kroki | spisany plan: jedna czynność na krok, każda powtórzona trzy razy | [Nagrywanie magistrali](record.md) |
| Nagraj ze znacznikami | plik z jednym znacznikiem na krok; sprawdzona liczba zgubionych ramek | [Nagrywanie magistrali](record.md) |
| Znajdź ramki, które niosą VIN | ID zapisane albo „nie znaleziono” | [Udostępnij swoje auto](contribute.md) |
| Oczyść nagranie (z katalogu repozytorium) | `python -I tools/sanitize_capture.py --check` zgłasza 0 problemów, z ID twoich ramek VIN w `--ids` | [Udostępnij swoje auto](contribute.md) |

## 3. Sygnały {#3-signals}

Dekoduj tylko to, czego potrzebujesz. Według przydatności:

| Sygnał | Zrobione, gdy | Do czego |
|---|---|---|
| Stan zapłonu i silnika | bajt i wartości dla wyłączony / włączony / silnik pracuje, widziane w trzech powtórzeniach; znany [okres ramki](decode.md#how-often-a-frame-comes) (musi być krótszy niż 1 s) | ACC dla radia |
| Bieg wsteczny | bajt.bit, widziany trzy razy | wyjście wstecznego (kamera) |
| Światła | bajt.bit; znany [okres ramki](decode.md#how-often-a-frame-comes) (musi być krótszy niż 1 s) | wyjście podświetlenia |
| Przyciski przy kierownicy | jeden wzór bajt.bit na przycisk, żaden nie występuje w spoczynku | przyciski dla radia |
| Pokrętła | bajt licznika, kierunek na ząbek | następny / poprzedni |
| Czujniki parkowania | stan, strefy 0 do 7 dla każdego czujnika, żądanie dźwięku, odstęp, jeśli auto go wysyła; [okres ramki](decode.md#how-often-a-frame-comes) (musi być krótszy niż 400 ms) | pikanie, obraz czujników w radiu |
| Drzwi, bagażnik, maska (opcjonalnie) | jeden bit na drzwi, każde drzwi widziane otwarte i zamknięte | obraz drzwi w radiu |
| Temperatura zewnętrzna, klimatyzacja, tryb ekonomiczny, ustawienia, komputer pokładowy (opcjonalnie) | bajt i skala każdej wartości, sprawdzone z wyświetlaczem auta | odpowiednie ekrany radia |

Każdy sygnał trafia na stronę pojazdu z ramką, `byte.bit`, wartościami, statusem i krokiem
nagrania, który go pokazał. Jak to zrobić, pokazuje [Dekodowanie ramek](decode.md).

## 4. Profil {#4-profile}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Skopiuj `firmware/lib/car_example` do `firmware/lib/car_<name>` i zmień nazwy | zmienione nazwy plików, obiektu, przestrzeni nazw i `name` w `library.json` | [Pisanie profilu auta](write-a-profile.md) |
| Wpisz tylko zmierzone sygnały | każdy nieznany wskaźnik to `nullptr`, każda nieznana tabela pusta | [Pisanie profilu auta](write-a-profile.md) |
| Przelicz numery bajtów | twoje notatki (bajt 1 = pierwszy) zamienione na indeksy w kodzie (`data[0]`, `byteIndex 0`) | [Pisanie profilu auta](write-a-profile.md) |
| Zdecyduj o `powerOutputs` | `true` tylko z niezawodnym `decodeBody` | [Pisanie profilu auta](write-a-profile.md) |
| Zbuduj `listen_mycar` | `SUCCESS`, `car_<name>` na liście zależności | [Pisanie profilu auta](write-a-profile.md) |

## 5. Testy na komputerze {#5-tests-on-the-pc}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Test na komputerze na twoim oczyszczonym nagraniu | wynik przeczytany linia po linii i porównany ze znacznikami, potem zapisany w `firmware/test_host/expected/` | [Testowanie profilu](test-your-profile.md) |
| Wszystkie testy na komputerze (z `firmware`) | `All tests match the references.` | [Testy na komputerze](../testing/host-tests.md) |
| Sprawdzenie warstw (z `firmware`) | `python -I tools/check_layering.py` kończy się na `Layering OK.` | [Testowanie profilu](test-your-profile.md) |
| Sprawdzone kody radia | każdy z twoich przycisków daje kod, który chcesz; domyślne zamiany (`keyRemapDefaults`) tylko te, które chcesz, a `keyRemapProtocol` wskazuje protokół, którego to kody. Przy własnym protokole radia (`-DRADIO_PROTOCOL=CUSTOM`): skopiuj `firmware/test_host/radio_example.cpp` do własnego testu, dołącz swój `protocol_<nazwa>.cpp` zamiast `protocol_example.cpp` (budowę pokazują wiersze `radio_example` w `firmware/tools/run_host_tests.sh`) i sprawdź, że linia do radia ma tylko twoje ramkowanie, żadnej ramki XP | [Przyciski i radio](radio-mapping.md) |

## 6. Stół i auto {#6-bench-and-car}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Test na stole z twoim programem | linie startowe pokazują twój profil i `# can=ok`; w programie tylko do nasłuchu wyjścia 12 V wyłączone | [Test na stole](../quick-start/bench-test.md) |
| W aucie obok skrzynki CAN (`listen_mycar`) | twoje przyciski widać po `keys`, zapłon i wsteczny w konsoli, pikanie czujników z głośnika urządzenia; nic nie jest wysyłane do radia | [Pierwsza jazda](../install-in-car/first-drive.md) |
| Tryb dekoder (`decoder_mycar`, skrzynka CAN wypięta) | radio włącza się i wyłącza z zapłonem; działają przyciski, kamera cofania i obraz czujników | [Połączenie z radiem](../install-in-car/radio-connection.md) |
| Notatki o tym, co nie działa | wypisane w sekcji „Not known yet” strony pojazdu | [Udostępnij swoje auto](contribute.md) |

## 7. Udostępnienie {#7-share}

| Krok | Zrobione, gdy | Strona |
|---|---|---|
| Strona pojazdu | `vehicles/<make>-<model>-<year>/README.md` ze statusem w każdym wierszu | [Udostępnij swoje auto](contribute.md) |
| Sprawdzenie danych prywatnych (z katalogu repozytorium) | `python -I tools/scan_private.py .` zgłasza 0 błędów | [Udostępnij swoje auto](contribute.md) |
| Pull request albo zgłoszenie | otwarte z szablonem „New vehicle” | [Udostępnij swoje auto](contribute.md) |

Sama strona pojazdu, bez profilu w programie, to też przydatny wkład.

Dalej: [Udostępnij swoje auto](contribute.md).
