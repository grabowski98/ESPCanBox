<!-- Project name: "project_name" in project.json. tools/set_project_name.py --apply rewrites the title line ("# ...") under the logo. The logo is a drawing: no tool rewrites it; the animated header logo is generated from it by tools/make_logo_animation.py (docs/en/contributing/brand.md). -->
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/brand/logo-animated.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/brand/logo-animated.svg">
    <img src="docs/assets/brand/logo-transparent.svg" alt="Projektlogo: eine Punktmatrix-Anzeige mit dem Projektnamen" width="527">
  </picture>
</p>

# ESPCanBox

Ein offenes Gerät mit ESP32-S3. Es sitzt zwischen dem CAN-Bus eines Autos und einem
Android-Radio und kann die nachgerüstete CAN-Box ersetzen, die mit dem Radio verkauft wird.

[English](README.md) | [Polski](README.pl.md) | Deutsch | [Español](README.es.md) | [Русский](README.ru.md) | [Українська](README.uk.md)

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend.** Diese Datei ist eine Übersetzung von
> [README.md](README.md). Verbindlich ist der englische Text. Fehler in der Übersetzung melden:
> [Übersetzungen](docs/en/contributing/translations.md#send-your-work).

> [!WARNING]
> **Hobbyprojekt, WIE ES IST, ohne jede Gewähr.** Die Nutzung erfolgt auf eigenes Risiko, am
> eigenen Auto.
>
> - Standardmäßig hört das Gerät auf dem Fahrzeugbus **nur mit**. Zum Senden brauchen Sie einen
>   eigenen Build (`decoder_tx_wifi`) und müssen ihn von Hand scharf schalten. Lesen Sie
>   [Senden auf dem CAN-Bus](docs/en/safety/transmit.md), bevor Sie ihn bauen.
> - Setzen Sie es für nichts ein, was die Sicherheit betrifft (Bremsen, Lenkung, Airbags,
>   gesetzlich vorgeschriebene Beleuchtung). Dafür ist es weder ausgelegt noch getestet.
> - Firmware und Platine wurden an **einem** Auto mit **einem** Radio entwickelt und gemessen.
>   Jedes andere Auto ist ungetestet.
> - Dieses Projekt ist nicht mit Peugeot, Stellantis, Simple Soft, Raise, Junsun oder FYT
>   verbunden und wird von ihnen weder unterstützt noch empfohlen. Ihre Namen stehen hier nur, um
>   zu sagen, was getestet wurde.

## Bildschirmfotos

<table>
  <tr>
    <td align="center" valign="top" width="50%"><a href="docs/assets/screenshots/panel-dashboard.png"><img src="docs/assets/screenshots/panel-dashboard.png" alt="Übersicht, Motor läuft, vordere Sensoren piepen" width="250"></a><br><sub>Übersicht: Autozustand und Parksensoren</sub></td>
    <td align="center" valign="top" width="50%"><a href="docs/assets/screenshots/panel-can.png"><img src="docs/assets/screenshots/panel-can.png" alt="CAN-Reiter: Frame-Tabelle und Live-Vorschau einer Aufnahme" width="250"></a><br><sub>CAN-Reiter: Frames live</sub></td>
  </tr>
  <tr>
    <td align="center" valign="top" width="50%"><a href="docs/assets/screenshots/panel-radio-keymap.png"><img src="docs/assets/screenshots/panel-radio-keymap.png" alt="Editor der Tastenbelegung mit den Standard-Tauschungen und der zuletzt gedrückten Taste" width="250"></a><br><sub>Radio-Reiter: Tastenbelegung</sub></td>
    <td align="center" valign="top" width="50%"><a href="docs/assets/screenshots/panel-system-firmware-wizard.png"><img src="docs/assets/screenshots/panel-system-firmware-wizard.png" alt="Update-Assistent: eine Decoder-Datei für ein reines Mithör-Gerät braucht ein ausdrückliches Häkchen" width="250"></a><br><sub>Firmware-Update im Browser</sub></td>
  </tr>
</table>

Alle Ansichten mit Erklärung: [Rundgang durch das Panel](docs/en/reference/panel-tour.md) (Englisch). Die Bilder stammen vom Geräte-Mock, nicht aus einem Auto.

## Für wen es ist

Für Bastler, die eine kleine Platine löten, ein Multimeter bedienen und Firmware mit PlatformIO
bauen können (die Anleitungen zeigen jeden Befehl). Was Sie bekommen, hängt von Auto und Radio
ab:

- **Peugeot 508 SW (PSA AEE2010) mit einem Android-Radio, das das „XP“-Protokoll spricht:** Das
  Gerät kann die CAN-Box ersetzen: Tasten, Parksensoren, Bordcomputer und Stromversorgung des
  Radios.
- **Jedes andere Auto:** CAN-Bus mithören und aufzeichnen (Build `listen_generic_wifi`), dann
  dekodieren und ein eigenes Fahrzeugprofil schreiben, mit den Anleitungen
  [Für Ihr Auto anpassen](docs/de/make-it-yours/porting-checklist.md). An das Radio wird nichts
  gesendet, bis Ihr Profil existiert.
- **Jedes andere Radio:** „XP“ ist bisher das einzige Radioprotokoll. Ein anderes Radio braucht
  ein neues Protokoll in der Firmware ([Radio-Zuordnung](docs/de/make-it-yours/radio-mapping.md)).

## Was es macht

- **Hört den CAN-Bus des Autos mit** und sendet in den Standard-Builds nie. Nur der eigene Build
  `decoder_tx_wifi` enthält Sendecode; ein Skript prüft das nach jedem Build.
- **Piept für die Parksensoren** aus dem eigenen kleinen Lautsprecher.
- **Spricht mit dem Radio** im Decoder-Modus, wie es die CAN-Box tat: Tasten an Lenkrad und
  Mittelkonsole, Anzeige der Parksensoren, Bordcomputer, Außentemperatur, Zustand der
  Klimaanlage, Türen und Fahrzeugeinstellungen.
- **Schaltet drei 12-V-Leitungen** für das Radio im Decoder-Modus: ACC (Stromversorgung des
  Radios), Beleuchtung, Rückwärtsgang.
- **Web-Panel** über ein eigenes WLAN: Status, CAN-Frames live, Konsole, Aufzeichnung,
  Firmware-Aktualisierung. Sprachen des Panels: Englisch, Polnisch, Deutsch, Spanisch, Russisch,
  Ukrainisch (die letzten vier sind maschinelle Übersetzungen, die noch geprüft werden müssen).
- **USB-Konsole** mit festem Zeilenformat, das Werkzeuge lesen können
  (siehe [Konsole](docs/en/reference/console.md)).
- **Fahrzeugprofile und Radioprotokolle werden beim Bauen gewählt:** `psa_aee2010`
  (Referenzauto) oder `generic` (jedes Auto, nur Mithören); Radioprotokoll `XP` oder `NONE`. Eine
  Vorlage für Ihr eigenes Auto liegt in `firmware/lib/car_example`
  (siehe [Profil schreiben](docs/de/make-it-yours/write-a-profile.md)).
- **Tests auf Ihrem Computer** für die ganze Logik, mit Aufzeichnungen aus dem Auto, ohne
  Hardware (siehe [Host-Tests](docs/en/testing/host-tests.md)).
- **Optionaler Sende-Build** (`decoder_tx_wifi`): Standardmäßig kann er nur einen Frame senden,
  die Anfrage für Fahrzeugeinstellungen `15B` (nur nach ID und Länge geprüft), und das erst, wenn
  Sie ihn scharf schalten. Den Versuchs-Frame `1A9` gibt es nur in einem Build mit
  `-DCAN_TX_EXPERIMENTAL=1`. Die Klimaanlage über das Radio zu steuern, hat auf diesem Weg **nicht** funktioniert.
  Details: [Senden auf dem CAN-Bus](docs/en/safety/transmit.md).

Stand: Version 2.0.0 kompiliert, besteht die Host-Tests, wurde auf dem Prüftisch auf einer Platine ESP32-S3-WROOM-1-N16R8 getestet und am 20.11.2026 in einem einzigen Auto ausprobiert, einschließlich Tiefschlaf und Aufwachen durch den Bus. Details: [CHANGELOG.md](CHANGELOG.md).

## Unterstützte Autos

| Auto | Firmware-Profil | Status |
|---|---|---|
| Peugeot 508 SW, 2015 (PSA AEE2010) | `psa_aee2010` | gemessen (measured) an einem Auto |
| jedes Auto | `generic` (Build `listen_generic_wifi`) | nur Mithören: zeichnet Frames auf, sendet nichts an das Radio, 12-V-Ausgänge aus |

Am Referenzauto läuft der Bus am Radio mit 125 kbit/s (gemessen). Seine Spannungen sehen nach
fehlertolerantem Low-Speed-CAN aus (CANH etwa 0.65 V, CANL etwa 4.5 V im Mittel bei
eingeschalteter Zündung); mit einem Oszilloskop ist das noch nicht bestätigt. Details:
`vehicles/peugeot-508-sw-2015/README.md` und `vehicles/SUPPORTED.md`.

## Unterstützte Radios

| Radioprotokoll | Firmware | Status |
|---|---|---|
| „XP“ (Protokoll der Simple-Soft-CAN-Box, Android-Radio auf der FYT-Plattform, CAN-Profil „XP Simple Soft -> Peugeot 508 -> Type 2“) | `-DRADIO_PROTOCOL=XP` | gemessen an einem Radio (UART-Aufzeichnungen der Box und des Radios) |
| kein Radio | `-DRADIO_PROTOCOL=NONE` | auf dem UART wird nichts gesendet |

Beschreibung des Protokolls, nur aus Aufzeichnungen: `radios/xp-simple-soft/protocol.md`.

## Schnellstart

Damit läuft das Gerät auf Ihrem Schreibtisch, ohne Auto. Sie brauchen ein ESP32-S3-Board mit
16 MB Flash und 8 MB PSRAM (N16R8) und ein USB-Kabel. Vollständige Anleitung:
[Schnellstart](docs/de/quick-start/flash.md).

1. **Lesen Sie [Sicherheit](docs/de/getting-started/safety.md).** Vorher kommt nichts ins Auto.
2. **Werkzeuge installieren.** Installieren Sie [uv](https://docs.astral.sh/uv/); es startet
   PlatformIO ohne eigene Installation. Git for Windows (Git Bash) brauchen Sie zum Flashen nicht:
   nur für die Tests auf dem Computer und für die Seiten von „Für Ihr Auto anpassen“ (eigenes Auto
   oder Radio).
3. **Den Build „nur Mithören“ mit Web-Panel bauen und flashen.** Tippen Sie das im obersten
   Ordner des Repositorys (unter Windows in PowerShell):

   ```
   uvx platformio run -d firmware -e listen_wifi -t upload
   ```

4. **USB-Konsole öffnen** (115200 Baud), zum Beispiel
   `uvx platformio device monitor -d firmware`. Ihre Eingabe wird nicht angezeigt; drücken Sie
   nach jedem Befehl Enter. Tippen Sie `ver`, dann `stat`, dann `wifi`
   (Netzwerkname) und `wifi showpass` (Passwort).
   Halten Sie das Passwort aus jedem Log heraus, das Sie teilen.
5. **Panel öffnen.** Verbinden Sie ein Telefon oder einen Laptop mit dem WLAN des Geräts (den
   Namen gibt `wifi` aus), öffnen Sie `http://10.50.8.1` und melden Sie sich mit diesem Passwort
   an. Wählen Sie Ihre Sprache auf dem Anmeldebildschirm.

Für ein anderes Auto als das Referenzauto nehmen Sie später `listen_generic_wifi`.

Weiter: [Prüftisch-Test](docs/de/quick-start/bench-test.md), dann
[Für Ihr Auto anpassen](docs/de/make-it-yours/porting-checklist.md) für ein anderes Auto oder
[Einbau im Auto](docs/en/install-in-car/checklist.md) für das Referenzauto.

Noch keine Platine? Das Mock-Gerät startet das Panel auf Ihrem Computer mit einer Aufzeichnung:
`python -I firmware/tools/mock_device.py firmware/test_data/captures/can_20261111_191045.log`
(unter Linux und macOS: `python3`; siehe [Erster Start](docs/de/quick-start/first-boot.md)).

## Dokumentation

- [Überblick](docs/de/index.md) und [So funktioniert es](docs/en/getting-started/how-it-works.md)
- [Sicherheit](docs/de/getting-started/safety.md), [Senden auf dem CAN-Bus](docs/en/safety/transmit.md),
  [Sicherheitsmodell](docs/en/safety/security.md)
- [Hardware](docs/en/hardware/overview.md): Prototyp-Platine, Stückliste, Gehäuse, Verkabelung
- [Für Ihr Auto anpassen](docs/de/make-it-yours/porting-checklist.md): eigenes Auto aufzeichnen,
  dekodieren und hinzufügen
- [Referenz](docs/en/reference/architecture.md): Protokoll, Konsole, Panel-API, Pins
- [Fehlersuche](docs/en/troubleshooting/index.md) und [FAQ](docs/en/troubleshooting/faq.md)

Am besten liest man die Dokumentation als gebaute Seite: mit Menü, Suche und Sprachumschalter. Um
sie auf Ihrem Computer zu sehen, führen Sie das im obersten Ordner des Repositorys aus und öffnen
http://127.0.0.1:8000:

```
uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs serve
```

Die Adresse der Seite im Netz wird hier nach der Veröffentlichung ergänzt. Die Dateien in `docs/`
lassen sich auch direkt auf GitHub lesen; alle Links zwischen ihnen funktionieren auch dort.

## Sprachen

- Dokumentation: Englisch (`docs/en/`, verbindlich). Maschinelle Übersetzung, noch nicht
  geprüft: [13 Seiten auf Deutsch](docs/de/index.md) (Überblick, Sicherheit, Schnellstart, „Für Ihr
  Auto anpassen“); fehlt eine Seite, wird die englische gezeigt. So helfen Sie mit:
  [Übersetzungen](docs/en/contributing/translations.md).
- Web-Panel: en, pl, de, es, ru, uk in `firmware/web/i18n/`.
- USB-Konsole, Konsolenbefehle und API-Codes: nur Englisch, mit Absicht (Werkzeuge lesen sie).

## Mitmachen

Siehe [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) und
[ROADMAP.md](ROADMAP.md). Sicherheitsprobleme: [SECURITY.md](SECURITY.md). Änderungen:
[CHANGELOG.md](CHANGELOG.md).

Das ist ein Hobbyprojekt. Fragen und Pull Requests sind willkommen, aber es gibt kein
Versprechen auf eine Antwort oder eine Korrektur innerhalb einer bestimmten Zeit.

## Lizenzen

- Code: Apache-2.0 ([LICENSE](LICENSE), [NOTICE](NOTICE));
- Dokumentation und Bilder: CC BY 4.0 ([LICENSE-docs](LICENSE-docs));
- Hardware (KiCad-Dateien, Gehäuse): CERN-OHL-S-2.0 ([LICENSE-hardware](LICENSE-hardware)).

Welche Lizenz für welche Dateien gilt:

| Lizenz | Dateien |
|---|---|
| Apache-2.0 | `firmware/` (mit Web-Panel, Testdaten und Aufzeichnungen), `tools/`, der Code in `extras/`, `vehicles/*/*.dbc`, `hardware/prototype-v1/enclosure/gen_enclosure.py`, Build- und CI-Dateien sowie jede Datei, die in den zwei Zeilen darunter nicht genannt ist |
| CC BY 4.0 | `docs/`, jede Markdown-Datei (`*.md`), die Logos (`docs/assets/brand/`, `hardware/brand/`), Fotos, Bildschirmfotos, Renderings und Zeichnungen |
| CERN-OHL-S-2.0 | in `hardware/prototype-v1/`: die KiCad-Dateien, die PDF- und Druckdateien, `bom.csv`, die Gehäusemodelle (STL) |

**Lizenz der einzelnen Dateien.** Die Dateien haben keinen eigenen Lizenzkopf. Für eine Datei
ohne eigenen Kopf gilt die Tabelle oben; eine Datei, die ihre eigene Lizenz nennt, steht unter
dieser Lizenz. Die Teile anderer Projekte, die ein Firmware-Image enthält (Arduino-Core für ESP32
unter LGPL-2.1-or-later, ESP-IDF und seine Komponenten, die Laufzeitbibliotheken des Compilers),
stehen mit Lizenz und Link in [NOTICE](NOTICE). Jedes Release-Archiv enthält die Lizenztexte,
NOTICE und einen Hinweis, wo der Quellcode genau dieser Version liegt
([LICENSES/](LICENSES/THIRD-PARTY.md)).

Peugeot, Stellantis, Simple Soft, Raise, Junsun, FYT und die anderen Produkt- und Firmennamen
gehören ihren Inhabern. Sie stehen hier nur, um die Kompatibilität zu beschreiben. Dieses Projekt
ist mit keinem von ihnen verbunden und wird von keinem unterstützt. ESP32 ist eine Marke von
Espressif Systems; andere Namen sind Marken ihrer jeweiligen Inhaber und stehen hier nur, um die
Kompatibilität zu beschreiben.

Autor: grabowski98.
