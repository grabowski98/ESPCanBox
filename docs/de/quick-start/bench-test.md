# Prüftisch-Test

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Auf dieser Seite prüfen Sie das Gerät auf dem Schreibtisch, bevor es in die Nähe des Autos kommt.
Jeder Teil braucht etwas mehr Hardware als der vorige. Hören Sie beim ersten Punkt auf, der nicht
stimmt, und suchen Sie die Ursache.

Dies ist die Kurzfassung für einen ersten Build. Das vollständige Schritt-für-Schritt-Protokoll
für einen neuen Firmware-Build, mit den Prüfungen für Entwickler, steht auf der Seite
[Prüftisch-Protokoll](../../en/testing/bench-test-protocol.md).

## 1. Das ESP32-S3-Board an USB {#1-the-esp32-s3-board-on-usb}

Flashen Sie `listen_wifi` (oder `listen`) wie auf der Seite [Firmware flashen](flash.md) und
öffnen Sie den Monitor. Tippen Sie dann (Ihre Eingabe wird nicht angezeigt; drücken Sie nach
jedem Befehl Enter):

| Befehl | Was Sie sehen sollten |
|---|---|
| `ver` | die Version und die Build-Zeit |
| `stat` | `[stat] mode=alongside the box can=ok audio=ok radio=ok` und die Zähler; ohne 12 V zeigt die Versorgung `no 12V` |
| `help` | die Liste der Befehle |
| `cantx` | `[cantx] built=0: no CAN transmit in this build (CAN listen only)` |

Die LED auf dem ESP32-S3-Board zeigt den Zustand: blau beim Start, aus bei stillem Bus, grün
solange Frames ankommen, gelb solange das Gerät piept, rot wenn beim Start ein Treiber
ausgefallen ist (die Ausgänge bleiben aus) oder der CAN-Controller nicht läuft.

Sie können einen CAN-Frame in die Konsole einfügen, zum Beispiel `0E1#B0F000FCFC1EC200`. Das Gerät
behandelt ihn so, als käme er vom Auto; so probieren Sie das Dekodieren ohne Auto aus. Erwartet:
Die Zeile `[park]` im nächsten `stat` ändert sich.

## 2. Die Trägerplatine an 12 V, ohne Module {#2-the-carrier-board-on-12-v-without-modules}

Die Trägerplatine verbindet das ESP32-S3-Board mit dem Auto. Prüfen Sie sie zuerst leer: ohne
ESP32-S3-Board und ohne Module in den Sockeln.

> [!WARNING]
> **Mit Strombegrenzung arbeiten**
>
> Versorgen Sie die Trägerplatine aus einem Labornetzteil mit einer Strombegrenzung von etwa
> 100 mA. Wenn das Netzteil in die Strombegrenzung geht, schalten Sie sofort ab und suchen Sie den
> Kurzschluss.

1. Ohne Spannung, mit dem Multimeter: kein Kurzschluss zwischen +12 V und Masse, +5 V und Masse,
   +3.3 V und Masse. Zwischen CANH und CANL sollten Sie einige zehn kΩ messen, nicht etwa 120 Ω.
2. 12 V an den Versorgungseingang (beim Prototyp v1: Klemme J5, `+12V` und `GND`). An der
   5-V-Schiene sollten Sie etwa 4.6-4.8 V messen (Ausgang des 5-V-Wandlers hinter einer Diode).
3. Abschalten, das ESP32-S3-Board einstecken, einschalten. Jetzt zeigen die 3.3-V-Pins 3.3 V und
   der Strom liegt bei einigen zehn mA.

Die Prototyp-Seite hat die vollständige Prüfliste mit den Pins der Sockel:
[Prototyp-Platine](../../en/hardware/prototype-pcb.md).

## 3. Mit den Modulen {#3-with-the-modules}

1. Abschalten. Stecken Sie das CAN-Modul (Terminator-Schalter auf OFF) und das Verstärkermodul
   mit seinem Lautsprecher ein.
2. Einschalten und USB anschließen. Tippen Sie `stat`. Erwartet: `can=ok audio=ok radio=ok` und
   eine Versorgungsspannung statt `no 12V`. (Nur an USB, ohne 12 V, zeigte der Prototyp ebenfalls
   `can=ok audio=ok radio=ok`; gemessen: ESP32-S3 + CAN-Modul + Verstärker.)
