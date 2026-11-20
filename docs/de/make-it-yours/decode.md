# Frames dekodieren

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Sie haben eine Aufzeichnung mit Markierungen ([Den Bus aufzeichnen](record.md)). Jetzt suchen Sie
die Bits, die sich geändert haben, als Sie im Auto etwas getan haben. Diese Seite macht das Schritt
für Schritt an echten Aufzeichnungen des Referenzautos, mit dem Werkzeug
`firmware/tools/can_diff.py`.

Das Werkzeug liest nur Dateien. Es braucht Python 3 und sonst nichts. Alle Befehle unten laufen
aus dem Repository-Ordner.

Die Beispiele nutzen Aufzeichnungen aus dem Repository. Ihre eigene Aufzeichnung von `record.py`
liegt im Ordner `logs` neben dem Repository-Ordner, in einer Datei, deren Name auf
`_can_YYYYMMDD_HHMMSS.log` endet. Geben Sie stattdessen ihren Pfad an, zum Beispiel
`python -I firmware/tools/can_diff.py ../logs/<Dateiname> --list`.

## Bytes und Bits {#bytes-and-bits}

Ein CAN-Frame hat eine ID (drei Hex-Ziffern, zum Beispiel `0F6`) und bis zu 8 Datenbytes. Dieses
Projekt nummeriert sie so:

- Bytes ab **1**: Byte 1 ist das erste Datenbyte (`data[0]` im C++-Code);
- Bits **7..0** innerhalb eines Bytes, Bit 0 ist das niederwertigste;
- Kurzform `byte.bit`: `8.7` = Byte 8, Bit 7.

Beispiel: Der Frame `0F6#884D1E2182585828` hat Byte 1 = `88`, Byte 2 = `4D`, ... Byte 8 = `28`.

Um Bits zu sehen, schreiben Sie jede Hex-Ziffer als vier Binärziffern:

| Hex | Binär (Bits 7..0) |
|---|---|
| `24` | `0010 0100` |
| `A4` | `1010 0100` |

`24` und `A4` unterscheiden sich nur im linken Bit, die Änderung ist also Bit 7.

## Was das Werkzeug macht {#what-the-tool-does}

Für jede Markierung vergleicht `can_diff.py` zwei Zeitfenster:

- **davor** (before): die Sekunden direkt vor der Markierung (standardmäßig 3 s, `--before`);
- **danach** (after): die Sekunden direkt danach (standardmäßig 5 s, `--after`).

Es listet jedes Bit, das „davor“ stabil war und sich „danach“ geändert hat. Zwei Filter entfernen
den größten Teil des Rauschens:

- **Zähler** (counters): ein Byte oder Halbbyte, das sich in fast jedem Frame der ganzen
  Aufzeichnung um denselben Betrag ändert (umlaufende Zähler, Timer). Das Werkzeug gibt sie einmal
  oben aus und meldet sie nie.
- **Hintergrund** (background): Ein Bit, das sich schon im Fenster „davor“ geändert hat, wird für
  diese Markierung ignoriert („ignored: N background bits“ in der Ausgabe).

Es listet auch IDs, die erst nach der Markierung auftauchen („new IDs“), und IDs, die aufhören.

Prüfen Sie zuerst, ob das Werkzeug auf Ihrem Computer funktioniert. Die synthetische Datei ist ein
erfundenes Auto mit bekannten Antworten:

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --selftest
```

```
selftest: 6 markers, 3216 frames, 0 errors
```

Nützliche Optionen:

| Option | Was sie macht |
|---|---|
| `--list` | listet die Markierungen mit Nummern und Zeiten, dann Ende |
| `--marker N` | nur Markierung N (wiederholbar) |
| `--match TEXT` | nur Markierungen, deren Text TEXT enthält |
| `--after S`, `--before S` | Fensterlängen in Sekunden |
| `--id 0F6,21F` | nur diese IDs |
| `--summary` | für die gewählten Markierungen: bei wie vielen sich jedes Bit geändert hat |
| `--export OUT` | schreibt nur die CAN-Zeilen nach OUT, für andere Programme |

Markierungen sind die Kommentarzeilen, die mit `# STEP`, `# MARK`, `# MARKER` oder `# KROK` beginnen
(das polnische Wort aus den Referenzaufzeichnungen). Ein anderes Muster legen Sie mit `--marker-regex` fest.

