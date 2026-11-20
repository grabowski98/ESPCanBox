# Zawieszony ton czujników parkowania

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

To historia usterki w aucie wzorcowym (Peugeot 508 SW, 2015) z radiem Android z rynku wtórnego i
jego skrzynką CAN. Opowiadamy ją razem ze ślepymi uliczkami, bo to one są najbardziej przydatne.
**Przyczyny nigdy nie znaleziono.** Znaleziono za to sposób wywołania usterki, to, co ją kasuje, i
to, że jeden z wczesnych wniosków był błędny.

Wszystkie obserwacje zrobił autor w aucie, chyba że podano nagranie. „Zmierzone” („measured”)
oznacza tu: zaobserwowane w aucie; większość prób powtórzono tylko kilka razy.

## Tło {#background}

Nowe radio nie pikało przy czujnikach parkowania. Żeby wróciły piknięcia, w dealerskim narzędziu
diagnostycznym ustawiono auto tak, by grało je przez moduł przełączników pod kierownicą. Od tego
czasu ton czujników czasem się zawieszał. Drugi problem, z pilotem, też był widoczny przy
zamontowanej skrzynce; kiedy się zaczął, nie zapisano.

## Objawy {#the-symptoms}

**Ton, który nie milknie.** Z przeszkodą przed autem włączenie albo wyłączenie zapłonu czasem
zostawiało grający ciągły ton czujników:

- wykres czujników w radiu znikał, ale ton zostawał;
- zostawał powyżej 10 km/h, przy wyłączonych czujnikach, przy wyłączonym zapłonie, przy
  zamkniętym aucie, przez około 10-15 minut;
- zapięcie pasa przerywało go na chwilę;
- odłączenie akumulatora go kończyło;
- same czujniki działały.

**Pilot.** Po 1-2 godzinach postoju pilot nie otwierał auta. Po otwarciu kluczem mechanicznym
znowu działał.

## Próba 1: wyjąć skrzynkę {#attempt-1-take-the-box-out}

Z odłączoną skrzynką CAN ton się nie zawieszał, a pilot działał (zmierzone: kilka prób). Wniosek
z tamtej chwili: **skrzynka powoduje oba problemy.**

Ten wniosek był pochopny. Opierał się na małej liczbie prób, a część z niego okazała się później
błędna (próba 6).

## Próba 2: pin czuwania {#attempt-2-the-standby-pin}

Skrzynka ma jeden transceiver CAN z wejściem czuwania (standby). Pomysł: może skrzynka wprowadza
swój transceiver w stan, który zakłóca magistralę. Pin czuwania odcięto od pola lutowniczego i
połączono z masą, więc transceiver zawsze zostaje w trybie normalnym. Patrz
[Wnętrze skrzynki CAN z rynku wtórnego](inside-a-can-box.md).

Wynik (zmierzone: po jednej próbie):

- ton dalej się zawieszał, powtarzalnie: przeszkoda z przodu, szybko wsteczny, zgaszenie silnika;
- teraz milkł około **1 minuty** po wyłączeniu zapłonu, a nie po 10-15 minutach;
- pilot dalej nie działał po 15 minutach postoju.

Przeróbka zmieniła długość zawieszonego tonu, a nie samą usterkę.

## Próba 3: odłączyć skrzynkę w trakcie zawieszonego tonu {#attempt-3-unplug-the-box-while-the-tone-is-stuck}

Ton zawieszony, zapłon wyłączony, skrzynka odłączona: **ton grał dalej.** Czyli skrzynka może
wywoływać usterkę, ale stan zawieszenia trzyma samo auto, nie skrzynka.

Później to samo przy pracującym silniku: odłączenie skrzynki też nie zatrzymało zawieszonego tonu.
Ton ucichł dopiero po ponownym podłączeniu skrzynki i ponownym uruchomieniu silnika.

## Próba 4: zawężanie wyzwalacza {#attempt-4-narrowing-down-the-trigger}

Aktualizacja programu skrzynki dała szczęśliwą obserwację. Gdy skrzynka siedziała w swoim
bootloaderze (poza magistralą), autor podjechał do przeszkody i ton się zawiesił **przy włączonym
zapłonie**. Wyłączenie nie było potrzebne.

Potem były kolejne próby. Większość wierszy poniżej to pojedyncze próby, więc pokazują, co może się
zdarzyć, a nie jak często:

| Próba | Wynik |
|---|---|
| Kilka podjazdów do przeszkody, przód w najbliższej strefie, zgaszenie silnika, uruchomienie silnika | zawieszony po ponownym uruchomieniu |
| Wyłączenie i włączenie z przeszkodą w strefie pikania **przerywanego** | nigdy się nie zawiesił |
| Skrzynka odłączona w trakcie pikania przerywanego | pikanie ucichło normalnie |
| Skrzynka odłączona w trakcie tonu **ciągłego**, potem odjazd | ton został |
| Skrzynka podłączona z powrotem, odjazd | ton ucichł |
| Skrzynka odłączona **przed** tonem, potem podjazd i odjazd | ton ucichł normalnie |
| Skrzynka podłączona, zgaszenie silnika w trakcie tonu ciągłego | raz ucichł od razu, raz został |
| Zgaszenie silnika w ciągu około 1 s od wejścia w ton ciągły | został |
| Poranne uruchomienie z przeszkodą z przodu (przypomnienie autora, nie planowana próba) | ton się włączył i został |

