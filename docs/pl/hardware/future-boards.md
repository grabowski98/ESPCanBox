# Przyszłe płytki

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Prototyp v1 potwierdził pomysł, ale to wytrawiona w domu płytka bazowa z przewodami zamiast złączy.
Nowa płytka jest planowana. **Nie jest jeszcze zaprojektowana.** Nic na tej stronie nie jest
obietnicą ani terminem: to lista wniosków i pomysłów, od których zacznie się następna płytka.

Pierwsze publiczne wydanie projektu jest planowane dopiero po zakończeniu prac nad nową płytką.

## Co ma poprawić następna płytka {#what-the-next-board-should-fix}

| Prototyp v1 | Plan na następną płytkę |
|---|---|
| trawienie w domu, przelotki z drutu, brak plików Gerber | płytka do fabryki PCB, z plikami Gerber i wierceń |
| przewlekane gniazda na gotowe moduły | elementy SMD, złącza na moduły tam, gdzie moduł nadal ma sens |
| gniazdo wiązki połączone luźnymi przewodami | złącze wiązki na płytce, z mapą pinów sprawdzoną na prawdziwej wtyczce |
| SLNT na stałe w stanie wysokim przez R3; nadawanie wymaga ręcznie dołożonego przewodu | wybierana blokada `JP_S` i zworka TX `JP_TX` ze [schematu](schematic.md) |
| stały kierunek UART | zworki zamiany `JP_UART` dla wiązek połączonych odwrotnie |
| 3 wyjścia (ACC, ILL, REV) | czwarte wyjście, już narysowane na schemacie |
| bez rozszerzeń, bez I2C, bez przycisku | złącze rozszerzeń, I2C, przycisk awaryjny (wszystko na schemacie) |
| prąd uśpienia niezmierzony | prąd uśpienia zmierzony i zapisany |

Schemat prototypu v1 zawiera już większość prawej kolumny. To punkt wyjścia, nie gotowy projekt.

## Pomysły do rozważenia {#ideas-under-consideration}

Pochodzą z wczesnych notatek planistycznych. Żaden nie był sprawdzony.

- **Transceiver z trybem czuwania** (np. TJA1042T/3, ta sama obudowa co TJA1051T/3), jeśli prąd
  uśpienia okaże się za duży. W czuwaniu nie odbiera normalnie ramek, więc jedyną sprzętową
  blokadą nadawania zostałaby zworka TX.
- **Transceiver low-speed fault-tolerant**, jeśli oscyloskop potwierdzi, że magistrala przy radiu
  auta wzorcowego jest typu low-speed fault-tolerant. Prototyp odbiera ją transceiverem
  high-speed, co działa, ale nie jest to element dopasowany do magistrali.
- **Ochrona linii CAN** (podwójna dioda TVS do CAN) i miejsca na koraliki ferrytowe, tak jak na
  schemacie.
- **Zasilanie, które samo się wyłącza**: przetwornica i transceiver, które pozwalają płytce spać
  przy bardzo małym prądzie i budzić się przy ruchu na magistrali.
- **Wersja z zewnętrzną anteną Wi-Fi** (ESP32-S3-WROOM-1U), jeśli zasięg zza deski rozdzielczej
  okaże się za mały. Niezmierzone.
- **Dwa urządzenia z jednego kodu**: stała płytka dekodera do auta i osobny analizator na stół
  z gniazdami na moduły (np. moduł do podsłuchu UART) do nagrywania i dekodowania.

## Własna płytka {#your-own-board}

Nie musisz czekać. Program bierze piny z jednego pliku nagłówkowego na płytkę. Skopiuj
`firmware/boards/board_template.h` do `firmware/boards/board_<name>.h`, wpisz każdy pin, usuń
linię `#error` i zbuduj z `-DBOARD=CUSTOM -DBOARD_CUSTOM_HEADER='"board_<name>.h"'`
(przykład w `firmware/platformio_override.sample.ini`). Kompilacja odrzuca GPIO 35-37 i 19/20,
piny startowe 0/3/45/46 bez zapisanego uzasadnienia, nieistniejące GPIO i jedno GPIO użyte dwa
razy. Zobacz [Konfiguracja i piny](../reference/config-and-pins.md).

Jeśli projektujesz płytkę, zachowaj zasady bezpieczeństwa prototypu:

- sprzętowy sposób na utrzymanie transceivera w trybie cichym po resecie (podciągnięcie wejścia
  trybu cichego);
- zworka TX, którą można zostawić otwartą;
- brak terminatora 120 omów;
- bezpiecznik i ogranicznik przepięć na wejściu 12 V;
- wyjścia 12 V wyłączone po resecie.

Pliki sprzętu są udostępnione na licencji CERN-OHL-S-2.0 (tymczasowo). Jeśli ulepszysz płytkę,
pull request jest mile widziany: zobacz [Udostępnij swoje auto](../make-it-yours/contribute.md).

Dalej: [Lista kontrolna montażu](../install-in-car/checklist.md).
