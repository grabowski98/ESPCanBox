# Graba el bus

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Una grabación (una captura) es un archivo de texto con cada trama CAN que vio el dispositivo, en
orden y con su tiempo. La haces mientras haces cosas en el coche: pulsar un botón, abrir una
puerta, meter la marcha atrás. Después buscas los bits que cambiaron en esos momentos
([Decodifica](decode.md)).

Primero necesitas los datos de [Encuentra tu bus](find-your-bus.md): los cables, el tipo de bus y
la velocidad de bits.

> [!CAUTION]
> **Graba con una compilación de solo escucha**
>
> Usa `listen_generic_wifi` o el sniffer aparte de `extras/sniffer/`. Los dos solo escuchan. No
> grabes un coche desconocido con una compilación que pueda transmitir.

## El formato {#the-format}

Un evento por línea, el mismo formato en todo este proyecto:

```
(416.094276) can0 532#0800000000000000      CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117                UART bytes (only the sniffer's recorder variant)
# STEP 3 19:06:41.149 Volume + three times   comment line: a marker
# stat rx=30529 ...                          comment line: statistics
```

Las líneas que empiezan por `#` son comentarios. Algunos comentarios son **marcadores**: líneas
que añades mientras grabas, en el momento en que empiezas una acción. Los marcadores son lo que
hace que una grabación sea fácil de decodificar. Cada herramienta escribe su propio tipo:

| Herramienta | Línea de marcador |
|---|---|
| `record.py` | `# MARKER n (time) computer clock HH:MM:SS: text` |
| `record_serial.py` (sniffer) | `# STEP n HH:MM:SS.mmm text` (Intro), `# MARK d HH:MM:SS.mmm` (una cifra `1` a `9` + Intro) |

Más sobre el formato: [Grabaciones del coche](../../en/testing/captures.md).

## Elige una herramienta {#choose-a-tool}

- Usa `firmware/tools/record.py` si tienes el dispositivo con `listen_generic_wifi` y un portátil
  en su red Wi-Fi. El archivo va a una carpeta `logs` junto a la carpeta del repositorio.
- Usa `extras/sniffer/record_serial.py` si tienes la placa sniffer aparte por USB. El archivo va
  a la carpeta indicada con `--out`.
- Usa `dump` en la consola USB solo para un vistazo rápido: no escribe marcadores.

## Planifica antes los pasos {#plan-the-steps-first}

Escribe los pasos antes de sentarte en el coche. Un buen plan:

- **Empieza con unos segundos sin hacer nada**, para que la grabación muestre el coche en reposo.
- **Una acción por paso.** "Subir volumen" y "bajar volumen" son dos pasos.
- **Repite cada acción tres veces**, con una pausa de 2 a 3 segundos. Un bit que cambia tres veces
  es tu señal. Un bit que cambia todo el rato es un contador o ruido.
- **Pon el marcador justo antes de la acción**, no después.
- **Anota el estado del coche** al principio de cada paso: encendido, motor, puertas, qué muestra la
  radio.

Un plan de ejemplo en inglés es `extras/sniffer/scenarios/example_buttons.txt`. Empieza así:

```
START: ignition off, key out, radio off, wait 10 s
Insert the key, ignition on (engine off), wait until the radio has started, then 10 s
Steering wheel: volume + three times, every 2 s
Steering wheel: volume - three times, every 2 s
```

Las grabaciones del coche de referencia en `firmware/test_data/captures/` se hicieron con planes
como este. Sus líneas de paso están en polaco (`# KROK n` = "paso n") y se dejaron tal como se
grabaron.

## Graba por Wi-Fi con `record.py` {#record-over-wi-fi-with-recordpy}

1. Compila y flashea `listen_generic_wifi` con la velocidad de bits que encontraste: desde la
   carpeta `firmware`, `uvx platformio run -e listen_generic_wifi -t upload`
   ([Flashear el firmware](../quick-start/flash.md)).
2. Conecta el dispositivo al coche ([Conexión CAN](../../en/install-in-car/can-connection.md)).
3. Conéctate con un portátil a la red Wi-Fi del dispositivo ([Primer arranque](../quick-start/first-boot.md)).
   Usa un portátil con batería, o alimentado desde el coche.
