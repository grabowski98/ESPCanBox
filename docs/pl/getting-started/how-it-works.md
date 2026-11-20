# Jak to działa

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona wyjaśnia, co robi urządzenie i jak jego części do siebie pasują. Do wgrania programu nie
jest potrzebna.

## Jaki problem rozwiązuje {#the-problem-it-solves}

Radio na Androidzie z rynku wtórnego nie rozumie auta. Pomiędzy nimi siedzi mała **skrzynka CAN**:
czyta magistralę CAN auta i przez łącze szeregowe (UART) mówi radiu, co się dzieje: przyciski przy
kierownicy, czujniki parkowania, drzwi, temperatura zewnętrzna, zapłon. Włącza też i wyłącza radio
(przewód ACC) i podaje mu sygnały biegu wstecznego i podświetlenia.

To urządzenie robi to samo na płytce ESP32-S3, z otwartym programem i panelem WWW. Może pracować
**obok** skrzynki (żeby się uczyć i porównywać) albo **zamiast** niej.

## Rysunek {#the-picture}

```text
   car CAN bus (125 kbit/s on the reference car)
        |
        v
   CAN transceiver module (SLNT high = transmitter off)
        |
        v
 +--------------------------- ESP32-S3 ---------------------------+
 |                                                                |
 |  TWAI controller (listen-only)                                 |
 |        |                                                       |
 |        v                                                       |
 |  car profile (lib/car_*)                                       |
 |        |                                                       |
 |        v                                                       |
 |  decoder logic (lib/app) ---+--> beeper -------> I2S amplifier --> speaker
 |                             |                                  |
 |                             +--> radio protocol (lib/radio_*) ---> UART to the radio  (*)
 |                             |                                  |
 |                             +--> power policy -> 12 V outputs ---> ACC, ILL, REV      (*)
 |                                                                |
 |  USB console, 115200 baud  <--> computer                       |
 |  Wi-Fi panel (*_wifi builds only)  <--> phone or laptop        |
 +----------------------------------------------------------------+

 (*) decoder mode only. In "beside the box" mode the device sends nothing
     to the radio and keeps the 12 V outputs off; it only listens to the
     radio -> box line.
```

TWAI = kontroler CAN wbudowany w ESP32-S3. UART = łącze szeregowe do radia.
I2S = cyfrowe łącze audio do wzmacniacza.

## Dwa tryby {#two-modes}

Tryb wybiera **środowisko** PlatformIO, które kompilujesz (nazwany zestaw opcji kompilacji
w `firmware/platformio.ini`), nigdy zmiana w kodzie. Tabela wszystkich środowisk jest na stronie
[Wgraj program](../quick-start/flash.md).

| Tryb | Środowiska | Kiedy | Co urządzenie wysyła |
|---|---|---|---|
| obok skrzynki (domyślny) | `listen`, `listen_wifi`, `listen_generic_wifi` | fabryczna albo dokupiona skrzynka CAN zostaje podłączona | nic do radia, wyjścia 12 V wyłączone; pika z własnego głośnika i słucha magistrali CAN oraz linii radio -> skrzynka |
| dekoder | `decoder`, `decoder_wifi`, `decoder_tx_wifi` | skrzynka CAN jest wypięta, urządzenie zajmuje jej miejsce | ramki do radia przez UART; ACC, podświetlenie i bieg wsteczny na wyjściach 12 V |

W obu trybach magistrala CAN auta jest tylko słuchana. Jedynym wyjątkiem jest osobne środowisko
`decoder_tx_wifi`, opisane na stronie [Nadawanie na magistralę CAN](../safety/transmit.md).

## Co się dzieje z jedną ramką CAN {#what-happens-to-one-can-frame}

1. **Transceiver CAN** zamienia napięcia magistrali na poziomy logiczne. Jego nóżka wyciszenia jest
   w stanie wysokim, więc nie może sterować magistralą.
2. **Kontroler TWAI** (kontroler CAN wbudowany w ESP32-S3) odbiera ramkę w trybie tylko do nasłuchu.
3. **Profil auta** wie, które ramki są ważne: w aucie wzorcowym profil `psa_aee2010` czyta na
   przykład zapłon z ramki `0F6`, światła z `036`, czujniki parkowania z `0E1` i przyciski przy
   kierownicy z `21F`. Profil `generic` nic nie dekoduje: to punkt wyjścia dla innego auta.
