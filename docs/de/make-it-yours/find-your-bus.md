# Ihren CAN-Bus finden

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Bevor Sie etwas aufzeichnen oder dekodieren können, brauchen Sie drei Fakten über Ihr Auto:

1. welche zwei Leitungen am Radio den CAN-Bus führen,
2. welcher CAN-Bustyp es ist (High-Speed oder fehlertoleranter Low-Speed-CAN),
3. seine Bitrate (die Geschwindigkeit, in kbit/s).

Diese Seite zeigt, wie Sie sie mit einem Multimeter und mit dem Gerät im Modus „nur Mithören“
finden. Nichts auf dieser Seite sendet etwas an das Auto.

Der ganze Weg, vom Messen des Busses bis zum Teilen Ihres Autos, steht auf einer Seite:
[Portierungs-Checkliste](porting-checklist.md). Lassen Sie sie offen und haken Sie die Zeilen
nacheinander ab.

> [!CAUTION]
> **Auf dieser Seite nur einen reinen Mithör-Build verwenden**
>
> Schritt 1 und 2 brauchen nur ein Multimeter. Ab Schritt 3 verwenden Sie nur einen reinen
> Mithör-Build: `listen_generic_wifi` oder den separaten Sniffer in `extras/sniffer/`. Verwenden
> Sie nie eine Umgebung mit Senden (`decoder_tx_wifi`) an einem Auto, das Sie noch nicht kennen.
> In einem Mithör-Build sendet der CAN-Controller keine Frames, keine Bestätigungsbits und keine
> Fehler-Frames. So bleibt er für das Auto unsichtbar, selbst bei falscher Bitrate. Lesen Sie
> [Sicherheit zuerst](../getting-started/safety.md), bevor Sie anfangen.

## Wonach Sie suchen {#what-you-are-looking-for}

Ein modernes Auto hat meist mehrere CAN-Busse. Jeder verbindet eine Gruppe von Steuergeräten und
hat seine eigene Geschwindigkeit. Der Radiostecker gibt Ihnen Zugang zu einem davon, und nur zu
diesem. Welcher Bus das ist und welche Signale er führt, hängt vom Auto ab: Für Ihr Auto ist es
unbekannt, bis Sie es messen.

Im Referenzauto (Peugeot 508 SW, 2015) führt der Bus am Radio Zündung, Rückwärtsgang, Licht,
Lenkradtasten, Türen und Parksensoren (gemessen: Aufzeichnungen in
`firmware/test_data/captures/`). Ein anderes Auto legt manche dieser Signale vielleicht auf einen
Bus, den das Radio nicht sieht.

Sammeln Sie Ihre Fakten in der Tabelle der Fahrzeugvorlage `vehicles/_template/README.md`,
Abschnitt „Bus at the radio“. Füllen Sie eine Zeile erst aus, wenn Sie sie gemessen haben.

## Schritt 1: die zwei Leitungen finden {#step-1-find-the-two-wires}

Hat Ihr Auto eine nachgerüstete CAN-Box, ist deren Kabelbaum der einfachste Weg: Das CAN-Paar
führt vom Fahrzeugstecker zum Stecker der CAN-Box. [CAN-Anschluss](../../en/install-in-car/can-connection.md)
(Schritt 1) zeigt, wie Sie es mit einem Durchgangsprüfer finden, und
[Verkabelung](../../en/hardware/wiring.md) zeigt, wie Sie einen ganzen Kabelbaum aufnehmen.

Notieren Sie beide Enden jeder Leitung und die Seite, von der Sie geschaut haben (Leitungsseite
oder Kontaktseite, wo die Verriegelung sitzt). Eine Pin-Nummer ohne Ansicht liest man leicht
falsch herum.

Beispiel aus dem Referenzauto: Quadlock-Pin 10 = CANH, Pin 13 = CANL (gemessen: Durchgang vom
Transceiver der CAN-Box, danach fehlerfreier Empfang mit dieser Belegung). Die Pin-Nummern des
Quadlock-Gehäuses sind die eigene Nummerierung des Autors; die Übereinstimmung mit der Nummerierung
des Herstellers ist unbestätigt (unverified). Details: `vehicles/peugeot-508-sw-2015/wiring.md`.

### Den Abschlusswiderstand der alten Box prüfen {#check-the-terminator-of-the-old-box}

Der Bus des Autos hat schon seine Abschlusswiderstände. Ersetzt das Gerät eine CAN-Box, vergewissern
Sie sich, dass die Box keinen hatte. Dann wissen Sie, dass auch das Gerät keinen haben darf:

