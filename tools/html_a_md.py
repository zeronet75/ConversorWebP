"""Regenera docs/MANUAL.md a partir de MANUAL_DE_USO.html.

Uso (opcional, solo para mantenimiento de la documentación):

    pip install pypandoc-binary
    python tools/html_a_md.py

El HTML es la fuente; este script produce la versión Markdown para GitHub.
"""

import re
from pathlib import Path

import pypandoc

src = Path(r"D:\ConversorWebP\MANUAL_DE_USO.html")
out = Path(r"D:\ConversorWebP\docs\MANUAL.md")

md = pypandoc.convert_file(
    str(src), "gfm", format="html",
    extra_args=["--wrap=none"],
)
md = md.replace("\r\n", "\n").replace("\r", "\n")

# Quita el indice manual del HTML
inicio = md.find("**Contenido**")
fin = md.find("**Novedades")
if inicio != -1 and fin != -1 and fin > inicio:
    md = md[:inicio] + md[fin:]

# Convierte tablas HTML que pandoc no supo interpretar (llevan colgroup)
_MAP = {
    "<strong>": "**", "</strong>": "**", "<b>": "**", "</b>": "**",
    "<em>": "*", "</em>": "*", "<i>": "*", "</i>": "*",
    "<code>": "`", "</code>": "`",
}


def _celda(texto: str) -> str:
    texto = re.sub(r"<br\s*/?>", "<br>", texto)
    for etiqueta, marca in _MAP.items():
        texto = texto.replace(etiqueta, marca)
    texto = re.sub(r"<[^>]+>", "", texto)
    return re.sub(r"\s+", " ", texto).strip()


def _tabla(match: re.Match) -> str:
    filas = re.findall(r"<tr>(.*?)</tr>", match.group(0), flags=re.S)
    lineas = []
    for i, fila in enumerate(filas):
        celdas = [_celda(c) for c in re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", fila, flags=re.S)]
        lineas.append("| " + " | ".join(celdas) + " |")
        if i == 0:
            lineas.append("|" + "|".join(["---"] * len(celdas)) + "|")
    return "\n".join(lineas)


md = re.sub(r"<table>.*?</table>", _tabla, md, flags=re.S)

# Limpieza de restos HTML
md = re.sub(r"</?div[^>]*>", "", md)
md = re.sub(r'<span class="kbd">(.*?)</span>', r"`\1`", md)
md = re.sub(r"</?span[^>]*>", "", md)
md = re.sub(r"<p[^>]*>\s*", "", md)
md = re.sub(r"</p>", "", md)
md = re.sub(r"<br\s*/?>", "<br>", md)

# Quita el H1 y subtitulo originales (los pone la cabecera)
md = re.sub(r"^# Conversor WebP 2\.3\s*$", "", md, flags=re.M)
md = re.sub(r"^Manual de uso · .*$\n?", "", md, flags=re.M)

# Normaliza espacios en blanco
md = re.sub(r"[ \t]+\n", "\n", md)
md = re.sub(r"\n{3,}", "\n\n", md)
md = md.strip() + "\n"

# Indice generado a partir de los H2
def slug(texto: str) -> str:
    t = texto.lower()
    t = re.sub(r"[^\w\s-]", "", t, flags=re.UNICODE)
    return t.strip().replace(" ", "-")

secciones = re.findall(r"^## (.+)$", md, flags=re.M)
toc = "\n".join(f"- [{s}](#{slug(s)})" for s in secciones)

cabecera = (
    "# Conversor WebP 2.3 · Manual de uso\n\n"
    "Manual completo del programa: qué hace cada opción y cómo sacarle partido.\n\n"
    "> Versión navegable del manual incluido en el repositorio "
    "([`MANUAL_DE_USO.html`](../MANUAL_DE_USO.html)).\n\n"
    "## Índice\n\n"
    f"{toc}\n\n"
    "---\n\n"
)

out.write_text(cabecera + md, encoding="utf-8", newline="\n")
print("bytes:", out.stat().st_size, "| lineas:", (cabecera + md).count("\n"))
print("secciones:", len(secciones))
