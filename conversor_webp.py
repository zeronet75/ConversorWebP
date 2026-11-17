"""
Conversor WebP 2.0
==================

Convierte imagenes (JPG, PNG, BMP, TIFF, GIF, ICO...) a WebP.

Novedades de la version 2:
  * Arrastrar y soltar desde el Explorador de Windows (archivos, carpetas,
    .zip, .rar y .7z).
  * Extraccion automatica de comprimidos con 7-Zip o WinRAR.
  * Opcion de empaquetar el resultado en un .zip dentro de la carpeta
    de salida elegida.
  * Interfaz grafica y modo consola.

Requisitos: Python 3.8+, Pillow, tkinter, tkinterdnd2 (opcional, para
arrastrar y soltar). Para .rar/.7z hace falta 7-Zip o WinRAR instalados.
"""

from __future__ import annotations

import ctypes
import csv
import hashlib
import importlib
import importlib.util
import io
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import urllib.request
import webbrowser
import zipfile
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

try:
    from PIL import Image, ImageCms, ImageOps, ImageTk

    Image.init()
except ImportError:  # pragma: no cover
    import tkinter.messagebox as mb

    mb.showerror(
        "Falta Pillow",
        "No se encontro la libreria Pillow.\n\nInstalala con:\n    pip install pillow",
    )
    raise SystemExit(1)

import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, simpledialog, ttk

try:  # solo existe en Windows, que es donde corre la app
    import winsound
except Exception:  # pragma: no cover
    winsound = None  # type: ignore[assignment]

try:
    from tkinterdnd2 import DND_FILES, TkinterDnD

    DND_AVAILABLE = True
    BaseTk = TkinterDnD.Tk
except Exception:  # pragma: no cover - la app sigue funcionando sin arrastrar
    DND_FILES = None  # type: ignore[assignment]
    TkinterDnD = None  # type: ignore[assignment]
    DND_AVAILABLE = False
    BaseTk = tk.Tk


APP_NAME = "Conversor WebP"
APP_VERSION = "2.5.1"

try:  # soporte HEIC/HEIF (iPhone) si pillow-heif esta instalado
    import pillow_heif

    pillow_heif.register_heif_opener()
    HEIF_DISPONIBLE = True
except Exception:  # pragma: no cover
    pillow_heif = None  # type: ignore[assignment]
    HEIF_DISPONIBLE = False

IMAGE_EXTS = {
    ".jpg", ".jpeg", ".jfif", ".jpe", ".png", ".bmp", ".dib", ".gif",
    ".tif", ".tiff", ".ico", ".ppm", ".pgm", ".pbm", ".tga", ".pcx",
    ".dds", ".webp", ".heic", ".heif",
}
ARCHIVE_EXTS = {".zip", ".rar", ".7z"}
DIMENSIONES = ["Sin cambio", "2560", "1920", "1600", "1200", "800", "512"]

# Presets incluidos (nombre -> ajustes de imagen)
PRESETS_BUILTIN: dict[str, dict] = {
    "Web (85 · 1920 px)": {
        "quality": 85, "method": 6, "lossless": False, "flatten": False,
        "background": "#ffffff", "reencode_webp": False,
        "max_dimension": 1920, "target_kb": 0,
    },
    "Catalogo (80 · 1200 px)": {
        "quality": 80, "method": 6, "lossless": False, "flatten": False,
        "background": "#ffffff", "reencode_webp": False,
        "max_dimension": 1200, "target_kb": 0,
    },
    "Iconos sin perdida": {
        "quality": 85, "method": 6, "lossless": True, "flatten": False,
        "background": "#ffffff", "reencode_webp": False,
        "max_dimension": 0, "target_kb": 0,
    },
    "Maxima calidad (95)": {
        "quality": 95, "method": 6, "lossless": False, "flatten": False,
        "background": "#ffffff", "reencode_webp": False,
        "max_dimension": 0, "target_kb": 0,
    },
}

def _ruta_config() -> Path:
    """Config en %APPDATA% o junto al programa en modo portatil."""
    base = (
        Path(sys.executable).parent
        if getattr(sys, "frozen", False)
        else Path(__file__).resolve().parent
    )
    if (base / "portable.txt").is_file() or os.environ.get("CONVERSORWEBP_PORTABLE") == "1":
        return base / "config.json"
    return Path(os.getenv("APPDATA", Path.home())) / "ConversorWebP" / "config.json"


CONFIG_FILE = _ruta_config()

# Paleta
BG = "#eef1f6"
CARD = "#ffffff"
ACCENT = "#1a73e8"
ACCENT_DARK = "#1557b0"
TEXT = "#1f2933"
MUTED = "#5f6b7a"
OK = "#137333"
ERR = "#c5221f"


# --------------------------------------------------------------------------- #
#  Utilidades de archivos y comprimidos
# --------------------------------------------------------------------------- #
def _no_window_flags() -> int:
    if os.name == "nt":
        return getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
    return 0


def _candidate_extractors():
    pf = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    pf86 = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    for p in (
        pf / "7-Zip" / "7z.exe",
        pf86 / "7-Zip" / "7z.exe",
        pf / "WinRAR" / "UnRAR.exe",
        pf / "WinRAR" / "WinRAR.exe",
        pf86 / "WinRAR" / "UnRAR.exe",
    ):
        if p.is_file():
            yield p
    for name in ("7z.exe", "7za.exe", "unrar.exe", "unrar"):
        found = shutil.which(name)
        if found:
            yield Path(found)


def find_extractor() -> Path | None:
    for p in _candidate_extractors():
        return p
    return None


def extract_with_tool(tool: Path, archive: Path, dest: Path) -> None:
    nombre = tool.name.lower()
    if nombre.startswith("unrar"):
        cmd = [str(tool), "x", "-o+", "-inul", str(archive), str(dest) + os.sep]
    elif nombre.startswith("winrar"):
        cmd = [str(tool), "x", "-y", "-inul", str(archive), str(dest) + os.sep]
    else:
        cmd = [str(tool), "x", "-y", "-bso0", "-bsp0", f"-o{dest}", str(archive)]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        errors="replace",
        creationflags=_no_window_flags(),
    )
    if proc.returncode not in (0, 1):
        detalle = (proc.stderr or proc.stdout or "").strip().splitlines()
        detalle = detalle[-1] if detalle else f"codigo {proc.returncode}"
        raise RuntimeError(f"{tool.name} no pudo extraer: {detalle}")


def extract_zip_builtin(archive: Path, dest: Path) -> None:
    dest_abs = dest.resolve()
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            nombre = info.filename.replace("\\", "/")
            if not nombre or nombre.endswith("/"):
                continue
            objetivo = (dest / nombre).resolve()
            if dest_abs not in objetivo.parents and objetivo != dest_abs:
                raise RuntimeError(f"Ruta no permitida dentro del ZIP: {nombre}")
            objetivo.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as origen, open(objetivo, "wb") as salida:
                shutil.copyfileobj(origen, salida, 1024 * 1024)


