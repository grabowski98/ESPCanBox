# Ein Fahrzeugprofil schreiben

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Ein **Fahrzeugprofil** sagt der Firmware, wie sie ein Auto liest: die Busgeschwindigkeit, welche
Bits die Lenkradtasten sind, wo Zündung, Rückwärtsgang, Licht und Parksensoren stehen. Es ist eine
konstante Struktur in C++ (`vehicle::CarProfile`, definiert in `firmware/lib/core/car_profile.h`),
die in das Programm kompiliert wird. Sie wählen das Profil beim Bauen der Firmware; während das
Gerät läuft, kann es sich nicht ändern.

Diese Seite führt Sie von Ihren dekodierten Frames zu einem Profil, das sich bauen lässt. Die
nächste Seite, [Ihr Profil testen](test-your-profile.md), prüft es an einer Aufzeichnung auf Ihrem
PC.

Jeder Schritt auf dieser Seite wurde an einer Kopie des Repositorys mit einem erfundenen Auto
ausprobiert. Wo dabei etwas schiefging, sagt die Seite das und zeigt die Meldung, die Sie sehen
werden.

> [!CAUTION]
> **Ihr eigenes Profil hört am Fahrzeugbus nur mit**
>
> Ein mit `-DCAR_PROFILE=CUSTOM` gebautes Profil kann nie auf den Fahrzeug-CAN-Bus senden. Die
> sendende Umgebung `decoder_tx_wifi` verweigert damit den Build und bricht ab mit
> `CAN transmit is not allowed with CAR_PROFILE=CUSTOM (listen-only; add a named profile in src/config.h)`.
> Versuchen Sie nicht, das zu umgehen. Senden für ein neues Auto braucht gemessene Frames, im
> Code geschriebene Inhaltsprüfungen und Tests: siehe
> [Senden auf dem CAN-Bus](../../en/safety/transmit.md).

## Was ein Profil abdeckt {#what-a-profile-covers}

Seien Sie sich darüber im Klaren, bevor Sie anfangen. Ein Profil trägt nur einen Teil dessen, was
das Gerät im Referenzauto (Peugeot 508 SW 2015, Profil `psa_aee2010`) tut.

| Funktion | Über Ihr Profil | Hinweise |
|---|---|---|
| Busgeschwindigkeit | ja | `canBitrate` |
| Lenkrad- und Bedienteiltasten, Drehknöpfe | ja | Tabellen `keys` und `knobs` |
| Zündung und Motorzustand, Rückwärtsgang | ja | Funktion `decodeBody`; steuert ACC und den Rückwärtsgang-Ausgang |
| Licht an | ja | Funktion `decodeLights`; steuert den Beleuchtungsausgang |
| Parksensoren und der Piepton aus dem Lautsprecher des Geräts | ja | Funktion `decodeParking` |
| Sende-Freigabeliste | **nein**: das Feld bleibt leer (`nullptr, 0`) | ein `CUSTOM`-Profil kann nicht senden (siehe oben) |
| Frames, die die VIN tragen | aufgeführt, nur zur Information | `privacyIds`; die Firmware nutzt es nicht |
| Türen, Kofferraum und Motorhaube | ja | Funktion `decodeDoors` -> `vehicle::DoorState` |
| Bordcomputer | ja, mit Einschränkung | Funktion `decodeTrip` -> `vehicle::TripData`. Die Werte behalten die Kodierung des Referenzautos (ihre Bedeutung ist nicht geprüft): füllen Sie sie nur, wenn Ihr Auto dieselbe verwendet |
| Außentemperatur, Energiesparmodus | ja | Funktionen `decodeOutsideTemp`, `decodeEcoMode` |
| Zustand der Klimaanlage | ja | Funktion `decodeClimate` -> `vehicle::ClimateState` |
| Lenkwinkel | ja, mit Einschränkung | Funktion `decodeSteering`; Skala am Referenzauto nicht geprüft |
| Fahrzeugeinstellungen (was das Radio anzeigt) | ja | Funktion `decodeSettings` -> `vehicle::VehicleSettings` (Einträge mit neutralen Kennungen) |
| Fahrzeugeinstellungen vom Radio aus ändern | **nein** | nur das Referenzprofil `psa_aee2010`, hinter der Sendesperre (Gate) |

