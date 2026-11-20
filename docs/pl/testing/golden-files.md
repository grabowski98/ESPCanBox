# Pliki wzorcowe

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Plik wzorcowy (golden file) to zapisany i sprawdzony wynik. Test generuje ten sam wynik jeszcze raz
i porównuje go bajt po bajcie. Każda różnica oznacza, że zmieniło się działanie, nawet o jedną
milisekundę.

Projekt ma dwa rodzaje, w trzech katalogach:

| Katalog | Co w nim jest | Kto go zapisuje |
|---|---|---|
| `firmware/test_host/expected/` | po jednym pliku tekstowym na test na komputerze: cały wynik programu (stdout i stderr) z nagłówkiem `### test arguments` i stopką `### code=N` | `run_host_tests.sh --record-golden` |
| `firmware/test_host/golden/` | test strumienia: co urządzenie wysłałoby i zrobiło, dla każdego nagrania i wariantu | `run_host_tests.sh --record-golden` |
| `firmware/test_host/golden_060/` | ten sam strumień, zapisany raz z wczesnej wersji programu (0.6.0) | **nigdy nie zapisywany ponownie** |

## Test strumienia {#the-stream-test}

`golden_stream` odtwarza nagranie krokami po 1 ms, w tej samej kolejności co pętla główna
programu, w trybie dekoder. Dla każdego nagrania zapisuje trzy pliki:

| Plik | Zawartość |
|---|---|
| `<capture>.radio_tx.txt` | każda ramka, którą urządzenie wysłałoby do radia: czas w ms, potem bajty szesnastkowo |
| `<capture>.outputs.txt` | każda zmiana aktywności magistrali, wyjść 12 V (ACC, podświetlenie, wsteczny) i brzęczyka, z czasem |
| `<capture>.console.txt` | linie konsoli, które wypisałaby logika, z czasem |

Część słów w tych plikach jest po polsku (na przykład `magistrala` = bus). Są częścią
zamrożonych wzorców i zostają bez zmian.

Test działa na pięciu nagraniach z `firmware/test_data/captures/` i na czterech wariantach jednego
z nich:

| Wariant | Co symuluje |
|---|---|
| `--stall 10,50` | co 10 s pętla główna staje na 50 ms |
| `--stall 10,500` | co 10 s pętla główna staje na 500 ms |
| `--drop 0E1` | co druga ramka czujników parkowania ginie |
| `--drop 0F6` | co druga ramka zapłonu ginie |

## Trzy przebiegi {#three-passes}

Skrypt uruchamia test strumienia trzy razy:

1. **Wzorzec 0.6.0.** Zamrożona kopia wczesnego kodu spinającego program (`test_host/ref_glue_060.h`).
   Musi dać dokładnie `golden_060/`. To dowodzi, że samo narzędzie testowe się nie zmieniło.
2. **Obecny `Decoder` bez ramek `38` i `3C`.** Obecny kod z wyłączonymi dwiema funkcjami dodanymi
   później. Też musi dać dokładnie `golden_060/`. To dowodzi, że przeróbka zmieniła tylko te dwie
   ramki.
3. **Obecny `Decoder`.** To, co urządzenie robi teraz. Musi dać dokładnie `golden/`.

Jeśli celowo zmieniasz strumień do radia, zmienia się przebieg 3 i trzeba ponownie zapisać
`golden/`. Przebiegi 1 i 2 nadal muszą się zgadzać: jeśli się nie zgadzają, zmieniłeś coś, czego
nie chciałeś zmienić.

Czwarty wynik, `.build_host/golden_box_report.txt`, liczy, ile ramek prawdziwej skrzynki CAN ma
pasującą ramkę z urządzenia. To tylko informacja, test od niej nie pada.

## Kiedy zapisać ponownie {#when-to-record-again}

Zapisuj wzorce (`bash tools/run_host_tests.sh --record-golden`) tylko wtedy, gdy:

- dodałeś test albo
- celowo zmieniłeś działanie i umiesz powiedzieć dlaczego.

Potem:

1. Obejrzyj różnicę w Git, plik po pliku.
2. Napisz w pull requeście, co się zmieniło i dlaczego, na przykład: „ramka `38` wychodzi teraz
   10 ms później, bo czeka w kolejce”.
3. Nigdy nie edytuj pliku wzorcowego ręcznie.

> [!WARNING]
> **Nie ruszaj `golden_060/` ani `ref_glue_060.h`**
>
> To stały punkt, z którym porównuje się wszystkie późniejsze zmiany. Skrypt nigdy nie zapisuje
> ponownie `golden_060/`; ty też nie rób tego ręcznie.