def extract_archive(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    error_zip: Exception | None = None
    if archive.suffix.lower() == ".zip":
        try:
            extract_zip_builtin(archive, dest)
            return
        except Exception as e:  # noqa: BLE001
            error_zip = e  # ZIP cifrado o compresion rara: se intenta con 7-Zip
    tool = find_extractor()
    if tool is None:
        if error_zip is not None:
            raise RuntimeError(
                f"No se pudo extraer {archive.name}: {error_zip}\n"
                "Instala 7-Zip o WinRAR para los formatos comprimidos avanzados."
            )
        raise RuntimeError(
            "Para abrir archivos .rar o .7z hace falta 7-Zip o WinRAR instalado.\n"
            "Los .zip funcionan sin instalar nada."
        )
    extract_with_tool(tool, archive, dest)


def is_image(path: Path) -> bool:
    return path.suffix.lower() in IMAGE_EXTS


def collect_images_from_dir(root: Path) -> list[Path]:
    return sorted(p for p in root.rglob("*") if p.is_file() and is_image(p))


def hex_to_rgb(value: str) -> tuple[int, int, int]:
    """Convierte #RGB o #RRGGBB en una tupla RGB. Lanza ValueError si no es valido."""
    texto = (value or "").strip().lstrip("#")
    if len(texto) == 3:
        texto = "".join(c * 2 for c in texto)
    if len(texto) != 6:
        raise ValueError(f"color no valido: {value!r} (usa #RRGGBB)")
    try:
        return tuple(int(texto[i: i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]
    except ValueError as e:
        raise ValueError(f"color no valido: {value!r} (usa #RRGGBB)") from e


_RESERVADOS = {
    "CON", "PRN", "AUX", "NUL",
    *[f"COM{i}" for i in range(1, 10)],
    *[f"LPT{i}" for i in range(1, 10)],
}


def nombre_seguro(nombre: str, defecto: str) -> tuple[str, bool]:
    """Devuelve un nombre de archivo válido para Windows y si hubo cambios."""
    original = (nombre or "").strip()
    limpio = Path(original).name
    limpio = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", limpio).strip(" .")
    if not limpio:
        limpio = defecto
    if limpio.split(".")[0].upper() in _RESERVADOS:
        limpio = "_" + limpio
    return limpio, limpio != original


def _ps_quote(texto: str) -> str:
    """Escapa una cadena para incluirla entre comillas simples de PowerShell."""
    return str(texto).replace("'", "''")


def version_tupla(texto: str) -> tuple[int, ...]:
    """Convierte 'v2.5.1' o '2.5' en una tupla comparable."""
    return tuple(int(p) for p in re.findall(r"\d+", str(texto))[:3])


def consultar_ultima_version(timeout: int = 8) -> tuple[str, str] | None:
    """Consulta la ultima release publicada en GitHub. Devuelve (tag, url)."""
    url = "https://api.github.com/repos/zeronet75/ConversorWebP/releases/latest"
    peticion = urllib.request.Request(
        url,
        headers={
            "User-Agent": f"ConversorWebP/{APP_VERSION}",
            "Accept": "application/vnd.github+json",
        },
    )
    with urllib.request.urlopen(peticion, timeout=timeout) as respuesta:
        datos = json.loads(respuesta.read().decode("utf-8"))
    etiqueta = str(datos.get("tag_name", "")).strip()
    if not etiqueta:
        return None
    return etiqueta, str(datos.get("html_url", ""))


def aplicar_patron(patron: str, src: Path, rel: Path, job: Job, ext: str) -> Path:
    """Construye la ruta relativa de salida segun el patron de nombres."""
    if not patron.strip():
        return rel.with_suffix(ext)

    ancho = alto = 0
    try:
        with Image.open(src) as im:
            ancho, alto = im.size
    except Exception:
        pass
    if job.max_dimension and max(ancho, alto) > job.max_dimension:
        escala = job.max_dimension / max(ancho, alto)
        ancho, alto = max(1, round(ancho * escala)), max(1, round(alto * escala))

    tokens = {
        "nombre": rel.stem,
        "ancho": str(ancho),
        "alto": str(alto),
        "formato": ext.lstrip("."),
        "calidad": str(job.quality),
        "fecha": datetime.now().strftime("%Y%m%d-%H%M%S"),
        "hash": "",
    }
    texto = patron
    if "{hash}" in texto:
        try:
            tokens["hash"] = hashlib.sha1(src.read_bytes()).hexdigest()[:10]
        except Exception:
            tokens["hash"] = ""
    for clave, valor in tokens.items():
        texto = texto.replace("{" + clave + "}", valor)

    texto, _ = nombre_seguro(texto, rel.stem)
    if not texto:
        texto = rel.stem
    return rel.parent / (texto + ext)


def ruta_manual() -> Path | None:
    """Localiza el manual HTML junto al ejecutable o dentro del paquete."""
    candidatos: list[Path] = []
    if getattr(sys, "frozen", False):
        candidatos.append(Path(sys.executable).parent / "MANUAL_DE_USO.html")
        base = getattr(sys, "_MEIPASS", None)
        if base:
            candidatos.append(Path(base) / "MANUAL_DE_USO.html")
    else:
        candidatos.append(Path(__file__).resolve().parent / "MANUAL_DE_USO.html")
    for c in candidatos:
        if c.is_file():
            return c
    return None


# --------------------------------------------------------------------------- #
#  Modelo del trabajo
# --------------------------------------------------------------------------- #
@dataclass
class Job:
    sources: list[Path]
    output_dir: Path
    quality: int = 85
    method: int = 6
    keep_structure: bool = True
    overwrite: bool = True
    reencode_webp: bool = False
    flatten: bool = False
    background: str = "#ffffff"
    lossless: bool = False
    make_zip: bool = False
    zip_name: str = ""
    max_dimension: int = 0      # 0 = sin redimensionar; si > 0, lado maximo en px
    target_kb: int = 0          # 0 = sin limite; si > 0, peso maximo por imagen
    formato: str = "WEBP"       # "WEBP" o "AVIF"
    csv_report: bool = False    # guardar un informe CSV en la carpeta de salida
    usar_gpu: bool = False      # modo experimental CUDA (desactivado por defecto)
    gpu_forzar: bool = False    # usar GPU aunque el lote sea pequeño
    name_pattern: str = ""      # patron de nombre; vacio = nombre original
    dry_run: bool = False       # solo analizar: no escribe archivos


@dataclass
class Result:
    converted: int = 0
    skipped: int = 0
    failed: list[tuple[Path, str]] = field(default_factory=list)
    bytes_in: int = 0
    bytes_out: int = 0
    archives: int = 0
    zip_path: Path | None = None
    csv_path: Path | None = None
    cancelled: bool = False
    dry_run: bool = False


# --------------------------------------------------------------------------- #
#  Aceleración por GPU (CUDA opcional)
# --------------------------------------------------------------------------- #
JPEG_EXTS = {".jpg", ".jpeg", ".jfif", ".jpe"}
GPU_MIN_PIXELES = 8_000_000  # por debajo de ~8 MP la GPU no compensa
GPU_MIN_LOTE = 30            # CUDA tarda varios segundos en cargar; solo compensa en lotes grandes


@dataclass
class EstadoGPU:
    hardware: list[str] = field(default_factory=list)
    nvidia: str = ""
    driver: str = ""
    memoria: str = ""
    activa: bool = False
    intentado: bool = False
    motivo: str = ""
    torch: str = ""
    torchvision: str = ""
    ruta: str = ""


_GPU = EstadoGPU()
_GPU_LOCK = threading.Semaphore(2)
_TORCH = None
_TORCHVISION = None
_DECODE_JPEG = None
_IMAGE_READ_MODE = None
_FUNCIONAL = None


def _leer_gpus_registro() -> list[str]:
    """Enumera las tarjetas gráficas desde el registro de Windows (rápido)."""
    nombres: list[str] = []
    try:
        import winreg
    except Exception:  # pragma: no cover
        return nombres
    clave = r"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}"
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, clave) as raiz:
            i = 0
            while True:
                try:
                    sub = winreg.EnumKey(raiz, i)
                except OSError:
                    break
                i += 1
                try:
                    with winreg.OpenKey(raiz, sub) as k:
                        nombre, _ = winreg.QueryValueEx(k, "DriverDesc")
                except OSError:
                    continue
                nombre = str(nombre).strip()
                if nombre and nombre not in nombres:
                    nombres.append(nombre)
    except Exception:
        pass
    return nombres


def _leer_nvidia_smi() -> None:
    exe = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "nvidia-smi.exe"
    if not exe.exists():
        encontrado = shutil.which("nvidia-smi")
        if not encontrado:
            return
        exe = Path(encontrado)
    try:
        proc = subprocess.run(
            [str(exe), "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"],
            capture_output=True,
            text=True,
            timeout=6,
            creationflags=_no_window_flags(),
        )
    except Exception:
        return
    if proc.returncode != 0 or not (proc.stdout or "").strip():
        return
    partes = [p.strip() for p in proc.stdout.strip().splitlines()[0].split(",")]
    if len(partes) >= 3:
        _GPU.nvidia, _GPU.driver, _GPU.memoria = partes[0], partes[1], partes[2]


def detectar_hardware_gpu(refrescar: bool = False) -> EstadoGPU:
    if _GPU.hardware and not refrescar:
        return _GPU
    _GPU.hardware = _leer_gpus_registro()
    _leer_nvidia_smi()
    return _GPU


def nombre_gpu() -> str:
    detectar_hardware_gpu()
    return _GPU.nvidia or (_GPU.hardware[0] if _GPU.hardware else "")


def _leer_gpu_path_config() -> str:
    try:
        data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        return str(data.get("gpu_path", "") or "")
    except Exception:
        return ""


def candidatos_torch() -> list[Path]:
    """Carpetas site-packages donde puede vivir un PyTorch con CUDA."""
    rutas: list[Path] = []

    def add(ruta) -> None:
        try:
            p = Path(ruta)
        except Exception:
            return
        if p not in rutas:
            rutas.append(p)

    base = (
        Path(sys.executable).parent
        if getattr(sys, "frozen", False)
        else Path(__file__).resolve().parent
    )
    for sub in ("gpu", "torch_gpu", "site-packages", "gpu/Lib/site-packages", ".venv/Lib/site-packages"):
        add(base / sub)

    configurada = _leer_gpu_path_config()
    if configurada:
        add(configurada)
        add(Path(configurada) / "Lib" / "site-packages")

    add(Path(os.environ.get("USERPROFILE", "")) / ".venv" / "Lib" / "site-packages")
    local = Path(os.environ.get("LOCALAPPDATA", ""))
    for p in local.glob("Programs/Python/Python3*/Lib/site-packages"):
        add(p)
    for p in Path("C:/").glob("Python3*/Lib/site-packages"):
        add(p)

    for unidad in ("C:/", "D:/", "E:/", "F:/"):
        try:
            raiz = Path(unidad)
            if not raiz.exists():
                continue
            for patron in ("*/.venv/Lib/site-packages", "*/Lib/site-packages", "*/*/Lib/site-packages"):
                for p in raiz.glob(patron):
                    add(p)
        except OSError:
            continue

    return [p for p in rutas if (p / "torch" / "__init__.py").is_file()]


def _cargar_torch():
    global _TORCH
    try:
        _TORCH = importlib.import_module("torch")
        return _TORCH
    except Exception as primer_error:
        errores: list[str] = []
        for ruta in candidatos_torch():
            sys.path.insert(0, str(ruta))
            try:
                _TORCH = importlib.import_module("torch")
                _GPU.ruta = str(ruta)
                return _TORCH
            except Exception as e:  # noqa: BLE001
                errores.append(f"{ruta}: {type(e).__name__}: {e}")
                try:
                    sys.path.remove(str(ruta))
                except ValueError:
                    pass
                for mod in [m for m in sys.modules if m == "torch" or m.startswith("torch.")]:
                    del sys.modules[mod]
        if errores:
            raise RuntimeError("; ".join(errores[-2:]))
        raise primer_error


def iniciar_gpu(refrescar: bool = False) -> EstadoGPU:
    """Detecta la GPU e intenta cargar PyTorch CUDA. Se cachea el resultado."""
    global _TORCH, _TORCHVISION, _DECODE_JPEG, _IMAGE_READ_MODE, _FUNCIONAL
    if _GPU.intentado and not refrescar:
        return _GPU

    detectar_hardware_gpu(refrescar=True)
    _GPU.intentado = True
    _GPU.activa = False
    _GPU.motivo = ""
    _GPU.torch = ""
    _GPU.torchvision = ""
    _GPU.ruta = ""

    if not _GPU.hardware:
        _GPU.motivo = "no se detecto ninguna tarjeta grafica"
        return _GPU

    try:
        torch = _cargar_torch()
    except Exception as e:  # noqa: BLE001
        _GPU.motivo = f"PyTorch no disponible: {e}"
        return _GPU

    try:
        if not bool(torch.cuda.is_available()):
            _GPU.motivo = "PyTorch instalado sin CUDA o sin driver compatible"
            return _GPU
        _TORCHVISION = importlib.import_module("torchvision")
        _io = importlib.import_module("torchvision.io")
        _DECODE_JPEG = getattr(_io, "decode_jpeg")
        _IMAGE_READ_MODE = importlib.import_module("torchvision.io.image").ImageReadMode
        _FUNCIONAL = importlib.import_module("torch.nn.functional")
    except Exception as e:  # noqa: BLE001
        _GPU.motivo = f"torchvision no disponible: {e}"
        return _GPU

    _GPU.activa = True
    _GPU.torch = getattr(torch, "__version__", "?")
    _GPU.torchvision = getattr(_TORCHVISION, "__version__", "?")
    return _GPU


def _orientar_tensor(tensor, orientacion: int):
    """Aplica la orientación EXIF a un tensor CHW en GPU."""
    if orientacion == 2:
        return _TORCH.flip(tensor, [2])
    if orientacion == 3:
        return _TORCH.rot90(tensor, 2, [1, 2])
    if orientacion == 4:
        return _TORCH.flip(tensor, [1])
    if orientacion == 5:
        return _TORCH.transpose(tensor, 1, 2)
    if orientacion == 6:
        return _TORCH.rot90(tensor, -1, [1, 2])
    if orientacion == 7:
        return _TORCH.flip(_TORCH.transpose(tensor, 1, 2), [1, 2])
    if orientacion == 8:
        return _TORCH.rot90(tensor, 1, [1, 2])
    return tensor


def convertir_jpeg_gpu(src: Path, dst: Path, job: Job) -> tuple[int, int, str] | None:
    """Ruta GPU para JPEG grandes que se van a reducir. None si no aplica."""
    if not (_GPU.activa and job.usar_gpu and job.max_dimension and _DECODE_JPEG is not None):
        return None
    if src.suffix.lower() not in JPEG_EXTS:
        return None

    try:
        with Image.open(src) as probe:
            if probe.format != "JPEG" or probe.mode != "RGB":
                return None
            ancho, alto = probe.size
            if max(ancho, alto) <= job.max_dimension:
                return None
            if ancho * alto < GPU_MIN_PIXELES:
                return None
            orientacion = probe.getexif().get(274, 1)
            icc = probe.info.get("icc_profile")

        datos = src.read_bytes()
        with _GPU_LOCK:
            tensor = _DECODE_JPEG(
                _TORCH.frombuffer(bytearray(datos), dtype=_TORCH.uint8),
                device="cuda",
                mode=_IMAGE_READ_MODE.RGB,
            )
            tensor = _orientar_tensor(tensor, int(orientacion))
            alto, ancho = int(tensor.shape[1]), int(tensor.shape[2])
            if max(alto, ancho) > job.max_dimension:
                escala = job.max_dimension / max(alto, ancho)
                nuevo = (max(1, round(alto * escala)), max(1, round(ancho * escala)))
                tensor = _FUNCIONAL.interpolate(
                    tensor.unsqueeze(0).float(), size=nuevo, mode="bicubic", antialias=True
                )
                tensor = tensor.clamp_(0, 255).to(_TORCH.uint8).squeeze(0)
            arreglo = tensor.permute(1, 2, 0).contiguous().cpu().numpy()

        imagen = Image.fromarray(arreglo)
        aviso, tam_out = _codificar(imagen, dst, job, icc)
        return src.stat().st_size, tam_out, aviso
    except Exception:  # noqa: BLE001 - cualquier fallo cae al camino CPU
        return None


def contar_jpeg_grandes(tareas, job: Job, limite: int = GPU_MIN_LOTE) -> int:
    """Cuenta JPEG que se beneficiarían de la GPU (hasta `limite`)."""
    if not job.max_dimension:
        return 0
    cuenta = 0
    for src, _rel, _dst in tareas:
        if src.suffix.lower() not in JPEG_EXTS:
            continue
        try:
            with Image.open(src) as im:
                if im.format != "JPEG" or im.mode != "RGB":
                    continue
                ancho, alto = im.size
        except Exception:
            continue
        if max(ancho, alto) > job.max_dimension and ancho * alto >= GPU_MIN_PIXELES:
            cuenta += 1
            if cuenta >= limite:
                return cuenta
    return cuenta


def _encode_imagen(im: Image.Image, formato: str, opciones: dict) -> bytes:
    buffer = io.BytesIO()
    im.save(buffer, formato, **opciones)
    return buffer.getvalue()


def _encode_ajustando_peso(
    im: Image.Image, formato: str, opciones: dict, objetivo_kb: int, calidad_max: int
) -> tuple[bytes, str]:
    """Busca la calidad mas alta cuyo peso quede por debajo del objetivo."""
    objetivo = objetivo_kb * 1024
    opciones = dict(opciones)
    mejor: bytes | None = None

    bajo, alto = 1, max(1, calidad_max)
    for _ in range(7):
        if bajo > alto:
            break
        medio = (bajo + alto) // 2
        opciones["quality"] = medio
        datos = _encode_imagen(im, formato, opciones)
        if len(datos) <= objetivo:
            mejor = datos
            bajo = medio + 1
        else:
            alto = medio - 1

    if mejor is None:
        opciones["quality"] = 1
        datos = _encode_imagen(im, formato, opciones)
        if len(datos) <= objetivo:
            return datos, ""
        return datos, f"no se alcanzo {objetivo_kb} KB (minimo {len(datos) / 1024:.0f} KB)"

    return mejor, ""


def _perfil_srgb() -> bytes | None:
    """Bytes de un perfil sRGB estándar (o None si ImageCms no está disponible)."""
    try:
        return ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
    except Exception:
        return None


def _codificar(
    im: Image.Image, dst: Path, job: Job, icc_salida: bytes | None, aviso: str = ""
) -> tuple[str, int]:
    """Codifica la imagen final (WebP/AVIF). Devuelve (aviso, bytes de salida)."""
    formato = "AVIF" if str(job.formato).upper() == "AVIF" else "WEBP"
    tiene_alfa = im.mode == "RGBA"

    if formato == "AVIF":
        opciones: dict = {"speed": 6 if job.method >= 4 else 8}
        if job.lossless:
            opciones["lossless"] = True
        else:
            opciones["quality"] = job.quality
    else:
        opciones = {"method": job.method}
        if job.lossless:
            opciones["lossless"] = True
        else:
            opciones["quality"] = job.quality
        if tiene_alfa:
            opciones["exact"] = True

    if icc_salida:
        opciones["icc_profile"] = icc_salida

    if job.target_kb > 0 and not job.lossless:
        datos, nota = _encode_ajustando_peso(im, formato, opciones, job.target_kb, job.quality)
        if nota:
            aviso = f"{aviso}; {nota}" if aviso else nota
    else:
        if job.target_kb > 0 and job.lossless:
            aviso = (
                f"{aviso}; el peso maximo no aplica en modo sin perdida"
                if aviso
                else "el peso maximo no aplica en modo sin perdida"
            )
        datos = _encode_imagen(im, formato, opciones)

    if job.dry_run:
        return aviso, len(datos)

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(datos)
    return aviso, len(datos)


def convert_one(src: Path, dst: Path, job: Job) -> tuple[int, int, str]:
    """Convierte una imagen. Devuelve (bytes_in, bytes_out, aviso)."""
    resultado_gpu = convertir_jpeg_gpu(src, dst, job)
    if resultado_gpu is not None:
        return resultado_gpu

    aviso = ""
    with Image.open(src) as im:
        fotogramas = getattr(im, "n_frames", 1)
        if fotogramas > 1:
            aviso = f"animada ({fotogramas} fotogramas), se usa el primero"
        im.seek(0)
        im.load()

        es_cmyk = im.mode == "CMYK"
        icc_salida: bytes | None = im.info.get("icc_profile")

        try:  # corrige fotos giradas por la camara (EXIF Orientation)
            im = ImageOps.exif_transpose(im) or im
        except Exception:
            pass

        # CMYK (imprenta): convertir con su perfil a sRGB; el perfil CMYK no vale para RGB
        if es_cmyk:
            convertida = None
            if icc_salida:
                try:
                    perfil = ImageCms.ImageCmsProfile(io.BytesIO(icc_salida))
                    convertida = ImageCms.profileToProfile(
                        im, perfil, ImageCms.createProfile("sRGB"), outputMode="RGB"
                    )
                    icc_salida = _perfil_srgb()
                except Exception:
                    convertida = None
            if convertida is None:
                im = im.convert("RGB")
                icc_salida = None
            else:
                im = convertida

        # Transparencia: se detecta antes de redimensionar y se pasa a RGBA
        tiene_alfa = im.mode in ("RGBA", "LA", "PA", "RGBa") or (
            im.mode == "P" and "transparency" in im.info
        )
        if tiene_alfa and not job.flatten and im.mode != "RGBA":
            im = im.convert("RGBA")

        # Redimensionado: solo reduce, nunca amplia imagenes pequenas
        if job.max_dimension and max(im.size) > job.max_dimension:
            im.thumbnail((job.max_dimension, job.max_dimension), Image.LANCZOS)

        # Modo final
        if job.flatten and tiene_alfa:
            rgba = im.convert("RGBA")
            fondo = Image.new("RGB", rgba.size, hex_to_rgb(job.background))
            fondo.paste(rgba, mask=rgba.split()[-1])
            im = fondo
        elif not job.flatten and tiene_alfa:
            if im.mode != "RGBA":
                im = im.convert("RGBA")
        elif im.mode not in ("RGB", "L"):
            im = im.convert("RGB")

        aviso, tam_out = _codificar(im, dst, job, icc_salida, aviso)

    return src.stat().st_size, tam_out, aviso


def run_job(job: Job, emit, cancel_event: threading.Event) -> None:
    """Ejecuta el trabajo completo. emit(tipo, datos) notifica a la interfaz."""
    temp_root: Path | None = None
    res = Result()
    producidos: list[Path] = []
    filas: list[tuple] = []  # informe CSV
    formato = "AVIF" if str(job.formato).upper() == "AVIF" else "WEBP"
    ext = ".avif" if formato == "AVIF" else ".webp"
    res.dry_run = job.dry_run
    try:
        temp_root = Path(tempfile.mkdtemp(prefix="conversor_webp_"))
        entradas: list[tuple[Path, Path]] = []  # (archivo, ruta relativa)
        # Si hay varios comprimidos se antepone su nombre para no mezclar contenido
        comprimidos = [s for s in job.sources if s.suffix.lower() in ARCHIVE_EXTS]
        prefijar_comprimidos = len(comprimidos) > 1
        n_archivo = 0

        for src in job.sources:
            if cancel_event.is_set():
                break
            if src.is_dir():
                for f in collect_images_from_dir(src):
                    entradas.append((f, f.relative_to(src)))
            elif src.suffix.lower() in ARCHIVE_EXTS:
                emit("status", f"Extrayendo {src.name}...")
                emit("log", ("info", f"Extrayendo {src.name} ..."))
                n_archivo += 1
                destino = temp_root / f"arc{n_archivo}_{src.stem}"
                try:
                    extract_archive(src, destino)
                    res.archives += 1
                except Exception as e:  # noqa: BLE001
                    emit("log", ("err", f"FALLO    {src.name}: {e}"))
                    res.failed.append((src, str(e)))
                    continue
                dentro = collect_images_from_dir(destino)
                emit("log", ("muted", f"         {len(dentro)} imagen(es) dentro de {src.name}"))
                base = Path(src.stem) if prefijar_comprimidos else Path()
                for f in dentro:
                    entradas.append((f, base / f.relative_to(destino)))
            elif is_image(src):
                entradas.append((src, Path(src.name)))
            else:
                emit("log", ("muted", f"OMITIDA  {src.name} (no es imagen ni comprimido)"))

        if not job.reencode_webp:
            entradas = [e for e in entradas if e[0].suffix.lower() != ext]

        total = len(entradas)
        emit("plan", total)
        if total == 0:
            res.cancelled = cancel_event.is_set()
            emit("done", res)
            return

        # ---- fase 1: calcular rutas de salida y omisiones ----
        usados: set[str] = set()
        tareas: list[tuple[Path, Path, Path]] = []
        for src, rel in entradas:
            if not job.keep_structure:
                rel = Path(rel.name)
            rel_salida = aplicar_patron(job.name_pattern, src, rel, job, ext)
            dst = job.output_dir / rel_salida
            clave = str(dst).lower()
            if clave in usados:
                dst = dst.with_name(f"{dst.stem}_{src.suffix.lstrip('.').lower()}{ext}")
                clave = str(dst).lower()
            usados.add(clave)

            if not job.dry_run and dst.exists() and not job.overwrite:
                res.skipped += 1
                nombre = dst.relative_to(job.output_dir).as_posix()
                filas.append((nombre, src.name, "", "", "", "omitida", "ya existia"))
                emit("log", ("muted", f"OMITIDA  {nombre}  (ya existe)"))
                emit(
                    "fila",
                    {
                        "archivo": nombre,
                        "entrada": None,
                        "salida": None,
                        "ahorro": None,
                        "estado": "omitida",
                        "nota": "ya existia",
                        "ruta": str(dst),
                    },
                )
                continue
            tareas.append((src, rel, dst))

        hechas = total - len(tareas)
        if hechas:
            emit("progress", (hechas, total))

        # ---- GPU: preparar solo si el lote lo amortiza ----
        if job.usar_gpu and job.max_dimension:
            n_grandes = contar_jpeg_grandes(tareas, job)
            if n_grandes >= GPU_MIN_LOTE or job.gpu_forzar:
                emit("status", "Preparando aceleracion GPU...")
                estado = iniciar_gpu()
                if estado.activa:
                    emit(
                        "log",
                        (
                            "info",
                            f"GPU activa: {estado.nvidia or estado.hardware[0]} · CUDA · "
                            f"torch {estado.torch}",
                        ),
                    )
                elif estado.hardware:
                    emit(
                        "log",
                        ("muted", f"GPU detectada ({nombre_gpu()}) sin aceleracion: {estado.motivo}"),
                    )
            elif n_grandes:
                emit(
                    "log",
                    (
                        "muted",
                        f"GPU no usada: {n_grandes} foto(s) grande(s). CUDA tarda varios "
                        "segundos en cargar y no compensa en lotes pequenos (fuerza con --gpu).",
                    ),
                )

        # ---- fase 2: convertir (en paralelo si hay lote) ----
        def exito(src: Path, dst: Path, tam_in: int, tam_out: int, aviso: str) -> None:
            if not job.dry_run:
                producidos.append(dst)
            res.converted += 1
            res.bytes_in += tam_in
            res.bytes_out += tam_out
            nombre = dst.relative_to(job.output_dir).as_posix()
            ahorro = (1 - tam_out / tam_in) * 100 if tam_in else 0
            estado = "analizada" if job.dry_run else "convertida"
            filas.append(
                (nombre, src.name, round(tam_in / 1024, 1), round(tam_out / 1024, 1),
                 round(ahorro, 1), estado, aviso)
            )
            nota = f"  ({aviso})" if aviso else ""
            prefijo = "ANALIZADA" if job.dry_run else "OK       "
            emit(
                "log",
                ("ok", f"{prefijo} {nombre}   {tam_in / 1024:.1f} KB -> {tam_out / 1024:.1f} KB{nota}"),
            )
            emit(
                "fila",
                {
                    "archivo": nombre,
                    "entrada": round(tam_in / 1024, 1),
                    "salida": round(tam_out / 1024, 1),
                    "ahorro": round(ahorro, 1),
                    "estado": estado,
                    "nota": aviso,
                    "ruta": str(src) if job.dry_run else str(dst),
                },
            )

        def fallo(src: Path, dst: Path, error: Exception) -> None:
            res.failed.append((src, str(error)))
            try:
                nombre = dst.relative_to(job.output_dir).as_posix()
            except Exception:
                nombre = dst.name
            filas.append((nombre, src.name, "", "", "", "fallo", str(error)))
            emit("log", ("err", f"FALLO    {src.name}   {error}"))
            emit(
                "fila",
                {
                    "archivo": nombre,
                    "entrada": None,
                    "salida": None,
                    "ahorro": None,
                    "estado": "fallo",
                    "nota": str(error),
                    "ruta": str(src),
                },
            )

        trabajadores = min(8, max(2, os.cpu_count() or 4))
        paralelo = len(tareas) >= 4 and trabajadores > 1

        if paralelo:
            emit("status", f"Convirtiendo {len(tareas)} imagenes ({trabajadores} en paralelo)...")
            with ThreadPoolExecutor(max_workers=trabajadores) as pool:
                futuros = {
                    pool.submit(convert_one, src, dst, job): (src, dst)
                    for src, _rel, dst in tareas
                }
                for futuro in as_completed(futuros):
                    src, dst = futuros[futuro]
                    if cancel_event.is_set():
                        res.cancelled = True
                        emit("log", ("warn", "Cancelado por el usuario."))
                        for f in futuros:
                            f.cancel()
                        break
                    try:
                        tam_in, tam_out, aviso = futuro.result()
                        exito(src, dst, tam_in, tam_out, aviso)
                    except Exception as e:  # noqa: BLE001
                        fallo(src, dst, e)
                    hechas += 1
                    emit("progress", (hechas, total))
        else:
            for src, _rel, dst in tareas:
                if cancel_event.is_set():
                    res.cancelled = True
                    emit("log", ("warn", "Cancelado por el usuario."))
                    break
                try:
                    tam_in, tam_out, aviso = convert_one(src, dst, job)
                    exito(src, dst, tam_in, tam_out, aviso)
                except Exception as e:  # noqa: BLE001
                    fallo(src, dst, e)
                hechas += 1
                emit("progress", (hechas, total))

        # ---- fase 3: ZIP e informe ----
        if job.make_zip and producidos and not cancel_event.is_set():
            emit("status", "Creando archivo ZIP...")
            defecto = f"webp_{datetime.now():%Y%m%d_%H%M}.zip"
            nombre, cambiado = nombre_seguro(
                job.zip_name.strip() or defecto, defecto
            )
            if cambiado:
                emit("log", ("warn", f"Nombre de ZIP ajustado a: {nombre}"))
            if not nombre.lower().endswith(".zip"):
                nombre += ".zip"
            ruta_zip = job.output_dir / nombre
            with zipfile.ZipFile(
                ruta_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6
            ) as z:
                for p in sorted(producidos):
                    z.write(p, p.relative_to(job.output_dir).as_posix())
            res.zip_path = ruta_zip
            emit("log", ("info", f"ZIP creado: {ruta_zip}  ({len(producidos)} archivos)"))

        if job.csv_report and filas:
            job.output_dir.mkdir(parents=True, exist_ok=True)
            prefijo = "analisis" if job.dry_run else "informe"
            ruta_csv = job.output_dir / f"{prefijo}_{datetime.now():%Y%m%d_%H%M%S}.csv"
            with open(ruta_csv, "w", newline="", encoding="utf-8-sig") as fh:
                escritor = csv.writer(fh, delimiter=";")
                escritor.writerow(
                    ["Archivo", "Origen", "KB original", "KB resultado", "Ahorro %", "Estado", "Nota"]
                )
                for fila in filas:
                    escritor.writerow(fila)
            res.csv_path = ruta_csv
            emit("log", ("info", f"Informe CSV: {ruta_csv}"))

        emit("done", res)

    except Exception:  # noqa: BLE001
        emit("log", ("err", "Error inesperado:\n" + traceback.format_exc()))
        emit("done", res)
    finally:
        if temp_root is not None:
            shutil.rmtree(temp_root, ignore_errors=True)


# --------------------------------------------------------------------------- #
#  Zona de arrastrar y soltar
# --------------------------------------------------------------------------- #
class DropZone(tk.Canvas):
    def __init__(self, master, texto: str, on_click, on_drop=None, height: int = 62):
        super().__init__(
            master, height=height, bg=CARD, highlightthickness=0, bd=0, cursor="hand2"
        )
        self.texto = texto
        self._on_click = on_click
        self._on_drop = on_drop
        self._hover = False
        self.bind("<Configure>", lambda e: self._draw())
        self.bind("<Button-1>", lambda e: self._on_click())
        self.bind("<Enter>", self._enter)
        self.bind("<Leave>", self._leave)
        if DND_AVAILABLE and on_drop is not None:
            self.drop_target_register(DND_FILES)  # type: ignore[attr-defined]
            self.dnd_bind("<<Drop>>", self._drop)  # type: ignore[attr-defined]
            self.dnd_bind("<<DropEnter>>", lambda e: self._set_hover(True))  # type: ignore[attr-defined]
            self.dnd_bind("<<DropLeave>>", lambda e: self._set_hover(False))  # type: ignore[attr-defined]

    def _enter(self, _e=None):
        self._set_hover(True)

    def _leave(self, _e=None):
        self._set_hover(False)

    def _set_hover(self, value: bool):
        self._hover = value
        self._draw()
        return "copy" if value else None

    def _draw(self):
        self.delete("all")
        w = max(self.winfo_width(), 10)
        h = max(self.winfo_height(), 10)
        color = ACCENT if self._hover else "#b9c3d1"
        fondo = "#f2f7ff" if self._hover else "#fafbfd"
        self.create_rectangle(2, 2, w - 2, h - 2, outline=color, dash=(6, 4), width=2, fill=fondo)
        self.create_text(
            w / 2,
            h / 2 - 8,
            text=self.texto,
            fill=TEXT if self._hover else MUTED,
            font=("Segoe UI Semibold", 10 if self._hover else 9),
        )
        self.create_text(
            w / 2,
            h / 2 + 12,
            text="Archivos, carpetas, .zip, .rar o .7z  ·  tambien puedes hacer clic aqui",
            fill=MUTED,
            font=("Segoe UI", 8),
        )

    def _drop(self, event):
        if self._on_drop is not None:
            self._on_drop(event.data)
        self._set_hover(False)
        return "copy"


# --------------------------------------------------------------------------- #
#  Ventana "Acerca de"
# --------------------------------------------------------------------------- #
class VentanaAcerca(tk.Toplevel):
    def __init__(self, master: tk.Misc, on_manual=None):
        super().__init__(master)
        self.title(f"Acerca de {APP_NAME}")
        self.configure(bg=BG)
        self.geometry("780x680")
        self.minsize(660, 480)
        self.transient(master)
        self._on_manual = on_manual

        cab = ttk.Frame(self, padding=(18, 16, 18, 8))
        cab.pack(fill="x")
        ttk.Label(cab, text=APP_NAME, style="Title.TLabel").pack(anchor="w")
        ttk.Label(
            cab,
            text=f"Version {APP_VERSION}  ·  Conversion local de imagenes a WebP  ·  Windows",
            style="Sub.TLabel",
        ).pack(anchor="w", pady=(2, 0))

        caja = tk.Frame(self, bg=CARD, highlightthickness=1, highlightbackground="#dde3ea")
        caja.pack(fill="both", expand=True, padx=18, pady=(6, 0))

        self.txt = tk.Text(
            caja,
            bg=CARD,
            fg=TEXT,
            relief="flat",
            wrap="word",
            padx=16,
            pady=12,
            spacing1=0,
            spacing3=2,
            font=("Segoe UI", 9),
            cursor="arrow",
        )
        self.txt.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(caja, orient="vertical", command=self.txt.yview)
        sb.pack(side="right", fill="y")
        self.txt.configure(yscrollcommand=sb.set)

        self.txt.tag_configure("h1", font=("Segoe UI Semibold", 13), foreground=TEXT, spacing1=14, spacing3=4)
        self.txt.tag_configure("h2", font=("Segoe UI Semibold", 10), foreground=ACCENT_DARK, spacing1=14, spacing3=3)
        self.txt.tag_configure("p", font=("Segoe UI", 9), foreground=TEXT, spacing3=3)
        self.txt.tag_configure("li", font=("Segoe UI", 9), foreground=TEXT, lmargin1=16, lmargin2=30, spacing3=2)
        self.txt.tag_configure("code", font=("Consolas", 9), background="#eef1f6", foreground="#233044")
        self.txt.tag_configure("muted", font=("Segoe UI", 8), foreground=MUTED, spacing1=10)

        self._rellenar()

        pie = ttk.Frame(self, padding=(18, 10, 18, 16))
        pie.pack(fill="x")
        ttk.Button(pie, text="Cerrar", command=self.destroy).pack(side="right")
        if on_manual is not None:
            ttk.Button(pie, text="Abrir manual de uso", command=on_manual).pack(side="right", padx=(0, 8))

        self.bind("<Escape>", lambda e: self.destroy())

    def _rellenar(self) -> None:
        t = self.txt
        t.configure(state="normal")
        t.delete("1.0", "end")

        t.insert("end", "Que hace\n", "h1")
        for linea in (
            "Convierte JPG, PNG, BMP, TIFF, GIF, ICO, TGA, PCX, DDS y PPM/PGM/PBM a formato WebP.",
            "Trabaja con imagenes sueltas, carpetas completas y comprimidos .zip / .rar / .7z.",
            "Arrastra y suelta desde el Explorador de Windows en cualquier punto de la ventana.",
            "Guarda el resultado en la carpeta que elijas y, si quieres, lo empaqueta en un .zip.",
            "Todo el proceso es local: no se envia nada a internet. Los originales nunca se modifican.",
        ):
            t.insert("end", "•  " + linea + "\n", "li")

        t.insert("end", "Opciones principales\n", "h2")
        for linea in (
            "Presets: Web, Catalogo, Iconos sin perdida y Maxima calidad, o guarda los tuyos.",
            "Calidad 1-100 (85 por defecto): mas calidad, mas peso. 85-92 es lo ideal para web.",
            "Esfuerzo 0-6 (6 por defecto): cuanto trabaja el compresor; mas esfuerzo, archivos mas pequenos.",
            "Sin perdida (lossless): fidelidad exacta al original, archivos mas grandes. Para capturas y logos.",
            "Recodificar tambien los .webp / .avif: por defecto se saltan los que ya estan en el formato destino.",
            "Aplanar transparencia sobre un color: rellena el fondo alfa de los PNG con el color elegido.",
            "Mantener subcarpetas del origen: reproduce la estructura de carpetas original.",
            "Sobrescribir archivos existentes: si se desactiva, los ya convertidos se omiten.",
            "Empaquetar el resultado en .zip: crea un ZIP con todos los archivos en la carpeta de salida.",
            "Lado maximo: reduce a 2560, 1920, 1600, 1200, 800 o 512 px sin ampliar las pequenas.",
            "Peso maximo: ajusta la calidad automaticamente para no superar los KB indicados.",
            "Formato de salida: WebP o AVIF (AVIF suele pesar menos con la misma calidad).",
            "Informe CSV: deja en la carpeta de salida una tabla con peso original, final y ahorro.",
            "Aviso sonoro al terminar: tono agudo si todo fue bien, grave si hubo errores o se cancelo.",
        ):
            t.insert("end", "•  " + linea + "\n", "li")

        t.insert("end", "Historial y atajos\n", "h2")
        for linea in (
            "El menu Historial guarda los ultimos 10 trabajos; al elegir uno se recuperan origen, destino y ajustes.",
            "Ctrl+O anade archivos, Ctrl+Mayus+O anade una carpeta, Ctrl+Enter o F5 convierten.",
            "Supr quita las entradas seleccionadas de la lista de origen (seleccion multiple con Ctrl/Mayus).",
            "F1 abre este manual.",
        ):
            t.insert("end", "•  " + linea + "\n", "li")

        t.insert("end", "Comprimidos\n", "h2")
        t.insert("end", "Los .zip se abren con el lector interno. Los .rar y .7z necesitan 7-Zip o WinRAR: "
                       "el programa los detecta e indica cual usa. El contenido se extrae a una carpeta "
                       "temporal que se borra al terminar. Si anades varios comprimidos, cada uno se "
                       "guarda en una subcarpeta con su nombre.\n", "p")

        t.insert("end", "Que se conserva\n", "h2")
        for linea in (
            "La orientacion de las fotos (etiqueta EXIF), para que no salgan giradas.",
            "La transparencia de PNG, GIF y WebP, salvo que se active el aplanado.",
            "El perfil de color ICC, si la imagen lo lleva incrustado.",
            "Los GIF animados se convierten usando el primer fotograma (se avisa en el registro).",
            "Los metadatos EXIF y la ubicacion GPS no se copian, para que los archivos pesen menos.",
        ):
            t.insert("end", "•  " + linea + "\n", "li")

        t.insert("end", "Nombres y duplicados\n", "h2")
        t.insert("end", "Se conserva el nombre y solo cambia la extension: foto.jpg pasa a foto.webp. "
                       "Si dos originales darian el mismo nombre (foto.jpg y foto.png), el segundo recibe "
                       "un sufijo: foto_png.webp. Nunca se pisa un archivo distinto.\n", "p")

        t.insert("end", "Modo consola\n", "h2")
        t.insert("end", "ConversorWebP.exe <origen...> -o <carpeta> [-q 85] [-m 6] [--zip salida.zip]\n", "code")
        t.insert("end", "\n", "p")
        t.insert("end", "Opciones: --format avif, --max-dim 1920, --max-kb 150, --csv, --lossless, "
                       "--flat --bg \"#ffffff\", --no-recursive, --no-overwrite, --webp.\n", "p")
        t.insert("end", "Sin -o, el programa abre la ventana con los archivos ya cargados (util para 'Enviar a').\n", "p")

        t.insert("end", "Entorno de este equipo\n", "h2")
        tool = find_extractor()
        t.insert("end", "•  " + f"Python {sys.version.split()[0]}  ·  Pillow {getattr(Image, '__version__', '?')}\n", "li")
        t.insert("end", "•  " + ("Arrastrar y soltar: disponible (TkDND)\n" if DND_AVAILABLE
                                 else "Arrastrar y soltar: no disponible (instala tkinterdnd2)\n"), "li")
        t.insert("end", "•  " + (f"Extractor de comprimidos: {tool}\n" if tool
                                 else "Extractor de comprimidos: ninguno (solo .zip)\n"), "li")
        estado_gpu = detectar_hardware_gpu()
        gpu = estado_gpu.nvidia or (estado_gpu.hardware[0] if estado_gpu.hardware else "no detectada")
        t.insert("end", "•  " + f"GPU: {gpu}\n", "li")
        t.insert(
            "end",
            "•  "
            + (
                "Aceleracion CUDA: activa\n"
                if _GPU.activa
                else "Aceleracion CUDA: opcional (Herramientas -> Aceleracion GPU)\n"
            ),
            "li",
        )

        t.insert("end", "Aceleración GPU\n", "h2")
        t.insert(
            "end",
            "Se detecta la tarjeta gráfica y, si hay PyTorch con CUDA, se usa para decodificar y "
            "reducir JPEG grandes (8 MP o más). La codificación WebP/AVIF sigue siendo CPU, así que "
            "la ganancia aparece al redimensionar fotos grandes (~1,3-1,5x); en imágenes pequeñas "
            "no se usa.\n",
            "p",
        )

        t.insert("end", "Consejos\n", "h2")
        for linea in (
            "Para un lote grande, deja esfuerzo 6 y vete a hacer otra cosa: puedes cancelar cuando quieras.",
            "Si repites una conversion, desactiva 'Sobrescribir' para reanudar sin repetir trabajo.",
            "El ZIP del resultado no sustituye a los WebP: ambos quedan en la carpeta de salida.",
            "El manual completo, con todas las opciones explicadas, esta en Ayuda -> Manual de uso (F1).",
        ):
            t.insert("end", "•  " + linea + "\n", "li")

        t.insert("end", f"\n{APP_NAME} {APP_VERSION}  ·  Hecho para Windows  ·  Sin telemetria ni conexiones externas.",
                 "muted")
        t.configure(state="disabled")


# --------------------------------------------------------------------------- #
#  Carpeta vigilada
# --------------------------------------------------------------------------- #
class Vigilante(threading.Thread):
    """Convierte automaticamente lo que aparezca en una carpeta."""

    def __init__(self, entrada: Path, salida: Path, base: Job, emit, stop_event):
        super().__init__(daemon=True)
        self.entrada = Path(entrada)
        self.salida = Path(salida)
        self.base = base
        self.emitir = emit
        self.stop_event = stop_event
        self.procesados: set[str] = set()

    def _job_para(self, archivo: Path) -> Job:
        b = self.base
        return Job(
            sources=[archivo],
            output_dir=self.salida,
            quality=b.quality,
            method=b.method,
            keep_structure=True,
            overwrite=False,
            reencode_webp=b.reencode_webp,
            flatten=b.flatten,
            background=b.background,
            lossless=b.lossless,
            make_zip=False,
            zip_name="",
            max_dimension=b.max_dimension,
            target_kb=b.target_kb,
            formato=b.formato,
            csv_report=False,
            usar_gpu=b.usar_gpu,
            gpu_forzar=False,
            name_pattern=b.name_pattern,
            dry_run=False,
        )

    def run(self) -> None:
        self.emitir(f"Vigilando: {self.entrada}  ->  {self.salida}")
        while not self.stop_event.wait(3):
            try:
                archivos = [
                    p
                    for p in self.entrada.rglob("*")
                    if p.is_file()
                    and (is_image(p) or p.suffix.lower() in ARCHIVE_EXTS)
                ]
            except Exception:
                continue

            for archivo in archivos:
                if self.stop_event.is_set():
                    break
                clave = str(archivo.resolve())
                if clave in self.procesados:
                    continue
                try:
                    if time.time() - archivo.stat().st_mtime < 3:
                        continue  # puede estar copiandose todavia
                except OSError:
                    continue

                self.emitir(f"Vigilante: convirtiendo {archivo.name} ...")
                resultado = {}
                try:
                    run_job(
                        self._job_para(archivo),
                        lambda t, d: resultado.update({"r": d}) if t == "done" else None,
                        self.stop_event,
                    )
                    res = resultado.get("r")
                    if res is not None:
                        self.emitir(
                            f"Vigilante: {archivo.name} -> {res.converted} convertida(s), "
                            f"{len(res.failed)} fallo(s)"
                        )
                except Exception as e:  # noqa: BLE001
                    self.emitir(f"Vigilante: error con {archivo.name}: {e}")
                self.procesados.add(clave)

        self.emitir("Vigilante detenido.")


class ToolTip:
    """Globo de ayuda que aparece al pasar el ratón por un widget."""

    _activo: "ToolTip | None" = None

    def __init__(self, widget: tk.Misc, texto: str, retardo: int = 450):
        self.widget = widget
        self.texto = texto
        self.retardo = retardo
        self._id_after: str | None = None
        self._ventana: tk.Toplevel | None = None

        widget.bind("<Enter>", self._entrar, add="+")
        widget.bind("<Leave>", self._salir, add="+")
        widget.bind("<ButtonPress>", self._salir, add="+")
        widget.bind("<Destroy>", lambda _e: self._ocultar(), add="+")

    def _entrar(self, _evento=None) -> None:
        self._cancelar()
        self._id_after = self.widget.after(self.retardo, self._mostrar)

    def _salir(self, _evento=None) -> None:
        self._cancelar()
        self._ocultar()

    def _cancelar(self) -> None:
        if self._id_after is not None:
            try:
                self.widget.after_cancel(self._id_after)
            except Exception:
                pass
            self._id_after = None

    def _mostrar(self) -> None:
        if self._ventana is not None or not self.texto:
            return
        if ToolTip._activo is not None and ToolTip._activo is not self:
            ToolTip._activo._ocultar()
        ToolTip._activo = self

        x = self.widget.winfo_rootx() + 12
        y = self.widget.winfo_rooty() + self.widget.winfo_height() + 8

        ventana = tk.Toplevel(self.widget)
        ventana.wm_overrideredirect(True)
        try:
            ventana.attributes("-topmost", True)
        except Exception:
            pass
        marco = tk.Frame(ventana, bg="#1f2933", bd=1, relief="solid")
        marco.pack()
        tk.Label(
            marco,
            text=self.texto,
            bg="#1f2933",
            fg="#f8fafc",
            font=("Segoe UI", 8),
            justify="left",
            wraplength=360,
            padx=9,
            pady=6,
        ).pack()
        ventana.update_idletasks()

        ancho = ventana.winfo_width()
        alto = ventana.winfo_height()
        pantalla_w = self.widget.winfo_screenwidth()
        pantalla_h = self.widget.winfo_screenheight()
        if x + ancho > pantalla_w - 12:
            x = max(6, pantalla_w - ancho - 12)
        if y + alto > pantalla_h - 12:
            y = max(6, self.widget.winfo_rooty() - alto - 8)
        ventana.wm_geometry(f"+{x}+{y}")
        self._ventana = ventana

    def _ocultar(self) -> None:
        if self._ventana is not None:
            try:
                self._ventana.destroy()
            except Exception:
                pass
            self._ventana = None
        if ToolTip._activo is self:
            ToolTip._activo = None


# --------------------------------------------------------------------------- #
#  Interfaz grafica
# --------------------------------------------------------------------------- #
class App(BaseTk):  # type: ignore[misc,valid-type]
    def __init__(self, sources_iniciales: list[Path] | None = None) -> None:
        super().__init__()
        self.title(f"{APP_NAME} {APP_VERSION}")
        ancho = max(860, min(1000, self.winfo_screenwidth() - 80))
        alto = max(640, min(840, self.winfo_screenheight() - 120))
        self.geometry(f"{ancho}x{alto}")
        self.minsize(860, 620)
        self.configure(bg=BG)

        self.sources: list[Path] = []
        self.output_dir = tk.StringVar()
        self.quality = tk.IntVar(value=85)
        self.method = tk.IntVar(value=6)
        self.lossless = tk.BooleanVar(value=False)
        self.keep_structure = tk.BooleanVar(value=True)
        self.overwrite = tk.BooleanVar(value=True)
        self.reencode_webp = tk.BooleanVar(value=False)
        self.flatten = tk.BooleanVar(value=False)
        self.background = tk.StringVar(value="#ffffff")
        self.make_zip = tk.BooleanVar(value=True)
        self.zip_name = tk.StringVar(value=f"webp_{datetime.now():%Y%m%d_%H%M}.zip")
        self.beep = tk.BooleanVar(value=True)
        self.max_dim = tk.StringVar(value="Sin cambio")
        self.peso_activo = tk.BooleanVar(value=False)
        self.peso_kb = tk.StringVar(value="150")
        self.preset_sel = tk.StringVar(value="Personalizado")
        self.presets_usuario: dict[str, dict] = {}
        self.historial: list[dict] = []
        self._aplicando_preset = False
        self.formato = tk.StringVar(value="WebP")
        self.csv_report = tk.BooleanVar(value=False)
        self.usar_gpu = tk.BooleanVar(value=False)
        self.gpu_path = ""
        self.gpu_forzar_var = tk.BooleanVar(value=False)
        self.name_pattern = tk.StringVar(value="")
        self.dry_run = tk.BooleanVar(value=False)

        self.queue: queue.Queue = queue.Queue()
        self.cancel_event = threading.Event()
        self.worker: threading.Thread | None = None
        self.running = False
        self.total = 0
        self._acerca_ventana: VentanaAcerca | None = None
        self._vigilante: Vigilante | None = None
        self._watch_stop = threading.Event()
        self._closing = False
        self._after_id: str | None = None
        self._actualizacion: tuple[str, str] | None = None
        self._comprobando_update = False
        self.buscar_actualizaciones = True
        self.ultima_comprobacion = ""
        self._tooltips: list[ToolTip] = []
        self._menu_descripciones: dict[str, str] = {}
        self._after_menu: str | None = None

        self._dpi_setup()
        self._build_style()
        self._build_ui()
        self._bind_shortcuts()
        for var in (
            self.quality, self.method, self.lossless, self.flatten,
            self.reencode_webp, self.background, self.max_dim,
            self.peso_activo, self.peso_kb,
        ):
            var.trace_add("write", self._marcar_personalizado)
        self._build_menu()
        self._load_config()
        self._center()

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.bind("<Destroy>", self._al_destruir, add="+")
        self._after_id = self.after(100, self._drain_queue)
        self.after(3000, self._auto_comprobar_update)

        if DND_AVAILABLE:
            self._registrar_dnd_global()
        else:
            self.after(400, self._aviso_sin_dnd)

        if sources_iniciales:
            self.after(150, lambda: self._anadir(list(sources_iniciales)))

    # ---------------- infraestructura ---------------- #
    def _dpi_setup(self) -> None:
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)  # type: ignore[attr-defined]
            dpi = ctypes.windll.user32.GetDpiForSystem()  # type: ignore[attr-defined]
            self.tk.call("tk", "scaling", dpi / 72.0)
        except Exception:
            pass

    def _build_style(self) -> None:
        style = ttk.Style(self)
        for tema in ("vista", "winnative", "clam"):
            if tema in style.theme_names():
                try:
                    style.theme_use(tema)
                    break
                except tk.TclError:
                    continue
        self._tema = style.theme_use()
        nativo = self._tema in ("vista", "winnative")

        style.configure("TFrame", background=BG)
        style.configure("Card.TFrame", background=CARD)
        style.configure("TLabel", background=BG, foreground=TEXT, font=("Segoe UI", 9))
        style.configure("Muted.TLabel", background=BG, foreground=MUTED, font=("Segoe UI", 8))
        style.configure("Title.TLabel", background=BG, foreground=TEXT, font=("Segoe UI Semibold", 16))
        style.configure("Sub.TLabel", background=BG, foreground=MUTED, font=("Segoe UI", 9))
        style.configure("Card.TLabel", background=CARD, foreground=TEXT, font=("Segoe UI", 9))
        style.configure("MutedCard.TLabel", background=CARD, foreground=MUTED, font=("Segoe UI", 8))
        style.configure(
            "Section.TLabel", background=CARD, foreground=ACCENT_DARK, font=("Segoe UI Semibold", 10)
        )
        style.configure("TCheckbutton", background=CARD, foreground=TEXT, font=("Segoe UI", 9))
        style.map("TCheckbutton", background=[("active", CARD)], foreground=[("disabled", "#9aa5b1")])
        style.configure("TButton", font=("Segoe UI", 9), padding=5)
        style.configure("Bar.TCheckbutton", background=BG, foreground=TEXT, font=("Segoe UI", 9))
        style.map("Bar.TCheckbutton", background=[("active", BG)], foreground=[("disabled", "#9aa5b1")])
        style.configure("TEntry", padding=5)
        style.configure("Horizontal.TProgressbar", background=ACCENT, troughcolor="#dde3ea", thickness=14)
        if not nativo:
            # El tema "clam" necesita un color de acento explicito
            style.configure(
                "Accent.TButton",
                font=("Segoe UI Semibold", 10),
                padding=(16, 9),
                background=ACCENT,
                foreground="white",
                borderwidth=0,
            )
            style.map("Accent.TButton", background=[("disabled", "#9db8dd"), ("active", ACCENT_DARK)])

    def _aviso_sin_dnd(self) -> None:
        messagebox.showinfo(
            APP_NAME,
            "El modulo de arrastrar y soltar (tkinterdnd2) no esta disponible.\n"
            "Puedes seguir usando los botones para seleccionar archivos y carpetas.\n\n"
            "Para activarlo:  pip install tkinterdnd2",
        )

    def _build_ui(self) -> None:
        outer = ttk.Frame(self, padding=16)
        outer.pack(fill="both", expand=True)

        cab = ttk.Frame(outer)
        cab.pack(fill="x")
        ttk.Label(cab, text=APP_NAME, style="Title.TLabel").pack(side="left")
        ttk.Label(cab, text=f"v{APP_VERSION}", style="Muted.TLabel").pack(side="left", padx=(8, 0), pady=(6, 0))
        ttk.Label(
            cab,
            text="JPG · PNG · BMP · TIFF · GIF · ICO  →  WebP / AVIF",
            style="Sub.TLabel",
        ).pack(side="right", pady=(6, 0))

        # ---- 1. Origen -------------------------------------------------- #
        card_src = ttk.Frame(outer, style="Card.TFrame", padding=12)
        card_src.pack(fill="x", pady=(10, 0))

        head_src = ttk.Frame(card_src, style="Card.TFrame")
        head_src.pack(fill="x")
        ttk.Label(head_src, text="1. Origen", style="Section.TLabel").pack(side="left")
        btns = ttk.Frame(head_src, style="Card.TFrame")
        btns.pack(side="right")
        self.btn_add_files = ttk.Button(btns, text="Archivos...", command=self._add_files)
        self.btn_add_files.pack(side="left", padx=(0, 5))
        self.btn_add_folder = ttk.Button(btns, text="Carpeta...", command=self._add_folder)
        self.btn_add_folder.pack(side="left", padx=(0, 5))
        self.btn_add_zip = ttk.Button(btns, text=".zip / .rar...", command=self._add_archive)
        self.btn_add_zip.pack(side="left", padx=(0, 5))
        self.btn_remove = ttk.Button(btns, text="Quitar", command=self._quitar_seleccionados)
        self.btn_remove.pack(side="left", padx=(0, 5))
        self.btn_clear = ttk.Button(btns, text="Quitar todo", command=self._clear_sources)
        self.btn_clear.pack(side="left")

        self.drop_zone = DropZone(
            card_src,
            "Arrastra aqui tus imagenes, carpetas o comprimidos .zip / .rar",
            on_click=self._add_files,
            on_drop=self._drop_paths,
        )
        self.drop_zone.pack(fill="x", pady=(8, 8))

        lista_wrap = ttk.Frame(card_src, style="Card.TFrame")
        lista_wrap.pack(fill="both", expand=True)
        self.source_list = tk.Listbox(
            lista_wrap,
            height=4,
            activestyle="none",
            selectmode="extended",
            bg="#fbfcfe",
            fg=TEXT,
            selectbackground="#d7e3fb",
            selectforeground=TEXT,
            highlightthickness=1,
            highlightbackground="#dde3ea",
            relief="flat",
            font=("Consolas", 9),
        )
        self.source_list.pack(side="left", fill="both", expand=True)
        self.source_list.bind("<Delete>", lambda _e: self._quitar_seleccionados())
        sb = ttk.Scrollbar(lista_wrap, orient="vertical", command=self.source_list.yview)
        sb.pack(side="right", fill="y")
        self.source_list.configure(yscrollcommand=sb.set)

        pie_src = ttk.Frame(card_src, style="Card.TFrame")
        pie_src.pack(fill="x", pady=(6, 0))
        self.lbl_count = ttk.Label(pie_src, text="Sin origen seleccionado", style="Card.TLabel")
        self.lbl_count.pack(side="left")
        self.lbl_extractor = ttk.Label(pie_src, text="", style="MutedCard.TLabel")
        self.lbl_extractor.pack(side="right")

        # ---- 2. Destino / 3. Calidad ------------------------------------ #
        columnas = ttk.Frame(outer)
        columnas.pack(fill="x", pady=(10, 0))

        card_out = ttk.Frame(columnas, style="Card.TFrame", padding=12)
        card_out.pack(side="left", fill="both", expand=True)
        card_cal = ttk.Frame(columnas, style="Card.TFrame", padding=12)
        card_cal.pack(side="left", fill="both", expand=True, padx=(10, 0))

        ttk.Label(card_out, text="2. Carpeta de salida", style="Section.TLabel").pack(anchor="w")
        fila_out = ttk.Frame(card_out, style="Card.TFrame")
        fila_out.pack(fill="x", pady=(8, 0))
        self.entry_out = ttk.Entry(fila_out, textvariable=self.output_dir)
        self.entry_out.pack(side="left", fill="x", expand=True)
        self.btn_browse = ttk.Button(fila_out, text="Examinar...", command=self._pick_output)
        self.btn_browse.pack(side="left", padx=(5, 0))

        self.chk_structure = ttk.Checkbutton(
            card_out, text="Mantener subcarpetas del origen", variable=self.keep_structure
        )
        self.chk_structure.pack(anchor="w", pady=(8, 0))
        self.chk_overwrite = ttk.Checkbutton(
            card_out, text="Sobrescribir archivos existentes", variable=self.overwrite
        )
        self.chk_overwrite.pack(anchor="w", pady=(3, 0))

        fila_zip = ttk.Frame(card_out, style="Card.TFrame")
        fila_zip.pack(fill="x", pady=(8, 0))
        self.chk_zip = ttk.Checkbutton(
            fila_zip,
            text="Empaquetar el resultado en .zip",
            variable=self.make_zip,
            command=self._on_zip_toggle,
        )
        self.chk_zip.pack(side="left")
        self.entry_zip = ttk.Entry(fila_zip, textvariable=self.zip_name, width=22)
        self.entry_zip.pack(side="left", fill="x", expand=True, padx=(8, 0))

        fila_fmt = ttk.Frame(card_out, style="Card.TFrame")
        fila_fmt.pack(fill="x", pady=(8, 0))
        ttk.Label(fila_fmt, text="Formato de salida", style="Card.TLabel").pack(side="left")
        self.cmb_formato = ttk.Combobox(
            fila_fmt,
            width=7,
            state="readonly",
            values=["WebP", "AVIF"],
            textvariable=self.formato,
        )
        self.cmb_formato.pack(side="left", padx=(6, 0))
        self.cmb_formato.bind("<<ComboboxSelected>>", self._on_formato)
        ttk.Label(fila_fmt, text="AVIF pesa menos", style="MutedCard.TLabel").pack(side="left", padx=(8, 0))

        self.chk_csv = ttk.Checkbutton(
            card_out,
            text="Guardar informe CSV en la carpeta de salida",
            variable=self.csv_report,
        )
        self.chk_csv.pack(anchor="w", pady=(6, 0))

        fila_nombre = ttk.Frame(card_out, style="Card.TFrame")
        fila_nombre.pack(fill="x", pady=(6, 0))
        ttk.Label(fila_nombre, text="Nombre de salida", style="Card.TLabel").pack(side="left")
        self.entry_patron = ttk.Entry(fila_nombre, textvariable=self.name_pattern)
        self.entry_patron.pack(side="left", fill="x", expand=True, padx=(6, 4))
        self.btn_patron_ayuda = ttk.Button(
            fila_nombre, text="?", width=3, command=self._ayuda_patron
        )
        self.btn_patron_ayuda.pack(side="left")

        self.chk_dry = ttk.Checkbutton(
            card_out,
            text="Solo analizar (no escribir archivos)",
            variable=self.dry_run,
            command=self._on_dry_run,
        )
        self.chk_dry.pack(anchor="w", pady=(6, 0))

        fila_gpu = ttk.Frame(card_out, style="Card.TFrame")
        fila_gpu.pack(fill="x", pady=(6, 0))
        self.chk_gpu = ttk.Checkbutton(
            fila_gpu, text="Acelerar con GPU (CUDA)", variable=self.usar_gpu
        )
        self.chk_gpu.pack(side="left")
        self.lbl_gpu = ttk.Label(fila_gpu, text="", style="MutedCard.TLabel")
        self.lbl_gpu.pack(side="right")

        ttk.Label(card_cal, text="3. Calidad y opciones", style="Section.TLabel").pack(anchor="w")

        fila_preset = ttk.Frame(card_cal, style="Card.TFrame")
        fila_preset.pack(fill="x", pady=(8, 0))
        ttk.Label(fila_preset, text="Preset", style="Card.TLabel").pack(side="left")
        self.cmb_preset = ttk.Combobox(
            fila_preset,
            state="readonly",
            width=24,
            textvariable=self.preset_sel,
            values=self._valores_preset(),
        )
        self.cmb_preset.pack(side="left", fill="x", expand=True, padx=(6, 6))
        self.cmb_preset.bind("<<ComboboxSelected>>", self._on_preset)
        self.btn_save_preset = ttk.Button(
            fila_preset, text="Guardar", width=8, command=self._guardar_preset
        )
        self.btn_save_preset.pack(side="left")
        self.btn_del_preset = ttk.Button(
            fila_preset, text="Borrar", width=7, command=self._borrar_preset
        )
        self.btn_del_preset.pack(side="left", padx=(4, 0))

        fila_q = ttk.Frame(card_cal, style="Card.TFrame")
        fila_q.pack(fill="x", pady=(8, 0))
        self.scale_q = tk.Scale(
            fila_q,
            from_=1,
            to=100,
            orient="horizontal",
            variable=self.quality,
            command=self._on_quality,
            showvalue=False,
            bg="#ffffff",
            troughcolor="#c3cddb",
            activebackground=ACCENT_DARK,
            highlightthickness=0,
            bd=1,
            sliderrelief="raised",
            sliderlength=16,
            width=10,
            resolution=1,
        )
        self.scale_q.pack(side="left", fill="x", expand=True, padx=(0, 8))
        self.lbl_q = ttk.Label(fila_q, text="85", style="Card.TLabel", width=4)
        self.lbl_q.pack(side="left")
        ttk.Label(fila_q, text="Esfuerzo", style="Card.TLabel").pack(side="left", padx=(12, 4))
        self.cmb_method = ttk.Combobox(
            fila_q, width=3, state="readonly", values=[str(i) for i in range(7)], textvariable=self.method
        )
        self.cmb_method.pack(side="left")

        self.chk_lossless = ttk.Checkbutton(
            card_cal, text="Sin perdida (lossless)", variable=self.lossless,
            command=self._on_lossless,
        )
        self.chk_lossless.pack(anchor="w", pady=(8, 0))
        self.chk_webp = ttk.Checkbutton(
            card_cal, text="Recodificar tambien los .webp", variable=self.reencode_webp
        )
        self.chk_webp.pack(anchor="w", pady=(3, 0))

        fila_alfa = ttk.Frame(card_cal, style="Card.TFrame")
        fila_alfa.pack(fill="x", pady=(3, 0))
        self.chk_flatten = ttk.Checkbutton(
            fila_alfa, text="Aplanar transparencia sobre", variable=self.flatten,
            command=self._on_flatten,
        )
        self.chk_flatten.pack(side="left")
        self.btn_color = tk.Button(
            fila_alfa, width=3, relief="flat", bg=self.background.get(), bd=1,
            activebackground=self.background.get(), command=self._pick_background, cursor="hand2",
        )
        self.btn_color.pack(side="left", padx=(6, 0))
        self.lbl_color = ttk.Label(fila_alfa, textvariable=self.background, style="MutedCard.TLabel")
        self.lbl_color.pack(side="left", padx=(6, 0))

        fila_dim = ttk.Frame(card_cal, style="Card.TFrame")
        fila_dim.pack(fill="x", pady=(6, 0))
        ttk.Label(fila_dim, text="Lado maximo", style="Card.TLabel").pack(side="left")
        self.cmb_dim = ttk.Combobox(
            fila_dim,
            width=10,
            state="readonly",
            values=DIMENSIONES,
            textvariable=self.max_dim,
        )
        self.cmb_dim.pack(side="left", padx=(6, 0))
        ttk.Label(fila_dim, text="px  ·  nunca amplia", style="MutedCard.TLabel").pack(side="left", padx=(6, 0))

        fila_peso = ttk.Frame(card_cal, style="Card.TFrame")
        fila_peso.pack(fill="x", pady=(4, 0))
        self.chk_peso = ttk.Checkbutton(
            fila_peso, text="Peso maximo", variable=self.peso_activo, command=self._on_peso
        )
        self.chk_peso.pack(side="left")
        self.entry_peso = ttk.Entry(fila_peso, textvariable=self.peso_kb, width=7, justify="right")
        self.entry_peso.pack(side="left", padx=(6, 0))
        ttk.Label(fila_peso, text="KB por imagen", style="MutedCard.TLabel").pack(side="left", padx=(4, 0))

        # ---- Acciones --------------------------------------------------- #
        accion = ttk.Frame(outer)
        accion.pack(fill="x", pady=(12, 0))
        self.btn_start = tk.Button(
            accion,
            text="Convertir a WebP",
            command=self._start,
            bg=ACCENT,
            fg="white",
            activebackground=ACCENT_DARK,
            activeforeground="white",
            disabledforeground="#e6eefc",
            relief="flat",
            bd=0,
            padx=18,
            pady=8,
            cursor="hand2",
            font=("Segoe UI Semibold", 10),
        )
        self.btn_start.pack(side="left")
        self.btn_cancel = ttk.Button(accion, text="Cancelar", command=self._cancel, state="disabled")
        self.btn_cancel.pack(side="left", padx=(8, 0))
        self.btn_open = ttk.Button(accion, text="Abrir carpeta de salida", command=self._open_output)
        self.btn_open.pack(side="right")
        self.chk_beep = ttk.Checkbutton(
            accion,
            text="Aviso sonoro al terminar",
            variable=self.beep,
            style="Bar.TCheckbutton",
        )
        self.chk_beep.pack(side="right", padx=(0, 18))

        self.progress = ttk.Progressbar(outer, mode="determinate", maximum=100)
        self.progress.pack(fill="x", pady=(10, 3))
        self.lbl_status = ttk.Label(outer, text="Listo.", style="Muted.TLabel")
        self.lbl_status.pack(anchor="w")

        # ---- Resultados / Registro --------------------------------------- #
        self.notas = ttk.Notebook(outer)
        self.notas.pack(fill="both", expand=True, pady=(6, 0))

        tab_res = ttk.Frame(self.notas)
        self.notas.add(tab_res, text="Resultados")

        columnas = ("archivo", "entrada", "salida", "ahorro", "estado")
        self.tabla = ttk.Treeview(
            tab_res, columns=columnas, show="headings", selectmode="browse"
        )
        cabeceras = {
            "archivo": ("Archivo", 460, "w"),
            "entrada": ("Original (KB)", 100, "e"),
            "salida": ("Final (KB)", 100, "e"),
            "ahorro": ("Ahorro %", 80, "e"),
            "estado": ("Estado", 95, "center"),
        }
        for col in columnas:
            texto, ancho, alineacion = cabeceras[col]
            self.tabla.heading(col, text=texto)
            self.tabla.column(col, width=ancho, anchor=alineacion, stretch=(col == "archivo"))
        self.tabla.pack(side="left", fill="both", expand=True)
        tsb = ttk.Scrollbar(tab_res, orient="vertical", command=self.tabla.yview)
        tsb.pack(side="right", fill="y")
        self.tabla.configure(yscrollcommand=tsb.set)
        self.tabla.tag_configure("convertida", foreground=OK)
        self.tabla.tag_configure("analizada", foreground=ACCENT_DARK)
        self.tabla.tag_configure("omitida", foreground=MUTED)
        self.tabla.tag_configure("fallo", foreground=ERR)
        self.tabla.bind("<Double-1>", self._abrir_fila)
        self.tabla.bind("<Return>", self._abrir_fila)
        self._rutas_filas: dict[str, str] = {}
        self._contador_filas = 0

        tab_log = ttk.Frame(self.notas)
        self.notas.add(tab_log, text="Registro")
        log_wrap = ttk.Frame(tab_log)
        log_wrap.pack(fill="both", expand=True)
        self.log = tk.Text(
            log_wrap, height=6, bg="#0f172a", fg="#cbd5e1", insertbackground="#cbd5e1",
            relief="flat", font=("Consolas", 9), padx=10, pady=6, wrap="none",
        )
        self.log.pack(side="left", fill="both", expand=True)
        lsb = ttk.Scrollbar(log_wrap, orient="vertical", command=self.log.yview)
        lsb.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=lsb.set, state="disabled")
        self.log.tag_configure("ok", foreground="#4ade80")
        self.log.tag_configure("err", foreground="#f87171")
        self.log.tag_configure("warn", foreground="#fbbf24")
        self.log.tag_configure("muted", foreground="#94a3b8")
        self.log.tag_configure("info", foreground="#93c5fd")

        self._on_zip_toggle()
        self._on_flatten()
        self._on_formato()
        self._refrescar_extractor()
        self._refrescar_gpu_ui()
        self._instalar_tooltips()

    def _tip(self, widget, texto: str) -> None:
        if widget is not None:
            self._tooltips.append(ToolTip(widget, texto))

    def _instalar_tooltips(self) -> None:
        tips = {
            "btn_add_files": "Añade una o varias imágenes al lote.",
            "btn_add_folder": "Añade una carpeta completa, incluidas sus subcarpetas.",
            "btn_add_zip": "Añade archivos .zip, .rar o .7z y convierte su contenido.",
            "btn_remove": "Quita de la lista las entradas seleccionadas (tecla Supr).",
            "btn_clear": "Vacía la lista de origen sin tocar tus archivos.",
            "drop_zone": "Arrastra aquí imágenes, carpetas o comprimidos desde el Explorador.",
            "source_list": "Orígenes añadidos. Selecciona varios con Ctrl o Mayús y pulsa Supr para quitarlos.",
            "lbl_count": "Resumen de carpetas, comprimidos e imágenes añadidos.",
            "lbl_extractor": "Herramienta usada para abrir .rar y .7z (7-Zip o WinRAR).",
            "entry_out": "Carpeta donde se guardarán los archivos convertidos.",
            "btn_browse": "Elige la carpeta de salida.",
            "chk_structure": "Reproduce las subcarpetas del origen en la carpeta de salida.",
            "chk_overwrite": "Activado reemplaza archivos existentes; desactivado los omite.",
            "chk_zip": "Crea además un ZIP con todos los archivos generados.",
            "entry_zip": "Nombre del ZIP que se creará en la carpeta de salida.",
            "cmb_formato": "Formato de salida: WebP (compatible) o AVIF (pesa menos).",
            "chk_csv": "Guarda un CSV con el peso original, final y el ahorro de cada archivo.",
            "entry_patron": "Patrón de nombre. Vacío conserva el original. Ej.: {nombre}_{ancho}x{alto}.",
            "btn_patron_ayuda": "Ver los marcadores disponibles para el nombre de salida.",
            "chk_dry": "Calcula el peso estimado sin escribir ninguna imagen.",
            "chk_gpu": "Modo experimental: usa CUDA en JPEG grandes. Desactivado por defecto.",
            "lbl_gpu": "Tarjeta gráfica detectada en este equipo.",
            "cmb_preset": "Aplica una combinación guardada de calidad y tamaño.",
            "btn_save_preset": "Guarda los ajustes actuales como preset propio.",
            "btn_del_preset": "Borra el preset propio seleccionado.",
            "scale_q": "Calidad de 1 a 100. Más calidad = mejor aspecto y más peso.",
            "lbl_q": "Valor de calidad actual.",
            "cmb_method": "Esfuerzo 0-6: más esfuerzo comprime mejor pero tarda más.",
            "chk_lossless": "Sin pérdida: fidelidad exacta, archivos más grandes.",
            "chk_webp": "Vuelve a convertir archivos que ya están en el formato de destino.",
            "chk_flatten": "Rellena la transparencia con un color sólido.",
            "btn_color": "Elige el color de fondo al aplanar la transparencia.",
            "cmb_dim": "Reduce el lado mayor a este tamaño; nunca amplía imágenes pequeñas.",
            "chk_peso": "Ajusta la calidad automáticamente para no superar el peso indicado.",
            "entry_peso": "Peso máximo por imagen, en KB (mínimo 5).",
            "btn_start": "Inicia la conversión con los ajustes actuales (Ctrl+Enter o F5).",
            "btn_cancel": "Detiene el proceso después del archivo en curso.",
            "chk_beep": "Pitido al terminar: agudo si todo va bien, grave si hay errores.",
            "btn_open": "Abre el Explorador en la carpeta de salida.",
            "tabla": "Resultados del último trabajo. Doble clic para abrir un archivo.",
            "notas": "Resultados en tabla y registro detallado del proceso.",
        }
        for atributo, texto in tips.items():
            self._tip(getattr(self, atributo, None), texto)

    def _al_recorrer_menu(self, event=None) -> None:
        if event is None or self.running:
            return
        menu = event.widget
        try:
            indice = menu.index("active")
        except Exception:
            return
        if indice in (None, "none", -1):
            return
        try:
            etiqueta = str(menu.entrycget(indice, "label"))
        except Exception:
            return
        texto = self._menu_descripciones.get(etiqueta)
        if not texto:
            return
        self.lbl_status.configure(text=texto)
        if self._after_menu is not None:
            try:
                self.after_cancel(self._after_menu)
            except Exception:
                pass
        self._after_menu = self.after(
            6000,
            lambda: None if self.running else self.lbl_status.configure(text="Listo."),
        )

    def _build_menu(self) -> None:
        barra = tk.Menu(self)
        ayuda = tk.Menu(barra, tearoff=0)
        ayuda.add_command(label="Manual de uso", accelerator="F1", command=self._abrir_manual)
        ayuda.add_separator()
        ayuda.add_command(
            label="Buscar actualizaciones",
            command=lambda: self._comprobar_actualizaciones(manual=True),
        )
        ayuda.add_command(label="Acerca de Conversor WebP", command=self._acerca_de)
        barra.add_cascade(label="Ayuda", menu=ayuda)
        self.menu_hist = tk.Menu(barra, tearoff=0)
        barra.add_cascade(label="Historial", menu=self.menu_hist)
        herr = tk.Menu(barra, tearoff=0)
        herr.add_command(
            label="Anadir a 'Enviar a' del Explorador", command=self._instalar_sendto
        )
        herr.add_command(label="Quitar de 'Enviar a'", command=self._quitar_sendto)
        herr.add_separator()
        herr.add_command(
            label="Anadir al menu contextual de Windows", command=self._instalar_menu_contextual
        )
        herr.add_command(
            label="Quitar del menu contextual", command=self._quitar_menu_contextual
        )
        herr.add_separator()
        herr.add_command(label="Aceleracion GPU (CUDA)...", command=self._dialogo_gpu)
        herr.add_checkbutton(
            label="Forzar GPU en lotes pequenos",
            variable=self.gpu_forzar_var,
            command=self._toggle_forzar_gpu,
        )
        herr.add_separator()
        herr.add_command(label="Carpeta vigilada...", command=self._dialogo_vigilante)
        herr.add_command(label="Vista previa de la primera imagen", command=self._vista_previa)
        barra.add_cascade(label="Herramientas", menu=herr)
        self._menu_descripciones = {
            "Manual de uso": "Abre el manual completo en el navegador (F1).",
            "Buscar actualizaciones": "Comprueba si hay una versión nueva publicada en GitHub.",
            "Acerca de Conversor WebP": "Versión, características y entorno del equipo.",
            "Anadir a 'Enviar a' del Explorador": "Crea un acceso en 'Enviar a' para abrir la app con los archivos seleccionados.",
            "Quitar de 'Enviar a'": "Elimina el acceso creado en 'Enviar a'.",
            "Anadir al menu contextual de Windows": "Añade 'Convertir a WebP' al clic derecho de imágenes, carpetas y comprimidos.",
            "Quitar del menu contextual": "Elimina la entrada del menú contextual de Windows.",
            "Aceleracion GPU (CUDA)...": "Muestra el estado de CUDA y permite indicar la carpeta de PyTorch.",
            "Forzar GPU en lotes pequenos": "Usa la GPU aunque el lote sea pequeño (puede tardar más al inicio).",
            "Carpeta vigilada...": "Convierte automáticamente lo que aparezca en una carpeta.",
            "Vista previa de la primera imagen": "Compara el original y el resultado con su peso y ahorro.",
            "Vaciar historial": "Borra el historial de trabajos.",
        }
        for menu in (ayuda, self.menu_hist, herr):
            menu.bind("<<MenuSelect>>", self._al_recorrer_menu)
        self.configure(menu=barra)
        self._refrescar_historial()
        self.bind_all("<F1>", lambda _e: self._abrir_manual())

    def _acerca_de(self) -> None:
        if self._acerca_ventana is not None and self._acerca_ventana.winfo_exists():
            self._acerca_ventana.deiconify()
            self._acerca_ventana.lift()
            self._acerca_ventana.focus_force()
            return
        self._acerca_ventana = VentanaAcerca(self, on_manual=self._abrir_manual)

    def _abrir_manual(self) -> None:
        ruta = ruta_manual()
        if ruta is not None:
            try:
                os.startfile(str(ruta))  # type: ignore[attr-defined]
                return
            except Exception:
                pass
        self._acerca_de()

    def _ruta_sendto(self) -> Path:
        return (
            Path(os.getenv("APPDATA", ""))
            / "Microsoft"
            / "Windows"
            / "SendTo"
            / "Conversor WebP.lnk"
        )

    def _instalar_sendto(self) -> None:
        try:
            lnk = self._ruta_sendto()
            lnk.parent.mkdir(parents=True, exist_ok=True)
            if getattr(sys, "frozen", False):
                objetivo = Path(sys.executable)
                argumentos = ""
                icono = str(objetivo)
            else:
                ejecutable = Path(sys.executable)
                pythonw = ejecutable.with_name("pythonw.exe")
                objetivo = pythonw if pythonw.exists() else ejecutable
                argumentos = f'"{Path(__file__).resolve()}"'
                icono = str(Path(__file__).resolve().parent / "icon.ico")
            script = (
                "$s=(New-Object -ComObject WScript.Shell).CreateShortcut('"
                + _ps_quote(lnk)
                + "');"
                f"$s.TargetPath='{_ps_quote(objetivo)}';"
                f"$s.Arguments='{_ps_quote(argumentos)}';"
                f"$s.WorkingDirectory='{_ps_quote(objetivo.parent)}';"
                f"$s.IconLocation='{_ps_quote(icono)}';"
                "$s.Save()"
            )
            proc = subprocess.run(
                ["powershell", "-NoProfile", "-Command", script],
                capture_output=True,
                text=True,
                creationflags=_no_window_flags(),
            )
            if proc.returncode != 0 or not lnk.exists():
                raise RuntimeError((proc.stderr or proc.stdout or "sin detalle").strip())
            messagebox.showinfo(
                APP_NAME,
                "Acceso creado en 'Enviar a'.\n\n"
                "Selecciona imagenes o un .zip/.rar en el Explorador, boton derecho -> "
                "Enviar a -> Conversor WebP, y el programa se abrira con esos archivos "
                "ya cargados.",
            )
        except Exception as e:  # noqa: BLE001
            messagebox.showerror(APP_NAME, f"No se pudo crear el acceso:\n{e}")

    def _quitar_sendto(self) -> None:
        lnk = self._ruta_sendto()
        if not lnk.exists():
            messagebox.showinfo(APP_NAME, "No habia ningun acceso creado en 'Enviar a'.")
            return
        try:
            lnk.unlink()
            messagebox.showinfo(APP_NAME, "Acceso eliminado de 'Enviar a'.")
        except Exception as e:  # noqa: BLE001
            messagebox.showerror(APP_NAME, f"No se pudo eliminar el acceso:\n{e}")

    # ---------------- menu contextual y actualizaciones ---------------- #
    def _comando_contexto(self) -> str:
        if getattr(sys, "frozen", False):
            return f'"{Path(sys.executable)}" "%1"'
        ejecutable = Path(sys.executable)
        pythonw = ejecutable.with_name("pythonw.exe")
        objetivo = pythonw if pythonw.exists() else ejecutable
        return f'"{objetivo}" "{Path(__file__).resolve()}" "%1"'

    def _icono_contexto(self) -> str:
        if getattr(sys, "frozen", False):
            return str(Path(sys.executable))
        return str(Path(__file__).resolve().parent / "icon.ico")

    @staticmethod
    def _claves_contexto() -> list[str]:
        claves = [
            r"Software\Classes\SystemFileAssociations\image\shell\ConversorWebP",
            r"Software\Classes\Directory\shell\ConversorWebP",
        ]
        for ext in (".zip", ".rar", ".7z"):
            claves.append(
                rf"Software\Classes\SystemFileAssociations\{ext}\shell\ConversorWebP"
            )
        return claves

    def _instalar_menu_contextual(self) -> None:
        try:
            import winreg

            comando = self._comando_contexto()
            icono = self._icono_contexto()
            for base in self._claves_contexto():
                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, base) as clave:
                    winreg.SetValueEx(
                        clave, "", 0, winreg.REG_SZ, "Convertir a WebP con Conversor WebP"
                    )
                    winreg.SetValueEx(clave, "Icon", 0, winreg.REG_SZ, f"{icono},0")
                with winreg.CreateKey(
                    winreg.HKEY_CURRENT_USER, base + r"\command"
                ) as clave:
                    winreg.SetValueEx(clave, "", 0, winreg.REG_SZ, comando)
            messagebox.showinfo(
                APP_NAME,
                "Menu contextual instalado.\n\n"
                "Haz clic derecho sobre una imagen, una carpeta o un .zip/.rar y elige "
                "'Convertir a WebP con Conversor WebP'. El programa se abrira con ese "
                "contenido cargado.",
            )
        except Exception as e:  # noqa: BLE001
            messagebox.showerror(APP_NAME, f"No se pudo instalar el menu contextual:\n{e}")

    def _quitar_menu_contextual(self) -> None:
        try:
            import winreg

            for base in self._claves_contexto():
                for sub in (base + r"\command", base):
                    try:
                        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, sub)
                    except FileNotFoundError:
                        pass
                    except OSError:
                        pass
            messagebox.showinfo(APP_NAME, "Menu contextual eliminado.")
        except Exception as e:  # noqa: BLE001
            messagebox.showerror(APP_NAME, f"No se pudo quitar el menu contextual:\n{e}")

    def _comprobar_actualizaciones(self, manual: bool = False) -> None:
        if self._comprobando_update:
            return
        self._comprobando_update = True
        if manual:
            self.lbl_status.configure(text="Buscando actualizaciones...")

        def tarea() -> None:
            try:
                datos = consultar_ultima_version()
                self.queue.put(("update", {"datos": datos, "manual": manual}))
            except Exception as e:  # noqa: BLE001
                self.queue.put(("update", {"error": str(e), "manual": manual}))

        threading.Thread(target=tarea, daemon=True).start()

    def _procesar_update(self, info: dict) -> None:
        self._comprobando_update = False
        manual = bool(info.get("manual"))
        if info.get("error"):
            if manual:
                messagebox.showwarning(
                    APP_NAME,
                    "No se pudo comprobar si hay actualizaciones:\n" + str(info["error"]),
                )
            return

        datos = info.get("datos")
        if not datos:
            if manual:
                messagebox.showinfo(APP_NAME, "No hay ninguna version publicada.")
            return

        etiqueta, url = datos
        self.ultima_comprobacion = datetime.now().strftime("%Y-%m-%d %H:%M")
        self._save_config()
        if version_tupla(etiqueta) > version_tupla(APP_VERSION):
            self._actualizacion = (etiqueta, url)
            self.lbl_status.configure(text=f"Actualizacion disponible: {etiqueta}")
            self._log("info", f"Nueva version disponible: {etiqueta} -> {url}")
            if manual and messagebox.askyesno(
                APP_NAME,
                f"Hay una version nueva: {etiqueta}\n"
                f"Tienes instalada la {APP_VERSION}.\n\n¿Abrir la pagina de descarga?",
            ):
                webbrowser.open(url)
        elif manual:
            messagebox.showinfo(
                APP_NAME, f"Estas en la ultima version ({APP_VERSION})."
            )

    def _auto_comprobar_update(self) -> None:
        if not self.buscar_actualizaciones:
            return
        try:
            ultima = datetime.strptime(self.ultima_comprobacion, "%Y-%m-%d %H:%M")
        except Exception:
            ultima = None
        if ultima and (datetime.now() - ultima).total_seconds() < 86400:
            return
        self._comprobar_actualizaciones(manual=False)

    def _refrescar_extractor(self) -> None:
        tool = find_extractor()
        if tool:
            self.lbl_extractor.configure(text=f"Extractor: {tool.name}", foreground=MUTED)
        else:
            self.lbl_extractor.configure(
                text="Sin 7-Zip/WinRAR: solo .zip", foreground="#b45309"
            )

    def _refrescar_gpu_ui(self) -> None:
        estado = detectar_hardware_gpu()
        if not estado.hardware:
            self.lbl_gpu.configure(text="Sin GPU detectada")
            self.chk_gpu.configure(state="disabled")
            self.usar_gpu.set(False)
            return
        nombre = estado.nvidia or estado.hardware[0]
        if _GPU.activa:
            texto = f"Activa: {nombre}"
        elif _GPU.intentado:
            texto = "Sin CUDA (ver Herramientas)"
        else:
            texto = nombre
        self.lbl_gpu.configure(text=texto)

    def _dialogo_gpu(self) -> None:
        estado = iniciar_gpu(refrescar=True)
        lineas = [
            "Aceleracion por GPU (CUDA)",
            "",
            "Tarjetas detectadas:",
            ("  · " + "\n  · ".join(estado.hardware)) if estado.hardware else "  (ninguna)",
        ]
        if estado.nvidia:
            lineas.append(
                f"\nNVIDIA: {estado.nvidia} · {estado.memoria} · driver {estado.driver}"
            )
        lineas.append(f"PyTorch: {estado.torch or 'no cargado'}")
        lineas.append(f"torchvision: {estado.torchvision or 'no cargado'}")
        lineas.append(f"Ruta de PyTorch: {estado.ruta or '(integrada en el sistema)'}")
        lineas.append(
            "Forzar en lotes pequenos: " + ("si" if self.gpu_forzar_var.get() else "no")
        )
        lineas.append("\nEstado: " + ("ACTIVA" if estado.activa else f"CPU · {estado.motivo}"))
        texto = "\n".join(lineas)

        if estado.activa:
            messagebox.showinfo(
                APP_NAME,
                texto
                + "\n\nSe usara automaticamente en lotes de 30 o mas JPEG grandes que se "
                "reduzcan (CUDA tarda varios segundos en cargar). Puedes forzarla en "
                "Herramientas -> 'Forzar GPU en lotes pequenos'. El codificador WebP/AVIF "
                "sigue siendo CPU.",
            )
            self._refrescar_gpu_ui()
            return

        texto += (
            "\n\nSi PyTorch CUDA esta instalado, puedes indicar la carpeta site-packages "
            "que lo contiene (por ejemplo, una .venv\\Lib\\site-packages)."
        )
        if not messagebox.askyesno(APP_NAME, texto + "\n\n¿Elegir la carpeta de PyTorch ahora?"):
            self._refrescar_gpu_ui()
            return

        carpeta = filedialog.askdirectory(title="Carpeta site-packages con PyTorch CUDA")
        if not carpeta:
            return
        self.gpu_path = carpeta
        self._save_config()
        estado = iniciar_gpu(refrescar=True)
        if estado.activa:
            messagebox.showinfo(
                APP_NAME,
                f"GPU activa: {estado.nvidia or nombre_gpu()}\n"
                f"PyTorch {estado.torch} · torchvision {estado.torchvision}",
            )
        else:
            messagebox.showwarning(APP_NAME, f"No se pudo activar la GPU:\n{estado.motivo}")
        self._refrescar_gpu_ui()

    def _toggle_forzar_gpu(self) -> None:
        self._save_config()
        estado = "activado" if self.gpu_forzar_var.get() else "desactivado"
        self._log("info", f"GPU forzada en lotes pequenos: {estado}")

    # ---------------- carpeta vigilada y vista previa ---------------- #
    def _crear_job(
        self,
        sources: list[Path],
        salida: Path,
        analizar: bool = False,
        zip_activo: bool | None = None,
    ) -> Job:
        return Job(
            sources=list(sources),
            output_dir=salida,
            quality=int(round(float(self.scale_q.get()))),
            method=int(self.method.get()),
            keep_structure=self.keep_structure.get(),
            overwrite=self.overwrite.get(),
            reencode_webp=self.reencode_webp.get(),
            flatten=self.flatten.get(),
            background=self.background.get(),
            lossless=self.lossless.get(),
            make_zip=(self.make_zip.get() if zip_activo is None else zip_activo),
            zip_name=self.zip_name.get(),
            max_dimension=self._max_dimension(),
            target_kb=self._target_kb(),
            formato="AVIF" if self.formato.get().upper() == "AVIF" else "WEBP",
            csv_report=self.csv_report.get(),
            usar_gpu=self.usar_gpu.get(),
            gpu_forzar=self.gpu_forzar_var.get(),
            name_pattern=self.name_pattern.get(),
            dry_run=analizar,
        )

    def _dialogo_vigilante(self) -> None:
        if self._vigilante is not None and self._vigilante.is_alive():
            if messagebox.askyesno(
                APP_NAME,
                "La carpeta vigilada esta activa.\n\n¿Quieres detenerla?",
            ):
                self._watch_stop.set()
                self._vigilante = None
                self._log("info", "Vigilante: detenido.")
            return

        entrada = filedialog.askdirectory(title="Carpeta que quieres vigilar")
        if not entrada:
            return
        salida = filedialog.askdirectory(title="Carpeta de salida del vigilante")
        if not salida:
            return
        try:
            Path(salida).mkdir(parents=True, exist_ok=True)
        except Exception as e:  # noqa: BLE001
            messagebox.showerror(APP_NAME, f"No se puede crear la carpeta de salida:\n{e}")
            return

        base = self._crear_job([], Path(salida), zip_activo=False)
        base.usar_gpu = False  # evitar cargar CUDA en segundo plano
        self._watch_stop = threading.Event()
        self._vigilante = Vigilante(
            Path(entrada),
            Path(salida),
            base,
            lambda texto: self.queue.put(("watch", texto)),
            self._watch_stop,
        )
        self._vigilante.start()
        self._log("info", f"Vigilante activo: {entrada}  ->  {salida}")
        self.lbl_status.configure(text="Carpeta vigilada activa.")

    def _primera_imagen(self) -> Path | None:
        for origen in self.sources:
            if origen.is_file() and is_image(origen):
                return origen
            if origen.is_dir():
                imagenes = collect_images_from_dir(origen)
                if imagenes:
                    return imagenes[0]
        return None

    def _miniatura(self, imagen: Image.Image, ancho: int, alto: int) -> ImageTk.PhotoImage:
        copia = imagen.copy()
        copia.thumbnail((ancho, alto), Image.LANCZOS)
        return ImageTk.PhotoImage(copia)

    def _vista_previa(self) -> None:
        origen = self._primera_imagen()
        if origen is None:
            messagebox.showinfo(
                APP_NAME,
                "Para la vista previa necesito una imagen suelta o una carpeta "
                "(no un comprimido). Anade una imagen primero.",
            )
            return

        temporal = Path(tempfile.mkdtemp(prefix="conversor_preview_"))
        ext = ".avif" if self.formato.get().upper() == "AVIF" else ".webp"
        destino = temporal / ("vista_previa" + ext)
        job = self._crear_job([origen], temporal, zip_activo=False)
        job.csv_report = False
        try:
            convert_one(origen, destino, job)
            with Image.open(origen) as original:
                original.load()
                tam_original = original.size
                imagen_original = original.convert("RGB")
            with Image.open(destino) as convertida:
                convertida.load()
                tam_convertida = convertida.size
                imagen_convertida = convertida.convert("RGB")
        except Exception as e:  # noqa: BLE001
            shutil.rmtree(temporal, ignore_errors=True)
            messagebox.showerror(APP_NAME, f"No se pudo generar la vista previa:\n{e}")
            return

        ventana = tk.Toplevel(self)
        ventana.title(f"Vista previa · {origen.name}")
        ventana.configure(bg=BG)
        ventana.transient(self)
        cont = ttk.Frame(ventana, padding=14)
        cont.pack(fill="both", expand=True)

        mini_a = self._miniatura(imagen_original, 380, 320)
        mini_b = self._miniatura(imagen_convertida, 380, 320)
        lbl_a = tk.Label(cont, image=mini_a, bg=CARD, relief="flat")
        lbl_a.grid(row=0, column=0, padx=8, pady=8)
        lbl_b = tk.Label(cont, image=mini_b, bg=CARD, relief="flat")
        lbl_b.grid(row=0, column=1, padx=8, pady=8)
        ventana._miniaturas = (mini_a, mini_b)  # type: ignore[attr-defined]

        peso_in = origen.stat().st_size / 1024
        peso_out = destino.stat().st_size / 1024
        ahorro = (1 - peso_out / peso_in) * 100 if peso_in else 0
        ttk.Label(
            cont,
            text=f"Original\n{tam_original[0]}x{tam_original[1]} px\n{peso_in:.1f} KB",
            style="TLabel",
            justify="center",
        ).grid(row=1, column=0, pady=(0, 8))
        ttk.Label(
            cont,
            text=(
                f"Resultado {job.formato}\n{tam_convertida[0]}x{tam_convertida[1]} px\n"
                f"{peso_out:.1f} KB  ({ahorro:.1f} % menos)"
            ),
            style="TLabel",
            justify="center",
        ).grid(row=1, column=1, pady=(0, 8))

        ttk.Button(
            cont,
            text="Cerrar",
            command=lambda: (shutil.rmtree(temporal, ignore_errors=True), ventana.destroy()),
        ).grid(row=2, column=0, columnspan=2, pady=(6, 0))

    def _center(self) -> None:
        self.update_idletasks()
        w, h = self.winfo_width(), self.winfo_height()
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        self.geometry(f"+{max(0, (sw - w) // 2)}+{max(0, (sh - h) // 3)}")

    # ---------------- eventos de UI ---------------- #
    def _on_quality(self, _value=None) -> None:
        self.lbl_q.configure(text=str(int(round(float(self.scale_q.get())))))

    def _on_lossless(self) -> None:
        perdida = self.lossless.get()
        self.scale_q.configure(state="disabled" if perdida else "normal")
        self.lbl_q.configure(foreground=MUTED if perdida else TEXT)
        self.cmb_method.configure(state="disabled" if perdida else "readonly")

    def _on_zip_toggle(self) -> None:
        activo = self.make_zip.get() and not self.dry_run.get()
        self.entry_zip.configure(state="normal" if activo else "disabled")

    def _on_dry_run(self) -> None:
        analizar = self.dry_run.get()
        self.chk_zip.configure(state="disabled" if analizar else "normal")
        self._on_zip_toggle()
        self.lbl_status.configure(
            text="Modo analisis: se calcula el peso sin escribir archivos."
            if analizar
            else "Listo."
        )

    def _ayuda_patron(self) -> None:
        messagebox.showinfo(
            APP_NAME,
            "Patron de nombre de salida\n\n"
            "Dejalo vacio para conservar el nombre original.\n\n"
            "Marcadores disponibles:\n"
            "  {nombre}   nombre original sin extension\n"
            "  {ancho}    ancho final en px (tras redimensionar)\n"
            "  {alto}     alto final en px\n"
            "  {hash}     SHA-1 corto del archivo original\n"
            "  {fecha}    AAAAMMDD-HHMMSS\n"
            "  {formato}  webp o avif\n"
            "  {calidad}  calidad configurada\n\n"
            "Ejemplos:\n"
            "  {nombre}_{ancho}x{alto}\n"
            "  {hash}\n"
            "  {fecha}_{nombre}",
        )

    def _on_formato(self, _event=None) -> None:
        fmt = "AVIF" if self.formato.get().upper() == "AVIF" else "WebP"
        self.formato.set(fmt)
        self.btn_start.configure(text=f"Convertir a {fmt}")
        self.chk_webp.configure(text="Recodificar tambien los .webp / .avif")

    def _on_peso(self) -> None:
        self.entry_peso.configure(state="normal" if self.peso_activo.get() else "disabled")

    # ---------------- presets ---------------- #
    def _valores_preset(self) -> list[str]:
        nombres = ["Personalizado"] + list(PRESETS_BUILTIN.keys())
        for nombre in self.presets_usuario:
            if nombre not in nombres:
                nombres.append(nombre)
        return nombres

    def _refrescar_combo_preset(self) -> None:
        self.cmb_preset.configure(values=self._valores_preset())

    def _marcar_personalizado(self, *_args) -> None:
        if not self._aplicando_preset:
            self.preset_sel.set("Personalizado")

    def _max_dimension(self) -> int:
        valor = self.max_dim.get().strip()
        return int(valor) if valor.isdigit() else 0

    def _target_kb(self) -> int:
        if not self.peso_activo.get():
            return 0
        valor = self.peso_kb.get().strip()
        return int(valor) if valor.isdigit() else 0

    def _ajustes_actuales(self) -> dict:
        return {
            "quality": int(round(float(self.scale_q.get()))),
            "method": int(self.method.get()),
            "lossless": bool(self.lossless.get()),
            "flatten": bool(self.flatten.get()),
            "background": self.background.get(),
            "reencode_webp": bool(self.reencode_webp.get()),
            "max_dimension": self._max_dimension(),
            "target_kb": self._target_kb(),
        }

    def _ajustes_completos(self) -> dict:
        """Ajustes de imagen + opciones de flujo, para el historial."""
        datos = self._ajustes_actuales()
        datos.update(
            {
                "keep_structure": bool(self.keep_structure.get()),
                "overwrite": bool(self.overwrite.get()),
                "make_zip": bool(self.make_zip.get()),
                "zip_name": self.zip_name.get(),
                "formato": self.formato.get(),
                "csv_report": bool(self.csv_report.get()),
            }
        )
        return datos

    def _aplicar_completos(self, datos: dict) -> None:
        self._aplicando_preset = True
        try:
            if "keep_structure" in datos:
                self.keep_structure.set(bool(datos["keep_structure"]))
            if "overwrite" in datos:
                self.overwrite.set(bool(datos["overwrite"]))
            if "make_zip" in datos:
                self.make_zip.set(bool(datos["make_zip"]))
            if str(datos.get("zip_name", "")).strip():
                self.zip_name.set(str(datos["zip_name"]))
            if "formato" in datos:
                self.formato.set(
                    "AVIF" if str(datos["formato"]).upper() == "AVIF" else "WebP"
                )
            if "csv_report" in datos:
                self.csv_report.set(bool(datos["csv_report"]))
        finally:
            self._aplicando_preset = False
        self._aplicar_ajustes(datos)
        self._on_zip_toggle()
        self._on_formato()

    def _aplicar_ajustes(self, datos: dict) -> None:
        self._aplicando_preset = True
        try:
            self.quality.set(int(datos.get("quality", 85)))
            self.scale_q.set(self.quality.get())
            self.method.set(int(datos.get("method", 6)))
            self.lossless.set(bool(datos.get("lossless", False)))
            self.reencode_webp.set(bool(datos.get("reencode_webp", False)))
            self.flatten.set(bool(datos.get("flatten", False)))
            self.background.set(datos.get("background", "#ffffff"))
            self.btn_color.configure(
                bg=self.background.get(), activebackground=self.background.get()
            )
            dim = int(datos.get("max_dimension", 0) or 0)
            self.max_dim.set(str(dim) if dim > 0 else "Sin cambio")
            kb = int(datos.get("target_kb", 0) or 0)
            if kb > 0:
                self.peso_kb.set(str(kb))
            self.peso_activo.set(kb > 0)
        finally:
            self._aplicando_preset = False
        self._on_quality()
        self._on_lossless()
        self._on_flatten()
        self._on_peso()

    def _on_preset(self, _event=None) -> None:
        nombre = self.preset_sel.get()
        if nombre == "Personalizado":
            return
        if nombre in self.presets_usuario:
            self._aplicar_ajustes(self.presets_usuario[nombre])
        elif nombre in PRESETS_BUILTIN:
            self._aplicar_ajustes(PRESETS_BUILTIN[nombre])
        else:
            return
        self.preset_sel.set(nombre)

    def _guardar_preset(self) -> None:
        nombre = simpledialog.askstring(APP_NAME, "Nombre del nuevo preset:", parent=self)
        if not nombre or not nombre.strip():
            return
        nombre = nombre.strip()
        if nombre in PRESETS_BUILTIN:
            messagebox.showinfo(
                APP_NAME, "Ese nombre esta reservado por un preset incluido. Elige otro."
            )
            return
        if nombre in self.presets_usuario and not messagebox.askyesno(
            APP_NAME, f"Ya existe un preset llamado '{nombre}'. ¿Sobrescribirlo?"
        ):
            return
        self.presets_usuario[nombre] = self._ajustes_actuales()
        self._refrescar_combo_preset()
        self.preset_sel.set(nombre)
        self._save_config()
        self._log("info", f"Preset guardado: {nombre}")

    def _borrar_preset(self) -> None:
        nombre = self.preset_sel.get()
        if nombre in self.presets_usuario:
            if messagebox.askyesno(APP_NAME, f"¿Borrar el preset '{nombre}'?"):
                del self.presets_usuario[nombre]
                self.preset_sel.set("Personalizado")
                self._refrescar_combo_preset()
                self._save_config()
        else:
            messagebox.showinfo(
                APP_NAME,
                "Los presets incluidos no se pueden borrar, solo los que guardes tu con 'Guardar'.",
            )

    # ---------------- historial ---------------- #
    def _registrar_historial(self, res: Result) -> None:
        if res.dry_run or not res.converted or res.cancelled:
            return
        entrada = {
            "fecha": datetime.now().strftime("%d/%m/%Y %H:%M"),
            "sources": [str(p) for p in self.sources],
            "output_dir": self.output_dir.get(),
            "ajustes": self._ajustes_completos(),
            "converted": res.converted,
        }
        if (
            self.historial
            and self.historial[0].get("sources") == entrada["sources"]
            and self.historial[0].get("output_dir") == entrada["output_dir"]
        ):
            self.historial[0] = entrada
        else:
            self.historial.insert(0, entrada)
        del self.historial[10:]
        self._refrescar_historial()
        self._save_config()

    def _refrescar_historial(self) -> None:
        if not hasattr(self, "menu_hist"):
            return
        menu = self.menu_hist
        menu.delete(0, "end")
        if not self.historial:
            menu.add_command(label="(sin conversiones todavia)", state="disabled")
            return
        for i, h in enumerate(self.historial):
            nombres = ", ".join(Path(p).name for p in h.get("sources", [])[:2])
            if len(h.get("sources", [])) > 2:
                nombres += f" +{len(h['sources']) - 2}"
            etiqueta = f"{h.get('fecha', '')}  ·  {nombres}  ({h.get('converted', 0)} img)"
            menu.add_command(label=etiqueta[:95], command=lambda i=i: self._repetir_historial(i))
        menu.add_separator()
        menu.add_command(label="Vaciar historial", command=self._vaciar_historial)

    def _repetir_historial(self, indice: int) -> None:
        if not (0 <= indice < len(self.historial)):
            return
        h = self.historial[indice]
        self.sources = [Path(p) for p in h.get("sources", []) if Path(p).exists()]
        if not self.sources:
            messagebox.showinfo(APP_NAME, "Los archivos de ese trabajo ya no existen.")
            return
        self.output_dir.set(h.get("output_dir", ""))
        self._aplicar_completos(h.get("ajustes", {}))
        self.preset_sel.set("Personalizado")
        self._refrescar_origen()
        self._log("info", f"Historial: cargado el trabajo del {h.get('fecha', '')}.")
        self.lbl_status.configure(text="Trabajo del historial cargado. Pulsa Convertir.")

    def _vaciar_historial(self) -> None:
        if self.historial and messagebox.askyesno(APP_NAME, "¿Vaciar el historial de trabajos?"):
            self.historial.clear()
            self._refrescar_historial()
            self._save_config()

    def _on_flatten(self) -> None:
        activo = self.flatten.get()
        self.btn_color.configure(state="normal" if activo else "disabled")
        self.lbl_color.configure(foreground=TEXT if activo else MUTED)

    def _pick_background(self) -> None:
        color = colorchooser.askcolor(color=self.background.get(), title="Color de fondo")
        if color and color[1]:
            self.background.set(color[1])
            self.btn_color.configure(bg=color[1], activebackground=color[1])

    # ---------------- origen ---------------- #
    def _refrescar_origen(self) -> None:
        self.source_list.delete(0, "end")
        for p in self.sources:
            etiqueta = str(p)
            if p.suffix.lower() in ARCHIVE_EXTS:
                etiqueta = f"{p}   [comprimido]"
            elif p.is_dir():
                etiqueta = f"{p}   [carpeta]"
            self.source_list.insert("end", etiqueta)
        if not self.sources:
            self.lbl_count.configure(text="Sin origen seleccionado")
            return
        carpetas = sum(1 for p in self.sources if p.is_dir())
        zips = sum(1 for p in self.sources if p.suffix.lower() in ARCHIVE_EXTS)
        archivos = len(self.sources) - carpetas - zips
        partes = []
        if carpetas:
            partes.append(f"{carpetas} carpeta(s)")
        if zips:
            partes.append(f"{zips} comprimido(s)")
        if archivos:
            partes.append(f"{archivos} imagen(es)")
        self.lbl_count.configure(text="Origen: " + ", ".join(partes))

    def _anadir(self, rutas: list[Path]) -> None:
        ignorados = 0
        existentes: set[Path] = set()
        for p in self.sources:
            try:
                existentes.add(p.resolve())
            except OSError:
                existentes.add(p)

        for r in rutas:
            try:
                clave = r.resolve()
            except OSError:
                clave = r
            if clave in existentes or r in self.sources:
                continue
            if not r.exists():
                ignorados += 1
                continue
            if r.is_dir() or is_image(r) or r.suffix.lower() in ARCHIVE_EXTS:
                self.sources.append(r)
                existentes.add(clave)
            else:
                ignorados += 1
        self._refrescar_origen()

        if not self.output_dir.get() and self.sources:
            primera = self.sources[0]
            if primera.is_dir():
                base, nombre = primera, primera.name
            elif primera.suffix.lower() in ARCHIVE_EXTS:
                base, nombre = primera.parent, primera.stem
            else:
                base, nombre = primera.parent, primera.parent.name
            self.output_dir.set(str(base / f"{nombre}_webp"))

        if ignorados:
            messagebox.showinfo(
                APP_NAME, f"Se ignoraron {ignorados} archivo(s) que no son imagenes ni comprimidos."
            )

    def _add_files(self) -> None:
        rutas = filedialog.askopenfilenames(
            title="Selecciona imagenes",
            filetypes=[
                (
                    "Imagenes",
                    "*.jpg *.jpeg *.jfif *.png *.bmp *.gif *.tif *.tiff *.ico "
                    "*.webp *.avif *.tga *.pcx *.dds *.ppm *.pgm *.pbm",
                ),
                ("Todos los archivos", "*.*"),
            ],
        )
        if rutas:
            self._anadir([Path(r) for r in rutas])

    def _add_folder(self) -> None:
        carpeta = filedialog.askdirectory(title="Selecciona una carpeta con imagenes")
        if carpeta:
            self._anadir([Path(carpeta)])

    def _add_archive(self) -> None:
        rutas = filedialog.askopenfilenames(
            title="Selecciona comprimidos",
            filetypes=[("Comprimidos", "*.zip *.rar *.7z"), ("Todos los archivos", "*.*")],
        )
        if rutas:
            self._anadir([Path(r) for r in rutas])

    def _clear_sources(self) -> None:
        self.sources.clear()
        self._refrescar_origen()

    def _quitar_seleccionados(self) -> None:
        indices = list(self.source_list.curselection())
        if not indices:
            return
        for i in reversed(indices):
            del self.sources[i]
            self.source_list.delete(i)
        self._refrescar_origen()

    def _bind_shortcuts(self) -> None:
        self.bind_all("<Control-o>", lambda _e: self._add_files())
        self.bind_all("<Control-O>", lambda _e: self._add_folder())  # Ctrl+Mayus+O
        self.bind_all("<Control-Return>", self._atajo_convertir)
        self.bind_all("<F5>", self._atajo_convertir)

    def _atajo_convertir(self, _event=None):
        if not self.running:
            self._start()
        return "break"

    def _parsear_drop(self, data: str) -> list[Path]:
        try:
            crudos = self.tk.splitlist(data)
        except Exception:
            crudos = [data.strip("{}")]
        return [Path(c.strip("{}")) for c in crudos if c.strip("{}")]

    def _drop_window(self, event):
        self._anadir(self._parsear_drop(event.data))
        return "copy"

    def _drop_paths(self, data: str) -> None:
        self._anadir(self._parsear_drop(data))

    def _drop_output(self, event):
        rutas = self._parsear_drop(event.data)
        if len(rutas) == 1 and rutas[0].is_dir():
            self.output_dir.set(str(rutas[0]))
        else:
            self._anadir(rutas)
        return "copy"

    def _registrar_dnd_global(self) -> None:
        """Permite soltar archivos en cualquier punto de la ventana."""
        pila: list[tk.Misc] = [self]
        while pila:
            widget = pila.pop()
            pila.extend(widget.winfo_children())
            if isinstance(widget, DropZone):
                continue  # ya tiene su propio manejador
            try:
                widget.drop_target_register(DND_FILES)  # type: ignore[attr-defined]
                if widget is self.entry_out:
                    widget.dnd_bind("<<Drop>>", self._drop_output)  # type: ignore[attr-defined]
                else:
                    widget.dnd_bind("<<Drop>>", self._drop_window)  # type: ignore[attr-defined]
            except Exception:
                continue

    # ---------------- destino ---------------- #
    def _pick_output(self) -> None:
        inicial = self.output_dir.get() or ""
        carpeta = filedialog.askdirectory(
            title="Carpeta de salida", initialdir=inicial if Path(inicial).is_dir() else None
        )
        if carpeta:
            self.output_dir.set(carpeta)

    def _open_output(self) -> None:
        destino = self.output_dir.get().strip()
        if destino and Path(destino).is_dir():
            os.startfile(destino)  # type: ignore[attr-defined]
        else:
            messagebox.showinfo(APP_NAME, "La carpeta de salida todavia no existe.")

    # ---------------- ejecucion ---------------- #
    def _start(self) -> None:
        if self.running:
            return
        if not self.sources:
            messagebox.showwarning(APP_NAME, "Arrastra o anade al menos una imagen, carpeta o comprimido.")
            return
        destino = self.output_dir.get().strip()
        analizar = self.dry_run.get()
        if not destino and not analizar:
            messagebox.showwarning(APP_NAME, "Selecciona la carpeta de salida.")
            return
        salida = (
            Path(destino)
            if destino
            else Path(tempfile.gettempdir()) / "conversor_webp_analisis"
        )
        if not analizar:
            try:
                salida.mkdir(parents=True, exist_ok=True)
                prueba = salida / ".escritura_test"
                prueba.write_text("ok", encoding="utf-8")
                prueba.unlink()
            except Exception as e:  # noqa: BLE001
                messagebox.showerror(APP_NAME, f"No se puede escribir en la carpeta de salida:\n{e}")
                return
        elif self.csv_report.get():
            try:
                salida.mkdir(parents=True, exist_ok=True)
            except Exception as e:  # noqa: BLE001
                messagebox.showerror(APP_NAME, f"No se puede escribir el informe CSV:\n{e}")
                return

        if self.peso_activo.get():
            texto_peso = self.peso_kb.get().strip()
            if not texto_peso.isdigit() or int(texto_peso) < 5:
                messagebox.showerror(
                    APP_NAME,
                    "El 'Peso maximo' debe ser un numero entero de KB (minimo 5).\n"
                    "Ejemplo: 150",
                )
                return
            if self.lossless.get():
                messagebox.showinfo(
                    APP_NAME,
                    "El modo 'Sin perdida' no admite un peso maximo: la calidad no se puede "
                    "ajustar sin perder fidelidad. Se aplicara sin limite de peso.",
                )

        if self.flatten.get():
            try:
                hex_to_rgb(self.background.get())
            except ValueError as e:
                messagebox.showerror(APP_NAME, f"Color de fondo no valido:\n{e}")
                return

        job = Job(
            sources=list(self.sources),
            output_dir=salida,
            quality=int(round(float(self.scale_q.get()))),
            method=int(self.method.get()),
            keep_structure=self.keep_structure.get(),
            overwrite=self.overwrite.get(),
            reencode_webp=self.reencode_webp.get(),
            flatten=self.flatten.get(),
            background=self.background.get(),
            lossless=self.lossless.get(),
            make_zip=self.make_zip.get() and not analizar,
            zip_name=self.zip_name.get(),
            max_dimension=self._max_dimension(),
            target_kb=self._target_kb(),
            formato="AVIF" if self.formato.get().upper() == "AVIF" else "WEBP",
            csv_report=self.csv_report.get(),
            usar_gpu=self.usar_gpu.get(),
            gpu_forzar=self.gpu_forzar_var.get(),
            name_pattern=self.name_pattern.get(),
            dry_run=analizar,
        )

        self._set_running(True)
        self.progress.configure(value=0, maximum=100)
        self._clear_log()
        self._limpiar_tabla()
        if analizar:
            self._log("warn", "MODO ANALISIS: no se escribiran imagenes, solo el informe.")
        self._log("info", f"Salida: {salida}  ·  Formato: {job.formato}")
        if job.name_pattern.strip():
            self._log("info", f"Patron de nombres: {job.name_pattern}")
        if job.max_dimension:
            self._log("info", f"Redimensionando al lado maximo: {job.max_dimension} px")
        if job.target_kb and not job.lossless:
            self._log("info", f"Peso maximo por imagen: {job.target_kb} KB")
        if job.csv_report:
            self._log("info", "Se guardara un informe CSV al terminar.")
        if job.usar_gpu and job.max_dimension:
            self._log("info", "Aceleracion GPU: se comprobara al empezar (solo JPEG grandes).")
        if job.make_zip:
            self._log("info", f"ZIP resultante: {job.zip_name}")
        self.total = 0
        self.lbl_status.configure(text="Preparando...")
        self.cancel_event.clear()

        self.worker = threading.Thread(
            target=run_job,
            args=(job, lambda *evento: self.queue.put(evento), self.cancel_event),
            daemon=True,
        )
        self.worker.start()

    def _cancel(self) -> None:
        if self.running:
            self.cancel_event.set()
            self.lbl_status.configure(text="Cancelando...")

    def _set_running(self, value: bool) -> None:
        self.running = value
        estado = "disabled" if value else "normal"
        for w in (
            self.btn_start, self.btn_add_files, self.btn_add_folder,
            self.btn_add_zip, self.btn_remove, self.btn_clear, self.btn_browse,
        ):
            w.configure(state=estado)
        self.cmb_preset.configure(state="disabled" if value else "readonly")
        for w in (self.btn_save_preset, self.btn_del_preset):
            w.configure(state=estado)
        self.btn_cancel.configure(state="normal" if value else "disabled")

    def _clear_log(self) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _limpiar_tabla(self) -> None:
        for iid in self.tabla.get_children():
            self.tabla.delete(iid)
        self._rutas_filas.clear()
        self._contador_filas = 0
        self.notas.select(0)

    def _anadir_fila(self, fila: dict) -> None:
        self._contador_filas += 1
        iid = f"f{self._contador_filas}"
        valores = (
            fila.get("archivo", ""),
            "" if fila.get("entrada") is None else f"{float(fila['entrada']):.1f}",
            "" if fila.get("salida") is None else f"{float(fila['salida']):.1f}",
            "" if fila.get("ahorro") is None else f"{float(fila['ahorro']):.1f}",
            fila.get("estado", ""),
        )
        self.tabla.insert(
            "", "end", iid=iid, values=valores, tags=(str(fila.get("estado", "")),)
        )
        if fila.get("ruta"):
            self._rutas_filas[iid] = str(fila["ruta"])
        self.tabla.see(iid)

    def _abrir_fila(self, _event=None):
        seleccion = self.tabla.selection()
        if not seleccion:
            return
        ruta = self._rutas_filas.get(seleccion[0], "")
        if not ruta:
            return
        destino = Path(ruta)
        if not destino.exists():
            messagebox.showinfo(APP_NAME, "El archivo de esa fila ya no existe.")
            return
        try:
            os.startfile(destino)  # type: ignore[attr-defined]
        except Exception:
            pass

    def _log(self, tag: str, texto: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", texto + "\n", tag)
        self.log.see("end")
        self.log.configure(state="disabled")

    def _drain_queue(self) -> None:
        if self._closing:
            return
        try:
            while True:
                tipo, dato = self.queue.get_nowait()
                if tipo == "plan":
                    self.total = dato
                    self.progress.configure(maximum=max(1, dato), value=0)
                    self.lbl_status.configure(text=f"{dato} imagen(es) a convertir...")
                    self._log("info", f"Imagenes a convertir: {dato}")
                elif tipo == "progress":
                    hechas, total = dato
                    self.progress.configure(value=hechas, maximum=max(1, total))
                    pct = (hechas / total * 100) if total else 0
                    self.lbl_status.configure(text=f"Convirtiendo {hechas}/{total}  ({pct:.0f}%)")
                elif tipo == "status":
                    self.lbl_status.configure(text=dato)
                elif tipo == "log":
                    self._log(*dato)
                elif tipo == "fila":
                    self._anadir_fila(dato)
                elif tipo == "watch":
                    self._log("info", str(dato))
                elif tipo == "update":
                    self._procesar_update(dato)
                elif tipo == "done":
                    self._finish(dato)
        except queue.Empty:
            pass
        if not self._closing:
            self._after_id = self.after(100, self._drain_queue)

    def _reproducir_aviso(self, correcto: bool) -> None:
        """Pitido final: agudo si todo fue bien, grave si hubo errores o se cancelo."""
        if not self.beep.get() or winsound is None:
            return

        def tono() -> None:
            try:
                if correcto:
                    winsound.Beep(880, 120)
                    winsound.Beep(1320, 160)
                else:
                    winsound.Beep(440, 200)
                    winsound.Beep(330, 280)
            except Exception:
                try:
                    winsound.MessageBeep(
                        winsound.MB_ICONASTERISK if correcto else winsound.MB_ICONHAND
                    )
                except Exception:
                    pass

        threading.Thread(target=tono, daemon=True).start()

    def _finish(self, res: Result) -> None:
        self._set_running(False)
        self._reproducir_aviso(not res.failed and not res.cancelled)
        self._registrar_historial(res)
        mb_in = res.bytes_in / 1048576
        mb_out = res.bytes_out / 1048576
        ahorro = (1 - res.bytes_out / res.bytes_in) * 100 if res.bytes_in else 0

        resumen = (
            f"{'Analizadas' if res.dry_run else 'Convertidas'}: {res.converted}   "
            f"Omitidas: {res.skipped}   Fallidas: {len(res.failed)}"
        )
        self._log("info", "-" * 72)
        self._log("info", resumen)
        if res.converted:
            self._log("info", f"Tamano: {mb_in:.2f} MB -> {mb_out:.2f} MB  (ahorro {ahorro:.1f} %)")
        if res.zip_path:
            self._log("info", f"ZIP: {res.zip_path}")
        if res.csv_path:
            self._log("info", f"Informe: {res.csv_path}")
        self.lbl_status.configure(text=resumen)
        if self.total:
            self.progress.configure(value=self.progress.cget("maximum"))

        if res.dry_run and not res.cancelled:
            extra = f"\n\nPeso estimado: {mb_in:.2f} MB -> {mb_out:.2f} MB  (ahorro {ahorro:.1f} %)"
            if res.csv_path:
                extra += f"\nInforme CSV:\n{res.csv_path.name}"
            if res.failed:
                extra += "\n\nRevisa el registro para ver los errores."
            messagebox.showinfo(APP_NAME, "Analisis completado.\n" + resumen + extra)
            return

        if res.cancelled:
            messagebox.showwarning(APP_NAME, "Proceso cancelado.\n\n" + resumen)
        elif res.failed:
            messagebox.showwarning(
                APP_NAME, resumen + "\n\nRevisa el registro inferior para ver los detalles."
            )
        elif res.converted:
            extra = f"\n\nAhorro de espacio: {ahorro:.1f} %"
            if res.zip_path:
                extra += f"\nZIP creado:\n{res.zip_path.name}"
            if res.csv_path:
                extra += f"\nInforme CSV:\n{res.csv_path.name}"
            messagebox.showinfo(APP_NAME, resumen + extra)
        elif res.skipped:
            messagebox.showinfo(
                APP_NAME,
                f"No se convirtio ninguna imagen: {res.skipped} ya existian y se omitieron.\n"
                "Activa 'Sobrescribir archivos existentes' si quieres regenerarlas.",
            )
        else:
            messagebox.showinfo(APP_NAME, "No se encontro ninguna imagen que convertir.")

    def _al_destruir(self, event=None) -> None:
        """Detiene el bucle de eventos pendiente al cerrar la ventana."""
        if event is not None and event.widget is not self:
            return
        self._closing = True
        if self._after_id is not None:
            try:
                self.after_cancel(self._after_id)
            except Exception:
                pass
            self._after_id = None
        if self._after_menu is not None:
            try:
                self.after_cancel(self._after_menu)
            except Exception:
                pass
            self._after_menu = None
        for tip in self._tooltips:
            tip._ocultar()

    def _on_close(self) -> None:
        if self.running:
            if not messagebox.askyesno(
                APP_NAME, "Hay una conversion en curso. ¿Salir de todas formas?"
            ):
                return
            self.cancel_event.set()
            if self.worker is not None:
                self.worker.join(timeout=1.5)
        self._watch_stop.set()
        self._al_destruir()
        self._save_config()
        self.destroy()

    # ---------------- configuracion persistente ---------------- #
    def _load_config(self) -> None:
        try:
            data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
            if not isinstance(data, dict):
                raise ValueError("configuracion no valida")
        except Exception:
            data = {}

        def entero(clave: str, defecto: int, minimo: int | None = None, maximo: int | None = None) -> int:
            try:
                valor = int(data.get(clave, defecto))
            except (TypeError, ValueError):
                valor = defecto
            if minimo is not None:
                valor = max(minimo, valor)
            if maximo is not None:
                valor = min(maximo, valor)
            return valor

        def booleano(clave: str, defecto: bool) -> bool:
            valor = data.get(clave, defecto)
            return valor if isinstance(valor, bool) else defecto

        self.quality.set(entero("quality", 85, 1, 100))
        self.method.set(entero("method", 6, 0, 6))
        self.lossless.set(booleano("lossless", False))
        self.keep_structure.set(booleano("keep_structure", True))
        self.overwrite.set(booleano("overwrite", True))
        self.reencode_webp.set(booleano("reencode_webp", False))
        self.flatten.set(booleano("flatten", False))
        self.make_zip.set(booleano("make_zip", True))
        self.beep.set(booleano("beep", True))
        self.csv_report.set(booleano("csv_report", False))
        self.usar_gpu.set(booleano("usar_gpu", False))
        self.gpu_path = str(data.get("gpu_path", "") or "")
        self.gpu_forzar_var.set(booleano("gpu_forzar", False))
        self.name_pattern.set(str(data.get("name_pattern", "") or ""))
        self.dry_run.set(booleano("dry_run", False))
        self.buscar_actualizaciones = booleano("buscar_actualizaciones", True)
        self.ultima_comprobacion = str(data.get("ultima_comprobacion", "") or "")
        self.formato.set("AVIF" if str(data.get("formato", "WebP")).upper() == "AVIF" else "WebP")

        fondo = data.get("background", "#ffffff")
        try:
            hex_to_rgb(str(fondo))
            self.background.set(str(fondo))
        except ValueError:
            self.background.set("#ffffff")
        self.btn_color.configure(bg=self.background.get(), activebackground=self.background.get())

        self.output_dir.set(str(data.get("output_dir", "") or ""))
        self.zip_name.set(str(data.get("zip_name", self.zip_name.get()) or self.zip_name.get()))

        dim = entero("max_dimension", 0, 0)
        self.max_dim.set(str(dim) if dim > 0 else "Sin cambio")
        kb = entero("target_kb", 0, 0)
        self.peso_kb.set(str(kb) if kb > 0 else "150")
        self.peso_activo.set(kb > 0)

        presets = data.get("presets")
        if isinstance(presets, dict):
            self.presets_usuario = {
                str(k): dict(v) for k, v in presets.items() if isinstance(v, dict)
            }
        else:
            self.presets_usuario = {}

        historial = data.get("historial")
        if isinstance(historial, list):
            self.historial = [h for h in historial if isinstance(h, dict)][:10]
        else:
            self.historial = []

        self.scale_q.set(self.quality.get())
        self._on_quality()
        self._on_lossless()
        self._on_flatten()
        self._on_zip_toggle()
        self._on_peso()
        self._on_formato()
        self._refrescar_combo_preset()
        self._refrescar_historial()
        self._refrescar_gpu_ui()
        self._on_dry_run()

    def _save_config(self) -> None:
        try:
            CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
            texto = json.dumps(
                {
                    "quality": int(round(float(self.scale_q.get()))),
                    "method": int(self.method.get()),
                    "lossless": self.lossless.get(),
                    "keep_structure": self.keep_structure.get(),
                    "overwrite": self.overwrite.get(),
                    "reencode_webp": self.reencode_webp.get(),
                    "flatten": self.flatten.get(),
                    "make_zip": self.make_zip.get(),
                    "beep": self.beep.get(),
                    "background": self.background.get(),
                    "output_dir": self.output_dir.get(),
                    "zip_name": self.zip_name.get(),
                    "max_dimension": self._max_dimension(),
                    "target_kb": self._target_kb(),
                    "formato": self.formato.get(),
                    "csv_report": self.csv_report.get(),
                    "usar_gpu": self.usar_gpu.get(),
                    "gpu_path": self.gpu_path,
                    "gpu_forzar": self.gpu_forzar_var.get(),
                    "name_pattern": self.name_pattern.get(),
                    "dry_run": self.dry_run.get(),
                    "buscar_actualizaciones": self.buscar_actualizaciones,
                    "ultima_comprobacion": self.ultima_comprobacion,
                    "presets": self.presets_usuario,
                    "historial": self.historial,
                },
                indent=2,
                ensure_ascii=False,
            )
            temporal = CONFIG_FILE.with_name(CONFIG_FILE.name + ".tmp")
            temporal.write_text(texto, encoding="utf-8")
            os.replace(temporal, CONFIG_FILE)
        except Exception:
            pass


# --------------------------------------------------------------------------- #
#  Modo consola
# --------------------------------------------------------------------------- #
def _say(msg: str) -> None:
    """print() seguro: en la version compilada sin consola sys.stdout es None."""
    try:
        if sys.stdout is not None:
            print(msg, flush=True)
    except Exception:
        pass


def run_cli(argv: list[str]) -> int:
    import argparse

    def color_arg(valor: str) -> str:
        try:
            hex_to_rgb(valor)
        except ValueError as e:
            raise argparse.ArgumentTypeError(str(e)) from e
        return valor

    parser = argparse.ArgumentParser(
        prog="ConversorWebP",
        description="Convierte imagenes a WebP. Sin argumentos abre la interfaz grafica.",
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {APP_VERSION}")
    parser.add_argument("origen", nargs="+", help="Archivos, carpetas o .zip/.rar de origen")
    parser.add_argument("-o", "--output", required=True, help="Carpeta de salida")
    parser.add_argument("-q", "--quality", type=int, default=85, help="Calidad 1-100 (85)")
    parser.add_argument("-m", "--method", type=int, default=6, help="Esfuerzo 0-6 (6)")
    parser.add_argument("--lossless", action="store_true", help="Modo sin perdida")
    parser.add_argument("--flat", action="store_true", help="Aplanar transparencia")
    parser.add_argument("--bg", type=color_arg, default="#ffffff", help="Color de fondo al aplanar")
    parser.add_argument("--no-recursive", action="store_true", help="No mantener subcarpetas")
    parser.add_argument("--no-overwrite", action="store_true", help="Omitir WebP existentes")
    parser.add_argument("--webp", action="store_true", help="Recodificar tambien los .webp")
    parser.add_argument("--zip", dest="zip_name", default=None, help="Crear un ZIP con el resultado")
    parser.add_argument("--max-dim", type=int, default=0,
                        help="Redimensionar al lado maximo indicado en px (0 = sin cambio)")
    parser.add_argument("--max-kb", type=int, default=0,
                        help="Peso maximo por imagen en KB (0 = sin limite)")
    parser.add_argument("--format", dest="formato", default="webp", type=str.lower,
                        choices=["webp", "avif"], help="Formato de salida (webp o avif)")
    parser.add_argument("--csv", action="store_true", help="Guardar informe CSV en la salida")
    parser.add_argument("--gpu", action="store_true",
                        help="Forzar la GPU (aunque el lote sea pequeno)")
    parser.add_argument("--no-gpu", action="store_true", help="Desactivar la aceleracion GPU")
    args = parser.parse_args(argv)

    job = Job(
        sources=[Path(p) for p in args.origen],
        output_dir=Path(args.output),
        quality=max(1, min(100, args.quality)),
        method=max(0, min(6, args.method)),
        keep_structure=not args.no_recursive,
        overwrite=not args.no_overwrite,
        reencode_webp=args.webp,
        flatten=args.flat,
        background=args.bg,
        lossless=args.lossless,
        make_zip=args.zip_name is not None,
        zip_name=args.zip_name or "",
        max_dimension=max(0, args.max_dim),
        target_kb=max(0, args.max_kb),
        formato="AVIF" if args.formato == "avif" else "WEBP",
        csv_report=bool(args.csv),
        usar_gpu=bool(args.gpu) and not args.no_gpu,
        gpu_forzar=bool(args.gpu) and not args.no_gpu,
    )
    try:
        job.output_dir.mkdir(parents=True, exist_ok=True)
    except Exception as e:  # noqa: BLE001
        _say(f"ERROR: no se puede crear la carpeta de salida: {e}")
        return 2

    def emit(tipo, dato):
        if tipo == "log":
            _say(dato[1])
        elif tipo == "done":
            emit.result = dato
        elif tipo == "plan":
            _say(f"Imagenes a convertir: {dato}")

    emit.result = Result()
    run_job(job, emit, threading.Event())
    res = emit.result
    _say("-" * 60)
    _say(f"Convertidas: {res.converted}  Omitidas: {res.skipped}  Fallidas: {len(res.failed)}")
    if res.bytes_in:
        ahorro = (1 - res.bytes_out / res.bytes_in) * 100
        _say(f"{res.bytes_in / 1048576:.2f} MB -> {res.bytes_out / 1048576:.2f} MB (ahorro {ahorro:.1f} %)")
    if res.zip_path:
        _say(f"ZIP: {res.zip_path}")
    return 0 if not res.failed else 1


def write_diag(destino: str) -> int:
    """Escribe un informe de diagnostico (util para soporte y pruebas)."""
    estado_gpu = iniciar_gpu(refrescar=True)
    try:
        importable = importlib.util.find_spec("torch") is not None
    except Exception:
        importable = False
    info: dict = {
        "app": APP_NAME,
        "version": APP_VERSION,
        "python": sys.version.split()[0],
        "congelado": bool(getattr(sys, "frozen", False)),
        "pillow": getattr(Image, "__version__", "?"),
        "tkinterdnd2_importado": DND_AVAILABLE,
        "extractor": str(find_extractor()) if find_extractor() else None,
        "manual": str(ruta_manual()) if ruta_manual() else None,
        "winsound": winsound is not None,
        "heif": HEIF_DISPONIBLE,
        "config": str(CONFIG_FILE),
        "portable": (Path(sys.executable).parent / "portable.txt").is_file()
        if getattr(sys, "frozen", False)
        else (Path(__file__).parent / "portable.txt").is_file(),
        "gpu": estado_gpu.hardware,
        "gpu_nvidia": estado_gpu.nvidia,
        "gpu_driver": estado_gpu.driver,
        "gpu_candidatos": [str(p) for p in candidatos_torch()],
        "torch_importable": importable,
        "gpu_activa": _GPU.activa,
        "gpu_motivo": _GPU.motivo,
        "gpu_ruta": _GPU.ruta,
    }
    if DND_AVAILABLE:
        try:
            prueba = TkinterDnD.Tk()  # type: ignore[union-attr]
            prueba.withdraw()
            prueba.update()
            prueba.destroy()
            info["tkdnd_operativo"] = True
        except Exception as e:  # noqa: BLE001
            info["tkdnd_operativo"] = False
            info["tkdnd_error"] = repr(e)
    try:
        Path(destino).write_text(json.dumps(info, indent=2, ensure_ascii=False), encoding="utf-8")
    except Exception:
        pass
    _say(json.dumps(info, indent=2, ensure_ascii=False))
    return 0


def main() -> None:
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "--diag":
        sys.exit(write_diag(args[1]))
    if args:
        rutas = [Path(a) for a in args]
        if all(r.exists() for r in rutas):
            # Venimos de "Enviar a" / "Abrir con": abrimos la GUI ya cargada
            App(sources_iniciales=rutas).mainloop()
            return
        sys.exit(run_cli(args))
    App().mainloop()


if __name__ == "__main__":
    main()