3. Tippen Sie `test`. Erwartet: 1.5 s Testton aus dem Lautsprecher.
4. Tippen Sie `vol 30`, um die Lautstärke zu ändern. Die Firmware begrenzt sie auf 80 %, um einen
   kleinen Lautsprecher zu schützen. Ein Wert unter 15 % gilt nur bis zum nächsten Neustart.

## 4. Die 12-V-Ausgänge (nur Decoder-Builds) {#4-the-12-v-outputs-decoder-builds-only}

> [!WARNING]
> **Nicht bei angeschlossener CAN-Box**
>
> Flashen Sie `decoder` oder `decoder_wifi` nur auf dem Prüftisch oder, im Auto, nur bei
> abgezogener CAN-Box. Die Ausgänge des Geräts und der Box dürfen nie denselben Draht treiben.

Die Ausgänge ACC, ILL und REV schalten nur im Decoder-Modus, und sie folgen dem Auto: ACC den
Zündungs-Frames, ILL dem Lichtbit, REV dem Bit für den Rückwärtsgang. Ohne CAN-Verkehr bleiben
sie aus. Einen Prüftisch-Bericht eines Nachbauers für diese Ausgänge gibt es noch nicht
(**unbestätigt**, unverified); prüfen Sie sie mit einem Multimeter und einer kleinen Last, bevor
das Radio angeschlossen wird.

## 5. Das Web-Panel (Builds mit WLAN) {#5-the-web-panel-builds-with-wi-fi}

Mit `listen_wifi` auf der Platine melden Sie sich wie unter [Erster Start](first-boot.md) an.
Dann:

1. Das Panel öffnet sich und zeigt die **Übersicht**.
2. Tippen Sie im Reiter **Konsole** `stat`. Erwartet: dieselbe Antwort wie an USB.

Aktualisierung und Zurücksetzen über WLAN sind die Schritte 7.1-7.7 des
[Prüftisch-Protokolls](../../en/testing/bench-test-protocol.md).

<details class="note" markdown="1">
<summary>Prüfungen des Panels für Entwickler</summary>

Prüfen Sie diese anhand der Werte im Panel oder von `stat` an der Konsole:

1. Während das WLAN startet und stoppt, wird die Hauptschleife nicht langsamer
   („Durchläufe / s“ und „Längster WLAN-Schritt“ im Reiter System).
2. Eine Firmware-Aktualisierung über das Panel funktioniert, ebenso das Zurücksetzen auf das
   vorherige Programm. Eine falsche Datei wird abgelehnt.
3. Mit einer CAN-Quelle auf dem Prüftisch: Während einer Aktualisierung bleiben die Zähler
   verlorener CAN-Frames auf null, und die Zähler der Radioverbindung zeigen keine verlorenen
   Frames, während ein Browser das Panel offen hält.
4. Ein WLAN-Scanner auf Ihrem Telefon zeigt über mehrere Starts hinweg nie ein offenes Netz
   namens `ESP_...`.
5. Tiefschlaf und Stromaufnahme im Schlaf, nachdem das WLAN an war: Messen Sie sie. Sie
   wurden auf der Hardware noch nicht gemessen.

Die Schritte 1 und 2 oben und die Punkte 1, 2 und 4 wurden am Prototyp an USB geprüft, ohne
CAN und Radio (gemessen: Prüftisch-Test eines früheren Builds dieser Firmware). Die Punkte 3
und 5 brauchen CAN-Verkehr oder 12 V und sind noch offen. Ebenso offen ist, wie lange das
Radio ohne ACC bleibt, während das Gerät nach einer Aktualisierung neu startet.

</details>

## 6. Vor dem Auto {#6-before-the-car}

- Die Überspannungsschutzdiode (`D2` beim Prototyp v1) ist bestückt, und der
  Eingangskondensator ist für 35 V oder mehr ausgelegt.
- Nirgends ein 120-Ω-Terminator: Der Terminator-Schalter des CAN-Moduls steht auf OFF.
- `JP1` beim Prototyp v1 ist offen (Hardware nur zum Mithören), es sei denn, Sie bauen bewusst die
  Sende-Firmware.
- Sie kennen die Pins Ihres Autos.

## Weiter {#next}

[Einbau-Checkliste](../../en/install-in-car/checklist.md) für das Referenzauto oder
[Ihren CAN-Bus finden](../make-it-yours/find-your-bus.md) für ein anderes Auto.
