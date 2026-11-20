# Glossary for translators

Terms used in the web panel (`firmware/web/i18n/*.json`) and their preferred translations.
Use them so that one thing has one name in the whole panel. The English column is the source; the
other columns are suggestions: a native speaker may propose a better word in a pull request
(change the table and the dictionary together).

Rules:

- **Device** = this project's board ("the device"). Never translate it as the radio or the car.
- **Box** = the aftermarket CAN interface box between the car and the radio (sold with the radio).
- Keep as they are: `CAN`, `UART`, `ACC`, `VIN`, `BSI`, `TEC`, `REC`, `Wi-Fi`, `HTTP`, `USB`,
  `ESP32-S3`, frame identifiers (`0E1`, `15B`), console commands (`wifi showpass`, `cantx reset`,
  `vset on NN`) and button names printed on the car (`SRC`, `LIST`, `NAV`, `MEDIA`, `TEL`, `SETUP`).
- Prefer short words: the panel is read on a phone and in the radio's browser (375 px wide).

## Car

| English | Polski | Deutsch | Español | Русский | Українська |
|---|---|---|---|---|---|
| ignition | zapłon | Zündung | encendido | зажигание | запалювання |
| engine running | silnik pracuje | Motor läuft | motor en marcha | двигатель работает | двигун працює |
| ACC (accessory power) | ACC (zasilanie akcesoriów) | ACC (Zubehörstrom) | ACC (alimentación de accesorios) | ACC (питание аксессуаров) | ACC (живлення аксесуарів) |
| reverse gear | bieg wsteczny | Rückwärtsgang | marcha atrás | задний ход (задняя передача) | задня передача (задній хід) |
| lights | światła | Licht | luces | фары | фари |
| illumination (dashboard lighting) | podświetlenie | Beleuchtung | iluminación | подсветка | підсвічування |
| parking sensors | czujniki parkowania | Parksensoren | sensores de aparcamiento | парктроники | парктроніки |
| sensor zone | strefa czujnika | Sensorzone | zona del sensor | зона датчика | зона датчика |
| beep | piknięcie | Piepton | pitido | звуковой сигнал | звуковий сигнал |
| beep interval | odstęp piknięć | Pieptonabstand | intervalo de pitidos | интервал сигналов | інтервал сигналів |
| continuous tone | ton ciągły | Dauerton | tono continuo | непрерывный сигнал | безперервний сигнал |
| obstacle | przeszkoda | Hindernis | obstáculo | препятствие | перешкода |
| front / rear | przód / tył | vorne / hinten | delantero / trasero | спереди / сзади | спереду / ззаду |
| driver's door | drzwi kierowcy | Fahrertür | puerta del conductor | дверь водителя | двері водія |
| passenger's door | drzwi pasażera | Beifahrertür | puerta del acompañante | дверь пассажира | двері пасажира |
| boot (trunk) | bagażnik | Kofferraum | maletero | багажник | багажник |
| outside temperature | temperatura zewnętrzna | Außentemperatur | temperatura exterior | наружная температура | зовнішня температура |
| steering wheel buttons | przyciski przy kierownicy | Lenkradtasten | botones del volante | кнопки на руле | кнопки на кермі |
| steering-wheel thumbwheel | pokrętło kierownicy | Lenkrad-Daumenrad | rueda del volante | колёсико на руле | коліщатко на кермі |
| trip computer | komputer pokładowy | Bordcomputer | ordenador de a bordo | бортовой компьютер | бортовий комп'ютер |
| stalk (wiper / light lever) | dźwignia | Lenkstockhebel | palanca | подрулевой переключатель | підрульовий перемикач |
| climate control | klimatyzacja | Klimaanlage | climatizador | климат-контроль | клімат-контроль |
| rear window heating | ogrzewanie tylnej szyby | Heckscheibenheizung | luneta térmica | обогрев заднего стекла | обігрів заднього скла |
| vehicle settings | ustawienia pojazdu | Fahrzeugeinstellungen | ajustes del vehículo | настройки автомобиля | налаштування автомобіля |
| parking assistant | asystent parkowania | Einparkhilfe | asistente de aparcamiento | помощь при парковке | допомога при паркуванні |
| body control unit (BSI) | BSI (moduł nadwozia) | BSI (Bordnetzsteuergerät) | BSI (unidad de control de carrocería) | BSI (блок управления кузовом) | BSI (блок керування кузовом) |
| economy mode | tryb ekonomiczny | Energiesparmodus | modo económico | экономичный режим | економний режим |
| while parked | na postoju | im Stand | con el coche parado | на стоянке | на стоянці |

## Radio

| English | Polski | Deutsch | Español | Русский | Українська |
|---|---|---|---|---|---|
| radio (head unit) | radio | Radio (Autoradio) | radio (unidad principal) | магнитола | магнітола |
| source | źródło | Quelle | fuente | источник | джерело |
| volume | głośność | Lautstärke | volumen | громкость | гучність |
| muted | wyciszone | stumm | silenciado | без звука | без звуку |
| track | utwór | Titel | pista | трек | трек |
| key / button | przycisk | Taste | botón | кнопка | кнопка |
| press / release / hold | wciśnij / puść / przytrzymaj | drücken / loslassen / halten | pulsar / soltar / mantener | нажать / отпустить / удерживать | натиснути / відпустити / утримувати |
| key remapping | zamiana przycisków | Tastenbelegung | reasignación de botones | переназначение кнопок | перепризначення кнопок |
| built-in (default) | wbudowane | integriert (Standard) | integrado (predeterminado) | встроенные | вбудовані |
| hex code | kod szesnastkowy | Hex-Code | código hexadecimal | шестнадцатеричный код | шістнадцятковий код |
| decoder mode | tryb dekoder | Decoder-Modus | modo decodificador | режим декодера | режим декодера |
| beside the box (the console prints "alongside the box"; same mode) | tryb obok (skrzynki) | Parallelmodus (neben der Box) | modo paralelo (junto a la caja) | параллельный режим (рядом с адаптером) | паралельний режим (поруч з адаптером) |
| CAN box | skrzynka CAN | CAN-Box | caja CAN | CAN-адаптер | CAN-адаптер |

