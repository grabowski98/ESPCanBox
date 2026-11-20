# Asigna la radio

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Tu perfil de coche dice **qué bits CAN son qué botón**. El protocolo de radio dice **qué código
recibe la radio para ese botón**. Entre los dos hay una lista de nombres lógicos de botón, `KeyId`.
Esta página explica las tres partes y cómo cambiar lo que hace un botón en la radio, con o sin
volver a compilar.

## Tres tablas {#three-tables}

```text
CAN frame ──► car profile ──► logical button ──► radio protocol ──► key code ──► remapping ──► radio
              keys / knobs      KeyId             keyCode()          e.g. 14       map FROM TO
```

| Parte | Archivo | Cambia con |
|---|---|---|
| bits CAN -> `KeyId` | tu perfil, tablas `keys` y `knobs` ([Escribe un perfil de coche](write-a-profile.md)) | el coche |
| `KeyId` -> código de botón | el protocolo de radio, `firmware/lib/radio_xp/protocol_xp.cpp` (`xp::keyCode`) | el protocolo de radio |
| código de botón -> otro código de botón | tabla de reasignación, en tiempo de ejecución, guardada en los ajustes del dispositivo | tu gusto |

El perfil de coche no conoce códigos de radio, y el protocolo de radio no conoce ID CAN.
`tools/check_layering.py` lo comprueba.

## Protocolos de radio disponibles {#radio-protocols-available}

| Opción de compilación | Protocolo | Qué hace |
|---|---|---|
| `-DRADIO_PROTOCOL=XP` (por defecto) | `xp` | el protocolo de la caja CAN Simple Soft ("XP") hacia una radio Android de la plataforma FYT, UART 38400 8N1. Solo se usan comandos vistos en el cable: [Protocolo XP](../../en/reference/xp-protocol.md) |
| `-DRADIO_PROTOCOL=NONE` | `none` | sin radio: el UART no arranca, ningún botón tiene código, nunca se envía nada |
| `-DRADIO_PROTOCOL=CUSTOM` | el tuyo | tu propio protocolo en `firmware/lib/radio_<nombre>`, indicado con `RADIO_PROTOCOL_CUSTOM_HEADER` y `RADIO_PROTOCOL_CUSTOM_OBJECT` (plantilla: `firmware/lib/radio_example`). En el CAN solo escucha: una compilación con transmisión se detiene con un error |

`listen_generic_wifi` usa `none`. Tu propio perfil con el entorno `listen_mycar` usa `xp`, pero en
modo paralelo ("junto a la caja") de todos modos no se envía nada a la radio.

> [!NOTE]
> **Aún no hay otro protocolo de radio**
>
> Los datos del coche para la pantalla de la radio (puertas, ordenador de a bordo, climatizador,
> ajustes ...) llegan a un protocolo de radio a través de los codificadores de `RadioProtocol`
> (`firmware/lib/core/radio_protocol.h`), como tipos neutros. El resto de la lógica XP (el enlace
> con la radio, las tramas de aparcamiento y de botones, las órdenes de la radio) sigue cableado
> dentro del decodificador. Cada trama hacia la radio, también las de los codificadores, recibe el
> entramado XP (cabecera `2E`, longitud, suma de comprobación XP), y además se envían las tramas XP
> de alimentación (`EA`), aparcamiento (`32`), obstáculo (`3E`) y botones (`20`): `RadioProtocol`
> aún no tiene una función de entramado. Así que un protocolo de radio nuevo también necesita cambios en
> `firmware/src/config.h` y `firmware/lib/app/`. Está en la hoja de ruta. Un protocolo nuevo debe
> escribirse a partir de tus propias mediciones del enlace con la radio, no de aplicaciones
> descompiladas ni de firmware de cajas.

## Botones lógicos y códigos XP {#logical-buttons-and-xp-codes}

Los nombres describen el botón del coche de referencia (Peugeot 508 SW 2015). Para tu coche, usa
el nombre de significado más cercano. El código XP es lo que recibe la radio en la trama de botón
`20` (DATA[0] = código, DATA[1] = 1 pulsado, 2 mantenido, 0 soltado). Es una trama UART: sus
bytes de datos se cuentan desde DATA[0], como en la página
[Protocolo de radio XP](../../en/reference/xp-protocol.md). Los bytes CAN de las demás páginas se cuentan
desde el byte 1.

