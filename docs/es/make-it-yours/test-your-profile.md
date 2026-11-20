# Prueba tu perfil

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Antes de acercar tu perfil al coche, ejecútalo en tu PC. Una **prueba en el PC** (host test) pasa
una grabación, trama a trama, por el mismo código de decodificación que funciona en el dispositivo,
e imprime lo que haría el dispositivo: líneas de consola, tramas para la radio y cada cambio de las
salidas de 12 V y del zumbador. Lees esa salida una vez, la contrastas con lo que hiciste en el
coche y la guardas como referencia. A partir de ahí la prueba te avisa cuando un cambio rompe algo.

No se flashea nada ni se abre ningún puerto. No necesitas la placa ni el coche.

Esta página continúa el tutorial de [Escribe un perfil de coche](write-a-profile.md) con el perfil
inventado `mine`. Cada comando se ejecutó en una copia del repositorio. Todos los comandos se
ejecutan desde la carpeta `firmware`, en Git Bash.

> [!TIP]
> **Primero, una prueba en seco**
>
> Antes de poner tus propias tramas en `car_mine`, puedes comprobar la copia y los cambios de
> nombre. Haz los pasos 2 a 4 con los valores de la plantilla aún en `car_mine` y con
> `run_file mine_car test_host/synthetic/example_car.log` en el paso 3. La salida debe terminar
> con `example profile works on every axis` y `### code=0`. Después cambia el perfil a tu coche
> y pon tu propia grabación en la línea `run_file`.

## Qué necesitas {#what-you-need}

- Git Bash en Windows, o un shell en Linux o macOS;
- `uv`: el script de pruebas obtiene un compilador de C++ a través de él (paquete `ziglang`; la
  primera ejecución lo descarga, así que necesita internet una vez), o indica el tuyo con
  `CXX=g++`. Detalles: [Pruebas en el PC](../../en/testing/host-tests.md);
- tu perfil en `firmware/lib/car_<name>/`;
- una grabación de tu coche con marcadores ([Graba el bus](record.md)), limpiada con
  `python -I ../tools/sanitize_capture.py` si la vas a compartir ([Comparte tu coche](contribute.md)).

## Paso 1: pon la grabación en su sitio {#step-1-put-the-recording-in-place}

| Grabación | Dónde |
|---|---|
| una grabación real de tu coche | `firmware/test_data/captures/`, con una fila en su `README.md` (qué es, cómo se grabó, qué prueba la usa) |
| tramas que escribiste a mano para probar un caso | `firmware/test_host/synthetic/`, con un comentario arriba que diga que las tramas son inventadas |

El formato es el habitual, `(seconds) can0 ID#DATA`. Las líneas que no son tramas CAN (comentarios,
líneas UART) la prueba se las salta. Los archivos escritos a mano para pruebas de perfil van a
`test_host/synthetic/`, no a `test_data/synthetic/`: esa carpeta es para `can_diff.py`.

Un archivo escrito a mano es un buen comienzo, porque sabes exactamente qué debe pasar. El que se
usó en el tutorial, `test_host/synthetic/mine_car.log`, empezaba así (el coche del tutorial de
[Escribe un perfil](write-a-profile.md); este archivo no está en el repositorio, así que escribe el
tuyo):

```text
# Invented frames for the "mine" profile (tutorial). Not a real car.
# 2C0 = ignition / reverse, 1F0 = buttons + knob, 350 = rear parking, 2C8 = lights
(0.000000) can0 2C0#00000000
(0.200000) can0 2C0#00010000
(0.400000) can0 2C0#00020000
(0.600000) can0 1F0#01000000
(0.700000) can0 1F0#01000000
```

## Paso 2: copia la prueba de ejemplo {#step-2-copy-the-example-test}

El perfil de plantilla tiene su propia prueba, `firmware/test_host/example_car.cpp`. Cópiala,
desde la carpeta `firmware`:

```bash
cp test_host/example_car.cpp test_host/mine_car.cpp
```

Después cambia cuatro cosas en `mine_car.cpp`:

1. `#include "profile_example.h"` pasa a ser `#include "profile_mine.h"`;
2. cada `kCarExample` pasa a ser `kCarMine` (seis veces, en cuatro líneas; lo más fácil es buscar
   y reemplazar);
