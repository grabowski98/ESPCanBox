# Flashear el firmware

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Esta página te lleva de un ordenador recién instalado a una placa que ejecuta el firmware y abre
su panel web. Los pasos 1-3 funcionan sin placa.

> [!NOTE]
> **Estado de este árbol de firmware**
>
> Este firmware compila y pasa las pruebas en un PC (todos los entornos compilan y las pruebas en
> el ordenador coinciden con sus referencias). La versión 2.0.0 se comprobó en una placa
> ESP32-S3-WROOM-1-N16R8 con el [protocolo de prueba en banco](../../en/testing/bench-test-protocol.md)
> y se probó en un solo coche el 20-11-2026.

**Dónde escribir los comandos.** En Windows escribe todos los comandos de esta página en
**PowerShell** (o Windows Terminal). Git Bash no hace falta para flashear, solo para las pruebas
en el ordenador y para las páginas de [Adáptalo a tu coche](../make-it-yours/porting-checklist.md). En Linux y macOS usa el terminal normal.

## Lo más rápido: una imagen lista en el navegador {#ready-image-in-the-browser}

Es la forma recomendada para la primera instalación. No necesitas PlatformIO, ni un paquete de
Python para flashear, ni normalmente un controlador extra: solo Chrome o Edge y el archivo de
imagen. También funciona donde `pip install esptool` falla (con una versión muy nueva de Python la
instalación puede detenerse con un error de compilación de una dependencia).

