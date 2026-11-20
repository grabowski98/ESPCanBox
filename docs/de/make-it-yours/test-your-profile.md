# Ihr Profil testen

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Bevor Ihr Profil in die Nähe des Autos kommt, lassen Sie es auf Ihrem PC laufen. Ein **Host-Test**
schickt eine Aufzeichnung Frame für Frame durch denselben Decoder-Code, der auf dem Gerät läuft, und
gibt aus, was das Gerät tun würde: Konsolenzeilen, Frames für das Radio und jede Änderung der
12-V-Ausgänge und des Piepsers. Sie lesen diese Ausgabe einmal, gleichen sie mit dem ab, was Sie im
Auto getan haben, und speichern sie als Referenz. Danach meldet der Test, wenn eine Änderung
etwas kaputtmacht.

Nichts wird geflasht, und kein Port wird geöffnet. Sie brauchen weder die Platine noch das Auto.

Diese Seite setzt das Tutorial von [Ein Fahrzeugprofil schreiben](write-a-profile.md) mit dem
erfundenen Profil `mine` fort. Jeder Befehl wurde an einer Kopie des Repositorys ausgeführt. Alle
Befehle laufen im Ordner `firmware`, in Git Bash.

> [!TIP]
> **Zuerst ein Probelauf**
>
> Bevor Sie Ihre eigenen Frames in `car_mine` eintragen, können Sie die Kopie und die
> Umbenennungen prüfen. Machen Sie die Schritte 2 bis 4 mit den Vorlagenwerten in `car_mine`
> und mit `run_file mine_car test_host/synthetic/example_car.log` in Schritt 3. Die Ausgabe
> muss mit `example profile works on every axis` und `### code=0` enden. Ändern Sie dann das
> Profil auf Ihr Auto und tragen Sie Ihre eigene Aufzeichnung in die Zeile `run_file` ein.

## Was Sie brauchen {#what-you-need}

- Git Bash unter Windows, oder eine Shell unter Linux oder macOS;
- `uv`: Das Testskript bekommt darüber einen C++-Compiler (Paket `ziglang`; der erste Lauf lädt ihn
  herunter und braucht dafür einmal Internet), oder Sie setzen Ihren eigenen mit `CXX=g++`.
  Details: [Host-Tests](../../en/testing/host-tests.md);
- Ihr Profil in `firmware/lib/car_<name>/`;
- eine Aufzeichnung Ihres Autos mit Markierungen ([Den Bus aufzeichnen](record.md)), bereinigt mit
  `python -I ../tools/sanitize_capture.py`, wenn Sie sie teilen werden ([Ihr Auto teilen](contribute.md)).

## Schritt 1: die Aufzeichnung ablegen {#step-1-put-the-recording-in-place}

| Aufzeichnung | Wohin |
|---|---|
| eine echte Aufzeichnung Ihres Autos | `firmware/test_data/captures/`, mit einer Zeile in dessen `README.md` (was, wie aufgezeichnet, welcher Test sie nutzt) |
| von Hand geschriebene Frames, um einen Fall auszuprobieren | `firmware/test_host/synthetic/`, mit einem Kommentar oben, der sagt, dass die Frames erfunden sind |

Das Format ist das übliche, `(seconds) can0 ID#DATA`. Zeilen, die keine CAN-Frames sind
(Kommentare, UART-Zeilen), überspringt der Test. Handgeschriebene Dateien für Profiltests
gehören nach `test_host/synthetic/`, nicht nach `test_data/synthetic/`: Dieser Ordner ist für
`can_diff.py`.

Eine von Hand geschriebene Datei ist ein guter Anfang, weil Sie genau wissen, was passieren soll.
Die im Tutorial genutzte, `test_host/synthetic/mine_car.log`, begann so (das Tutorial-Auto von
[Ein Profil schreiben](write-a-profile.md); diese Datei ist nicht im Repository, schreiben Sie also
Ihre eigene):

