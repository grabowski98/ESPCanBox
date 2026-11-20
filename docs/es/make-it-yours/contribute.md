# Comparte tu coche

> [!NOTE]
> **Traducción automática, pendiente de revisión**
>
> Esta página se ha traducido automáticamente. El texto vinculante es la página en inglés.
> Si encuentras un error, consulta [cómo enviar una corrección](../../en/contributing/translations.md#send-your-work).

Has medido un coche que no está en `vehicles/SUPPORTED.md`. Tus notas ayudan a la próxima persona
con el mismo coche, incluso sin un perfil de firmware. Esta página explica qué compartir, cómo
limpiarlo antes y dónde va cada archivo. Las reglas generales para contribuir están en
`CONTRIBUTING.md`, en la raíz del repositorio.

Es un proyecto de aficionado con un solo mantenedor. No hay ninguna promesa de respuesta ni de
revisión en un plazo concreto.

## Qué puedes compartir {#what-you-can-share}

| Contribución | Dónde va |
|---|---|
| Una descripción del coche y de su bus | `vehicles/<make>-<model>-<year>/README.md`, a partir de `vehicles/_template/README.md` |
| Tramas y señales | `vehicles/<make>-<model>-<year>/<car>.dbc`, cada comentario empieza por su estado |
| Vistas de conectores y tablas de pines | `vehicles/<make>-<model>-<year>/wiring.md`, indicando desde qué lado miraste |
| Grabaciones que usa una prueba | `firmware/test_data/captures/`, con una fila en su `README.md` |
| Un perfil de firmware | `firmware/lib/car_<name>/` ([Escribe un perfil de coche](write-a-profile.md)) |
| Su prueba en el PC | `firmware/test_host/<name>_car.cpp`, la referencia en `firmware/test_host/expected/`, un bloque en `firmware/tools/run_host_tests.sh` ([Prueba tu perfil](test-your-profile.md)) |
| Qué hace tu radio con cada código de botón | la página de tu vehículo, sección "Radio side" ([Asigna la radio](radio-mapping.md)) |
| Una fila para la lista de compatibles | `vehicles/SUPPORTED.md`: vehículo, perfil, estado, qué funciona, enlace a tu página |

Un pull request con solo la página del vehículo es bienvenido. También un issue con la plantilla
"New vehicle", si no usas Git.

## Las reglas para los datos {#the-rules-for-facts}

- Cada dato tiene un estado: **medido** (measured; cómo: multímetro, grabación, consola del
  dispositivo; para una grabación, el archivo y el paso) o **sin verificar** (unverified; la
  fuente: una base de datos pública, un foro, un dibujo del vendedor).
- Una conclusión no es una medición. "Las tensiones parecen de CAN de baja velocidad tolerante a
  fallos" es una conclusión; di qué la confirmaría.
- Escribe solo lo que viste. "No medido" es una entrada válida.
- Solo tu propio trabajo: tus grabaciones, fotos y palabras. No envíes código descompilado,
  firmware ni aplicaciones de fabricantes de cajas o radios, tablas sacadas de ellos, hojas de
  datos ni código copiado de proyectos con una licencia incompatible. Enlaza una fuente pública en
  lugar de copiarla.

## Limpia antes tus grabaciones {#clean-your-recordings-first}

Una grabación del bus puede contener el VIN de tu coche, y muestra cuándo y cómo condujiste.
Limpia cada archivo antes de que salga de tu ordenador.

### 1. Encuentra las tramas que llevan el VIN {#1-find-the-frames-that-carry-the-vin}

En el coche de referencia el VIN va en las tramas `336`, `3B6` y `2B6` (medido: grabaciones del
coche de referencia). Esos tres ID son los valores por defecto de la herramienta de limpieza y de
`record.py`. **En otra marca el VIN puede ir en otras tramas, o no estar en este bus.**
Compruébalo antes de compartir:

1. Toma tu VIN del permiso de circulación. No lo escribas en ningún archivo que vayas a compartir.
2. El VIN tiene 17 letras y cifras. Convierte los tres primeros caracteres en códigos ASCII
   hexadecimales: por ejemplo `V` = `56`, `F` = `46`, `3` = `33`.
3. Busca esos bytes en tu grabación con un editor de texto, y después los códigos hexadecimales de
   los demás caracteres. Toda trama cuyos datos formen una parte del VIN es una trama de VIN.
   Guardados como texto, 17 caracteres no caben en una trama de 8 bytes, así que espera más de una.
   Si nada coincide, puede que el coche no envíe el VIN como texto en este bus; dilo en la página
   de tu vehículo.
4. Apunta los ID. Si no encuentras ninguno, escribe "VIN frames: none found" en la página de tu
   vehículo.

### 2. Sobrescríbelas {#2-overwrite-them}

Desde la raíz del repositorio:

```bash
python -I tools/sanitize_capture.py my_drive.log my_drive_clean.log --ids 336,3B6,2B6,ABC
python -I tools/sanitize_capture.py --check my_drive_clean.log --ids 336,3B6,2B6,ABC
```

Sustituye `ABC` por los ID de tus tramas de VIN (deja los tres valores por defecto; no hacen
daño). La herramienta sobrescribe los datos de esas tramas con ceros de la misma longitud: el
número de líneas, los tiempos, los ID y las longitudes quedan igual, así que las pruebas siguen
funcionando. También sustituye por `# capture` una línea de cabecera que nombre tu puerto serie.
`--check` debe indicar `0 problem(s)`.

### 3. Lee tú mismo los comentarios {#3-read-the-comments-yourself}

La herramienta copia las líneas de comentario (`# ...`) tal cual. En ellas están los textos de tus
marcadores. Abre el archivo y busca nombres, lugares, direcciones, nombres de redes o cualquier
otra cosa privada.

### 4. Revisa todo lo que añadas {#4-scan-everything-you-add}

```bash
python -I tools/scan_private.py .
```

Busca VIN, tramas de VIN con datos, direcciones de correo electrónico, rutas de usuario de Windows,
direcciones MAC y nombres de la red del dispositivo con un sufijo real. Debe terminar sin errores.

> [!WARNING]
> **El escáner solo conoce el formato de VIN del coche de referencia**
>
> `scan_private.py` reconoce un VIN del grupo PSA (`VF3` + 14 caracteres) y las tramas de VIN de
> PSA. No conoce el VIN de otra marca ni tus propias tramas de VIN. Los pasos 1 a 3 siguen siendo
> tarea tuya.

## Antes de abrir un pull request {#before-you-open-a-pull-request}

La plantilla del pull request tiene una lista de comprobación. Para un coche nuevo, estas son las
partes que importan:

| Comprobación | Comando | Resultado esperado |
|---|---|---|
| Grabaciones limpias | `python -I tools/sanitize_capture.py --check <files> --ids ...` | `0 problem(s)` |
| Sin datos privados | `python -I tools/scan_private.py .` | sin errores |
| Pruebas en el PC (desde `firmware/`) | `bash tools/run_host_tests.sh` | `All tests match the references.` |
| Capas (desde `firmware/`) | `python -I tools/check_layering.py` | `Layering OK.` |
| Tu compilación (desde `firmware/`) | `uvx platformio run -e listen_mycar` | `SUCCESS` |
| Enlaces de la documentación, si cambiaste `docs/` | `python -I tools/check_doc_links.py` | sin enlaces rotos |

Tu `platformio_override.ini` se queda en tu ordenador: Git no hace seguimiento de él. Indica en el pull request
qué entorno de compilación usaste para tu coche y si probaste el firmware en una placa y en el
coche.

No cambies las referencias de prueba existentes (`firmware/test_host/expected/`, `golden/`,
`golden_060/`) en un pull request que añade un coche. Añadir tu propio archivo de referencia sí
está bien.

## La transmisión no forma parte de una contribución de coche {#transmission-is-not-part-of-a-car-contribution}

Un perfil compilado con `-DCAR_PROFILE=CUSTOM` no puede transmitir, y el firmware no tiene forma de
añadir una trama de transmisión en tiempo de ejecución. Una trama que el dispositivo pueda enviar
necesita su propia medición, una comprobación del contenido escrita en C++, una prueba en el PC y
un perfil con nombre en `firmware/src/config.h`. Es una discusión aparte, no parte de añadir un
coche: lee [Transmitir en el bus CAN](../../en/safety/transmit.md) y abre antes un issue.

## Licencias {#licences}

Al enviar una contribución aceptas que se publique bajo las licencias de este repositorio: código
Apache-2.0, documentación CC BY 4.0, hardware CERN-OHL-S-2.0. Qué licencia cubre qué archivos
está en `README.md`, sección «Licences».

Este proyecto no está vinculado con ningún fabricante de coches, de radios ni de cajas CAN. Nómbralos
solo para decir qué probaste.
