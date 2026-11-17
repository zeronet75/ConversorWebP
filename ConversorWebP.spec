# -*- mode: python ; coding: utf-8 -*-
"""Especificación de PyInstaller para Conversor WebP.

Genera un único .exe sin consola. PyTorch NO se incluye (ocuparía ~2,4 GB):
la aplicación lo carga en tiempo de ejecución desde una instalación del
sistema si existe, para la aceleración CUDA opcional.

Por eso se incluye toda la biblioteca estándar: torch/torchvision importan
módulos (timeit, pdb, pickletools, modulefinder, html.parser...) que
PyInstaller no detectaría en un análisis estático de esta app.

Uso:
    pyinstaller --noconfirm --clean ConversorWebP.spec
"""

import sys

from PyInstaller.utils.hooks import collect_all, collect_submodules

EXCLUIR_STDLIB = {
    "antigravity", "this", "idlelib", "turtledemo", "test",
    "ensurepip", "venv", "lib2to3", "tkinter",
}

stdlib = [
    m for m in sys.stdlib_module_names
    if not m.startswith("_") and m not in EXCLUIR_STDLIB
]

hiddenimports = stdlib + [
    # submódulos que torchvision importa de forma dinámica
    "html.parser", "html.entities",
    # módulos de Pillow que usa torchvision y la app no necesita
    "PIL.ImageDraw", "PIL.ImageFont", "PIL.ImageColor", "PIL.ImageEnhance",
    "PIL.ImageFilter", "PIL.ImageChops", "PIL.ImageOps", "PIL.ImageMath",
    "PIL.ImagePalette",
]

for paquete in ("html", "http", "urllib", "email", "xml"):
    hiddenimports += collect_submodules(paquete)

datos_tkdnd = collect_all("tkinterdnd2")
datas = [("MANUAL_DE_USO.html", ".")] + datos_tkdnd[0]
binaries = datos_tkdnd[1]
hiddenimports += datos_tkdnd[2]

a = Analysis(
    ["conversor_webp.py"],
    pathex=[],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=["torch", "torchvision", "numpy", "PyQt5", "PySide6", "matplotlib"],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="ConversorWebP",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon="icon.ico",
)