3. el texto de uso `example_car <capture.log>` pasa a ser `mine_car <capture.log>` (en el
   comentario de arriba y en la línea `fprintf`);
4. la última comprobación, la línea que empieza por `const bool ok =`. Exige que **cada** salida
   haya hecho algo. Cada parte: `r.accMs` = ACC, `r.illMs` = luces, `r.revMs` = marcha atrás,
   `r.toneMs` = zumbador, `r.uartFrames` = tramas para la radio, `d.haveLastKey()` = al menos un
   botón. Quita las partes que tu perfil no tiene. Un perfil sin sensores de aparcamiento falla en
   `r.toneMs > 0`, y entonces la prueba termina con el código de salida 1. Líneas listas:
    - solo encendido y botones:
      `const bool ok = r.accMs > 0 && r.uartFrames > 0 && d.haveLastKey();`
    - solo encendido: `const bool ok = r.accMs > 0;`

    El texto `example profile works on every axis` puede quedarse: es solo una etiqueta.

## Paso 3: añade la prueba al script de pruebas {#step-3-add-the-test-to-the-test-script}

Abre `firmware/tools/run_host_tests.sh` y busca el bloque que termina con `finish example_car`.
Añade tu prueba justo después:

```bash
# My car profile
# shellcheck disable=SC2086
build mine_car $S38_LIBS profile_mine.cpp
rm -f "$OUT/mine_car.actual.txt"
run_file mine_car test_host/synthetic/mine_car.log
finish mine_car
```

`build` compila tu prueba junto con el decodificador y sus bibliotecas y el archivo de tu perfil.
`$S38_LIBS` es la lista de archivos del decodificador que necesita la prueba; déjala como está. `run_file` la ejecuta sobre el archivo que le das, con una ruta relativa a
`firmware/`. Para una grabación en `test_data/captures/`, da esa ruta.

## Paso 4: primera ejecución, lee la salida, guarda la referencia {#step-4-first-run-read-the-output-store-the-reference}

Ejecuta todas las pruebas en el PC (`cd firmware` solo hace falta si estás en la carpeta del
repositorio):

```bash
cd firmware
bash tools/run_host_tests.sh
```

Resultado esperado: todas las pruebas existentes imprimen `OK`. La tuya detiene el script, porque aún no tiene
referencia:

```text
OK     example_car
FAIL   mine_car: no reference test_host/expected/mine_car.txt
```

La salida está ahora en `firmware/.build_host/mine_car.actual.txt`. **Léela** y compárala con lo
que hiciste en el coche (o escribiste en el archivo). Esta es la prueba de verdad; el archivo
guardado solo la repite más adelante. Un extracto de la ejecución del tutorial:

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

Cómo leerla:

| Línea | Significado |
|---|---|
| `profile mine: ...` | lo que contiene tu perfil: velocidad, número de filas de botones y de mandos, lista de transmisión (siempre 0 en tu propio perfil) |
| `console ...` | la línea que el dispositivo imprimiría en su consola USB |
| `200 radio 2E EA ...` | una trama para la radio a los 200 ms (tiempo según el reloj de la grabación). `EA` = encendido, `20` = botón, `32` = sensores de aparcamiento, `3E` = indicador de obstáculo. Ver [Protocolo XP](../../en/reference/xp-protocol.md) |
| `outputs acc=1 ill=0 rev=0 tone=0 continuous=0` | un cambio de las salidas de 12 V (ACC, iluminación, marcha atrás) o del zumbador |
| `[key] code=14 state=1` | botón con código de radio `14` (volumen +): 1 pulsado, 2 mantenido, 0 soltado |
| últimas líneas | totales en milisegundos, y tu comprobación `ok` |

Cosas que merece la pena comprobar en tu propia salida:

- El ACC se enciende con tu trama de encendido puesto y se apaga 2.5 s después de quitar el
  encendido. En el extracto no se apaga nunca, porque la reproducción termina 1 s después de la
  última trama.
- La salida de iluminación se apaga 1 s después de la última trama de luces (en la ejecución del
  tutorial: trama de luces a los 1100 ms, `ill=0` a los 2101 ms). Si tu coche repite su trama de
  luces, esto no pasa.
