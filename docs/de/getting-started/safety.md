# Sicherheit zuerst

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Lesen Sie diese Seite, bevor Sie etwas bauen, flashen oder anschließen. Sie ist absichtlich kurz.

## Begriffe auf diesen Seiten {#words-used-on-these-pages}

- **das Gerät**: die ESP32-S3-Platine dieses Projekts;
- **die Box** oder **CAN-Box**: die nachgerüstete Interface-Box, die mit dem Radio verkauft wird;
- **das Radio**: das Android-Autoradio;
- **nur Mithören** (listen-only): Das Gerät empfängt CAN-Frames und sendet nichts auf den
  Fahrzeugbus;
- **TWAI**: der im ESP32-S3 eingebaute CAN-Controller;
- **SLNT**: der Silent-Pin des CAN-Transceiver-Moduls; High = sein Sender ist aus;
- **BSI**: das Bordnetzsteuergerät des Autos;
- **`JP1`**: ein Jumper auf der Prototyp-Platine in der TX-Leitung zum CAN-Modul;
- **Umgebung**: ein benannter Satz von Build-Optionen; er legt fest, was das Programm tun kann.

Das [Glossar](../../en/contributing/glossary.md) listet weitere Begriffe.

## Was dieses Projekt ist {#what-this-project-is}

- Ein Hobbyprojekt, bereitgestellt **so, wie es ist**, ohne jede Gewähr. Die Nutzung erfolgt auf eigenes Risiko.
- Es wurde an **einem Auto** (einem Peugeot 508 SW von 2015) mit einem Android-Radio entwickelt
  und gemessen. An jedem anderen Auto ist jeder Frame **unbestätigt** (unverified), bis Sie ihn
  selbst aufgezeichnet haben.
- Es ist **nicht** für Dinge gedacht, die Menschen schützen. Schließen Sie es nicht an Bremsen,
  Lenkung, Airbags, Motorsteuerung oder einen Bus an, der diese überträgt.
- Das Projekt ist nicht mit Peugeot, Stellantis, Simple Soft, Raise, Junsun oder FYT verbunden.
  Ihre Namen stehen hier nur, um zu sagen, was getestet wurde.

## Das Gerät hört standardmäßig nur mit {#the-device-listens-by-default}

Auf der Fahrzeugseite ist das Gerät ein CAN-Empfänger. In jedem normalen Build **hört es nur
mit**:

- der TWAI-Controller läuft im Modus „nur Mithören“ (er sendet nicht einmal das Bestätigungsbit
  ACK);
- das Programm hält den Pin `SLNT` des CAN-Transceivers auf High, damit der Sender des Chips aus
  ist;
- die Builds `listen`, `decoder`, `listen_wifi`, `decoder_wifi` und `listen_generic_wifi`
  enthalten den Sendecode überhaupt nicht (das Verzeichnis `firmware/src/can_tx/` wird nicht
  kompiliert; `firmware/tools/check_no_can_tx.sh` prüft die kompilierten Programme).

> [!CAUTION]
> **Senden kann den Zustand des Autos ändern**
>
> Flashen Sie `decoder_tx_wifi` nur, wenn Sie
> [Senden auf dem CAN-Bus](../../en/safety/transmit.md) gelesen haben und das Risiko bewusst in Kauf
> nehmen. Senden Sie nie Frames, die Sie nicht selbst am Auto aufgezeichnet haben.

Nur eine Umgebung, `decoder_tx_wifi`, kann auf den Fahrzeugbus senden. Das tut sie erst, wenn
Sie sie absichtlich scharf schalten und das Gerät neu starten:

| Build | Frames, die er auf den Fahrzeugbus senden darf |
|---|---|
| alle Umgebungen außer `decoder_tx_wifi` | keine |
| `decoder_tx_wifi` (Standard) | nur Frame `15B`, nur nach ID und Länge geprüft, nach `cantx on` und einem Neustart |
| `decoder_tx_wifi` mit `-DCAN_TX_EXPERIMENTAL=1` in Ihrer eigenen `platformio_override.ini` | `15B`, dazu der Versuchs-Frame `1A9` (nur die Tasten Einparkhilfe und Toter Winkel) |

Die Klimaanlage über das Radio zu steuern, funktioniert mit diesem Projekt **nicht**. Bei `15B`
hat das BSI die Einstellungen angenommen (sichtbar in Frame `260`), aber die sichtbare Wirkung im
Auto wurde **nicht geprüft**. Was jeder Frame am Referenzauto bewirkt hat:
[Senden auf dem CAN-Bus](../../en/safety/transmit.md).

## Hardware-Sperre auf der Prototyp-Platine {#hardware-lock-on-the-prototype-board}

Die [Prototyp-Platine](../../en/hardware/prototype-pcb.md) hat laut Zeichnung eine Hardware-Sperre:
Der Pin `SLNT` des CAN-Moduls wird über einen Widerstand auf 3.3 V gezogen, und der Jumper
`JP1` bleibt **offen**. Mit offenem `JP1` kann kein Programm auf den Bus senden.

