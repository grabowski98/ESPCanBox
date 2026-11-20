# Escribe un perfil de coche

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Un **perfil de coche** le dice al firmware cómo leer un coche: la velocidad del bus, qué bits son
los botones del volante, dónde están el encendido, la marcha atrás, las luces y los sensores de
aparcamiento. Es una estructura constante en C++ (`vehicle::CarProfile`, definida en
`firmware/lib/core/car_profile.h`), compilada dentro del programa. Eliges el perfil al compilar el
firmware; no puede cambiar mientras el dispositivo funciona.

Esta página te lleva desde tus tramas decodificadas hasta un perfil que compila. La página
siguiente, [Prueba tu perfil](test-your-profile.md), lo comprueba con una grabación en tu PC.

Cada paso de esta página se probó en una copia del repositorio con un coche inventado. Donde algo
salió mal, la página lo dice y muestra el mensaje que verás.

> [!CAUTION]
> **Tu propio perfil es de solo escucha en el bus del coche**
>
> Un perfil compilado con `-DCAR_PROFILE=CUSTOM` nunca puede transmitir en el bus CAN del
> coche. El entorno con transmisión `decoder_tx_wifi` se niega a compilar con él y se detiene
> con `CAN transmit is not allowed with CAR_PROFILE=CUSTOM (listen-only; add a named profile in src/config.h)`.
> No intentes saltarte esto. La transmisión para un coche nuevo necesita tramas medidas,
> comprobaciones del contenido escritas en código y pruebas: ver
> [Transmitir en el bus CAN](../../en/safety/transmit.md).

## Qué cubre un perfil {#what-a-profile-covers}

Tenlo claro antes de empezar. Un perfil lleva solo una parte de lo que hace el dispositivo en el
coche de referencia (Peugeot 508 SW 2015, perfil `psa_aee2010`).

| Función | A través de tu perfil | Notas |
|---|---|---|
| Velocidad del bus | sí | `canBitrate` |
| Botones del volante y del panel, mandos giratorios | sí | tablas `keys` y `knobs` |
| Estado del encendido y del motor, marcha atrás | sí | función `decodeBody`; controla el ACC y la salida de marcha atrás |
| Luces encendidas | sí | función `decodeLights`; controla la salida de iluminación |
| Sensores de aparcamiento y el pitido por el altavoz del dispositivo | sí | función `decodeParking` |
| Lista de tramas permitidas | **no**: el campo queda vacío (`nullptr, 0`) | un perfil `CUSTOM` no puede transmitir (ver arriba) |
| Tramas que llevan el VIN | se listan, solo informativo | `privacyIds`; el firmware no lo usa |
| Puertas, maletero y capó | sí | función `decodeDoors` -> `vehicle::DoorState` |
| Ordenador de a bordo | sí, con un límite | función `decodeTrip` -> `vehicle::TripData`. Los valores mantienen la codificación del coche de referencia (su significado no está verificado): rellénalos solo si tu coche usa la misma |
| Temperatura exterior, modo económico | sí | funciones `decodeOutsideTemp`, `decodeEcoMode` |
| Estado del climatizador | sí | función `decodeClimate` -> `vehicle::ClimateState` |
| Ángulo del volante | sí, con un límite | función `decodeSteering`; escala no verificada en el coche de referencia |
| Ajustes del vehículo (lo que muestra la radio) | sí | función `decodeSettings` -> `vehicle::VehicleSettings` (elementos con identificadores neutros) |
| Cambiar un ajuste del vehículo desde la radio | **no** | solo el perfil de referencia `psa_aee2010`, detrás de la compuerta de transmisión |

Los datos del coche para la pantalla de la radio (de las puertas a los ajustes del vehículo) son
opcionales: deja un puntero en `nullptr` y la radio nunca recibirá esos datos. Cada función rellena
un tipo neutro de `firmware/lib/core/vehicle_signals.h`; el protocolo de radio lo convierte en su
propia trama. La sección «Datos del coche para la pantalla de la radio» del paso 3 los enumera.

Con el protocolo de radio XP, el perfil de otro coche le da por tanto a la radio: el estado de
alimentación (ACC), las pulsaciones de botones, la imagen de los sensores de aparcamiento, el
indicador de obstáculo y cada función de datos del coche que el perfil rellene. Cambiar los ajustes
del vehículo desde la radio sigue siendo cosa del coche de referencia.