1. Ziehen Sie die CAN-Box ab und lassen Sie sie ohne Spannung.
2. Messen Sie mit einem Multimeter den Widerstand zwischen ihren Pins CANH und CANL.
3. Notieren Sie den Wert. Einige zehn Kiloohm = kein Abschlusswiderstand in der Box. Etwa
   120 Ohm = die Box schließt den Bus ab; notieren Sie das und fragen Sie in einem Issue, bevor Sie
   weitermachen.

Beispiel aus dem Referenzauto: zwei nachgerüstete Boxen, 48 kOhm und 59 kOhm (gemessen: Ohmmeter,
ohne Spannung). Keine der beiden Boxen schließt also den Bus ab.

## Schritt 2: den Bustyp erkennen {#step-2-tell-the-kind-of-bus}

Zündung an, Multimeter auf Gleichspannung, Minus-Messspitze an Masse, Plus-Messspitze an jede
CAN-Leitung. Die Tabelle in
[CAN-Anschluss, Schritt 2](../../en/install-in-car/can-connection.md#step-2-measure-the-voltages) sagt
Ihnen, was die Werte bedeuten. Kurz:

- beide Leitungen nahe 2.5 V: wahrscheinlich **High-Speed-CAN**;
- eine Leitung nahe 0 V und die andere nahe 5 V: wahrscheinlich **fehlertoleranter Low-Speed-CAN**;
  die Leitung nahe 0 V ist CANH;
- etwa 12 V: keine CAN-Leitung, aufhören.

Ein Multimeter zeigt einen Mittelwert. Das ist also ein Hinweis, kein Beweis. Ein Oszilloskop zeigt
die echten Ruhe- und dominanten Pegel.

Beispiel aus dem Referenzauto: CANH 0.65 V, CANL 4.5 V gegen Masse, Zündung an, gleich mit und ohne
angeschlossenes Gerät (gemessen: Multimeter). Diese Pegel sehen nach fehlertolerantem Low-Speed-CAN
aus; das ist eine Schlussfolgerung aus den Spannungen und braucht zur Bestätigung noch ein
Oszilloskop.

> [!NOTE]
> **Das Gerät hat einen High-Speed-Transceiver**
>
> Der Transceiver des Geräts ist ein 3.3-V-High-Speed-Typ. Im Referenzauto hat er den Bus
> fehlerfrei empfangen, obwohl die Spannungen nach fehlertolerantem Low-Speed-CAN aussehen
> (gemessen: alle Aufzeichnungen in `firmware/test_data/captures/`). Ob er den Bus Ihres Autos
> empfängt, ist unbekannt, bis Sie es versuchen. Was ein High-Speed-Sender an einem solchen Bus
> tun würde, wurde nicht gemessen. Ein Grund mehr, beim reinen Mithören zu bleiben.

## Schritt 3: die Bitrate finden {#step-3-find-the-bit-rate}

### Mit einem Oszilloskop {#with-an-oscilloscope}

Schauen Sie sich den kürzesten Impuls auf CANH oder CANL an, während Frames laufen. Dieser Impuls
ist ein Bit:

| Kürzester Impuls | Bitrate |
|---|---|
| 2 µs | 500 kbit/s |
| 4 µs | 250 kbit/s |
| 8 µs | 125 kbit/s |
| 10 µs | 100 kbit/s |
| 20 µs | 50 kbit/s |

### Mit dem Gerät, durch Ausprobieren {#with-the-device-by-trial}

Die Bitrate kommt aus dem Fahrzeugprofil. Für ein unbekanntes Auto nehmen Sie das Profil
`generic`: Es dekodiert nichts, sendet nie und schaltet die 12-V-Ausgänge nie ein. Seine
Geschwindigkeit ist eine einzelne Zahl in `firmware/lib/car_generic/profile_generic.cpp`:

```cpp
    125000,
```

125 kbit/s ist nur der Standardwert, weil das die Geschwindigkeit des Referenzautos ist. Es ist
kein Fakt über Ihr Auto.

1. Schließen Sie das Gerät an die zwei Leitungen aus Schritt 1 an
   ([CAN-Anschluss](../../en/install-in-car/can-connection.md)).
2. Wählen Sie eine erste Vermutung aus Schritt 2. Ein fehlertoleranter Low-Speed-Bus läuft mit
   125 kbit/s oder weniger; ein High-Speed-Bus oft mit 500 kbit/s, manchmal mit 250 kbit/s
   (allgemeine Eigenschaften der beiden Standards, keine Fakten über Ihr Auto). Probieren Sie in
   dieser Reihenfolge: Low-Speed `125000`, dann `100000`, `50000`; High-Speed `500000`, dann
   `250000`, `125000`.
3. Tragen Sie die Geschwindigkeit in Bit pro Sekunde in `profile_generic.cpp` ein. Die Firmware
   kennt nur diese Werte: `50000`, `100000`, `125000`, `250000`, `500000`, `1000000` (die Liste
   steht in `firmware/src/can_bus.cpp`). Mit jedem anderen Wert startet der CAN-Controller nicht,
   und das Gerät bleibt im sicheren Zustand.
4. Bauen und flashen Sie, im Ordner `firmware`: `uvx platformio run -e listen_generic_wifi -t upload`.
   Erwartet: `[SUCCESS]`. Startet das Hochladen nicht, siehe
   [Die Firmware flashen](../quick-start/flash.md).
5. Zündung an. Öffnen Sie die USB-Konsole mit `uvx platformio device monitor` und tippen Sie ein
   paar Mal `stat` ([Konsole](../../en/reference/console.md)).
6. Lesen Sie die Zeile, die mit `[stat] rx=` beginnt:

| Was Sie sehen | Was es bedeutet |
|---|---|
| `rx=` steigt um Hunderte pro Sekunde, `errors=` bleibt gleich | richtige Geschwindigkeit |
| `rx=` bleibt bei 0, `errors=` steigt | falsche Geschwindigkeit, oder CANH und CANL vertauscht |
| `rx=` bleibt bei 0, `errors=` bleibt bei 0 | kein Verkehr: Bus schläft, falsche Leitungen oder keine Masse |

Ein paar Fehler im Moment, in dem der Bus aufwacht, können normal sein. In einer Referenzaufzeichnung
stieg der Fehlerzähler des Sniffers beim Aufwachen des Busses von 136 auf 192 und blieb dann für
den Rest der 14 Minuten bei 192 (gemessen: die `# stat`-Zeilen von `can_20261110_190421.log`).

Ist die Geschwindigkeit falsch, ändern Sie die Zahl, flashen Sie erneut und wiederholen Sie den Test.
Probieren Sie den nächsten Wert erst, wenn Sie die Leitungen geprüft haben.

Beim separaten Sniffer in `extras/sniffer/` ist die Geschwindigkeit fest im Quelltext
eingestellt: `CAN_TIMING`, 125 kbit/s (`extras/sniffer/README.md`). Der Befehl `scan` seiner
Recorder-Variante misst die zwei UART-Leitungen zum Radio, nicht den CAN-Bus.

## Schritt 4: wann ist der Bus wach {#step-4-when-is-the-bus-awake}

Finden Sie heraus, wann Ihr Bus still ist. Das brauchen Sie, um die Aufzeichnungen zu planen: Eine
Markierung, die gesetzt wird, während der Bus schläft, bekommt die Zeit des Moments, in dem der Bus
aufwacht ([Aufzeichnen](record.md)).

Beispiel aus dem Referenzauto: Mit abgezogenem Schlüssel ist der Bus still. Nach dem Ausschalten der
Zündung gibt es etwa 100 s vollen Verkehr, dann 14 s lang nur einen Frame (`036`), dann Stille
(gemessen: Aufzeichnungen dieses Autos; `vehicles/peugeot-508-sw-2015/wiring.md`).

## Schritt 5: ein erster Blick {#step-5-a-first-look}

Mit der richtigen Geschwindigkeit tippen Sie `dump` in die USB-Konsole. Das Gerät gibt jeden Frame
als eine Zeile aus:

```
(140.548) can0 0F6#8E501E33BE5D5D24
(140.569) can0 128#C0B0010200411400
(140.573) can0 0E1#B4003FFCFCFCC200
```

Die Felder: Zeit in Sekunden, Schnittstelle, Frame-ID, `#` und die Datenbytes in Hex. Tippen Sie
erneut `dump`, um die Ausgabe zu stoppen. Die Zeilen oben stammen aus `centralka_can_20261117_184411.log`.

Zum Vergleich: Das Referenzauto sendet bei eingeschalteter Zündung etwa 218 Frames pro Sekunde mit
70 verschiedenen IDs (gemessen: `can_20261110_190421.log`). Ihr Auto kann mehr oder weniger senden.

Notieren Sie, was Sie gefunden haben (Stecker, Pins und Ansicht, Bustyp, Bitrate, wann der
Bus schläft).

Weiter: [Den Bus aufzeichnen](record.md).
