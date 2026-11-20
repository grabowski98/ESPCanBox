# Przegląd panelu

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona pokazuje panel WWW ekran po ekranie. Każdy obrazek to prawdziwy stan panelu, a tekst pod
nim mówi, co widać i dlaczego to ważne. Co robi każdy przycisk, opisuje
[Przewodnik po panelu WWW](panel-guide.md); zapytania za kartami opisuje [API panelu](panel-api.md).

Wszystkie obrazki pochodzą z makiety urządzenia (`firmware/tools/mock_device.py`) na komputerze,
nigdy z auta. Stany auta to krótkie syntetyczne nagrania ramek, które urządzenie dekoduje; zakładka
CAN odtwarza zanonimizowane nagranie z `firmware/test_data/captures/`. Obrazki oznaczone „radio,
które odpowiada” używają opcji makiety `--demo-radio`: pola radia mają przykładowe wartości z
[API panelu](panel-api.md). Makieta działa też z `--screenshot`: jedna stała data zbudowania i skrót,
nazwa sieci `ESPCanBox-A1B2`, linie konsoli takie, jakie wypisuje program, i napięcie 12 V pasujące
do stanu auta (13,42 V przy pracującym silniku, 11,63 V przy włączonym zapłonie; obrazek ze stołu
nie ma napięcia, jak przy zasilaniu z USB). Kliknij obrazek, żeby otworzyć go w pełnym rozmiarze.
Obrazki z polskim panelem są tam, gdzie je zrobiono; pozostałe pokazują panel po angielsku. Konsola
urządzenia, daty zbudowania i komunikaty programu są po angielsku także w polskim panelu.

## Logowanie

[![Ekran logowania z wyborem języka](../../assets/screenshots/pl/panel-login.png)](../../assets/screenshots/pl/panel-login.png)

Pierwszy ekran. Hasło to klucz sieci Wi-Fi urządzenia; język można wybrać przed zalogowaniem.

[![Logowanie odrzucone: złe hasło](../../assets/screenshots/panel-login-wrong-password.png)](../../assets/screenshots/panel-login-wrong-password.png)

Złe hasło daje czerwoną linię pod przyciskiem. Pole hasła jest czyszczone do następnej próby.

[![Logowanie z zaznaczonym „Zapamiętaj na tym urządzeniu”](../../assets/screenshots/panel-login-remember.png)](../../assets/screenshots/panel-login-remember.png)

„Zapamiętaj na tym urządzeniu” zapisuje hasło w tej przeglądarce, więc telefon w aucie nie pyta za
każdym razem. Zaznaczaj tylko na własnym urządzeniu: panel działa po nieszyfrowanym HTTP.

[![Blokada logowania po pięciu złych hasłach z sieci domowej](../../assets/screenshots/panel-login-locked-out.png)](../../assets/screenshots/panel-login-locked-out.png)

Z sieci domowej pięć różnych złych haseł w ciągu minuty blokuje logowanie. Czerwony pasek odlicza
czas; każda kolejna seria wydłuża blokadę.

## Pulpit i trzy rodzaje programu

[![Pulpit, program tylko do nasłuchu, silnik pracuje, przednie czujniki pikają](../../assets/screenshots/pl/panel-dashboard.png)](../../assets/screenshots/pl/panel-dashboard.png)

Ekran główny programu tylko do nasłuchu. Nagłówek pokazuje tryb („obok skrzynki”) i „CAN: tylko
nasłuch”: ten program nie może nic wysłać na magistralę auta.

[![Pulpit w jasnym schemacie](../../assets/screenshots/panel-dashboard-light.png)](../../assets/screenshots/panel-dashboard-light.png)

Panel idzie za jasnym albo ciemnym schematem telefonu lub komputera.

[![Pulpit programu dekodera](../../assets/screenshots/panel-dashboard-decoder.png)](../../assets/screenshots/panel-dashboard-decoder.png)

W programie dekodera znaczek trybu jest pomarańczowy: urządzenie samo rozmawia z radiem i zastępuje
fabryczną skrzynkę CAN.

[![Program dekodera z radiem, które odpowiada: wypełniona karta Radio](../../assets/screenshots/panel-dashboard-radio-linked.png)](../../assets/screenshots/panel-dashboard-radio-linked.png)

