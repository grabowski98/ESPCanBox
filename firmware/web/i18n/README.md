# Web panel translations

The web panel shows every text from a dictionary in this directory. One file per language:

| File | Language | Status |
|---|---|---|
| `en.json` | English | **source**: every key must exist here |
| `pl.json` | Polish | written and checked by the author |
| `de.json`, `es.json`, `ru.json`, `uk.json` | German, Spanish, Russian, Ukrainian | machine translation, needs native review |

The panel starts in English. The language picker (login screen and the System tab) stores the
choice in the device (NVS key `ui_lang`, `POST /api/lang`, console `lang`); the browser keeps only a
cache in `localStorage` (key `ui_lang`), and the device value wins. The device accepts only the codes
of the dictionaries in this directory (list `WEB_UI_LANGS`, written by `tools/embed_web.py`).
A missing text falls back to English, and a missing English text shows the key itself.

The USB console, the console lines in the panel, API error words, JSON keys and the `can0 ID#DATA`
recording format are **not** translated: they are a fixed technical format.

## File format

- UTF-8 **without BOM**, Unix line endings.
- A flat JSON object: `"key": "text"`, **one line per key**, keys sorted alphabetically
  (so keys of one group stay together). `python -I tools/check_i18n.py --format` rewrites a file
  in exactly this layout.
- Optional `"_meta"` object at the top, for example
  `"_meta": {"status": "machine, needs review"}`. When `status` starts with `machine`, the
  System tab shows a short note that the translation needs review.
- No HTML. Texts are inserted with `textContent`, so `<b>` would be shown literally.

## Keys

Keys are semantic, lower case, with groups separated by dots: `park.status.active`,
`err.car_busy`, `ota.ask.title`. The first part is the area of the panel:

| Group | Area |
|---|---|
| `app`, `nav`, `modal`, `banner`, `lang`, `fmt`, `common` | header, tabs, dialogs, language, number format, common words |
| `login`, `err` | login screen, error words of the API (`docs/PANEL_API.md`, 1.6) |
| `car`, `park`, `can`, `cantx`, `set` | dashboard: car, parking sensors, CAN bus, CAN transmit gate, vehicle settings |
| `radio`, `link`, `keymap`, `console`, `log` | Radio tab, UART link, key remapping, Console tab |
| `sys`, `load`, `wifi`, `sta`, `pw`, `ota` | System tab |
| `psa`, `xp` | texts of the vehicle profile (`web/profiles/psa_aee2010.json`) and of the XP radio protocol |

Do not rename a key in one language only: change `en.json` and the code together, then the
other files.

## Placeholders

`{name}` is replaced by a value at run time. Keep every placeholder of the English text, in any
order your language needs:

    "ota.mode_diff": "Different operating mode than the running one ({from} → {to})."
    "ota.mode_diff": "Inny tryb pracy niż działający ({from} → {to})."

Text between backticks is shown as code (monospace) and is normally not translated:

    "login.forgot": "A forgotten password is shown by the `wifi showpass` command on the USB console."

## Plural forms

A key with a count has one entry per plural form, named after the CLDR categories
(`_one`, `_few`, `_many`, `_other`); the panel picks the form with `Intl.PluralRules` and uses
`_other` when the browser has no `Intl`.

| Language | Required forms |
|---|---|
| en, de, es | `_one`, `_other` |
| pl, ru, uk | `_one`, `_few`, `_many`, `_other` |

    "can.rec.frames_one": "{n} ramka",
    "can.rec.frames_few": "{n} ramki",
    "can.rec.frames_many": "{n} ramek",
    "can.rec.frames_other": "{n} ramki",

Where possible the panel uses wording without a count, so there are only a few plural keys.

## Numbers and symbols

`fmt.decimal` and `fmt.group` are the decimal and thousands separators (Polish uses a comma and a
narrow no-break space U+202F, which is invisible in most editors). Units (`V`, `°C`, `ms`, `B`, `%`)
are added by the code and are the same in every language.

## Adding or fixing a translation

1. Copy `en.json` to `<code>.json` (ISO 639-1 code: `de`, `es`, `ru`, `uk`...) or open the existing file.
2. Translate the values, never the keys. `python -I tools/check_i18n.py --todo <code>` lists the
   keys still missing, with the English text.
3. Use the terms from `docs/en/contributing/glossary.md`.
4. Run `python -I tools/check_i18n.py` (it must print `0 errors`) and
   `python -I tools/embed_web.py --report` (page and dictionary sizes).
5. Look at the panel at phone width in the device mock (`tools/mock_device.py`).

A pull request that adds a new key only needs the English text: the other languages show
English until somebody translates it (`check_i18n.py` reports it as a warning, not an error).

## How it gets into the firmware

`tools/embed_web.py` puts `en.json`, the list of languages (from each `lang.name`) and the vehicle
profile into the single script of the page before the Content-Security-Policy hash is computed, then
compresses the page into the firmware. Every other dictionary is compressed on its own and stored in
the firmware as a separate resource, `GET /i18n/<lang>.json`; the panel fetches it only when that
language is chosen and shows English until it arrives (or when the request fails). Budgets: 56 KB gzip
for the page, 16 KB gzip for each other dictionary (about 10-12 KB today); a build over a budget fails
(`python -I tools/embed_web.py --report`).
