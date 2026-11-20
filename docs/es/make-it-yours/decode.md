# Decodifica tramas

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Tienes una grabación con marcadores ([Graba el bus](record.md)). Ahora buscas los bits que
cambiaron cuando hiciste algo en el coche. Esta página lo hace paso a paso con grabaciones reales
del coche de referencia, con la herramienta `firmware/tools/can_diff.py`.

La herramienta solo lee archivos. Necesita Python 3 y nada más. Todos los comandos de abajo se
ejecutan desde la carpeta del repositorio.

Los ejemplos usan grabaciones que están en el repositorio. Tu propia grabación de `record.py` está
en la carpeta `logs` junto a la carpeta del repositorio, en un archivo cuyo nombre termina en
`_can_YYYYMMDD_HHMMSS.log`. Pon su ruta en su lugar, por ejemplo
`python -I firmware/tools/can_diff.py ../logs/<nombre del archivo> --list`.

## Bytes y bits {#bytes-and-bits}

Una trama CAN tiene un ID (tres cifras hexadecimales, por ejemplo `0F6`) y hasta 8 bytes de datos.
Este proyecto los numera así:

- bytes desde **1**: el byte 1 es el primer byte de datos (`data[0]` en el código C++);
- bits **7..0** dentro de un byte, el bit 0 es el menos significativo;
- forma corta `byte.bit`: `8.7` = byte 8, bit 7.

Ejemplo: la trama `0F6#884D1E2182585828` tiene byte 1 = `88`, byte 2 = `4D`, ... byte 8 = `28`.

Para ver los bits, escribe cada cifra hexadecimal como cuatro cifras binarias:

| Hex | Binario (bits 7..0) |
|---|---|
| `24` | `0010 0100` |
| `A4` | `1010 0100` |

`24` y `A4` solo se diferencian en el bit de más a la izquierda, así que el cambio está en el
bit 7.

## Qué hace la herramienta {#what-the-tool-does}

Para cada marcador, `can_diff.py` compara dos ventanas de tiempo:

- **antes** (before): los segundos justo antes del marcador (3 s por defecto, `--before`);
- **después** (after): los segundos justo después (5 s por defecto, `--after`).

Lista cada bit que estaba fijo en "antes" y cambió en "después". Dos filtros quitan la mayor parte
del ruido:

- **contadores**: un byte o medio byte que avanza la misma cantidad en casi todas las tramas de
  toda la grabación (contadores cíclicos, temporizadores). La herramienta los imprime una vez
  arriba y nunca los incluye en los resultados.
- **fondo**: un bit que ya cambiaba en la ventana "antes" se ignora para ese marcador (el
  "ignored: N background bits" de la salida).

También lista los ID que solo aparecen después del marcador ("new IDs") y los ID que desaparecen.

Primero comprueba que la herramienta funciona en tu ordenador. El archivo sintético es un coche
inventado con respuestas conocidas:

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --selftest
```

```
selftest: 6 markers, 3216 frames, 0 errors
```

Opciones útiles:

| Opción | Qué hace |
|---|---|
| `--list` | lista los marcadores con sus números y tiempos, y para |
| `--marker N` | solo el marcador N (se puede repetir) |
| `--match TEXT` | solo los marcadores cuyo texto contiene TEXT |
| `--after S`, `--before S` | longitud de las ventanas en segundos |
| `--id 0F6,21F` | solo estos ID |
| `--summary` | para los marcadores elegidos: en cuántos de ellos cambió cada bit |
| `--export OUT` | escribe solo las líneas CAN en OUT, para otros programas |

Los marcadores son las líneas de comentario que empiezan por `# STEP`, `# MARK`, `# MARKER` o
`# KROK` (la palabra polaca usada en las grabaciones de referencia). Otro patrón se indica con
`--marker-regex`.

