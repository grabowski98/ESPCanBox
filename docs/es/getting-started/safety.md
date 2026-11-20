# Primero, la seguridad

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Lee esta página antes de compilar, flashear o conectar nada. Es corta a propósito.

## Términos usados en estas páginas {#words-used-on-these-pages}

- **el dispositivo**: la placa ESP32-S3 de este proyecto;
- **la caja** o **caja CAN**: la caja de interfaz de posventa que se vende con la radio;
- **la radio**: la unidad principal Android;
- **solo escucha** (listen-only): el dispositivo recibe tramas CAN y no envía nada al bus del
  coche;
- **TWAI**: el controlador CAN integrado en el ESP32-S3;
- **SLNT**: el pin de silencio del módulo transceptor CAN; nivel alto = su transmisor está
  apagado;
- **BSI**: la unidad de control de carrocería del coche;
- **`JP1`**: un puente de la placa prototipo en la línea TX hacia el módulo CAN;
- **entorno**: un conjunto de opciones de compilación con nombre; decide qué puede hacer el
  programa.

El [glosario](../../en/contributing/glossary.md) recoge más términos.

## Qué es este proyecto {#what-this-project-is}

- Un proyecto de aficionado, ofrecido **tal cual**, sin ninguna garantía. Lo usas bajo tu propia
  responsabilidad.
- Se desarrolló y midió en **un coche** (un Peugeot 508 SW de 2015) con una radio Android. En
  cualquier otro coche, cada trama está **sin verificar** (unverified) hasta que la hayas grabado
  tú mismo.
- **No** sirve para nada que proteja a las personas. No lo conectes a frenos, dirección, airbags,
  control del motor ni a ningún bus que los transporte.
- El proyecto no está vinculado con Peugeot, Stellantis, Simple Soft, Raise, Junsun ni FYT. Sus
  nombres aparecen solo para indicar qué se ha probado.

## El dispositivo escucha por defecto {#the-device-listens-by-default}

Del lado del coche, el dispositivo es un receptor CAN. En todas las compilaciones normales
**solo escucha**:

- el controlador TWAI funciona en modo solo escucha (ni siquiera envía el bit de confirmación,
  acknowledge);
- el programa mantiene en nivel alto el pin `SLNT` del transceptor CAN, así que el transmisor del
  chip está apagado;
- las compilaciones `listen`, `decoder`, `listen_wifi`, `decoder_wifi` y `listen_generic_wifi` no
  contienen en absoluto el código de transmisión (el directorio `firmware/src/can_tx/` no se
  compila; `firmware/tools/check_no_can_tx.sh` comprueba los programas compilados).

> [!CAUTION]
> **Transmitir puede cambiar el estado del coche**
>
> Flashea `decoder_tx_wifi` solo si has leído
> [Transmitir en el bus CAN](../../en/safety/transmit.md) y aceptas el riesgo. Nunca envíes
> tramas que no hayas grabado del propio coche.

Solo un entorno, `decoder_tx_wifi`, puede transmitir en el bus del coche. Y solo lo hace después
de que lo armes a propósito y reinicies el dispositivo:

| Compilación | Tramas que puede enviar al bus del coche |
|---|---|
| todos los entornos salvo `decoder_tx_wifi` | ninguna |
| `decoder_tx_wifi` (por defecto) | solo la trama `15B`, comprobada solo por ID y longitud, después de `cantx on` y un reinicio |
| `decoder_tx_wifi` con `-DCAN_TX_EXPERIMENTAL=1` en tu propio `platformio_override.ini` | `15B`, más la trama de prueba `1A9` (solo los botones de sensores de aparcamiento y ángulo muerto) |

El climatizador desde la radio **no** funciona con este proyecto. Con `15B` la BSI aceptó los
ajustes (visible en la trama `260`), pero el efecto visible en el coche **no se comprobó**. Qué hizo
cada trama en el coche de referencia: [Transmitir en el bus CAN](../../en/safety/transmit.md).

## Bloqueo por hardware en la placa prototipo {#hardware-lock-on-the-prototype-board}

