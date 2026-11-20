# Prueba en banco

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

En esta página compruebas el dispositivo en tu mesa antes de acercarlo al coche. Cada parte
necesita un poco más de hardware que la anterior. Detente en lo primero que no coincida y busca la
causa.

Esta es la versión corta para una primera compilación. El protocolo completo, paso a paso, para
una compilación nueva del firmware, con las comprobaciones para desarrolladores, está en la página
[Protocolo de prueba en banco](../../en/testing/bench-test-protocol.md).

## 1. La placa ESP32-S3 por USB {#1-the-esp32-s3-board-on-usb}

Flashea `listen_wifi` (o `listen`) como en la página [Flashear el firmware](flash.md) y abre el
monitor. Después escribe (lo que escribes no se ve; pulsa Intro después de cada comando):

| Comando | Qué deberías ver |
|---|---|
| `ver` | la versión y la hora de compilación |
| `stat` | `[stat] mode=alongside the box can=ok audio=ok radio=ok` y los contadores; sin 12 V la alimentación indica `no 12V` |
| `help` | la lista de comandos |
| `cantx` | `[cantx] built=0: no CAN transmit in this build (CAN listen only)` |

El LED de la placa ESP32-S3 muestra el estado: azul al arrancar, apagado mientras el bus está en
silencio, verde mientras llegan tramas, amarillo mientras el dispositivo pita, rojo cuando un
controlador falló al arrancar (las salidas siguen apagadas) o el controlador CAN no está
funcionando.

Puedes pegar una trama CAN en la consola, por ejemplo `0E1#B0F000FCFC1EC200`. El dispositivo la
trata como si viniera del coche, así que puedes probar la decodificación sin coche. Resultado
esperado: la línea `[park]` del siguiente `stat` cambia.

## 2. La placa portadora a 12 V, sin módulos {#2-the-carrier-board-on-12-v-without-modules}

La placa portadora es la placa que conecta la placa ESP32-S3 con el coche. Compruébala primero
vacía: sin la placa ESP32-S3 y sin módulos en sus zócalos.

> [!WARNING]
> **Usa un límite de corriente**
>
> Alimenta la placa portadora desde una fuente de laboratorio con el límite de corriente en unos
> 100 mA. Si la fuente llega al límite, apágala enseguida y busca un cortocircuito.

1. Sin alimentación, con un multímetro: no hay cortocircuito entre +12 V y masa, +5 V y masa,
   +3.3 V y masa. Entre CANH y CANL deberías leer decenas de kΩ, no unos 120 Ω.
2. 12 V en la entrada de alimentación (en el prototipo v1: borne J5, `+12V` y `GND`). En la línea
   de 5 V deberías leer unos 4.6-4.8 V (la salida del convertidor de 5 V después de un diodo).
3. Apaga, inserta la placa ESP32-S3, enciende. Ahora los pines de 3.3 V marcan 3.3 V y la
   corriente es de decenas de mA.

La página del prototipo tiene la lista de comprobación completa con los pines de los zócalos:
[PCB prototipo](../../en/hardware/prototype-pcb.md).

## 3. Con los módulos {#3-with-the-modules}

1. Apaga. Inserta el módulo CAN (interruptor del terminador en OFF) y el módulo amplificador con
   su altavoz.
2. Enciende y conecta el USB. Escribe `stat`. Resultado esperado: `can=ok audio=ok radio=ok` y una
   tensión de alimentación en lugar de `no 12V`. (Solo por USB, sin 12 V, el prototipo también
   mostró `can=ok audio=ok radio=ok`; medido (measured): ESP32-S3 + módulo CAN + amplificador.)
3. Escribe `test`. Resultado esperado: 1.5 s de pitidos de prueba por el altavoz.
4. Escribe `vol 30` para cambiar el volumen. El firmware lo limita al 80 % para proteger un
   altavoz pequeño. Un valor por debajo del 15 % solo dura hasta el siguiente reinicio.

## 4. Las salidas de 12 V (solo compilaciones de decodificador) {#4-the-12-v-outputs-decoder-builds-only}

> [!WARNING]
> **No con la caja CAN conectada**
>
> Flashea `decoder` o `decoder_wifi` en el banco, o en el coche solo con la caja CAN
> desconectada. Las salidas del dispositivo y de la caja nunca deben controlar el mismo cable.

Las salidas ACC, ILL y REV solo conmutan en modo decodificador y siguen al coche: ACC según las
tramas de encendido, ILL según el bit de las luces, REV según el bit de la marcha atrás. Sin
tráfico CAN se quedan apagadas. Todavía no existe ningún informe de prueba en banco de alguien que
haya montado la placa para estas salidas (**sin verificar**, unverified); compruébalas con un
multímetro y una carga pequeña antes de conectar la radio.

## 5. El panel web (compilaciones con Wi-Fi) {#5-the-web-panel-builds-with-wi-fi}

Con `listen_wifi` en la placa, sigue [Primer arranque](first-boot.md) para iniciar sesión.
Después:

1. El panel se abre y muestra el **Resumen**.
2. En la pestaña **Consola** escribe `stat`. Resultado esperado: la misma respuesta que por USB.

La actualización y la vuelta al programa anterior por Wi-Fi son los pasos 7.1-7.7 del
[Protocolo de prueba en banco](../../en/testing/bench-test-protocol.md).

<details class="note" markdown="1">
<summary>Comprobaciones del panel para desarrolladores</summary>

Compruébalas con los valores del panel o con `stat` en la consola:

1. Mientras el Wi-Fi arranca y se detiene, el bucle principal no se ralentiza
   («Iteraciones / s» y «Paso de Wi-Fi más largo» en la pestaña Sistema).
2. Una actualización del firmware desde el panel funciona, y también volver al programa
   anterior. Un archivo incorrecto se rechaza.
3. Con una fuente CAN en el banco: durante una actualización los contadores de tramas CAN
   perdidas se quedan en cero, y los contadores del enlace con la radio no muestran tramas
   perdidas mientras un navegador mantiene el panel abierto.
4. Un escáner Wi-Fi en tu móvil nunca muestra una red abierta llamada `ESP_...` durante varios
   arranques.
5. El sueño profundo y la corriente en sueño después de tener el Wi-Fi encendido: mídelos.
   Todavía no se han medido en el hardware.

Los pasos 1 y 2 de arriba y los puntos 1, 2 y 4 se comprobaron en el prototipo por USB, sin
CAN ni radio (medido: prueba en banco de una compilación anterior de este firmware). Los
puntos 3 y 5 necesitan tráfico CAN o 12 V y siguen pendientes, igual que el tiempo que la
radio se queda sin ACC mientras el dispositivo se reinicia tras una actualización.

</details>

## 6. Antes del coche {#6-before-the-car}

- El diodo supresor de sobretensiones (`D2` en el prototipo v1) está montado y el condensador de
  entrada es de 35 V o más.
- No hay ningún terminador de 120 Ω: el interruptor del terminador del módulo CAN está en OFF.
- `JP1` en el prototipo v1 está abierto (hardware de solo escucha), salvo que compiles a propósito
  el firmware de transmisión.
- Conoces los pines de tu coche.

## Siguiente paso {#next}

[Lista de comprobación de instalación](../../en/install-in-car/checklist.md) para el coche de
referencia, o [Encuentra tu bus CAN](../make-it-yours/find-your-bus.md) para otro coche.
