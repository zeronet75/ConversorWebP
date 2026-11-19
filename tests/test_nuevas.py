"""Pruebas de las funciones añadidas en 2.5.0."""

import os
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
import zipfile
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

    def test_heic_conserva_exif(self):
        if not cw.HEIF_DISPONIBLE:
            self.skipTest("pillow-heif no disponible")
        ruta = self.tmp / "meta.heic"
        exif = Image.Exif()
        exif[271] = "HeicCam"
        try:
            Image.new("RGB", (120, 90), (40, 160, 90)).save(
                ruta, format="HEIF", exif=exif
            )
        except Exception as e:
            self.skipTest(f"no se pudo crear HEIC con EXIF: {e}")
        destino = self.tmp / "meta.webp"
        job = cw.Job(sources=[ruta], output_dir=self.tmp, preservar_extra=True)
        cw.convert_one(ruta, destino, job)
        with Image.open(destino) as salida:
            self.assertEqual(salida.getexif().get(271), "HeicCam")


class TestAVIF(Base):
    def test_avif_es_formato_de_entrada(self):
        self.assertIn(".avif", cw.IMAGE_EXTS)

    def test_avif_conserva_exif(self):
        ruta = self.tmp / "meta.avif"
        exif = Image.Exif()
        exif[271] = "AvifCam"
        try:
            Image.new("RGB", (120, 90), (160, 40, 90)).save(ruta, exif=exif)
        except Exception as e:
            self.skipTest(f"no se pudo crear AVIF con EXIF: {e}")
        destino = self.tmp / "meta.webp"
        job = cw.Job(sources=[ruta], output_dir=self.tmp, preservar_extra=True)
        cw.convert_one(ruta, destino, job)
        with Image.open(destino) as salida:
            self.assertEqual(salida.getexif().get(271), "AvifCam")


class TestAnimacion(Base):
    def test_gif_animado_a_webp(self):
        src = self.tmp / "anim.gif"
        frames = [Image.new("RGB", (80, 60), (i * 60, 40, 120)) for i in range(3)]
        frames[0].save(
            src,
            save_all=True,
            append_images=frames[1:],
            duration=[100, 120, 140],
            loop=0,
        )
        dst = self.tmp / "anim.webp"
        job = cw.Job(sources=[src], output_dir=self.tmp, quality=80, method=4)
        _entrada, _salida, aviso = cw.convert_one(src, dst, job)
        self.assertIn("animada", aviso)
        with Image.open(dst) as im:
            self.assertGreater(getattr(im, "n_frames", 1), 1)


class TestMetadatos(Base):
    def test_conservar_exif_y_fecha(self):
        src = self.tmp / "meta.jpg"
        dst = self.tmp / "meta.webp"
        imagen = Image.new("RGB", (64, 64), (10, 100, 200))
        exif = Image.Exif()
        exif[271] = "TestCamera"
        imagen.save(src, exif=exif)
        hora = 1_600_000_000
        os.utime(src, (hora, hora))
        job = cw.Job(sources=[src], output_dir=self.tmp, preservar_extra=True)
        cw.convert_one(src, dst, job)
        with Image.open(dst) as salida:
            self.assertEqual(salida.getexif().get(271), "TestCamera")
        self.assertAlmostEqual(dst.stat().st_mtime, hora, delta=3)


class TestInformeHTML(Base):
    def test_genera_html(self):
        src = self.imagen("html.png", (64, 48))
        job = cw.Job(sources=[src], output_dir=self.tmp / "out", html_report=True)
        res = self.ejecutar(job)
        self.assertIsNotNone(res.html_path)
        self.assertTrue(res.html_path.exists())
        self.assertIn("<table>", res.html_path.read_text(encoding="utf-8"))


class TestRobustez(Base):
    def test_historial_valido(self):
        self.assertFalse(cw._historial_valido({"sources": 5}))
        self.assertFalse(cw._historial_valido({"sources": ["a"], "ajustes": 5}))
        self.assertTrue(
            cw._historial_valido(
                {"sources": ["a"], "output_dir": "x", "ajustes": {}}
            )
        )

    def test_zip_limite(self):
        zip_path = self.tmp / "bomba.zip"
        with zipfile.ZipFile(zip_path, "w") as z:
            z.writestr("a.txt", "A" * 100)
        destino = self.tmp / "destino"
        destino.mkdir()
        anterior = cw.MAX_EXTRACCION_BYTES
        cw.MAX_EXTRACCION_BYTES = 10
        try:
            with self.assertRaises(cw.ErrorExtraccionSegura):
                cw.extract_zip_builtin(zip_path, destino)
        finally:
            cw.MAX_EXTRACCION_BYTES = anterior

    def test_zip_traversal(self):
        zip_path = self.tmp / "traversal.zip"
        with zipfile.ZipFile(zip_path, "w") as z:
            z.writestr("../fuera.txt", "X")
        destino = self.tmp / "destino2"
        destino.mkdir()
        with self.assertRaises(cw.ErrorExtraccionSegura):
            cw.extract_zip_builtin(zip_path, destino)
        self.assertFalse((self.tmp / "fuera.txt").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
