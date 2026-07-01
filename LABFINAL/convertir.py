import re
import time
from pathlib import Path

from docling.document_converter import DocumentConverter
from docling.datamodel.pipeline_options import PdfPipelineOptions
from docling.document_converter import PdfFormatOption

# ----------------------------
# Configuración
# ----------------------------

PDF_DIR = Path("documentos")
OUTPUT_DIR = Path("salida")
IMAGES_DIR = OUTPUT_DIR / "images_todas"  # carpeta ÚNICA para TODAS las imágenes

print("=" * 50)
print("CONVERSIÓN DE PDFs A MARKDOWN")
print("=" * 50)

print(f"\nCarpeta de entrada : {PDF_DIR.resolve()}")
print(f"Carpeta de salida  : {OUTPUT_DIR.resolve()}")
print(f"Carpeta de imágenes: {IMAGES_DIR.resolve()}")

OUTPUT_DIR.mkdir(exist_ok=True)
IMAGES_DIR.mkdir(parents=True, exist_ok=True)

# El conversor se crea SOLO si hay algo que convertir (cargarlo descarga/inicia
# modelos y tarda). Lo construimos de forma perezosa con esta función.
_converter = None


def obtener_conversor():
    global _converter
    if _converter is None:
        print("\nInicializando el conversor (OCR desactivado)...")
        pipeline_options = PdfPipelineOptions()
        pipeline_options.do_ocr = False  # PDFs con texto seleccionable

        # Necesario para que Docling EXTRAIGA las imágenes/figuras del PDF.
        # Sin esto, save_as_markdown(image_mode="referenced") no escribe imágenes.
        pipeline_options.images_scale = 2.0  # resolución (1.0 = 72 dpi)
        pipeline_options.generate_picture_images = True  # recorta las figuras
        pipeline_options.generate_page_images = True  # imagen de cada página

        _converter = DocumentConverter(
            format_options={
                "pdf": PdfFormatOption(pipeline_options=pipeline_options)
            }
        )
        print("Conversor listo.")
    return _converter


def ya_convertido(pdf):
    """True si el .md ya existe y todas sus imágenes están en disco."""
    md = OUTPUT_DIR / f"{pdf.stem}.md"
    if not md.exists():
        return False
    texto = md.read_text(encoding="utf-8")
    # Rutas tipo: images_todas/<nombre_pdf>__img_NNN.png  (pueden tener espacios)
    refs = re.findall(r"images_todas/([^)]+\.png)", texto)
    for ref in refs:
        if not (IMAGES_DIR / ref).exists():
            return False  # falta una imagen -> hay que regenerar este PDF
    return True  # md presente y todas sus imágenes también

# ----------------------------
# Conversión
# ----------------------------

pdfs = sorted(PDF_DIR.glob("*.pdf"))
total = len(pdfs)

print(f"\nSe encontraron {total} PDF(s).")

if total == 0:
    print(f"\nNo hay archivos .pdf en '{PDF_DIR}'. Nada que convertir.")
    raise SystemExit(0)

print("-" * 50)

exitosos = 0
fallidos = 0
omitidos = 0
inicio_total = time.perf_counter()

for i, pdf in enumerate(pdfs, start=1):

    tamano_mb = pdf.stat().st_size / (1024 * 1024)
    print(f"\n[{i}/{total}] {pdf.name} ({tamano_mb:.2f} MB)")

    # Si ya está el .md y todas sus imágenes, no lo regeneramos ni lo tocamos.
    if ya_convertido(pdf):
        omitidos += 1
        print("   -> Ya convertido (md + imágenes presentes). Se omite.")
        continue

    inicio = time.perf_counter()

    try:
        print("   -> Procesando documento...")
        result = obtener_conversor().convert(pdf)

        md = OUTPUT_DIR / f"{pdf.stem}.md"

        print("   -> Guardando Markdown e imágenes...")
        # Todas las imágenes van a la MISMA carpeta (images_todas).
        # OJO: docling concatena un artifacts_dir RELATIVO con la carpeta del
        # .md. Por eso le pasamos solo el nombre (relativo al .md), no la ruta
        # completa "salida/images_todas" (eso creaba "salida/salida/images_todas").
        result.document.save_as_markdown(
            md,
            image_mode="referenced",
            artifacts_dir=Path(IMAGES_DIR.name)
        )

        # Docling guarda las imágenes como "image_000000_<hash>.png".
        # Como todos los PDFs comparten la misma carpeta, las renombramos a
        # "<nombre_pdf>__img_NNN.png" para que tengan el nombre del PDF y se
        # puedan indexar fácil, y actualizamos las referencias del .md.
        md_text = md.read_text(encoding="utf-8")
        patron = re.compile(r"(images_todas/)image_(\d+)_[0-9a-f]+\.png")
        renombradas = 0

        def _renombrar(m):
            global renombradas
            nombre_orig = m.group(0).split("/", 1)[1]
            indice = m.group(2)
            nuevo = f"{pdf.stem}__img_{indice}.png"
            src = IMAGES_DIR / nombre_orig
            dst = IMAGES_DIR / nuevo
            if src.exists():
                src.rename(dst)
                renombradas += 1
            return m.group(1) + nuevo

        nuevo_texto = patron.sub(_renombrar, md_text)
        md.write_text(nuevo_texto, encoding="utf-8")

        duracion = time.perf_counter() - inicio
        exitosos += 1
        print(f"   OK Guardado en: {md}  ({renombradas} imágenes) (en {duracion:.1f}s)")

    except Exception as e:
        duracion = time.perf_counter() - inicio
        fallidos += 1
        print(f"   ERROR al convertir {pdf.name} (tras {duracion:.1f}s): {e}")

# ----------------------------
# Resumen
# ----------------------------

duracion_total = time.perf_counter() - inicio_total

print("\n" + "=" * 50)
print("RESUMEN")
print("=" * 50)
print(f"Total PDFs       : {total}")
print(f"Convertidos      : {exitosos}")
print(f"Omitidos (listos): {omitidos}")
print(f"Fallidos         : {fallidos}")
print(f"Tiempo total     : {duracion_total:.1f}s")
print(f"Imágenes en      : {IMAGES_DIR}")
print("\n¡Todo listo!")
