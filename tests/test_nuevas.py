"""Pruebas de las funciones añadidas en 2.5.0."""

import os
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ))

import conversor_webp as cw  # noqa: E402


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="test_nuevas_"))
        self._config = cw.CONFIG_FILE
        cw.CONFIG_FILE = self.tmp / "config.json"

    def tearDown(self):
        cw.CONFIG_FILE = self._config
        shutil.rmtree(self.tmp, ignore_errors=True)

    def imagen(self, nombre="foto.jpg", tam=(640, 480)):
        ruta = self.tmp / nombre
        ruta.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGB", tam, (30, 120, 200)).save(ruta)
        return ruta

    def ejecutar(self, job):
        resultado = {}

        def emit(tipo, dato):
            if tipo == "done":
                resultado["r"] = dato

        cw.run_job(job, emit, threading.Event())
        return resultado["r"]


class TestPatrones(Base):
    def test_patron_dimensiones(self):
        src = self.imagen()
        job = cw.Job(sources=[src], output_dir=self.tmp / "out", max_dimension=320)
        ruta = cw.aplicar_patron("{nombre}_{ancho}x{alto}", src, Path("foto.jpg"), job, ".webp")
        self.assertEqual(ruta.name, "foto_320x240.webp")

    def test_patron_hash(self):
        src = self.imagen()
        job = cw.Job(sources=[src], output_dir=self.tmp / "out")
        ruta = cw.aplicar_patron("{hash}", src, Path("foto.jpg"), job, ".webp")
        self.assertEqual(len(ruta.stem), 10)
        self.assertNotEqual(ruta.stem, "foto")

    def test_patron_vacio(self):
        src = self.imagen()
        job = cw.Job(sources=[src], output_dir=self.tmp / "out")
        ruta = cw.aplicar_patron("", src, Path("sub/foto.jpg"), job, ".webp")
        self.assertEqual(ruta.as_posix(), "sub/foto.webp")


class TestDryRun(Base):
    def test_no_escribe_y_genera_csv(self):
        src = self.imagen()
        out = self.tmp / "out"
        job = cw.Job(sources=[src], output_dir=out, dry_run=True, csv_report=True)
        res = self.ejecutar(job)
        self.assertTrue(res.dry_run)
        self.assertEqual(res.converted, 1)
        self.assertFalse(list(out.glob("*.webp")))
        self.assertIsNotNone(res.csv_path)
        self.assertTrue(res.csv_path.name.startswith("analisis_"))


class TestVersiones(unittest.TestCase):
    def test_comparacion(self):
        self.assertGreater(cw.version_tupla("v2.10.0"), cw.version_tupla("2.9.9"))
        self.assertEqual(cw.version_tupla("2.5"), (2, 5))
        self.assertFalse(cw.version_tupla("v2.4.1") > cw.version_tupla("2.5.0"))


class TestPortable(unittest.TestCase):
    def test_variable_de_entorno(self):
        entorno = {**os.environ, "CONVERSORWEBP_PORTABLE": "1"}
        proc = subprocess.run(
            [
                sys.executable,
                "-c",
                f"import sys; sys.path.insert(0, r'{RAIZ}'); "
                "import conversor_webp as cw; print(cw.CONFIG_FILE)",
            ],
            capture_output=True,
            text=True,
            env=entorno,
            timeout=120,
        )
        self.assertTrue(proc.stdout.strip().endswith("config.json"))
        self.assertIn("ConversorWebP".lower(), proc.stdout.lower())


class TestHEIC(Base):
    def test_conversion_heic(self):
        if not cw.HEIF_DISPONIBLE:
            self.skipTest("pillow-heif no disponible")
        ruta = self.tmp / "foto.heic"
        try:
            Image.new("RGB", (120, 90), (200, 40, 40)).save(ruta, format="HEIF")
        except Exception as e:  # codificador no disponible
            self.skipTest(f"no se pudo crear HEIC: {e}")
        res = self.ejecutar(cw.Job(sources=[ruta], output_dir=self.tmp / "out"))
        self.assertEqual(res.converted, 1)
        self.assertTrue((self.tmp / "out" / "foto.webp").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
