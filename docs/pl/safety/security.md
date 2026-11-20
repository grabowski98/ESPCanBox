# Model zabezpieczeń

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona opisuje, co zakłada konstrukcja urządzenia i przed czym ono nie chroni. Przeczytaj ją, zanim
skompilujesz środowisko z Wi-Fi (`*_wifi`) albo z nadawaniem (`decoder_tx_wifi`). Wszystko, co tu
opisano, wynika z kodu, chyba że zaznaczono inaczej.

Jak zgłosić problem z bezpieczeństwem: `SECURITY.md` w głównym katalogu repozytorium.

## Magistrala auta {#the-car-bus}

- Domyślnie urządzenie **tylko słucha**. W każdym środowisku poza `decoder_tx_wifi` kod nadawania
  (`firmware/src/can_tx/`) w ogóle nie jest kompilowany. `firmware/tools/check_no_can_tx.sh`
  sprawdza, czy w skompilowanych obrazach jest funkcja nadawania.
- W `decoder_tx_wifi` urządzenie może wysłać tylko ramki z listy dozwolonych profilu auta
  (`firmware/lib/car_psa_aee2010/tx_allowlist.cpp`). Domyślnie jest to ramka `15B`, sprawdzana
  **wyłącznie po identyfikatorze i długości, nie po treści**. `1A9` jest na liście tylko
  w kompilacji z `-DCAN_TX_EXPERIMENTAL=1`. Lista to stałe dane w pamięci flash; nic nie może jej
  rozszerzyć, gdy urządzenie pracuje.
- Nadawanie wymaga też uzbrojenia (`cantx on`) i restartu. Brama wysyła po jednej ramce, co
  najmniej 500 ms jedna od drugiej, najwyżej 10 na minutę, i trzyma nadajnik włączony najwyżej
  20 ms. Szczegóły: [Nadawanie na magistralę CAN](transmit.md).
- Środowisko z nadawaniem nie kompiluje się z profilem `generic` ani z pustą listą.

> [!CAUTION]
> **Przy zwartej JP1 jedyną barierą jest program**
>
> Otwarta JP1 = nadawanie niemożliwe: zworka JP1 w linii TX modułu CAN prototypu daje sprzętową
> gwarancję trybu tylko do nasłuchu. Do codziennej pracy zostaw ją otwartą. Zewrzyj ją tylko po
> to, żeby celowo nadawać z `decoder_tx_wifi` ([Nadawanie na magistralę CAN](transmit.md#switch-transmitting-on-step-by-step)).
> Przy zwartej JP1 decyduje wgrany program. W prototypie autora JP1 jest zwarta.

## Wi-Fi i panel WWW {#wi-fi-and-the-web-panel}

- Urządzenie tworzy własną sieć Wi-Fi (WPA2). Jej hasło jest też hasłem panelu: kto je zna, może
  się zalogować.
- Przy pierwszym starcie urządzenie tworzy losowe, 12-znakowe hasło. Hasło, które ustawiasz sam
  (`wifi pass`), ma od 10 do 63 drukowalnych znaków, bez spacji. Hasło jest pokazywane tylko na
  konsoli USB (`wifi showpass`). `wifi newpass` tworzy nowe losowe hasło.
- Panel używa **zwykłego HTTP, bez TLS**. We własnej sieci urządzenia jedyną ochroną ruchu jest
  WPA2. W sieci domowej (`wifi sta`) każdy w tej sieci może odczytać ruch, łącznie z danymi logowania.
- Logowanie to HTTP Basic, a hasło jest porównywane w stałym czasie. Dla klienta w sieci domowej
  5 błędnych haseł w ciągu 60 s blokuje logowanie na 30 s; każda kolejna blokada podwaja przerwę,
  do 5 minut. Klienci we własnej sieci urządzenia nie są blokowani.
- Przeglądarka trzyma hasło jawnym tekstem, bo panel dołącza je do każdego zapytania. Domyślnie
  jest w `sessionStorage`: tylko ta karta, znika po jej zamknięciu. Z zaznaczonym przy logowaniu
  **Zapamiętaj na tym urządzeniu** jest w `localStorage`: każda karta tej przeglądarki, zostaje po
  jej ponownym uruchomieniu, może je odczytać każdy, kto ma dostęp do tego profilu przeglądarki
  albo jego plików. **Wyloguj** usuwa je z obu miejsc; odrzucone hasło (`401`) też jest usuwane.
  Zaznaczaj to pole tylko na własnym urządzeniu.
- Z sieci domowej panel jest **tylko do odczytu**, chyba że wpiszesz `wifi lan on` w konsoli USB.
- Polecenia, które uzbrajają albo rozbrajają nadawanie (`cantx on`, `cantx off`, `vset on NN`,
  `vset off NN`), panel przyjmuje **tylko** od klienta we własnej sieci urządzenia, nawet po
  `wifi lan on`. Polecenia `wifi` (zawierają hasła), `cantx reset` i `set NN VV` działają tylko
  w konsoli USB.
- Ramka CAN wpisana do konsoli (`0E1#...`) trafia tylko do logiki samego urządzenia, tak jakby
  została odebrana. Nigdy nie jest wysyłana do auta.

## Aktualizacja programu przez Wi-Fi {#firmware-update-over-wi-fi}

> [!WARNING]
> **Obrazy aktualizacji nie są podpisane**
>
> Każdy, kto może się zalogować do panelu, może wgrać dowolny program, także taki, który nadaje.
> Sprawdzenia przed aktualizacją (nazwa produktu i znacznik w obrazie) chronią przed pomyłkami,
> nie przed atakami.

- Obraz z innym trybem pracy albo inną zdolnością nadawania na CAN niż działający program wymaga
  osobnego potwierdzenia w panelu (API najpierw odpowiada `409 mode_change`). To pytanie
  dla bezpieczeństwa, a nie podpis.
- Uzbrojenie jest zapisane razem z napisem kompilacji programu, więc każdy inny program startuje
  rozbrojony.

API aktualizacji: [API panelu](../reference/panel-api.md).

## Czego to nie obejmuje {#what-is-not-covered}

- **Fizycznego dostępu.** Każdy z kablem USB może odczytać hasło i wgrać dowolny program.
- **Radia i auta.** Ten projekt nie czyni ich ani bardziej, ani mniej bezpiecznymi i nie chroni
  auta przed innymi urządzeniami na jego magistrali.
- **Secure boot i szyfrowanie pamięci flash** układu ESP32-S3 nie są używane.

## Zanim cokolwiek udostępnisz {#before-you-share-anything}

Nagrania magistrali auta zawierają VIN (ramki `336`, `3B6` i `2B6` w aucie wzorcowym) i pokazują,
kiedy i jak auto jeździło. Zanim udostępnisz nagranie, oczyść je. Wpisz to w głównym katalogu
repozytorium (nie w `firmware/`):

```
python -I tools/sanitize_capture.py IN.log OUT.log
```

Oczekiwane: linia `IN.log -> OUT.log: ... frames overwritten ...` i nowy plik `OUT.log`.
Potem `python -I tools/sanitize_capture.py --check OUT.log` musi wypisać `0 problem(s)`.

Nigdy nie publikuj linii `[wifi] password:` z konsoli.

Dalej: [Udostępnij swoje auto](../make-it-yours/contribute.md) mówi, jak udostępnić nagranie.