## Antes de empezar {#before-you-start}

Necesitas:

- la velocidad de bits, los cables CAN y el tipo de bus de tu coche ([Encuentra tu bus](find-your-bus.md));
- una grabación con marcadores ([Graba el bus](record.md));
- las tramas y los bits que decodificaste ([Decodifica tramas](decode.md)). Apunta cada uno con su
  estado, **medido** (measured) o **sin verificar** (unverified), en una copia de
  `vehicles/_template/README.md`;
- las herramientas de compilación de [Flashear el firmware](../quick-start/flash.md): `uv` y, en
  Windows, Git Bash.

No necesitas todas las señales. Un perfil con solo el encendido ya es útil: puede conmutar el ACC
de la radio. Deja fuera todo lo que no hayas medido.

## Paso 1: copia la plantilla {#step-1-copy-the-template}

La plantilla `firmware/lib/car_example/` describe un coche inventado con comentarios en cada campo.
Solo la compilan las pruebas en el PC, nunca entra en el firmware.

> [!NOTE]
> **Dos coches inventados**
>
> La plantilla usa las tramas `100`, `200`, `300` y `400`. Los ejemplos de esta página y de
> [Prueba tu perfil](test-your-profile.md) vienen de un **segundo** coche inventado, el "coche
> del tutorial" (tramas `2C0`, `1F0`, `350`, `2C8`), creado cuando se probaron estas páginas. Su
> grabación no está en el repositorio; los fragmentos solo muestran cómo quedan tus propios
> valores. En tu copia sustituyes los valores de la plantilla por las tramas de tu coche.

Elige un nombre corto para tu perfil, en minúsculas, por ejemplo `mine`. Después abre Git Bash en
la carpeta `firmware`: en el Explorador, abre la carpeta, haz clic con el botón derecho y elige
"Open Git Bash here" (en Windows 11, dentro de "Mostrar más opciones"). Ejecuta:

```bash
cp -r lib/car_example lib/car_mine
cd lib/car_mine
mv profile_example.h profile_mine.h
mv profile_example.cpp profile_mine.cpp
cd ../..
```

Los nombres de archivo deben ser únicos en todo `firmware/lib/`: todas las bibliotecas comparten
una misma ruta de inclusión, y `tools/check_layering.py` informa de un nombre de cabecera que
exista dos veces.

## Paso 2: renombra dentro de los archivos {#step-2-rename-inside-the-files}

Abre los tres archivos con un editor de texto y renombra:

| Dónde | De | A |
|---|---|---|
| `profile_mine.cpp`, primer `#include` | `"profile_example.h"` | `"profile_mine.h"` |
| los dos archivos | `kCarExample` | `kCarMine` |
| los dos archivos | `namespace example` y `example::` | `namespace mine` y `mine::` |
| `profile_mine.cpp`, primer campo del perfil | `"example"` | `"mine"` |
| `library.json`, campo `name` | `"car_example"` | `"car_mine"` |
| `library.json`, campo `description` | el texto de la plantilla | una línea sobre tu coche |

> [!WARNING]
> **No olvides `library.json`**
>
> PlatformIO conoce la biblioteca por el `name` de `library.json`, no por el nombre de la
> carpeta. Los entornos `listen` ignoran una biblioteca llamada `car_example` (`lib_ignore` en
> `firmware/platformio.ini`). Con el nombre antiguo la compilación falla con
> `Warning! Could not find the package with 'car_mine' requirements` y después
> `fatal error: profile_mine.h: No such file or directory`.

## Paso 3: rellena el perfil {#step-3-fill-in-the-profile}

Trabaja en este orden:

1. En `profile_mine.h`, pon los cuatro ID de trama `ID_BODY`, `ID_KEYS`, `ID_PARKING` e
   `ID_LIGHTS` con tus tramas. Un ID cuya función pongas a `nullptr` (punto 4) no se lee.
2. En `profile_mine.cpp`, reescribe las funciones `decodeBody`, `decodeParking` y
   `decodeLights` (secciones de abajo).
3. En el mismo archivo, rellena las tablas `KEYS` y `KNOBS` (secciones de abajo).
4. Al final del archivo, rellena la constante del perfil (tabla de abajo). Sus campos no tienen
   nombre en el código, solo cuenta su orden, así que conserva el comentario junto a cada línea.