Gdy radio odpowiada, karta Radio pokazuje włączenie, głośność, źródło i zegar (radio, które
odpowiada).

## Stany auta

[![Brak ruchu na CAN: każda wartość auta to „brak ramki”, kropka magistrali jest żółta](../../assets/screenshots/panel-dashboard-bus-silent.png)](../../assets/screenshots/panel-dashboard-bus-silent.png)

Na magistrali nic: kropka obok „Auto” robi się żółta (bursztynowa), a każda wartość auta to „brak
ramki”. Tak wygląda panel na stole, przed podłączeniem przewodów albo przy uśpionym aucie; przy
zasilaniu z USB wiersz 12 V to mówi.

[![Zapłon wyłączony: ACC zgasło, auto dalej nadaje na magistrali](../../assets/screenshots/panel-dashboard-ignition-off.png)](../../assets/screenshots/panel-dashboard-ignition-off.png)

Zapłon wyłączony: bajt zapłonu `80`, ACC gaśnie 2,5 s później. Magistrala nie zasypia od razu po
wyjęciu kluczyka, więc kropka jest dalej zielona.

[![Zapłon włączony, silnik nie pracuje](../../assets/screenshots/panel-dashboard-ignition-on.png)](../../assets/screenshots/panel-dashboard-ignition-on.png)

Zapłon bez silnika: bajt `88`, ACC włączone. Z tego stanu radio dostaje ACC. Wiersz 12 V pokazuje
akumulator za diodą urządzenia.

[![Silnik pracuje, czujniki czekają](../../assets/screenshots/panel-dashboard-engine-running.png)](../../assets/screenshots/panel-dashboard-engine-running.png)

Silnik pracuje: bajt `8E`, napis „silnik pracuje”, a napięcie 12 V rośnie od alternatora.

[![Światła włączone](../../assets/screenshots/panel-dashboard-lights-on.png)](../../assets/screenshots/panel-dashboard-lights-on.png)

Światła: znacznik Światła idzie za ramką oświetlenia `036`, gdy zapłon jest włączony.

[![Otwarte drzwi kierowcy i bagażnik](../../assets/screenshots/panel-dashboard-doors-open.png)](../../assets/screenshots/panel-dashboard-doors-open.png)

Otwarte drzwi są niebieskie na widoku z góry i wypisane z nazwy.

[![Wiersz Ustawienia pojazdu: ostatnia komenda radia odrzucona przez program bez nadawania](../../assets/screenshots/panel-dashboard-vehicle-settings.png)](../../assets/screenshots/panel-dashboard-vehicle-settings.png)

Wiersz Ustawienia pojazdu pokazuje ostatnią ramkę `260`, stan wysłany do radia i ostatnie
ustawienie, o które prosiło radio. Program bez nadawania na CAN je odrzuca i to mówi (radio, które
odpowiada).

## Czujniki parkowania

[![Wsteczny: tylne czujniki aktywne i pikają](../../assets/screenshots/panel-dashboard-park-rear.png)](../../assets/screenshots/panel-dashboard-park-rear.png)

Wsteczny: tylne strefy świecą od daleko (zielony) do blisko (żółty), a urządzenie pika w odstępie,
o który prosi auto.

[![Przednie i tylne czujniki aktywne jednocześnie](../../assets/screenshots/pl/panel-dashboard-park-front-rear.png)](../../assets/screenshots/pl/panel-dashboard-park-front-rear.png)

Przód i tył aktywne razem; dźwięk idzie za stroną, którą auto podaje w ramce.

[![Przeszkoda bardzo blisko: ton ciągły](../../assets/screenshots/panel-dashboard-park-continuous.png)](../../assets/screenshots/panel-dashboard-park-continuous.png)

Strefa 0 jest czerwona, a odstęp 0 oznacza ton ciągły.

[![Czujniki parkowania wyłączone przyciskiem w aucie](../../assets/screenshots/panel-dashboard-park-off.png)](../../assets/screenshots/panel-dashboard-park-off.png)

Czujniki wyłączone w aucie: obie strony „wyłączone”, urządzenie milczy.