| `KeyId` | Botón en el coche de referencia | Código XP |
|---|---|---|
| `VOLUME_UP` | volumen + del volante | `14` |
| `VOLUME_DOWN` | volumen - del volante | `15` |
| `MUTE` | los dos botones de volumen a la vez | `16` |
| `PHONE` | botón de teléfono del volante | `30` |
| `LIST` | LIST del volante | `50` |
| `WHEEL_KNOB_PRESS` | pulsación de la rueda del volante | `07` |
| `SOURCE` | SRC | `21` |
| `TRIP` | botón del ordenador de a bordo en la palanca del limpiaparabrisas | `23` |
| `NAV` | panel: NAV | `9E` |
| `MEDIA` | panel: MEDIA | `A0` |
| `SCROLL_LEFT` | panel: búsqueda a la izquierda | `97` |
| `ZOOM_IN` | panel: lupa + | `9C` |
| `SCROLL_RIGHT` | panel: búsqueda a la derecha | `98` |
| `TEL` | panel: TEL | `9F` |
| `SETUP` | panel: SETUP | `99` |
| `DRIVE` | panel: DRIVE | `9B` |
| `MOON` | panel: apagar pantalla (luna) | `A4` |
| `ZOOM_OUT` | panel: lupa - | `9D` |
| `MIC_MUTE` | panel: silenciar micrófono | `A1` |
| `WEB` | panel: WEB | `A3` |
| `PANEL_EXTRA` | un botón del panel, sin identificar | `22` |
| `PANEL_KNOB_PRESS` | pulsación del mando de encendido del panel | `16` (el mismo código que `MUTE`) |
| `NEXT` / `PREV` | rueda del volante, un paso | `17` / `18` |
| `PANEL_KNOB_UP` / `PANEL_KNOB_DOWN` | mando del panel, un paso a la derecha / izquierda | `81` / `82` |

Estado: **medido** (measured). Los códigos son lo que la caja CAN Simple Soft envió a la radio para
cada botón, grabado junto con el bus CAN con un mismo reloj (grabaciones `can_20261111_190817.log`
y `can_20261111_191045.log` en `firmware/test_data/captures/`; origen de cada constante en
`radios/xp-simple-soft/provenance.md`). El botón que envía la trama `0A2` (`SOURCE`) aparece
descrito de dos formas en las notas del coche de referencia ("SRC" y "el botón entre SRC y LIST");
hay que comprobarlo de nuevo en el coche.

Si tu coche tiene un botón sin nombre que le corresponda, se puede añadir un nombre nuevo a
`enum class KeyId` en `firmware/lib/core/vehicle_signals.h`, **solo al final de la lista**, para
que los números de los nombres existentes no cambien. También necesita un código en el protocolo
de radio, o no se envía nunca.

## Las reasignaciones por defecto vienen del perfil de coche {#default-remappings-come-from-the-car-profile}

Justo antes de que una trama de botón vaya a la radio, el firmware aplica una pequeña tabla de
reasignación (`firmware/lib/radio_xp/xp_keys_custom.h`). Sus entradas iniciales vienen del perfil de
coche (campos `keyRemapDefaults` y `keyRemapDefaultCount`, ver [Escribe un perfil de coche](write-a-profile.md)).
El perfil de referencia `psa_aee2010` tiene dos, elegidas para el coche de referencia y su radio:

| De | A | Por qué (medido: prueba del autor en el coche, una radio) |
|---|---|---|
| `9F` (`TEL`) | `9B` | abre la pantalla del teléfono; `9F` no hacía nada sin un teléfono conectado |
| `21` (`SOURCE`) | `1F` | asistente de voz (Siri en CarPlay) |

Son preferencias del propietario, no hechos sobre el bus: otro coche puede no tener estos botones.
Los perfiles `generic` y `example` no tienen entradas por defecto, ni tampoco un perfil copiado de
la plantilla, así que cada botón llega a la radio con su propio código. La prueba de la plantilla
lo muestra: `[key] code=21 state=1` (SRC se envía como `21`).

Para tener una reasignación por defecto en tu propio perfil, añade una fila a su tabla
`keyRemapDefaults`. Para quitar una en el dispositivo en marcha, usa `map 21 21` en la consola USB,
o la sección de reasignación de botones del panel web.

