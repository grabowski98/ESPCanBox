# Translations

There are two things to translate: the **web panel** of the device and this **documentation**.
They share one [glossary](glossary.md), so that one thing has one name everywhere.

English is the source and the binding text. A translation follows the English page; if they
differ, the English one is right.

## What is translated now

| Part | Languages | Status |
|---|---|---|
| Web panel (`firmware/web/i18n/`) | en, pl | written by the author |
| Web panel | de, es, ru, uk | machine translation, needs native review |
| Documentation (`docs/en/`) | en | source |
| Documentation, every page (`docs/pl/`) | pl | machine translation, needs native review |
| Documentation, 13 pages (`docs/<code>/`) | de, es, ru, uk | machine translation, needs native review |
| `README.md` | pl, de, es, ru, uk (`README.<code>.md`) | machine translation, needs native review |

The 13 pages in German, Spanish, Russian and Ukrainian are the overview, the safety page, the
three quick start pages and the eight pages of "Make it yours". Every other page of these
languages falls back to English: the site shows the English page with the language switcher still
on the chosen language.

> [!WARNING]
> **Safety pages are not reviewed yet**
>
> `getting-started/safety.md` and `safety/*.md` are machine translations in every language. A
> person who speaks the language must check them before the site is published. Until then the
> English safety pages are binding.

What is **never** translated: the USB console, console commands, API error words, JSON keys, the
`can0 ID#DATA` recording format, frame IDs, environment names and button names printed in the car
(`SRC`, `LIST`). Tools read them, so they must stay the same in every language.

## Translate the web panel

The rules for the dictionary files are in `firmware/web/i18n/README.md`. In short (type the
commands in the top directory of the repository):

1. Open `firmware/web/i18n/<code>.json`, or copy `en.json` to it for a new language
   (ISO 639-1 code: `de`, `es`, `ru`, `uk`...).
2. Translate the values, never the keys. Keep every placeholder such as `{n}` or `{from}`.
3. List the keys that are still missing:

    ```
    python -I firmware/tools/check_i18n.py --todo <code>
    ```

4. Check the files. It must print `0 errors`:

    ```
    python -I firmware/tools/check_i18n.py
    ```

5. Look at the panel at phone width in the mock device, see [First boot](../quick-start/first-boot.md).

A machine translation has `"_meta": {"status": "machine translation, needs native review"}` at the
top of the file. The panel then shows a short note that the translation needs review. Remove the
line only when a native speaker has checked the whole file.

## Translate the documentation

The writing rules are in `docs/STYLE.md`. In short:

- Put the page in `docs/<code>/` with the **same file name and path** as in `docs/en/`. A page you
  have not translated yet is simply missing: the site shows the English one. Other pages of your
  language link to it straight in English (`../en/...`), so the link also works on GitHub; when you
  add the translation, point those links to it (`check_doc_links.py` lists them).
- Keep the structure of headings, code blocks and tables. Give a translated heading the English
  anchor with `{#id}`, for example `## Wyślij swoją pracę {#send-your-work}`, so that links from
  other languages keep working.
- Copy code blocks unchanged: commands, console output and recordings are the same in every
  language.
- Keep the status words of every fact (**measured** / **unverified**) and their details. Do not
  add facts that the English page does not have.
- Link to a page with its translated title as the link text.
- A machine translation starts with this banner, right under the page title, and keeps it until a
  native speaker has checked the page. The last sentence links to [Send your work](#send-your-work)
  in your language (in a language that has no translated `contributing/translations.md`, the link
  goes to the English page, as below):

    ```markdown
    > [!NOTE]
    > **Machine translation, needs review**
    >
    > This page was translated automatically. The English page is binding. How to report a
    > mistake: [Translations](../../en/contributing/translations.md#send-your-work).
    ```

- Safety pages (`getting-started/safety.md`, `safety/*.md`) are checked by a person who speaks
  the language before they are published.

A new language is switched on in `mkdocs.yml`: `build: true` for it and the section titles of the
menu under `nav_translations`. Page titles in the menu come from the first heading of each page.

Check a translation before a pull request. `check_translations.py` compares every page in
`docs/<code>/` with the English one: the English page exists, the banner is there, the number of
headings, code blocks and table rows is the same, and every code block is identical. Type these
three commands in the top directory of the repository:

```
python -I tools/check_translations.py
python -I tools/check_doc_links.py
uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs build --strict
```

Expected: `check_translations.py` ends with `... 0 with problems`, `check_doc_links.py` prints
`0 dead links ...`, and MkDocs prints `Documentation built` without warnings. A `FAIL` line names
the page and what differs from the English one.

## Use the glossary

[The glossary](glossary.md) lists the terms of the panel with their preferred translations. If you
find a better word, change the glossary and the dictionary files in the same pull request.

Keep the three names apart in every language:

- **the device**: the ESP32-S3 board of this project;
- **the box** (CAN box): the aftermarket interface box sold with the radio;
- **the radio**: the Android head unit.

## Send your work

Found a mistake in a translation? Open an issue with the **Translation** template: name the page,
quote the wrong sentence and, if you can, write a better one. Or fix it yourself in a pull
request: change the page in `docs/<code>/` and run the three checks above.

A mistake in the English page itself is fixed in `docs/en/` and in every translation of that page
in the same pull request.

A review of an existing machine translation by a native speaker is as useful as a new one. When
you have checked a whole page, remove its banner in the same pull request. See `CONTRIBUTING.md`
for the other checks to run.

Next: the words to use are in the [Glossary for translators](glossary.md).