## CAN bus and electronics

| English | Polski | Deutsch | Español | Русский | Українська |
|---|---|---|---|---|---|
| CAN bus | magistrala CAN | CAN-Bus | bus CAN | шина CAN | шина CAN |
| frame | ramka | Frame (Botschaft) | trama | кадр | кадр |
| identifier (ID) | identyfikator (ID) | Kennung (ID) | identificador (ID) | идентификатор (ID) | ідентифікатор (ID) |
| listen only | tylko nasłuch | nur Mithören | solo escucha | только прослушивание | лише прослуховування |
| transmit / transmission | nadawać / nadawanie | senden / Senden | transmitir / transmisión | передавать / передача | передавати / передача |
| arm / disarm (transmission) | uzbroić / rozbroić | scharf schalten / entschärfen | armar / desarmar | разрешить / запретить (передачу) | активувати / деактивувати |
| transmit gate | brama nadawania | Sendesperre (Gate) | compuerta de transmisión | шлюз передачи | шлюз передачі |
| acknowledged (frame) | potwierdzona | bestätigt | confirmada | подтверждён | підтверджено |
| bus-off | bus-off | Bus-Off | bus-off | bus-off | bus-off |
| bus error | błąd magistrali | Busfehler | error de bus | ошибка шины | помилка шини |
| controller (CAN) | kontroler | Controller | controlador | контроллер | контролер |
| transceiver | transceiver | Transceiver | transceptor | трансивер | трансивер |
| checksum | suma kontrolna | Prüfsumme | suma de verificación | контрольная сумма | контрольна сума |
| queue | kolejka | Warteschlange | cola | очередь | черга |
| lost (frames) | zgubione | verloren | perdidas | потеряно | втрачено |
| 12 V supply | zasilanie 12 V | 12-V-Versorgung | alimentación de 12 V | питание 12 В | живлення 12 V |
| 12 V output | wyjście 12 V | 12-V-Ausgang | salida de 12 V | выход 12 В | вихід 12 V |
| diode | dioda | Diode | diodo | диод | діод |
| jumper | zworka | Jumper (Brücke) | puente | перемычка | перемичка |
| power supply glitch | zakłócenie zasilania | Versorgungsstörung | fallo de alimentación | сбой питания | збій живлення |
| supply voltage drop (brownout) | spadek napięcia | Unterspannung | caída de tensión | просадка напряжения | просідання напруги |
| watchdog | strażnik (watchdog) | Watchdog | watchdog | сторожевой таймер | сторожовий таймер |
| chip temperature | temperatura układu | Chiptemperatur | temperatura del chip | температура чипа | температура чипа |

## Firmware and network

| English | Polski | Deutsch | Español | Русский | Українська |
|---|---|---|---|---|---|
| firmware | program | Firmware | firmware | прошивка | прошивка |
| flash (write firmware) | wgrać | flashen | flashear (grabar) | прошить | прошити |
| update | aktualizacja | Aktualisierung | actualización | обновление | оновлення |
| roll back | wrócić (cofnąć) | zurücksetzen | revertir | откатить | відкотити |
| slot (firmware slot) | miejsce | Slot | ranura | слот | слот |
| image hash | skrót obrazu | Image-Hash | hash de la imagen | хеш образа | хеш образу |
| restart | restart | Neustart | reinicio | перезапуск | перезапуск |
| uptime | czas pracy | Laufzeit | tiempo en marcha | время работы | час роботи |
| main loop | pętla główna | Hauptschleife | bucle principal | главный цикл | головний цикл |
| free memory (heap) | wolna pamięć | freier Speicher | memoria libre | свободная память | вільна пам'ять |
| stack headroom | zapas stosu | Stack-Reserve | margen de pila | запас стека | запас стека |
| console | konsola | Konsole | consola | консоль | консоль |
| command | polecenie | Befehl | comando | команда | команда |
| recording (CAN log) | nagranie | Aufzeichnung | grabación | запись | запис |
| device network (own Wi-Fi) | sieć urządzenia | Gerätenetz | red del dispositivo | сеть устройства | мережа пристрою |
| home network | sieć domowa | Heimnetz | red doméstica | домашняя сеть | домашня мережа |
| password | hasło | Passwort | contraseña | пароль | пароль |
| log in / log out | zaloguj / wyloguj | anmelden / abmelden | iniciar / cerrar sesión | войти / выйти | увійти / вийти |
| lockout (after wrong passwords) | blokada | Sperre | bloqueo | блокировка | блокування |
| signal (RSSI) | sygnał | Signal | señal | сигнал | сигнал |

## Documentation

Words of the documentation pages that the panel does not use. The statuses are explained in
`docs/STYLE.md`.

| English | Polski | Deutsch | Español | Русский | Українська |
|---|---|---|---|---|---|
| measured (status) | zmierzone | gemessen | medido | измерено | виміряно |
| unverified (status) | niezweryfikowane | unbestätigt | sin verificar | не проверено | не перевірено |
| bench (test without the car) | stół (test na stole) | Prüftisch | banco de pruebas | стенд | стенд |
| transmit allow list | lista dozwolonych ramek | Sende-Freigabeliste | lista de tramas permitidas | список разрешённых кадров | список дозволених кадрів |
| fault latch (transmit) | blokada po błędzie | Fehlersperre | bloqueo por fallo | блокировка после ошибки | блокування після помилки |
