# Das Radio zuordnen

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Ihr Fahrzeugprofil sagt, **welche CAN-Bits welche Taste sind**. Das Radioprotokoll sagt, **welchen
Code das Radio für diese Taste bekommt**. Dazwischen steht eine Liste logischer Tastennamen,
`KeyId`. Diese Seite erklärt die drei Teile und wie Sie ändern, was eine Taste im Radio tut, mit
oder ohne neuen Build.

## Drei Tabellen {#three-tables}

```text
CAN frame ──► car profile ──► logical button ──► radio protocol ──► key code ──► remapping ──► radio
              keys / knobs      KeyId             keyCode()          e.g. 14       map FROM TO
```

| Teil | Datei | Ändert sich mit |
|---|---|---|
| CAN-Bits -> `KeyId` | Ihr Profil, Tabellen `keys` und `knobs` ([Ein Fahrzeugprofil schreiben](write-a-profile.md)) | dem Auto |
| `KeyId` -> Tastencode | das Radioprotokoll, `firmware/lib/radio_xp/protocol_xp.cpp` (`xp::keyCode`) | dem Radioprotokoll |
| Tastencode -> anderer Tastencode | Umbelegungstabelle, zur Laufzeit, gespeichert in den Einstellungen des Geräts | Ihrem Geschmack |

Das Fahrzeugprofil kennt keine Radiocodes, und das Radioprotokoll kennt keine CAN-IDs.
`tools/check_layering.py` prüft das.

## Verfügbare Radioprotokolle {#radio-protocols-available}

| Build-Flag | Protokoll | Was es tut |
|---|---|---|
| `-DRADIO_PROTOCOL=XP` (Standard) | `xp` | das Protokoll der CAN-Box von Simple Soft („XP“) zu einem Android-Radio auf der FYT-Plattform, UART 38400 8N1. Nur Befehle, die auf der Leitung gesehen wurden, werden genutzt: [XP-Protokoll](../../en/reference/xp-protocol.md) |
| `-DRADIO_PROTOCOL=NONE` | `none` | kein Radio: Der UART wird nicht gestartet, keine Taste hat einen Code, es wird nie etwas gesendet |
| `-DRADIO_PROTOCOL=CUSTOM` | eigenes | eigenes Protokoll in `firmware/lib/radio_<name>`, angegeben mit `RADIO_PROTOCOL_CUSTOM_HEADER` und `RADIO_PROTOCOL_CUSTOM_OBJECT` (Vorlage: `firmware/lib/radio_example`). Auf dem CAN nur Mithören: ein Build mit Senden bricht mit einem Fehler ab |

`listen_generic_wifi` nutzt `none`. Ihr eigenes Profil mit der Umgebung `listen_mycar` nutzt `xp`,
aber im Parallelmodus (neben der Box) wird ohnehin nichts an das Radio gesendet.

> [!NOTE]
> **Noch kein anderes Radioprotokoll**
>
> Die Fahrzeugdaten für den Radiobildschirm (Türen, Bordcomputer, Klimaanlage, Einstellungen ...)
> erreichen ein Radioprotokoll über die Encoder in `RadioProtocol`
> (`firmware/lib/core/radio_protocol.h`), als neutrale Typen. Die übrige XP-Logik (die Verbindung
> zum Radio, die Parksensor- und Tasten-Frames, die Befehle vom Radio) ist noch fest im Decoder
> verdrahtet. Jeder Frame an das Radio, auch die der Encoder, bekommt die XP-Rahmung (Kopf `2E`,
> Länge, XP-Prüfsumme), und die XP-Frames für Stromzustand (`EA`), Parksensoren (`32`), Hindernis
> (`3E`) und Tasten (`20`) werden ebenfalls gesendet: `RadioProtocol` hat noch keine Funktion für
> die Rahmung. Daher braucht ein neues Radioprotokoll auch Änderungen in `firmware/src/config.h` und
> `firmware/lib/app/`. Das steht auf der Roadmap. Ein neues Protokoll muss aus Ihren eigenen
> Messungen der Radioverbindung geschrieben werden, nicht aus dekompilierten Apps oder Box-Firmware.

## Logische Tasten und XP-Codes {#logical-buttons-and-xp-codes}

Die Namen beschreiben die Taste des Referenzautos (Peugeot 508 SW 2015). Nehmen Sie für Ihr Auto
den Namen mit der nächstliegenden Bedeutung. Der XP-Code ist das, was das Radio im Tasten-Frame
`20` empfängt (DATA[0] = Code, DATA[1] = 1 gedrückt, 2 gehalten, 0 losgelassen). Das ist ein UART-Frame: Seine
Datenbytes werden ab DATA[0] gezählt, wie auf der Seite [XP-Radioprotokoll](../../en/reference/xp-protocol.md).
CAN-Bytes auf den anderen Seiten werden ab Byte 1 gezählt.

