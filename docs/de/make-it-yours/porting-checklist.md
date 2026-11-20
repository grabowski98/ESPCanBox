# Portierungs-Checkliste

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Der ganze Weg von „ein Auto, das noch niemand gemessen hat“ bis „das Gerät arbeitet anstelle der
CAN-Box“, auf einer Seite. Gehen Sie von oben nach unten. Jede Zeile sagt, wann der Schritt erledigt
ist. Überspringen Sie keine Zeile, weil ein anderes Auto „wahrscheinlich“ genauso funktioniert:
Jeder Fakt über Ihr Auto braucht einen Status, **gemessen** (measured) oder **unbestätigt**
(unverified).

Schreiben Sie Ihre Ergebnisse unterwegs in eine Kopie von `vehicles/_template/README.md`.
Befehle mit `tools/` laufen in dem Ordner, den die Zeile nennt: im Repository-Ordner oder in
`firmware`.

> [!CAUTION]
> **Nichts auf dieser Liste sendet auf den Fahrzeugbus**
>
> Jeder hier genutzte Build hört am Fahrzeug-CAN-Bus nur mit. Ihr eigenes Profil lässt sich gar
> nicht mit Senden bauen. Senden für ein neues Auto ist ein eigenes Projekt mit eigenen Regeln:
> [Senden auf dem CAN-Bus](../../en/safety/transmit.md).