Die Fahrzeugdaten für den Radiobildschirm (Türen bis Fahrzeugeinstellungen) sind optional:
lassen Sie einen Zeiger auf `nullptr`, dann bekommt das Radio diese Daten nie. Jede Funktion füllt
einen neutralen Typ aus `firmware/lib/core/vehicle_signals.h`; das Radioprotokoll macht daraus
seinen eigenen Frame. Der Abschnitt „Fahrzeugdaten für den Radiobildschirm“ in Schritt 3 listet
sie auf.

Mit dem XP-Radioprotokoll gibt ein Profil eines anderen Autos dem Radio daher: den Stromzustand
(ACC), Tastendrücke, das Parksensorbild, das Hindernis-Flag und jede Fahrzeugdaten-Funktion, die
das Profil ausfüllt. Das Ändern von Fahrzeugeinstellungen vom Radio aus bleibt beim Referenzauto.

## Bevor Sie anfangen {#before-you-start}

Sie brauchen:

- die Bitrate, die CAN-Leitungen und den Bustyp Ihres Autos ([Ihren CAN-Bus finden](find-your-bus.md));
- eine Aufzeichnung mit Markierungen ([Den Bus aufzeichnen](record.md));
- die Frames und Bits, die Sie dekodiert haben ([Frames dekodieren](decode.md)). Notieren Sie jedes
  Signal mit seinem Status, **gemessen** (measured) oder **unbestätigt** (unverified), in einer Kopie von
  `vehicles/_template/README.md`;
- die Build-Werkzeuge aus [Die Firmware flashen](../quick-start/flash.md): `uv` und unter Windows
  Git Bash.

Sie brauchen nicht jedes Signal. Ein Profil nur mit der Zündung ist schon nützlich: Es kann ACC des
Radios schalten. Lassen Sie alles weg, was Sie nicht gemessen haben.

## Schritt 1: die Vorlage kopieren {#step-1-copy-the-template}

Die Vorlage `firmware/lib/car_example/` beschreibt ein erfundenes Auto, mit Kommentaren zu jedem
Feld. Sie wird nur für die Host-Tests kompiliert, nie in die Firmware.

> [!NOTE]
> **Zwei erfundene Autos**
>
> Die Vorlage nutzt die Frames `100`, `200`, `300` und `400`. Die Beispiele auf dieser Seite und
> auf [Ihr Profil testen](test-your-profile.md) stammen von einem **zweiten** erfundenen Auto,
> dem „Tutorial-Auto“ (Frames `2C0`, `1F0`, `350`, `2C8`), das beim Ausprobieren dieser Seiten
> ausgedacht wurde. Seine Aufzeichnung ist nicht im Repository; die Ausschnitte zeigen nur, wie
> Ihre eigenen Werte aussehen. In Ihrer Kopie ersetzen Sie die Werte der Vorlage durch die Frames
> Ihres Autos.

Wählen Sie einen kurzen Namen für Ihr Profil, in Kleinbuchstaben, zum Beispiel `mine`. Öffnen
Sie dann Git Bash im Ordner `firmware`: Öffnen Sie den Ordner im Explorer, klicken Sie mit der
rechten Maustaste und wählen Sie „Open Git Bash here“ (unter Windows 11 unter „Weitere Optionen
anzeigen“). Führen Sie aus:

```bash
cp -r lib/car_example lib/car_mine
cd lib/car_mine
mv profile_example.h profile_mine.h
mv profile_example.cpp profile_mine.cpp
cd ../..
```