| `KeyId` | Taste im Referenzauto | XP-Code |
|---|---|---|
| `VOLUME_UP` | Lenkrad Lautstärke + | `14` |
| `VOLUME_DOWN` | Lenkrad Lautstärke - | `15` |
| `MUTE` | beide Lautstärketasten zusammen | `16` |
| `PHONE` | Telefontaste am Lenkrad | `30` |
| `LIST` | Lenkrad LIST | `50` |
| `WHEEL_KNOB_PRESS` | Lenkrad-Daumenrad gedrückt | `07` |
| `SOURCE` | SRC | `21` |
| `TRIP` | Bordcomputer-Taste am Wischerhebel | `23` |
| `NAV` | Bedienteil: NAV | `9E` |
| `MEDIA` | Bedienteil: MEDIA | `A0` |
| `SCROLL_LEFT` | Bedienteil: Suchlauf links | `97` |
| `ZOOM_IN` | Bedienteil: Zoom + | `9C` |
| `SCROLL_RIGHT` | Bedienteil: Suchlauf rechts | `98` |
| `TEL` | Bedienteil: TEL | `9F` |
| `SETUP` | Bedienteil: SETUP | `99` |
| `DRIVE` | Bedienteil: DRIVE | `9B` |
| `MOON` | Bedienteil: Bildschirm aus (Mond) | `A4` |
| `ZOOM_OUT` | Bedienteil: Zoom - | `9D` |
| `MIC_MUTE` | Bedienteil: Mikrofon stumm | `A1` |
| `WEB` | Bedienteil: WEB | `A3` |
| `PANEL_EXTRA` | eine Bedienteil-Taste, nicht identifiziert | `22` |
| `PANEL_KNOB_PRESS` | Ein/Aus-Drehknopf des Bedienteils gedrückt | `16` (derselbe Code wie `MUTE`) |
| `NEXT` / `PREV` | Lenkrad-Daumenrad, eine Raste | `17` / `18` |
| `PANEL_KNOB_UP` / `PANEL_KNOB_DOWN` | Drehknopf des Bedienteils, eine Raste rechts / links | `81` / `82` |

Status: **gemessen** (measured). Die Codes sind das, was die CAN-Box von Simple Soft für jede Taste
an das Radio gesendet hat, aufgezeichnet zusammen mit dem CAN-Bus mit gemeinsamer Zeitbasis (Aufzeichnungen
`can_20261111_190817.log` und `can_20261111_191045.log` in `firmware/test_data/captures/`; Herkunft
jeder Konstante in `radios/xp-simple-soft/provenance.md`). Die Taste, die Frame `0A2` sendet
(`SOURCE`), ist in den Notizen zum Referenzauto auf zwei Arten beschrieben („SRC“ und „die Taste
zwischen SRC und LIST“); sie muss im Auto noch einmal geprüft werden.

Hat Ihr Auto eine Taste ohne passenden Namen, kann ein neuer Name zu `enum class KeyId` in
`firmware/lib/core/vehicle_signals.h` hinzugefügt werden, **nur am Ende der Liste**, damit die
Nummern der bestehenden Namen gleich bleiben. Er braucht auch einen Code im Radioprotokoll, sonst
wird er nie gesendet.

## Standard-Umbelegungen kommen aus dem Fahrzeugprofil {#default-remappings-come-from-the-car-profile}

Kurz bevor ein Tasten-Frame an das Radio geht, wendet die Firmware eine kleine Umbelegungstabelle
an (`firmware/lib/radio_xp/xp_keys_custom.h`). Ihre Anfangseinträge kommen aus dem Fahrzeugprofil
(Felder `keyRemapDefaults` und `keyRemapDefaultCount`, siehe [Ein Fahrzeugprofil schreiben](write-a-profile.md)).
Das Referenzprofil `psa_aee2010` hat zwei, gewählt für das Referenzauto und sein Radio:

| Von | Nach | Warum (gemessen: Test des Autors im Auto, ein Radio) |
|---|---|---|
| `9F` (`TEL`) | `9B` | öffnet den Telefonbildschirm; `9F` tat ohne verbundenes Telefon nichts |
| `21` (`SOURCE`) | `1F` | Sprachassistent (Siri in CarPlay) |

Das sind Vorlieben des Besitzers, keine Fakten über den Bus: Ein anderes Auto hat diese Tasten
vielleicht nicht. Die Profile `generic` und `example` haben keine Standardeinträge, ebenso ein aus
der Vorlage kopiertes Profil; jede Taste erreicht das Radio also mit ihrem eigenen Code. Der
Vorlagentest zeigt das: `[key] code=21 state=1` (SRC geht als `21`).

Für eine Standard-Umbelegung in Ihrem eigenen Profil fügen Sie eine Zeile zu dessen Tabelle
`keyRemapDefaults` hinzu. Zum Entfernen im laufenden Gerät: `map 21 21` in der USB-Konsole oder
der Abschnitt „Tastenbelegung“ des Web-Panels.

