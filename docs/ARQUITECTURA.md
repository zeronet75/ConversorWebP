# Arquitectura

Documento técnico de **Conversor WebP 2.3**. Explica cómo está organizado el código, cómo fluye un
trabajo de conversión y dónde tocar para ampliar el programa.

---

## 1. Visión general

El programa vive en **un único archivo**, `conversor_webp.py` (~1.900 líneas), con tres capas bien
separadas:

```text
┌──────────────────────────────────────────────────────────────┐
│  Interfaz (Tkinter)                                          │
│  App · DropZone · VentanaAcerca                              │
│  Estado de los controles, presets, historial, configuración   │
└───────────────▲──────────────────────────────┬───────────────┘
                │ eventos (cola)               │ Job
┌───────────────┴──────────────────────────────▼───────────────┐
│  Motor de conversión                                         │
│  Job · Result · run_job · convert_one                        │
│  Planificación, extracción, paralelismo, ZIP, CSV            │
└───────────────▲──────────────────────────────┬───────────────┘
                │                              │
┌───────────────┴──────────────────────────────▼───────────────┐
│  Utilidades de sistema                                       │
│  Comprimidos (7-Zip/WinRAR/zipfile), formatos, rutas, config  │
└──────────────────────────────────────────────────────────────┘
```

La interfaz **no convierte**: construye un `Job` inmutable con todo lo necesario y lo entrega a un hilo
de trabajo. Así la ventana nunca se bloquea y el motor puede probarse sin GUI (es lo que hacen las
pruebas automáticas).

---

## 2. Modelo de datos

### `Job`

Fotografía completa de una conversión:

| Campo | Tipo | Descripción |
|---|---|---|
| `sources` | `list[Path]` | Archivos, carpetas y comprimidos de entrada. |
| `output_dir` | `Path` | Carpeta de destino. |
| `quality` | `int` | Calidad 1-100 (tope superior si se usa peso máximo). |
| `method` | `int` | Esfuerzo WebP 0-6 (o velocidad AVIF). |
| `keep_structure` | `bool` | Reproducir subcarpetas. |
| `overwrite` | `bool` | Sobrescribir u omitir existentes. |
| `reencode_webp` | `bool` | Recodificar archivos ya en el formato destino. |
| `flatten`, `background` | `bool`, `str` | Aplanado de transparencia y color. |
| `lossless` | `bool` | WebP/AVIF sin pérdida. |
| `make_zip`, `zip_name` | `bool`, `str` | Empaquetado del resultado. |
| `max_dimension` | `int` | Lado máximo en px (0 = sin cambio). |
| `target_kb` | `int` | Peso máximo por imagen en KB (0 = sin límite). |
| `formato` | `str` | `"WEBP"` o `"AVIF"`. |
| `csv_report` | `bool` | Generar informe CSV. |

### `Result`

Resumen devuelto al terminar:

```python
@dataclass
class Result:
    converted: int
    skipped: int
    failed: list[tuple[Path, str]]
    bytes_in: int
    bytes_out: int
    archives: int
    zip_path: Path | None
    csv_path: Path | None
    cancelled: bool
```

---

## 3. Flujo de `run_job`

`run_job(job, emit, cancel_event)` se ejecuta en un hilo y notifica a la interfaz mediante
`emit(tipo, datos)`. Tipos de evento:

| Evento | Datos | Uso en la UI |
|---|---|---|
| `plan` | `int` | Total de imágenes; reinicia la barra. |
| `status` | `str` | Texto de estado (“Extrayendo…”, “Creando ZIP…”). |
| `log` | `(nivel, texto)` | Línea del registro con color. |
| `progress` | `(hechas, total)` | Avance de la barra. |
| `done` | `Result` | Cierre: resumen, sonido, historial, mensaje. |

### Fase 1 — Planificación

1. Recorre cada origen:
   - carpeta → `collect_images_from_dir` (recursivo, solo extensiones de imagen);
   - comprimido → extrae a una carpeta temporal y recoge su contenido;
   - imagen → entrada directa.
2. Si hay **varios comprimidos**, antepone el nombre del comprimido a las rutas relativas para no mezclar
   contenidos.
3. Filtra los archivos que ya están en el formato destino salvo que `reencode_webp` esté activo.

### Fase 2 — Rutas y omisiones

Se calcula la ruta de salida de cada imagen:

```python
dst = job.output_dir / rel.with_suffix(".avif" if job.formato == "AVIF" else ".webp")
```

- Si `keep_structure` es falso, `rel` se reduce al nombre del archivo.
- Un `set` normalizado (`lower()`) detecta colisiones **antes** de convertir. La segunda imagen con el
  mismo nombre recibe un sufijo con su extensión original: `foto.webp` y `foto_png.webp`.
