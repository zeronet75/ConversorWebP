# Conversor WebP 2.3 · Manual de uso

Manual completo del programa: qué hace cada opción y cómo sacarle partido.

> Versión navegable del manual incluido en el repositorio ([`MANUAL_DE_USO.html`](../MANUAL_DE_USO.html)).

## Índice

- [1. Qué es y qué hace](#1-qué-es-y-qué-hace)
- [2. Primeros pasos en 5 minutos](#2-primeros-pasos-en-5-minutos)
- [3. Sección 1 · Origen](#3-sección-1--origen)
- [4. Sección 2 · Carpeta de salida](#4-sección-2--carpeta-de-salida)
- [5. Sección 3 · Calidad y opciones](#5-sección-3--calidad-y-opciones)
- [6. Botones de acción, progreso y registro](#6-botones-de-acción-progreso-y-registro)
- [7. Arrastrar y soltar](#7-arrastrar-y-soltar)
- [8. Comprimidos .zip, .rar y .7z](#8-comprimidos-zip-rar-y-7z)
- [9. Nombres de archivo y duplicados](#9-nombres-de-archivo-y-duplicados)
- [10. Qué se conserva al convertir](#10-qué-se-conserva-al-convertir)
- [11. Modo consola](#11-modo-consola)
- [12. Dónde se guardan los ajustes](#12-dónde-se-guardan-los-ajustes)
- [13. Preguntas frecuentes](#13-preguntas-frecuentes)
- [14. Solución de problemas](#14-solución-de-problemas)
- [15. Automatización, extras y distribución](#15-automatización-extras-y-distribución)

---

# Conversor WebP 2.5.1

**Novedades de la versión 2.5.1:** tooltips explicativos, HEIC, carpeta vigilada, patrones de nombre, solo analizar, tabla de resultados, vista previa, menú contextual, instalador y portable, detección de GPU, presets de calidad, redimensionado, peso máximo por imagen, historial de trabajos, selección múltiple, atajos de teclado, conversión en paralelo, informe CSV, formato AVIF y acceso directo en **Enviar a** del Explorador.

## 1. Qué es y qué hace

**Conversor WebP** es una aplicación de escritorio para Windows que transforma imágenes **JPG, PNG, BMP, TIFF, GIF, ICO, TGA, PCX, DDS y PPM/PGM/PBM** al formato **WebP**, el formato de imagen moderno que pesa entre un 25 % y un 85 % menos que JPG o PNG con una calidad visual equivalente.

Todo el trabajo se hace **en tu equipo**: el programa no envía archivos a internet, no pide cuenta y no modifica ni borra los originales.

Opcionalmente puede generar **AVIF**, un formato aún más eficiente, y un **informe CSV** con el peso de cada archivo antes y después.

Puede trabajar con **imágenes sueltas, carpetas enteras o archivos comprimidos** (`.zip`, `.rar`, `.7z`), guardar el resultado en la carpeta que elijas y, si quieres, entregarlo todo empaquetado en un **ZIP**.

## 2. Primeros pasos en 5 minutos

1.  Abre `ConversorWebP.exe` (no hace falta instalar Python ni nada más).
2.  Arrastra tus imágenes, una carpeta o un `.rar` desde el Explorador de Windows y suéltalas sobre la ventana. También puedes usar los botones.
3.  Pulsa **Examinar…** y elige la carpeta donde quieres los `.webp`.
4.  Deja la calidad en **85** (buen equilibrio) y, si quieres el resultado en un solo archivo, marca **Empaquetar el resultado en .zip**.
5.  Pulsa **Convertir a WebP**. Al terminar verás el resumen y podrás abrir la carpeta con **Abrir carpeta de salida**.

## 3. Sección 1 · Origen

Es la lista de lo que se va a convertir. Puedes mezclar libremente imágenes, carpetas y comprimidos.

| Elemento | Qué hace |
|----|----|
| **Archivos…** | Abre el explorador para elegir **una o varias imágenes** (mantén `Ctrl` para seleccionar sueltas o `Shift` para un rango). |
| **Carpeta…** | Añade una carpeta completa. **Incluye automáticamente todas las subcarpetas** y solo toma archivos de imagen. |
| **.zip / .rar…** | Añade uno o varios comprimidos. El programa los abre en una carpeta temporal, convierte lo que hay dentro y luego borra esa carpeta temporal. |
| **Quitar todo** | Vacía la lista de origen sin tocar ningún archivo del disco. |
| **Quitar** | Elimina de la lista solo las entradas seleccionadas. Puedes seleccionar varias con `Ctrl` o `Shift` y borrarlas también con la tecla `Supr`. |
| **Zona punteada** | Zona de arrastre: suelta ahí lo que quieras convertir. Hacer clic en ella equivale a **Archivos…**. |
| **Lista central** | Muestra cada origen añadido con la etiqueta `[carpeta]` o `[comprimido]`. |
| **Contador** | Resume cuántas carpetas, comprimidos e imágenes has añadido (p. ej. «Origen: 2 carpeta(s), 1 comprimido(s), 5 imagen(es)»). |
| **Extractor** | Indica qué herramienta se usará para abrir `.rar`/`.7z` (p. ej. «Extractor: 7z.exe»). Si dice «Sin 7-Zip/WinRAR: solo .zip», los ZIP siguen funcionando pero los RAR no. |

**Truco:** si eliges primero la carpeta de salida, el programa la recuerda para la próxima vez. Si no la eliges, propone una automáticamente (`nombre_webp` junto al origen).

## 4. Sección 2 · Carpeta de salida

Es la carpeta donde se escribirán los `.webp`. Debe existir o se creará sola; antes de empezar, el programa comprueba que realmente se puede escribir en ella y avisa si no.

| Opción | Qué significa |
|---|---|
| **Examinar…** | Elige la carpeta de destino con el diálogo de Windows. |
| **Arrastrar una carpeta sobre el campo** | Si sueltas una única carpeta encima del campo de texto, se toma como carpeta de salida. |
| **Mantener subcarpetas del origen** *(activado por defecto)* | **Activado:** reproduce la estructura de carpetas original (`fotos/viaje/playa.jpg` → `salida/viaje/playa.webp`). **Desactivado:** todos los WebP quedan sueltos en la carpeta de salida (útil para tener una galería plana). |
| **Sobrescribir archivos existentes** *(activado por defecto)* | **Activado:** si ya existe un archivo con ese nombre en el formato elegido, se reemplaza. **Desactivado:** si ya existe, se omite y se cuenta como «omitida». Ideal para reanudar una conversión larga sin repetir trabajo. |
| **Formato de salida** | **WebP** (por defecto) o **AVIF**. AVIF es más moderno y suele pesar entre un 20 % y un 30 % menos con calidad similar, a cambio de tardar algo más en convertirse. El botón principal y la extensión de los archivos se adaptan al formato elegido. |
| **Guardar informe CSV** | Escribe en la carpeta de salida un archivo `informe_AAAAMMDD_HHMMSS.csv` con una fila por imagen: archivo generado, nombre original, KB de entrada, KB de salida, ahorro, estado y nota. Se abre directamente en Excel (separador «;» y codificación UTF-8). |
| **Empaquetar el resultado en .zip** *(activado por defecto)* | Crea además un ZIP con **todos los WebP generados** dentro de la misma carpeta de salida. Los archivos sueltos también quedan ahí: el ZIP es una copia empaquetada, no un reemplazo. |
| **Nombre del ZIP** | Campo de texto a la derecha de la casilla. Si no pones extensión, se añade `.zip` automáticamente. Por defecto se propone `webp_AAAAMMDD_HHMM.zip`. |

El ZIP conserva las subcarpetas tal y como quedaron en la carpeta de salida, así que al descomprimirlo mantiene el mismo orden.

## 5. Sección 3 · Calidad y opciones

### Preset

Un preset es una combinación guardada de opciones que se aplica de golpe. El desplegable trae cuatro incluidos:

| Preset | Qué aplica |
|----|----|
| **Web (85 · 1920 px)** | Calidad 85, esfuerzo 6, lado máximo 1920 px. El equilibrio típico para publicar. |
| **Catálogo (80 · 1200 px)** | Calidad 80 y 1200 px. Listados y fichas de producto. |
| **Iconos sin pérdida** | Modo lossless sin redimensionar. Logos, iconos y capturas. |
| **Máxima calidad (95)** | Calidad 95 sin redimensionar ni límite de peso. |

Si tocas cualquier control después de aplicar un preset, el desplegable vuelve a **Personalizado**, que refleja la configuración actual.

**Guardar:** pide un nombre y almacena las opciones actuales como preset propio, disponible para siempre. **Borrar:** elimina el preset propio seleccionado (los cuatro incluidos no se pueden borrar).

### Calidad (1 – 100)

Controla cuánto detalle se conserva. Más calidad = mejor aspecto y más peso. La barra muestra el valor numérico a la derecha. Referencias prácticas:

| Valor | Uso recomendado | Resultado típico |
|----|----|----|
| **95 – 100** | Archivo fotográfico, impresión, originales que se van a seguir editando | Máxima fidelidad, peso alto |
| **85 – 92** | Fotos para web, catálogos, blogs | Indistinguible del original en pantalla |
| **75 – 84** | Miniaturas, listados, correo | Muy buen equilibrio peso/calidad |
| **50 – 74** | Vistas previas, cargas rápidas en móvil | Peso mínimo, se notan artefactos al ampliar |

El valor por defecto, **85**, es el que suele recomendarse para publicar en web.

### Esfuerzo (0 – 6)

Cuánto trabaja el compresor para conseguir el mismo peso. Con **6** (por defecto) los archivos salen algo más pequeños, a costa de tardar más. Con **0** convierte muy rápido pero pesa más. Para un lote grande de una sola vez, 6 merece la pena; para pruebas rápidas, 4.

### Sin pérdida (lossless)

Comprime sin descartar ningún píxel: la imagen es idéntica al original. El archivo resultante suele ser **más grande que un JPG**, así que solo tiene sentido para capturas de pantalla, gráficos, logos o imágenes con texto. Al activarlo, el control de calidad y el de esfuerzo se deshabilitan porque no intervienen.

### Recodificar también los .webp

Por defecto los archivos que ya son `.webp` se ignoran (no tiene mucho sentido volver a comprimirlos). Si lo activas, se vuelven a procesar con los ajustes actuales: útil para bajar la calidad de una carpeta ya convertida o para cambiar de con pérdida a sin pérdida.

### Aplanar transparencia sobre…

Los PNG con fondo transparente se guardan como WebP con canal alfa (la transparencia se conserva). Si activas esta casilla, la transparencia se «rellena» con un color sólido, que eliges con el cuadro de color de al lado (blanco por defecto). Sirve para:

- Evitar bordes raros en visores o programas antiguos que no entienden el alfa.
- Publicar en sitios que no admiten PNG transparentes.
- Reducir aún más el peso cuando el fondo da igual.

### Lado máximo

Reduce la imagen para que su lado más largo no supere el valor elegido (2560, 1920, 1600, 1200, 800 o 512 px). **Nunca amplía**: una foto de 300 px se queda en 300 px. Es la forma más eficaz de bajar peso en fotos de móvil o cámara, porque la resolución sobrante no aporta nada en pantalla.

### Peso máximo (KB)

Al activarlo, el programa busca automáticamente la calidad más alta que deje cada imagen por debajo del peso indicado. La calidad del deslizador actúa como **tope superior**: nunca usará una calidad mayor que esa. Si ni con calidad 1 se consigue el objetivo, guarda la imagen igualmente y lo avisa en el registro (en ese caso conviene bajar el «Lado máximo»). El modo sin pérdida no admite peso máximo, porque no se puede ajustar la calidad sin perder fidelidad.

El peso máximo es aproximado y por imagen: se comprueba el archivo real generado, así que el resultado siempre queda en o por debajo del límite (salvo el caso imposible citado).

### Aceleración GPU (experimental)

El programa detecta la tarjeta gráfica al arrancar y muestra su nombre junto a la casilla **Acelerar con GPU (CUDA)**. El modo CUDA está **desactivado por defecto**: en las pruebas reales la CPU en paralelo es más rápida para lotes, porque el codificador WebP/AVIF es de CPU y la GPU solo puede decodificar JPEG y reescalar.

| Escenario medido (RTX 3060) | CPU | GPU |
|----|----|----|
| 1 foto de 24 MP reducida a 1920 px | 669 ms | 435 ms (1,5×) |
| 60 fotos de 24 MP reducidas a 1600 px (app completa) | **12,5 s** | 18,0 s |

Si la activas, solo se aplica a **JPEG RGB de 8 MP o más que se vayan a reducir**. En lotes pequeños ni siquiera se carga CUDA salvo que marques *Herramientas → Forzar GPU en lotes pequeños*. Cualquier fallo de CUDA revierte a CPU para esa imagen.

El ejecutable no incluye PyTorch (ocuparía ~2,4 GB): usa el que ya esté instalado en el equipo. En *Herramientas → Aceleración GPU* puedes ver el estado y elegir la carpeta que lo contiene.

## 6. Botones de acción, progreso y registro

| Elemento | Qué hace |
|----|----|
| **Convertir a WebP** | Comprueba los datos, crea la carpeta de salida si falta y arranca la conversión. |
| **Cancelar** | Detiene el proceso después del archivo en curso. Lo ya convertido se conserva. |
| **Abrir carpeta de salida** | Abre el Explorador de Windows en la carpeta de destino. |
| **Aviso sonoro al terminar** | Pitido al finalizar: dos tonos agudos si todo salió bien y dos graves si hubo errores o cancelaste. Se puede desactivar con la casilla que está a la derecha de los botones. |
| **Barra de progreso** | Archivos procesados sobre el total y porcentaje. Si hay comprimidos, primero indica «Extrayendo…». |
| **Registro** | Detalle línea a línea. Colores: **verde** = convertida, **rojo** = error, **ámbar** = aviso, **gris** = omitida/informativa. |

Cada línea muestra el nombre final, el tamaño original y el obtenido, por ejemplo: `OK viaje/playa.webp 1.204,3 KB -> 386,1 KB`.

Los lotes de 4 o más imágenes se convierten en paralelo usando varios núcleos del procesador, así que un pack grande termina antes. El orden del registro puede variar por eso; el resultado es el mismo.

### Atajos de teclado

| Atajo | Acción |
|----|----|
| `Ctrl` + `O` | Añadir archivos |
| `Ctrl` + `Mayús` + `O` | Añadir una carpeta |
| `Ctrl` + `Enter` o `F5` | Iniciar la conversión |
| `Supr` | Quitar las entradas seleccionadas de la lista |
| `F1` | Abrir este manual |

### Menú Historial

Guarda los **últimos 10 trabajos** (origen, carpeta de salida y ajustes). Al elegir una entrada se recupera todo y solo hay que pulsar **Convertir**. Si un origen ya no existe, se avisa y se cargan los que queden. Desde el mismo menú puedes vaciar el historial.

## 7. Arrastrar y soltar

Puedes soltar contenido **en cualquier punto de la ventana**, no solo en la zona punteada:

- **Imágenes sueltas** o **varias seleccionadas** desde una carpeta.
- **Carpetas completas** (se recorren todas las subcarpetas).
- **Archivos comprimidos** `.zip`, `.rar` y `.7z`.
- **Mezclas** de todo lo anterior en un mismo gesto.

Mientras arrastras, la zona punteada se ilumina en azul y el cursor indica que se puede soltar. Si sueltas algo que no es imagen ni comprimido (un PDF, un vídeo…), el programa lo ignora y te avisa de cuántos descartó.

### Enviar a del Explorador de Windows

En **Herramientas → Añadir a «Enviar a» del Explorador** se crea un acceso directo en el menú contextual de Windows. Después puedes seleccionar fotos o un comprimido, hacer clic derecho → **Enviar a → Conversor WebP** y la aplicación se abre con esos archivos ya cargados, lista para elegir destino y convertir. Con **Quitar de «Enviar a»** se elimina el acceso.

## 8. Comprimidos .zip, .rar y .7z

Los `.zip` se abren con un lector interno, sin depender de nada instalado. Para `.rar` y `.7z` se usa **7-Zip** o **WinRAR** si están instalados; el programa los busca solo en las rutas habituales de Windows.

- El contenido se extrae a una carpeta temporal y **se borra al terminar**.
- Se respeta la estructura interna del comprimido.
- Si añades **varios comprimidos**, cada uno se guarda dentro de una subcarpeta con su nombre para que no se mezclen los contenidos.
- Si un comprimido está protegido con contraseña, el programa mostrará un error en el registro.
- Los comprimidos dentro de otro comprimido no se abren: extrae primero el exterior.

**Sin 7-Zip ni WinRAR** los `.rar` no se pueden abrir. Instala 7-Zip (gratuito, `https://www.7-zip.org`) y la aplicación lo detectará automáticamente en el siguiente arranque.

## 9. Nombres de archivo y duplicados

- Se conserva el nombre original y solo cambia la extensión: `foto.jpg` → `foto.webp`.
- Los nombres con espacios, acentos o `ñ` se mantienen tal cual.
- Si dos originales generarían el mismo nombre (por ejemplo `foto.jpg` y `foto.png`), el segundo recibe un sufijo con su extensión original: `foto.webp` y `foto_png.webp`. Nunca se pisa un archivo distinto.
- Si dos comprimidos contienen el mismo nombre de archivo, se aplica la misma regla.

## 10. Qué se conserva al convertir

| Aspecto | Comportamiento |
|----|----|
| **Orientación de la foto** | Se aplica la etiqueta EXIF: las fotos hechas con el móvil en vertical no salen giradas. |
| **Transparencia** | Se conserva el canal alfa de PNG, GIF y WebP (salvo que se active «aplanar»). |
| **Perfil de color** | Si la imagen lleva perfil ICC incrustado, se mantiene. |
| **GIF animados** | Se convierte el **primer fotograma** (WebP animado no se genera); el registro lo avisa. |
| **Metadatos EXIF** | No se copian (permite archivos más ligeros y evita publicar datos como la ubicación GPS). |
| **Originales** | Nunca se modifican ni se borran. |

## 11. Modo consola

El mismo ejecutable puede usarse sin ventana, por ejemplo en un script o una tarea programada:

    ConversorWebP.exe <origen...> -o <carpeta_salida> [opciones]

| Opción | Significado |
|----|----|
| `-o, --output` | Carpeta de salida (obligatoria). |
| `-q, --quality` | Calidad 1–100. Por defecto 85. |
| `-m, --method` | Esfuerzo 0–6. Por defecto 6. |
| `--zip NOMBRE` | Crea un ZIP con el resultado dentro de la carpeta de salida. |
| `--format webp|avif` | Formato de salida. Por defecto `webp`. |
| `--max-dim N` | Redimensiona al lado máximo de N píxeles (no amplía). |
| `--max-kb N` | Busca la calidad necesaria para no superar N KB por imagen. |
| `--csv` | Genera el informe CSV en la carpeta de salida. |
| `--gpu` | Fuerza la GPU CUDA aunque el lote sea pequeño. |
| `--no-gpu` | Desactiva la aceleración GPU. |
| `--lossless` | Modo sin pérdida. |
| `--flat --bg "#ffffff"` | Aplana la transparencia sobre ese color. |
| `--no-recursive` | No mantiene subcarpetas (todo plano). |
| `--no-overwrite` | Omite los WebP que ya existan. |
| `--webp` | Recodifica también los `.webp` existentes. |

Ejemplos:

    ConversorWebP.exe "C:\fotos\vacaciones.rar" -o "C:\fotos\webp" --zip vacaciones.zip
    ConversorWebP.exe "C:\capturas" -o "C:\capturas\webp" -q 95 --webp
    ConversorWebP.exe "C:\logos\logo.png" -o "C:\logos" --lossless

Sin argumentos, el ejecutable abre siempre la interfaz gráfica.

## 12. Dónde se guardan los ajustes

La última carpeta de salida y todas las opciones (calidad, esfuerzo, casillas, nombre de ZIP…) se guardan en:

    %APPDATA%\ConversorWebP\config.json

Si quieres volver a la configuración de fábrica, cierra el programa y borra ese archivo.

## 13. Preguntas frecuentes

### ¿Pierdo calidad al convertir?

Con calidad 85 la pérdida es imperceptible en pantalla y el archivo pesa mucho menos. Si necesitas fidelidad absoluta, activa **Sin pérdida**.

### ¿Puedo convertir cientos de archivos a la vez?

Sí. Una carpeta entera o un comprimido con miles de imágenes funcionan igual. Verás el progreso y podrás cancelar en cualquier momento; lo ya convertido se conserva.

### ¿Se borran los JPG originales?

No, nunca. El programa solo escribe archivos nuevos `.webp` en la carpeta de salida.

### ¿Funciona sin internet?

Sí, todo el proceso es local. No se descarga nada ni se envía nada a ningún servidor.

### ¿Por qué el ZIP del resultado pesa casi lo mismo que los WebP?

WebP ya está comprimido; el ZIP aporta todo en un solo archivo y un pequeño ahorro adicional, pero no puede comprimir mucho más.

### ¿Qué pasa si la misma imagen está en dos comprimidos?

Se convierten las dos y la segunda recibe un sufijo para no pisar la primera.

### ¿WebP o AVIF?

WebP tiene soporte universal y es la opción segura. AVIF comprime mejor (20-30 % menos peso con la misma calidad), pero tarda más en convertirse y algunos programas antiguos no lo abren. Para web moderna, AVIF es una buena elección; para máxima compatibilidad, WebP.

### ¿Por qué una imagen no baja del peso máximo que puse?

Porque ni con la calidad mínima se consigue. En ese caso reduce el «Lado máximo» (por ejemplo a 1600 px) y vuelve a intentarlo; el propio registro indica el mínimo alcanzado.

### ¿El informe CSV se puede abrir en Excel?

Sí. Se guarda con separador «;» y UTF-8 con BOM, así que Excel en español lo abre en columnas y respeta acentos y eñes.

### ¿Puedo usar rutas de red o unidades externas?

Sí, siempre que Windows pueda leer y escribir en ellas. Si la unidad se desconecta durante el proceso verás un error en el registro.

## 14. Solución de problemas

| Síntoma | Causa y solución |
|----|----|
| «Sin 7-Zip/WinRAR: solo .zip» | No se encontró ningún extractor. Instala 7-Zip y vuelve a abrir la aplicación. |
| «No se puede escribir en la carpeta de salida» | La ruta es de solo lectura, está en uso o no tienes permisos. Elige otra carpeta. |
| Windows muestra un aviso al abrir el .exe | Es habitual en programas nuevos sin firma digital. Pulsa «Más información → Ejecutar de todas formas». El ejecutable es autocontenido y funciona sin instalación. |
| Arrastrar y soltar no responde | Usa los botones; si persiste, cierra y vuelve a abrir. La ventana avisa si el módulo de arrastre no está disponible. |
| Un archivo aparece como «FALLO» | Está dañado o es un formato no soportado. El registro indica el motivo concreto. |
| El antivirus bloquea el .exe | Los ejecutables generados con PyInstaller a veces dan falsos positivos. Añade una exclusión o usa el código fuente con Python. |

## 15. Automatización, extras y distribución

### Carpeta vigilada

*Herramientas → Carpeta vigilada…* permite elegir una carpeta de entrada y otra de salida. Cada 3 segundos se revisan las imágenes y comprimidos nuevos y se convierten con los ajustes actuales, sin sobrescribir lo que ya exista. Se detiene desde el mismo menú o al cerrar el programa.

### Patrón de nombres

El campo **Nombre de salida** acepta marcadores: `{nombre}`, `{ancho}`, `{alto}`, `{hash}`, `{fecha}`, `{formato}` y `{calidad}`. Vacío conserva el nombre original. Ejemplo: `{nombre}_{ancho}x{alto}`.

### Solo analizar

Calcula el peso estimado de todo el lote sin crear imágenes. Con el informe CSV se guarda un `analisis_*.csv` con los tamaños estimados.

### Vista previa

*Herramientas → Vista previa de la primera imagen* muestra el original y el resultado lado a lado, con dimensiones, peso y porcentaje de ahorro.

### Menú contextual de Windows

Se puede añadir «Convertir a WebP con Conversor WebP» al clic derecho de imágenes, carpetas y comprimidos. Se instala en HKCU (sin administrador) y se quita desde Herramientas.

### HEIC/HEIF

Las fotos de iPhone en `.heic`/`.heif` se abren y convierten como cualquier otra imagen (requiere pillow-heif, incluido en el ejecutable).

### Ayuda emergente (tooltips)

Al dejar el ratón unos instantes sobre cualquier botón, casilla, campo o desplegable aparece un globo con una explicación breve de para qué sirve. Al recorrer los menús, la barra de estado inferior muestra la descripción de la opción resaltada.

### Instalador y versión portable

El instalador crea accesos directos y desinstalador, instala en `%LOCALAPPDATA%\Programs\ConversorWebP` (sin pedir administrador) y guarda la configuración en `%APPDATA%`. La versión portable se descomprime y usa un `config.json` junto al `.exe`, sin tocar el registro ni el perfil del usuario.

### Actualizaciones

Ambas ediciones comprueban la última release de GitHub al arrancar (como máximo una vez al día) y muestran un aviso en la barra de estado. *Ayuda → Buscar actualizaciones* fuerza la comprobación y ofrece abrir la página de descarga.

Conversor WebP 2.5.1 · Programa local para Windows · Interfaz Tkinter, conversión con Pillow, arrastre con TkDND, extracción con 7-Zip/WinRAR.\
Este manual se abre desde el programa en **Ayuda → Manual de uso** y también puede abrirse con doble clic en cualquier navegador.
