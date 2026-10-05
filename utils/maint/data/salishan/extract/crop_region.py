"""Render one region of a page to a PNG, for reading a table or a mark off the page.

usage: python crop_region.py <stem> <page> <top> <bottom> <out.png> [dpi]

top and bottom are PDF points from the foot of the page, the baselines table_cells.py prints; the
region runs the whole width of the page.
"""
import os
import sys

import pypdfium2 as pdfium

from page_text import CORPUS

stem, page, top, bottom, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
dpi = int(sys.argv[6]) if len(sys.argv) > 6 else 300
document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
one = document[page - 1]
height = one.get_height()
scale = dpi / 72.0
image = one.render(scale=scale).to_pil()
# Optional left and right, in points from the page's left edge, narrow the region.
left = float(sys.argv[7]) * scale if len(sys.argv) > 8 else 0
right = float(sys.argv[8]) * scale if len(sys.argv) > 8 else image.width
image.crop((int(left), int((height - top) * scale), int(right), int((height - bottom) * scale))).save(out)
print(out)