- Si el destino existe y `overwrite` es falso, se cuenta como omitida y se registra en el CSV.

### Fase 3 — Conversión

- **Secuencial** si hay menos de 4 tareas.
- **Paralela** en caso contrario: `ThreadPoolExecutor` con hasta 8 hilos.
  - Los resultados se recogen con `as_completed` **en el hilo de `run_job`**, no en los hilos de trabajo.
    Por eso las estructuras compartidas (`Result`, `producidos`, filas del CSV) no necesitan cerrojos.
  - Cada hilo abre su propia imagen y escribe su propio archivo: no comparten objetos de Pillow.
  - `cancel_event` detiene la recogida y cancela los futuros pendientes; los que ya están en curso
    terminan (como máximo, los que quepan en el grupo de hilos).

### Fase 4 — Empaquetado e informe

1. ZIP con `ZIP_DEFLATED` (nivel 6), entradas ordenadas por ruta para que el resultado sea reproducible.
2. CSV con `utf-8-sig` y separador `;` para que Excel en español lo abra en columnas.
3. `emit("done", res)`.

La carpeta temporal de extracción se elimina en un bloque `finally`, incluso si hay error o cancelación.

---

## 4. Conversión de una imagen (`convert_one`)

Orden exacto de las operaciones:

```text
abrir → leer primer fotograma → exif_transpose → redimensionar →
convertir modo (RGBA/RGB) → aplanar alfa (opcional) → leer ICC →
codificar (calidad fija o búsqueda de peso) → escribir
```

Notas:

- **EXIF antes de redimensionar**: si la foto viene girada, el lado máximo se aplica sobre la orientación
  real.
- **`Image.thumbnail`**: reduce manteniendo proporción y **nunca amplía**.
- **Alfa**: se conserva como RGBA (WebP con `exact=True`; AVIF admite alfa de forma nativa). Con
  `flatten`, se compone sobre el color elegido.
- **ICC**: si existe `icc_profile`, se pasa al codificador.
- **Opciones por formato**:
  - WebP: `method`, `quality` o `lossless`, `exact` si hay alfa.
  - AVIF: `speed` (6 con esfuerzo ≥ 4, 8 en caso contrario), `quality` o `lossless`.
- **GIF animado**: se convierte el primer fotograma y se añade un aviso al registro.

### Búsqueda de peso objetivo

`_encode_ajustando_peso` hace una búsqueda binaria de la calidad:

1. Rango `[1, calidad_max]`.
2. Hasta 7 codificaciones en memoria; se queda con el mayor `quality` cuyo tamaño ≤ objetivo.
3. Si ninguna cabe, codifica a `quality=1` y devuelve un aviso
   `no se alcanzo X KB (minimo Y KB)`.
4. El resultado se escribe una sola vez en disco.

Coste: hasta 8 codificaciones por imagen, así que con peso objetivo el lote tarda más. Es el compromiso
necesario para garantizar el límite real del archivo.

---

## 5. Comprimidos

`extract_archive` decide la estrategia:

1. **`.zip`** → `extract_zip_builtin` (lector interno).
   - Normaliza separadores y **rechaza rutas que salgan de la carpeta destino** (protección zip-slip).
   - Si falla (cifrado, compresión exótica), continúa con el paso 2.