[![Próbne pikanie z karty czujników](../../assets/screenshots/panel-dashboard-test-beep.png)](../../assets/screenshots/panel-dashboard-test-beep.png)

Przycisk próby pika przez 1,5 s, więc głośność da się ustawić bez przeszkody.

[![Głośność poniżej 15 %: tylko do restartu](../../assets/screenshots/panel-dashboard-volume-low.png)](../../assets/screenshots/panel-dashboard-volume-low.png)

Poniżej 15 % głośność nie jest zapisywana: zbyt cicha wartość zapisana przez pomyłkę wyciszyłaby
czujniki.

## Nadawanie na CAN (decoder_tx_wifi)

[![Program z nadawaniem na CAN, rozbrojony](../../assets/screenshots/pl/panel-dashboard-decoder-tx.png)](../../assets/screenshots/pl/panel-dashboard-decoder-tx.png)

Tylko ten program może nadawać na magistralę auta i tylko po uzbrojeniu. Karta Magistrala CAN
pokazuje ostrzeżenie, stan bramki i dwa przyciski.

[![Uzbrajanie nadawania: okno z ostrzeżeniami](../../assets/screenshots/pl/panel-dashboard-cantx-arm-dialog.png)](../../assets/screenshots/pl/panel-dashboard-cantx-arm-dialog.png)

Uzbrojenie pyta jeszcze raz i mówi, co będzie wysyłane, jak to sprawdzono i jak rozbroić.

[![Uzbrojenie zapisane: działa po restarcie](../../assets/screenshots/panel-dashboard-cantx-arm-stored.png)](../../assets/screenshots/panel-dashboard-cantx-arm-stored.png)

Uzbrojenie jest zapisane, ale urządzenie słucha dalej aż do restartu: wiersz mówi „po restarcie:
uzbrojone”.

[![Uzbrojone po restarcie](../../assets/screenshots/panel-dashboard-cantx-armed.png)](../../assets/screenshots/panel-dashboard-cantx-armed.png)

Po restarcie znaczek w nagłówku mówi „CAN: uzbrojone”, a bramka jest gotowa.

[![Program z nadawaniem otwarty z sieci domowej](../../assets/screenshots/panel-dashboard-cantx-home-network.png)](../../assets/screenshots/panel-dashboard-cantx-home-network.png)

Z sieci domowej przyciski uzbrajania są nieczynne: uzbroić można tylko z własnej sieci Wi-Fi
urządzenia albo przez USB.

## Zakładka CAN

[![Zakładka CAN: tabela ramek i podgląd nagrania na żywo](../../assets/screenshots/pl/panel-can.png)](../../assets/screenshots/pl/panel-can.png)

Jeden wiersz na identyfikator: okres, liczba, wiek i ostatnie dane; zmienione bajty migają na żółto.

[![Zakładka CAN z filtrem jednego identyfikatora](../../assets/screenshots/panel-can-filter.png)](../../assets/screenshots/panel-can-filter.png)

Wpisz identyfikator albo dotknij wiersza, a podgląd pokaże tylko tę ramkę.

[![Podgląd CAN wstrzymany; ramki z numerem VIN pokazują kropki](../../assets/screenshots/panel-can-paused.png)](../../assets/screenshots/panel-can-paused.png)

Pauza zatrzymuje tabelę i podgląd, żeby dało się je przeczytać; przycisk zmienia się na Wznów. Ramki
z numerem VIN (`336` w podglądzie) pokazują kropki, dopóki zaznaczone jest „maskuj VIN”.

[![Nagrywanie ramek CAN w przeglądarce](../../assets/screenshots/panel-can-recording.png)](../../assets/screenshots/panel-can-recording.png)

Nagrywanie zbiera ramki w przeglądarce i zapisuje je jako plik `.log` w formacie sniffera.

## Zakładka Radio

[![Zakładka Radio w programie tylko do nasłuchu](../../assets/screenshots/pl/panel-radio.png)](../../assets/screenshots/pl/panel-radio.png)

Obok fabrycznej skrzynki urządzenie tylko słucha linii radia, więc pola radia są puste, a wysyłanie
jest wyłączone.