## Encuentra los marcadores {#find-the-markers}

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --list
```

```
firmware/test_data/captures/can_20261110_190421.log: 186700 frames, 70 IDs, 73 markers, 0 broken lines skipped
counters (never reported): 0E6:8H 10B:5L 236:5 3F6:3H
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
    3  line   4078     416.094 s  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
...
```

La línea de contadores se lee así: en `0E6` cuenta el medio byte alto (`H`) del byte 8, en `10B`
el medio byte bajo (`L`) del byte 5, en `236` el byte 5 entero, en `3F6` el medio byte alto del
byte 3. La herramienta nunca los incluirá en los resultados.

## Ejemplo 1: marcha atrás (`0F6`) {#example-1-reverse-gear-0f6}

El paso 17 de esta grabación dice, en polaco, "marcha atrás: meter 3 s, quitar, 3 veces (sin
obstáculo)". El paso duró unos 27 s, así que usa una ventana "después" larga:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25
```

Sin `--id` la herramienta lista 13 ID: `0A1 0B6 0E1 0E6 0F6 10B 128 161 227 236 2AD 3E7 3F6`.
La trama de los sensores de aparcamiento `0E1` también cambia, y los valores lentos derivan
durante 25 s. Busca el bit que cambia **tantas veces como hiciste la acción**. Aquí está en
`0F6`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25 --id 0F6
```

```
==============================================================================
marker 17, line 43732, at 597.094 s
  # KROK 17 19:08:20.249 Wsteczny: wlacz 3 s, wylacz, 3 razy (bez przeszkody)
  window: 3.0 s before, 25.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 2: 51 52
      bits: 2.1 0->1 x1 @+23.87s, 2.0 1->0 x1 @+23.87s
    byte 8: 24 A4 24 A4 24 A4 24 A4
      bits: 8.7 0->1 x7 @+8.67s
```

Cómo leerlo:

- `byte 8: 24 A4 24 ...` es la secuencia de valores que tomó el byte, sin repeticiones;
- `8.7 0->1 x7 @+8.67s`: el bit 8.7 valía 0 antes del marcador, cambió 7 veces, la primera a los
  8.67 s del marcador.

El bit 8.7 pasó a 1 y volvió una y otra vez, al ritmo de la acción. El byte 2 cambió una vez,
cerca del final: eso no es la acción.

El plan decía tres veces; el bit pasó a 1 cuatro veces en la ventana. La grabación no dice por qué.
Fíate de lo que muestra el archivo, contrástalo con tus notas y confirma una señal en una segunda
grabación.

Resultado: marcha atrás = `0F6` 8.7 (medido (measured): `can_20261110_190421.log`, paso 17; el
mismo bit está en `vehicles/peugeot-508-sw-2015/508.dbc`).

## Ejemplo 2: encendido y motor (`0F6` byte 1) {#example-2-ignition-and-engine-0f6-byte-1}

Algunas señales no son un bit sino un byte entero con unos pocos valores. El paso 3 dice "arrancar
el motor, 10 s sin moverse":

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 3 --after 10 --id 0F6
```

```
==============================================================================
marker 3, line 4078, at 416.094 s
  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
  window: 3.0 s before, 10.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 1: 88 90 91 89 8A 8E
      bits: 1.4 0->1 x2 @+3.30s, 1.3 1->0 x2 @+3.30s, 1.2 0->1 x1 @+5.02s, 1.1 0->1 x1 @+4.03s, 1.0 0->1 x2 @+3.46s
    byte 8: 20 24 64
      bits: 8.6 0->1 x1 @+6.12s, 8.2 0->1 x1 @+4.07s
```

Aquí la lista de bits confunde, pero los **valores del byte** cuentan la historia: `88` antes,
luego `90 91 89 8A` mientras gira el motor de arranque, y luego `8E` con el motor en marcha. Con
más grabaciones de encendido quitado, encendido puesto y arranque del motor, el coche de referencia
da `80` apagado, `88` encendido, `90`/`91`/`89`/`8A` arrancando, `8E` en marcha, `86`/`84`
apagándose (medido: grabaciones con pasos de encendido y motor, `508.dbc`).

