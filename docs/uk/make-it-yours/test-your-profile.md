# Перевірте свій профіль

> [!NOTE]
> **Машинний переклад, потребує перевірки**
>
> Цю сторінку перекладено автоматично. Обов'язковим є англійський текст. Як надіслати
> виправлення: [Переклади](../../en/contributing/translations.md#send-your-work).

Перш ніж профіль потрапить в автомобіль, запустіть його на своєму ПК. **Тест на комп'ютері** (host test) подає запис,
кадр за кадром, через той самий код декодера, що працює на пристрої, і виводить, що зробив би
пристрій: рядки консолі, кадри для магнітоли і кожну зміну виходів 12 V та зумера. Ви один раз
читаєте цей вивід, звіряєте з тим, що робили в автомобілі, і зберігаєте його як еталон. Відтоді
тест повідомляє вам, коли зміна щось ламає.

Нічого не прошивається і жоден порт не відкривається. Вам не потрібні ні плата, ні автомобіль.

Ця сторінка продовжує посібник зі сторінки [Напишіть профіль автомобіля](write-a-profile.md) з
вигаданим профілем `mine`. Кожну команду запускали на копії репозиторію. Усі команди запускайте
з теки `firmware`, у Git Bash.

> [!TIP]
> **Спершу пробний прогін**
>
> Перш ніж вписувати в `car_mine` свої кадри, можна перевірити копію й перейменування.
> Виконайте кроки 2–4 зі значеннями шаблону, які ще лишилися в `car_mine`, і з
> `run_file mine_car test_host/synthetic/example_car.log` у кроці 3. Вивід має закінчуватися
> рядками `example profile works on every axis` і `### code=0`. Потім змініть профіль під свій
> автомобіль і впишіть свій запис у рядок `run_file`.

## Що вам потрібно {#what-you-need}

- Git Bash у Windows або оболонка в Linux чи macOS;
- `uv`: тестовий скрипт отримує через нього компілятор C++ (пакет `ziglang`; перший запуск його
  завантажує, тож один раз потрібен інтернет), або задайте власний через
  `CXX=g++`. Подробиці: [Тести на комп'ютері](../../en/testing/host-tests.md);
- ваш профіль у `firmware/lib/car_<name>/`;
- запис вашого автомобіля з маркерами ([Запишіть шину](record.md)), очищений через
  `python -I ../tools/sanitize_capture.py`, якщо будете ним ділитися ([Поділіться своїм автомобілем](contribute.md)).

## Крок 1: покладіть запис на місце {#step-1-put-the-recording-in-place}

| Запис | Куди |
|---|---|
| справжній запис вашого автомобіля | `firmware/test_data/captures/`, з одним рядком у його `README.md` (що, як записано, який тест його використовує) |
| кадри, написані вручну, щоб спробувати випадок | `firmware/test_host/synthetic/`, з коментарем угорі, що кадри вигадані |

Формат звичайний, `(seconds) can0 ID#DATA`. Рядки, що не є кадрами CAN (коментарі, рядки UART),
тест пропускає. Написані вручну файли для тестів профілю кладіть у `test_host/synthetic/`, а не
в `test_data/synthetic/`: та тека для `can_diff.py`.

Файл, написаний вручну, — добрий початок, бо ви точно знаєте, що має статися. Той, що
використовувався в посібнику, `test_host/synthetic/mine_car.log`, починався так (навчальний
автомобіль зі сторінки [Напишіть профіль](write-a-profile.md); цього файлу немає в репозиторії,
тож напишіть власний):

```text
# Invented frames for the "mine" profile (tutorial). Not a real car.
# 2C0 = ignition / reverse, 1F0 = buttons + knob, 350 = rear parking, 2C8 = lights
(0.000000) can0 2C0#00000000
(0.200000) can0 2C0#00010000
(0.400000) can0 2C0#00020000
(0.600000) can0 1F0#01000000
(0.700000) can0 1F0#01000000
```

## Крок 2: скопіюйте приклад тесту {#step-2-copy-the-example-test}

Профіль-шаблон має власний тест, `firmware/test_host/example_car.cpp`. Скопіюйте його, з теки
`firmware`:

```bash
cp test_host/example_car.cpp test_host/mine_car.cpp
```

Потім змініть чотири речі в `mine_car.cpp`:

1. `#include "profile_example.h"` стає `#include "profile_mine.h"`;
2. кожен `kCarExample` стає `kCarMine` (шість разів, у чотирьох рядках; найпростіше знайти й
   замінити);
3. текст використання `example_car <capture.log>` стає `mine_car <capture.log>` (у коментарі
   вгорі та в рядку `fprintf`);
4. останню перевірку, рядок, що починається з `const bool ok =`. Вона вимагає, щоб **кожен**
   вихід щось зробив. Частини: `r.accMs` = ACC, `r.illMs` = фари, `r.revMs` = задня передача,
   `r.toneMs` = зумер, `r.uartFrames` = кадри для магнітоли, `d.haveLastKey()` = хоча б одна
   кнопка. Приберіть частини, яких ваш профіль не має. Профіль без парктроніків не проходить
   `r.toneMs > 0`, і тоді тест завершується з кодом виходу 1. Готові рядки:
    - лише запалювання й кнопки:
      `const bool ok = r.accMs > 0 && r.uartFrames > 0 && d.haveLastKey();`
    - лише запалювання: `const bool ok = r.accMs > 0;`

    Текст `example profile works on every axis` можна залишити: це лише підпис.

## Крок 3: додайте тест до тестового скрипта {#step-3-add-the-test-to-the-test-script}

Відкрийте `firmware/tools/run_host_tests.sh` і знайдіть блок, що закінчується на
`finish example_car`. Додайте свій тест одразу після нього:

```bash
# My car profile
# shellcheck disable=SC2086
build mine_car $S38_LIBS profile_mine.cpp
rm -f "$OUT/mine_car.actual.txt"
run_file mine_car test_host/synthetic/mine_car.log
finish mine_car
```

`build` компілює ваш тест разом з декодером і його бібліотеками та файлом вашого профілю.
`$S38_LIBS` — список файлів декодера, потрібних тесту; залиште його як є. `run_file` запускає його на заданому файлі, зі шляхом відносно `firmware/`. Для запису в
`test_data/captures/` вкажіть цей шлях.

## Крок 4: перший запуск, прочитайте вивід, збережіть еталон {#step-4-first-run-read-the-output-store-the-reference}

Запустіть усі тести на комп'ютері (`cd firmware` потрібен, лише якщо ви в теці репозиторію):

```bash
cd firmware
bash tools/run_host_tests.sh
```

Очікуваний результат: усі наявні тести виводять `OK`. Ваш зупиняє скрипт, бо ще не має еталона:

```text
OK     example_car
FAIL   mine_car: no reference test_host/expected/mine_car.txt
```

Вивід тепер лежить у `firmware/.build_host/mine_car.actual.txt`. **Прочитайте його** і порівняйте
з тим, що ви робили в автомобілі (або написали у файлі). Це і є справжній тест; збережений файл
лише повторює його згодом. Уривок із проходження посібника:

```text
### mine_car test_host/synthetic/mine_car.log
profile mine: 500000 bit/s, 4 key rows, 1 knob(s), txAllow 0
     0 outputs acc=0 ill=0 rev=0 tone=0 continuous=0
console [car] acc=1
   200 radio 2E EA 02 01 0A 08
   200 outputs acc=1 ill=0 rev=0 tone=0 continuous=0
console [key] code=14 state=1
   600 radio 2E 20 02 14 01 C8
console [key] code=14 state=2
   800 radio 2E 20 02 14 02 C7
...
console [park] rear=4 front=0 sound=1 rear interval=0x00 zones rear 707 front 777
  1600 radio 2E 32 07 02 05 00 05 05 05 05 AB
  1600 outputs acc=1 ill=1 rev=1 tone=1 continuous=1
...
frames=21 uart_bytes=106 uart_frames=13 acc_ms=3101 ill_ms=1001 rev_ms=800 tone_ms=390 keys_dropped=0
example profile works on every axis
### code=0
```

Як це читати:

| Рядок | Значення |
|---|---|
| `profile mine: ...` | що містить ваш профіль: швидкість, кількість рядків кнопок і ручок, список передачі (для власного профілю завжди 0) |
| `console ...` | рядок, який пристрій вивів би в консоль USB |
| `200 radio 2E EA ...` | кадр для магнітоли на позначці 200 ms (час за годинником запису). `EA` = живлення ввімкнено, `20` = кнопка, `32` = парктроніки, `3E` = прапорець перешкоди. Див. [Протокол XP](../../en/reference/xp-protocol.md) |
| `outputs acc=1 ill=0 rev=0 tone=0 continuous=0` | зміна виходів 12 V (ACC, підсвічування, задня передача) або зумера |
| `[key] code=14 state=1` | кнопка з кодом магнітоли `14` (гучність +): 1 натиснуто, 2 утримується, 0 відпущено |
| останні рядки | підсумки в мілісекундах і ваша перевірка `ok` |

Що варто перевірити у власному виводі:

- ACC вмикається з вашим кадром увімкнення запалювання і вимикається через 2.5 s після вимкнення
  запалювання. В уривку він ніколи не вимикається, бо відтворення закінчується через 1 s після
  останнього кадру.
- Вихід підсвічування вимикається через 1 s після останнього кадру фар (під час проходження посібника: кадр
  фар на 1100 ms, `ill=0` на 2101 ms). Якщо ваш автомобіль повторює свій кадр фар, цього не
  стається.
- Кожна кнопка дає `state=1`, потім `state=2` кожні 200 ms, поки утримується, потім `state=0`.
- Сигнал починається з першої зони нижче 7 і стає безперервним лише з інтервалом `0x00`.
- Кнопка показує не той код, який ви чекаєте: див. [Зіставте кнопки з магнітолою](radio-mapping.md).
  Типові перепризначення беруться з профілю автомобіля; приклад профілю їх не має, тож приклад
  тесту показує SRC як `code=21 state=1`, надісланий без змін.

> [!WARNING]
> **Збережений `### code=1` проходить як OK**
>
> Скрипт лише порівнює вивід зі збереженим файлом. На збереженому `### code=1` він не
> зупиняється. Перевірте останні рядки (пункт 1 нижче) до копіювання, інакше тест пройде, хоча
> він не вдався.

Коли вивід правильний, збережіть його як еталон:

1. Перевірте два останні рядки `.build_host/mine_car.actual.txt`. Це мають бути
   `example profile works on every axis` і `### code=0`.
2. Якщо бачите `FAIL: an axis of the example profile did nothing` і `### code=1`, виправте рядок
   `const bool ok =` (крок 2, пункт 4) і запустіть скрипт знову.
3. Скопіюйте файл і запустіть скрипт знову:

```bash
cp .build_host/mine_car.actual.txt test_host/expected/mine_car.txt
bash tools/run_host_tests.sh
```

Очікуваний результат: `OK     mine_car` і `All tests match the references.`

> [!WARNING]
> **Не використовуйте для цього `--record-golden`**
>
> `bash tools/run_host_tests.sh --record-golden` перезаписує еталони **всіх** тестів,
> включно з golden-файлами еталонного автомобіля. Використовуйте його лише тоді, коли ви
> свідомо змінили поведінку і перевірили кожну відмінність. Для одного нового тесту скопіюйте
> його файл, як показано вище.

## Швидший цикл для одного тесту {#a-faster-loop-for-one-test}

Під час проходження посібника повний скрипт тривав 30–50 секунд. Поки ви працюєте над профілем, можна
компілювати й запускати лише свій тест. Спершу один раз запустіть повний тестовий скрипт: він
завантажує компілятор у кеш `uv`, а команда нижче використовує цей кеш (`--offline`). З теки
`firmware`, у Git Bash:

```bash
LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp profile_mine.cpp vehicle_signals.cpp"
SRCS=""; for f in $LIBS; do SRCS="$SRCS $(ls lib/*/$f)"; done
INC=""; for d in lib/*/; do INC="$INC -I ${d%/}"; done
mkdir -p .build_host
uvx --offline --from ziglang python -I -m ziglang c++ -std=c++17 -O1 -Wall $INC -I include \
    test_host/mine_car.cpp $SRCS -o .build_host/mine_car.exe < /dev/null
./.build_host/mine_car.exe test_host/synthetic/mine_car.log
```

Список файлів той самий, що `$S38_LIBS` у тестовому скрипті, плюс ваш профіль і
`vehicle_signals.cpp`. Якщо список у скрипті зміниться, скопіюйте його звідти. Перш ніж будь-чим
ділитися, запустіть повний скрипт.

## Запустіть також перевірку шарів {#also-run-the-layering-check}

```bash
python -I tools/check_layering.py
```

Вона перевіряє, що ваш профіль не включає нічого з протоколу магнітоли, з `src/` чи з фреймворку
Arduino, і що жодна назва заголовного файлу не трапляється двічі. Під час проходження посібника вона
перелічила `car_mine` серед бібліотек і закінчилася на `Layering OK.`

## Чого цей тест не покриває {#what-this-test-does-not-cover}

- Код у `firmware/src/` (драйвер CAN, UART, виходи 12 V, звук) на ПК не виконується. Перша
  перевірка справжнього обладнання — [стендовий тест](../quick-start/bench-test.md).
- Часові характеристики на справжній шині: відтворення йде кроками по 1 ms і не втрачає кадрів.
- Чи правильне ваше декодування для автомобіля. Тест може показати лише те, що ваш профіль робить
  з кадрами. Тому ви читаєте вивід і звіряєте з маркерами, перш ніж його зберегти.

Далі: [Зіставте кнопки з магнітолою](radio-mapping.md).