La [placa prototipo](../../en/hardware/prototype-pcb.md), tal como está dibujada, tiene un bloqueo por
hardware: una resistencia lleva el pin `SLNT` del módulo CAN a 3.3 V, y el puente `JP1` se queda
**abierto**. Con `JP1` abierto ningún programa puede transmitir en el bus.

La placa del propio autor se modificó después para pruebas de transmisión (`JP1` cerrado, `SLNT`
conectado a GPIO6). En una placa así **solo el firmware** decide si el dispositivo transmite.
Deja `JP1` abierto, salvo que compiles a propósito el firmware de transmisión.

## Antes de empezar {#before-you-start}

Estas reglas vienen de cómo se construyó el propio proyecto. Síguelas en este orden.

1. **Mide antes de conectar.** Averigua qué pines llevan el CAN, qué tipo de bus es (alta velocidad
   o baja velocidad tolerante a fallos) y su velocidad, con un multímetro y, mejor aún, con un
   osciloscopio o un analizador lógico. No conectes nada al bus antes de saberlo.
   La página [Encuentra tu bus CAN](../make-it-yours/find-your-bus.md) explica cómo.
2. **Sin terminador.** El bus del coche ya está terminado. Desactiva el terminador de 120 Ω de tu
   módulo CAN y no añadas ninguno.
3. **Primero, escuchar.** En la mesa de trabajo vale cualquier compilación de solo escucha. En un coche,
   usa `listen` o `listen_wifi` solo en el coche de referencia (Peugeot 508, perfil PSA AEE2010);
   en cualquier otro coche usa solo `listen_generic_wifi` (perfil genérico: graba tramas, no envía
   nada a la radio). Graba, decodifica, compara. Transmitir es lo último, si es que llega a hacerse.
4. **Nada de escribir en las unidades de control del coche.** Este proyecto no hace diagnosis y
   nunca escribe en módulos del coche (BSI ni ningún otro). No añadas código así.
5. **Una sola fuente por cable.** Usa el modo decodificador (`decoder`, `decoder_wifi`) solo con la
   caja CAN de posventa desconectada. Dos transmisores en la línea de datos de la radio, o dos
   fuentes en el cable ACC, significan salidas en cortocircuito.
6. **Banco antes que coche.** Alimenta la placa portadora desde una fuente de laboratorio con
   límite de corriente (unos 100 mA para el primer encendido) antes de llevarla al coche. La placa
   ESP32-S3 sola con un cable USB no necesita fuente de laboratorio. Consulta
   [Prueba en banco](../quick-start/bench-test.md).

## 12 V y la batería {#12-v-and-the-battery}

> [!WARNING]
> **Monta la protección de entrada antes de ir al coche**
>
> En la placa prototipo, monta el diodo supresor de sobretensiones `D2` (SMBJ18A) y usa un
> condensador de entrada de 35 V o más antes de conectar la placa al coche. Deja los fusibles en
> su sitio; un puente de cable en lugar de un fusible es solo para el banco.

- El dispositivo está conectado a 12 V permanentes (como la caja CAN que sustituye). Entra en
  sueño profundo tras 30 s de silencio en el bus. El sueño y la corriente en sueño **no se han
  medido** todavía en el hardware. Mide la corriente de reposo del coche con y sin el dispositivo
  antes de dejarlo conectado durante días.
- Con el Wi-Fi encendido el dispositivo consume más corriente. El firmware apaga el Wi-Fi cuando el
  coche duerme y nadie usa el panel.

## El panel web y las actualizaciones por Wi-Fi {#the-web-panel-and-updates-over-wi-fi}

Las compilaciones `*_wifi` abren su propia red Wi-Fi con un panel web. Conoce sus límites:

- el panel usa HTTP sin cifrar; no hay más cifrado que el WPA2 de la red Wi-Fi;
- la contraseña del dispositivo es también la clave del Wi-Fi;
- las imágenes de firmware enviadas por Wi-Fi **no están firmadas**. Cualquiera que tenga la
  contraseña puede flashear otro programa. En una placa sin el bloqueo por hardware, ese programa
  podría transmitir.

Más información en la página [Modelo de seguridad](../../en/safety/security.md).

## Siguiente paso {#next}

[Qué necesitas](../../en/getting-started/requirements.md): placa, componentes y herramientas.
