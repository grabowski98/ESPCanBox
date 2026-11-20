# Lista de adaptación

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Todo el camino desde "un coche que nadie ha medido" hasta "el dispositivo funciona en lugar de la
caja CAN", en una sola página. Ve de arriba abajo. Cada fila dice cuándo está hecho el paso. No te
saltes una fila porque otro coche "probablemente" funcione igual: cada dato sobre tu coche necesita
un estado, **medido** (measured) o **sin verificar** (unverified).

Apunta tus resultados en una copia de `vehicles/_template/README.md` a medida que avanzas. Los
comandos con `tools/` se ejecutan desde la carpeta que indica la fila: la del repositorio o
`firmware`.

> [!CAUTION]
> **Nada de esta lista transmite en el bus del coche**
>
> Todas las compilaciones usadas aquí solo escuchan en el bus CAN del coche. Tu propio perfil
> no se puede compilar con transmisión en absoluto. Transmitir para un coche nuevo es un proyecto
> aparte con sus propias reglas: [Transmitir en el bus CAN](../../en/safety/transmit.md).

## 1. El bus {#1-the-bus}

| Paso | Hecho cuando | Página |
|---|---|---|
| Encontrar los cables CANH y CANL en la radio | continuidad medida desde el conector hasta un punto conocido, y tramas recibidas en ellos | [Encuentra tu bus](find-your-bus.md) |
| Medir las tensiones de las líneas a masa, con el encendido puesto | tensiones de CANH y CANL apuntadas | [Encuentra tu bus](find-your-bus.md) |
| Decidir si es de alta velocidad o de baja velocidad tolerante a fallos | conclusión escrita como conclusión; comprobación con osciloscopio anotada como hecha o pendiente | [Conexión CAN](../../en/install-in-car/can-connection.md) |
| Medir el terminador de la caja CAN antigua | resistencia CANH-CANL, sin alimentación, apuntada | [Encuentra tu bus](find-your-bus.md#check-the-terminator-of-the-old-box) |
| Compilar y flashear `listen_generic_wifi` con tu primera suposición de velocidad de bits | la consola muestra `# profile: car=generic radio=none` | [Flashear el firmware](../quick-start/flash.md) |
| Encontrar la velocidad de bits | tramas recibidas sin errores a 50, 100, 125, 250, 500 o 1000 kbit/s | [Encuentra tu bus](find-your-bus.md) |
| Comprobar que el bus queda en silencio con la llave quitada | tiempo desde quitar la llave hasta el silencio, a partir de una grabación | [Encuentra tu bus, paso 4](find-your-bus.md#step-4-when-is-the-bus-awake) |

## 2. Grabaciones {#2-recordings}

| Paso | Hecho cuando | Página |
|---|---|---|
| Planificar los pasos | plan escrito: una acción por paso, cada una repetida tres veces | [Graba el bus](record.md) |
| Grabar con marcadores | archivo con un marcador por paso; número de tramas perdidas comprobado | [Graba el bus](record.md) |
| Encontrar las tramas que llevan el VIN | ID apuntados, o "no encontrado" | [Comparte tu coche](contribute.md) |
| Limpiar la grabación (desde la carpeta del repositorio) | `python -I tools/sanitize_capture.py --check` indica 0 problemas, con los ID de tus tramas de VIN en `--ids` | [Comparte tu coche](contribute.md) |

## 3. Señales {#3-signals}

Decodifica solo lo que necesites. Por orden de utilidad:

| Señal | Hecho cuando | Se usa para |
|---|---|---|
| Estado del encendido y del motor | byte y valores para apagado / encendido / motor en marcha, vistos en tres repeticiones; [periodo de la trama](decode.md#how-often-a-frame-comes) conocido (debe ser menor de 1 s) | ACC a la radio |
| Marcha atrás | byte.bit, visto tres veces | salida de marcha atrás (cámara) |
| Luces | byte.bit; [periodo de la trama](decode.md#how-often-a-frame-comes) conocido (debe ser menor de 1 s) | salida de iluminación |
| Botones del volante | un patrón byte.bit por botón, ninguno presente en reposo | botones a la radio |
| Mandos giratorios | byte contador, sentido por cada paso | siguiente / anterior |
| Sensores de aparcamiento | estado, zonas 0 a 7 por sensor, petición de sonido, intervalo si el coche lo envía; [periodo de la trama](decode.md#how-often-a-frame-comes) (debe ser menor de 400 ms) | zumbador, imagen de los sensores en la radio |
| Puertas, maletero, capó (opcional) | un bit por puerta, cada puerta vista abierta y cerrada | imagen de las puertas en la radio |
| Temperatura exterior, climatizador, modo económico, ajustes, ordenador de a bordo (opcional) | byte y escala de cada valor, comparados con la pantalla del propio coche | las pantallas correspondientes de la radio |

Cada señal va a la página del vehículo con su trama, `byte.bit`, valores, estado y el paso de la
grabación que la mostró. [Decodifica tramas](decode.md) explica cómo.

## 4. Perfil {#4-profile}

| Paso | Hecho cuando | Página |
|---|---|---|
| Copiar `firmware/lib/car_example` a `firmware/lib/car_<name>` y renombrar | archivos, objeto, espacio de nombres y nombre en `library.json` renombrados | [Escribe un perfil de coche](write-a-profile.md) |
| Rellenar solo las señales medidas | todo puntero desconocido es `nullptr`, toda tabla desconocida está vacía | [Escribe un perfil de coche](write-a-profile.md) |
| Números de byte convertidos | tus notas (byte 1 = el primero) convertidas en índices del código (`data[0]`, `byteIndex 0`) | [Escribe un perfil de coche](write-a-profile.md) |
| `powerOutputs` decidido | `true` solo con un `decodeBody` fiable | [Escribe un perfil de coche](write-a-profile.md) |
| Compilar `listen_mycar` | `SUCCESS`, `car_<name>` en la lista de dependencias | [Escribe un perfil de coche](write-a-profile.md) |

## 5. Pruebas en el PC {#5-tests-on-the-pc}

| Paso | Hecho cuando | Página |
|---|---|---|
| Prueba en el PC con tu grabación limpia | salida leída línea a línea frente a tus marcadores, y luego guardada en `firmware/test_host/expected/` | [Prueba tu perfil](test-your-profile.md) |
| Todas las pruebas en el PC (desde `firmware`) | `All tests match the references.` | [Pruebas en el PC](../../en/testing/host-tests.md) |
| Comprobación de capas (desde `firmware`) | `python -I tools/check_layering.py` termina con `Layering OK.` | [Prueba tu perfil](test-your-profile.md) |
| Códigos de la radio comprobados | cada uno de tus botones da el código que quieres; reasignaciones por defecto (`keyRemapDefaults`) solo las que quieras | [Asigna la radio](radio-mapping.md) |

## 6. Banco y coche {#6-bench-and-car}

| Paso | Hecho cuando | Página |
|---|---|---|
| Prueba en banco con tu compilación | las líneas de arranque muestran tu perfil y `# can=ok`; salidas de 12 V apagadas en la compilación de escucha | [Prueba en banco](../quick-start/bench-test.md) |
| En el coche junto a la caja CAN (`listen_mycar`) | tus botones aparecen con `keys`, encendido y marcha atrás en la consola, pitidos de aparcamiento por el altavoz del dispositivo; nada enviado a la radio | [Primer viaje](../../en/install-in-car/first-drive.md) |
| Modo decodificador (`decoder_mycar`, caja CAN desconectada) | la radio se enciende y se apaga con el encendido; funcionan los botones, la cámara de marcha atrás y la imagen de los sensores | [Conexión de la radio](../../en/install-in-car/radio-connection.md) |
| Notas de lo que no funciona | listadas en "Not known yet" de la página de tu vehículo | [Comparte tu coche](contribute.md) |

## 7. Compartir {#7-share}

| Paso | Hecho cuando | Página |
|---|---|---|
| Página del vehículo | `vehicles/<make>-<model>-<year>/README.md` con un estado en cada fila | [Comparte tu coche](contribute.md) |
| Comprobación de datos privados (desde la carpeta del repositorio) | `python -I tools/scan_private.py .` con 0 errores | [Comparte tu coche](contribute.md) |
| Pull request o issue | abierto con la plantilla "New vehicle" | [Comparte tu coche](contribute.md) |

Una página del vehículo sola, sin perfil de firmware, también es una contribución útil.

Siguiente: [Comparte tu coche](contribute.md).