- Cada botón da `state=1`, luego `state=2` cada 200 ms mientras se mantiene, y luego `state=0`.
- El tono empieza con la primera zona por debajo de 7 y solo pasa a continuo con el intervalo
  `0x00`.
- Si un botón muestra un código distinto del que esperas: ver [Asigna la radio](radio-mapping.md).
  Las reasignaciones por defecto vienen del perfil de coche; el perfil de ejemplo no tiene ninguna,
  así que la prueba de ejemplo muestra SRC como `code=21 state=1`, enviado sin cambios.

> [!WARNING]
> **Un `### code=1` guardado pasa como OK**
>
> El script solo compara la salida con el archivo guardado. No se detiene ante un
> `### code=1` guardado. Comprueba las últimas líneas (punto 1 de abajo) antes de copiar, o la
> prueba pasará aunque haya fallado.

Cuando la salida sea correcta, guárdala como referencia:

1. Comprueba las dos últimas líneas de `.build_host/mine_car.actual.txt`. Deben ser
   `example profile works on every axis` y `### code=0`.
2. Si ves `FAIL: an axis of the example profile did nothing` y `### code=1`, corrige la línea
   `const bool ok =` (paso 2, punto 4) y vuelve a ejecutar el script.
3. Copia el archivo y vuelve a ejecutar el script:

```bash
cp .build_host/mine_car.actual.txt test_host/expected/mine_car.txt
bash tools/run_host_tests.sh
```

Resultado esperado: `OK     mine_car` y `All tests match the references.`

> [!WARNING]
> **No uses `--record-golden` para esto**
>
> `bash tools/run_host_tests.sh --record-golden` vuelve a grabar las referencias de **todas** las
> pruebas, incluidos los archivos golden del coche de referencia. Úsalo solo cuando hayas
> cambiado el comportamiento a propósito y hayas revisado cada diferencia. Para una sola prueba
> nueva, copia su archivo como se muestra arriba.

## Un ciclo más rápido para una sola prueba {#a-faster-loop-for-one-test}

En la ejecución del tutorial, el script completo tardó de 30 a 50 segundos. Mientras trabajas en
el perfil, puedes compilar y ejecutar solo tu prueba. Ejecuta antes una vez el script completo:
descarga el compilador en la caché de `uv`, y el comando de abajo usa esa caché (`--offline`).
Desde la carpeta `firmware`, en Git Bash:

```bash
LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp profile_mine.cpp vehicle_signals.cpp"
SRCS=""; for f in $LIBS; do SRCS="$SRCS $(ls lib/*/$f)"; done
INC=""; for d in lib/*/; do INC="$INC -I ${d%/}"; done
mkdir -p .build_host
uvx --offline --from ziglang python -I -m ziglang c++ -std=c++17 -O1 -Wall $INC -I include \
    test_host/mine_car.cpp $SRCS -o .build_host/mine_car.exe < /dev/null
./.build_host/mine_car.exe test_host/synthetic/mine_car.log
```

La lista de archivos es la misma que `$S38_LIBS` del script de pruebas, más tu perfil y
`vehicle_signals.cpp`. Si la lista del script cambia, cópiala de ahí. Ejecuta el script completo
antes de compartir nada.

## Ejecuta también la comprobación de capas {#also-run-the-layering-check}

```bash
python -I tools/check_layering.py
```

Comprueba que tu perfil no incluye nada de un protocolo de radio, de `src/` ni del framework
Arduino, y que ningún nombre de cabecera existe dos veces. En la ejecución del tutorial listó
`car_mine` entre las bibliotecas y terminó con `Layering OK.`

## Lo que esta prueba no cubre {#what-this-test-does-not-cover}

- El código de `firmware/src/` (controlador CAN, UART, salidas de 12 V, sonido) no se ejecuta en el
  PC. La primera comprobación del hardware real es la [prueba en banco](../quick-start/bench-test.md).
- La temporización en el bus real: la reproducción avanza en pasos de 1 ms y no pierde tramas.
- Si tu decodificación es correcta para el coche. La prueba solo puede mostrar lo que tu perfil
  hace con las tramas. Por eso lees la salida frente a tus marcadores antes de guardarla.

Siguiente: [Asigna la radio](radio-mapping.md).
