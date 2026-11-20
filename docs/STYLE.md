# Writing and translation style

Short rules for everybody who writes or translates the documentation. English in `docs/en/` is
the source and the binding text; translations follow it page by page.

## Readers and tone

- The reader is a hobby electronics person. They can use a multimeter and a soldering iron, but
  they may never have used PlatformIO, Git or Python.
- Write in the second person ("connect", "you will see"). Short sentences, one idea each.
- Plain words, no marketing ("powerful", "seamless", "simply"). Explain a technical word the first
  time a page uses it, or link to the glossary.
- Timeless text: no "today", no dates of single days in the story, no paths to private logs, no
  names of people. The person who built the project is "the author" (pseudonym `grabowski98`).
- Say plainly what is not known: "Not known yet." or "Not measured." is a good sentence.

## Every fact about a car has a status

Each statement about a car, a radio or a CAN box carries one of two words:

| Status | Meaning | What you must add |
|---|---|---|
| **measured** | seen on the car or on the bench | how it was measured (multimeter, recording, console of the device) and, for a recording, the file in `firmware/test_data/captures/` |
| **unverified** | from a description we did not confirm | the source (for example PSA-RE, with path and commit) |

In tables use a `Status` column. In running text put it in brackets right after the claim:
"(measured: multimeter, ignition on)" or "(unverified: PSA-RE `LS.INFO-DIV/364.yml`)".
A conclusion drawn from a measurement is not a measurement: write it as a conclusion and say what
would confirm it ("looks like low-speed fault-tolerant CAN; an oscilloscope is needed to confirm").

Do not add facts that are not in the sources of the project. Do not copy text from datasheets,
forum posts or other projects; describe and link instead.

## Warnings and other boxes {#boxes}

The pages are read on the built site and as plain files on GitHub. Write boxes in the GitHub alert
syntax, which both render, always with a title as a bold first line:

```markdown
> [!CAUTION]
> **Transmits on the car bus**
>
> This build can send frames to the car. ...

> [!WARNING]
> **Disconnect the battery first**
>
> ...

> [!NOTE]
> **Not measured yet**
>
> ...
```

- `[!CAUTION]` (red on the site): anything that can change the state of the car or damage
  hardware (transmitting on CAN, 12 V outputs, cutting wires).
- `[!WARNING]`: a step that can go wrong or leave the car in a bad state (battery drain, wrong pin).
- `[!NOTE]` / `[!TIP]`: background or a shortcut.
- The marker line holds only `> [!TYPE]`: GitHub shows text after it as a plain quote. The bold
  second line becomes the box title on the site (hook `docs/_hooks/alert_titles.py`); GitHub shows
  it under "Note" / "Warning".
- Every line of the box starts with `>`, blank lines too (`>`). Lists, tables and code blocks work
  inside. Before a list item that follows an indented paragraph or code block of the previous item,
  leave a `>` line, or the site joins the item to the paragraph.
- A collapsible box (details for the curious) is HTML that both render:

  ```markdown
  <details class="note" markdown="1">
  <summary>All six environments</summary>

  Text, tables and code blocks, not indented.

  </details>
  ```

- Do not use the MkDocs `!!!` / `???` syntax: GitHub shows it as plain text.
  `tools/check_doc_links.py` reports it.

Put a warning **before** the step it is about, never after it. Keep the first sentence a complete
instruction.

## Facts that must stay right

Some earlier notes of the project were wrong. Never write:

- that the bus at the radio of the reference car is "verified high-speed CAN". It runs at
  125 kbit/s; its voltages look like low-speed fault-tolerant CAN, not yet confirmed with an
  oscilloscope;
- that the device "transmits only 15B" without details, or that frame `15B` is checked byte by
  byte. The default transmit build allows only `15B`, checked by ID and length only. `1A9` exists
  only in a build with `-DCAN_TX_EXPERIMENTAL=1`, and no build has a climate frame;
- that climate control from the radio works. Single `364` frames from the device did not change it;
- that the settings sent with `15B` "work in the car". The body control unit (BSI) accepted them
  (seen in frame `260`); the visible effect in the car was not checked;
- anything that suggests the device transmits by default. It listens only, unless you build and arm
  the transmit environment.

## Names

- Project name: do not type it in pages. Write "this project" or "the device". The name lives in
  `project.json`; `tools/set_project_name.py --apply` also rewrites `site_name` of `mkdocs.yml`
  and the few title lines that carry it.
- "the device" = the ESP32-S3 board of this project. "the box" / "CAN box" = the aftermarket
  interface box sold with the radio. "the radio" = the Android head unit. Never mix them.
- Third-party names (Peugeot, Stellantis, Simple Soft, Raise, Junsun, FYT) only to say what was
  tested. The project is not affiliated with any of them.
