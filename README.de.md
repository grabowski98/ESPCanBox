<!-- Project name: "project_name" in project.json. tools/set_project_name.py --apply rewrites the title line ("# ...") under the logo. The logo is a drawing: no tool rewrites it (docs/en/contributing/brand.md). -->
<p align="center">
  <img src="docs/assets/brand/logo-transparent.svg" alt="Projektlogo: eine Punktmatrix-Anzeige mit dem Projektnamen" width="527">
</p>

# ESPCanBox

Ein offenes Gerät mit ESP32-S3. Es sitzt zwischen dem CAN-Bus eines Autos und einem
Android-Radio und kann die nachgerüstete CAN-Box ersetzen, die mit dem Radio verkauft wird.

[English](README.md) | [Polski](README.pl.md) | Deutsch | [Español](README.es.md) | [Русский](README.ru.md) | [Українська](README.uk.md)

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend.** Diese Datei ist eine Übersetzung von
> [README.md](README.md). Verbindlich ist der englische Text. Fehler in der Übersetzung melden:
> [Übersetzungen](docs/en/contributing/translations.md#send-your-work).

![Übersicht des Web-Panels (Geräte-Mock, dunkles Schema)](docs/assets/screenshots/panel-dashboard.png)

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
  Sie ihn scharf schalten. Zwei weitere Versuchs-Frames gibt es nur in einem experimentellen
  Build. Die Klimaanlage über das Radio zu steuern, hat auf diesem Weg **nicht** funktioniert.
  Details: [Senden auf dem CAN-Bus](docs/en/safety/transmit.md).

Nicht fertig oder nicht bekannt: Der Tiefschlaf bei stillem Bus ist geschrieben, aber auf der
Hardware nicht getestet. Die Firmware in diesem Repository wurde aus der Version umgebaut, die im
Referenzauto läuft; sie kompiliert und besteht die Host-Tests, aber dieser Baum wurde **noch
nicht auf dem Prüftisch** an einem Gerät getestet.

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

Vorläufig bis zur ersten öffentlichen Version:

- Code: Apache-2.0 ([LICENSE](LICENSE), [NOTICE](NOTICE));
- Dokumentation und Bilder: CC BY 4.0 ([LICENSE-docs](LICENSE-docs));
- Hardware (KiCad-Dateien, Gehäuse): CERN-OHL-S-2.0 ([LICENSE-hardware](LICENSE-hardware)).

Autor: grabowski98.