```text
# Invented frames for the "mine" profile (tutorial). Not a real car.
# 2C0 = ignition / reverse, 1F0 = buttons + knob, 350 = rear parking, 2C8 = lights
(0.000000) can0 2C0#00000000
(0.200000) can0 2C0#00010000
(0.400000) can0 2C0#00020000
(0.600000) can0 1F0#01000000
(0.700000) can0 1F0#01000000
```

## Schritt 2: den Beispieltest kopieren {#step-2-copy-the-example-test}

Das Vorlagenprofil hat einen eigenen Test, `firmware/test_host/example_car.cpp`. Kopieren Sie ihn
im Ordner `firmware`:

```bash
cp test_host/example_car.cpp test_host/mine_car.cpp
```

Ändern Sie dann vier Dinge in `mine_car.cpp`:

1. `#include "profile_example.h"` wird zu `#include "profile_mine.h"`;
2. jedes `kCarExample` wird zu `kCarMine` (sechsmal, in vier Zeilen; Suchen und Ersetzen ist am
   einfachsten);
3. der Hilfetext `example_car <capture.log>` wird zu `mine_car <capture.log>` (im Kommentar oben
   und in der Zeile mit `fprintf`);
4. die letzte Prüfung, die Zeile, die mit `const bool ok =` beginnt. Sie verlangt, dass **jeder**
   Ausgang etwas getan hat. Die Teile: `r.accMs` = ACC, `r.illMs` = Licht, `r.revMs` =
   Rückwärtsgang, `r.toneMs` = Piepser, `r.uartFrames` = Frames für das Radio,
   `d.haveLastKey()` = mindestens eine Taste. Entfernen Sie die Teile, die Ihr Profil nicht hat.
   Ein Profil ohne Parksensoren scheitert an `r.toneMs > 0`, und der Test endet dann mit
   Exit-Code 1. Fertige Zeilen:
    - nur Zündung und Tasten:
      `const bool ok = r.accMs > 0 && r.uartFrames > 0 && d.haveLastKey();`
    - nur Zündung: `const bool ok = r.accMs > 0;`

    Der Text `example profile works on every axis` darf bleiben: Er ist nur eine Beschriftung.

## Schritt 3: den Test in das Testskript aufnehmen {#step-3-add-the-test-to-the-test-script}

Öffnen Sie `firmware/tools/run_host_tests.sh` und suchen Sie den Block, der mit
`finish example_car` endet. Fügen Sie Ihren Test direkt danach ein:

```bash
# My car profile
# shellcheck disable=SC2086
build mine_car $S38_LIBS profile_mine.cpp
rm -f "$OUT/mine_car.actual.txt"
run_file mine_car test_host/synthetic/mine_car.log
finish mine_car
```

`build` kompiliert Ihren Test zusammen mit dem Decoder und seinen Bibliotheken und Ihrer
Profildatei. `$S38_LIBS` ist die Liste der Decoder-Dateien, die der Test braucht; lassen Sie sie,
wie sie ist. `run_file` führt ihn mit der angegebenen Datei aus, mit einem Pfad relativ zu
`firmware/`. Für eine Aufzeichnung in `test_data/captures/` geben Sie diesen Pfad an.

## Schritt 4: erster Lauf, Ausgabe lesen, Referenz speichern {#step-4-first-run-read-the-output-store-the-reference}

Führen Sie alle Host-Tests aus (`cd firmware` brauchen Sie nur, wenn Sie im Repository-Ordner
sind):

```bash
cd firmware
bash tools/run_host_tests.sh
```

Erwartet: Alle bestehenden Tests geben `OK` aus. Ihr Test hält das Skript an, weil er noch keine Referenz hat:

```text
OK     example_car
FAIL   mine_car: no reference test_host/expected/mine_car.txt
```

