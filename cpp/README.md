# Conversor WebP · versión C++ (portable)

Recreación nativa en C++ del programa Python, **como prueba**. No se ha inicializado
git ni se ha subido nada a GitHub: esta carpeta queda fuera del control de versiones
del proyecto principal.

- **Interfaz:** Win32 nativa (sin dependencias de UI), con la misma distribución de
  tarjetas, secciones y controles que la versión Tkinter.
- **Decodificación:** WIC (incluye HEIC/HEIF si el códec HEVC de Windows está instalado) y libavif (AVIF).
- **Codificación:** libwebp 1.6.0 y libavif 1.4.2 con libaom 3.12.1, estáticas.
- **Color:** lcms2 para validar/convertir perfiles; los perfiles RGB se incrustan en
  WebP (chunk ICCP) y AVIF.
- **Portable:** el ejecutable no escribe en el registro; usa `config.ini` junto al
  `.exe`. La integración con el Explorador es opcional y se instala en HKCU.
- **Modo oscuro:** conmutable con el icono sol/luna de la cabecera y recordado en `config.ini`.

## Resultado de la comparación con Python

Medido con las 52 imágenes de prueba y calidad 85 / esfuerzo 6:

| Comprobación | Resultado |
|---|---|
| Dimensiones de salida | 52/52 idénticas |
| Píxeles decodificados sin reducir | **52/52 exactamente iguales** |
| Archivos byte a byte sin reducir | 39/52 idénticos |
| Orientaciones EXIF 1–8 | 8/8 píxeles idénticos |
| HEIC (foto de iPhone) | Convertida correctamente |
| Redimensionado con LANCZOS | **Píxeles idénticos** en las pruebas realizadas (PNG con alfa y JPEG con EXIF/ICC) |

El redimensionado replica el algoritmo de Pillow 13: conversión a alfa premultiplicado,
reducción entera previa (`reducing_gap = 2`), filtro LANCZOS con coeficientes en punto
fijo de 22 bits, dos pasadas y despremultiplicado final. Por eso puede coincidir píxel
a píxel, no solo visualmente.

## Rendimiento

- Conversión por lotes con **pool de hasta 8 hilos**.
- Resultados completos de arranque, lotes, memoria y comparación con Python:
  [`BENCHMARK.md`](BENCHMARK.md).

## Capturas

La interfaz C++ está ajustada a la de Python: paleta, tarjetas, opciones, botones,
slider, scrollbar y pestañas.

- [`docs/gui_python.png`](docs/gui_python.png)
- [`docs/gui_python_dark.png`](docs/gui_python_dark.png)
- [`docs/gui_cpp.png`](docs/gui_cpp.png)
- [`docs/gui_cpp_dark.png`](docs/gui_cpp_dark.png)
- [`docs/gui_comparacion.png`](docs/gui_comparacion.png)
- [`docs/gui_comparacion_oscura.png`](docs/gui_comparacion_oscura.png)
- [`docs/gui_tooltip_cpp.png`](docs/gui_tooltip_cpp.png)

## Qué incluye

### Interfaz y flujo
- Arrastrar y soltar archivos, carpetas y comprimidos.
- Botones Archivos/Carpeta/Quitar/Quitar todo.
- Carpeta de salida, mantener subcarpetas, sobrescribir.
- Calidad 1–100, esfuerzo 0–6, sin pérdida, recodificar el formato destino.
- Lado máximo (2560/1920/1600/1200/800/512) sin ampliar.
- Peso máximo por imagen con búsqueda de calidad.
- Formato de salida **WebP o AVIF**.
- **Presets** incluidos y propios, guardados en `config.ini`.
- **Historial** de los últimos 10 trabajos (recupera origen, destino y ajustes).
- **Patrón de nombres** con `{nombre}`, `{ancho}`, `{alto}`, `{formato}`,
  `{calidad}`, `{fecha}` y `{hash}` (SHA-1).
- **Solo analizar (dry-run)**: calcula pesos sin escribir archivos.
- **Aplanar transparencia** sobre un color elegible.
- **Atajos**: Ctrl+O, Ctrl+Shift+O, Ctrl+Enter, F5, Supr, Ctrl+P y F1.
- **Apagado al terminar** con cuenta atrás cancelable.
- Descripciones de menú en la barra de estado y tooltips.
- Tabla de resultados con doble clic para abrir, registro aparte.
- Aviso sonoro, manual y «Acerca de».