¿Quieres comprobar la copia y los cambios de nombre antes de cambiar ningún valor? Haz primero la
prueba en seco de [Prueba tu perfil](test-your-profile.md).

Los campos de la constante del perfil, en orden:

| Campo | Qué poner ahí |
|---|---|
| `id` | nombre ASCII corto, se muestra en la consola (`# profile: car=mine radio=xp`) y en el panel |
| `canBitrate` | la velocidad de bits medida en bit/s |
| `physicalLayer` | `vehicle::PhysicalLayer::UNKNOWN`, `HIGH_SPEED` o `LOW_SPEED_FT`. Solo informativo: el transceptor lo decide la placa. Deja `UNKNOWN` hasta que lo hayas medido |
| `keys`, `keyCount` | la tabla de botones (abajo); `nullptr, 0` si no hay ninguno |
| `knobs`, `knobCount` | la tabla de mandos (abajo); `nullptr, 0` si no hay ninguno |
| `txAllow`, `txAllowCount` | `nullptr, 0`. Un perfil `CUSTOM` nunca transmite |
| `privacyIds`, `privacyCount` | los ID de las tramas que llevan el VIN en tu coche, si los conoces; si no, `nullptr, 0` |
| `decodeParking` | tu función de sensores de aparcamiento, o `nullptr` |
| `decodeBody` | tu función de encendido y marcha atrás, o `nullptr` |
| `decodeLights` | tu función de luces, o `nullptr` |
| `powerOutputs` | `true` solo cuando `decodeBody` lee el encendido de forma fiable; `false` mantiene apagadas todas las salidas de 12 V |
| `keyRemapDefaults`, `keyRemapDefaultCount` | `nullptr, 0`: cada botón llega a la radio con su propio código. Filas `{from, to}` (códigos XP) solo para las reasignaciones que quieras por defecto; ver [Asigna la radio](radio-mapping.md#default-remappings-come-from-the-car-profile) |

Un `nullptr` siempre está permitido: esa señal no se produce nunca.

### En el código, los números de byte empiezan en 0 {#byte-numbers-in-code-start-at-0}

La documentación y las páginas de vehículos cuentan los bytes CAN desde **1** (byte 1 = el primer
byte de datos). En C++ el primer byte es `f.data[0]`, y el campo `byteIndex` de las tablas es ese
mismo índice. Así, un botón que apuntaste como "byte 1, bit 2" es `byteIndex 0, mask 0x04`.
Compruébalo dos veces: es el error más fácil de cometer.

### Velocidad de bits {#bit-rate}

`src/can_bus.cpp` conoce estas velocidades: 50000, 100000, 125000, 250000, 500000 y 1000000 bit/s.
Cualquier otro valor compila, pero el controlador CAN no arranca: la consola muestra `# can=FAIL`,
el LED de estado se pone rojo y las salidas de 12 V se quedan apagadas. Es el estado seguro, no un
cuelgue.

### Encendido y marcha atrás: `decodeBody` {#ignition-and-reverse-decodebody}

La función recibe cada trama recibida. Devuelve `false` para cualquier trama que no sea la suya, o
que sea demasiado corta, y `true` después de rellenar `out`. Comprueba siempre primero el ID y la
longitud; nunca leas más allá de la longitud.

El firmware no usa directamente el byte de encendido de tu coche. Lo conviertes al tipo
`vehicle::Ignition`, que no depende del coche (`firmware/lib/core/vehicle_signals.h`):

| Valor | Significado |
|---|---|
| `OFF` | encendido quitado, o aún no se ha decodificado nada |
| `ON` | encendido puesto, motor parado |
| `ENGINE` | motor en marcha |
| `TRANSITION` | cualquier otra cosa que no sea apagado: arrancando, apagándose, un valor que no conoces |

Guarda el valor en bruto de tu coche en `ignitionRaw`. Solo se muestra en la consola y en el panel.

Ejemplo del coche inventado de este tutorial (trama `2C0`: byte 2 = encendido, byte 4 bit 6 =
marcha atrás):

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

Cómo lo usa la lógica genérica (`firmware/lib/core/power_policy.h`):

- el ACC está encendido en todos los estados salvo `OFF`, así que el arranque no apaga la radio;
- el ACC se apaga 2.5 s después de que el encendido pase a `OFF`;
- la trama tiene que seguir llegando. Si falta durante más de 1 s, el encendido cuenta como
  apagado ([cómo medir el periodo](decode.md#how-often-a-frame-comes)). En el coche de referencia la trama de encendido `0F6` llega cada 500 ms (medido
  (measured): grabación `can_20261111_191045.log`). Si tu trama de encendido es más lenta que una
  vez por segundo, las salidas de 12 V se caerán;
- la salida de marcha atrás solo está encendida con el encendido puesto y una trama reciente.

### Luces: `decodeLights` {#lights-decodelights}

El mismo patrón: comprueba el ID y la longitud, pon `lightsOn` y devuelve `true`. La salida de
iluminación solo está encendida cuando el encendido está puesto **y** ha llegado una trama de luces
en el último segundo ([cómo medir el periodo](decode.md#how-often-a-frame-comes)). Un coche que envía el estado de las luces solo cuando cambia perderá la
salida de iluminación al cabo de un segundo.

### Sensores de aparcamiento: `decodeParking` {#parking-sensors-decodeparking}

Rellena un `vehicle::ParkingState` con el mismo significado que en el coche de referencia:

| Campo | Significado |
|---|---|
| `rearStatus`, `frontStatus` | `4` (`PARK_ACTIVE`) = funcionando. El zumbador solo usa un lado con estado 4. Otros valores: 1 avería, 2 apagado por el conductor, 5 en espera, 6 fuera de servicio |
| `rearLeft` ... `frontRight` | zonas 0 a 7: 0 = muy cerca, 7 = lejos o sin obstáculo. Limita tus valores a 0..7 |
| `soundRequest`, `soundFront` | el coche pide sonido; desde delante (`true`) o desde detrás |
| `intervalRaw` | `0x00` = tono continuo, `0x3F` = sin pitidos, valores intermedios = cada vez más cerca |

Empieza con `out = vehicle::ParkingState();` para que cada campo que no rellenes tenga su valor de
"nada" (zonas 7, estado 0).

Qué hace el dispositivo con ello (`firmware/lib/core/parking_beeper.h`):

- pita desde la primera zona por debajo de 7 en un lado activo, aunque el coche no pida sonido; el
  ritmo viene de la zona más cercana;
- toca el tono continuo **solo** cuando `soundRequest` está activo e `intervalRaw` vale `0x00`.
  Si tu coche no indica un intervalo, calcúlalo a partir de la zona, como hace la plantilla;
- se calla cuando no ha llegado ninguna trama de aparcamiento durante 400 ms. En el coche de
  referencia la trama llega cada 100 ms.

### Datos del coche para la pantalla de la radio {#car-data-for-the-radio-screen}

Siete funciones opcionales, todas con las mismas reglas: primero comprueba el ID y la longitud,
rellena el tipo neutro, pon su indicador `valid` y devuelve `true` (o los bits de página o de
cambio). `firmware/lib/car_example/profile_example.cpp` muestra `decodeDoors` y `decodeTrip`.

- `decodeDoors` -> `DoorState`: `open` contiene los bits `DOOR_DRIVER`, `DOOR_PASSENGER`,
  `DOOR_REAR_LEFT`, `DOOR_REAR_RIGHT`, `DOOR_BOOT`, `DOOR_BONNET`; `anyOpen` = el aviso de puerta
  abierta del coche. Devuelve `true` solo cuando la trama dice algo sobre las puertas.
- `decodeTrip` -> `TripData`: devuelve `TRIP_NOW`, `TRIP_1` o `TRIP_2` según la página que la
  trama actualizó.
- `decodeOutsideTemp` -> `OutsideTemp`, en medios grados Celsius.
- `decodeEcoMode` -> `EcoMode`: el coche está a punto de cortar los accesorios.
- `decodeClimate` -> `ClimateState`: temperaturas en medios grados (0 = sin valor), ventilador
  0..15, indicadores `AIR_*`.
- `decodeSteering` -> `SteeringAngle`: valor en bruto con la codificación del coche de referencia.
- `decodeSettings` -> `VehicleSettings`: para cada elemento `at(SettingId::...)` el `value`,
  `known` (tu última trama lo indicó) y `present` / `presentKnown` (el coche tiene el elemento).
  Devuelve `SETTINGS_VALUES` y / o `SETTINGS_PRESENT`.

Las funciones con un argumento `io` (ordenador de a bordo, climatizador, puertas, ajustes) reciben
el estado que el decodificador guarda entre tramas. Cambia solo lo que dice tu trama, para que
varias tramas puedan formar un mismo estado.

### Botones: la tabla `keys` {#buttons-the-keys-table}

Una fila es un botón que está "pulsado mientras haya un patrón de bits":

```cpp
{canId, byteIndex, mask, value, KeyId::...}
```

El botón cuenta como pulsado mientras `(data[byteIndex] & mask) == value` en la trama `canId`.
Reglas del código (`firmware/lib/core/car_profile.h`, `firmware/lib/app/xp_keys.h`):

- el orden de las filas es el orden de comparación y el orden de los eventos para la radio;
- se usan como máximo 32 filas;
- una trama más corta que `byteIndex + 1` se salta para esa fila;
- una fila cuyo patrón está presente cuando no se pulsa nada informa de un botón mantenido todo el
  rato. Con `value` = `0x00` esto pasa fácilmente; da a cada fila un patrón que solo produzca una
  pulsación;
- pulsación = un evento inmediato, luego cada 200 ms "mantenido" mientras siga el patrón, y luego
  "soltado".

Dos formas en que un coche puede codificar los botones, ambas vistas en la práctica:

```cpp
// one bit per button (template car: byte 1 bit 0 = volume +, bit 1 = volume -)
{0x200, 0, 0x01, 0x01, KeyId::VOLUME_UP},
{0x200, 0, 0x02, 0x02, KeyId::VOLUME_DOWN},

// a button number in a group of bits (tutorial car: byte 1 bits 3-0 = 1, 2, 3, ...)
{0x1F0, 0, 0x0F, 0x01, KeyId::VOLUME_UP},
{0x1F0, 0, 0x0F, 0x02, KeyId::VOLUME_DOWN},
```

El perfil de referencia también usa la segunda forma: en la trama `21F` byte 1, la máscara `0C` da
volumen + (`08`), volumen - (`04`) y los dos a la vez, silencio (`0C`) (medido: grabaciones
`can_20261111_190817.log` y `can_20261111_191045.log`, CAN y la línea de la caja CAN hacia la radio
con un mismo reloj).

Los nombres (`KeyId`) son botones lógicos. Qué código recibe la radio para cada nombre es tarea
del protocolo de radio: ver [Asigna la radio](radio-mapping.md).

### Mandos giratorios: la tabla `knobs` {#rotary-knobs-the-knobs-table}

Un mando es un contador de 8 bits en un byte de una trama:

```cpp
{canId, byteIndex, onIncrease, onDecrease}
```

Un paso del contador de +1 es un paso `onIncrease`, -1 es un paso `onDecrease`. Se cuentan como
máximo 4 pasos por trama, y se usan como máximo 4 filas de mandos. La primera trama solo fija el
valor inicial. Si tu mando cuenta hacia abajo cuando lo giras "hacia delante", intercambia los dos
nombres, como hace el perfil de referencia con la trama `21F`.

## Paso 4: compílalo {#step-4-build-it}

Tu perfil se elige con tres opciones de compilación, sin editar nada en `src/`. El archivo
`firmware/platformio_override.sample.ini` ya tiene un entorno para un perfil llamado `mine`:

1. Copia `firmware/platformio_override.sample.ini` a `firmware/platformio_override.ini`. Git no
   hace seguimiento del archivo nuevo; `platformio.ini` lo carga automáticamente.
2. Si tu perfil no se llama `mine`, cambia la sección `listen_mycar` para que quede así, con tu
   nombre en lugar de `<name>` y `<Name>`:

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

    `lib_deps` es necesario: PlatformIO no sigue un `#include` que llega a través de una macro.
    La sección `listen_mine` del mismo archivo es para tu propia placa, no para tu coche; déjala.
3. Compila, desde la carpeta `firmware`:

    ```bash
    uvx platformio run -e listen_mycar
    ```

    Resultado esperado: la compilación termina con `SUCCESS`, y la lista de dependencias cerca del
    principio muestra `|-- car_mine @ 1.0.0`. Otros mensajes: mira
    [Errores frecuentes](#common-mistakes).
4. Flashéalo y abre la consola:

    ```bash
    uvx platformio run -e listen_mycar -t upload
    uvx platformio device monitor
    ```

    Resultado esperado: `# profile: car=mine radio=xp` al arrancar. Puerto y modo bootloader:
    [Flashear el firmware](../quick-start/flash.md).

`listen_mycar` es una compilación de solo escucha "junto a la caja" (modo paralelo): decodifica tu
coche, hace sonar los sensores de aparcamiento por el altavoz del dispositivo e imprime líneas de
consola, pero no envía nada a la radio y mantiene apagadas las salidas de 12 V. En la consola,
escribe `keys` para imprimir cada evento de botón, `dump` para imprimir cada trama CAN.

### Modo decodificador con tu perfil {#decoder-mode-with-your-profile}

Para controlar la radio (tramas UART, ACC, salidas de iluminación y de marcha atrás), añade una
segunda sección a tu `platformio_override.ini`. Esta compiló correctamente en la ejecución del
tutorial:

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
> **El modo decodificador controla la radio y las salidas de 12 V**
>
> Flashea una compilación de decodificador solo después de que el perfil haya pasado su prueba en
> el PC y en el banco, y solo con la caja CAN de posventa desconectada: dos transmisores en una
> misma línea UART y dos fuentes de ACC significan salidas trabajando una contra otra. Lee antes
> [Conexión de la radio](../../en/install-in-car/radio-connection.md) y
> [Primer viaje](../../en/install-in-car/first-drive.md). En el bus CAN del coche esta compilación
> sigue siendo de solo escucha.

### Lo que no compila, a propósito {#what-does-not-build-on-purpose}

| Intento | Resultado |
|---|---|
| `decoder_tx_wifi` con `-DCAR_PROFILE=CUSTOM` | `#error "CAN transmit is not allowed with CAR_PROFILE=CUSTOM ..."` (probado en la ejecución del tutorial) |
| `decoder_tx_wifi` con `-DCAR_PROFILE=GENERIC` | `#error "CAN transmit is not allowed with CAR_PROFILE=GENERIC ..."` (lo comprueba `tools/check_tx_profile_lock.sh`) |
| `-DCAR_PROFILE=EXAMPLE` en cualquier entorno | `#error "CAR_PROFILE=EXAMPLE is a template for the host tests only ..."` |
| `CUSTOM` sin la opción de cabecera o de objeto | `#error "CAR_PROFILE=CUSTOM needs ..."` |

## No empieces desde el perfil de referencia {#do-not-start-from-the-reference-profile}

Podrías copiar `firmware/lib/car_psa_aee2010/` en lugar de la plantilla. Ten en cuenta lo que pasa
entonces: si tu perfil mantiene las funciones de referencia, decodifica los ID de trama del coche de
referencia (puertas, ajustes, ordenador de a bordo, climatizador). Si además mantiene
`psa::decodeBodyState` para el encendido, el firmware trata tu coche como un coche PSA AEE2010
también para el cambio de ajustes y las condiciones de transmisión
(`firmware/lib/app/decoder.cpp`, `Decoder::begin`). Eso solo es correcto para un coche del que se
ha medido que usa las mismas tramas. Trata cada trama de otro coche como **sin verificar** hasta
que la hayas grabado, incluso en un pariente cercano del coche de referencia.

## Errores frecuentes {#common-mistakes}

| Síntoma | Causa |
|---|---|
| `fatal error: profile_mine.h: No such file or directory` | `name` de `library.json` sin cambiar, o falta `lib_deps` |
| `# can=FAIL` al arrancar, LED rojo | `canBitrate` no es una de las seis velocidades admitidas |
| un botón aparece como mantenido todo el rato | su patrón `mask` / `value` está presente en reposo |
| botones en el bit equivocado | `byteIndex` contado desde 1 en lugar de 0 |
| la salida de iluminación se cae al cabo de 1 s | la trama de luces no se repite al menos una vez por segundo |
| no hay pitido, pero las zonas cambian | el estado del lado no es `4` (`PARK_ACTIVE`) |

Siguiente: [Prueba tu perfil](test-your-profile.md).
