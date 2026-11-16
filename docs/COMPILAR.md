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
pyinstaller --noconfirm --clean --onefile --windowed --name ConversorWebP ^
  --icon icon.ico ^
  --collect-all tkinterdnd2 ^
  --add-data "MANUAL_DE_USO.html;." ^
  --exclude-module numpy --exclude-module matplotlib ^
  conversor_webp.py
```

Resultado: `dist\ConversorWebP.exe` (unos 22 MB).

### Por qué cada opción

| Opción | Motivo |
|---|---|
| `--onefile` | Un único archivo fácil de mover y compartir. |
| `--windowed` | Sin ventana de consola al abrir la GUI. |
| `--icon icon.ico` | Icono del ejecutable y de la barra de tareas. |
| `--collect-all tkinterdnd2` | Copia los binarios TkDND, necesarios para arrastrar y soltar. Sin esto, el `.exe` abre pero no acepta arrastres. |
| `--add-data "MANUAL_DE_USO.html;."` | Incrusta el manual para que `Ayuda → Manual de uso` funcione aunque el HTML no esté al lado. |
| `--exclude-module ...` | Evita arrastrar dependencias que no se usan. |

> En Linux/macOS el separador de `--add-data` es `:` en lugar de `;`. El programa está pensado para
> Windows.

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