Die Ausgabe liegt jetzt in `firmware/.build_host/mine_car.actual.txt`. **Lesen Sie sie** und
vergleichen Sie sie mit dem, was Sie im Auto getan (oder in die Datei geschrieben) haben. Das ist
der eigentliche Test; die gespeicherte Datei wiederholt ihn später nur. Ein Ausschnitt aus dem
Tutorial-Lauf:

```text
### mine_car test_host/synthetic/mine_car.log
profile mine: 500000 bit/s, 4 key rows, 1 knob(s), txAllow 0
     0 outputs acc=0 ill=0 rev=0 tone=0 continuous=0
console [car] acc=1
   200 radio 2E EA 02 01 0A 08
   200 outputs acc=1 ill=0 rev=0 tone=0 continuous=0
console [key] code=14 state=1
   600 radio 2E 20 02 14 01 C8
console [key] code=14 state=2
   800 radio 2E 20 02 14 02 C7
...
console [park] rear=4 front=0 sound=1 rear interval=0x00 zones rear 707 front 777
  1600 radio 2E 32 07 02 05 00 05 05 05 05 AB
  1600 outputs acc=1 ill=1 rev=1 tone=1 continuous=1
...
frames=21 uart_bytes=106 uart_frames=13 acc_ms=3101 ill_ms=1001 rev_ms=800 tone_ms=390 keys_dropped=0
example profile works on every axis
### code=0
```

So lesen Sie es:

| Zeile | Bedeutung |
|---|---|
| `profile mine: ...` | was Ihr Profil enthält: Geschwindigkeit, Zahl der Tastenzeilen und Drehknöpfe, Sendeliste (für Ihr eigenes Profil immer 0) |
| `console ...` | die Zeile, die das Gerät auf seiner USB-Konsole ausgeben würde |
| `200 radio 2E EA ...` | ein Frame für das Radio bei 200 ms (Zeit nach der Uhr der Aufzeichnung). `EA` = Strom an, `20` = Taste, `32` = Parksensoren, `3E` = Hindernis-Flag. Siehe [XP-Protokoll](../../en/reference/xp-protocol.md) |
| `outputs acc=1 ill=0 rev=0 tone=0 continuous=0` | eine Änderung der 12-V-Ausgänge (ACC, Beleuchtung, Rückwärtsgang) oder des Piepsers |
| `[key] code=14 state=1` | Taste mit Radiocode `14` (Lautstärke +): 1 gedrückt, 2 gehalten, 0 losgelassen |
| letzte Zeilen | Summen in Millisekunden und Ihre `ok`-Prüfung |

Was sich in Ihrer eigenen Ausgabe zu prüfen lohnt:

- ACC geht mit Ihrem Frame „Zündung an“ an und 2.5 s nach Zündung aus wieder aus. Im Ausschnitt
  geht es nie aus, weil die Wiedergabe 1 s nach dem letzten Frame endet.
- Der Beleuchtungsausgang geht 1 s nach dem letzten Licht-Frame aus (im Tutorial-Lauf: Licht-Frame
  bei 1100 ms, `ill=0` bei 2101 ms). Wiederholt Ihr Auto seinen Licht-Frame, passiert das nicht.
- Jede Taste ergibt `state=1`, dann alle 200 ms `state=2`, solange sie gehalten wird, dann
  `state=0`.
- Der Ton beginnt mit der ersten Zone unter 7 und wird nur mit Pieptonabstand `0x00` zum Dauerton.
- Eine Taste zeigt einen anderen Code als erwartet: siehe [Das Radio zuordnen](radio-mapping.md).
  Standard-Umbelegungen kommen aus dem Fahrzeugprofil; das Beispielprofil hat keine, daher zeigt der
  Beispieltest SRC als `code=21 state=1`, unverändert gesendet.

> [!WARNING]
> **Ein gespeichertes `### code=1` geht als OK durch**
>
> Das Skript vergleicht die Ausgabe nur mit der gespeicherten Datei. Bei einem gespeicherten
> `### code=1` hält es nicht an. Prüfen Sie die letzten Zeilen (Punkt 1 unten), bevor Sie
> kopieren, sonst besteht der Test, obwohl er gescheitert ist.