### Sistema y automatización
- **Menú contextual** del Explorador para imágenes y carpetas (HKCU, sin admin).
- **Enviar a**: acceso directo en la carpeta SendTo.
- **Carpeta vigilada**: convierte automáticamente las imágenes nuevas.
- **Vista previa** original/resultado antes de convertir.
- **Comprobación de actualizaciones** contra GitHub.
- **Instalador Inno Setup** (`installer/ConversorWebPC.iss`) y **ZIP portable**.

### Formatos y entrada
- JPG, PNG, BMP, GIF (primer fotograma), TIFF, ICO, WebP, AVIF, HEIC/HEIF.
- **AVIF** de entrada y salida con libavif + AOM.
- **HEIC/HEIF** mediante el códec HEVC de Windows (HEIF Image Extensions). Si el
  sistema no lo tiene instalado, el formato no se puede abrir.
- **Perfiles ICC** RGB incrustados; los CMYK se convierten a RGB con WIC/lcms2.
- **ZIP** integrado; **7Z** con `7zr.exe` incluido; **RAR** si hay 7-Zip o WinRAR.
- ZIP del resultado e informe CSV.

### Rendimiento
- Resize LANCZOS propio en C++ (sin dependencias externas).
- Codificación AVIF con AOM multiproceso (4 hilos).
- El ejecutable es un binario estático de ~10 MB; no necesita DLLs.

## Diferencias conocidas con la versión Python

- **GPU/CUDA no implementada**: en Python está desactivada por defecto y la ruta CPU
  paralela fue igual o más rápida en las pruebas. Se considera fuera de alcance.
- **GIF animado**: igual que Python, se usa el primer fotograma y se avisa en el
  registro; no se genera WebP animado.
- **RAR**: requiere 7-Zip o WinRAR instalado (igual que Python). `.zip` y `.7z`
  funcionan sin instalar nada.
- **HEIC/HEIF**: usa el códec HEVC de Windows; en un Windows sin HEIF Image
  Extensions no se puede abrir (la versión Python lo trae con pillow-heif).
- La GUI no es literalmente idéntica a Tkinter (fuentes y controles nativos), pero es
  funcionalmente equivalente.
- Al abrirse muestra brevemente la consola (subsistema de consola para el modo CLI);
  se oculta de inmediato.

## Compilar

Requisitos: Visual Studio 18 con MSVC 14.51 y Windows SDK 10.0.26100. Los scripts usan
CMake y Ninja de las herramientas de vcpkg; ajusta las rutas si cambias de versión.

```bat
:: 1) Dependencias (solo la primera vez)
tools\build_libwebp.bat
tools\build_aom.bat
tools\build_avif.bat
tools\build_lcms2.bat
tools\build_zlib.bat

:: 2) Aplicación
build.bat
```

Genera `bin\ConversorWebPC.exe` y copia el manual e icono.

### Instalador y portable

```powershell
:: ZIP portable
tools\build_portable.ps1

:: Instalador (requiere Inno Setup 6)
ISCC.exe installer\ConversorWebPC.iss

:: Instalador portable por usuario (sin Inno Setup)
powershell -ExecutionPolicy Bypass -File installer\install.ps1 -ContextMenu -SendTo
powershell -ExecutionPolicy Bypass -File installer\uninstall.ps1
```

## Uso

```bat
bin\ConversorWebPC.exe
bin\ConversorWebPC.exe "C:\Fotos" -o "C:\Fotos\webp" -q 85 -m 6 --max-dim 1600 --zip salida.zip
bin\ConversorWebPC.exe "C:\Fotos" -o "C:\Fotos\avif" --format avif --pattern "{nombre}_{ancho}"
```

La configuración se guarda en `bin\config.ini`.

### Integración con Windows

```bat
bin\ConversorWebPC.exe --install-context
bin\ConversorWebPC.exe --install-sendto
bin\ConversorWebPC.exe --uninstall-context
bin\ConversorWebPC.exe --uninstall-sendto
```

También se puede hacer desde **Herramientas** en el menú.

## Estructura

```
cpp/
├── bin/                       ejecutable, manual, icono, 7zr y config.ini
├── src/main.cpp               toda la aplicación (GUI + CLI + motor)
├── installer/                 script de Inno Setup
├── tools/                     compilación de dependencias y ZIP portable
├── third_party/               fuentes y bibliotecas estáticas generadas
├── build.bat                  compila la aplicación
└── README.md
```
