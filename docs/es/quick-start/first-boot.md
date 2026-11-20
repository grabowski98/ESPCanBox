# Primer arranque

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Esta página explica cómo leer la contraseña del dispositivo y abrir el panel web en tu móvil o tu
portátil.

Trata de las compilaciones con red (`listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`,
`listen_generic_wifi`). Abren su propia red Wi-Fi con un panel web. Las compilaciones sin red
(`listen`, `decoder`) no contienen código Wi-Fi; con ellas solo existe la consola USB.

¿Aún no tienes placa? Salta a [Prueba el panel sin placa](#try-the-panel-without-a-board).

## 1. Flashea una compilación con red {#1-flash-a-build-with-network}

Si flasheaste `listen_wifi` como en la página [Flashear el firmware](flash.md) (una imagen lista
o tu propia compilación), pasa al paso 2.
Si no, en el terminal abierto en la carpeta `firmware` (como en esa página), ejecuta:

```text
uvx platformio run -e listen_wifi -t upload
uvx platformio device monitor
```

Deja la placa conectada al ordenador. Mientras haya un ordenador en el USB, el dispositivo
mantiene el Wi-Fi encendido, aunque no haya tráfico CAN.

## 2. Lee la contraseña en la consola USB {#2-read-the-password-from-the-usb-console}

En el primer arranque el dispositivo genera una contraseña aleatoria de 12 caracteres y la guarda.
La consola muestra:

```text
[wifi] device password generated; the command wifi showpass shows it
```

Puede que no veas esta línea: sale al arrancar, a menudo antes de que el monitor se conecte. No
pasa nada. En el monitor (lo que escribes no se ve; pulsa Intro después de cada comando):

1. Escribe `wifi`. Apunta el nombre de la red que aparece.
2. Escribe `wifi showpass`. La contraseña está en la línea `[wifi] password: <12 caracteres>`.

La contraseña es a la vez la **clave Wi-Fi** de la red del dispositivo y la **contraseña del
panel**. Solo se muestra en la consola USB, nunca en el panel. Si la olvidas, `wifi showpass` por
USB la vuelve a mostrar siempre.

> [!WARNING]
> **No dejes la contraseña en registros que compartas**
>
> La línea que muestra `wifi showpass` contiene la contraseña. No la pegues en un informe de
> error, en un mensaje de un foro ni en una grabación que compartas.

Comandos opcionales:

| Comando | Qué muestra o hace |
|---|---|
| `wifi` | estado de la red: su nombre, la dirección `http://10.50.8.1`, el nombre `.local`, los dispositivos conectados, la red doméstica |
| `wifi showpass` | la contraseña del dispositivo |
| `wifi pass <new password>` | pone tu propia contraseña: de 10 a 63 caracteres imprimibles, sin espacios |
| `wifi newpass` | una contraseña aleatoria nueva; la anterior deja de funcionar en todos los dispositivos |

Todos los comandos `wifi` funcionan solo desde la consola USB; el panel los rechaza.

## 3. Conéctate a la red del dispositivo {#3-connect-to-the-device-network}

1. En tu móvil o portátil abre la lista de redes Wi-Fi. El nombre de la red es el nombre del
   proyecto, un `-` y cuatro dígitos hexadecimales de la dirección MAC del chip
   (`<project name>-XXXX`). El comando `wifi` muestra el nombre exacto.
2. Conéctate con la contraseña del paso 2. Es una red WPA2.
3. Si el móvil pregunta si quieres seguir conectado a una red sin internet, elige seguir
   conectado.

Como máximo pueden estar conectados cuatro dispositivos a la vez (por ejemplo, la radio, tu móvil y un portátil).

> [!NOTE]
> **VPN y datos móviles**
>
> Una VPN en tu portátil o tu móvil puede enviar la dirección `10.50.8.1` fuera de la red Wi-Fi.
> Apaga la VPN mientras usas el panel. Si el panel no se abre en un móvil, apaga también un
> momento los datos móviles.

## 4. Abre el panel {#4-open-the-panel}

1. Abre `http://10.50.8.1` en el navegador. El nombre `.local` que muestra `wifi` también
   funciona en sistemas con mDNS.
2. Inicia sesión con la contraseña del dispositivo. Marca **Recordar en este dispositivo** solo en tu
   propio móvil o portátil: así una pestaña nueva abre el panel sin volver a pedir la contraseña.

El panel usa `http://` sin más, no `https://`; si tu navegador cambia a `https://` por su cuenta,
escribe la dirección con `http://` delante.

Resultado esperado: el panel muestra cinco pestañas: **Resumen**, **CAN**, **Radio**, **Consola**
y **Sistema**.

[![Resumen del panel web (dispositivo simulado, tema oscuro)](../../assets/screenshots/panel-dashboard.png)](../../assets/screenshots/panel-dashboard.png)

## 5. Elige el idioma {#5-choose-the-language}

El selector de idioma está en la pantalla de inicio de sesión y en la pestaña **Sistema**. El panel
arranca en inglés. Tu elección se guarda en el dispositivo, así que otro móvil abre el panel en el
mismo idioma, también la pantalla de inicio de sesión. La página del panel solo incluye el inglés;
otro idioma se descarga del dispositivo al elegirlo y, hasta entonces, el panel se muestra en
inglés.

Disponibles: inglés, polaco, alemán, español, ruso y ucraniano. El alemán, el español, el ruso y el
ucraniano son traducciones automáticas; la pestaña Sistema lo indica mientras esperan revisión.
Las líneas de la consola, los identificadores de trama y el formato `can0 ID#DATA` no se traducen
nunca.

## Cuándo está encendido el Wi-Fi {#when-wi-fi-is-on}

En el escritorio, deja la placa conectada a un ordenador: así el Wi-Fi sigue encendido todo el
tiempo. Con un cargador USB o una batería externa, el Wi-Fi se apaga 60 s después del arranque si
no hay tráfico CAN y no usas el panel.

En el coche el dispositivo está conectado a 12 V permanentes, así que no mantiene el Wi-Fi
encendido todo el tiempo. El Wi-Fi está encendido:

- mientras haya tráfico en el bus CAN o la radio tenga ACC;
- mientras haya un ordenador conectado al USB;
- durante un rato después de que el bus se quede en silencio, si el panel está en uso: hasta 60 s
  después de la última petición de un cliente con sesión iniciada y como máximo 10 minutos en
  total, y solo si la tensión de alimentación es suficiente (cuando la placa la mide);
- durante los primeros 60 s después de un arranque con el bus en silencio, para que te dé tiempo a
  conectarte.

`wifi off` apaga la red y la mantiene apagada tras un reinicio; `wifi on` la vuelve a encender.

## Opcional: tu red doméstica {#optional-your-home-network}

El dispositivo también puede conectarse a tu Wi-Fi de casa, para abrir el panel desde tu red
doméstica:

```text
wifi sta <network name>|<password>
```

La primera `|` separa el nombre de la contraseña; la contraseña es obligatoria (de 8 a 63
caracteres, o 64 dígitos hexadecimales como clave WPA2 ya preparada). `wifi sta off` olvida la
red. El dispositivo intenta conectarse cada 30 s (cada 5 minutos mientras alguien esté conectado a
su propia red), y nunca con el motor en marcha.

Desde la red doméstica el panel es de **solo lectura** por defecto. `wifi lan on` permite cambios
también desde la red doméstica; `wifi lan off` los vuelve a bloquear. Armar la compilación de
transmisión CAN no se permite nunca desde la red doméstica, solo desde la red propia del
dispositivo o por USB.

## Prueba el panel sin placa {#try-the-panel-without-a-board}

`firmware/tools/mock_device.py` simula el dispositivo: reproduce una grabación CAN y responde a las
mismas peticiones HTTP que el firmware, con la misma página del panel. Solo necesita Python 3.

En el terminal abierto en la carpeta `firmware`, ejecuta (en Linux y macOS escribe `python3` en
lugar de `python`):

```text
python -I tools/mock_device.py test_data/captures/can_20261111_191045.log
```

Muestra la dirección y una contraseña aleatoria del simulador:

```text
Device mock: http://127.0.0.1:8080/  (frames in the capture: ..., VIN masked)
mock password: ...
```

1. Abre `http://127.0.0.1:8080/`. Resultado esperado: la pantalla de inicio de sesión.
2. Inicia sesión con la contraseña del simulador. Resultado esperado: el Resumen con tráfico de la
   grabación.
3. No cierres la ventana del terminal mientras uses el panel. Detén el simulador con Ctrl+C.

Usa una grabación con tráfico desde el principio, como `can_20261111_191045.log`; una grabación
que empieza con el bus en silencio no muestra tráfico durante ese tiempo.

Opciones útiles:

| Opción | Efecto |
|---|---|
| `--password <text>` | tu propia contraseña del simulador (de 10 a 63 caracteres) |
| `--port 8081` | otro puerto |
| `--mode decoder` | se comporta como una compilación de decodificador |
| `--lan` | se comporta como si estuvieras en la red doméstica (solo lectura, bloqueo de inicio de sesión) |
| `--lan --lan-admin` | como después de `wifi lan on` |
| `--speed 2` | reproduce el doble de rápido |
| `--no-loop` | no repite la grabación |

El simulador simplifica algunas cosas (por ejemplo, el zumbador y la decodificación de botones);
la lista está al principio del script.

## Siguiente paso {#next}

[Prueba en banco](bench-test.md): comprueba la placa antes de que vaya al coche.