4. Desde la carpeta del repositorio, ejecuta:

    ```
    python -I firmware/tools/record.py
    ```

    La dirección por defecto es `10.50.8.1`. Otra dirección se pasa como primer argumento.
5. Escribe la contraseña del dispositivo cuando te la pida. Es la misma que la clave Wi-Fi;
   `wifi showpass` en la consola USB la muestra ([Primer arranque](../quick-start/first-boot.md)).
   No se muestra en pantalla ni se guarda.
6. Espera a `[ok] connected, recording`. El script imprime los nombres de dos archivos: uno con las
   tramas CAN y otro con las líneas de consola del dispositivo.
7. En cada paso: escribe un texto corto (por ejemplo `volume up 3x`), pulsa Intro y después haz la
   acción.
8. Escribe `q` e Intro para parar.

`record.py` solo lee del dispositivo. No envía comandos y no cambia nada. Las tramas `336`, `3B6`
y `2B6` se escriben como `# masked`, porque en el coche de referencia llevan el VIN. En tu coche el
VIN puede ir en otras tramas: encuéntralas antes de compartir nada ([Comparte tu coche](contribute.md)).

> [!WARNING]
> **`record.py` siempre oculta `336`, `3B6` y `2B6`**
>
> En otro coche estos tres ID pueden llevar señales normales, y el script no tiene ninguna
> opción para conservarlos. Si no encuentras una señal, haz una grabación con el sniffer o con
> `dump`: los dos conservan todas las tramas, así que limpia ese archivo antes de compartirlo.

Si el enlace Wi-Fi se corta, el script sigue intentándolo. Las tramas que el dispositivo no pudo
entregar se escriben como `# lost N frames`. El dispositivo guarda unos 9 s de tramas en memoria,
así que los cortes breves no pierden nada.

> [!NOTE]
> **No grabes en el panel del navegador**
>
> La vista CAN del panel web solo graba mientras su pestaña del navegador está visible. Cuando el
> autor pasó a otra pestaña durante una grabación en el panel, se perdieron 26 593 tramas
> (medido (measured): la línea `# zgubiono 26593 ramek`, "se perdieron 26593 tramas" en polaco, en
> `centralka_can_20261117_184411.log`). Usa `record.py`.

## Graba por USB con el sniffer {#record-over-usb-with-the-sniffer}

El sniffer es un pequeño programa aparte para una placa ESP32-S3 con un módulo transceptor CAN y
sin ningún cable de transmisión. La compilación, el cableado y el comando de grabación están en
`extras/sniffer/README.md`. En resumen:

```
uv run --with pyserial python -I extras/sniffer/record_serial.py COM7 --scenario extras/sniffer/scenarios/example_buttons.txt
```

Ejecútalo desde la carpeta del repositorio. Cambia `COM7` por tu puerto:
`uvx platformio device list` lo muestra. El script muestra el siguiente paso del escenario. Pulsa
Intro cuando lo empieces.

## Cómo reciben su tiempo los marcadores {#how-markers-get-their-time}

Una línea de comentario no tiene tiempo propio. La herramienta de decodificación
([Decodifica](decode.md)) da a cada marcador el tiempo de la primera trama CAN que le sigue. Un
marcador de `record.py` lleva el tiempo del dispositivo entre paréntesis, y entonces se usa ese
tiempo.

Esto importa cuando el bus está dormido. En la grabación de referencia `can_20261110_190421.log`
el bus estuvo en silencio durante los dos primeros pasos ("inicio, esperar 10 s" y "poner el encendido"). Los
dos marcadores recibieron el mismo tiempo, el momento en que el bus despertó:

```
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
```

Así que, para "poner el encendido", mira las primeras tramas de la grabación, no un cambio alrededor del
marcador.

## Después de la grabación {#after-the-recording}

- Comprueba los números que imprime el script al final: tramas, marcadores, tramas perdidas.
- Abre el archivo con un editor de texto y lee las líneas de comentario. Los textos de tus
  marcadores se copian en el archivo sin cambios: no escribas en ellos nada privado.
- Conserva el archivo original. Trabaja sobre una copia.
- Antes de compartir una grabación con nadie, quita el VIN y comprueba el archivo
  ([Comparte tu coche](contribute.md)).

Siguiente: [Decodifica tramas](decode.md).
