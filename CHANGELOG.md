# Registro de cambios

Todos los cambios relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y versionado
[SemVer](https://semver.org/lang/es/).

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
