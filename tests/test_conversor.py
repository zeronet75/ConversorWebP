"""Pruebas automáticas del motor de Conversor WebP.

No necesitan interfaz gráfica ni archivos externos: generan sus propias
imágenes en una carpeta temporal.

Ejecutar desde la raíz del repositorio:

    python -m unittest discover -s tests -v
"""

import shutil
import sys
import tempfile
import threading
import unittest
from pathlib import Path

from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ))

import conversor_webp as cw  # noqa: E402


def ejecutar(job):
    """Ejecuta un Job en el hilo actual y devuelve el Result."""
    resultado = {}

    def emit(tipo, dato):
        if tipo == "done":
            resultado["r"] = dato

    cw.run_job(job, emit, threading.Event())
    return resultado["r"]


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="test_conversor_"))
        self._config_original = cw.CONFIG_FILE
        cw.CONFIG_FILE = self.tmp / "config.json"

    def tearDown(self):
        cw.CONFIG_FILE = self._config_original
        shutil.rmtree(self.tmp, ignore_errors=True)

    def imagen(self, nombre="foto.jpg", tamano=(320, 240), color=(30, 120, 200), **kwargs):
        ruta = self.tmp / nombre
        ruta.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGB", tamano, color).save(ruta, **kwargs)
        return ruta


class TestUtilidades(unittest.TestCase):
    def test_hex_valido(self):
        self.assertEqual(cw.hex_to_rgb("#fff"), (255, 255, 255))
        self.assertEqual(cw.hex_to_rgb("#1a73e8"), (26, 115, 232))
        self.assertEqual(cw.hex_to_rgb("1A73E8"), (26, 115, 232))

    def test_hex_invalido(self):
        for valor in ("", "#ff", "rojo", "#gggggg"):
            with self.assertRaises(ValueError):
                cw.hex_to_rgb(valor)

    def test_nombre_seguro(self):
        self.assertEqual(cw.nombre_seguro("fotos.zip", "x.zip")[0], "fotos.zip")
        self.assertEqual(cw.nombre_seguro(r"..\evil.zip", "x.zip")[0], "evil.zip")
        self.assertEqual(cw.nombre_seguro("C:\\ruta\\a.zip", "x.zip")[0], "a.zip")
        self.assertEqual(cw.nombre_seguro('a<b>c:"d".zip', "x.zip")[0], "a_b_c__d_.zip")
        self.assertEqual(cw.nombre_seguro("CON.zip", "x.zip")[0], "_CON.zip")
        self.assertTrue(cw.nombre_seguro(r"..\evil.zip", "x.zip")[1])


class TestConversion(Base):
    def test_webp_basico(self):
        src = self.imagen()
        res = ejecutar(cw.Job(sources=[src], output_dir=self.tmp / "out", quality=80))
        salida = self.tmp / "out" / "foto.webp"
        self.assertEqual(res.converted, 1)
        self.assertEqual(res.failed, [])
        self.assertTrue(salida.exists())
        with Image.open(salida) as im:
            self.assertEqual(im.format, "WEBP")
            self.assertEqual(im.size, (320, 240))

    def test_alfa_conservado(self):
        ruta = self.tmp / "alfa.png"
        Image.new("RGBA", (80, 60), (255, 0, 0, 90)).save(ruta)
        ejecutar(cw.Job(sources=[ruta], output_dir=self.tmp / "out"))
        with Image.open(self.tmp / "out" / "alfa.webp") as im:
            self.assertIn("A", im.getbands())
            self.assertLess(im.getpixel((40, 30))[3], 255)

    def test_redimensionado_no_amplia(self):
        grande = self.imagen("grande.jpg", (1200, 800))
        pequena = self.imagen("pequena.jpg", (300, 200))
        ejecutar(
            cw.Job(sources=[grande, pequena], output_dir=self.tmp / "out", max_dimension=500)
        )
        with Image.open(self.tmp / "out" / "grande.webp") as im:
            self.assertLessEqual(max(im.size), 500)
        with Image.open(self.tmp / "out" / "pequena.webp") as im:
            self.assertEqual(im.size, (300, 200))

    def test_peso_objetivo(self):
        ruta = self.tmp / "ruido.jpg"
        Image.effect_noise((700, 500), 90).convert("RGB").save(ruta, quality=95)
        res = ejecutar(
            cw.Job(sources=[ruta], output_dir=self.tmp / "out", quality=95, target_kb=80)
        )
        salida = self.tmp / "out" / "ruido.webp"
        self.assertEqual(res.converted, 1)
        self.assertLessEqual(salida.stat().st_size, 80 * 1024)

    def test_avif(self):
        src = self.imagen()
        ejecutar(cw.Job(sources=[src], output_dir=self.tmp / "out", formato="AVIF", quality=60))
        salida = self.tmp / "out" / "foto.avif"
        self.assertTrue(salida.exists())
        with Image.open(salida) as im:
            self.assertEqual(im.format, "AVIF")

    def test_zip_con_nombre_peligroso(self):
        src = self.imagen()
        res = ejecutar(
            cw.Job(sources=[src], output_dir=self.tmp / "out", make_zip=True,
                   zip_name=r"..\evil.zip")
        )
        self.assertTrue((self.tmp / "out" / "evil.zip").exists())
        self.assertFalse((self.tmp / "evil.zip").exists())
        self.assertIsNotNone(res.zip_path)

    def test_omitir_existentes(self):
        src = self.imagen()
        out = self.tmp / "out"
        ejecutar(cw.Job(sources=[src], output_dir=out))
        res = ejecutar(cw.Job(sources=[src], output_dir=out, overwrite=False))
        self.assertEqual(res.converted, 0)
        self.assertEqual(res.skipped, 1)

    def test_cmyk_sin_perfil(self):
        ruta = self.tmp / "cmyk.jpg"
        Image.new("CMYK", (120, 90), (0, 100, 100, 0)).save(ruta, quality=90)
        ejecutar(cw.Job(sources=[ruta], output_dir=self.tmp / "out"))
        with Image.open(self.tmp / "out" / "cmyk.webp") as im:
            self.assertEqual(im.mode, "RGB")
            self.assertFalse(im.info.get("icc_profile"))

    def test_varios_comprimidos(self):
        import zipfile

        for nombre in ("uno", "dos"):
            carpeta = self.tmp / nombre
            carpeta.mkdir()
            Image.new("RGB", (60, 40), (1, 2, 3)).save(carpeta / "foto.jpg")
            with zipfile.ZipFile(self.tmp / f"{nombre}.zip", "w") as z:
                z.write(carpeta / "foto.jpg", "foto.jpg")
        res = ejecutar(
            cw.Job(
                sources=[self.tmp / "uno.zip", self.tmp / "dos.zip"],
                output_dir=self.tmp / "out",
            )
        )
        self.assertEqual(res.converted, 2)
        self.assertTrue((self.tmp / "out" / "uno" / "foto.webp").exists())
        self.assertTrue((self.tmp / "out" / "dos" / "foto.webp").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
