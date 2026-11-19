# Registro de cambios

Todos los cambios relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y versionado
[SemVer](https://semver.org/lang/es/).

---

## [2.15.1] — 2026-11-19

### Corregido

- **Python:** `ctypes.wintypes` no estaba importado, por lo que la llamada nativa para permitir el modo oscuro (`AllowDarkModeForWindow`) fallaba silenciosamente. Ahora se aplica correctamente.
- **Python:** el vigilante de carpeta no copiaba `preservar_extra` a sus trabajos; ahora conserva EXIF/XMP y fecha si la opción está activa.
- **C++:** al usar `--preset` se perdían `--html` y los parámetros de vídeo indicados por CLI; ahora se conservan.
- **C++:** compilación elevada a `/W4` sin avisos.

### Pruebas

- 32 pruebas Python superadas.
- C++ compilado sin avisos y probado con HTML, EXIF/XMP, AVIF y vídeo.
- Ejecutadas ambas interfaces.

---

## [2.15.0] — 2026-11-19

### Añadido

- **Paridad de informes en C++:** la fila de opciones ahora es idéntica a Python:
  - **Informe CSV**
  - **Informe HTML**
  - **Conservar metadatos (EXIF/XMP) y fecha original**
- **Informe HTML** en C++ con la misma tabla, resumen, ahorro total y estilo que el de Python.
- **Conservación de metadatos en C++:** EXIF y XMP en JPEG, PNG y WebP; EXIF/XMP también al generar AVIF.
- Fecha original conservada con la misma casilla combinada.
- Nuevos argumentos CLI en C++: `--html` y `--keep-metadata` (alias de `--keep-date`).
- El estado de las casillas se guarda en configuración, presets e historial.

### Pruebas

- 32 pruebas Python superadas.
- C++ verificado con EXIF, XMP, AVIF, CSV y HTML.
- Captura de la interfaz C++ revisada: la fila coincide con Python.

---

## [2.14.0] — 2026-11-19

### Añadido

- **Presets de vídeo** en el diálogo de opciones, en Python y C++:
  - **Baja:** 480p, CRF 36, audio 64 kbps, 30 fps.
  - **Media:** 720p, CRF 32 (dentro de 30–33), audio 96 kbps, 30 fps.
  - **Alta:** 1080p, CRF 26, audio 128 kbps, FPS original.
  - **Personalizada:** mantiene los parámetros manuales.
- Al cambiar cualquier control después de elegir un preset, el selector pasa solo a **Personalizada**.
- El preset se detecta al volver a abrir el diálogo si los valores coinciden.
- Botón **Restablecer** también en el diálogo C++.
- Los presets se aplican a los mismos parámetros ya existentes: resolución, CRF/VBR, bitrate, audio, FPS, contenido y denoise.

### Pruebas

- 32 pruebas Python superadas, incluidas las que validan los valores de los presets.
- C++ probado seleccionando **Media** y comprobando `720p`, `CRF 32`, `96 kbps` y `30 fps` en `config.ini`.

---

## [2.13.0] — 2026-11-18

### Añadido

- **Opciones avanzadas de vídeo** en Python y C++, accesibles con el botón **Vídeo: …** junto a «Formato de salida».
- **Resolución de salida:** Original, 2160p, 1440p, 1080p, 720p, 480p o 360p; altura máxima sin ampliar y proporción conservada.
- **Modo de calidad:** CRF (calidad constante) o VBR (tasa de bits objetivo).
- **CRF manual** 15–50 o automático según la calidad global.
- **Bitrate de vídeo** 250–200000 kbps, con `-maxrate` y `-bufsize` para limitar picos.
- **Audio Opus:** sin audio, 64, 96, 128 o 192 kbps.
- **FPS máximo:** Original, 60, 30, 24 o 15.
- **Contenido:** Normal, Pantalla o Película (`-tune-content` de VP9).
- **Reducción de ruido** opcional con `hqdn3d`.
- Escalado **Lanczos** de alta calidad y dimensiones pares para `yuv420p`.
- Nuevos argumentos CLI: `--video-height`, `--video-mode`, `--video-crf`, `--video-bitrate`, `--video-fps`, `--audio-bitrate`, `--video-tune` y `--denoise`.
- Los parámetros se guardan en `config.ini`/`config.json`, presets, historial y cola pendiente.

### Pruebas

- 30 pruebas Python superadas, incluida una de resolución, bitrate, FPS y sin audio.
- C++ probado con parámetros de vídeo, incluido el diálogo y una conversión desde la GUI.
- Python probado con el diálogo, la configuración y una conversión con todos los parámetros.

---

## [2.12.0] — 2026-11-18

### Añadido

- **FFmpeg incluido** en las cuatro distribuciones empaquetadas: portable y setup de Python y de C++.
- `ffmpeg.exe`, `ffprobe.exe` y `LICENCIA_FFMPEG.txt` se instalan/descomprimen **en la misma carpeta** que el ejecutable.
- El programa los detecta automáticamente al arrancar; ya no hay que configurar el `PATH` en esas versiones.
- Script `tools/preparar_ffmpeg.ps1` para descargar/cachear FFmpeg y `tools/empaquetar_python.ps1` / `tools/empaquetar_cpp.ps1` para generar los paquetes completos.
- Los paquetes incluyen el texto de la **GPLv3** de FFmpeg y el enlace al código fuente.

### Notas

- El ejecutable suelto (`ConversorWebP.exe`) y el código fuente siguen necesitando FFmpeg aparte.
- El ZIP y los instaladores crecen unos 100 MB por la build estática de FFmpeg.

### Pruebas

- Paquetes generados y verificados: `ffmpeg.exe`, `ffprobe.exe` y licencia presentes.
- Conversión de vídeo desde la carpeta portable y desde una instalación silenciosa del setup.

---

## [2.11.0] — 2026-11-18

### Añadido

- **Conversión de vídeo a WebM** en las ediciones Python y C++, con vídeo **VP9** y audio **Opus**.
- Entradas habituales: MP4, M4V, MOV, MKV, AVI, WMV, FLV, WebM, MPEG, TS, M2TS, 3GP, OGV y otras compatibles con FFmpeg.
- **Lotes mixtos**: imágenes a WebP/AVIF y vídeos a WebM en el mismo trabajo.
- La **calidad 1-100** se traduce al CRF de VP9 (85 ≈ CRF 30); el **esfuerzo 0-6** ajusta `-cpu-used`.
- El **lado máximo** limita el vídeo sin ampliarlo y el **audio se conserva** como Opus 96 kbps.
- Los vídeos se convierten de uno en uno (FFmpeg ya usa varios hilos); las imágenes siguen en paralelo.
- **Detección de FFmpeg** en `PATH`, junto al ejecutable, en `bin\`, `tools\` o `tools\ffmpeg\`; opción `--ffmpeg` y clave `ffmpeg` en `config.ini`.
- Menú **Herramientas → Descargar FFmpeg para vídeo** y avisos claros si falta.
- Entradas de **menú contextual** para vídeos en la edición C++.

### Limitaciones

- FFmpeg es una dependencia externa: sin él, los vídeos fallan pero las imágenes siguen funcionando.
- El **peso máximo en KB** no se aplica a vídeos.
- No se conservan todos los metadatos del contenedor; WebM solo admite un subconjunto.

### Pruebas

- 29 pruebas Python superadas, incluidas 4 de vídeo (WebM, lote mixto, omisión y dry-run).
- C++ compilado y probado con vídeo real: VP9, Opus, audio, redimensionado, patrón y lote mixto.

### Pendiente

- Traducción completa al inglés.

---

## [2.10.0] — 2026-11-18

### Añadido

- **DPI nítido del sistema en C++:** la interfaz se dibuja a la resolución real del monitor, sin que Windows estire una imagen. Se escalan coordenadas, tamaños, fuentes, controles nativos, pestañas, sliders, checkboxes, diálogos, vista previa, ventana «Acerca de» y tooltips.
- Opción `[opciones] dpi` en `config.ini`:
  - `auto` (por defecto): usa el DPI del sistema y reduce la escala si la ventana no cabe en el área de trabajo.
  - `system`: usa exactamente el DPI del sistema.
  - `gdi`: conserva el modo anterior con `UNAWARE_GDISCALED`.
  - Un valor numérico (`120`, `144`, `192`…) fuerza esa escala; es una opción avanzada.
- La edición Python ya usaba DPI del sistema y fuentes escaladas; se mantiene sin cambios funcionales.

### Limitaciones

- **No es DPI por monitor:** al mover la ventana a un monitor con otra escala, Windows puede interpolar la imagen hasta reiniciar. El modo `permonitor` no se implementa.
- Una escala numérica forzada puede hacer que la ventana no quepa en pantallas pequeñas.

### Pendiente

- Traducción completa al inglés.

---

## [2.9.1] — 2026-11-18

### Añadido

- **Menú contextual moderno de Windows 11** mediante la DLL COM `ConversorWebPContext.dll`.
- Tarea opcional en el instalador C++ y scripts `INSTALAR_MENU_MODERNO.bat` / `QUITAR_MENU_MODERNO.bat` para la versión portable.
- La extensión solo lanza `ConversorWebPC.exe` con las rutas seleccionadas; no contiene lógica de conversión.

### Pendiente

- Traducción completa al inglés.
- DPI nítido por monitor; se mantiene el escalado GDI.

---

## [2.9.0] — 2026-11-18

### Añadido

- **DDS en C++:** texturas DXT1, DXT3, DXT5 y DDS sin comprimir.

### Pendiente

- Menú contextual moderno de Windows 11 (extensión COM/MSIX).
- Traducción completa al inglés.
- DPI nítido por monitor; el escalado GDI se mantiene como solución compatible.

---

## [2.8.0] — 2026-11-18

### Añadido

- **Portapapeles:** añadir una imagen o archivos copiados desde el menú Herramientas.
- **Informe HTML** con tabla de resultados, ahorro y enlace al CSV.
- **Exportar/importar presets** en JSON.
- **TGA, PCX, PPM, PGM y PBM** en C++.
- **Escalado GDI** en C++ para que la interfaz no quede diminuta en pantallas con DPI alto.

### Pendiente

- Menú contextual moderno de Windows 11 (extensión COM/MSIX).
- Traducción completa al inglés.
- Decodificador DDS y DPI nítido por monitor.

---

## [2.7.0] — 2026-11-18

### Añadido

- **Cola persistente** con pausa, reanudación, reintento de fallidos y reordenación de orígenes.
- **Vista previa interactiva** con calidad de prueba y estimación de peso.
- **WebP animado** desde GIF/WebP en Python y C++.
- **Metadatos EXIF/XMP y fecha original** opcionales en Python; fecha original en C++.
- **CLI** con `--json`, `--quiet`, `--jobs`, `--preset` y `--keep-metadata`.
- Verificación Authenticode de las actualizaciones y script de firma para cuando exista certificado.
- Configuración INI atómica en C++.
- Aviso de espacio libre, tiempo restante estimado e instancia única.

### Pendiente para próximas versiones

- Certificado Authenticode real: el pipeline está listo, pero los binarios publicados siguen sin firmar.
- Paridad completa de DPI, formatos de imagen y menú contextual moderno de Windows 11.
- Traducción completa al inglés.

---

## [2.6.5] — 2026-11-18

### Corregido

- **Python:** al cambiar de tema se conservan las rutas de la tabla, por lo que el doble clic sigue abriendo el archivo.
- **Python:** la vista previa se genera en segundo plano, se limpia al cerrar con la X y al cambiar de tema.
- **Python:** un `config.json` con historial corrupto ya no impide arrancar.
- **Python:** límite de tamaño y número de archivos al descomprimir.
- **C++:** el CSV escapa correctamente campos con `;`, comillas o saltos de línea.
- **C++:** la sobrescritura de imágenes usa sustitución atómica y no pierde el archivo anterior si falla.
- **C++:** al cerrar durante una conversión se espera al hilo antes de borrar los temporales.
- **C++:** el CLI muestra el registro en tiempo real y admite `--shutdown` y `--shutdown-delay`.
- **C++:** el pitido y el icono final distinguen éxito de fallo.
- **C++:** comprobaciones defensivas de WIC, URL de descarga y errores del actualizador.

---

## [2.6.4] — 2026-11-18

### Corregido

- En la edición C++, la etiqueta **Lado máximo** se solapaba con su desplegable y el texto
  «Sin cambio» aparecía recortado. Ahora la fila tiene más espacio y se lee completa.

---

## [2.6.3] — 2026-11-18

### Añadido

- **Modo oscuro** en las dos ediciones (Python y C++), conmutable desde un icono de sol/luna.
- El icono cambia al instante: sol en modo claro y luna menguante en modo oscuro.
- La preferencia se guarda en `config.json` (Python) y `config.ini` (C++), y se restaura al abrir.
- Paleta oscura completa: tarjetas, entradas, listas, tabla, barras de desplazamiento, barra de estado,
  pestañas, menús y barra de título.
- En modo oscuro, las dos ediciones usan una barra de menú propia para que también sea oscura.

---

## [2.6.2] — 2026-11-18

### Añadido

- **Actualización automática** desde la propia aplicación, en Python y C++.
- Elige el archivo correcto según cómo esté instalado el programa: instalador (setup), portable (ZIP) o ejecutable suelto.
- Descarga desde GitHub, verifica el **SHA-256** publicado por la API y aplica la actualización en el mismo directorio.
- Un script auxiliar espera a que la aplicación se cierre, sustituye los archivos y la vuelve a abrir automáticamente.
- Si la descarga o la verificación fallan, no se modifica nada y se muestra el error.
- La actualización manual sigue mostrando el aviso de «estás al día» o de nueva versión.

---

## [2.6.1] — 2026-11-18

### Añadido

- **Edición C++ (Win32)** con la misma interfaz y funciones que la versión Python, incluida en `cpp/`.
- Icono de la aplicación en la ventana, la barra de tareas y el propio ejecutable.
- Selector de color de aplanado visible y clicable en ambas ediciones.
- Ventana C++ maximizable y redimensionable, sin ventana de consola.
- Manual y «Acerca de» propios de C++, sin referencias a GPU.

### Corregido

- Nombres de salida únicos cuando coinciden varios originales (ambas ediciones).
- Deduplicación de orígenes repetidos y escritura atómica de las imágenes.
- ZIP en streaming en C++ y sin recompresión en Python; validación de límites y errores.
- Vigilante de carpeta con reintentos y espera a que el archivo esté estable.
- Configuración, presets e historial validados y acotados.
- Comprobación de actualizaciones asíncrona y tooltips seguros al destruir controles.

---

## [2.6.0] — 2026-11-17

### Añadido

- Casilla **Apagar al terminar** (barra inferior), desactivada por defecto y con confirmación al activarla.
- Ventana de **cuenta atrás de 30 segundos** con «Cancelar apagado» y «Apagar ahora».
- El apagado solo se ejecuta si hubo conversiones, no hubo fallos, no se canceló y no era un análisis.
- Opciones de consola `--shutdown` y `--shutdown-delay N`.
- El apagado real se lanza con `shutdown /s /t 10`, por lo que sigue siendo cancelable con `shutdown /a`.

---

## [2.5.1] — 2026-11-17

### Añadido

- **Tooltips explicativos** en 41 controles: al pasar el ratón por botones, casillas, campos, desplegables
  y la tabla aparece un globo con una explicación breve.
- **Descripciones de menú** en la barra de estado al recorrer `Ayuda`, `Historial` y `Herramientas`.
- El globo se oculta al hacer clic o al salir del control, y se coloca dentro de la pantalla.

---

## [2.5.0] — 2026-11-17

### Añadido

- **HEIC/HEIF** (fotos de iPhone) mediante `pillow-heif`, incluido en el ejecutable.
- **Carpeta vigilada**: convierte automáticamente lo que aparezca en una carpeta, con los ajustes actuales.
- **Patrón de nombres** con marcadores `{nombre}`, `{ancho}`, `{alto}`, `{hash}`, `{fecha}`, `{formato}` y `{calidad}`.
- **Modo solo analizar** (dry-run) con informe CSV de pesos estimados.
- **Tabla de resultados** con doble clic para abrir cada archivo, junto al registro tradicional.
- **Vista previa** antes/después con dimensiones, peso y ahorro.
- **Menú contextual de Windows** para imágenes, carpetas y `.zip/.rar/.7z` (HKCU, sin administrador).
- **Instalador Inno Setup** y **versión portable** (config junto al `.exe`).
- **Comprobación de actualizaciones** desde GitHub Releases, manual y automática (1 vez/día).
- **GitHub Actions** para ejecutar las pruebas en cada push.
- Nuevas pruebas: patrones, dry-run, HEIC, portable y versiones (19 en total).

### Cambiado

- El área inferior pasa a dos pestañas: **Resultados** (tabla) y **Registro**.
- La especificación de PyInstaller incluye `pillow_heif` e `ImageTk`.

---

## [2.4.1] — 2026-11-17

### Cambiado

- Eliminado el menú **Acerca de** de la barra superior: duplicaba `Ayuda → Acerca de Conversor WebP`.
  El acceso queda solo dentro de **Ayuda** (y `F1` sigue abriendo el manual).

---

## [2.4.0] — 2026-11-17

### Añadido

- **Detección de GPU** al arrancar (registro de Windows + `nvidia-smi`) con nombre, driver y memoria.
- **Aceleración CUDA opcional**: decodificado nvJPEG y reescalado en GPU para JPEG RGB de 8 MP o más que
  se vayan a reducir, con `fallback` automático a CPU por imagen.
- **Carga de PyTorch desde el sistema**: el `.exe` no incluye CUDA (~2,4 GB); busca una instalación
  existente o una carpeta indicada por el usuario.
- Menú **Herramientas → Aceleración GPU** con estado, ruta de PyTorch y selección de carpeta.
- Opción **Forzar GPU en lotes pequeños** y argumentos `--gpu` / `--no-gpu`.
- `--diag` ampliado con GPU, candidatos de PyTorch y estado de CUDA.
- `requirements-gpu.txt` y documentación con mediciones reales.

### Notas

- La codificación WebP/AVIF es CPU; la ganancia aparece al redimensionar fotos grandes (1,3–2× por imagen).
- **El modo CUDA está desactivado por defecto**: en la app completa, 60 fotos de 24 MP tardaron 12,5 s con
  CPU (8 hilos) y 18,0 s con GPU. Se mantiene como opción experimental.
- La activación automática, si se marca la casilla, exige un lote de 30+ fotos grandes porque cargar CUDA
  tarda ~7 s.

---

## [2.3.1] — 2026-11-16

### Corregido

- **Perfil de color en imágenes CMYK**: ahora se convierten a sRGB con su perfil ICC mediante
  `ImageCms`. Antes se incrustaba un perfil CMYK dentro de un archivo RGB, lo que producía colores
  incorrectos en navegadores y visores.
- **Historial incompleto**: al repetir un trabajo se recuperan también el formato, el ZIP, el informe
  CSV, las subcarpetas y la sobrescritura, no solo la calidad.
- **Nombres de ZIP**: se sanean rutas y caracteres no válidos (`..\`, `:`, nombres reservados de
  Windows), de modo que el ZIP no puede escribirse fuera de la carpeta de salida.
- **Colores de fondo**: se aceptan `#RGB` y `#RRGGBB`; un color inválido se rechaza al empezar en lugar
  de hacer fallar cada imagen.
- **WinRAR**: extracción síncrona (se eliminó `-ibck`, que podía devolver el control antes de terminar).
- **Configuración**: escritura atómica (archivo temporal + reemplazo) y lectura tolerante a tipos o
  valores corruptos, con límites en calidad y esfuerzo.
- **Cierre de la ventana**: se cancela el temporizador de la interfaz, evitando errores
  `invalid command name` al salir.
- **Mensajes**: se distingue «todo omitido» de «no hay imágenes» y el aviso sonoro acierta en ese caso.
- **Duplicados**: se comparan rutas resueltas y se ignoran los archivos que ya no existen.
- **Selector de archivos**: incluye AVIF y el resto de formatos soportados.
- **Ventana**: tamaño adaptativo en pantallas pequeñas.

### Añadido

- `--version` en la línea de comandos.
- `--format` acepta `AVIF` en mayúsculas o minúsculas.

---

## [2.3] — 2026-11-16

### Añadido

- **Conversión en paralelo**: los lotes de 4 o más imágenes se reparten entre hasta 8 hilos.
- **Informe CSV** opcional en la carpeta de salida (`informe_AAAAMMDD_HHMMSS.csv`), con UTF-8 con BOM y
  separador `;` para Excel.
- **Formato de salida AVIF** además de WebP (alfa, ICC, sin pérdida y peso objetivo incluidos).
- **Acceso directo en “Enviar a”** del Explorador (`Herramientas → Añadir a “Enviar a”`), con opción de
  eliminarlo.
- **Precarga desde argumentos**: si se pasan rutas sin `-o`, la interfaz se abre con esos archivos
  cargados (base de “Enviar a” y “Abrir con”).
- Nuevas opciones de consola: `--format`, `--max-dim`, `--max-kb`, `--csv`.
- Documentación del repositorio: README, arquitectura, guía de compilación y este registro.

### Cambiado

- La etiqueta «Sobrescribir WebP existentes» pasa a «Sobrescribir archivos existentes».
- La recodificación ahora se aplica al formato destino (`.webp` o `.avif`).
- El ZIP ordena las entradas por ruta para que el contenido sea reproducible.

---

## [2.2] — 2026-11-16

### Añadido

- **Presets** de calidad: Web (85 · 1920 px), Catálogo (80 · 1200 px), Iconos sin pérdida y Máxima
  calidad (95), más presets propios con `Guardar`/`Borrar`.
- **Redimensionado** por lado máximo (2560/1920/1600/1200/800/512 px) sin ampliar imágenes pequeñas.
- **Peso máximo por imagen** en KB, con búsqueda automática de calidad y aviso si es imposible.
- **Historial** de los últimos 10 trabajos con restauración de orígenes, destino y ajustes.
- **Selección múltiple** en la lista de origen, botón `Quitar` y tecla `Supr`.
- **Atajos**: `Ctrl+O`, `Ctrl+Mayús+O`, `Ctrl+Enter`, `F5`, `F1`.

### Corregido

- El deslizador de calidad podía guardar valores fraccionarios redondeados a la baja (por ejemplo 84 en
  lugar de 85). Ahora usa resolución entera y redondeo correcto.

---

## [2.1] — 2026-11-16

### Añadido

- **Aviso sonoro al terminar**: dos tonos agudos si todo va bien, dos graves si hay errores o se cancela.
- Casilla para desactivar el sonido, con preferencia guardada.

### Documentación

- Manual actualizado con el aviso sonoro.

---

## [2.0] — 2026-11-16

### Añadido

- **Arrastrar y soltar** desde el Explorador de Windows en cualquier punto de la ventana.
- **Comprimidos** `.zip`, `.rar` y `.7z`: extracción temporal, estructura interna y subcarpeta por
  comprimido cuando hay varios.
- Detección automática de **7-Zip** o **WinRAR**.
- **Carpeta de salida** explícita con comprobación de escritura.
- **Empaquetado ZIP** del resultado con nombre configurable.
- **Menú Ayuda** con manual (`F1`) y ventana **Acerca de** con resumen de características y entorno.
- **Manual de uso** HTML completo, incrustado también en el ejecutable.
- **Modo consola** con `--zip`, `--lossless`, `--flat`, `--no-recursive`, `--no-overwrite`, `--webp`.
- Icono propio y configuración persistente en `%APPDATA%`.

### Corregido

- Error de comunicación entre el hilo de conversión y la interfaz que dejaba la barra de progreso sin
  avanzar en la versión 1.0.

---

## [1.0] — 2026-11-16

### Añadido

- Conversión de imágenes JPG, PNG, BMP, TIFF, GIF e ICO a WebP.
- Interfaz Tkinter con selección de archivos o carpeta, calidad, esfuerzo, subcarpetas, sobrescritura,
  transparencia y registro de progreso.
- Conservación de transparencia, perfil ICC y orientación EXIF.
- Modo consola básico.

[2.15.1]: #2151--2026-11-19
[2.15.0]: #2150--2026-11-19
[2.14.0]: #2140--2026-11-19
[2.13.0]: #2130--2026-11-18
[2.12.0]: #2120--2026-11-18
[2.11.0]: #2110--2026-11-18
[2.10.0]: #2100--2026-11-18
[2.9.1]: #291--2026-11-18
[2.9.0]: #290--2026-11-18
[2.8.0]: #280--2026-11-18
[2.7.0]: #270--2026-11-18
[2.6.5]: #265--2026-11-18
[2.6.4]: #264--2026-11-18
[2.6.3]: #263--2026-11-18
[2.6.2]: #262--2026-11-18
[2.6.1]: #261--2026-11-18
[2.6.0]: #260--2026-11-17
[2.5.1]: #251--2026-11-17
[2.5.0]: #250--2026-11-17
[2.4.1]: #241--2026-11-17
[2.4.0]: #240--2026-11-17
[2.3.1]: #231--2026-11-16
[2.3]: #23--2026-11-16
[2.2]: #22--2026-11-16
[2.1]: #21--2026-11-16
[2.0]: #20--2026-11-16
[1.0]: #10--2026-11-16
