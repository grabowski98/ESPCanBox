# Przegląd

![Logo projektu: wyświetlacz z kropek](../assets/brand/logo-transparent.svg)

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](contributing/translations.md#send-your-work).

Ten projekt to otwarte urządzenie na ESP32-S3, które pracuje między magistralą CAN auta a radiem
na Androidzie. Może zastąpić skrzynkę CAN („skrzynkę”) sprzedawaną razem z radiem.

> [!CAUTION]
> **Projekt hobbystyczny, udostępniony w obecnym stanie**
>
> Używasz go na własne ryzyko, we własnym aucie. Domyślnie urządzenie **tylko słucha**
> magistrali auta. Nadawanie wymaga osobnej kompilacji i ręcznego uzbrojenia. Nie używaj go do
> niczego, co wpływa na bezpieczeństwo. Zanim cokolwiek podłączysz, przeczytaj
> [Bezpieczeństwo](getting-started/safety.md).

> [!NOTE]
> **Bez powiązań**
>
> Projekt nie jest związany z firmami Peugeot, Stellantis, Simple Soft, Raise, Junsun ani FYT,
> nie jest przez nie wspierany ani z nimi połączony. Ich nazwy służą tylko do opisu tego, na czym
> go sprawdzono.

## Co robi urządzenie {#what-the-device-does}

- **Słucha magistrali CAN auta** w trybie tylko do nasłuchu, z wyciszonym transceiverem. Żadne
  środowisko kompilacji poza `decoder_tx_wifi` nie zawiera w ogóle kodu nadawania.
- **Pika przy czujnikach parkowania** z własnego głośnika, według ramki `0E1`.
- **Rozmawia z radiem** w trybie dekoder, przez UART, w protokole „XP” skrzynki Simple Soft:
  przyciski, wykres czujników parkowania, komputer pokładowy, temperatura zewnętrzna, stan
  klimatyzacji, drzwi i ustawienia pojazdu.
- **Steruje trzema wyjściami 12 V** w trybie dekoder: ACC (zasilanie radia), podświetlenie i bieg
  wsteczny.
- **Panel WWW** przez własną sieć Wi-Fi: stan, ramki CAN na żywo, konsola, nagrywanie
  i aktualizacja programu.
- **Konsola USB** ze stałym formatem linii (`can0 ID#DATA`, komentarze zaczynają się od `#`).

Jak te części do siebie pasują: [Jak to działa](getting-started/how-it-works.md).

[![Pulpit panelu WWW (makieta urządzenia, schemat ciemny)](../assets/screenshots/panel-dashboard.png)](../assets/screenshots/panel-dashboard.png)

## Co jest sprawdzone {#what-is-tested}

Program i płytka powstały i były mierzone w **jednym** aucie z **jednym** radiem:

| Auto | Profil | Stan |
|---|---|---|
| Peugeot 508 SW, 2015 (PSA AEE2010) | `psa_aee2010` | zmierzone (measured) w jednym aucie |
| dowolne auto | `generic` | tylko nasłuch: zapisuje ramki, nic nie wysyła do radia, wyjścia 12 V wyłączone |

W aucie wzorcowym magistrala przy radiu pracuje z prędkością 125 kbit/s (zmierzone: odbiór bez
błędów). Jej napięcia wyglądają na CAN low-speed fault-tolerant; potwierdzić to może dopiero
oscyloskop. Szczegóły: [Peugeot 508 SW](reference/vehicles/peugeot-508.md).

Drzewo programu w tym repozytorium zostało przeorganizowane z wersji, która pracuje w aucie
wzorcowym. Kompiluje się i przechodzi testy na komputerze, ale **nie zostało jeszcze sprawdzone
na stole** na urządzeniu.

## Od czego zacząć {#where-to-start}

Czytaj strony w tej kolejności:

1. [Bezpieczeństwo](getting-started/safety.md): co może pójść źle i zasady projektu.
2. [Czego potrzebujesz](getting-started/requirements.md): płytka, części i narzędzia.
3. [Szybki start](quick-start/flash.md): kompilacja, wgranie i otwarcie panelu na biurku, bez
   auta.
4. Potem jedna z dwóch ścieżek:
    - **Twoje auto nie jest autem wzorcowym:** [Dostosuj do siebie](make-it-yours/porting-checklist.md)
      pokazuje, jak znaleźć magistralę, nagrać ją, rozkodować ramki i napisać profil.
    - **Masz auto wzorcowe i to samo radio:** [Montaż w aucie](install-in-car/checklist.md).
5. [Udostępnij](make-it-yours/contribute.md) swoje nagrania albo opis swojego auta.

Nie masz jeszcze płytki? Makieta urządzenia uruchamia panel WWW na komputerze z nagraniem z auta,
zobacz [Pierwsze uruchomienie](quick-start/first-boot.md).

## Licencje {#licences}

Tymczasowe do pierwszego publicznego wydania: kod Apache-2.0, dokumentacja CC BY 4.0, sprzęt
CERN-OHL-S-2.0. Autor: grabowski98.
