# Encuentra tu bus CAN

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Antes de grabar o decodificar nada, necesitas tres datos sobre tu coche:

1. qué dos cables llevan el bus CAN en la radio,
2. qué tipo de bus CAN es (de alta velocidad o de baja velocidad tolerante a fallos),
3. su velocidad de bits (la velocidad, en kbit/s).

Esta página muestra cómo encontrarlos con un multímetro y con el dispositivo en modo solo escucha.
Nada de esta página envía nada al coche.

Todo el camino, desde medir el bus hasta compartir tu coche, está en una sola página:
[Lista de adaptación](porting-checklist.md). Tenla abierta y marca las filas a medida que avanzas.

> [!CAUTION]
> **En esta página usa solo una compilación de solo escucha**
>
> Los pasos 1 y 2 solo necesitan un multímetro. Desde el paso 3, usa solo una compilación de
> solo escucha: `listen_generic_wifi` o el sniffer aparte de `extras/sniffer/`. No uses nunca un
> entorno con transmisión (`decoder_tx_wifi`) en un coche que aún no conoces. En una compilación
> de solo escucha el controlador CAN no envía tramas, ni bits de confirmación (acknowledge), ni
> tramas de error, así que sigue siendo invisible para el coche incluso con una velocidad de bits
> equivocada. Lee [Primero, la seguridad](../getting-started/safety.md) antes de empezar.

## Qué estás buscando {#what-you-are-looking-for}

Un coche moderno suele tener varios buses CAN. Cada uno une un grupo de módulos y tiene su propia
velocidad. El conector de la radio te da acceso a uno de ellos, y solo a ese. Qué bus es, y qué
señales lleva, depende del coche: en tu coche no se sabe hasta que lo midas.

En el coche de referencia (Peugeot 508 SW, 2015) el bus de la radio lleva el encendido, la marcha
atrás, las luces, los botones del volante, las puertas y los sensores de aparcamiento (medido
(measured): grabaciones en `firmware/test_data/captures/`). Otro coche puede poner algunas de estas
señales en un bus que la radio no ve.

Reúne tus datos en la tabla de la plantilla de vehículo, `vehicles/_template/README.md`, sección
"Bus at the radio". Rellena una fila solo cuando la hayas medido.

## Paso 1: encuentra los dos cables {#step-1-find-the-two-wires}

Si tu coche tiene una caja CAN de posventa, su mazo de cables es la forma más fácil de entrar: el
par CAN va del conector del coche al enchufe de la caja CAN.
[Conexión CAN](../../en/install-in-car/can-connection.md) (paso 1) explica cómo encontrarlo con un
comprobador de continuidad, y [Cableado](../../en/hardware/wiring.md) explica cómo trazar un mazo entero.

Apunta los dos extremos de cada cable y el lado desde el que miraste (lado de los cables o lado de
los contactos, dónde está el pestillo). Un número de pin sin vista se lee fácilmente al revés.

Ejemplo del coche de referencia: Quadlock pin 10 = CANH, pin 13 = CANL (medido: continuidad desde
el transceptor de la caja CAN, y después recepción sin errores con esta asignación). Los números de
pin de la carcasa Quadlock son una numeración propia del autor; la correspondencia con la
numeración del fabricante está sin verificar (unverified). Detalles:
`vehicles/peugeot-508-sw-2015/wiring.md`.

### Comprueba el terminador de la caja antigua {#check-the-terminator-of-the-old-box}

El bus del coche ya tiene sus resistencias de terminación. Si el dispositivo sustituye a una caja
CAN, comprueba que la caja no tenía ninguna, para saber que el dispositivo tampoco debe tenerla:

1. Desconecta la caja CAN y déjala sin alimentación.
2. Mide con un multímetro la resistencia entre sus pines CANH y CANL.
3. Apunta el valor. Decenas de kiloohmios = la caja no tiene terminador. Unos 120 ohmios = la caja
   termina el bus; apúntalo y pregunta en un issue antes de seguir.

Ejemplo del coche de referencia: dos cajas de posventa, 48 kOhm y 59 kOhm (medido: óhmetro, sin
alimentación), así que ninguna de las dos cajas termina el bus.

## Paso 2: distingue el tipo de bus {#step-2-tell-the-kind-of-bus}

