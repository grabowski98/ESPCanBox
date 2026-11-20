# Najpierw bezpieczeństwo

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Przeczytaj tę stronę, zanim cokolwiek zbudujesz, wgrasz albo podłączysz. Jest celowo krótka.

## Słowa używane na tych stronach {#words-used-on-these-pages}

- **urządzenie**: płytka ESP32-S3 tego projektu;
- **skrzynka** albo **skrzynka CAN**: dokupiona skrzynka pośrednicząca, sprzedawana razem z radiem;
- **radio**: radio na Androidzie (jednostka główna);
- **tylko nasłuch** (listen-only): urządzenie odbiera ramki CAN i nic nie wysyła na magistralę
  auta;
- **TWAI**: kontroler CAN wbudowany w ESP32-S3;
- **SLNT**: nóżka wyciszenia modułu transceivera CAN; stan wysoki = nadajnik wyłączony;
- **BSI**: moduł nadwozia auta;
- **`JP1`**: zworka na płytce prototypu w linii TX do modułu CAN;
- **środowisko**: nazwany zestaw opcji kompilacji; decyduje, co program może robić.

[Słowniczek](../contributing/glossary.md) zawiera więcej pojęć.

## Czym jest ten projekt {#what-this-project-is}

- Projektem hobbystycznym, udostępnionym **w obecnym stanie**, bez żadnej gwarancji. Używasz go na
  własne ryzyko.
- Powstał i był mierzony w **jednym aucie** (Peugeot 508 SW z 2015 roku) z jednym radiem na
  Androidzie. W każdym innym aucie każda ramka jest **niezweryfikowana** (unverified), dopóki sam
  jej nie nagrasz.
- **Nie** jest przeznaczony do niczego, co chroni ludzi. Nie podłączaj go do hamulców, układu
  kierowniczego, poduszek powietrznych, sterowania silnikiem ani do żadnej magistrali, która je
  obsługuje.
- Projekt nie jest związany z firmami Peugeot, Stellantis, Simple Soft, Raise, Junsun ani FYT.
  Ich nazwy pojawiają się tylko po to, by powiedzieć, na czym go sprawdzono.

## Urządzenie domyślnie słucha {#the-device-listens-by-default}

Od strony auta urządzenie jest odbiornikiem CAN. W każdej zwykłej kompilacji **tylko słucha**:

- kontroler TWAI pracuje w trybie tylko do nasłuchu (nie wysyła nawet bitu potwierdzenia, ACK);
- program trzyma nóżkę `SLNT` transceivera CAN w stanie wysokim, więc nadajnik układu jest
  wyłączony;
- kompilacje `listen`, `decoder`, `listen_wifi`, `decoder_wifi` i `listen_generic_wifi` w ogóle
  nie zawierają kodu nadawania (katalog `firmware/src/can_tx/` nie jest kompilowany;
  `firmware/tools/check_no_can_tx.sh` sprawdza skompilowane programy).

> [!CAUTION]
> **Nadawanie może zmienić stan auta**
>
> Nie wgrywaj `decoder_tx_wifi`, dopóki nie przeczytasz strony
> [Nadawanie na magistralę CAN](../safety/transmit.md) i nie zaakceptujesz ryzyka. Nigdy nie
> wysyłaj ramek, których sam nie nagrałeś z auta.

Tylko jedno środowisko, `decoder_tx_wifi`, może nadawać na magistralę auta. Robi to dopiero wtedy,
gdy celowo je uzbroisz i zrestartujesz urządzenie:

| Kompilacja | Ramki, które może wysłać na magistralę auta |
|---|---|
| wszystkie środowiska poza `decoder_tx_wifi` | żadne |
| `decoder_tx_wifi` (domyślnie) | tylko ramkę `15B`, sprawdzaną wyłącznie po identyfikatorze i długości, po `cantx on` i restarcie |
| `decoder_tx_wifi` z `-DCAN_TX_EXPERIMENTAL=1` we własnym `platformio_override.ini` | `15B` oraz ramka próbna `1A9` (tylko przyciski czujników parkowania i martwego pola) |

Sterowanie klimatyzacją z radia z tym projektem **nie** działa. Dla `15B` BSI przyjęło ustawienia
(widać to w ramce `260`), ale widocznego skutku w aucie **nie sprawdzono**. Co każda ramka zrobiła
w aucie wzorcowym: [Nadawanie na magistralę CAN](../safety/transmit.md).

## Blokada sprzętowa na płytce prototypu {#hardware-lock-on-the-prototype-board}