Dateinamen müssen in ganz `firmware/lib/` eindeutig sein: Alle Bibliotheken teilen einen
Include-Pfad, und `tools/check_layering.py` meldet einen Header-Namen, der doppelt vorkommt.

## Schritt 2: in den Dateien umbenennen {#step-2-rename-inside-the-files}

Öffnen Sie die drei Dateien in einem Texteditor und benennen Sie um:

| Wo | Von | Nach |
|---|---|---|
| `profile_mine.cpp`, erstes `#include` | `"profile_example.h"` | `"profile_mine.h"` |
| beide Dateien | `kCarExample` | `kCarMine` |
| beide Dateien | `namespace example` und `example::` | `namespace mine` und `mine::` |
| `profile_mine.cpp`, erstes Feld des Profils | `"example"` | `"mine"` |
| `library.json`, Feld `name` | `"car_example"` | `"car_mine"` |
| `library.json`, Feld `description` | der Text der Vorlage | eine Zeile über Ihr Auto |

> [!WARNING]
> **`library.json` nicht vergessen**
>
> PlatformIO erkennt die Bibliothek am `name` in `library.json`, nicht am Ordnernamen. Die
> `listen`-Umgebungen ignorieren eine Bibliothek namens `car_example` (`lib_ignore` in
> `firmware/platformio.ini`). Mit dem alten Namen scheitert der Build mit
> `Warning! Could not find the package with 'car_mine' requirements` und danach
> `fatal error: profile_mine.h: No such file or directory`.

## Schritt 3: das Profil ausfüllen {#step-3-fill-in-the-profile}

Arbeiten Sie in dieser Reihenfolge:

1. Setzen Sie in `profile_mine.h` die vier Frame-IDs `ID_BODY`, `ID_KEYS`, `ID_PARKING` und
   `ID_LIGHTS` auf Ihre Frames. Eine ID, deren Funktion Sie auf `nullptr` setzen (Punkt 4),
   wird nicht gelesen.
2. Schreiben Sie in `profile_mine.cpp` die Funktionen `decodeBody`, `decodeParking` und
   `decodeLights` neu (Abschnitte unten).
3. Füllen Sie in derselben Datei die Tabellen `KEYS` und `KNOBS` (Abschnitte unten).
4. Füllen Sie am Ende der Datei die Profil-Konstante (Tabelle unten). Ihre Felder haben im Code
   keine Namen, nur ihre Reihenfolge zählt. Lassen Sie daher den Kommentar neben jeder Zeile
   stehen.

Wollen Sie die Kopie und die Umbenennungen prüfen, bevor Sie einen Wert ändern? Machen Sie zuerst
den Probelauf auf [Ihr Profil testen](test-your-profile.md).

Die Felder der Profil-Konstante, der Reihe nach:

| Feld | Was hineingehört |
|---|---|
| `id` | kurzer ASCII-Name, angezeigt in der Konsole (`# profile: car=mine radio=xp`) und im Panel |
| `canBitrate` | die gemessene Bitrate in bit/s |
| `physicalLayer` | `vehicle::PhysicalLayer::UNKNOWN`, `HIGH_SPEED` oder `LOW_SPEED_FT`. Nur zur Information: Die Platine bestimmt den Transceiver. Lassen Sie `UNKNOWN`, bis Sie es gemessen haben |
| `keys`, `keyCount` | die Tastentabelle (unten); `nullptr, 0` für keine |
| `knobs`, `knobCount` | die Drehknopftabelle (unten); `nullptr, 0` für keine |
| `txAllow`, `txAllowCount` | `nullptr, 0`. Ein `CUSTOM`-Profil sendet nie |
| `privacyIds`, `privacyCount` | IDs der Frames, die in Ihrem Auto die VIN tragen, falls Sie sie kennen; sonst `nullptr, 0` |
| `decodeParking` | Ihre Parksensor-Funktion, oder `nullptr` |
| `decodeBody` | Ihre Funktion für Zündung und Rückwärtsgang, oder `nullptr` |
| `decodeLights` | Ihre Licht-Funktion, oder `nullptr` |
| `powerOutputs` | `true` nur, wenn `decodeBody` die Zündung zuverlässig liest; `false` hält alle 12-V-Ausgänge aus |
| `keyRemapDefaults`, `keyRemapDefaultCount` | `nullptr, 0`: Jede Taste erreicht das Radio mit ihrem eigenen Code. Zeilen `{from, to}` (XP-Codes) nur für Umbelegungen, die Sie standardmäßig wollen; siehe [Das Radio zuordnen](radio-mapping.md#default-remappings-come-from-the-car-profile) |

Ein `nullptr` ist immer erlaubt: Dieses Signal wird dann nie erzeugt.

### Bytenummern im Code beginnen bei 0 {#byte-numbers-in-code-start-at-0}

Die Dokumentation und die Fahrzeugseiten zählen CAN-Bytes ab **1** (Byte 1 = das erste Datenbyte).
In C++ ist das erste Byte `f.data[0]`, und das Feld `byteIndex` der Tabellen ist derselbe Index.
Eine Taste, die Sie als „Byte 1, Bit 2“ notiert haben, ist also `byteIndex 0, mask 0x04`. Prüfen
Sie das zweimal: Das ist der Fehler, der am leichtesten passiert.

### Bitrate {#bit-rate}

`src/can_bus.cpp` kennt diese Geschwindigkeiten: 50000, 100000, 125000, 250000, 500000 und
1000000 bit/s. Jeder andere Wert kompiliert, aber der CAN-Controller startet nicht: Die Konsole
zeigt `# can=FAIL`, die Status-LED wird rot, und die 12-V-Ausgänge bleiben aus. Das ist der sichere
Zustand, kein Absturz.

### Zündung und Rückwärtsgang: `decodeBody` {#ignition-and-reverse-decodebody}

Die Funktion bekommt jeden empfangenen Frame. Sie gibt `false` für jeden Frame zurück, der nicht
zu ihr gehört oder der zu kurz ist, und `true`, nachdem sie `out` gefüllt hat. Prüfen Sie immer zuerst
die ID und die Länge; lesen Sie nie über die Länge hinaus.

Die Firmware nutzt das Zündungsbyte Ihres Autos nicht direkt. Sie wandeln es in das
fahrzeugneutrale `vehicle::Ignition` um (`firmware/lib/core/vehicle_signals.h`):

| Wert | Bedeutung |
|---|---|
| `OFF` | Zündung aus, oder noch nichts dekodiert |
| `ON` | Zündung an, Motor läuft nicht |
| `ENGINE` | Motor läuft |
| `TRANSITION` | alles andere, was nicht aus ist: Anlassen, Ausschalten, ein Wert, den Sie nicht kennen |

Behalten Sie den Rohwert Ihres Autos in `ignitionRaw`. Er wird nur in der Konsole und im Panel
angezeigt.

Beispiel aus dem erfundenen Auto dieses Tutorials (Frame `2C0`: Byte 2 = Zündung, Byte 4 Bit 6 =
Rückwärtsgang):

```cpp
bool decodeBody(const CanFrame& f, vehicle::BodyState& out) {
    if (f.id != ID_BODY || f.len < 4) return false;   // our frame, long enough?
    out.ignitionRaw = f.data[1];                       // byte 2
    switch (f.data[1]) {
        case 0: out.ignition = vehicle::Ignition::OFF; break;
        case 1: out.ignition = vehicle::Ignition::ON; break;
        case 2: out.ignition = vehicle::Ignition::ENGINE; break;
        default: out.ignition = vehicle::Ignition::TRANSITION; break;
    }
    out.reverseGear = (f.data[3] & 0x40) != 0;         // byte 4 bit 6
    return true;
}
```

Wie die allgemeine Logik das nutzt (`firmware/lib/core/power_policy.h`):

- ACC ist in jedem Zustand außer `OFF` an, das Anlassen schaltet das Radio also nicht aus.
- ACC geht 2.5 s, nachdem die Zündung auf `OFF` geht, aus.
- Der Frame muss laufend weiter eintreffen. Fehlt er länger als 1 s, gilt die Zündung als aus
  ([wie Sie die Periode messen](decode.md#how-often-a-frame-comes)). Im Referenzauto
  kommt der Zündungs-Frame `0F6` alle 500 ms (gemessen: Aufzeichnung
  `can_20261111_191045.log`). Kommt Ihr Zündungs-Frame seltener als einmal pro Sekunde, fallen die
  12-V-Ausgänge ab.
- Der Rückwärtsgang-Ausgang ist nur bei eingeschalteter Zündung und frischem Frame an.

### Licht: `decodeLights` {#lights-decodelights}

Dasselbe Muster: ID und Länge prüfen, `lightsOn` setzen, `true` zurückgeben. Der
Beleuchtungsausgang ist nur an, wenn die Zündung an ist **und** in der letzten Sekunde ein
Licht-Frame kam ([wie Sie die Periode messen](decode.md#how-often-a-frame-comes)). Ein Auto, das seinen Lichtzustand nur bei einer Änderung sendet, verliert den
Beleuchtungsausgang nach einer Sekunde.

### Parksensoren: `decodeParking` {#parking-sensors-decodeparking}

Füllen Sie einen `vehicle::ParkingState` mit derselben Bedeutung wie im Referenzauto:

| Feld | Bedeutung |
|---|---|
| `rearStatus`, `frontStatus` | `4` (`PARK_ACTIVE`) = in Betrieb. Der Piepser nutzt nur eine Seite mit Status 4. Andere Werte: 1 Fehler, 2 vom Fahrer ausgeschaltet, 5 Bereitschaft, 6 außer Betrieb |
| `rearLeft` ... `frontRight` | Zonen 0 bis 7: 0 = sehr nah, 7 = weit oder kein Hindernis. Begrenzen Sie Ihre Werte auf 0..7 |
| `soundRequest`, `soundFront` | das Auto fordert einen Ton an; von vorne (`true`) oder hinten |
| `intervalRaw` | `0x00` = Dauerton, `0x3F` = keine Pieptöne, Werte dazwischen = immer näher |

Beginnen Sie mit `out = vehicle::ParkingState();`, damit jedes Feld, das Sie nicht füllen, seinen
„Nichts“-Wert hat (Zonen 7, Status 0).

Was das Gerät damit macht (`firmware/lib/core/parking_beeper.h`):

- Es piept ab der ersten Zone unter 7 auf einer aktiven Seite, auch wenn das Auto keinen Ton
  anfordert; das Tempo kommt von der nächstgelegenen Zone;
- es spielt den Dauerton **nur**, wenn `soundRequest` gesetzt ist und `intervalRaw` `0x00` ist.
  Meldet Ihr Auto keinen Abstand, berechnen Sie einen aus der Zone, wie es die Vorlage tut;
- es verstummt, wenn 400 ms lang kein Parksensor-Frame kam. Im Referenzauto kommt der Frame alle
  100 ms.

### Fahrzeugdaten für den Radiobildschirm {#car-data-for-the-radio-screen}

Sieben optionale Funktionen, alle nach denselben Regeln: zuerst ID und Länge prüfen, den neutralen
Typ füllen, sein Flag `valid` setzen, `true` zurückgeben (oder die Seiten- bzw. Änderungsbits).
`firmware/lib/car_example/profile_example.cpp` zeigt `decodeDoors` und `decodeTrip`.

- `decodeDoors` -> `DoorState`: `open` enthält die Bits `DOOR_DRIVER`, `DOOR_PASSENGER`,
  `DOOR_REAR_LEFT`, `DOOR_REAR_RIGHT`, `DOOR_BOOT`, `DOOR_BONNET`; `anyOpen` = die Türwarnung des
  Autos. Geben Sie `true` nur zurück, wenn der Frame etwas über die Türen sagt.
- `decodeTrip` -> `TripData`: geben Sie `TRIP_NOW`, `TRIP_1` oder `TRIP_2` für die Seite zurück,
  die der Frame aktualisiert hat.
- `decodeOutsideTemp` -> `OutsideTemp`, in halben Grad Celsius.
- `decodeEcoMode` -> `EcoMode`: das Auto schaltet gleich die Verbraucher ab.
- `decodeClimate` -> `ClimateState`: Temperaturen in halben Grad (0 = kein Wert), Gebläse 0..15,
  Flags `AIR_*`.
- `decodeSteering` -> `SteeringAngle`: Rohwert in der Kodierung des Referenzautos.
- `decodeSettings` -> `VehicleSettings`: pro Eintrag `at(SettingId::...)` der Wert `value`,
  `known` (Ihr letzter Frame hat ihn angegeben) und `present` / `presentKnown` (das Auto hat den
  Eintrag). Geben Sie `SETTINGS_VALUES` und / oder `SETTINGS_PRESENT` zurück.

Funktionen mit einem Argument `io` (Bordcomputer, Klimaanlage, Türen, Einstellungen) bekommen den
Zustand, den der Decoder zwischen den Frames hält. Ändern Sie nur, was Ihr Frame sagt, damit
mehrere Frames einen Zustand ergeben können.

### Tasten: die Tabelle `keys` {#buttons-the-keys-table}

Eine Zeile ist eine Taste, die „gedrückt ist, solange ein Bitmuster vorliegt“:

```cpp
{canId, byteIndex, mask, value, KeyId::...}
```

Die Taste gilt als gedrückt, solange im Frame `canId` `(data[byteIndex] & mask) == value` gilt.
Regeln aus dem Code (`firmware/lib/core/car_profile.h`, `firmware/lib/app/xp_keys.h`):

- die Reihenfolge der Zeilen ist die Reihenfolge der Prüfung und die Reihenfolge der Ereignisse für
  das Radio;
- höchstens 32 Zeilen werden genutzt;
- ein Frame, der kürzer als `byteIndex + 1` ist, wird für diese Zeile übersprungen;
- eine Zeile, deren Muster vorliegt, wenn nichts gedrückt ist, meldet eine dauernd gehaltene Taste.
  Mit `value` = `0x00` passiert das leicht; geben Sie jeder Zeile ein Muster, das nur ein Druck
  erzeugt;
- Druck = sofort ein Ereignis, dann alle 200 ms „gehalten“, solange das Muster bleibt, dann
  „losgelassen“.

Zwei Arten, wie ein Auto Tasten kodieren kann, beide in der Praxis gesehen:

```cpp
// one bit per button (template car: byte 1 bit 0 = volume +, bit 1 = volume -)
{0x200, 0, 0x01, 0x01, KeyId::VOLUME_UP},
{0x200, 0, 0x02, 0x02, KeyId::VOLUME_DOWN},

// a button number in a group of bits (tutorial car: byte 1 bits 3-0 = 1, 2, 3, ...)
{0x1F0, 0, 0x0F, 0x01, KeyId::VOLUME_UP},
{0x1F0, 0, 0x0F, 0x02, KeyId::VOLUME_DOWN},
```

Auch das Referenzprofil nutzt die zweite Art: Im Frame `21F` Byte 1 ergibt die Maske `0C`
Lautstärke + (`08`), Lautstärke - (`04`) und beide zusammen, stumm (`0C`) (gemessen: Aufzeichnungen
`can_20261111_190817.log` und `can_20261111_191045.log`, CAN und die Leitung der CAN-Box zum Radio
mit gemeinsamer Zeitbasis).

Die Namen (`KeyId`) sind logische Tasten. Welchen Code das Radio für jeden Namen bekommt, ist
Aufgabe des Radioprotokolls: siehe [Das Radio zuordnen](radio-mapping.md).

### Drehknöpfe: die Tabelle `knobs` {#rotary-knobs-the-knobs-table}

Ein Drehknopf ist ein 8-Bit-Zähler in einem Byte eines Frames:

```cpp
{canId, byteIndex, onIncrease, onDecrease}
```

Ein Zählerschritt von +1 ist eine Raste `onIncrease`, -1 eine Raste `onDecrease`. Höchstens 4
Rasten werden pro Frame gemeldet, und höchstens 4 Drehknopfzeilen werden genutzt. Der erste Frame
setzt nur den Startwert. Zählt Ihr Drehknopf rückwärts, wenn Sie ihn „vorwärts“ drehen, tauschen
Sie die beiden Namen, wie es das Referenzprofil für Frame `21F` tut.

## Schritt 4: bauen {#step-4-build-it}

Ihr Profil wird mit drei Build-Flags gewählt, ohne etwas in `src/` zu ändern. Die Datei
`firmware/platformio_override.sample.ini` hat schon eine Umgebung für ein Profil namens `mine`:

1. Kopieren Sie `firmware/platformio_override.sample.ini` nach `firmware/platformio_override.ini`.
   Die neue Datei wird von Git nicht versioniert; `platformio.ini` lädt sie automatisch.
2. Heißt Ihr Profil nicht `mine`, ändern Sie den Abschnitt `listen_mycar` so, dass er wie unten
   aussieht, mit Ihrem Namen statt `<name>` und `<Name>`:

    ```ini
    [env:listen_mycar]
    extends = env:listen_wifi
    lib_deps = car_<name>
    build_flags =
        ${env:listen_wifi.build_flags}
        -DCAR_PROFILE=CUSTOM
        '-DCAR_PROFILE_CUSTOM_HEADER="profile_<name>.h"'
        -DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>
    ```

    `lib_deps` ist nötig: PlatformIO folgt keinem `#include`, das über ein Makro kommt. Der
    Abschnitt `listen_mine` in derselben Datei ist für Ihre eigene Platine, nicht für Ihr Auto;
    lassen Sie ihn stehen.
3. Bauen Sie, aus dem Ordner `firmware`:

    ```bash
    uvx platformio run -e listen_mycar
    ```

    Erwartet: Der Build endet mit `SUCCESS`, und die Abhängigkeitsliste im oberen Teil zeigt
    `|-- car_mine @ 1.0.0`. Andere Meldungen: siehe [Häufige Fehler](#common-mistakes).
4. Flashen Sie und öffnen Sie die Konsole:

    ```bash
    uvx platformio run -e listen_mycar -t upload
    uvx platformio device monitor
    ```

    Erwartet: `# profile: car=mine radio=xp` beim Start. Port und Bootloader-Modus:
    [Die Firmware flashen](../quick-start/flash.md).

`listen_mycar` ist ein reiner Mithör-Build im Parallelmodus (neben der Box): Er dekodiert Ihr Auto,
gibt die Pieptöne der Parksensoren über den Lautsprecher des Geräts aus und schreibt
Konsolenzeilen, sendet aber nichts an das Radio und hält die 12-V-Ausgänge aus. Tippen Sie in der
Konsole `keys`, um jedes Tastenereignis auszugeben, und `dump`, um jeden CAN-Frame auszugeben.

### Decoder-Modus mit Ihrem Profil {#decoder-mode-with-your-profile}

Um das Radio anzusteuern (UART-Frames, ACC-, Beleuchtungs- und Rückwärtsgang-Ausgang), fügen Sie
Ihrer `platformio_override.ini` einen zweiten Abschnitt hinzu. Dieser wurde im Tutorial-Lauf
erfolgreich gebaut:

```ini
[env:decoder_mycar]
extends = env:decoder_wifi
lib_deps = car_mine
build_flags =
    ${env:decoder_wifi.build_flags}
    -DCAR_PROFILE=CUSTOM
    '-DCAR_PROFILE_CUSTOM_HEADER="profile_mine.h"'
    -DCAR_PROFILE_CUSTOM_OBJECT=kCarMine
```

> [!CAUTION]
> **Der Decoder-Modus steuert das Radio und die 12-V-Ausgänge**
>
> Flashen Sie einen Decoder-Build erst, wenn das Profil seinen Test auf dem PC und auf dem
> Prüftisch bestanden hat, und nur, wenn die mitgelieferte CAN-Box abgezogen ist: Zwei Sender auf
> einer UART-Leitung und zwei ACC-Quellen bedeuten Ausgänge, die gegeneinander arbeiten. Lesen
> Sie zuerst [Radioanschluss](../../en/install-in-car/radio-connection.md) und
> [Erste Fahrt](../../en/install-in-car/first-drive.md). Am Fahrzeug-CAN-Bus hört auch dieser Build nur
> mit.

### Was sich absichtlich nicht bauen lässt {#what-does-not-build-on-purpose}

| Versuch | Ergebnis |
|---|---|
| `decoder_tx_wifi` mit `-DCAR_PROFILE=CUSTOM` | `#error "CAN transmit is not allowed with CAR_PROFILE=CUSTOM ..."` (im Tutorial-Lauf ausprobiert) |
| `decoder_tx_wifi` mit `-DCAR_PROFILE=GENERIC` | `#error "CAN transmit is not allowed with CAR_PROFILE=GENERIC ..."` (geprüft von `tools/check_tx_profile_lock.sh`) |
| `-DCAR_PROFILE=EXAMPLE` in einer beliebigen Umgebung | `#error "CAR_PROFILE=EXAMPLE is a template for the host tests only ..."` |
| `CUSTOM` ohne das Header- oder Objekt-Flag | `#error "CAR_PROFILE=CUSTOM needs ..."` |

## Nicht mit dem Referenzprofil anfangen {#do-not-start-from-the-reference-profile}

Sie könnten statt der Vorlage `firmware/lib/car_psa_aee2010/` kopieren. Bedenken Sie, was dann
passiert: Behält Ihr Profil die Referenzfunktionen, dekodiert es die Frame-IDs des Referenzautos (Türen,
Einstellungen, Bordcomputer, Klima). Behält es auch `psa::decodeBodyState` für die Zündung, behandelt
die Firmware Ihr Auto auch beim Ändern von Einstellungen und bei den Sendebedingungen als
PSA-AEE2010-Auto (`firmware/lib/app/decoder.cpp`, `Decoder::begin`). Das ist nur für ein Auto richtig, bei dem gemessen wurde, dass es dieselben
Frames nutzt. Behandeln Sie jeden Frame eines anderen Autos als **unbestätigt** (unverified), bis
Sie ihn aufgezeichnet haben, selbst bei einem nahen Verwandten des Referenzautos.

## Häufige Fehler {#common-mistakes}

| Symptom | Ursache |
|---|---|
| `fatal error: profile_mine.h: No such file or directory` | `name` in `library.json` nicht geändert, oder `lib_deps` fehlt |
| `# can=FAIL` beim Start, rote LED | `canBitrate` ist keine der sechs unterstützten Geschwindigkeiten |
| eine Taste wird als dauernd gehalten gemeldet | ihr Muster aus `mask` / `value` liegt in Ruhe vor |
| Tasten auf dem falschen Bit | `byteIndex` ab 1 statt ab 0 gezählt |
| Beleuchtungsausgang fällt nach 1 s ab | der Licht-Frame wird nicht mindestens einmal pro Sekunde wiederholt |
| kein Piepton, aber die Zonen ändern sich | der Status der Seite ist nicht `4` (`PARK_ACTIVE`) |

Weiter: [Ihr Profil testen](test-your-profile.md).