## 1. Der Bus {#1-the-bus}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| Die Leitungen CANH und CANL am Radio finden | Durchgang vom Stecker zu einem bekannten Punkt gemessen, und Frames darauf empfangen | [Ihren CAN-Bus finden](find-your-bus.md) |
| Die Leitungsspannungen gegen Masse messen, Zündung an | Spannungen von CANH und CANL notiert | [Ihren CAN-Bus finden](find-your-bus.md) |
| Entscheiden: High-Speed oder fehlertoleranter Low-Speed-CAN | Schlussfolgerung als Schlussfolgerung notiert; Prüfung mit Oszilloskop als erledigt oder noch offen vermerkt | [CAN-Anschluss](../../en/install-in-car/can-connection.md) |
| Den Abschlusswiderstand der alten CAN-Box messen | Widerstand CANH-CANL, ohne Spannung, notiert | [Ihren CAN-Bus finden](find-your-bus.md#check-the-terminator-of-the-old-box) |
| `listen_generic_wifi` mit Ihrer ersten Vermutung der Bitrate bauen und flashen | Konsole zeigt `# profile: car=generic radio=none` | [Die Firmware flashen](../quick-start/flash.md) |
| Die Bitrate finden | Frames fehlerfrei empfangen bei einer von 50, 100, 125, 250, 500 oder 1000 kbit/s | [Ihren CAN-Bus finden](find-your-bus.md) |
| Prüfen, ob der Bus bei abgezogenem Schlüssel still wird | Zeit vom Abziehen des Schlüssels bis zur Stille, aus einer Aufzeichnung | [Ihren CAN-Bus finden, Schritt 4](find-your-bus.md#step-4-when-is-the-bus-awake) |

## 2. Aufzeichnungen {#2-recordings}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| Die Schritte planen | schriftlicher Plan: eine Aktion pro Schritt, jede dreimal wiederholt | [Den Bus aufzeichnen](record.md) |
| Mit Markierungen aufzeichnen | Datei mit einer Markierung pro Schritt; Zahl der verlorenen Frames geprüft | [Den Bus aufzeichnen](record.md) |
| Die Frames finden, die die VIN tragen | IDs notiert, oder „nicht gefunden“ | [Ihr Auto teilen](contribute.md) |
| Die Aufzeichnung bereinigen (im Repository-Ordner) | `python -I tools/sanitize_capture.py --check` meldet 0 Probleme, mit den IDs Ihrer VIN-Frames in `--ids` | [Ihr Auto teilen](contribute.md) |

## 3. Signale {#3-signals}

Dekodieren Sie nur, was Sie brauchen. In der Reihenfolge des Nutzens:

| Signal | Erledigt, wenn | Wofür |
|---|---|---|
| Zündung und Motorzustand | Byte und Werte für aus / an / Motor läuft, in drei Wiederholungen gesehen; [Periode des Frames](decode.md#how-often-a-frame-comes) bekannt (muss unter 1 s liegen) | ACC zum Radio |
| Rückwärtsgang | byte.bit, dreimal gesehen | Rückwärtsgang-Ausgang (Kamera) |
| Licht | byte.bit; [Periode des Frames](decode.md#how-often-a-frame-comes) bekannt (muss unter 1 s liegen) | Beleuchtungsausgang |
| Lenkradtasten | ein byte.bit-Muster pro Taste, keines davon in Ruhe vorhanden | Tasten zum Radio |
| Drehknöpfe | Zählerbyte, Richtung pro Raste | nächster / vorheriger |
| Parksensoren | Status, Zonen 0 bis 7 pro Sensor, Tonanforderung, Abstand, wenn das Auto einen sendet; [Periode des Frames](decode.md#how-often-a-frame-comes) (muss unter 400 ms liegen) | Piepser, Sensorbild im Radio |
| Türen, Kofferraum, Motorhaube (optional) | ein Bit pro Tür, jede Tür offen und geschlossen gesehen | Türbild im Radio |
| Außentemperatur, Klimaanlage, Energiesparmodus, Einstellungen, Bordcomputer (optional) | Byte und Skala jedes Werts, mit der Anzeige des Autos verglichen | die passenden Radiobildschirme |

Jedes Signal kommt mit seinem Frame, `byte.bit`, Werten, Status und dem Aufzeichnungsschritt, der
es gezeigt hat, auf die Fahrzeugseite. [Frames dekodieren](decode.md) zeigt, wie.

## 4. Profil {#4-profile}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| `firmware/lib/car_example` nach `firmware/lib/car_<name>` kopieren und umbenennen | Dateien, Objekt, Namespace und Name in `library.json` umbenannt | [Ein Fahrzeugprofil schreiben](write-a-profile.md) |
| Nur gemessene Signale eintragen | jeder unbekannte Zeiger ist `nullptr`, jede unbekannte Tabelle leer | [Ein Fahrzeugprofil schreiben](write-a-profile.md) |
| Bytenummern umrechnen | Ihre Notizen (Byte 1 = erstes) in Code-Indizes (`data[0]`, `byteIndex 0`) übertragen | [Ein Fahrzeugprofil schreiben](write-a-profile.md) |
| Über `powerOutputs` entscheiden | `true` nur mit einem zuverlässigen `decodeBody` | [Ein Fahrzeugprofil schreiben](write-a-profile.md) |
| `listen_mycar` bauen | `SUCCESS`, `car_<name>` in der Abhängigkeitsliste | [Ein Fahrzeugprofil schreiben](write-a-profile.md) |

## 5. Tests auf dem PC {#5-tests-on-the-pc}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| Host-Test an Ihrer bereinigten Aufzeichnung | Ausgabe Zeile für Zeile mit Ihren Markierungen abgeglichen, dann in `firmware/test_host/expected/` gespeichert | [Ihr Profil testen](test-your-profile.md) |
| Alle Host-Tests (in `firmware`) | `All tests match the references.` | [Host-Tests](../../en/testing/host-tests.md) |
| Schichtungsprüfung (in `firmware`) | `python -I tools/check_layering.py` endet mit `Layering OK.` | [Ihr Profil testen](test-your-profile.md) |
| Radiocodes geprüft | jede Ihrer Tasten ergibt den gewünschten Code; Standard-Umbelegungen (`keyRemapDefaults`) nur die gewünschten | [Das Radio zuordnen](radio-mapping.md) |

## 6. Prüftisch und Auto {#6-bench-and-car}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| Prüftisch-Test mit Ihrem Build | Startzeilen zeigen Ihr Profil und `# can=ok`; 12-V-Ausgänge im Mithör-Build aus | [Prüftisch-Test](../quick-start/bench-test.md) |
| Im Auto parallel zur CAN-Box (`listen_mycar`) | Ihre Tasten erscheinen mit `keys`, Zündung und Rückwärtsgang in der Konsole, Pieptöne der Parksensoren aus dem Lautsprecher des Geräts; nichts an das Radio gesendet | [Erste Fahrt](../../en/install-in-car/first-drive.md) |
| Decoder-Modus (`decoder_mycar`, CAN-Box abgezogen) | Radio schaltet sich mit der Zündung ein und aus; Tasten, Rückfahrkamera und Sensorbild funktionieren | [Radioanschluss](../../en/install-in-car/radio-connection.md) |
| Notieren, was nicht funktioniert | aufgeführt unter „Not known yet“ auf Ihrer Fahrzeugseite | [Ihr Auto teilen](contribute.md) |

## 7. Teilen {#7-share}

| Schritt | Erledigt, wenn | Seite |
|---|---|---|
| Fahrzeugseite | `vehicles/<make>-<model>-<year>/README.md` mit einem Status in jeder Zeile | [Ihr Auto teilen](contribute.md) |
| Prüfung auf private Daten (im Repository-Ordner) | `python -I tools/scan_private.py .` mit 0 Fehlern | [Ihr Auto teilen](contribute.md) |
| Pull Request oder Issue | geöffnet mit der Vorlage „New vehicle“ | [Ihr Auto teilen](contribute.md) |

Auch eine Fahrzeugseite allein, ohne Firmware-Profil, ist ein nützlicher Beitrag.

Weiter: [Ihr Auto teilen](contribute.md).
