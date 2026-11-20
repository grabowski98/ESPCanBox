# Erster Start

> [!NOTE]
> **Maschinelle Übersetzung, Prüfung ausstehend**
>
> Diese Seite wurde automatisch übersetzt. Verbindlich ist die englische Seite. Fehler in der
> Übersetzung melden: [Übersetzungen](../../en/contributing/translations.md#send-your-work).

Diese Seite zeigt, wie Sie das Gerätepasswort ablesen und das Web-Panel auf Telefon oder Laptop
öffnen.

Sie gilt für die Builds mit Netzwerk (`listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`,
`listen_generic_wifi`). Sie öffnen ein eigenes WLAN mit einem Web-Panel. Die Builds ohne
Netzwerk (`listen`, `decoder`) enthalten überhaupt keinen WLAN-Code; für sie gibt es nur die
USB-Konsole.

Noch keine Platine? Springen Sie zu [Panel ohne Platine ausprobieren](#try-the-panel-without-a-board).

## 1. Build mit Netzwerk flashen {#1-flash-a-build-with-network}

Wenn Sie `listen_wifi` wie auf der Seite [Firmware flashen](flash.md) geflasht haben, gehen Sie
zu Schritt 2. Sonst führen Sie im Terminal, das im Ordner `firmware` geöffnet ist (wie auf jener
Seite), aus:

```text
uvx platformio run -e listen_wifi -t upload
uvx platformio device monitor
```

Lassen Sie die Platine am Computer angeschlossen. Solange ein Computer an USB hängt, lässt das
Gerät das WLAN eingeschaltet, auch ohne CAN-Verkehr.

## 2. Passwort an der USB-Konsole ablesen {#2-read-the-password-from-the-usb-console}

Beim allerersten Start erzeugt das Gerät ein zufälliges Passwort mit 12 Zeichen und speichert es.
Die Konsole gibt aus:

```text
[wifi] device password generated; the command wifi showpass shows it
```

Diese Zeile sehen Sie vielleicht nicht: Sie kommt beim Start, oft bevor der Monitor verbunden ist.
Das macht nichts. Im Monitor (Ihre Eingabe wird nicht angezeigt; drücken Sie nach jedem Befehl
Enter):

1. Tippen Sie `wifi`. Notieren Sie den Netzwerknamen aus der Ausgabe.
2. Tippen Sie `wifi showpass`. Das Passwort steht in der Zeile `[wifi] password: <12 Zeichen>`.

Das Passwort ist zugleich der **WLAN-Schlüssel** des Gerätenetzes und das **Panel-Passwort**. Es
wird nur an der USB-Konsole ausgegeben, nie im Panel. Ein vergessenes Passwort zeigt
`wifi showpass` über USB jederzeit wieder an.

> [!WARNING]
> **Passwort nicht in geteilten Logs**
>
> Die Zeile, die `wifi showpass` ausgibt, enthält das Passwort. Fügen Sie sie nicht in einen
> Fehlerbericht, einen Forenbeitrag oder eine Aufzeichnung ein, die Sie teilen.

Optionale Befehle:

| Befehl | Was er zeigt oder tut |
|---|---|
| `wifi` | Zustand des Netzwerks: Name, die Adresse `http://10.50.8.1`, den `.local`-Namen, verbundene Geräte, Heimnetz |
| `wifi showpass` | das Gerätepasswort |
| `wifi pass <new password>` | setzt ein eigenes Passwort: 10 bis 63 druckbare Zeichen, ohne Leerzeichen |
| `wifi newpass` | ein neues zufälliges Passwort; das alte gilt dann auf keinem Gerät mehr |

Alle `wifi`-Befehle funktionieren nur an der USB-Konsole; das Panel lehnt sie ab.

## 3. Mit dem Gerätenetz verbinden {#3-connect-to-the-device-network}

1. Öffnen Sie auf Telefon oder Laptop die WLAN-Liste. Der Netzwerkname besteht aus dem
   Projektnamen, einem `-` und vier Hex-Ziffern aus der MAC-Adresse des Chips
   (`<project name>-XXXX`). Der Befehl `wifi` gibt den genauen Namen aus.
2. Verbinden Sie sich mit dem Passwort aus Schritt 2. Es ist ein WPA2-Netz.
3. Fragt das Telefon, ob es mit einem Netz ohne Internet verbunden bleiben soll, wählen Sie
   „verbunden bleiben“.

Höchstens vier Geräte können gleichzeitig verbunden sein (zum Beispiel ein Laptop, Ihr Telefon und das
Radio).

> [!NOTE]
> **VPN und mobile Daten**
>
> Ein VPN auf Laptop oder Telefon kann die Adresse `10.50.8.1` am WLAN vorbeileiten. Schalten
> Sie das VPN aus, solange Sie das Panel benutzen. Öffnet sich das Panel auf einem Telefon
> nicht, schalten Sie auch die mobilen Daten kurz aus.

## 4. Panel öffnen {#4-open-the-panel}

1. Öffnen Sie `http://10.50.8.1` im Browser. Der `.local`-Name, den `wifi` ausgibt, funktioniert
   auf Systemen mit mDNS ebenfalls.
2. Melden Sie sich mit dem Gerätepasswort an. Aktivieren Sie **Auf diesem Gerät merken** nur auf
   Ihrem eigenen Telefon oder Laptop: Dann öffnet ein neuer Tab das Panel ohne erneute Anmeldung.

Das Panel nutzt einfaches `http://`, nicht `https://`. Wenn Ihr Browser von selbst auf
`https://` umschaltet, tippen Sie die Adresse mit `http://` davor ein.

Erwartet: Das Panel zeigt fünf Reiter: **Übersicht** (englisch: Dashboard), **CAN**, **Radio**,
**Konsole** und **System**.

[![Übersicht des Web-Panels (Geräte-Mock, dunkles Schema)](../../assets/screenshots/panel-dashboard.png)](../../assets/screenshots/panel-dashboard.png)

## 5. Sprache wählen {#5-choose-the-language}

Die Sprachauswahl finden Sie auf dem Anmeldebildschirm und im Reiter **System**. Das Panel
startet auf Englisch. Ihre Wahl wird im Gerät gespeichert; ein anderes Telefon öffnet das Panel
also in derselben Sprache, auch den Anmeldebildschirm. In der Panel-Seite ist nur Englisch
enthalten; eine andere Sprache lädt das Panel bei der Auswahl vom Gerät, bis dahin zeigt es
Englisch.

Verfügbar: Englisch, Polnisch, Deutsch, Spanisch, Russisch und Ukrainisch. Deutsch, Spanisch,
Russisch und Ukrainisch sind maschinelle Übersetzungen; der Reiter System weist darauf hin,
solange sie auf eine Prüfung warten. Konsolenzeilen, Frame-IDs und das Format `can0 ID#DATA`
werden nie übersetzt.

## Wann das WLAN an ist {#when-wi-fi-is-on}

Lassen Sie die Platine auf dem Schreibtisch am Computer angeschlossen: Dann bleibt das WLAN die
ganze Zeit an. An einem USB-Ladegerät oder einer Powerbank geht das WLAN 60 s nach dem Start aus,
wenn kein CAN-Verkehr da ist und Sie das Panel nicht benutzen.

Im Auto hängt das Gerät an Dauerplus 12 V. Deshalb lässt es das WLAN nicht die ganze Zeit
laufen. Das WLAN ist an:

- solange auf dem CAN-Bus Verkehr ist oder das Radio ACC hat;
- solange ein Computer an USB angeschlossen ist;
- kurze Zeit, nachdem der Bus still geworden ist, wenn das Panel benutzt wird: bis zu 60 s nach
  der letzten Anfrage eines angemeldeten Clients und insgesamt höchstens 10 Minuten, und nur,
  wenn die Versorgungsspannung hoch genug ist (wenn die Platine sie misst);
- in den ersten 60 s nach einem Start bei stillem Bus, damit Sie Zeit zum Verbinden haben.

`wifi off` schaltet das Netzwerk aus, auch über einen Neustart hinweg; `wifi on` schaltet es
wieder ein.

## Optional: Ihr Heimnetz {#optional-your-home-network}

Das Gerät kann sich auch mit Ihrem WLAN zu Hause verbinden. Dann öffnen Sie das Panel aus Ihrem
Heimnetz:

```text
wifi sta <network name>|<password>
```

Das erste `|` trennt den Namen vom Passwort; das Passwort ist Pflicht (8 bis 63 Zeichen oder
64 Hex-Ziffern als fertiger WPA2-Schlüssel). `wifi sta off` vergisst das Netz. Das Gerät versucht
alle 30 s, sich zu verbinden (alle 5 Minuten, solange jemand mit seinem eigenen Netz verbunden
ist), und nie, während der Motor läuft.

Aus dem Heimnetz ist das Panel standardmäßig **nur lesbar**. `wifi lan on` erlaubt Änderungen
auch aus dem Heimnetz; `wifi lan off` sperrt sie wieder. Das Scharfschalten des CAN-Sende-Builds
ist aus dem Heimnetz nie erlaubt, nur aus dem eigenen Netz des Geräts oder über USB.

## Panel ohne Platine ausprobieren {#try-the-panel-without-a-board}

`firmware/tools/mock_device.py` ist eine Nachbildung des Geräts (Mock). Das Skript spielt eine
CAN-Aufzeichnung ab und beantwortet dieselben HTTP-Anfragen wie die Firmware, mit derselben
Panel-Seite. Es braucht nur Python 3.

Führen Sie im Terminal, das im Ordner `firmware` geöffnet ist, aus (unter Linux und macOS tippen
Sie `python3` statt `python`):

```text
python -I tools/mock_device.py test_data/captures/can_20261111_191045.log
```

Das Skript gibt die Adresse und ein zufälliges Mock-Passwort aus:

```text
Device mock: http://127.0.0.1:8080/  (frames in the capture: ..., VIN masked)
mock password: ...
```

1. Öffnen Sie `http://127.0.0.1:8080/`. Erwartet: der Anmeldebildschirm.
2. Melden Sie sich mit dem Mock-Passwort an. Erwartet: die Übersicht mit Verkehr aus der
   Aufzeichnung.
3. Lassen Sie das Terminalfenster offen, solange Sie das Panel benutzen. Beenden Sie den Mock mit
   Strg+C.

Nehmen Sie eine Aufzeichnung, die von Anfang an Verkehr hat, etwa `can_20261111_191045.log`. Eine
Aufzeichnung, die mit einem stillen Bus beginnt, zeigt während dieser Zeit keinen Verkehr.

Nützliche Optionen:

| Option | Wirkung |
|---|---|
| `--password <text>` | eigenes Mock-Passwort (10 bis 63 Zeichen) |
| `--port 8081` | anderer Port |
| `--mode decoder` | verhält sich wie ein Decoder-Build |
| `--lan` | verhält sich, als wären Sie im Heimnetz (nur lesbar, Anmeldesperre) |
| `--lan --lan-admin` | wie nach `wifi lan on` |
| `--speed 2` | doppelt so schnell abspielen |
| `--no-loop` | Aufzeichnung nicht wiederholen |

Der Mock vereinfacht ein paar Dinge (zum Beispiel den Piepser und das Dekodieren der Tasten); die
Liste steht am Anfang des Skripts.

## Weiter {#next}

[Prüftisch-Test](bench-test.md): Prüfen Sie die Platine, bevor sie ins Auto kommt.
