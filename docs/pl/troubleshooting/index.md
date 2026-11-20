# Rozwiązywanie problemów

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona pomaga, gdy urządzenie nie robi tego, czego się spodziewasz. Wymienia tylko problemy,
które naprawdę wystąpiły w aucie wzorcowym albo na stole, oraz to, co robi kod.

1. Podłącz USB i otwórz konsolę w głównym katalogu repozytorium:
   `uvx platformio device monitor -d firmware` (szczegóły: [Konsola USB](../reference/console.md)).
2. Wpisz `stat`. Dobry wynik przy włączonym zapłonie zawiera `can=ok audio=ok radio=ok`
   i `lost=0 errors=0 bus=active`. Pomaga też kolor diody:
   [Dioda stanu](../reference/config-and-pins.md#status-led).
3. Znajdź swój objaw:
    - `FAIL` w liniach startowych, czerwona dioda: [Urządzenie zgłasza FAIL przy starcie](#the-device-reports-fail-at-start);
    - `rx=0`, `bus=quiet`: [Brak ramek CAN](#no-can-frames);
    - `lost` albo `errors` powyżej 0: [Zgubione ramki albo błędy magistrali](#frames-lost-or-bus-errors);
    - radio się nie włącza: [Radio się nie włącza (brak ACC)](#the-radio-does-not-turn-on-no-acc);
    - brak przycisków albo danych auta w radiu: [Radio nie reaguje na przyciski albo nie pokazuje danych auta](#the-radio-does-not-react-to-buttons-or-shows-no-car-data);
    - panel się nie otwiera: [Panel WWW się nie otwiera](#the-web-panel-does-not-open);
    - pilot: [Pilot nie otwiera auta po postoju](#the-remote-key-does-not-unlock-after-parking);
    - ton, który nie milknie: [Ton czujników parkowania nie milknie](#the-parking-tone-keeps-sounding).

Jeśli twojego problemu tu nie ma, patrz [Jak poprosić o pomoc](faq.md#how-do-i-ask-for-help).

## Urządzenie zgłasza FAIL przy starcie {#the-device-reports-fail-at-start}

Linie startowe kończą się na `# can=... audio=... radio=...`. Jeśli któraś z nich mówi `FAIL`:

- dioda świeci na czerwono;
- wyjścia 12 V zostają wyłączone i nic nie jest wysyłane do radia;
- urządzenie **nie** restartuje się samo, więc radio i auto nie dostają impulsów ACC.

To stan bezpieczny programu (z kodu, `firmware/src/main.cpp`). Sprawdź moduł oznaczony `FAIL`:
`can` = kontroler CAN albo jego piny, `audio` = moduł wzmacniacza, `radio` = UART. Potem porównaj
piny w pliku twojej płytki (`firmware/boards/`) z twoim okablowaniem.

## Brak ramek CAN {#no-can-frames}

`stat` pokazuje `rx=0` i `bus=quiet`, dioda jest zgaszona.

| Sprawdź | Dlaczego |
|---|---|
| Czy auto jest wybudzone? | W aucie wzorcowym magistrala milczy przy wyjętym kluczyku. Po wyłączeniu przez około 100 s jest pełny ruch, potem przez 14 s sama ramka `036`, potem cisza aż do otwarcia auta (zmierzone (measured): nagranie na postoju). Brak ramek w uśpionym aucie jest normalny. |
| CANH i CANL na właściwych przewodach | W aucie wzorcowym: pin 10 Quadlocka = CANH, pin 13 = CANL (zmierzone: napięcia i odbiór bez błędów). W innym aucie najpierw zmierz: [Znajdź swoją magistralę](../make-it-yours/find-your-bus.md). |
| Prędkość w profilu | Program nie wykrywa prędkości magistrali. Profil `generic` używa domyślnie 125 kbit/s (`firmware/lib/car_generic/profile_generic.cpp`). Ustaw zmierzoną prędkość twojego auta. Pierwsza linia startowa pokazuje używaną prędkość. |
| Zworka nadawania w prototypie v1 | Widziane raz na płytce autora: w aucie, przy zamkniętej JP1 (linia TX do modułu CAN), płytka nie odbierała żadnych ramek; po otwarciu JP1 odbiór wrócił. Sprawdzenie ciągłości nie wykazało zwarcia między ścieżkami TX i RX. Przyczyna nieznana. Otwarta JP1 = nadawanie niemożliwe: do codziennego słuchania zostaw ją otwartą, zewrzyj tylko do celowego nadawania ([Nadawanie na magistralę CAN](../safety/transmit.md)). |
| Magistrala testowa | Urządzenie w trybie tylko do nasłuchu nie potwierdza ramek. Na stole potrzebny jest inny węzeł, który to robi. |

## Zgubione ramki albo błędy magistrali {#frames-lost-or-bus-errors}

`stat` pokazuje `lost` albo `errors` powyżej zera, albo konsola wypisuje linię w rodzaju
`[can] controller: state=... tec=N rec=N`.

- Stan kontrolera jest odczytywany raz na sekundę; linia pojawia się tylko przy zmianie. Dopóki
  stan nie jest „running”, dioda świeci na czerwono. Wyjścia 12 V dalej idą za zapłonem z
  ostatnich świeżych ramek (z kodu, `firmware/src/main.cpp`).
- Zapis do pamięci flash wstrzymuje odbiór CAN na kilka milisekund. Dlatego program odkłada zapis
  ustawień podczas cofania albo przy aktywnych czujnikach parkowania i dlatego aktualizacja
  programu jest w takich chwilach odrzucana albo przerywana.
- **Nagrywanie w panelu w przeglądarce** gubi ramki, gdy jego karta nie jest widoczna (zmierzone:
  dziesiątki tysięcy zgubionych ramek, gdy autor przełączył kartę w trakcie jednego nagrania).
  Zamiast tego użyj `firmware/tools/record.py`; oznacza przerwy jako `# lost N frames`.

Dla porównania: nagrania z auta wzorcowego zrobiono z zerem zgubionych ramek i zerem błędów
magistrali (zmierzone: sniffer w trybie tylko do nasłuchu i ten program, 125 kbit/s).

## Radio się nie włącza (brak ACC) {#the-radio-does-not-turn-on-no-acc}

| Sprawdź | Dlaczego |
|---|---|
| Które środowisko? | `listen` i `listen_wifi` nigdy nie przełączają wyjść 12 V. Tylko `decoder`, `decoder_wifi` i `decoder_tx_wifi` sterują ACC, podświetleniem i wstecznym. |
| Który profil? | Profil `generic` nigdy nie przełącza wyjść 12 V. |
| Ramki zapłonu | W trybie dekoder ACC idzie za ramką zapłonu z profilu (w aucie wzorcowym `0F6` bajt 1: `88` zapłon włączony, `8E` silnik pracuje). `stat` wypisuje `ignition=... acc=...`. |
| Czy skrzynka CAN jest nadal podłączona? | Nigdy nie uruchamiaj trybu dekoder z podłączoną skrzynką CAN: dwa źródła ACC na jednym przewodzie zwierają wyjścia. |

W aucie wzorcowym radio dostawało ACC ze skrzynki CAN, nie z instalacji auta: po odłączeniu
skrzynki przy pracującym silniku radio zapisało w logu zanik ACC (zmierzone: log radia). Jeśli
usuwasz skrzynkę, ACC musi dawać coś innego.

> [!NOTE]
> **Wyjścia 12 V prototypu v1**
>
> Nie ma osobnego raportu z testu na stole, w którym wyjścia ACC, ILL i REV się włączają.
> Sprawdź je [protokołem testu na stole](../testing/bench-test-protocol.md), zanim zaczniesz
> na nich polegać.

## Radio nie reaguje na przyciski albo nie pokazuje danych auta {#the-radio-does-not-react-to-buttons-or-shows-no-car-data}

| Sprawdź | Dlaczego |
|---|---|
| Zamienione przewody UART | W aucie wzorcowym, przy zamienionych dwóch przewodach danych, radio nie odbierało i nie odpowiadało (zmierzone: w aucie). Urządzenie nadaje na przewodzie, na którym nadawała skrzynka, a słucha na drugim. |
| Tryb | W `listen` wyjście UART nie ma pinu. `radio` pokazuje `transmit=no`; `key A5` odpowiada `not sent (listen mode)`. |
| Czy radio się odezwało? | `radio` wypisuje `the radio has not spoken yet`, gdy z radia nie przyszło nic poprawnego. Sprawdź przewód od radia, 38400 8N1, poziomy 3,3 V, wspólną masę. |
| Brak ACC | Bez ACC urządzenie celowo odrzuca ramki przycisków i czujników dla radia. `radio` liczy je pod `dropped without ACC`. |
| Profil radia | Radio musi mieć ustawiony protokół, którym mówi program (profil XP / Simple Soft dla radia wzorcowego). |

## Panel WWW się nie otwiera {#the-web-panel-does-not-open}

| Sprawdź | Dlaczego |
|---|---|
| Środowisko | Wi-Fi mają tylko środowiska `*_wifi`. `listen` i `decoder` nie zawierają kodu sieci. |
| Czy Wi-Fi jest teraz włączone? | Wi-Fi wyłącza się, gdy auto śpi. Zasady są pod tabelą. |
| Adres | `http://10.50.8.1`, zwykłe `http`, nie `https`. Nazwa `.local` może nie działać w każdym systemie; używaj adresu. |
| VPN na komputerze | Na komputerze autora VPN z „kill switch” blokował cały ruch do sieci urządzenia, a system pokazywał „połączono”. Rozłącz VPN albo zezwól na ruch w sieci lokalnej. |
| Komputer nie łączy się ponownie | Po wgraniu przez USB, które resetuje płytkę, komputer autora z Windows nie połączył się sam z siecią urządzenia. Połącz ręcznie. |
| Hasło | Odczytaj je na USB poleceniem `wifi showpass`. To klucz Wi-Fi i zarazem hasło panelu. |
| Wyłączone | `wifi off` jest zapamiętywane po restarcie. Po powtarzających się restartach po awarii sieć jest wstrzymana. `wifi` na USB mówi, który to przypadek; `wifi on` włącza ją ponownie. |
| W trakcie aktualizacji | Podczas zapisu pliku programu panel nie odpowiada na inne zapytania. |

Wi-Fi działa, gdy (z kodu, `firmware/lib/core/net_policy.h` i `firmware/src/config.h`):

- auto jest wybudzone (ramki na magistrali albo ACC radia), albo
- do USB jest podłączony komputer.

Gdy magistrala ucichnie, Wi-Fi zostaje włączone tylko dla zalogowanego klienta, który o coś pytał
w ostatnich 60 s. Trwa to najwyżej 10 minut i tylko wtedy, gdy zasilanie za diodą wejściową ma
co najmniej 11,0 V (ok. 11,8 V na akumulatorze), o ile płytka je mierzy. Po starcie przy cichej
magistrali pierwsze 60 s liczy się jak takie pytanie.

Z sieci domowej (po `wifi sta`) panel jest tylko do odczytu, dopóki nie wpiszesz na USB
`wifi lan on`. Przyciski nadawania działają tylko z własnej sieci urządzenia, nigdy przez sieć
domową.

## Pilot nie otwiera auta po postoju {#the-remote-key-does-not-unlock-after-parking}

W aucie wzorcowym zdarzało się to z jedną skrzynką CAN z rynku wtórnego, nie z inną i nie bez
skrzynki. Przyczyny nie znaleziono. Cała historia: [Zawieszony ton czujników parkowania](../case-studies/parking-tone-bug.md#the-remote-key).

Co wiadomo o tym urządzeniu:

- przechodzi w głębokie uśpienie 30 s po ucichnięciu magistrali, jeśli radio nie ma ACC, nic nie
  trzyma Wi-Fi i na USB nie ma komputera, a budzi się na pierwszą ramkę CAN (z kodu; **nie
  testowane na sprzęcie**);
- jego prąd w uśpieniu **nie jest zmierzony**.

Jeśli pilot przestaje działać przy zamontowanym urządzeniu: odłącz urządzenie, odstaw auto na tyle
samo czasu i spróbuj ponownie. Zapisz czas postoju, stan akumulatora i to, co było podłączone.

## Ton czujników parkowania nie milknie {#the-parking-tone-keeps-sounding}

Patrz [Zawieszony ton czujników parkowania](../case-studies/parking-tone-bug.md). Własny brzęczyk
urządzenia milknie 400 ms po ostatniej ramce czujników parkowania (test na komputerze
`beeper_scale`); opisany tam zawieszony ton pochodził z auta, nie z tego urządzenia.

Dalej: krótkie odpowiedzi na częste pytania, zobacz [Najczęstsze pytania](faq.md).