Die eigene Platine des Autors wurde später für Sendetests geändert (`JP1` geschlossen, `SLNT`
an GPIO6). Auf so einer Platine entscheidet **nur die Firmware**, ob das Gerät sendet. Lassen
Sie `JP1` offen, es sei denn, Sie bauen bewusst die Sende-Firmware.

## Bevor Sie anfangen {#before-you-start}

Diese Regeln stammen aus der Entstehung des Projekts selbst. Befolgen Sie sie in dieser
Reihenfolge.

1. **Erst messen, dann anschließen.** Finden Sie heraus, welche Pins CAN führen, welcher Bustyp
   es ist (High-Speed oder fehlertoleranter Low-Speed-CAN) und wie schnell er ist: mit einem
   Multimeter und besser mit einem Oszilloskop oder Logikanalysator. Schließen Sie nichts an den
   Bus an, bevor Sie das wissen. Die Seite [Ihren CAN-Bus finden](../make-it-yours/find-your-bus.md)
   zeigt, wie.
2. **Kein Terminator.** Der Fahrzeugbus ist schon terminiert. Schalten Sie den 120-Ω-Terminator
   Ihres CAN-Moduls aus und fügen Sie keinen hinzu.
3. **Zuerst mithören.** Auf dem Schreibtisch ist jeder Build, der nur mithört, in Ordnung. Im
   Auto nutzen Sie `listen` oder `listen_wifi` nur am Referenzauto (Peugeot 508, Profil
   PSA AEE2010); in jedem anderen Auto nur `listen_generic_wifi` (generisches Profil: zeichnet
   Frames auf, sendet nichts an das Radio). Aufzeichnen, dekodieren, vergleichen. Senden kommt
   zuletzt, wenn überhaupt.
4. **Keine Schreibzugriffe auf Steuergeräte des Autos.** Dieses Projekt macht keine Diagnose und
   schreibt nie in Module des Autos (BSI oder andere). Fügen Sie keinen solchen Code hinzu.
5. **Eine Quelle pro Draht.** Nutzen Sie den Decoder-Modus (`decoder`, `decoder_wifi`) nur bei
   abgezogener nachgerüsteter CAN-Box. Zwei Sender an der Datenleitung des Radios oder zwei
   Quellen am ACC-Draht bedeuten kurzgeschlossene Ausgänge.
6. **Erst Prüftisch, dann Auto.** Versorgen Sie die Trägerplatine aus einem Labornetzteil mit
   Strombegrenzung (etwa 100 mA beim ersten Einschalten), bevor Sie sie ins Auto bringen. Das
   ESP32-S3-Board allein am USB-Kabel braucht kein Labornetzteil. Siehe
   [Prüftisch-Test](../quick-start/bench-test.md).

## 12 V und die Batterie {#12-v-and-the-battery}

> [!WARNING]
> **Eingangsschutz vor dem Auto bestücken**
>
> Bestücken Sie auf der Prototyp-Platine die Überspannungsschutzdiode `D2` (SMBJ18A) und
> nehmen Sie einen Eingangskondensator für 35 V oder mehr, bevor Sie die Platine ans Auto
> anschließen. Lassen Sie die Sicherungen eingesetzt; eine Drahtbrücke statt einer Sicherung
> ist nur für den Prüftisch.

- Das Gerät hängt an Dauerplus 12 V (wie die CAN-Box, die es ersetzt). Nach 30 s Stille auf dem
  Bus geht es in den Tiefschlaf. Der Schlafmodus und die Stromaufnahme im Schlaf wurden an der
  Hardware **noch nicht gemessen**. Messen Sie den Ruhestrom des Autos mit und ohne Gerät, bevor
  Sie es tagelang angeschlossen lassen.
- Mit eingeschaltetem WLAN braucht das Gerät mehr Strom. Die Firmware schaltet das WLAN aus,
  wenn das Auto schläft und niemand das Panel benutzt.

## Das Web-Panel und Aktualisierungen über WLAN {#the-web-panel-and-updates-over-wi-fi}

Die `*_wifi`-Builds öffnen ein eigenes WLAN mit einem Web-Panel. Beachten Sie die Grenzen:

- das Panel nutzt einfaches HTTP; verschlüsselt ist nur das WLAN selbst (WPA2);
- das Gerätepasswort ist zugleich der WLAN-Schlüssel;
- Firmware-Images, die über WLAN geschickt werden, sind **nicht signiert**. Wer das Passwort
  hat, kann ein anderes Programm flashen. Auf einer Platine ohne Hardware-Sperre könnte dieses
  Programm senden.

Mehr dazu auf der Seite [Sicherheitsmodell](../../en/safety/security.md).

## Weiter {#next}

[Was Sie brauchen](../../en/getting-started/requirements.md): Platine, Teile und Werkzeug.
