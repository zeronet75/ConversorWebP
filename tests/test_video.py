"""Pruebas de conversión de vídeo a WebM (requiere FFmpeg)."""

import os
import shutil
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from PIL import Image

import conversor_webp as cw


FFMPEG = cw.find_ffmpeg()
FFPROBE = cw.find_ffprobe()
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


@unittest.skipUnless(FFMPEG and FFPROBE, "FFmpeg/ffprobe no disponibles")
class TestVideo(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="test_video_"))
        self.video = self.tmp / "entrada.mp4"
        subprocess.run(
            [
                str(FFMPEG), "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
                "-f", "lavfi", "-i", "testsrc=size=160x120:rate=10",
                "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
                "-t", "1", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                "-c:a", "aac", str(self.video),
            ],
            check=True,
            timeout=120,
            creationflags=FLAGS,
        )

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _run(self, job):
        resultado = {}

        def emit(tipo, dato):
            if tipo == "done":
                resultado["r"] = dato

        cw.run_job(job, emit, threading.Event())
        return resultado["r"]

    def _codecs(self, ruta: Path) -> list[str]:
        proc = subprocess.run(
            [
                str(FFPROBE), "-v", "error", "-show_entries", "stream=codec_name",
                "-of", "csv=p=0", str(ruta),
            ],
            capture_output=True,
            text=True,
            timeout=60,
            creationflags=FLAGS,
        )
        return [x for x in proc.stdout.replace("\r", "").split("\n") if x]

    def test_video_a_webm(self):
        salida = self.tmp / "out"
        res = self._run(cw.Job(sources=[self.video], output_dir=salida))
        self.assertEqual(res.converted, 1)
        self.assertFalse(res.failed)
        destino = salida / "entrada.webm"
        self.assertTrue(destino.exists())
        codecs = self._codecs(destino)
        self.assertIn("vp9", codecs)
        self.assertIn("opus", codecs)

    def test_lote_mixto(self):
        foto = self.tmp / "foto.jpg"
        Image.new("RGB", (320, 240), (20, 120, 200)).save(foto)
        salida = self.tmp / "out"
        res = self._run(cw.Job(sources=[foto, self.video], output_dir=salida))
        self.assertEqual(res.converted, 2)
        self.assertTrue((salida / "foto.webp").exists())
        self.assertTrue((salida / "entrada.webm").exists())

    def test_omite_webm_si_no_se_recodifica(self):
        salida = self.tmp / "out"
        salida.mkdir()
        (salida / "entrada.webm").write_bytes(b"no es un webm real")
        res = self._run(
            cw.Job(
                sources=[self.video],
                output_dir=salida,
                overwrite=False,
                reencode_webp=False,
            )
        )
        self.assertEqual(res.converted, 0)
        self.assertEqual(res.skipped, 1)
        self.assertEqual((salida / "entrada.webm").read_bytes(), b"no es un webm real")

    def test_dry_run_no_escribe(self):
        salida = self.tmp / "out"
        res = self._run(
            cw.Job(sources=[self.video], output_dir=salida, dry_run=True)
        )
        self.assertEqual(res.converted, 1)
        self.assertFalse(salida.exists() and any(salida.iterdir()))