Encendido puesto, multímetro en voltios de continua, punta negativa a masa, punta positiva en cada
cable CAN. La tabla de
[Conexión CAN, paso 2](../../en/install-in-car/can-connection.md#step-2-measure-the-voltages) te dice qué
significan las lecturas. En resumen:

- los dos cables cerca de 2.5 V: probablemente **CAN de alta velocidad**;
- un cable cerca de 0 V y el otro cerca de 5 V: probablemente **CAN de baja velocidad tolerante a
  fallos**; el cable cerca de 0 V es CANH;
- unos 12 V: no es un cable CAN, para.

Un multímetro muestra un valor medio, así que esto es una pista, no una prueba. Un osciloscopio
muestra los niveles reales en reposo y en estado dominante.

Ejemplo del coche de referencia: CANH 0.65 V, CANL 4.5 V respecto a masa, con el encendido puesto,
iguales con y sin el dispositivo conectado (medido: multímetro). Estos niveles parecen de CAN de
baja velocidad tolerante a fallos; es una conclusión a partir de las tensiones y aún hace falta un
osciloscopio para confirmarla.

> [!NOTE]
> **El dispositivo tiene un transceptor de alta velocidad**
>
> El transceptor del dispositivo es de tipo alta velocidad a 3.3 V. En el coche de referencia
> recibió el bus sin errores aunque las tensiones parecen de CAN de baja velocidad tolerante a
> fallos (medido: todas las grabaciones de `firmware/test_data/captures/`). Si recibe el bus de
> tu coche no se sabe hasta que lo pruebes. Qué haría un transmisor de alta velocidad en un bus
> así no se ha medido, y es un motivo más para quedarse en solo escucha.

## Paso 3: encuentra la velocidad de bits {#step-3-find-the-bit-rate}

### Con un osciloscopio {#with-an-oscilloscope}

Mira el impulso más corto en CANH o CANL mientras pasan tramas. Ese impulso es un bit:

| Impulso más corto | Velocidad de bits |
|---|---|
| 2 µs | 500 kbit/s |
| 4 µs | 250 kbit/s |
| 8 µs | 125 kbit/s |
| 10 µs | 100 kbit/s |
| 20 µs | 50 kbit/s |

### Con el dispositivo, probando {#with-the-device-by-trial}

La velocidad de bits viene del perfil del coche. Para un coche desconocido usas el perfil
`generic`: no decodifica nada, nunca transmite y nunca enciende las salidas de 12 V. Su velocidad
es un único número en `firmware/lib/car_generic/profile_generic.cpp`:

```cpp
    125000,
```

125 kbit/s es solo el valor por defecto, porque es la velocidad del coche de referencia. No es un
dato sobre tu coche.

1. Conecta el dispositivo a los dos cables del paso 1
   ([Conexión CAN](../../en/install-in-car/can-connection.md)).
2. Elige una primera suposición a partir del paso 2. Un bus de baja velocidad tolerante a fallos va
   a 125 kbit/s o menos; un bus de alta velocidad suele ir a 500 kbit/s, a veces a 250 kbit/s
   (propiedades generales de los dos estándares, no datos sobre tu coche). Prueba en este orden:
   baja velocidad `125000`, luego `100000`, `50000`; alta velocidad `500000`, luego `250000`,
   `125000`.
3. Pon la velocidad en bits por segundo en `profile_generic.cpp`. El firmware solo conoce estos
   valores: `50000`, `100000`, `125000`, `250000`, `500000`, `1000000` (la lista está en
   `firmware/src/can_bus.cpp`). Con cualquier otro valor el controlador CAN no arranca y el
   dispositivo se queda en su estado seguro.
4. Compila y flashea, desde la carpeta `firmware`: `uvx platformio run -e listen_generic_wifi -t upload`.
   Resultado esperado: `[SUCCESS]`. Si la carga no empieza, mira
   [Flashear el firmware](../quick-start/flash.md).
5. Pon el encendido. Abre la consola USB con `uvx platformio device monitor` y escribe `stat`
   unas cuantas veces ([Consola](../../en/reference/console.md)).
6. Lee la línea que empieza por `[stat] rx=`:

| Qué ves | Qué significa |
|---|---|
| `rx=` sube cientos por segundo, `errors=` no cambia | velocidad correcta |
| `rx=` se queda en 0, `errors=` sube | velocidad equivocada, o CANH y CANL intercambiados |
| `rx=` se queda en 0, `errors=` se queda en 0 | no hay tráfico: bus dormido, cables equivocados o sin masa |

Unos pocos errores en el momento en que el bus despierta pueden ser normales. En una grabación de
referencia el contador de errores del sniffer pasó de 136 a 192 mientras el bus despertaba y
después se quedó en 192 el resto de los 14 minutos (medido: las líneas `# stat` de
`can_20261110_190421.log`).

Si la velocidad es incorrecta, cambia el número, vuelve a flashear y repite. Prueba el siguiente
valor solo después de haber comprobado los cables.

El sniffer aparte de `extras/sniffer/` tiene la velocidad fija en su código fuente: `CAN_TIMING`,
125 kbit/s (`extras/sniffer/README.md`). La orden `scan` de su variante grabadora mide las dos
líneas UART hacia la radio, no el bus CAN.

## Paso 4: cuándo está despierto el bus {#step-4-when-is-the-bus-awake}

Averigua cuándo está en silencio tu bus. Lo necesitas para planificar las grabaciones: un marcador
puesto mientras el bus duerme recibe la hora del momento en que el bus despierta
([Graba](record.md)).

Ejemplo del coche de referencia: con la llave quitada el bus está en silencio. Después de quitar el
encendido hay tráfico completo durante unos 100 s, luego 14 s de una sola trama (`036`) y después
silencio (medido: grabaciones de este coche; `vehicles/peugeot-508-sw-2015/wiring.md`).

## Paso 5: un primer vistazo {#step-5-a-first-look}

Con la velocidad correcta, escribe `dump` en la consola USB. El dispositivo imprime cada trama en
una línea:

```
(140.548) can0 0F6#8E501E33BE5D5D24
(140.569) can0 128#C0B0010200411400
(140.573) can0 0E1#B4003FFCFCFCC200
```

Eso es el tiempo en segundos, la interfaz, el ID de la trama, `#` y los bytes de datos en
hexadecimal. Escribe `dump` otra vez para pararlo. Las líneas de arriba son de
`centralka_can_20261117_184411.log`.

Como referencia: con el encendido puesto, el coche de referencia envía unas 218 tramas por segundo
con 70 ID distintos (medido: `can_20261110_190421.log`). Tu coche puede enviar más o menos.

Apunta lo que encontraste (conector, pines y vista, tipo de bus, velocidad de bits, cuándo duerme
el bus).

Siguiente: [Graba el bus](record.md).