[![Zakładka Radio programu dekodera z radiem, które odpowiada](../../assets/screenshots/panel-radio-decoder.png)](../../assets/screenshots/panel-radio-decoder.png)

W programie dekodera liczniki łącza UART pokazują ramki w obie strony i potwierdzenia każdej
strony. Złe sumy, zgubione ramki i pełne miejsca są czerwone: tu dwie złe sumy (radio, które
odpowiada).

[![Próba przycisku w programie tylko do nasłuchu](../../assets/screenshots/panel-radio-key-test-beside.png)](../../assets/screenshots/panel-radio-key-test-beside.png)

Próba przycisku wysyła kod do radia; obok skrzynki trafia on tylko do konsoli.

[![Próba przycisku w programie dekodera: wysłano do radia](../../assets/screenshots/panel-radio-key-test-decoder.png)](../../assets/screenshots/panel-radio-key-test-decoder.png)

W programie dekodera ten sam przycisk idzie do radia (radio, które odpowiada).

[![Edytor zamiany przycisków z ostatnio naciśniętym przyciskiem](../../assets/screenshots/panel-radio-keymap.png)](../../assets/screenshots/panel-radio-keymap.png)

Tabela zamian podmienia przycisk auta na inną funkcję radia. „Dodaj do tabeli” bierze ostatnio
naciśnięty przycisk (radio, które odpowiada).

[![Edytor zamiany przycisków z nowym, niezapisanym wierszem](../../assets/screenshots/panel-radio-keymap-edit.png)](../../assets/screenshots/panel-radio-keymap-edit.png)

Zmieniona tabela ma znacznik „niezapisane zmiany” aż do naciśnięcia Zapisz.

[![Powrót do wbudowanej tabeli: potwierdzenie](../../assets/screenshots/panel-radio-keymap-reset-dialog.png)](../../assets/screenshots/panel-radio-keymap-reset-dialog.png)

Powrót do wbudowanej tabeli najpierw pyta.

[![Tabela przycisków zapisana w urządzeniu](../../assets/screenshots/panel-radio-keymap-saved.png)](../../assets/screenshots/panel-radio-keymap-saved.png)

Zapis trafia do pamięci, gdy auto nie potrzebuje urządzenia.

## Konsola

[![Konsola po poleceniach ver i stat](../../assets/screenshots/pl/panel-console.png)](../../assets/screenshots/pl/panel-console.png)

Ta sama konsola co na USB, z przyciskami częstych poleceń. Linie mają czas urządzenia w sekundach.

[![Nieznane polecenie: konsola wypisuje listę poleceń](../../assets/screenshots/panel-console-unknown-command.png)](../../assets/screenshots/panel-console-unknown-command.png)

Nieznane polecenie wypisuje listę poleceń, które panel przyjmuje. Linia w panelu ma najwyżej 200
znaków, więc koniec listy (przykładowa ramka) jest tu ucięty; konsola USB pokazuje całość.

[![Polecenie tylko z konsoli USB](../../assets/screenshots/panel-console-usb-only.png)](../../assets/screenshots/panel-console-usb-only.png)

Niektóre polecenia, np. `set NN VV`, działają tylko z USB; panel to mówi zamiast „złe dane”.

[![Polecenia wifi odrzucone w panelu](../../assets/screenshots/panel-console-wifi-refused.png)](../../assets/screenshots/panel-console-wifi-refused.png)

Polecenia `wifi` dotyczą haseł, więc panel ich w ogóle nie wysyła.

[![Polecenie cantx w programie z nadawaniem](../../assets/screenshots/panel-console-cantx-status.png)](../../assets/screenshots/panel-console-cantx-status.png)

`cantx` wypisuje stan bramki: wbudowana, uzbrojona, tryb kontrolera, włączone pozycje i liczniki.

[![Pozycja ustawień pojazdu odrzucana na stałe przez program](../../assets/screenshots/panel-console-vset-refused.png)](../../assets/screenshots/panel-console-vset-refused.png)

Włączyć można tylko pozycje ze zmierzoną ramką; inne odrzuca sam program.

[![Konsola z filtrem linii czujników parkowania](../../assets/screenshots/panel-console-filter.png)](../../assets/screenshots/panel-console-filter.png)