Ist die Ausgabe richtig, speichern Sie sie als Referenz:

1. Prüfen Sie die letzten zwei Zeilen von `.build_host/mine_car.actual.txt`. Sie müssen
   `example profile works on every axis` und `### code=0` lauten.
2. Sehen Sie `FAIL: an axis of the example profile did nothing` und `### code=1`, korrigieren Sie
   die Zeile `const bool ok =` (Schritt 2, Punkt 4) und führen Sie das Skript erneut aus.
3. Kopieren Sie die Datei und führen Sie das Skript erneut aus:

```bash
cp .build_host/mine_car.actual.txt test_host/expected/mine_car.txt
bash tools/run_host_tests.sh
```

Erwartet: `OK     mine_car` und `All tests match the references.`

> [!WARNING]
> **Dafür nicht `--record-golden` verwenden**
>
> `bash tools/run_host_tests.sh --record-golden` zeichnet die Referenzen **aller** Tests neu auf,
> einschließlich der Golden-Dateien des Referenzautos. Verwenden Sie es nur, wenn Sie das
> Verhalten absichtlich geändert und jeden Unterschied geprüft haben. Für einen neuen Test kopieren
> Sie seine Datei wie oben gezeigt.

## Eine schnellere Schleife für einen Test {#a-faster-loop-for-one-test}

Das ganze Skript brauchte im Tutorial-Lauf 30 bis 50 Sekunden. Während Sie am Profil arbeiten,
können Sie nur Ihren Test kompilieren und ausführen. Führen Sie zuerst einmal das ganze Testskript
aus: Es lädt den Compiler in den `uv`-Cache, und der Befehl unten nutzt diesen Cache (`--offline`).
Aus dem Ordner `firmware`, in Git Bash:

```bash
LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp profile_mine.cpp vehicle_signals.cpp"
SRCS=""; for f in $LIBS; do SRCS="$SRCS $(ls lib/*/$f)"; done
INC=""; for d in lib/*/; do INC="$INC -I ${d%/}"; done
mkdir -p .build_host
uvx --offline --from ziglang python -I -m ziglang c++ -std=c++17 -O1 -Wall $INC -I include \
    test_host/mine_car.cpp $SRCS -o .build_host/mine_car.exe < /dev/null
./.build_host/mine_car.exe test_host/synthetic/mine_car.log
```

Die Dateiliste ist dieselbe wie `$S38_LIBS` im Testskript, plus Ihr Profil und
`vehicle_signals.cpp`. Ändert sich die Liste im Skript, kopieren Sie sie von dort. Führen Sie das
ganze Skript aus, bevor Sie etwas teilen.

## Auch die Schichtungsprüfung ausführen {#also-run-the-layering-check}

```bash
python -I tools/check_layering.py
```

Sie stellt sicher, dass Ihr Profil nichts aus einem Radioprotokoll, aus `src/` oder aus dem
Arduino-Framework einbindet und dass kein Header-Name doppelt vorkommt. Im Tutorial-Lauf hat sie
`car_mine` unter den Bibliotheken aufgeführt und mit `Layering OK.` geendet.

## Was dieser Test nicht abdeckt {#what-this-test-does-not-cover}

- Der Code in `firmware/src/` (CAN-Treiber, UART, 12-V-Ausgänge, Ton) läuft nicht auf dem PC. Die
  erste Prüfung echter Hardware ist der [Prüftisch-Test](../quick-start/bench-test.md).
- Timing am echten Bus: Die Wiedergabe läuft in 1-ms-Schritten und verliert keine Frames.
- Ob Ihre Dekodierung für das Auto stimmt. Der Test kann nur zeigen, was Ihr Profil aus den Frames
  macht. Deshalb gleichen Sie die Ausgabe mit Ihren Markierungen ab, bevor Sie sie speichern.

Weiter: [Das Radio zuordnen](radio-mapping.md).
