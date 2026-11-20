# Pierwsze uruchomienie

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Ta strona pokazuje, jak odczytać hasło urządzenia i otworzyć panel WWW na telefonie albo
laptopie.

Dotyczy kompilacji z siecią (`listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`,
`listen_generic_wifi`). Otwierają one własną sieć Wi-Fi z panelem WWW. Kompilacje bez sieci
(`listen`, `decoder`) w ogóle nie zawierają kodu Wi-Fi; dla nich jest tylko konsola USB.

Nie masz jeszcze płytki? Przejdź do [Wypróbuj panel bez płytki](#try-the-panel-without-a-board).

## 1. Wgraj kompilację z siecią {#1-flash-a-build-with-network}

Jeśli wgrałeś `listen_wifi` jak na stronie [Wgraj program](flash.md), przejdź do kroku 2.
W przeciwnym razie w terminalu otwartym w folderze `firmware` (jak na tamtej stronie) uruchom:

```text
uvx platformio run -e listen_wifi -t upload
uvx platformio device monitor
```

Zostaw płytkę podłączoną do komputera. Dopóki na USB jest komputer, urządzenie trzyma Wi-Fi
włączone, nawet bez ruchu na CAN.

## 2. Odczytaj hasło z konsoli USB {#2-read-the-password-from-the-usb-console}

Przy pierwszym starcie urządzenie generuje losowe, 12-znakowe hasło i je zapisuje. Konsola
wypisuje:

```text
[wifi] device password generated; the command wifi showpass shows it
```

Tej linii możesz nie zobaczyć: pada przy starcie, często zanim monitor się połączy. To nie
szkodzi. W monitorze (wpisywanych liter nie widać; po każdym poleceniu naciśnij Enter):

1. Wpisz `wifi`. Zapisz nazwę sieci z odpowiedzi.
2. Wpisz `wifi showpass`. Hasło jest w linii `[wifi] password: <12 znaków>`.

Hasło jest jednocześnie **kluczem Wi-Fi** sieci urządzenia i **hasłem panelu**. Jest wypisywane
tylko na konsoli USB, nigdy w panelu. Zapomniane hasło zawsze pokaże ponownie `wifi showpass`
przez USB.

> [!WARNING]
> **Nie wklejaj hasła do udostępnianych logów**
>
> Linia wypisana przez `wifi showpass` zawiera hasło. Nie wklejaj jej do zgłoszenia błędu, na
> forum ani do nagrania, które komuś udostępniasz.

Polecenia opcjonalne:

| Polecenie | Co pokazuje albo robi |
|---|---|
| `wifi` | stan sieci: jej nazwę, adres `http://10.50.8.1`, nazwę `.local`, połączone urządzenia, sieć domową |
| `wifi showpass` | hasło urządzenia |
| `wifi pass <new password>` | ustawia twoje własne hasło: od 10 do 63 drukowalnych znaków, bez spacji |
| `wifi newpass` | nowe losowe hasło; stare przestaje działać na wszystkich urządzeniach |

Wszystkie polecenia `wifi` działają tylko z konsoli USB; panel je odrzuca.

## 3. Połącz się z siecią urządzenia {#3-connect-to-the-device-network}

1. Na telefonie albo laptopie otwórz listę sieci Wi-Fi. Nazwa sieci to nazwa projektu, znak `-`
   i cztery cyfry szesnastkowe z adresu MAC układu (`<project name>-XXXX`). Dokładną nazwę
   wypisuje polecenie `wifi`.
2. Połącz się z nią hasłem z kroku 2. To sieć WPA2.
3. Jeśli telefon zapyta, czy pozostać w sieci bez internetu, wybierz pozostanie w sieci.

Naraz mogą być połączone najwyżej cztery urządzenia (na przykład radio, twój telefon i laptop).

> [!NOTE]
> **VPN i dane komórkowe**
>
> VPN na laptopie albo telefonie może kierować adres `10.50.8.1` poza sieć Wi-Fi. Wyłącz VPN na
> czas korzystania z panelu. Jeśli panel nie otwiera się na telefonie, wyłącz też na chwilę
> dane komórkowe.

## 4. Otwórz panel {#4-open-the-panel}

1. Otwórz w przeglądarce `http://10.50.8.1`. Nazwa `.local` wypisana przez `wifi` też działa na
   systemach, które obsługują mDNS.
2. Zaloguj się hasłem urządzenia. Pole **Zapamiętaj na tym urządzeniu** zaznaczaj tylko na
   własnym telefonie albo laptopie: wtedy nowa karta otwiera panel bez ponownego pytania o hasło.

Panel używa zwykłego `http://`, nie `https://`; jeśli przeglądarka sama przełącza na `https://`,
wpisz adres z `http://` na początku.

Oczekiwany wynik: panel pokazuje pięć zakładek: **Pulpit**, **CAN**, **Radio**, **Konsola**
i **System**.

[![Pulpit panelu WWW (makieta urządzenia, schemat ciemny)](../../assets/screenshots/panel-dashboard.png)](../../assets/screenshots/panel-dashboard.png)

## 5. Wybierz język {#5-choose-the-language}

Wybór języka jest na ekranie logowania i na zakładce **System**. Panel startuje po angielsku.
Twój wybór jest zapisywany w urządzeniu, więc inny telefon otwiera się w tym samym języku, także
ekran logowania. W stronie panelu jest tylko angielski; inny język panel pobiera z urządzenia po
wybraniu go, a do tego czasu pokazuje angielski.

Dostępne: angielski, polski, niemiecki, hiszpański, rosyjski i ukraiński. Niemiecki, hiszpański,
rosyjski i ukraiński to tłumaczenia maszynowe; zakładka System o tym informuje, dopóki czekają na
sprawdzenie. Linie konsoli, identyfikatory ramek i format `can0 ID#DATA` nigdy nie są
tłumaczone.

## Kiedy Wi-Fi jest włączone {#when-wi-fi-is-on}

Na biurku zostaw płytkę podłączoną do komputera: wtedy Wi-Fi działa cały czas. Przy zasilaniu
z ładowarki USB albo powerbanka Wi-Fi gaśnie 60 s po starcie, jeśli na CAN nie ma ruchu i nie
używasz panelu.

W aucie urządzenie jest zasilane ze stałego 12 V, więc nie trzyma Wi-Fi włączonego cały czas.
Wi-Fi jest włączone:

- gdy na magistrali CAN jest ruch albo radio ma ACC;
- gdy do USB jest podłączony komputer;
- przez krótki czas po ucichnięciu magistrali, jeśli ktoś używa panelu: do 60 s od ostatniego
  zapytania zalogowanego klienta i najwyżej 10 minut w sumie, i tylko gdy napięcie zasilania jest
  dość wysokie (o ile płytka je mierzy);
- przez pierwsze 60 s po starcie przy cichej magistrali, żebyś zdążył się połączyć.

`wifi off` wyłącza sieć i trzyma ją wyłączoną także po restarcie; `wifi on` włącza ją z powrotem.

## Opcjonalnie: twoja sieć domowa {#optional-your-home-network}

Urządzenie może też połączyć się z twoją domową siecią Wi-Fi, żebyś mógł otwierać panel z sieci
domowej:

```text
wifi sta <network name>|<password>
```

Pierwszy znak `|` oddziela nazwę od hasła; hasło jest wymagane (od 8 do 63 znaków albo 64 cyfry
szesnastkowe jako gotowy klucz WPA2). `wifi sta off` zapomina sieć. Urządzenie próbuje się
połączyć co 30 s (co 5 minut, gdy ktoś jest połączony z jego własną siecią) i nigdy, gdy pracuje
silnik.

Z sieci domowej panel jest domyślnie **tylko do odczytu**. `wifi lan on` pozwala na zmiany także
z sieci domowej; `wifi lan off` znowu je blokuje. Uzbrojenie kompilacji z nadawaniem CAN nigdy nie
jest dozwolone z sieci domowej, tylko z własnej sieci urządzenia albo przez USB.

## Wypróbuj panel bez płytki {#try-the-panel-without-a-board}

`firmware/tools/mock_device.py` to makieta urządzenia: odtwarza nagranie CAN i odpowiada na te
same zapytania HTTP co program, z tą samą stroną panelu. Potrzebuje tylko Pythona 3.

W terminalu otwartym w folderze `firmware` uruchom (w Linuksie i macOS wpisz `python3` zamiast
`python`):

```text
python -I tools/mock_device.py test_data/captures/can_20261111_191045.log
```

Wypisuje adres i losowe hasło makiety:

```text
Device mock: http://127.0.0.1:8080/  (frames in the capture: ..., VIN masked)
mock password: ...
```

1. Otwórz `http://127.0.0.1:8080/`. Oczekiwany wynik: ekran logowania.
2. Zaloguj się hasłem makiety. Oczekiwany wynik: Pulpit z ruchem z nagrania.
3. Nie zamykaj okna terminala, póki używasz panelu. Makietę zatrzymujesz przez Ctrl+C.

Użyj nagrania, w którym ruch jest od początku, takiego jak `can_20261111_191045.log`; nagranie,
które zaczyna się od cichej magistrali, przez ten czas nie pokazuje żadnego ruchu.

Przydatne opcje:

| Opcja | Działanie |
|---|---|
| `--password <text>` | twoje własne hasło makiety (od 10 do 63 znaków) |
| `--port 8081` | inny port |
| `--mode decoder` | zachowuje się jak kompilacja dekodera |
| `--lan` | zachowuje się tak, jakbyś był w sieci domowej (tylko odczyt, blokada logowania) |
| `--lan --lan-admin` | jak po `wifi lan on` |
| `--speed 2` | odtwarza dwa razy szybciej |
| `--no-loop` | nie powtarza nagrania |

Makieta upraszcza kilka rzeczy (na przykład brzęczyk i dekodowanie przycisków); lista jest na
początku skryptu.

## Dalej {#next}

[Test na stole](bench-test.md): sprawdź płytkę, zanim trafi do auta.
