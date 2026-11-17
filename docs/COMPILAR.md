# Compilar el ejecutable

Guía para generar `ConversorWebP.exe` desde el código fuente. Se usa **PyInstaller** en modo
*onefile* y *windowed* (sin consola).

---

## 1. Preparar el entorno

```bat
git clone https://github.com/zeronet75/ConversorWebP.git
cd ConversorWebP
python -m venv .venv
.venv\Scripts\activate
python -m pip install --upgrade pip
pip install -r requirements-dev.txt
```

`requirements-dev.txt` instala Pillow, tkinterdnd2 y PyInstaller.

Comprueba que todo carga antes de compilar:

```bat
python -c "import PIL, tkinterdnd2; print(PIL.__version__)"
python conversor_webp.py
```

---

## 2. Compilar

Desde la raíz del repositorio:

```bat
pyinstaller --noconfirm --clean ConversorWebP.spec
```

Resultado: `dist\ConversorWebP.exe` (~24 MB).

La especificación `ConversorWebP.spec` contiene toda la configuración: modo *onefile* sin consola, icono,
manual incrustado, TkDND, exclusiones y la lista de módulos que necesita la carga opcional de PyTorch.

### Por qué el `.spec` y no una línea de comandos

| Elemento | Motivo |
|---|---|
| `--onefile` / `console=False` | Un único archivo, sin ventana de consola. |
| `icon="icon.ico"` | Icono del ejecutable y de la barra de tareas. |
| `collect_all("tkinterdnd2")` | Copia los binarios TkDND; sin ellos el `.exe` abre pero no acepta arrastres. |
| `datas` con el manual | `Ayuda → Manual de uso` funciona aunque el HTML no esté al lado. |
| `excludes=["torch", "torchvision", ...]` | Evita empaquetar ~2,4 GB de CUDA; se cargan del sistema si existen. |
| Toda la stdlib en `hiddenimports` | torch/torchvision importan módulos (`timeit`, `pdb`, `pickletools`, `modulefinder`, `cProfile`, `html.parser`…) que un análisis estático de la app no detecta. |
| `collect_submodules("html", "http", ...)` | Submódulos que torchvision carga de forma dinámica. |

> La lista de la stdlib se genera con `sys.stdlib_module_names`, así que se adapta a la versión de Python
> con la que compiles. En Linux/macOS el separador de datas sería `:` y el programa no está pensado para
> esas plataformas.

### Aceleración GPU en el ejecutable

El `.exe` **no incluye PyTorch**. Al activar la GPU, `conversor_webp.py` busca una instalación existente
(`...\.venv\Lib\site-packages`, Python del sistema o una carpeta `gpu` junto al `.exe`) y la importa en
tiempo de ejecución. Comprueba el resultado con:

```bat
ConversorWebP.exe --diag "%TEMP%\diag.json"
```

Debe aparecer `"gpu_activa": true` cuando encuentra CUDA. Si no, revisa `gpu_motivo` y
`gpu_candidatos`.

---

## 3. Variante `onedir`

Si prefieres arranque instantáneo (el *onefile* descomprime en cada inicio y tarda 2-3 s), usa:

```bat
pyinstaller --noconfirm --clean --windowed --name ConversorWebP ^
  --icon icon.ico --collect-all tkinterdnd2 ^
  --add-data "MANUAL_DE_USO.html;." conversor_webp.py
```

El resultado es `dist\ConversorWebP\` con el `.exe` y sus librerías. Distribuye la carpeta completa.

---

## 4. Comprobar el ejecutable

```bat
:: Diagnóstico: imprime versión, Pillow, extractor, manual y TkDND
ConversorWebP.exe --diag "C:\temp\diag.json"

:: Conversión de prueba
ConversorWebP.exe "C:\Fotos\prueba.zip" -o "C:\Fotos\salida" --zip salida.zip --csv
```

El diagnóstico debe mostrar:

```json
{
  "congelado": true,
  "tkinterdnd2_importado": true,
  "tkdnd_operativo": true,
  "winsound": true,
  "manual": "C:\\...\\MANUAL_DE_USO.html"
}
```

Después, abre la GUI y comprueba arrastrar y soltar, el menú Ayuda y una conversión completa.

---

## 5. Problemas frecuentes

| Síntoma | Causa / solución |
|---|---|
| `PermissionError: [WinError 5]` al compilar | El `.exe` anterior está abierto. Ciérralo (incluido el proceso hijo en el Administrador de tareas) y repite. |
| El `.exe` no acepta arrastrar y soltar | Falta `--collect-all tkinterdnd2` o el antivirus bloqueó `tkdnd`. Revisa el diagnóstico (`tkdnd_operativo`). |
| `Ayuda → Manual` no abre nada | No se incluyó `--add-data` y el HTML no está junto al `.exe`. |
| SmartScreen avisa al ejecutar | Normal sin firma digital. Se elimina comprando un certificado de firma de código. |
| Antivirus marca el `.exe` | Falso positivo habitual de PyInstaller. Añade una exclusión o distribuye el `.py`. |

---

## 6. Icono

Los iconos ya están en el repositorio:

- `icon.ico`: multi-resolución (16 a 256 px) para el ejecutable.
- `icon.png`: 256×256 para el README y documentación.

Si quieres regenerarlos, cualquier editor de imágenes sirve; el `.ico` debe contener al menos 16, 32,
48, 64, 128 y 256 px.

---

## 7. Instalador, versión portable y CI

### Instalador con Inno Setup

```bat
winget install --id JRSoftware.InnoSetup
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" instalador\ConversorWebP.iss
```

Genera `dist_instalador\ConversorWebP-Setup-2.5.0.exe` con accesos en Inicio, desinstalador y el manual.
La configuración se guarda en `%APPDATA%\ConversorWebP`.

### Versión portable

El modo portable se activa con un archivo `portable.txt` junto al `.exe` (o con la variable de entorno
`CONVERSORWEBP_PORTABLE=1`). En ese caso la configuración se escribe en `config.json` al lado del programa.

```bat
mkdir dist_portable\ConversorWebP
copy dist\ConversorWebP.exe dist_portable\ConversorWebP\
copy MANUAL_DE_USO.html dist_portable\ConversorWebP\
copy LEEME.txt dist_portable\ConversorWebP\
copy LICENSE dist_portable\ConversorWebP\
type nul > dist_portable\ConversorWebP\portable.txt
powershell Compress-Archive -Path dist_portable\ConversorWebP -DestinationPath dist\ConversorWebP-Portable-2.5.0.zip -Force
```

### Integración continua

`.github/workflows/pruebas.yml` ejecuta en cada push y pull request:

```bat
python -m pip install -r requirements.txt
python -m py_compile conversor_webp.py
python -m unittest discover -s tests -v
```
