# Den Bus aufzeichnen

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Eine Aufzeichnung (ein Capture) ist eine Textdatei mit jedem CAN-Frame, den das Gerät gesehen hat,
in Reihenfolge und mit Zeit. Sie machen sie, während Sie im Auto etwas tun: eine Taste drücken,
eine Tür öffnen, den Rückwärtsgang einlegen. Später suchen Sie die Bits, die sich in diesen
Momenten geändert haben ([Dekodieren](decode.md)).

Dafür brauchen Sie zuerst die Fakten aus [Ihren CAN-Bus finden](find-your-bus.md): die Leitungen,
den Bustyp und die Bitrate.

> [!CAUTION]
> **Mit einem reinen Mithör-Build aufzeichnen**
>
> Verwenden Sie `listen_generic_wifi` oder den separaten Sniffer in `extras/sniffer/`. Beide hören
> nur mit. Zeichnen Sie ein unbekanntes Auto nicht mit einem Build auf, der senden kann.

## Das Format {#the-format}

Ein Ereignis pro Zeile, überall in diesem Projekt dasselbe Format:

```
(416.094276) can0 532#0800000000000000      CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117                UART bytes (only the sniffer's recorder variant)
# STEP 3 19:06:41.149 Volume + three times   comment line: a marker
# stat rx=30529 ...                          comment line: statistics
```

Zeilen, die mit `#` beginnen, sind Kommentare. Manche Kommentare sind **Markierungen**: Zeilen, die
Sie während der Aufzeichnung hinzufügen, in dem Moment, in dem Sie eine Aktion beginnen.
Erst Markierungen machen eine Aufzeichnung leicht dekodierbar. Jedes Werkzeug schreibt seine
eigene Art:

| Werkzeug | Markierungszeile |
|---|---|
| `record.py` | `# MARKER n (time) computer clock HH:MM:SS: text` |
| `record_serial.py` (Sniffer) | `# STEP n HH:MM:SS.mmm text` (Enter), `# MARK d HH:MM:SS.mmm` (eine Ziffer `1` bis `9` + Enter) |

Mehr zum Format: [Fahrzeugaufzeichnungen](../../en/testing/captures.md).

## Ein Werkzeug wählen {#choose-a-tool}

- Nehmen Sie `firmware/tools/record.py`, wenn Sie das Gerät mit `listen_generic_wifi` und einen
  Laptop in seinem WLAN haben. Die Datei landet in einem Ordner `logs` neben dem
  Repository-Ordner.
- Nehmen Sie `extras/sniffer/record_serial.py`, wenn Sie die separate Sniffer-Platine an USB
  haben. Die Datei landet in dem mit `--out` angegebenen Ordner.
- Nehmen Sie `dump` in der USB-Konsole nur für einen schnellen Blick: Es schreibt keine
  Markierungen.

## Die Schritte zuerst planen {#plan-the-steps-first}

Schreiben Sie die Schritte auf, bevor Sie sich ins Auto setzen. Ein guter Plan:

- **Beginnen Sie mit ein paar Sekunden Nichtstun**, damit die Aufzeichnung das Auto in Ruhe zeigt.
- **Eine Aktion pro Schritt.** „Lautstärke hoch“ und „Lautstärke runter“ sind zwei Schritte.
- **Wiederholen Sie jede Aktion dreimal**, mit 2 bis 3 Sekunden Pause. Ein Bit, das sich dreimal
  ändert, ist Ihr Signal. Ein Bit, das sich ständig ändert, ist ein Zähler oder Rauschen.
- **Setzen Sie die Markierung kurz vor der Aktion**, nicht danach.
- **Notieren Sie den Zustand des Autos** zu Beginn jedes Schritts: Zündung, Motor, Türen, was das
  Radio zeigt.

Ein Beispielplan auf Englisch ist `extras/sniffer/scenarios/example_buttons.txt`. Er beginnt so:

```
START: ignition off, key out, radio off, wait 10 s
Insert the key, ignition on (engine off), wait until the radio has started, then 10 s
Steering wheel: volume + three times, every 2 s
Steering wheel: volume - three times, every 2 s
```

Die Aufzeichnungen des Referenzautos in `firmware/test_data/captures/` wurden mit Plänen wie diesem
gemacht. Ihre Schrittzeilen sind auf Polnisch (`# KROK n` = „Schritt n“) und wurden so belassen,
wie sie aufgezeichnet wurden.

## Über WLAN mit `record.py` aufzeichnen {#record-over-wi-fi-with-recordpy}

1. Bauen und flashen Sie `listen_generic_wifi` mit der Bitrate, die Sie gefunden haben: im Ordner
   `firmware` mit `uvx platformio run -e listen_generic_wifi -t upload`
   ([Die Firmware flashen](../quick-start/flash.md)).
2. Schließen Sie das Gerät an das Auto an ([CAN-Anschluss](../../en/install-in-car/can-connection.md)).
3. Verbinden Sie einen Laptop mit dem Gerätenetz (dem WLAN des Geräts) ([Erster Start](../quick-start/first-boot.md)).
   Nehmen Sie einen Laptop im Akkubetrieb oder einen, der vom Auto versorgt wird.
