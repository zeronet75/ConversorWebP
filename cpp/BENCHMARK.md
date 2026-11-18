# Pruebas de carga · versión C++ vs Python

Fecha: 17/11/2026. Equipo: Windows 11 Pro, Intel Xeon E5-1680 v3 (8 núcleos / 16 hilos),
64 GB de RAM y NVIDIA RTX 3060. Las pruebas de conversión son **CPU**, con
`--no-gpu` en Python para comparar en igualdad de condiciones.

- C++: `ConversorWebPC.exe` 2.6.2, libwebp 1.6.0, libavif 1.4.2 + libaom y **pool de
  hasta 8 hilos** (una factoría WIC por trabajador).
- Python: `conversor_webp.py` 2.6.2, Pillow 13.0.0 (libwebp 1.6.0) y el `.exe` onefile
  de PyInstaller. Python también convierte en paralelo (hasta 8 trabajadores).
- Caché caliente. Arranque: 5 ejecuciones. Conversión: 3 ejecuciones (se muestra la
  media). Los tiempos de Python incluyen su arranque; el C++ también.

## Arranque

| Versión | Ventana GUI visible | CLI `--version` |
|---|---:|---:|
| **C++** | **0,86 s** | **0,63 s** |
| Python fuente (`pythonw`) | 2,20 s | 1,67 s |
| Python onefile (30 MB) | 7,29 s | 7,39 s |

El C++ arranca **2,3× antes** que Python desde fuente y **7,7× antes** que el onefile.

## Conversión

| Prueba | C++ (pool 8) | Python fuente | Python onefile |
|---|---:|---:|---:|
| 52 imágenes 400×400 → WebP q85 m6 | **11,49 s** | 13,25 s | 20,40 s |
| 20 fotos 6000×4000 → WebP 1600 q85 m6 | **3,69 s** | 4,51 s | 10,23 s |
| 1 foto 6000×4000 → WebP 1600 | **1,49 s** | 2,35 s | 8,01 s |
| 3 fotos 6000×4000 → AVIF 1600 | **1,71 s** | 3,45 s | 8,96 s |

Por imagen, descontando el arranque aproximado:

| Prueba | C++ | Python fuente |
|---|---:|---:|
| 52 pequeñas | 0,21 s/img | 0,22 s/img |
| 20 grandes | 0,15 s/img | 0,13 s/img |
| 1 grande | 0,86 s/img | 0,68 s/img |
| 3 AVIF | 0,36 s/img | 0,59 s/img |

**Lectura:** con el pool, el C++ gana en todos los escenarios totales. En imágenes
grandes su ventaja viene del menor arranque y de no recompilar/extraer como el onefile;
Python sigue siendo algo más rápido por imagen individual gracias a su resampling C.

## Memoria

Lote de 20 fotos de 6000×4000 → WebP 1600:

| Versión | Pico de RAM | Procesos |
|---|---:|---:|
| **C++ (pool 8)** | **1.675,1 MB** | 1 |
| Python fuente | 1.723,1 MB | 2 (lanzador + intérprete) |
| Python onefile | 1.729,3 MB | 2 (padre + hijo) |

El C++ sigue usando algo menos de memoria que Python en el lote grande, pero el pool
multiplica el consumo respecto al modo secuencial anterior (220 MB), porque mantiene
varias imágenes de 24 MP en vuelo. En un equipo con poca RAM conviene reducir el número
de trabajadores.

## Correctitud de los resultados

- 20/20 WebP del lote grande: **píxeles idénticos** entre C++ y Python.
- 52/52 archivos generados en el lote pequeño.
- ZIP con 20 entradas y CSV con 20 filas, sin pérdidas por concurrencia.
- Cancelación probada: se detiene el lote, se conservan las imágenes ya terminadas
  (8 en la prueba) y la aplicación sigue estable.
- 3/3 AVIF generados; diferencia media 0,61/255 y máxima 20/255 frente a Pillow AVIF
  (codificadores y mapeo de calidad distintos), visualmente equivalente.

## Conclusión

- **Arranque:** C++ claramente mejor.
- **Lotes:** C++ ahora gana a Python fuente y al onefile.
- **Memoria:** C++ ligeramente por debajo de Python en el lote grande; mucho más bajo
  si se redujera el pool.
- **Una sola imagen / AVIF:** C++ también gana en tiempo total.

## Reproducir

```powershell
# Lote pequeño: 52 imágenes 400x400
bin\ConversorWebPC.exe <carpeta> -o <salida> -q 85 -m 6

# Lote grande: 20 imágenes 6000x4000
bin\ConversorWebPC.exe <carpeta> -o <salida> -q 85 -m 6 --max-dim 1600

# AVIF
bin\ConversorWebPC.exe <carpeta> -o <salida> -q 85 -m 6 --max-dim 1600 --format avif
```
