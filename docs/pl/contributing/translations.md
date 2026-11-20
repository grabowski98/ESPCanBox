# Tłumaczenia

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Wyślij swoją pracę](#send-your-work).

Do przetłumaczenia są dwie rzeczy: **panel WWW** urządzenia i ta **dokumentacja**. Mają wspólny
[słowniczek](glossary.md), żeby jedna rzecz miała wszędzie jedną nazwę.

Angielski jest źródłem i tekstem wiążącym. Tłumaczenie idzie za stroną angielską; jeśli się
różnią, rację ma angielska.

## Co jest teraz przetłumaczone {#what-is-translated-now}

| Część | Języki | Stan |
|---|---|---|
| Panel WWW (`firmware/web/i18n/`) | en, pl | napisane przez autora |
| Panel WWW | de, es, ru, uk | tłumaczenie maszynowe, wymaga sprawdzenia przez rodzimego użytkownika |
| Dokumentacja (`docs/en/`) | en | źródło |
| Dokumentacja, wszystkie strony (`docs/pl/`) | pl | tłumaczenie maszynowe, wymaga sprawdzenia przez rodzimego użytkownika |
| Dokumentacja, 13 stron (`docs/<code>/`) | de, es, ru, uk | tłumaczenie maszynowe, wymaga sprawdzenia przez rodzimego użytkownika |
| `README.md` | pl, de, es, ru, uk (`README.<code>.md`) | tłumaczenie maszynowe, wymaga sprawdzenia przez rodzimego użytkownika |

13 stron po niemiecku, hiszpańsku, rosyjsku i ukraińsku to przegląd, strona o bezpieczeństwie,
trzy strony szybkiego startu i osiem stron działu „Dostosuj do siebie”. Pozostałe strony tych
języków są zastępowane angielskimi: serwis pokazuje stronę angielską, a przełącznik języków
zostaje na wybranym języku.

> [!WARNING]
> **Strony o bezpieczeństwie nie są jeszcze sprawdzone**
>
> `getting-started/safety.md` i `safety/*.md` są we wszystkich językach tłumaczeniem maszynowym.
> Zanim serwis zostanie opublikowany, musi je sprawdzić osoba znająca dany język. Do tego czasu
> wiążące są angielskie strony o bezpieczeństwie.

Czego **nigdy** się nie tłumaczy: konsoli USB, poleceń konsoli, słów błędów API, kluczy JSON,
formatu nagrań `can0 ID#DATA`, identyfikatorów ramek, nazw środowisk i nazw przycisków nadrukowanych
w aucie (`SRC`, `LIST`). Czytają je narzędzia, więc muszą być takie same w każdym języku.

## Tłumaczenie panelu WWW {#translate-the-web-panel}

Zasady dla plików słowników są w `firmware/web/i18n/README.md`. W skrócie (polecenia wpisuj
w głównym katalogu repozytorium):

1. Otwórz `firmware/web/i18n/<code>.json` albo, dla nowego języka, skopiuj do niego `en.json`
   (kod ISO 639-1: `de`, `es`, `ru`, `uk`...).
2. Tłumacz wartości, nigdy klucze. Zachowaj każdy symbol zastępczy, na przykład `{n}` albo `{from}`.
3. Wypisz klucze, których jeszcze brakuje:

    ```
    python -I firmware/tools/check_i18n.py --todo <code>
    ```

4. Sprawdź pliki. Musi wypisać `0 errors`:

    ```
    python -I firmware/tools/check_i18n.py
    ```

5. Obejrzyj panel w szerokości telefonu w makiecie urządzenia, patrz [Pierwsze uruchomienie](../quick-start/first-boot.md).

Tłumaczenie maszynowe ma na początku pliku `"_meta": {"status": "machine translation, needs native review"}`.
Panel pokazuje wtedy krótką informację, że tłumaczenie wymaga sprawdzenia. Usuń tę linię dopiero
wtedy, gdy rodzimy użytkownik języka sprawdzi cały plik.

## Tłumaczenie dokumentacji {#translate-the-documentation}

Zasady pisania są w `docs/STYLE.md`. W skrócie:

- Umieść stronę w `docs/<code>/` z **tą samą nazwą pliku i ścieżką** co w `docs/en/`. Strona,
  której jeszcze nie przetłumaczyłeś, po prostu nie istnieje: serwis pokazuje angielską. Inne
  strony twojego języka linkują do niej wprost po angielsku (`../en/...`), więc link działa też na
  GitHubie; gdy dodasz tłumaczenie, przestaw te linki na nie (`check_doc_links.py` je wypisze).
- Zachowaj układ nagłówków, bloków kodu i tabel. Przetłumaczonemu nagłówkowi daj angielską
  kotwicę przez `{#id}`, na przykład `## Wyślij swoją pracę {#send-your-work}`, żeby linki z
  innych języków dalej działały.
- Bloki kodu przepisz bez zmian: polecenia, wyjście konsoli i nagrania są takie same w każdym
  języku.
- Zachowaj słowa statusu każdego faktu (**zmierzone** / **niezweryfikowane**, w oryginale
  **measured** / **unverified**) i ich szczegóły. Nie dodawaj faktów, których nie ma na stronie
  angielskiej.
- W tekście linku do strony użyj jej przetłumaczonego tytułu.
- Tłumaczenie maszynowe zaczyna się od tego banera, zaraz pod tytułem strony, i zachowuje go,
  dopóki rodzimy użytkownik języka nie sprawdzi strony. Ostatnie zdanie prowadzi do sekcji
  [Wyślij swoją pracę](#send-your-work) w twoim języku (w języku bez przetłumaczonej strony
  `contributing/translations.md` link prowadzi do strony angielskiej, jak niżej):

    ```markdown
    > [!NOTE]
    > **Machine translation, needs review**
    >
    > This page was translated automatically. The English page is binding. How to report a
    > mistake: [Translations](../../en/contributing/translations.md#send-your-work).
    ```

- Strony o bezpieczeństwie (`getting-started/safety.md`, `safety/*.md`) sprawdza osoba znająca
  dany język, zanim zostaną opublikowane.

Nowy język włącza się w `mkdocs.yml`: `build: true` dla niego i tytuły działów menu w
`nav_translations`. Tytuły stron w menu pochodzą z pierwszego nagłówka każdej strony.

Sprawdź tłumaczenie przed pull requestem. `check_translations.py` porównuje każdą stronę w
`docs/<code>/` z angielską: strona angielska istnieje, baner jest na miejscu, liczba nagłówków,
bloków kodu i wierszy tabel się zgadza, a każdy blok kodu jest identyczny. Te trzy polecenia
wpisz w głównym katalogu repozytorium:

```
python -I tools/check_translations.py
python -I tools/check_doc_links.py
uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs build --strict
```

Oczekiwane: `check_translations.py` kończy się na `... 0 with problems`, `check_doc_links.py`
wypisuje `0 dead links ...`, a MkDocs wypisuje `Documentation built` bez ostrzeżeń. Linia `FAIL`
podaje stronę i to, czym różni się od angielskiej.

## Korzystanie ze słowniczka {#use-the-glossary}

[Słowniczek](glossary.md) wymienia terminy panelu z ich zalecanymi tłumaczeniami. Jeśli znajdziesz
lepsze słowo, zmień słowniczek i pliki słowników w tym samym pull requeście.

W każdym języku trzymaj osobno trzy nazwy:

- **urządzenie** (the device): płytka ESP32-S3 tego projektu;
- **skrzynka** (the box, skrzynka CAN): kupowana osobno skrzynka pośrednicząca, sprzedawana z
  radiem;
- **radio** (the radio): jednostka główna z Androidem.

## Wyślij swoją pracę {#send-your-work}

Znalazłeś błąd w tłumaczeniu? Otwórz zgłoszenie (issue) z szablonem **Translation**: podaj
stronę, zacytuj błędne zdanie i, jeśli możesz, napisz lepsze. Możesz też poprawić je sam w pull
requeście: zmień stronę w `docs/<code>/` i uruchom trzy sprawdzenia opisane wyżej.

Błąd na samej stronie angielskiej poprawia się w `docs/en/` i we wszystkich tłumaczeniach tej
strony w tym samym pull requeście.

Sprawdzenie istniejącego tłumaczenia maszynowego przez rodzimego użytkownika języka jest tak samo
przydatne jak nowe tłumaczenie. Gdy sprawdzisz całą stronę, usuń jej baner w tym samym pull
requeście. Pozostałe sprawdzenia do uruchomienia opisuje `CONTRIBUTING.md`.

Dalej: zobacz [Słowniczek dla tłumaczy](glossary.md) ze słowami, których należy używać.