4. Führen Sie im Repository-Ordner aus:

    ```
    python -I firmware/tools/record.py
    ```

    Die Standardadresse ist `10.50.8.1`. Eine andere Adresse geben Sie als erstes Argument an.
5. Geben Sie das Passwort des Geräts ein, wenn das Skript danach fragt. Es ist dasselbe wie der
   WLAN-Schlüssel; `wifi showpass` in der USB-Konsole zeigt es ([Erster Start](../quick-start/first-boot.md)).
   Es wird nicht angezeigt und nicht gespeichert.
6. Warten Sie auf `[ok] connected, recording`. Das Skript gibt die Namen von zwei Dateien aus: eine
   mit den CAN-Frames, eine mit den Konsolenzeilen des Geräts.
7. Für jeden Schritt: Tippen Sie einen kurzen Text (zum Beispiel `volume up 3x`), drücken Sie
   Enter und führen Sie dann die Aktion aus.
8. Tippen Sie `q` und Enter, um aufzuhören.

`record.py` liest nur vom Gerät. Es sendet keine Befehle und ändert nichts. Die Frames `336`,
`3B6` und `2B6` werden als `# masked` geschrieben, weil sie im Referenzauto die VIN tragen. In
Ihrem Auto kann die VIN in anderen Frames stehen: Finden Sie diese, bevor Sie etwas teilen
([Ihr Auto teilen](contribute.md)).

> [!WARNING]
> **`record.py` verbirgt `336`, `3B6` und `2B6` immer**
>
> In einem anderen Auto können diese drei IDs gewöhnliche Signale tragen, und das Skript hat
> keine Option, sie zu behalten. Finden Sie ein Signal nicht, machen Sie eine Aufzeichnung mit
> dem Sniffer oder mit `dump`: Beide behalten jeden Frame. Bereinigen Sie diese Datei, bevor Sie
> sie teilen.

Bricht die WLAN-Verbindung ab, versucht das Skript weiter, sich zu verbinden. Frames, die das Gerät nicht übergeben
konnte, werden als `# lost N frames` geschrieben. Das Gerät hält etwa 9 s an Frames im Speicher,
bei kurzen Abbrüchen geht also nichts verloren.

> [!NOTE]
> **Nicht im Browser-Panel aufzeichnen**
>
> Die CAN-Ansicht des Web-Panels zeichnet nur auf, solange ihr Browser-Tab sichtbar ist. Als der
> Autor während einer Panel-Aufzeichnung zu einem anderen Tab wechselte, gingen 26 593 Frames
> verloren (gemessen: die Zeile `# zgubiono 26593 ramek`, polnisch für „26593 Frames
> verloren“, in `centralka_can_20261117_184411.log`). Verwenden Sie `record.py`.

## Über USB mit dem Sniffer aufzeichnen {#record-over-usb-with-the-sniffer}

Der Sniffer ist ein kleines separates Programm für eine ESP32-S3-Platine mit einem
CAN-Transceiver-Modul und ganz ohne Sendeleitung. Aufbau, Verkabelung und der Aufzeichnungsbefehl
stehen in `extras/sniffer/README.md`. Kurz:

```
uv run --with pyserial python -I extras/sniffer/record_serial.py COM7 --scenario extras/sniffer/scenarios/example_buttons.txt
```

Führen Sie es im Repository-Ordner aus. Ersetzen Sie `COM7` durch Ihren Port:
`uvx platformio device list` zeigt ihn. Das Skript zeigt den nächsten Schritt des Szenarios.
Drücken Sie Enter, wenn Sie ihn beginnen.

## Wie Markierungen ihre Zeit bekommen {#how-markers-get-their-time}

Eine Kommentarzeile hat keine eigene Zeit. Das Dekodierwerkzeug ([Dekodieren](decode.md)) gibt jeder
Markierung die Zeit des ersten CAN-Frames danach. Eine Markierung von `record.py` trägt die
Gerätezeit in Klammern, und diese Zeit wird stattdessen verwendet.

Das spielt eine Rolle, wenn der Bus schläft. In der Referenzaufzeichnung `can_20261110_190421.log`
war der Bus während der ersten zwei Schritte still („Start, 10 s warten“ und „Zündung an“). Beide
Markierungen bekamen dieselbe Zeit, den Moment, in dem der Bus aufwachte:

```
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
```

Für „Zündung an“ schauen Sie sich also die ersten Frames der Aufzeichnung an, nicht eine Änderung
um die Markierung herum.

## Nach der Aufzeichnung {#after-the-recording}

- Prüfen Sie die Zahlen, die das Skript am Ende ausgibt: Frames, Markierungen, verlorene Frames.
- Öffnen Sie die Datei in einem Texteditor und lesen Sie die Kommentarzeilen. Ihre
  Markierungstexte werden unverändert in die Datei übernommen: Tippen Sie nichts Privates hinein.
- Behalten Sie die Originaldatei. Arbeiten Sie an einer Kopie.
- Bevor Sie eine Aufzeichnung mit jemandem teilen, entfernen Sie die VIN und prüfen Sie die Datei
  ([Ihr Auto teilen](contribute.md)).

Weiter: [Frames dekodieren](decode.md).