1. Consigue la imagen. Las imágenes listas están en la página del proyecto, en **Releases**, un
   par por cada entorno sin transmisión en CAN (`listen`, `decoder`, `listen_wifi`,
   `decoder_wifi`, `listen_generic_wifi`; lee [Qué entorno elegir](#which-environment)). Una
   imagen con transmisión nunca se ofrece lista. Elige el archivo según lo que hagas:

    | Archivo | Dirección | Para qué |
    |---|---|---|
    | `<environment>-<version>-merged.bin` | `0x0` | primera instalación; borra toda la flash, incluidos los ajustes guardados (el dispositivo genera después una nueva contraseña Wi-Fi) |
    | `<environment>-<version>.bin` | `0x10000` | actualización de un dispositivo que ya ejecuta este firmware; conserva los ajustes guardados |

    Tu propia compilación (pasos 1-3 abajo) da la imagen de aplicación
    `.pio/build/<environment>/firmware.bin`: como el archivo sin `-merged`, va a `0x10000` y solo
    actualiza un dispositivo que ya ejecuta este firmware.

2. Conecta la placa al ordenador con un cable USB de datos, en el conector USB nativo del
   ESP32-S3 (consulta [Conecta la placa](#4-connect-the-board)).
3. Abre <https://espressif.github.io/esptool-js/> en Chrome o Edge.
4. Pulsa **Connect** y elige el puerto de la placa en la lista que muestra el navegador.
5. Escribe la dirección de la tabla en el campo de dirección, elige al lado el archivo de imagen y
   pulsa **Program**. No hace falta tocar botones del módulo: la placa se reinicia sola.
6. Resultado esperado: el registro de la página termina con `Hash of data verified.` Si el nuevo
   programa no arranca, pulsa RESET o vuelve a conectar el cable.

La consola de esa página solo muestra lo que imprime el dispositivo; en ella no se pueden escribir
comandos. Para escribir comandos, instala una vez el pequeño terminal serie de Python y ábrelo (en
Linux y macOS escribe `python3` en lugar de `python`):

```text
python -m pip install pyserial
python -m serial.tools.miniterm - 115200
```

El `-` hace que liste los puertos; elige la placa. Lo que escribes no se muestra: escribe el
comando a ciegas y termínalo con Enter. Sal del terminal con Ctrl+].

Tras una primera instalación, `ver` muestra la versión. Una compilación con red abre entonces su
propia red Wi-Fi `<project name>-XXXX` con el panel en `http://10.50.8.1`, para cuatro dispositivos
a la vez como máximo. Sigue con [Primer arranque](first-boot.md#2-read-the-password-from-the-usb-console)
para leer la contraseña.

<details class="note" markdown="1">
<summary>Alternativa: esptool en la línea de comandos</summary>

Si `esptool` se instala en tu ordenador, escribe los mismos archivos en las mismas direcciones.
Usa uno de los dos comandos de escritura:

```text
python -m pip install esptool
python -m esptool --chip esp32s3 write-flash 0x0 <environment>-<version>-merged.bin
python -m esptool --chip esp32s3 write-flash 0x10000 <environment>-<version>.bin
```

</details>

**Compílalo tú.** Los pasos numerados de abajo compilan el firmware desde el código fuente con
PlatformIO y lo suben por USB. Úsalos cuando no haya una imagen lista para lo que necesitas o
cuando cambies el código.

## 1. Instala uv {#1-install-uv}

El firmware se compila con [PlatformIO](https://platformio.org/). No lo instalas tú:
`uvx platformio ...` lo descarga y lo ejecuta a través de [uv](https://docs.astral.sh/uv/), una
pequeña herramienta para lanzar programas.

### Windows {#windows}

1. Abre PowerShell y ejecuta:

    ```powershell
    winget install --id=astral-sh.uv -e
    ```

2. Cierra PowerShell y vuelve a abrirlo para que encuentre `uv`.

No hace falta para flashear; se necesita para las pruebas en el ordenador y para «Adáptalo a tu
coche»: [Git for Windows](https://git-scm.com/download/win). Trae **Git Bash**, que ejecuta los
scripts de prueba y los comandos de esas páginas.

### Linux {#linux}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Para usar el puerto USB sin root, añádete al grupo del puerto serie (`dialout` en Debian y
Ubuntu, `uucp` en Arch) e instala las reglas udev de PlatformIO como se explica en la
[documentación de PlatformIO](https://docs.platformio.org/en/latest/core/installation/udev-rules.html).
Cierra la sesión y vuelve a entrar.

### macOS {#macos}

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

o, con Homebrew, `brew install uv`.

### Compruébalo {#check-it}

Abre un terminal nuevo y ejecuta:

```text
uvx platformio --version
```

Resultado esperado: una línea con la versión de PlatformIO, por ejemplo
`PlatformIO Core, version 6.2.0`. La primera llamada descarga PlatformIO y tarda un poco; las
siguientes arrancan al momento. Si ves «command not found» o «not recognized», cierra el terminal,
abre uno nuevo y vuelve a probar.

Si prefieres un editor, también sirve la extensión PlatformIO para VS Code: abre la carpeta
`firmware` y elige el entorno en la barra de estado. Los comandos de abajo son lo que la extensión
ejecuta por ti.

## 2. Descarga el código {#2-get-the-code}

1. Abre la página del proyecto (el repositorio donde encontraste esta guía) y elige
   **Code → Download ZIP**. Si usas Git, también sirve `git clone`.
2. Descomprime el ZIP. En Windows: clic derecho → **Extraer todo**.
3. Abre la carpeta descomprimida en la que ves `firmware`, `docs` y `README.md` juntos. Después
   de descomprimir suele estar un nivel más abajo, dentro de una carpeta con el mismo nombre.
4. Abre un terminal en la carpeta `firmware` de dentro:
    - **Windows:** abre `firmware` en el Explorador de archivos, haz clic en la barra de
      direcciones, escribe `powershell` y pulsa Intro. (En Windows 11 también sirve clic derecho
      en un espacio vacío → **Abrir en Terminal**.)
    - **Linux y macOS:** abre un terminal, escribe `cd ` (con un espacio), arrastra la carpeta
      `firmware` a la ventana y pulsa Intro.

Todos los comandos de abajo se ejecutan en este terminal, en la carpeta `firmware`.

## 3. Compila {#3-build}

```text
uvx platformio run -e listen_wifi
```

Este programa solo escucha al coche y tiene el panel web. Sin panel, usa `-e listen` aquí y en el
paso 5.

La primera compilación descarga la plataforma ESP32 (`espressif32@8.0.0`, fijada en
`platformio.ini`) y el compilador. Es una descarga grande y puede tardar muchos minutos; no la
interrumpas. Las compilaciones siguientes la reutilizan.

Resultado esperado: al final PlatformIO muestra `SUCCESS`. El programa queda en
`.pio/build/listen_wifi/firmware.bin`. Si ves `Error`, lee la primera línea de error: un error de
red significa que la descarga se cortó; ejecuta el comando otra vez.

### Qué entorno elegir {#which-environment}

Un **entorno** es un conjunto de opciones de compilación con nombre, en `firmware/platformio.ini`.
Decide qué tiene permitido hacer el dispositivo. En el escritorio basta con `listen_wifi`. Para
otro coche que no sea el de referencia, usa más adelante `listen_generic_wifi`.

<details class="note" markdown="1">
<summary>Los seis entornos, y compilarlos todos</summary>

Nunca cambies el modo editando `src/config.h`; elige el entorno.

| Entorno | Modo | Panel Wi-Fi y actualizaciones por Wi-Fi | Transmite en el bus del coche | Para qué sirve |
|---|---|---|---|---|
| `listen` (por defecto) | paralelo (junto a la caja) | no | no (el programa no tiene código de transmisión) | primer flasheo, banco, escuchar junto a la caja CAN |
| `decoder` | decodificador | no | no | sustituir la caja CAN, sin red |
| `listen_wifi` | paralelo (junto a la caja) | sí | no | escuchar con el panel web |
| `decoder_wifi` | decodificador | sí | no | sustituir la caja CAN, con el panel web |
| `decoder_tx_wifi` | decodificador | sí | **solo la trama `15B`, después de `cantx on` y un reinicio** | cambios de personalización desde la pantalla de la radio; lee antes [Transmitir en el bus CAN](../../en/safety/transmit.md) |
| `listen_generic_wifi` | paralelo (junto a la caja) | sí | no | otro coche: perfil `generic`, radio `none`, graba el bus y no envía nada a la radio |

El «modo paralelo» (junto a la caja) es el modo de solo escucha: el dispositivo trabaja al
lado de la caja CAN y no envía nada. La consola llama a este mismo modo `alongside the box`.

Un `uvx platformio run` sin más (sin `-e`) compila `listen`, el modo seguro sin red.

Para compilar los seis entornos de una vez, ejecuta `make build` en el directorio principal
(necesita `make`, que Git Bash no incluye; los comandos sencillos `uvx platformio run -e ...`
funcionan siempre).

Los ajustes opcionales que se quedan en tu ordenador van en
`firmware/platformio_override.ini` (copia `platformio_override.sample.ini`). El ejemplo
muestra una placa propia (`listen_mine`) y un perfil de coche propio (`listen_mycar`). La
trama de prueba `1A9` solo existe si añades ahí, a propósito,
`-DCAN_TX_EXPERIMENTAL=1` a una compilación de transmisión.

</details>

> [!WARNING]
> **Modo decodificador solo sin la caja CAN**
>
> Flashea un entorno `decoder*` solo con la caja CAN de posventa desconectada, o en el banco.
> Dos dispositivos en la línea de datos de la radio y dos fuentes en el cable ACC cortocircuitan
> las salidas entre sí.

## 4. Conecta la placa {#4-connect-the-board}

1. Conecta la placa al ordenador con un cable USB que lleve datos (no uno solo de carga). En
   placas con dos conectores USB usa el que va al propio ESP32-S3 (a menudo marcado `USB`, no
   `UART`): este firmware se comunica por el USB nativo del chip.
2. Lista los puertos:

    ```text
    uvx platformio device list
    ```

Resultado esperado: al conectar la placa aparece una entrada nueva. Windows 10 y 11 normalmente
no necesitan ningún controlador adicional para este puerto.

| Sistema | El puerto se ve así |
|---|---|
| Windows | `COM5` (cualquier número) |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem1101` (cualquier número) |

¿No aparece nada nuevo? Prueba otro cable. Si sigue sin aparecer, entra en el bootloader a mano,
consulta [Si la carga no empieza](#if-the-upload-does-not-start-bootloader-mode).

## 5. Sube el programa {#5-upload}

```text
uvx platformio run -e listen_wifi -t upload
```

PlatformIO encuentra el puerto solo. Si hay varias placas conectadas, indícalo:
`uvx platformio run -e listen_wifi -t upload --upload-port COM5`.

Resultado esperado: la carga termina con `[SUCCESS]`.

- «could not open port» o «Access is denied»: el puerto está ocupado. Cierra el monitor
  (Ctrl+C) en la otra ventana y vuelve a lanzar la carga.
- No encuentra el puerto: prueba otro cable y después el modo bootloader de abajo.

Una carga por USB conserva los ajustes guardados (volumen, tabla de botones, contraseña Wi-Fi).

### Si la carga no empieza: modo bootloader {#if-the-upload-does-not-start-bootloader-mode}

Normalmente la herramienta de carga reinicia el ESP32-S3 en su bootloader integrado a través del
USB. Si el puerto no aparece, o la carga se detiene con un error de conexión (por ejemplo, después
de un programa que se cuelga al arrancar), entra en el bootloader a mano:

1. Mantén pulsado el botón **BOOT** (en algunas placas marcado `0` o `IO0`).
2. Pulsa y suelta **RESET** (`RST` o `EN`), o conecta el cable USB mientras mantienes BOOT.
3. Suelta BOOT. El puerto vuelve a aparecer; ejecuta el comando de carga.
4. Después de la carga pulsa RESET una vez para que arranque el programa nuevo.

Son botones de la placa de desarrollo ESP32-S3, no de la placa portadora.

## 6. Mira la consola {#6-watch-the-console}

```text
uvx platformio device monitor
```

La velocidad (115200) sale de `platformio.ini`. Pulsa RESET en la placa para ver las líneas de
arranque. Se ven así (la primera línea empieza por el nombre del proyecto; `listen` muestra
`network=no`, `listen_wifi` `network=yes`):

```text
# <project name>. CAN 125 kbit/s LISTEN ONLY. Mode: alongside the box (sends nothing).
# profile: car=psa_aee2010 radio=xp
# version: <product> 2.0.0, built <date and time>, network=yes, volume=40%
# can=ok audio=ok radio=ok
# start: <reset reason>
```

RESET también desconecta el puerto USB un momento. Si el monitor se detiene o no ves las líneas de
arranque, vuelve a abrir el monitor y escribe `ver`.

> [!NOTE]
> **Lo que escribes no se ve**
>
> Las letras que escribes no aparecen en la pantalla. Es normal: el firmware no las devuelve.
> Escribe el comando a ciegas y pulsa Intro; la respuesta sale en una línea nueva.

Escribe `help` y pulsa Intro para ver la lista de comandos, `ver` para la versión y `stat` para el
estado. Sal del monitor con Ctrl+C.

Un `FAIL` en la línea `can=... audio=... radio=...` significa que un controlador no arrancó. El
dispositivo se queda entonces en su estado seguro (salidas de 12 V apagadas, nada enviado) y el LED
de la placa se pone rojo. No se reinicia en bucle.

## Borrarlo todo {#erasing-everything}

`uvx platformio run -e listen -t erase` borra toda la memoria flash, incluidos los ajustes
guardados. El siguiente arranque de la red genera una contraseña Wi-Fi nueva. Después vuelve a
subir un programa.

## Siguiente paso {#next}

- [Primer arranque](first-boot.md): lee la contraseña y abre el panel web.
- [Prueba en banco](bench-test.md): comprueba la placa antes del coche.
