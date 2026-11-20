# Ihr Auto teilen

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Sie haben ein Auto vermessen, das nicht in `vehicles/SUPPORTED.md` steht. Ihre Notizen helfen der
nächsten Person mit demselben Auto, auch ohne Firmware-Profil. Diese Seite erklärt, was Sie teilen,
wie Sie es vorher bereinigen und wohin jede Datei gehört. Die allgemeinen Regeln für Beiträge
stehen in `CONTRIBUTING.md` im Stammverzeichnis des Repositorys.

Das ist ein Hobbyprojekt mit einem Maintainer. Eine Antwort oder ein Review innerhalb einer
bestimmten Zeit ist nicht zugesagt.

## Was Sie teilen können {#what-you-can-share}

| Beitrag | Wohin |
|---|---|
| Eine Beschreibung des Autos und seines Busses | `vehicles/<make>-<model>-<year>/README.md`, aus `vehicles/_template/README.md` |
| Frames und Signale | `vehicles/<make>-<model>-<year>/<car>.dbc`, jeder Kommentar beginnt mit seinem Status |
| Steckeransichten und Pin-Tabellen | `vehicles/<make>-<model>-<year>/wiring.md`, mit der Seite, von der Sie geschaut haben |
| Aufzeichnungen, die ein Test nutzt | `firmware/test_data/captures/`, mit einer Zeile in dessen `README.md` |
| Ein Firmware-Profil | `firmware/lib/car_<name>/` ([Ein Fahrzeugprofil schreiben](write-a-profile.md)) |
| Der Host-Test dazu | `firmware/test_host/<name>_car.cpp`, die Referenz in `firmware/test_host/expected/`, ein Block in `firmware/tools/run_host_tests.sh` ([Ihr Profil testen](test-your-profile.md)) |
| Was Ihr Radio mit jedem Tastencode macht | Ihre Fahrzeugseite, Abschnitt „Radio side“ ([Das Radio zuordnen](radio-mapping.md)) |
| Eine Zeile für die Liste der unterstützten Fahrzeuge | `vehicles/SUPPORTED.md`: Fahrzeug, Profil, Status, was funktioniert, Link zu Ihrer Seite |

Ein Pull Request nur mit der Fahrzeugseite ist willkommen. Ebenso ein Issue mit der Vorlage
„New vehicle“, wenn Sie Git nicht nutzen.

## Die Regeln für Fakten {#the-rules-for-facts}

- Jeder Fakt hat einen Status: **gemessen** (measured; wie: Multimeter, Aufzeichnung, Konsole des
  Geräts; bei einer Aufzeichnung die Datei und der Schritt) oder **unbestätigt** (unverified; die
  Quelle: eine öffentliche Datenbank, ein Forum, eine Zeichnung eines Verkäufers).
- Eine Schlussfolgerung ist keine Messung. „Die Spannungen sehen nach fehlertolerantem Low-Speed-CAN
  aus“ ist eine Schlussfolgerung; sagen Sie, was sie bestätigen würde.
- Schreiben Sie nur, was Sie gesehen haben. „Nicht gemessen“ ist ein guter Eintrag.
- Nur Ihre eigene Arbeit: Ihre Aufzeichnungen, Fotos und Worte. Schicken Sie keinen dekompilierten
  Code, keine Firmware oder Apps von Box- oder Radioherstellern, keine daraus entnommenen Tabellen,
  keine Datenblätter und keinen Code, der aus Projekten mit inkompatibler Lizenz kopiert ist.
  Verlinken Sie eine öffentliche Quelle, statt sie zu kopieren.

## Bereinigen Sie zuerst Ihre Aufzeichnungen {#clean-your-recordings-first}

Eine Aufzeichnung des Busses kann die VIN Ihres Autos enthalten, und sie zeigt, wann und wie Sie
gefahren sind. Bereinigen Sie jede Datei, bevor sie Ihren Computer verlässt.

### 1. Die Frames finden, die die VIN tragen {#1-find-the-frames-that-carry-the-vin}

Im Referenzauto steht die VIN in den Frames `336`, `3B6` und `2B6` (gemessen: Aufzeichnungen des
Referenzautos). Diese drei IDs sind die Standardwerte des Bereinigungswerkzeugs und von
`record.py`. **Bei einer anderen Marke kann die VIN in anderen Frames stehen oder gar nicht auf
diesem Bus.** Finden Sie es heraus, bevor Sie teilen:

1. Nehmen Sie Ihre VIN aus dem Fahrzeugschein. Tippen Sie sie in keine Datei, die Sie teilen.
2. Die VIN hat 17 Buchstaben und Ziffern. Wandeln Sie die ersten drei Zeichen in hexadezimale
   ASCII-Codes um: zum Beispiel `V` = `56`, `F` = `46`, `3` = `33`.
3. Suchen Sie diese Bytes in einem Texteditor in Ihrer Aufzeichnung, danach die Hex-Codes der
   übrigen Zeichen. Jeder Frame, dessen Daten einen Teil der VIN ergeben, ist ein VIN-Frame. Als
   Text passen 17 Zeichen nicht in einen 8-Byte-Frame, rechnen Sie also mit mehr als einem. Passt
   nichts, sendet das Auto die VIN auf diesem Bus womöglich nicht als Text; schreiben Sie das auf
   Ihre Fahrzeugseite.