2. **7-Zip / WinRAR / UnRAR** detectados con `find_extractor()`:
   - 7-Zip: `7z.exe x -y -o<destino> <archivo>`
   - UnRAR: `unrar.exe x -o+ -inul <archivo> <destino>\`
   - WinRAR: `WinRAR.exe x -ibck -y <archivo> <destino>\`
3. Si no hay herramienta y el archivo no es ZIP, se lanza un error explicativo que se muestra en el
   registro.

Los procesos se lanzan con `CREATE_NO_WINDOW` para que no parpadee una consola.

---

## 6. Interfaz

### Construcción

`App` hereda de `TkinterDnD.Tk` cuando `tkinterdnd2` está disponible; si no, de `tk.Tk` y avisa al
usuario. La UI se construye una vez en `_build_ui()`:

- `style.theme_use("vista")` con caída a `winnative` y `clam`.
- Tarjetas blancas `Card.TFrame` sobre fondo gris.
- `DropZone` es un `Canvas` que dibuja el borde punteado y se ilumina al arrastrar.
- `_registrar_dnd_global()` recorre el árbol de widgets y registra **54 destinos** de arrastre, de modo
  que soltar funciona sobre cualquier parte de la ventana. El campo de salida tiene un manejador propio:
  si se suelta una carpeta, la toma como destino.

### Estado y sincronización

- La conversión se lanza en `threading.Thread(daemon=True)`.
- `run_job` recibe `lambda *evento: self.queue.put(evento)`.
- `_drain_queue()` se ejecuta cada 100 ms en el hilo de Tk y aplica los eventos a los widgets. Toda
  mutación de UI ocurre en el hilo principal.

### Presets

- `PRESETS_BUILTIN`: diccionario constante con cuatro combinaciones.
- `self.presets_usuario`: presets guardados por el usuario, persistidos en la configuración.
- `_aplicar_ajustes` envuelve los cambios en `_aplicando_preset` para que los `trace_add` de las
  variables no devuelvan el desplegable a “Personalizado” mientras se aplica un preset.

### Historial

Cada trabajo correcto se guarda con fecha, orígenes, destino, ajustes y número de imágenes. El menú se
reconstruye con `_refrescar_historial()`. Se conservan los 10 últimos y se evita duplicar el mismo
trabajo consecutivo.

### Configuración

`%APPDATA%\ConversorWebP\config.json` con lectura defensiva: cualquier error se ignora y se usan valores
por defecto. Se guarda al cerrar la ventana y al cambiar presets o historial.

---

## 7. Puntos de extensión

### Añadir un formato de salida

1. Añadir el nombre a `FORMATOS` (lista de la GUI) y la extensión en `run_job`
   (`ext = ".avif" if ... else ".webp"`).
2. Añadir el bloque de opciones en `convert_one` (qué acepta el codificador de Pillow).
3. Ajustar `_encode_ajustando_peso` si el parámetro de calidad se llama distinto.
4. Actualizar manual, README y `Job.formato`.

### Añadir un preset incluido

Añadir una entrada a `PRESETS_BUILTIN` con las mismas claves que devuelve `_ajustes_actuales()`.

### Añadir una opción de conversión

1. Campo nuevo en `Job` con valor por defecto.
2. Control en la UI y su valor en `_ajustes_actuales()`.
3. Uso en `convert_one`.
4. Incluirlo en `_load_config`/`_save_config` si debe recordarse.

### Cambiar el empaquetado

El ZIP se construye en la fase 4 de `run_job`; basta con sustituir `zipfile` por otro formato
(`tarfile`, `py7zr`…) manteniendo la firma `Result.zip_path`.

---

## 8. Decisiones de diseño

| Decisión | Motivo |
|---|---|
| **Un solo archivo** | Distribución trivial, sin paquete que instalar; el `.py` se puede ejecutar directamente. |
| **Tkinter** | Incluido en Python, sin dependencias nativas, suficiente para esta UI. |
| **tkinterdnd2** | Única forma práctica de DnD real en Tk; se degrada con elegancia si falta. |
| **7-Zip externo** | No hay lector RAR fiable en Python puro; 7-Zip/WinRAR están muy extendidos en Windows. |
| **Hilos, no procesos** | Evita serializar imágenes y arrancar intérpretes; Pillow libera el GIL en partes del proceso. |
| **Cola de eventos** | Aísla la UI del hilo de trabajo y evita condiciones de carrera. |
| **Peso por búsqueda binaria** | Garantiza el límite real del archivo, no una estimación. |
| **Configuración en JSON** | Legible, editable y sin dependencias. |

---

## 9. Rendimiento

- La decodificación y la codificación dominan el tiempo; el redimensionado es barato en comparación.
- El paralelismo ayuda sobre todo con fotos grandes y muchos archivos. Con imágenes pequeñas, la
  sobrecarga de hilos puede igualar el tiempo secuencial (por eso el umbral de 4 tareas).
- El peso objetivo multiplica el coste de codificación (hasta 8 pasadas). Para lotes enormes conviene
  fijar calidad en lugar de peso, o combinar `--max-kb` con `--max-dim`.
- El ZIP usa compresión nivel 6: WebP/AVIF ya están comprimidos, así que el ahorro adicional es pequeño.

---

## 10. Pruebas

El motor está diseñado para probarse sin GUI:

```python
job = Job(sources=[Path("fotos")], output_dir=Path("salida"), quality=80, max_dimension=1200)
resultado = {}
run_job(job, lambda t, d: resultado.update({t: d}) if t == "done" else None, threading.Event())
print(resultado["done"])
```

Checklist manual recomendada antes de publicar una versión:

1. Convertir un JPG, un PNG con alfa y un GIF animado.
2. Convertir un ZIP y un RAR; comprobar que la carpeta temporal desaparece.
3. Activar `--max-dim` y `--max-kb` por separado y a la vez.
4. Generar AVIF y abrirlo con el visor de Windows.
5. Activar ZIP y CSV; verificar el contenido y que Excel abre el CSV en columnas.
6. Probar «omitir existentes» ejecutando dos veces el mismo trabajo.
7. Cancelar a mitad y comprobar que lo convertido se conserva.
8. Arrastrar una carpeta sobre el campo de salida.
9. Crear y eliminar el acceso de «Enviar a».
10. Revisar que la configuración se conserva entre sesiones.