## Die Markierungen finden {#find-the-markers}

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --list
```

```
firmware/test_data/captures/can_20261110_190421.log: 186700 frames, 70 IDs, 73 markers, 0 broken lines skipped
counters (never reported): 0E6:8H 10B:5L 236:5 3F6:3H
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
    3  line   4078     416.094 s  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
...
```

Die Zähler-Zeile bedeutet: In `0E6` zählt das obere Halbbyte (`H`) von Byte 8, in `10B` das untere
Halbbyte (`L`) von Byte 5, in `236` das ganze Byte 5, in `3F6` das obere Halbbyte von Byte 3. Das
Werkzeug wird sie nie melden.

## Beispiel 1: Rückwärtsgang (`0F6`) {#example-1-reverse-gear-0f6}

Schritt 17 dieser Aufzeichnung sagt auf Polnisch: „Rückwärtsgang: 3 s einlegen, herausnehmen,
3 Mal (ohne Hindernis)“. Der Schritt dauerte etwa 27 s, nehmen Sie also ein langes Fenster
„danach“:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25
```

Ohne `--id` listet das Werkzeug 13 IDs: `0A1 0B6 0E1 0E6 0F6 10B 128 161 227 236 2AD 3E7 3F6`.
Der Parksensor-Frame `0E1` ändert sich auch, und langsame Werte driften während 25 s. Suchen Sie
das Bit, das sich **so oft ändert, wie Sie die Aktion ausgeführt haben**. Hier steckt es in `0F6`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25 --id 0F6
```

```
==============================================================================
marker 17, line 43732, at 597.094 s
  # KROK 17 19:08:20.249 Wsteczny: wlacz 3 s, wylacz, 3 razy (bez przeszkody)
  window: 3.0 s before, 25.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 2: 51 52
      bits: 2.1 0->1 x1 @+23.87s, 2.0 1->0 x1 @+23.87s
    byte 8: 24 A4 24 A4 24 A4 24 A4
      bits: 8.7 0->1 x7 @+8.67s
```

So lesen Sie es:

- `byte 8: 24 A4 24 ...` ist die Folge der Werte, die das Byte angenommen hat, ohne Wiederholungen;
- `8.7 0->1 x7 @+8.67s`: Bit 8.7 war vor der Markierung 0, hat sich 7 Mal geändert, zum ersten Mal
  8.67 s nach der Markierung.

Bit 8.7 ging immer wieder auf 1 und zurück, im Takt der Aktion. Byte 2 hat sich einmal geändert,
kurz vor dem Ende: Das ist nicht die Aktion.

Der Plan sagte dreimal; das Bit ging im Fenster viermal auf 1. Die Aufzeichnung sagt nicht, warum.
Vertrauen Sie dem, was die Datei zeigt, gleichen Sie es mit Ihren Notizen ab und bestätigen Sie ein
Signal an einer zweiten Aufzeichnung.

Ergebnis: Rückwärtsgang = `0F6` 8.7 (gemessen: `can_20261110_190421.log`, Schritt 17;
dasselbe Bit steht in `vehicles/peugeot-508-sw-2015/508.dbc`).

## Beispiel 2: Zündung und Motor (`0F6` Byte 1) {#example-2-ignition-and-engine-0f6-byte-1}

Manche Signale sind kein einzelnes Bit, sondern ein ganzes Byte mit wenigen Werten. Schritt 3 sagt
„Motor starten, 10 s ohne Bewegung“:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 3 --after 10 --id 0F6
```

```
==============================================================================
marker 3, line 4078, at 416.094 s
  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
  window: 3.0 s before, 10.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 1: 88 90 91 89 8A 8E
      bits: 1.4 0->1 x2 @+3.30s, 1.3 1->0 x2 @+3.30s, 1.2 0->1 x1 @+5.02s, 1.1 0->1 x1 @+4.03s, 1.0 0->1 x2 @+3.46s
    byte 8: 20 24 64
      bits: 8.6 0->1 x1 @+6.12s, 8.2 0->1 x1 @+4.07s
```

Hier verwirrt die Bit-Liste, aber die **Byte-Werte** erzählen die Geschichte: `88` davor, dann
`90 91 89 8A`, während der Anlasser dreht, dann `8E` bei laufendem Motor. Mit weiteren
Aufzeichnungen von Zündung aus, an und Motorstart ergibt das Referenzauto `80` aus, `88` an,
`90`/`91`/`89`/`8A` Anlassen, `8E` Motor läuft, `86`/`84` Ausschalten (gemessen: Aufzeichnungen mit
Schritten für Zündung und Motor, `508.dbc`).