[Płytka prototypu](../hardware/prototype-pcb.md) w wersji ze schematu ma blokadę sprzętową: nóżka
`SLNT` modułu CAN jest podciągnięta rezystorem do 3,3 V, a zworka `JP1` zostaje **otwarta**. Przy
otwartej `JP1` żaden program nie może nadawać na magistralę.

Płytkę autora przerobiono później do testów nadawania (`JP1` zwarta, `SLNT` podłączona do GPIO6).
Na takiej płytce **tylko program** decyduje, czy urządzenie nadaje. Trzymaj `JP1` otwartą, chyba że
celowo budujesz program z nadawaniem.

## Zanim zaczniesz {#before-you-start}

Te zasady wynikają z tego, jak powstawał sam projekt. Stosuj je w tej kolejności.

1. **Zmierz, zanim podłączysz.** Ustal, które piny przenoszą CAN, jaki to typ magistrali
   (high-speed czy low-speed fault-tolerant) i jaka jest jej prędkość: multimetrem, a lepiej
   oscyloskopem albo analizatorem logicznym. Nie podłączaj niczego do magistrali, zanim tego nie
   wiesz. Jak to zrobić, pokazuje strona [Znajdź swoją magistralę CAN](../make-it-yours/find-your-bus.md).
2. **Bez terminatora.** Magistrala auta ma już terminację. Wyłącz terminator 120 Ω w swoim module
   CAN i nie dokładaj żadnego.
3. **Najpierw słuchaj.** Na biurku możesz wgrać dowolną kompilację tylko do nasłuchu. W aucie
   `listen` albo `listen_wifi` używaj tylko w aucie wzorcowym (Peugeot 508, profil PSA AEE2010);
   w każdym innym aucie używaj tylko `listen_generic_wifi` (profil ogólny: zapisuje ramki, nic nie
   wysyła do radia). Nagrywaj, dekoduj, porównuj. Nadawanie jest na końcu, o ile w ogóle.
4. **Żadnych zapisów do sterowników auta.** Ten projekt nie robi diagnostyki i nigdy nie zapisuje
   niczego do modułów auta (BSI ani żadnego innego). Nie dodawaj takiego kodu.
5. **Jedno źródło na przewód.** Trybu dekoder (`decoder`, `decoder_wifi`) używaj tylko przy
   wypiętej skrzynce CAN. Dwa nadajniki na linii danych radia albo dwa źródła na przewodzie ACC
   oznaczają zwarcie wyjść.
6. **Najpierw stół, potem auto.** Zanim zabierzesz płytkę bazową do auta, zasil ją z zasilacza
   laboratoryjnego z ograniczeniem prądu (ok. 100 mA przy pierwszym zasileniu). Sama płytka
   ESP32-S3 na kablu USB nie potrzebuje zasilacza laboratoryjnego. Zobacz
   [Test na stole](../quick-start/bench-test.md).

## 12 V i akumulator {#12-v-and-the-battery}

> [!WARNING]
> **Zabezpieczenie wejścia przed podłączeniem do auta**
>
> Na płytce prototypu wlutuj transil `D2` (SMBJ18A) i użyj kondensatora wejściowego na 35 V lub
> więcej, zanim podłączysz płytkę do auta. Zostaw bezpieczniki na miejscu; mostek z drutu
> zamiast bezpiecznika jest tylko na stół.

- Urządzenie jest zasilane ze stałego 12 V (jak skrzynka CAN, którą zastępuje). Po 30 s ciszy na
  magistrali przechodzi w głęboki sen. Uśpienia i prądu w uśpieniu **nie zmierzono** jeszcze na
  sprzęcie. Zanim zostawisz urządzenie podłączone na wiele dni, zmierz prąd spoczynkowy auta
  z urządzeniem i bez niego.
- Z włączonym Wi-Fi urządzenie pobiera więcej prądu. Program wyłącza Wi-Fi, gdy auto śpi i nikt
  nie używa panelu.

## Panel WWW i aktualizacje przez Wi-Fi {#the-web-panel-and-updates-over-wi-fi}

Kompilacje `*_wifi` otwierają własną sieć Wi-Fi z panelem WWW. Pamiętaj o ich ograniczeniach:

- panel używa zwykłego HTTP, bez szyfrowania poza WPA2 sieci Wi-Fi;
- hasło urządzenia jest też kluczem Wi-Fi;
- obrazy programu wysyłane przez Wi-Fi **nie są podpisane**. Każdy, kto zna hasło, może wgrać inny
  program. Na płytce bez blokady sprzętowej taki program mógłby nadawać.

Więcej na stronie [Model zabezpieczeń](../safety/security.md).

## Dalej {#next}

[Czego potrzebujesz](requirements.md): płytka, części i narzędzia.