4. Notieren Sie die IDs. Finden Sie keine, schreiben Sie „VIN frames: none found“ auf Ihre
   Fahrzeugseite.

### 2. Die Frames überschreiben {#2-overwrite-them}

Aus dem Stammverzeichnis des Repositorys:

```bash
python -I tools/sanitize_capture.py my_drive.log my_drive_clean.log --ids 336,3B6,2B6,ABC
python -I tools/sanitize_capture.py --check my_drive_clean.log --ids 336,3B6,2B6,ABC
```

Ersetzen Sie `ABC` durch die IDs Ihrer VIN-Frames (behalten Sie die drei Standardwerte; sie
schaden nicht). Das Werkzeug überschreibt die Daten dieser Frames mit Nullen derselben Länge:
Zeilenzahl, Zeiten, IDs und Längen bleiben, wie sie waren, damit Tests weiter funktionieren. Es
ersetzt außerdem eine Kopfzeile, die Ihren seriellen Port nennt, durch `# capture`. `--check` muss
`0 problem(s)` melden.

### 3. Die Kommentare selbst lesen {#3-read-the-comments-yourself}

Das Werkzeug übernimmt Kommentarzeilen (`# ...`) unverändert. Ihre Markierungstexte stehen darin.
Öffnen Sie die Datei und suchen Sie nach Namen, Orten, Adressen, Netzwerknamen oder anderem
Privatem.

### 4. Alles prüfen, was Sie hinzufügen {#4-scan-everything-you-add}

```bash
python -I tools/scan_private.py .
```

Das Skript sucht nach VINs, VIN-Frames mit Daten, E-Mail-Adressen, Windows-Benutzerpfaden, MAC-Adressen und
Gerätenetz-Namen mit echtem Suffix. Es muss ohne Fehler enden.

> [!WARNING]
> **Der Scanner kennt nur das VIN-Format des Referenzautos**
>
> `scan_private.py` erkennt eine VIN der PSA-Gruppe (`VF3` + 14 Zeichen) und die VIN-Frames von
> PSA. Die VIN einer anderen Marke oder Ihre eigenen VIN-Frames kennt er nicht. Die Schritte 1
> bis 3 bleiben Ihre Aufgabe.

## Bevor Sie einen Pull Request öffnen {#before-you-open-a-pull-request}

Die Pull-Request-Vorlage hat eine Checkliste. Für ein neues Auto sind das die wichtigen Teile:

| Prüfung | Befehl | Erwartet |
|---|---|---|
| Aufzeichnungen bereinigt | `python -I tools/sanitize_capture.py --check <files> --ids ...` | `0 problem(s)` |
| Keine privaten Daten | `python -I tools/scan_private.py .` | keine Fehler |
| Host-Tests (aus `firmware/`) | `bash tools/run_host_tests.sh` | `All tests match the references.` |
| Schichtung (aus `firmware/`) | `python -I tools/check_layering.py` | `Layering OK.` |
| Ihr Build (aus `firmware/`) | `uvx platformio run -e listen_mycar` | `SUCCESS` |
| Links der Dokumentation, falls Sie `docs/` geändert haben | `python -I tools/check_doc_links.py` | keine toten Links |

Ihre `platformio_override.ini` bleibt auf Ihrem Computer: Sie wird nicht versioniert. Schreiben Sie
im Pull Request, welche Build-Umgebung Sie für Ihr Auto genutzt haben und ob Sie die Firmware auf
einer Platine und im Auto ausprobiert haben.

Ändern Sie in einem Pull Request, der ein Auto hinzufügt, keine bestehenden Testreferenzen
(`firmware/test_host/expected/`, `golden/`, `golden_060/`). Eine eigene Referenzdatei hinzuzufügen
ist in Ordnung.

## Senden gehört nicht zu einem Fahrzeugbeitrag {#transmission-is-not-part-of-a-car-contribution}

Ein mit `-DCAR_PROFILE=CUSTOM` gebautes Profil kann nicht senden, und die Firmware kann zur
Laufzeit keinen Sende-Frame hinzufügen. Ein Frame, den das Gerät senden darf, braucht eine eigene
Messung, eine in C++ geschriebene Inhaltsprüfung, einen Host-Test und ein benanntes Profil in
`firmware/src/config.h`. Das ist eine eigene Diskussion und gehört nicht zum Hinzufügen eines
Autos: Lesen Sie [Senden auf dem CAN-Bus](../../en/safety/transmit.md) und öffnen Sie zuerst
ein Issue.

## Lizenzen {#licences}

Mit einem Beitrag stimmen Sie zu, dass er unter den Lizenzen dieses Repositorys veröffentlicht
wird: Code Apache-2.0, Dokumentation CC BY 4.0, Hardware CERN-OHL-S-2.0. Diese Lizenzen sind bis
zur ersten öffentlichen Version vorläufig.

Dieses Projekt ist mit keinem Auto-, Radio- oder CAN-Box-Hersteller verbunden. Nennen Sie sie nur,
um zu sagen, was Sie getestet haben.
