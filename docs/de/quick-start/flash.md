# Firmware flashen

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Diese Seite führt Sie von einem frischen Computer bis zu einer Platine, auf der die Firmware läuft
und ihr Web-Panel öffnet. Die Schritte 1-3 funktionieren ohne Platine.

> [!NOTE]
> **Stand dieses Firmware-Baums**
>
> Diese Firmware lässt sich bauen und besteht die Tests auf einem PC (alle Umgebungen bauen, die
> Host-Tests stimmen mit ihren Referenzen überein). Auf einer Platine wurde sie noch nicht mit dem
> [Prüftisch-Protokoll](../../en/testing/bench-test-protocol.md) geprüft. Frühere Builds desselben
> Codes liefen im Referenzauto.

**In welches Fenster Sie tippen.** Unter Windows tippen Sie alle Befehle dieser Seite in
**PowerShell** (oder Windows Terminal). Git Bash brauchen Sie zum Flashen nicht, nur für die Tests
auf dem Computer und für die Seiten von [Für Ihr Auto anpassen](../make-it-yours/porting-checklist.md). Unter Linux und macOS nehmen Sie das normale Terminal.

## 1. uv installieren {#1-install-uv}

Die Firmware wird mit [PlatformIO](https://platformio.org/) gebaut. Sie installieren es nicht
selbst: `uvx platformio ...` lädt es über [uv](https://docs.astral.sh/uv/), ein kleines Werkzeug
zum Starten von Programmen, herunter und startet es.

### Windows {#windows}

1. Öffnen Sie PowerShell und führen Sie aus:

    ```powershell
    winget install --id=astral-sh.uv -e
    ```

2. Schließen Sie PowerShell und öffnen Sie es neu, damit es `uv` findet.

Zum Flashen nicht nötig; nötig für die Tests auf dem Computer und für „Für Ihr Auto anpassen“:
[Git for Windows](https://git-scm.com/download/win). Es bringt **Git Bash** mit, das die Testskripte
und die Befehle dieser Seiten ausführt.

### Linux {#linux}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Damit Sie den USB-Port ohne root nutzen können, fügen Sie sich der Gruppe für serielle Ports
hinzu (`dialout` unter Debian und Ubuntu, `uucp` unter Arch) und installieren Sie die
udev-Regeln von PlatformIO wie in der
[PlatformIO-Dokumentation](https://docs.platformio.org/en/latest/core/installation/udev-rules.html)
beschrieben. Melden Sie sich ab und wieder an.

### macOS {#macos}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

oder mit Homebrew `brew install uv`.

### Prüfen {#check-it}

Öffnen Sie ein neues Terminal und führen Sie aus:

```text
uvx platformio --version
```

Erwartet: eine Zeile mit der PlatformIO-Version, zum Beispiel `PlatformIO Core, version 6.2.0`.
Der erste Aufruf lädt PlatformIO herunter und dauert einen Moment; spätere Aufrufe starten sofort.
Sehen Sie „command not found“ oder „not recognized“, schließen Sie das Terminal, öffnen Sie ein
neues und versuchen Sie es noch einmal.

Wenn Sie lieber mit einem Editor arbeiten: Die PlatformIO-Erweiterung für VS Code geht auch.
Öffnen Sie den Ordner `firmware` und wählen Sie die Umgebung in der Statusleiste. Die Befehle
unten sind das, was die Erweiterung für Sie ausführt.

## 2. Code holen {#2-get-the-code}

1. Öffnen Sie die Projektseite (das Repository, in dem Sie diese Anleitung gefunden haben) und
   wählen Sie **Code → Download ZIP**. Wenn Sie Git nutzen, geht auch `git clone`.
2. Entpacken Sie die ZIP-Datei. Unter Windows: Rechtsklick darauf → **Alle extrahieren**.
3. Öffnen Sie den entpackten Ordner, in dem `firmware`, `docs` und `README.md` nebeneinander
   liegen. Nach dem Entpacken liegt er oft eine Ebene tiefer, in einem Ordner mit demselben Namen.
4. Öffnen Sie ein Terminal im Ordner `firmware` darin:
    - **Windows:** Öffnen Sie `firmware` im Datei-Explorer, klicken Sie in die Adressleiste,
      tippen Sie `powershell` und drücken Sie Enter. (Unter Windows 11 geht auch Rechtsklick auf
      eine freie Stelle → **In Terminal öffnen**.)
    - **Linux und macOS:** Öffnen Sie ein Terminal, tippen Sie `cd ` (mit Leerzeichen), ziehen
      Sie den Ordner `firmware` in das Fenster und drücken Sie Enter.

Alle Befehle unten führen Sie in diesem Terminal aus, im Ordner `firmware`.

## 3. Bauen {#3-build}

```text
uvx platformio run -e listen_wifi
```

Dieses Programm hört nur das Auto mit und hat das Web-Panel. Ohne Panel nehmen Sie hier und in
Schritt 5 `-e listen`.

Der erste Build lädt die ESP32-Plattform (`espressif32@8.0.0`, in `platformio.ini` fest
eingestellt) und den Compiler herunter. Das ist ein großer Download und kann viele Minuten
dauern; brechen Sie ihn nicht ab. Spätere Builds verwenden ihn wieder.

Erwartet: Am Ende gibt PlatformIO `SUCCESS` aus. Das Programm liegt in
`.pio/build/listen_wifi/firmware.bin`. Sehen Sie stattdessen `Error`, lesen Sie die erste
Fehlerzeile: Ein Netzwerkfehler bedeutet, dass der Download abgebrochen ist; führen Sie den Befehl
dann noch einmal aus.

### Welche Umgebung {#which-environment}

Eine **Umgebung** ist ein benannter Satz von Build-Optionen in `firmware/platformio.ini`. Sie
legt fest, was das Gerät tun darf. Auf dem Schreibtisch reicht `listen_wifi`. Für ein anderes Auto
als das Referenzauto nehmen Sie später `listen_generic_wifi`.

<details class="note" markdown="1">
<summary>Alle sechs Umgebungen, und alle auf einmal bauen</summary>

Ändern Sie den Modus nie durch Bearbeiten von `src/config.h`; wählen Sie stattdessen die
Umgebung.

| Umgebung | Modus | Web-Panel und Aktualisierung über WLAN | Sendet auf den Fahrzeugbus | Wofür |
|---|---|---|---|---|
| `listen` (Standard) | Parallelmodus (neben der Box) | nein | nein (kein Sendecode im Programm) | erstes Flashen, Prüftisch, Mithören neben der CAN-Box |
| `decoder` | Decoder | nein | nein | CAN-Box ersetzen, ohne Netzwerk |
| `listen_wifi` | Parallelmodus | ja | nein | Mithören mit dem Web-Panel |
| `decoder_wifi` | Decoder | ja | nein | CAN-Box ersetzen, mit dem Web-Panel |
| `decoder_tx_wifi` | Decoder | ja | **nur Frame `15B`, nach `cantx on` und einem Neustart** | Personalisierung über den Radiobildschirm ändern; zuerst [Senden auf dem CAN-Bus](../../en/safety/transmit.md) lesen |
| `listen_generic_wifi` | Parallelmodus | ja | nein | anderes Auto: Profil `generic`, Radio `none`, zeichnet den Bus auf und sendet nichts an das Radio |

Der „Parallelmodus“ ist der Modus „nur Mithören“: Das Gerät arbeitet neben der CAN-Box und
sendet nichts. Die Konsole nennt denselben Modus `alongside the box`.

Ein einfaches `uvx platformio run` (ohne `-e`) baut `listen`, den sicheren Modus ohne
Netzwerk.

Um alle sechs Umgebungen auf einmal zu bauen, führen Sie `make build` im obersten Verzeichnis
aus (braucht `make`, das in Git Bash nicht enthalten ist; die einfachen Befehle
`uvx platformio run -e ...` funktionieren immer).

Optionale Einstellungen, die auf Ihrem Computer bleiben, gehören in
`firmware/platformio_override.ini` (kopieren Sie `platformio_override.sample.ini`). Das
Beispiel zeigt eine eigene Platine (`listen_mine`) und ein eigenes Fahrzeugprofil
(`listen_mycar`). Den Versuchs-Frame `1A9` gibt es nur, wenn Sie dort einem
Sende-Build absichtlich `-DCAN_TX_EXPERIMENTAL=1` hinzufügen.

</details>

> [!WARNING]
> **Decoder-Modus nur ohne CAN-Box**
>
> Flashen Sie eine `decoder*`-Umgebung nur, wenn die nachgerüstete CAN-Box abgezogen ist, oder
> auf dem Prüftisch. Zwei Geräte an der Datenleitung des Radios und zwei Quellen am ACC-Draht
> schließen gegenseitig ihre Ausgänge kurz.

## 4. Platine anschließen {#4-connect-the-board}

1. Stecken Sie die Platine mit einem USB-Kabel, das Daten überträgt (kein reines Ladekabel), am
   Computer an. Bei Platinen mit zwei USB-Buchsen nehmen Sie die, die direkt mit dem ESP32-S3
   verbunden ist (oft mit `USB` beschriftet, nicht `UART`): Diese Firmware spricht über das
   native USB des Chips.
2. Ports auflisten:

    ```text
    uvx platformio device list
    ```

Erwartet: Nach dem Anstecken der Platine erscheint ein neuer Eintrag. Windows 10 und 11 brauchen
für diesen Port normalerweise keinen zusätzlichen Treiber.

| System | Der Port sieht so aus |
|---|---|
| Windows | `COM5` (beliebige Nummer) |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem1101` (beliebige Nummer) |

Kein neuer Eintrag? Versuchen Sie ein anderes Kabel. Fehlt er weiterhin, starten Sie den
Bootloader von Hand, siehe
[Wenn der Upload nicht startet](#if-the-upload-does-not-start-bootloader-mode).

## 5. Hochladen {#5-upload}

```text
uvx platformio run -e listen_wifi -t upload
```

PlatformIO findet den Port selbst. Wenn mehrere Platinen angeschlossen sind, geben Sie ihn an:
`uvx platformio run -e listen_wifi -t upload --upload-port COM5`.

Erwartet: Der Upload endet mit `[SUCCESS]`.

- „could not open port“ oder „Access is denied“: Der Port ist belegt. Schließen Sie den Monitor
  (Strg+C) im anderen Fenster und starten Sie den Upload noch einmal.
- Kein Port gefunden: Versuchen Sie ein anderes Kabel, dann den Bootloader-Modus unten.

Ein Upload über USB behält die gespeicherten Einstellungen (Lautstärke, Tastentabelle,
WLAN-Passwort).

### Wenn der Upload nicht startet: Bootloader-Modus {#if-the-upload-does-not-start-bootloader-mode}

Normalerweise setzt das Upload-Werkzeug den ESP32-S3 über USB in seinen eingebauten Bootloader
zurück. Wenn der Port nicht erscheint oder der Upload mit einem Verbindungsfehler abbricht (zum
Beispiel nach einem Programm, das beim Start abstürzt), starten Sie den Bootloader von Hand:

1. Halten Sie die Taste **BOOT** gedrückt (auf manchen Platinen `0` oder `IO0`).
2. Drücken Sie kurz **RESET** (`RST` oder `EN`), oder stecken Sie das USB-Kabel ein, während Sie
   BOOT halten.
3. Lassen Sie BOOT los. Der Port erscheint wieder; führen Sie den Upload-Befehl aus.
4. Drücken Sie nach dem Upload einmal RESET, damit das neue Programm startet.

Das sind Tasten des ESP32-S3-Entwicklungsboards, nicht der Trägerplatine.

## 6. Konsole beobachten {#6-watch-the-console}

```text
uvx platformio device monitor
```

Die Geschwindigkeit (115200) kommt aus `platformio.ini`. Drücken Sie RESET an der Platine, um
die Startzeilen zu sehen. Sie sehen so aus (die erste Zeile beginnt mit dem Projektnamen;
`listen` zeigt `network=no`, `listen_wifi` `network=yes`):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0-beta.1, built <date and time>, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: <reset reason>
```

RESET trennt auch kurz den USB-Port. Bleibt der Monitor stehen oder verpassen Sie die Startzeilen,
starten Sie den Monitor neu und tippen Sie `ver`.

> [!NOTE]
> **Ihre Eingabe wird nicht angezeigt**
>
> Die Buchstaben, die Sie tippen, erscheinen nicht auf dem Bildschirm. Das ist normal: Die
> Firmware gibt sie nicht zurück. Tippen Sie den Befehl blind und drücken Sie Enter; die Antwort
> erscheint in einer neuen Zeile.

Tippen Sie `help` und Enter für die Liste der Befehle, `ver` für die Version, `stat` für den
Zustand. Den Monitor verlassen Sie mit Strg+C.

Ein `FAIL` in der Zeile `can=... audio=... radio=...` bedeutet, dass ein Treiber nicht gestartet
ist. Das Gerät bleibt dann im sicheren Zustand (12-V-Ausgänge aus, nichts wird gesendet), und die
LED auf der Platine leuchtet rot. Es startet nicht in einer Schleife neu.

## Alles löschen {#erasing-everything}

`uvx platformio run -e listen -t erase` löscht den ganzen Flash-Speicher, auch die gespeicherten
Einstellungen. Der nächste Netzwerkstart erzeugt ein neues WLAN-Passwort. Laden Sie danach
wieder ein Programm hoch.

## Weiter {#next}

- [Erster Start](first-boot.md): Passwort lesen und das Web-Panel öffnen.
- [Prüftisch-Test](bench-test.md): die Platine vor dem Auto prüfen.