Die Mondtaste (`A4`) wird getrennt von der Tabelle behandelt (die Verriegelung folgt dem Code, den
das Radioprotokoll `MOON` gibt, in XP `A4`, nicht jeder Taste, die zufällig diesen Code bekommt):
Der erste Druck sendet nur
„gedrückt“ (das Radio schaltet seinen Bildschirm dunkel), der zweite nur „losgelassen“. Eine
Umbelegung für `A4` hat keine Wirkung.

## Eine Zuordnung ohne neuen Build ändern {#change-a-mapping-without-rebuilding}

Befehle der USB-Konsole ([USB-Konsole](../../en/reference/console.md)):

| Befehl | Was er tut |
|---|---|
| `map` | listet die Tabelle, zum Beispiel `[key] swap 9F -> 9B` |
| `map FROM TO` | Tastencode `FROM` sendet `TO` an das Radio, beide in Hex: `map 9B 1F`. `map 9B 9B` entfernt ihn |
| `map reset` | zurück zu den Standardeinträgen des Fahrzeugprofils (in `generic` keine) |
| `keys` | schaltet die Ausgabe jedes Tastenereignisses ein oder aus (`[key] printing=1` / `0`) |

Die Tabelle hält höchstens 16 Einträge und wird in den Einstellungen des Geräts gespeichert, sie
übersteht also einen Neustart. Das Web-Panel bearbeitet dieselbe Tabelle.

`FROM` ist der Code, den das Radioprotokoll der Taste gibt (die dritte Spalte oben), kein CAN-Wert.

## Herausfinden, was ein Code in Ihrem Radio tut {#find-out-what-a-code-does-in-your-radio}

In einem Decoder-Build können Sie aus der USB-Konsole jeden Tastencode an das Radio senden, ohne
das Auto zu berühren:

| Befehl | Was er sendet |
|---|---|
| `key 1F` | Code `1F` gedrückt, dann losgelassen |
| `kd A4` | nur „gedrückt“ |
| `ku A4` | nur „losgelassen“ |

In einem reinen Mithör-Build antwortet die Konsole `: not sent (listen mode)`, und nichts geht
hinaus.

Was einige Codes im Radio des Referenzautos taten, ausprobiert mit `key` (gemessen: Test des Autors,
ein Radio, CAN-Profil „XP Simple Soft -> Peugeot 508 -> Type 2“):

| Code | Reaktion |
|---|---|
| `08` | Quellenwechsel |
| `1F` | Sprachassistent (Siri in CarPlay) |
| `9A` | Bildschirmhelligkeit |
| `9B` | Telefon |
| `A5` | Bildschirm der Klimaanlage |
| `A6` | zurück |
| `B5` | FM / AM |
| `B9`, `BF` | beide: Senderabstimmung um 0.05 MHz nach unten (zwei Codes mit derselben Notiz; noch einmal zu prüfen) |
| `B1`, `91` bis `96` | keine Reaktion |

Ein anderes Radio oder ein anderes CAN-Profil in den Einstellungen des Radios kann anders reagieren.
Probieren Sie jeden Code an Ihrem eigenen Radio aus und notieren Sie, was er getan hat.

## Was mit Ihrem Profil sonst noch an das Radio geht {#what-else-goes-to-the-radio-with-your-profile}

Mit dem XP-Protokoll und einem anderen Profil als dem Referenzprofil bekommt das Radio nur diese
Frames (gemessen: Host-Test des Vorlagenprofils, `firmware/test_host/expected/example_car.txt`):

| XP-Befehl | Inhalt | Aus Ihrem Profil |
|---|---|---|
| `EA` | Stromzustand (ACC) | `decodeBody` (Zündung) |
| `20` | Taste | `keys`, `knobs` |
| `32` | Parksensorbild | `decodeParking` |
| `3E` | Hindernis-Flag | `decodeParking` |

Die Fahrzeugdaten für den Radiobildschirm kommen aus den optionalen Funktionen Ihres Profils:

- `38` Türen und Einstellungswerte: `decodeDoors`, `decodeSettings`;
- `3C` welche Einstellungszeilen angezeigt werden: `decodeSettings`;
- `33`, `34`, `35` Seiten des Bordcomputers: `decodeTrip`;
- `36` Außentemperatur: `decodeOutsideTemp`;
- `44` Energiesparmodus: `decodeEcoMode`;
- `21` Zustand der Klimaanlage: `decodeClimate`;
- `29` Lenkwinkel: `decodeSteering`.

Ein Frame, dessen Funktion in Ihrem Profil `nullptr` ist, wird nie gesendet. Die XP-Encoder stehen
in `firmware/lib/radio_xp/xp_signals.h`. Das Ändern von Fahrzeugeinstellungen vom Radio aus
(Befehl `80`) funktioniert nur mit dem Referenzprofil.

Weiter: [Portierungs-Checkliste](porting-checklist.md).