El byte 8 también cambió (`20 24 64`). No se sabe qué significan esos dos bits. No le des un
significado a un bit solo porque cambió al mismo tiempo.

Para el primer encendido no hay "antes": el bus estaba dormido, y todos los ID son "nuevos" en el
marcador (ver [Graba](record.md#how-markers-get-their-time)). En ese caso lee directamente las
primeras tramas. La primera trama `0F6` de esta grabación es:

```
(398.212947) can0 0F6#884D1E2182585828
```

Byte 1 = `88`: encendido puesto.

## Ejemplo 3: un botón del volante (`21F`) {#example-3-a-steering-wheel-button-21f}

Un caso limpio. El paso 3 de `can_20261111_190817.log` dice "volante: volumen + tres veces, cada
2 s". Esta vez sin `--id`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_190817.log --marker 3 --after 4.5
```

```
firmware/test_data/captures/can_20261111_190817.log: 30040 frames, 62 IDs, 14 markers, 0 broken lines skipped
counters (never reported): 0E6:8H,8L 10B:5L 236:5 3F6:3H
==============================================================================
marker 3, line 6809, at 223.999 s
  # KROK 3 19:08:54.163 Kierownica: glosnosc + trzy razy, co 2 s
  window: 3.0 s before, 4.5 s after; ignored: 10 background bits, counters in 4 IDs
  21F
    byte 1: 00 08 00 08 00
      bits: 1.3 0->1 x4 @+0.78s
  new IDs (absent before, present after): 1A5(4)
```

Un bit: `21F` 1.3 (byte 1 = `08` mientras el botón está pulsado). La ventana de 4.5 s contiene dos
de las tres pulsaciones: 4 cambios. El siguiente marcador llega 4.5 s después, así que una ventana
más larga mezclaría el paso siguiente.

El ID nuevo `1A5` no viene del botón. En esta grabación estaba conectada la caja CAN de posventa, y
envía `1A5` (el nivel de volumen) unos milisegundos después de que la radio cambie el volumen
(medido: [Peugeot 508 SW](../../en/reference/vehicles/peugeot-508.md)). Otros dispositivos
del bus también reaccionan a tu acción. Graba una vez sin ellos si puedes.

Resultado: volumen + = `21F` 1.3 (medido: `can_20261111_190817.log` y `can_20261111_191045.log`
con la línea UART hacia la radio).

## Ejemplo 4: sensores de aparcamiento (`0E1`) {#example-4-parking-sensors-0e1}

Una trama llena de campos. El paso 18 de `can_20261111_191045.log` dice "sensores traseros: marcha
atrás metida, acerca un objeto al parachoques trasero hasta el tono continuo, retíralo, quita la
marcha atrás". Este archivo empieza en el paso 13, así que el paso 18 es el marcador 6
(compruébalo con `--list`):

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_191045.log --marker 6 --after 25 --id 0E1
```

```
==============================================================================
marker 6, line 18775, at 421.532 s
  # KROK 18 19:12:11.696 Czujniki TYL: wsteczny wlaczony, zblizaj przedmiot do tylnego zderzaka az do tonu ciaglego, zabierz, wylacz wsteczny
  window: 3.0 s before, 25.0 s after; ignored: 2 background bits, counters in 0 IDs
  0E1
    byte 1: B0 B4 90 B0
      bits: 1.2 0->1 x2 @+2.53s
    byte 2: 00 D0 00 D0 50 D0 00
      bits: 2.7 0->1 x6 @+15.31s, 2.6 0->1 x4 @+15.31s, 2.4 0->1 x4 @+15.31s
    byte 3: 3F 30 2B 3F 2E 28 25 22 ...
      bits: 3.5 1->0 x2 @+16.86s, 3.4 1->0 x6 @+15.51s, 3.3 1->0 x10 @+15.31s, 3.2 1->0 x10 @+15.31s, 3.1 1->0 x14 @+15.31s, 3.0 1->0 x12 @+15.31s
    byte 4: FC F8 F4 F8 F4 F0 EC E8 ...
      bits: 4.7 1->0 x2 @+17.37s, 4.6 1->0 x2 @+17.75s, 4.5 1->0 x4 @+17.37s, 4.4 1->0 x2 @+16.86s, 4.3 1->0 x6 @+15.51s, 4.2 1->0 x10 @+15.31s
    byte 5: FC 5C 3C 1C FC
      bits: 5.7 1->0 x2 @+17.56s, 5.6 1->0 x2 @+17.82s, 5.5 1->0 x4 @+17.56s
```

Cuando muchos bits vecinos cambian a la vez, forman un solo campo. Divide los bytes en grupos de
bits que se mueven juntos:

| Campo | Dónde | Qué hacen aquí los valores | Significado en el coche de referencia |
|---|---|---|---|
| estado trasero, estado delantero | byte 1, bits 7-5 y 4-2 | `90` = `100 100 00`: trasero 4, delantero 4 | 4 activo, 5 en espera (medido) |
| petición de sonido | 2.4 | 1 mientras el coche pita | medido |
| lados | 2.7-2.6 | `D0`: los dos lados | medido |
| intervalo de pitidos | byte 3, bits 5-0 | `3F`, y luego valores que bajan a medida que el objeto se acerca | `3F` sin sonido, `39` ... `0B` pitidos, `00` tono continuo (medido) |
| zonas de los sensores | bytes 4-6, 3 bits cada una | los valores bajan a medida que el objeto se acerca | 0 lo más cerca ... 7 lejos (medido) |

Significados: `vehicles/peugeot-508-sw-2015/508.dbc` y
[Peugeot 508 SW](../../en/reference/vehicles/peugeot-508.md#parking-sensors-0e1).

El bit 1.5 también cambió (`B4` a `90`), pero no está en la lista: ya se movía en la ventana
"antes", que se solapa con el final del paso de la marcha atrás. El filtro de fondo puede ocultar
un bit real. Si esperas un bit y no lo ves, mueve la ventana: con `--before 1` esta ejecución
muestra también `1.5 1->0 x2`. Los valores del byte (`B0 B4 90 B0`) se muestran siempre completos.

## Repite la acción, cuenta los aciertos {#repeat-the-action-count-the-hits}

La mejor prueba es un bit que cambia en **cada** marcador de la misma acción y en ningún otro. Con
un marcador por repetición, `--match` y `--summary` lo cuentan por ti. En el archivo sintético (un
coche inventado, tres aperturas de puerta y tres pulsaciones de un botón):

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --match door --summary
```

```
firmware/test_data/synthetic/marker_demo.log: 3216 frames, 5 IDs, 6 markers, 4 broken lines skipped
counters (never reported): 100:2,8L
==============================================================================
summary of 3 markers: 1 3 5
  3/3  300 2.5
```

`3/3  300 2.5` = en la trama `300`, el bit 2.5 (byte 2, bit 5) cambió en los tres marcadores
de la puerta. Con `--match volume` la respuesta es
`3/3  310 1.3`. Las 4 líneas rotas están puestas a propósito: la herramienta se salta las líneas
que parecen tramas pero están dañadas (número impar de cifras hexadecimales, más de 8 bytes, un ID
de 4 cifras).

## Cada cuánto llega una trama {#how-often-a-frame-comes}

Algunas señales necesitan una trama que se repita con suficiente frecuencia (mira la
[Lista de adaptación](porting-checklist.md)). La herramienta no muestra el periodo. Léelo en el
archivo:

1. Abre la grabación en un editor de texto.
2. Busca `can0 0F6#` (pon tu ID en lugar de `0F6`).
3. Resta los tiempos de dos líneas seguidas. En `can_20261111_191045.log`:
   336.199989 - 335.699989 = 0.5 s.
4. Comprueba varios pares. Una trama que solo se envía al cambiar tiene huecos largos e
   irregulares.

En Git Bash, `grep "can0 0F6#" <archivo> | head -5` imprime las cinco primeras líneas de un ID.

## De los bits a los valores {#from-bits-to-values}

Un campo se convierte en un número con una **escala** y un **desplazamiento** (offset):

```
value = raw × scale + offset
```

Ejemplo del coche de referencia: temperatura exterior = `0F6` byte 7, escala 0.5, desplazamiento
-40, es decir `value / 2 - 40` °C (medido: coincide con la temperatura que la caja CAN envió a la
radio en 21 de 21 tramas, `can_20261111_191045.log`). En la trama `0F6#884D1E2182585828` de arriba,
byte 7 = `58` = 88, y 88 / 2 - 40 = 4 °C.

Para encontrar una escala, graba el valor en dos puntos conocidos (por ejemplo la temperatura que
muestra el cuadro de instrumentos, o el volumen de la radio) y compara.

### Escríbelo en un archivo DBC {#write-it-into-a-dbc-file}

Un archivo DBC es un archivo de texto que lista tramas y señales. Muchas herramientas CAN lo leen.
El archivo del coche de referencia es `vehicles/peugeot-508-sw-2015/508.dbc`. Una línea por señal:

```
BO_ 246 BODY_STATE: 8 CAR
 SG_ reverse : 63|1@0+ (1,0) [0|1] "" DEVICE
```

- `246` es el ID en decimal (`0F6` hex = 246).
- `63|1@0+`: bit de inicio 63, longitud 1 bit, `@0` = orden de bytes big-endian (también llamado
  Motorola; elige este en SavvyCAN o cantools), `+` = sin signo.
  Con `@0`, bit de inicio = (byte - 1) × 8 + bit del bit más alto del campo: byte 8 bit 7 =
  7 × 8 + 7 = 63. El intervalo de pitidos, byte 3 bits 5-0, es `21|6@0+`.
- `(1,0)` = escala y desplazamiento; la temperatura exterior tiene `(0.5,-40)`.

Pon a cada señal un comentario que empiece por su estado (`measured: ...` con la grabación, o
`unverified: ...` con la fuente), como en `508.dbc`. Después convierte las señales en un perfil de
coche: [Escribe un perfil de coche](write-a-profile.md).

## SavvyCAN y otras herramientas {#savvycan-and-other-tools}

SavvyCAN es un programa de escritorio gratuito para análisis CAN que lee archivos DBC y archivos de
registro en varios formatos. Este proyecto **no tiene enlace en vivo** con él: el firmware no tiene
GVRET ni un protocolo parecido. Trabajas con archivos grabados. No se ha comprobado si tu versión
de SavvyCAN abre directamente el formato de grabación de este proyecto; si rechaza las líneas de
comentario o `uartA`, exporta antes las líneas CAN:

```
python -I firmware/tools/can_diff.py my_recording.log --export frames_only.log
```

## Límites de la herramienta {#limits-of-the-tool}

- Un marcador no tiene tiempo propio: recibe el tiempo de la siguiente trama CAN. En un bus dormido
  ese es el momento en que el bus despierta.
- Una señal que cambia despacio (temperatura, combustible) puede no cambiar dentro de la ventana.
- Un bit que ya se movía antes del marcador queda oculto (ver el ejemplo 4).
- El filtro de contadores busca pasos regulares. Un contador que salta, o una suma de verificación,
  no se reconoce y aparece como un bit inquieto en todas las ventanas.
- Solo se leen ID de 11 bits (tres cifras hexadecimales).

Lo que no se sabe después de decodificar: qué hace el coche con un valor que no viste cambiar. Una
señal que encontraste está medida para las acciones que grabaste, nada más.

Siguiente: [Escribe un perfil de coche](write-a-profile.md).