Filtr pokazuje tylko linie z danym tekstem, tu raporty czujników parkowania.

[![Z sieci domowej panel tylko czyta](../../assets/screenshots/panel-console-home-network-read-only.png)](../../assets/screenshots/panel-console-home-network-read-only.png)

Z sieci domowej, bez `wifi lan on`, każde polecenie jest odrzucane (tu `vol 50`), a żółty pasek
mówi dlaczego.

## Zakładka System

[![Zakładka System](../../assets/screenshots/pl/panel-system.png)](../../assets/screenshots/pl/panel-system.png)

U góry wersje, obciążenie, pamięć i Wi-Fi, niżej sieć domowa i hasło. Kopia ustawień i
aktualizacja programu są dalej i mają na tej stronie własne rozdziały.

[![Formularze sieci domowej i hasła urządzenia](../../assets/screenshots/pl/panel-system-network-password.png)](../../assets/screenshots/pl/panel-system-network-password.png)

Oba formularze w pełnym rozmiarze: sieć domowa (nieobowiązkowa) i hasło urządzenia.

[![Zakładka System w jasnym schemacie](../../assets/screenshots/panel-system-light.png)](../../assets/screenshots/panel-system-light.png)

Ta sama zakładka w jasnym schemacie.

[![Karty Urządzenie, Obciążenie i Wi-Fi](../../assets/screenshots/panel-system-device-cards.png)](../../assets/screenshots/panel-system-device-cards.png)

Wolną pamięć i zapas stosów warto sprawdzić po aktualizacji albo przy nowej funkcji.

[![Otwarte z sieci domowej po „wifi lan on”: zmiany dozwolone](../../assets/screenshots/panel-system-home-network-admin.png)](../../assets/screenshots/panel-system-home-network-admin.png)

Po `wifi lan on` z USB karta Wi-Fi mówi, że zmiany z sieci domowej są dozwolone.

[![Formularz sieci domowej: panel sprawdza nazwę przed wysłaniem](../../assets/screenshots/panel-system-home-network-error.png)](../../assets/screenshots/panel-system-home-network-error.png)

Panel sprawdza formularz sieci domowej, zanim cokolwiek wyśle; ostrzeżenie nad nim tłumaczy,
dlaczego sieć domowa jest mniej bezpieczna.

[![Zmiana hasła: dwa nowe hasła się różnią](../../assets/screenshots/panel-system-password-mismatch.png)](../../assets/screenshots/panel-system-password-mismatch.png)

Nowe hasło trzeba wpisać dwa razy.

[![Zmiana hasła: potwierdzenie](../../assets/screenshots/panel-system-password-dialog.png)](../../assets/screenshots/panel-system-password-dialog.png)

Hasło jest też kluczem Wi-Fi, więc okno ostrzega, że każde urządzenie musi połączyć się na nowo.

[![Hasło zmienione](../../assets/screenshots/panel-system-password-changed.png)](../../assets/screenshots/panel-system-password-changed.png)

Po zmianie sieć urządzenia startuje od nowa z nowym kluczem.

[![Wybór języka z sześcioma językami panelu](../../assets/screenshots/panel-system-language-list.png)](../../assets/screenshots/panel-system-language-list.png)

Sześć języków; wybór jest zapisany w urządzeniu, więc każdy telefon dostaje ten sam. Na obrazku lista
jest rozwinięta.

[![Panel po niemiecku, z uwagą o tłumaczeniu maszynowym](../../assets/screenshots/panel-system-language-de.png)](../../assets/screenshots/panel-system-language-de.png)

Język tłumaczony maszynowo pokazuje uwagę na dole zakładki System.

## Kopia ustawień

[![Karta Kopia ustawień](../../assets/screenshots/pl/panel-system-settings-backup.png)](../../assets/screenshots/pl/panel-system-settings-backup.png)

Plik kopii ma język, głośność pikania i tabelę przycisków, nigdy hasła ani sieci.

[![Ustawienia zapisane do pliku](../../assets/screenshots/panel-system-backup-exported.png)](../../assets/screenshots/panel-system-backup-exported.png)

Zapis do pliku nadaje plikowi nazwę z datą i godziną.