El botón de la luna (`A4`) se trata aparte de la tabla (el enclavamiento sigue el código que el
protocolo de radio asigna a `MOON`, en XP `A4`, no a cualquier botón que reciba ese código por
casualidad): la primera pulsación envía solo "pulsado"
(la radio apaga su pantalla), la segunda solo "soltado". Una reasignación de `A4` no tiene efecto.

## Cambia una asignación sin volver a compilar {#change-a-mapping-without-rebuilding}

Comandos de la consola USB ([Consola USB](../../en/reference/console.md)):

| Comando | Qué hace |
|---|---|
| `map` | lista la tabla, por ejemplo `[key] swap 9F -> 9B` |
| `map FROM TO` | el código de botón `FROM` envía `TO` a la radio, ambos en hexadecimal: `map 9B 1F`. `map 9B 9B` lo quita |
| `map reset` | vuelve a las entradas por defecto del perfil de coche (ninguna en `generic`) |
| `keys` | activa o desactiva la impresión de cada evento de botón (`[key] printing=1` / `0`) |

La tabla admite como máximo 16 entradas y se guarda en los ajustes del dispositivo, así que
sobrevive a un reinicio. El panel web edita la misma tabla.

`FROM` es el código que el protocolo de radio da al botón (la tercera columna de arriba), no un
valor CAN.

## Averigua qué hace un código en tu radio {#find-out-what-a-code-does-in-your-radio}

En una compilación de decodificador puedes enviar cualquier código de botón a la radio desde la
consola USB, sin tocar el coche:

| Comando | Qué envía |
|---|---|
| `key 1F` | código `1F` pulsado y después soltado |
| `kd A4` | solo "pulsado" |
| `ku A4` | solo "soltado" |

En una compilación de solo escucha la consola responde `: not sent (listen mode)` y no sale nada.

Lo que hicieron algunos códigos en la radio del coche de referencia, probados con `key` (medido:
prueba del autor, una radio, perfil CAN "XP Simple Soft -> Peugeot 508 -> Type 2"):

| Código | Reacción |
|---|---|
| `08` | cambio de fuente |
| `1F` | asistente de voz (Siri en CarPlay) |
| `9A` | brillo de pantalla |
| `9B` | teléfono |
| `A5` | pantalla del climatizador |
| `A6` | atrás |
| `B5` | FM / AM |
| `B9`, `BF` | los dos: sintonía hacia abajo 0.05 MHz (dos códigos con la misma nota; hay que comprobarlo de nuevo) |
| `B1`, `91` a `96` | sin reacción |

Otra radio, u otro perfil CAN en los ajustes de la radio, puede reaccionar de otra forma. Prueba
cada código en tu propia radio y apunta qué hizo.

## Qué más llega a la radio con tu perfil {#what-else-goes-to-the-radio-with-your-profile}

Con el protocolo XP y un perfil distinto del de referencia, la radio solo recibe estas tramas
(medido: prueba en el PC del perfil de plantilla, `firmware/test_host/expected/example_car.txt`):

| Comando XP | Contenido | De tu perfil |
|---|---|---|
| `EA` | estado de alimentación (ACC) | `decodeBody` (encendido) |
| `20` | botón | `keys`, `knobs` |
| `32` | imagen de los sensores de aparcamiento | `decodeParking` |
| `3E` | indicador de obstáculo | `decodeParking` |

Los datos del coche para la pantalla de la radio vienen de las funciones opcionales de tu perfil:

- `38` puertas y valores de ajustes: `decodeDoors`, `decodeSettings`;
- `3C` qué filas de ajustes mostrar: `decodeSettings`;
- `33`, `34`, `35` páginas del ordenador de a bordo: `decodeTrip`;
- `36` temperatura exterior: `decodeOutsideTemp`;
- `44` modo económico: `decodeEcoMode`;
- `21` estado del climatizador: `decodeClimate`;
- `29` ángulo del volante: `decodeSteering`.

Una trama cuya función es `nullptr` en tu perfil no se envía nunca. Los codificadores XP están en
`firmware/lib/radio_xp/xp_signals.h`. Cambiar los ajustes del vehículo desde la radio (orden `80`)
solo funciona con el perfil de referencia.

Siguiente: [Lista de adaptación](porting-checklist.md).