- Code words exactly as in the code, in backticks: environment names (`listen`, `decoder`,
  `listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`, `listen_generic_wifi`), build flags
  (`-DCAR_PROFILE=PSA_AEE2010`, `-DRADIO_PROTOCOL=XP`, `-DCAN_TX_EXPERIMENTAL=1`), profile names
  (`psa_aee2010`, `generic`), console commands (`cantx on`, `wifi showpass`), file paths. Check a
  name in the code before you write it: the code is the source of truth, old notes are not.
- Frame IDs: three hex digits, upper case, no `0x`, in backticks: `0E1`, `15B`, `260`.
  Byte values: two hex digits, upper case, separated by spaces: `00 1C 8F BB C5 50 00 00`.
- Units with a space: `125 kbit/s`, `12 V`, `0.65 V`, `5 ms`. Decimal point, not comma (in English).

## Bytes and bits

The same convention as the code comments, `vehicles/` and `radios/`:

- **CAN frames:** bytes are numbered from **1** (byte 1 = the first data byte = `data[0]` in C++).
  Bits are numbered **7..0** inside a byte, bit 0 = least significant. Short form `byte.bit`:
  `5.7` = byte 5 bit 7. A field is written high to low: "bits 6-5", "`3E1` 4.7-4.6".
- **UART frames of the radio protocol:** `DATA[0]` = the first data byte after the header, as in
  `radios/xp-simple-soft/protocol.md`. Say "DATA[n]" every time, so nobody mixes the two styles.

## Links

- Link between pages with relative paths to the `.md` file: `[Safety](../getting-started/safety.md)`.
  Anchors are the heading text in lower case with spaces as `-`: `safety.md#before-you-start`.
  A heading with an explicit `{#id}` has that id instead (translations, see below).
- Two headings with the same text on one page get the ids `radio` and `radio_1`; give the second
  one an explicit `{#id}` or a different text, so links to it are clear.
- Every relative link points to a file that exists on disk, so it works on the site and on
  GitHub. A page of a translation links to a page that is not translated yet straight in English:
  from `docs/de/index.md` write `../en/getting-started/how-it-works.md`, from
  `docs/de/quick-start/flash.md` write `../../en/safety/transmit.md`. The site still shows that
  page inside the German site (English text, language switcher on German). Once the page is
  translated, link the translation; `check_doc_links.py` reports a link into `docs/en/` that has a
  translated page.
- A page of the site cannot link to a file outside `docs/<lang>/` and `docs/en/` (firmware,
  vehicles, hardware): write the repository path as code, for example
  `vehicles/peugeot-508-sw-2015/508.dbc`.
- Images shared by every language (logo, screenshots of the web panel) are in `docs/assets/`. Link
  them with the Markdown image syntax and a relative path, for example
  `![Dashboard tab](../../assets/screenshots/panel-dashboard.png)` from `docs/pl/reference/`. Make
  a screenshot a link to its own file, `[![alt](path)](path)`: the page shows it reduced and the
  link opens the full size. Do
  not use an HTML `<img>` tag in a page of the site: the site does not rewrite its address. The alt
  text is translated; the image is the same in every language. Screenshots of the panel come from
  the mock device (`firmware/tools/mock_device.py`), never from a car.
- Check before a pull request: `python -I tools/check_doc_links.py` and
  `uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs build --strict`.

## Translations

- Same file names and headings structure as `docs/en/`. A page you have not translated yet is
  simply missing: the site shows the English one, and links to it go to `docs/en/` (see "Links").
- Same number of headings, code blocks and table rows as the English page; code blocks are copied
  unchanged. `python -I tools/check_translations.py` checks this and the banner below.
- Machine translations start with this banner, right under the page title, and keep it until a
  native speaker has checked them. The link goes to the section "Send your work" of the
  translations page, with the link text in your language:

  ```markdown
  > [!NOTE]
  > **Machine translation, needs review**
  >
  > This page was translated automatically. The English page is binding. How to report a
  > mistake: [Translations](../../en/contributing/translations.md#send-your-work).
  ```

  The path depends on the depth of the page and on whether `contributing/translations.md` exists
  in your language.

- A translated heading keeps the English anchor with an explicit id:
  `## Wyślij swoją pracę {#send-your-work}`. Links from every language then use the same anchor.
- The text of a link to a page is the title of that page (in the translation: its translated
  title), the same everywhere: `[Transmitting on the CAN bus](../safety/transmit.md)`.

- Safety pages are checked by a person who speaks the language before they are published.
- Use the terms of the [glossary](en/contributing/glossary.md), shared with the web panel
  dictionaries (`firmware/web/i18n/*.json`). One thing has one name in the panel and in the
  documentation. If you find a better word, change the glossary and the dictionaries together.
- Do not translate: code, console output, console commands, the `can0 ID#DATA` recording format,
  JSON keys, frame IDs, environment names, button names printed in the car (`SRC`, `LIST`).
- Keep heading anchors working: when a translated heading changes its id, fix the links that point
  to it in the same language.

## Licence of the text

Documentation is CC BY 4.0 (`LICENSE-docs`). Write only text you may
publish under it: your own words, your own photos and measurements.