Byte 8 hat sich auch geändert (`20 24 64`). Was diese beiden Bits bedeuten, ist unbekannt. Geben
Sie einem Bit keine Bedeutung, nur weil es sich zur selben Zeit geändert hat.

Für das erste Einschalten gibt es kein „davor“: Der Bus schlief, und jede ID ist an der Markierung
„neu“ (siehe [Aufzeichnen](record.md#how-markers-get-their-time)). Lesen Sie dann direkt die ersten
Frames. Der erste `0F6`-Frame dieser Aufzeichnung ist:

```
(398.212947) can0 0F6#884D1E2182585828
```

Byte 1 = `88`: Zündung an.

## Beispiel 3: eine Lenkradtaste (`21F`) {#example-3-a-steering-wheel-button-21f}

Ein sauberer Fall. Schritt 3 von `can_20261111_190817.log` sagt „Lenkrad: Lautstärke + dreimal,
alle 2 s“. Diesmal ohne `--id`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_190817.log --marker 3 --after 4.5
```

```
firmware/test_data/captures/can_20261111_190817.log: 30040 frames, 62 IDs, 14 markers, 0 broken lines skipped
counters (never reported): 0E6:8H,8L 10B:5L 236:5 3F6:3H
==============================================================================
marker 3, line 6809, at 223.999 s
  # KROK 3 19:08:54.163 Kierownica: glosnosc + trzy razy, co 2 s
  window: 3.0 s before, 4.5 s after; ignored: 10 background bits, counters in 4 IDs
  21F
    byte 1: 00 08 00 08 00
      bits: 1.3 0->1 x4 @+0.78s
  new IDs (absent before, present after): 1A5(4)
```

Ein Bit: `21F` 1.3 (Byte 1 = `08`, solange die Taste gedrückt ist). Das Fenster von 4.5 s enthält
zwei der drei Drücke: 4 Änderungen. Die nächste Markierung kommt 4.5 s später, ein längeres Fenster
würde also den nächsten Schritt mit hineinmischen.

Die neue ID `1A5` kommt nicht von der Taste. In dieser Aufzeichnung war die nachgerüstete CAN-Box
angeschlossen, und sie sendet `1A5` (die Lautstärke) wenige Millisekunden, nachdem das Radio die
Lautstärke geändert hat (gemessen: [Peugeot 508 SW](../../en/reference/vehicles/peugeot-508.md)).
Andere Geräte am Bus reagieren auch auf Ihre Aktion. Zeichnen Sie wenn möglich einmal ohne sie auf.

Ergebnis: Lautstärke hoch = `21F` 1.3 (gemessen: `can_20261111_190817.log` und
`can_20261111_191045.log` mit der UART-Leitung zum Radio).

## Beispiel 4: Parksensoren (`0E1`) {#example-4-parking-sensors-0e1}

Ein Frame voller Felder. Schritt 18 von `can_20261111_191045.log` sagt „hintere Sensoren:
Rückwärtsgang eingelegt, einen Gegenstand bis zum Dauerton an die hintere Stoßstange heranführen,
wegnehmen, Rückwärtsgang herausnehmen“. Diese Datei beginnt bei Schritt 13, also ist Schritt 18
die Markierung 6 (prüfen Sie es mit `--list`):

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_191045.log --marker 6 --after 25 --id 0E1
```

```
==============================================================================
marker 6, line 18775, at 421.532 s
  # KROK 18 19:12:11.696 Czujniki TYL: wsteczny wlaczony, zblizaj przedmiot do tylnego zderzaka az do tonu ciaglego, zabierz, wylacz wsteczny
  window: 3.0 s before, 25.0 s after; ignored: 2 background bits, counters in 0 IDs
  0E1
    byte 1: B0 B4 90 B0
      bits: 1.2 0->1 x2 @+2.53s
    byte 2: 00 D0 00 D0 50 D0 00
      bits: 2.7 0->1 x6 @+15.31s, 2.6 0->1 x4 @+15.31s, 2.4 0->1 x4 @+15.31s
    byte 3: 3F 30 2B 3F 2E 28 25 22 ...
      bits: 3.5 1->0 x2 @+16.86s, 3.4 1->0 x6 @+15.51s, 3.3 1->0 x10 @+15.31s, 3.2 1->0 x10 @+15.31s, 3.1 1->0 x14 @+15.31s, 3.0 1->0 x12 @+15.31s
    byte 4: FC F8 F4 F8 F4 F0 EC E8 ...
      bits: 4.7 1->0 x2 @+17.37s, 4.6 1->0 x2 @+17.75s, 4.5 1->0 x4 @+17.37s, 4.4 1->0 x2 @+16.86s, 4.3 1->0 x6 @+15.51s, 4.2 1->0 x10 @+15.31s
    byte 5: FC 5C 3C 1C FC
      bits: 5.7 1->0 x2 @+17.56s, 5.6 1->0 x2 @+17.82s, 5.5 1->0 x4 @+17.56s
```

Wenn sich viele benachbarte Bits zusammen ändern, sind sie ein Feld. Teilen Sie die Bytes in
Gruppen von Bits auf, die sich gemeinsam bewegen:

| Feld | Wo | Was die Werte hier tun | Bedeutung im Referenzauto |
|---|---|---|---|
| Zustand hinten, Zustand vorne | Byte 1, Bits 7-5 und 4-2 | `90` = `100 100 00`: hinten 4, vorne 4 | 4 aktiv, 5 Bereitschaft (gemessen) |
| Tonanforderung | 2.4 | 1, solange das Auto piept | gemessen |
| Seiten | 2.7-2.6 | `D0`: beide Seiten | gemessen |
| Pieptonabstand | Byte 3, Bits 5-0 | `3F`, dann fallende Werte, während der Gegenstand näher kommt | `3F` kein Ton, `39` ... `0B` Pieptöne, `00` Dauerton (gemessen) |
| Sensorzonen | Bytes 4-6, je 3 Bits | Werte fallen, während der Gegenstand näher kommt | 0 am nächsten ... 7 weit (gemessen) |

Bedeutungen: `vehicles/peugeot-508-sw-2015/508.dbc` und
[Peugeot 508 SW](../../en/reference/vehicles/peugeot-508.md#parking-sensors-0e1).

Bit 1.5 hat sich auch geändert (`B4` auf `90`), steht aber nicht in der Liste: Es bewegte sich
schon im Fenster „davor“, das sich mit dem Ende des Rückwärtsgang-Schritts überschneidet. Der
Hintergrundfilter kann ein echtes Bit verstecken. Erwarten Sie ein Bit und sehen Sie es nicht,
verschieben Sie das Fenster: Mit `--before 1` zeigt dieser Lauf auch `1.5 1->0 x2`. Die Byte-Werte
(`B0 B4 90 B0`) werden immer vollständig angezeigt.

## Die Aktion wiederholen, die Treffer zählen {#repeat-the-action-count-the-hits}

Der beste Beweis ist ein Bit, das sich bei **jeder** Markierung derselben Aktion ändert und bei
keiner anderen. Mit einer Markierung pro Wiederholung zählen `--match` und `--summary` das für Sie.
An der synthetischen Datei (ein erfundenes Auto, dreimal Tür öffnen und dreimal eine Taste
drücken):

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --match door --summary
```

```
firmware/test_data/synthetic/marker_demo.log: 3216 frames, 5 IDs, 6 markers, 4 broken lines skipped
counters (never reported): 100:2,8L
==============================================================================
summary of 3 markers: 1 3 5
  3/3  300 2.5
```

`3/3  300 2.5` = im Frame `300` hat sich Bit 2.5 (Byte 2, Bit 5) bei allen drei
Tür-Markierungen geändert. Mit `--match volume` lautet die
Antwort `3/3  310 1.3`. Die 4 beschädigten Zeilen sind absichtlich enthalten: Das Werkzeug überspringt Zeilen,
die wie Frames aussehen, aber beschädigt sind (ungerade Zahl von Hex-Ziffern, mehr als 8 Bytes,
eine 4-stellige ID).

## Wie oft ein Frame kommt {#how-often-a-frame-comes}

Manche Signale brauchen einen Frame, der oft genug wiederholt wird (siehe
[Portierungs-Checkliste](porting-checklist.md)). Das Werkzeug zeigt die Periode nicht an. Lesen Sie
sie aus der Datei ab:

1. Öffnen Sie die Aufzeichnung in einem Texteditor.
2. Suchen Sie nach `can0 0F6#` (setzen Sie Ihre ID statt `0F6` ein).
3. Ziehen Sie die Zeiten zweier aufeinanderfolgender Zeilen voneinander ab. In
   `can_20261111_191045.log`: 336.199989 - 335.699989 = 0,5 s.
4. Prüfen Sie einige Paare. Ein Frame, der nur bei Änderung gesendet wird, hat lange,
   unregelmäßige Lücken.

In Git Bash gibt `grep "can0 0F6#" <Datei> | head -5` die ersten fünf Zeilen einer ID aus.

## Von Bits zu Werten {#from-bits-to-values}

Ein Feld wird mit einem **Faktor** (scale) und einem **Offset** zur Zahl:

```
value = raw × scale + offset
```

Beispiel aus dem Referenzauto: Außentemperatur = `0F6` Byte 7, Faktor 0.5, Offset -40, also
`value / 2 - 40` °C (gemessen: stimmt in 21 von 21 Frames mit der Temperatur überein, die die
CAN-Box an das Radio gesendet hat, `can_20261111_191045.log`). Im Frame `0F6#884D1E2182585828` oben
ist Byte 7 = `58` = 88, und 88 / 2 - 40 = 4 °C.

Um einen Faktor zu finden, zeichnen Sie den Wert an zwei bekannten Punkten auf (zum Beispiel die im
Kombiinstrument angezeigte Temperatur oder die Lautstärke des Radios) und vergleichen Sie.

### In eine DBC-Datei schreiben {#write-it-into-a-dbc-file}

Eine DBC-Datei ist eine Textdatei, die Frames und Signale auflistet. Viele CAN-Werkzeuge lesen sie.
Die Datei des Referenzautos ist `vehicles/peugeot-508-sw-2015/508.dbc`. Eine Zeile pro Signal:

```
BO_ 246 BODY_STATE: 8 CAR
 SG_ reverse : 63|1@0+ (1,0) [0|1] "" DEVICE
```

- `246` ist die ID in Dezimal (`0F6` hex = 246).
- `63|1@0+`: Startbit 63, Länge 1 Bit, `@0` = Big-Endian-Bytereihenfolge (auch Motorola genannt;
  wählen Sie das in SavvyCAN oder cantools), `+` = vorzeichenlos.
  Mit `@0` gilt: Startbit = (Byte - 1) × 8 + Bit des höchsten Bits des Felds: Byte 8 Bit 7 =
  7 × 8 + 7 = 63. Der Pieptonabstand, Byte 3 Bits 5-0, ist `21|6@0+`.
- `(1,0)` = Faktor und Offset; die Außentemperatur hat `(0.5,-40)`.

Geben Sie jedem Signal einen Kommentar, der mit seinem Status beginnt (`measured: ...` mit der
Aufzeichnung oder `unverified: ...` mit der Quelle), wie in `508.dbc`. Machen Sie dann aus den
Signalen ein Fahrzeugprofil: [Ein Fahrzeugprofil schreiben](write-a-profile.md).

## SavvyCAN und andere Werkzeuge {#savvycan-and-other-tools}

SavvyCAN ist ein freies Desktop-Programm für die CAN-Analyse, das DBC-Dateien und Log-Dateien in
mehreren Formaten liest. Dieses Projekt hat **keine Live-Verbindung** dazu: Die Firmware hat kein
GVRET oder ein ähnliches Protokoll. Sie arbeiten mit aufgezeichneten Dateien. Ob Ihre
SavvyCAN-Version das Aufzeichnungsformat dieses Projekts direkt öffnet, wurde nicht geprüft; lehnt
sie die Kommentar- oder `uartA`-Zeilen ab, exportieren Sie zuerst die CAN-Zeilen:

```
python -I firmware/tools/can_diff.py my_recording.log --export frames_only.log
```

## Grenzen des Werkzeugs {#limits-of-the-tool}

- Eine Markierung hat keine eigene Zeit: Sie bekommt die Zeit des nächsten CAN-Frames. An einem
  schlafenden Bus ist das der Moment, in dem der Bus aufwacht.
- Ein Signal, das sich langsam ändert (Temperatur, Kraftstoff), ändert sich im Fenster vielleicht
  nicht.
- Ein Bit, das sich schon vor der Markierung bewegte, wird versteckt (siehe Beispiel 4).
- Der Zählerfilter sucht gleichmäßige Schritte. Ein springender Zähler oder eine Prüfsumme wird
  nicht erkannt und erscheint in jedem Fenster als unruhiges Bit.
- Nur 11-Bit-IDs (drei Hex-Ziffern) werden gelesen.

Was nach dem Dekodieren unbekannt bleibt: was das Auto mit einem Wert macht, dessen Änderung Sie
nicht gesehen haben. Ein gefundenes Signal ist für die aufgezeichneten Aktionen gemessen, nicht
mehr.

Weiter: [Ein Fahrzeugprofil schreiben](write-a-profile.md).