4. **Logika dekodera** pamięta stan auta i decyduje, co zrobić:
   - **brzęczyk** gra rytm czujników parkowania według najbliższej strefy, ton ciągły, gdy auto
     o niego prosi, i milknie 400 ms po ostatniej ramce `0E1`;
   - **protokół radia** (`XP` dla radia ustawionego na profil skrzynki CAN Simple Soft albo `none`)
     zamienia naciśnięcia przycisków i stany na ramki UART, co najmniej 10 ms jedna od drugiej;
   - **zasady zasilania** decydują, kiedy włączają się ACC, podświetlenie i bieg wsteczny.
5. W trybie dekoder ramki UART idą do radia, a wyjścia 12 V się przełączają. W trybie „obok
   skrzynki” jedno i drugie jest wyłączone.

Ta sama logika (wszystko w `firmware/lib/`) kompiluje się też na PC. Testy na komputerze
odtwarzają przez nią prawdziwe nagrania z auta i porównują wynik z zapisanymi wzorcami. Zobacz
[Testy na komputerze](../testing/host-tests.md).

## Gdzie jest kod {#where-the-code-lives}

| Katalog | Co zawiera |
|---|---|
| `firmware/lib/core/` | logika bez sprzętu: rozbiór ramek, rytm brzęczyka, zasady zasilania, brama nadawania, pomocnicze funkcje panelu |
| `firmware/lib/car_psa_aee2010/`, `lib/car_generic/` | profile aut: które ramki, które bity, jaka prędkość magistrali |
| `firmware/lib/radio_xp/`, `lib/radio_none/` | protokoły radia |
| `firmware/lib/app/` | klasa `Decoder`, która łączy auto, radio i wyjścia |
| `firmware/src/` | sprzęt: sterownik CAN, dźwięk, UART, wyjścia 12 V, uśpienie, konsola, panel Wi-Fi |
| `firmware/boards/` | numery nóżek każdej płytki |

Więcej na stronie [Architektura](../reference/architecture.md).

## Konsola i panel {#console-and-panel}

- **Konsola USB** (115200 bodów) wypisuje stan i przyjmuje polecenia, takie jak `stat`, `ver`,
  `test` i `dump`. Z `dump` wypisuje każdą ramkę CAN w formacie `(seconds) can0 ID#DATA`, tym
  samym, którego używają nagrania. Wszystkie polecenia: [Konsola](../reference/console.md).
- **Panel WWW** (tylko kompilacje `*_wifi`) działa we własnej sieci Wi-Fi urządzenia. Pokazuje
  stan auta, ruch na CAN, łącze z radiem i konsolę, a do tego potrafi zaktualizować program przez
  Wi-Fi. Zobacz [Pierwsze uruchomienie](../quick-start/first-boot.md) i
  [opis panelu](../reference/panel-guide.md).

## Uśpienie {#sleep}

Urządzenie jest zasilane ze stałego 12 V. Po 30 s bez ramek CAN (i bez komputera na USB) przechodzi w głęboki
sen i budzi się przy następnej ramce CAN. W aucie wzorcowym magistrala cichnie ok. 100 s po wyjęciu
kluczyka (zmierzone: nagranie wyłączenia). Samego uśpienia ani prądu w uśpieniu nie zmierzono
jeszcze na sprzęcie.

## Co na pewno działa {#what-is-known-to-work}

Stan każdej funkcji w aucie wzorcowym jest na stronie
[Peugeot 508 SW](../reference/vehicles/peugeot-508.md) i w `vehicles/SUPPORTED.md`. W skrócie:
zapłon i ACC, bieg wsteczny, światła, czujniki parkowania i ton, przyciski przy kierownicy
i panelu środkowego, komputer pokładowy, temperatura zewnętrzna, stan klimatyzacji i drzwi są
dekodowane ze zmierzonych ramek. Magistrala przy radiu tego auta pracuje z prędkością 125 kbit/s
(zmierzone); jej napięcia wyglądają na CAN low-speed fault-tolerant, co trzeba jeszcze potwierdzić
oscyloskopem.

## Dalej {#next}

[Czego potrzebujesz](requirements.md): płytka, części i narzędzia.