[![Wczytanie pliku ustawień: potwierdzenie](../../assets/screenshots/panel-system-backup-import-dialog.png)](../../assets/screenshots/panel-system-backup-import-dialog.png)

Wczytanie pliku najpierw pyta i mówi, które pola zostaną zastąpione.

[![Ustawienia wczytane](../../assets/screenshots/panel-system-backup-imported.png)](../../assets/screenshots/panel-system-backup-imported.png)

Urządzenie zapisuje wartości, gdy auto go nie potrzebuje.

[![Wczytanie odrzucone przez urządzenie](../../assets/screenshots/panel-system-backup-import-rejected.png)](../../assets/screenshots/panel-system-backup-import-rejected.png)

Urządzenie sprawdza cały plik, zanim cokolwiek zmieni; tu głośność jest niższa od najniższej
zapisywanej.

[![Wczytanie pliku, który nie jest kopią ustawień](../../assets/screenshots/panel-system-backup-import-bad-file.png)](../../assets/screenshots/panel-system-backup-import-bad-file.png)

Plik, który nie jest obiektem JSON, odrzuca już przeglądarka.

## Aktualizacja programu

[![Kreator aktualizacji przed wyborem pliku](../../assets/screenshots/panel-ota-empty.png)](../../assets/screenshots/panel-ota-empty.png)

Trzy kroki: wybierz plik, pozwól panelowi go sprawdzić, wgraj i poczekaj. Góra karty pokazuje oba
miejsca na program.

[![Krok 2: plik nie jest obrazem programu](../../assets/screenshots/panel-ota-not-firmware.png)](../../assets/screenshots/panel-ota-not-firmware.png)

Plik bez nagłówka obrazu ESP32 zatrzymuje się w kroku 2, więc `-merged.bin` ani zdjęcie nie trafią
do urządzenia.

[![Krok 2: program innego produktu](../../assets/screenshots/panel-ota-wrong-product.png)](../../assets/screenshots/panel-ota-wrong-product.png)

Obraz ESP32 bez znacznika tego projektu też jest odrzucany.

[![Krok 2: plik za krótki](../../assets/screenshots/panel-ota-too-short.png)](../../assets/screenshots/panel-ota-too-short.png)

Przerwane pobieranie daje za krótki plik i jest odrzucane.

[![Kreator: plik sprawdzony, ten sam tryb, gotowy do wysłania](../../assets/screenshots/panel-ota-ready.png)](../../assets/screenshots/panel-ota-ready.png)

Plik tego samego rodzaju co działający przechodzi od razu do kroku 3.

[![Kreator: plik dekodera dla urządzenia do nasłuchu wymaga zaznaczenia](../../assets/screenshots/pl/panel-system-firmware-wizard.png)](../../assets/screenshots/pl/panel-system-firmware-wizard.png)

Inny tryb pracy jest zaznaczony na czerwono i wymaga osobnego potwierdzenia, zanim przycisk się
odblokuje.

[![Kreator: plik z nadawaniem na CAN wymaga osobnego zaznaczenia](../../assets/screenshots/panel-ota-cantx-ack.png)](../../assets/screenshots/panel-ota-cantx-ack.png)

Tak samo dla pliku, który może nadawać na magistralę auta.

[![Kreator: plik bez panelu sieciowego](../../assets/screenshots/panel-ota-nonet-ack.png)](../../assets/screenshots/panel-ota-nonet-ack.png)

Plik bez panelu sieciowego oznacza, że następna aktualizacja wymaga kabla USB.

[![Krok 3: ostatnie potwierdzenie przed wysłaniem](../../assets/screenshots/panel-ota-confirm-dialog.png)](../../assets/screenshots/panel-ota-confirm-dialog.png)

Ostatnie pytanie powtarza wersję i tryb i prosi o wgrywanie na postoju.

[![Nowy program działa i czeka na potwierdzenie](../../assets/screenshots/panel-ota-pending.png)](../../assets/screenshots/panel-ota-pending.png)

Po restarcie nowy program działa próbnie; sam się potwierdza po około 20 s pracy.

[![Aktualizacja zakończona i potwierdzona; dostępny powrót](../../assets/screenshots/panel-ota-done.png)](../../assets/screenshots/panel-ota-done.png)