Co z tego wynikło:

- zawiesza się tylko ton **ciągły** (najbliższa strefa), nigdy pikanie przerywane;
- zmiana stanu zapłonu w trakcie tonu ciągłego może go wywołać, zarówno przy wyłączaniu, jak i
  przy uruchamianiu; nie za każdym razem, a reguły nie znaleziono;
- zniknięcie skrzynki z magistrali w trakcie tonu ciągłego też go wywołuje.

## Próba 5: co go kasuje {#attempt-5-what-clears-it}

Znaleziono dwa sposoby (zmierzone):

- **Nowy ton ciągły, który kończy się normalnie.** Przy zawieszonym tonie podjedź do najbliższej
  strefy na około 2 sekundy i odjedź: ton milknie. Powtórzone dwa razy w jednej sesji.
- **Czekanie.** W jednej próbie przy wyłączonym zapłonie ton ucichł sam po około 1-1,5 minuty, w
  chwili gdy auto wyłączyło akcesoria.

## Próba 6: ton zawiesił się bez żadnej skrzynki {#attempt-6-the-tone-got-stuck-without-any-box}

Skrzynka była odłączona od około godziny, a radio wyłączone. Autor podjechał do przeszkody,
odjechał, znowu podjechał do tonu ciągłego i szybko zgasił. **Ton został.** Ucichł sam po około
1 minucie.

To obaliło wniosek z próby 1 w części dotyczącej tonu. **Auto potrafi zawiesić ton samo**, bez
niczego podłączonego przy radiu. Czy skrzynka sprawia, że zdarza się to częściej, nie wiadomo:
prób bez skrzynki było za mało, żeby to stwierdzić.

## Próba 7: spojrzeć na magistralę {#attempt-7-look-at-the-bus}

Sniffer w trybie tylko do nasłuchu nagrał magistralę przy radiu podczas pięciu wyłączeń w trakcie tonu
ciągłego. W dwóch z nich ton został.

We **wszystkich pięciu**, w chwili wyłączenia, moduł czujników parkowania wysłał ramkę `0E1`
**bez** żądania dźwięku (`D8 00 3F FC FC FC C2 00`), także w dwóch próbach, w których ton został
(zmierzone: nagrania snifferem).

Czyli zawieszonego tonu nie żąda żadna ramka na tej magistrali. Trzyma go coś głębiej w aucie: BSI
(moduł nadwozia) albo moduł pod kierownicą. Według publicznego opisu (PSA-RE) BSI mówi temu
modułowi, jaki dźwięk zagrać, ramką `102` na innej magistrali, wysyłaną tylko przy zmianie dźwięku
(niezweryfikowane (unverified): PSA-RE `LS.CAR`). Ta magistrala nie dochodzi do złącza radia, więc
ten projekt jej nie widzi.

Ton został tylko w próbach, w których silnik pracował przed wyłączeniem dłużej niż 30 sekund. Pięć
prób to za mało, żeby nazwać to regułą.

## Pilot {#the-remote-key}

Problem z pilotem potoczył się inaczej:

- bez skrzynki pilot działał, także w dniu, w którym ton zawiesił się bez skrzynki;
- ze skrzynką i przeróbką pinu czuwania nie działał po 15 minutach postoju;
- ze starszym programem w tej samej skrzynce (przeróbka pinu czuwania nadal na miejscu) pilot
  działał po 16 minutach i po około 30 minutach postoju. Przez te 16 minut sniffer pokazywał, że magistrala i skrzynka całkowicie
  milczą (zmierzone: nagranie na postoju);
- z inną skrzynką CAN (Raise) pilot działał po każdym postoju, do 90 minut, a potem także po nocy.

W dniu, w którym pilot znowu zaczął działać, zmieniły się naraz trzy rzeczy: program skrzynki,
naładowanie akumulatora po dłuższej jeździe i sniffer na magistrali. Przypuszczenie autora, że
znaczenie miało naładowanie akumulatora, jest **niezweryfikowane**. Przyczyna nie jest znana.

## Na czym stoimy {#where-it-stands}

- **Przyczyna: nieznana**, zarówno dla tonu, jak i dla pilota.
- Ustawienie, które gra piknięcia czujników przez moduł pod kierownicą, zostało w narzędziu
  dealerskim z powrotem wyłączone. Czy w tym stanie ton nadal może się zawiesić, nie testowano.
- To urządzenie gra własne piknięcia czujników z ramki `0E1` przez własny głośnik. Jego brzęczyk
  milknie 400 ms po ostatniej ramce `0E1` (test na komputerze `beeper_scale`), więc nie może
  trzymać tonu, gdy auto ucichnie. Nie naprawia własnej usterki auta.

## Czego się nauczyliśmy {#what-we-learned}

- **Kilka prób to nie dowód.** „To przez skrzynkę” opierało się na garstce prób i było w połowie
  błędne.
- **Zmieniaj jedną rzecz naraz.** „Naprawa” pilota przyszła z trzema zmianami naraz, więc nikt nie
  wie, która pomogła.
- **Nagraj magistralę, zanim obwinisz ramkę.** Nagranie w jeden wieczór pokazało, że ton nie był
  żądany na magistrali przy radiu.
- **Zapisuj, czego nie przetestowałeś.** „Odłączenie skrzynki przy pracującym silniku” było luką w
  pierwszych notatkach i zmieniło obraz, gdy wreszcie to sprawdzono.