Gotowe: drugie miejsce ma teraz poprzedni program, a „Wróć do poprzedniego” jest czynne.

[![Powrót do poprzedniego programu: potwierdzenie](../../assets/screenshots/panel-ota-rollback-dialog.png)](../../assets/screenshots/panel-ota-rollback-dialog.png)

Powrót podaje wersję, datę zbudowania i skrót programu, który uruchomi.

## Restart i utrata połączenia

[![Restart urządzenia: potwierdzenie](../../assets/screenshots/panel-system-restart-dialog.png)](../../assets/screenshots/panel-system-restart-dialog.png)

W programie dekodera restart na chwilę zabiera radiu ACC, więc panel najpierw pyta.

[![Restart zaplanowany: panel czeka na urządzenie](../../assets/screenshots/panel-system-restart-pending.png)](../../assets/screenshots/panel-system-restart-pending.png)

Panel liczy sekundy, aż urządzenie znów odpowie.

[![Urządzenie wróciło po restarcie: konsola to zaznacza](../../assets/screenshots/panel-system-restarted.png)](../../assets/screenshots/panel-system-restarted.png)

Wróciło: konsola zaznacza restart osobną linią, a po niej idą linie startowe nowego uruchomienia z
czasem urządzenia od zera.

[![Brak połączenia z urządzeniem](../../assets/screenshots/pl/panel-offline.png)](../../assets/screenshots/pl/panel-offline.png)

Gdy urządzenie przestaje odpowiadać, pojawia się czerwony pasek, a panel trzyma ostatnie wartości do
powrotu połączenia.

## Na telefonie i tablecie

[![Pulpit na telefonie](../../assets/screenshots/pl/panel-phone-dashboard.png)](../../assets/screenshots/pl/panel-phone-dashboard.png) [![Ton ciągły na telefonie](../../assets/screenshots/panel-phone-dashboard-park-continuous.png)](../../assets/screenshots/panel-phone-dashboard-park-continuous.png)

[![Zakładka CAN na telefonie](../../assets/screenshots/pl/panel-phone-can.png)](../../assets/screenshots/pl/panel-phone-can.png) [![Zakładka Radio na telefonie](../../assets/screenshots/pl/panel-phone-radio.png)](../../assets/screenshots/pl/panel-phone-radio.png)

[![Konsola na telefonie](../../assets/screenshots/pl/panel-phone-console.png)](../../assets/screenshots/pl/panel-phone-console.png) [![Zakładka System na telefonie](../../assets/screenshots/pl/panel-phone-system.png)](../../assets/screenshots/pl/panel-phone-system.png)

[![Uzbrajanie na telefonie](../../assets/screenshots/pl/panel-phone-dashboard-cantx-arm-dialog.png)](../../assets/screenshots/pl/panel-phone-dashboard-cantx-arm-dialog.png) [![Sprawdzenie pliku na telefonie](../../assets/screenshots/panel-phone-ota-ready.png)](../../assets/screenshots/panel-phone-ota-ready.png)

[![Logowanie na telefonie](../../assets/screenshots/pl/panel-phone-login.png)](../../assets/screenshots/pl/panel-phone-login.png) [![Brak połączenia na telefonie](../../assets/screenshots/pl/panel-phone-offline.png)](../../assets/screenshots/pl/panel-phone-offline.png)

Zakładki przechodzą na dół ekranu, a karty układają się w jedną kolumnę. Ten sam panel otwiera się w
przeglądarce radia.

[![Pulpit na tablecie: wsteczny, tylne czujniki pikają](../../assets/screenshots/panel-tablet-dashboard.png)](../../assets/screenshots/panel-tablet-dashboard.png)

Na tablecie karty stoją w dwóch kolumnach.

## Jak zrobić obrazki od nowa

Wszystkie obrazki z tej strony robi jedno polecenie uruchomione w katalogu głównym repozytorium:

```sh
uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py
```

Wystarczy [uv](https://docs.astral.sh/uv/) i Microsoft Edge albo Google Chrome. Sceny są w
`tools/screenshots.json`; zobacz [Zrzuty ekranu](../../en/contributing/screenshots.md).
