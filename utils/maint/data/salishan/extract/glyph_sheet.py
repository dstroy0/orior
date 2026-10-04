"""Draw every glyph of a paper's embedded font on one sheet, each under the code the page sets it at.

usage: python glyph_sheet.py <stem> <font name substring> <out.png> [scale]

A font with no map to Unicode, a Windows TTE subset that numbers its glyphs 1, 2, 3 in the order
the document first used them, gives the text layer control characters. Cropping each code out of a
page render finds one glyph per crop. This reads the font program itself, CFF or TrueType, and draws
all of them at once, each cell labeled with its code, its glyph name and its advance width, for
reading the cipher by eye in one pass. The cell's baseline is the thin rule.
"""
import io
import os
import sys

import pypdf
from fontTools.cffLib import CFFFontSet
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

from page_text import CORPUS

CELL, BASE = 150, 105


def fonts_of(stem, want):
    """Each font dictionary of the paper whose base name holds want, once apiece."""
    reader = pypdf.PdfReader(os.path.join(CORPUS, "papers", stem + ".pdf"))
    seen = {}
    for page in reader.pages:
        resources = page["/Resources"] if "/Resources" in page else {}
        stack = [resources]
        while stack:
            here = stack.pop()
            here = here.get_object() if hasattr(here, "get_object") else here
            for key, ref in (here.get("/Font") or {}).items():
                font = ref.get_object()
                name = str(font.get("/BaseFont", key))
                if want in name and name not in seen:
                    seen[name] = font
            for ref in (here.get("/XObject") or {}).values():
                inner = ref.get_object()
                if "/Resources" in inner:
                    stack.append(inner["/Resources"])
    return seen


def glyphs(font):
    """[(code, glyph name, width in font units, outline commands in font units)]."""
    differences = {}
    encoding = font.get("/Encoding")
    if encoding is not None and hasattr(encoding.get_object(), "get"):
        code = 0
        for item in encoding.get_object().get("/Differences", []):
            if isinstance(item, int):
                code = item
            else:
                differences[code] = str(item).lstrip("/")
                code += 1
    descriptor = font["/FontDescriptor"].get_object()
    if "/FontFile3" in descriptor:
        data = descriptor["/FontFile3"].get_object().get_data()
        cff = CFFFontSet()
        cff.decompile(io.BytesIO(data), None)
        charstrings = cff[cff.fontNames[0]].CharStrings
        scale = 1.0
        getter = lambda name: charstrings[name]
        names = set(charstrings.keys())
    else:
        data = descriptor["/FontFile2"].get_object().get_data()
        program = TTFont(io.BytesIO(data))
        glyphset = program.getGlyphSet()
        scale = 1000.0 / program["head"].unitsPerEm
        getter = lambda name: glyphset[name]
        names = set(glyphset.keys())
    found = []
    for code in sorted(differences):
        name = differences[code]
        if name not in names:
            continue
        pen = DecomposingRecordingPen(None)
        glyph = getter(name)
        glyph.draw(pen)
        width = getattr(glyph, "width", 0) or 0
        found.append((code, name, width * scale, [(op, [(x * scale, y * scale) for x, y in points])
                                                   for op, points in pen.value]))
    return found


def draw(cells, out, zoom):
    columns = 10
    rows = (len(cells) + columns - 1) // columns
    sheet = Image.new("L", (columns * CELL, rows * CELL), 255)
    ink = ImageDraw.Draw(sheet)
    label = ImageFont.load_default()
    for index, (title, code, name, width, outline) in enumerate(cells):
        left, top = (index % columns) * CELL, (index // columns) * CELL
        ink.rectangle([left, top, left + CELL - 1, top + CELL - 1], outline=200)
        ink.line([left + 4, top + BASE, left + CELL - 5, top + BASE], fill=220)
        ink.text((left + 3, top + 2), "%s %d %s" % (title, code, name), fill=0, font=label)
        ink.text((left + 3, top + CELL - 12), "w%d" % width, fill=90, font=label)
        polygon = []

        def place(point):
            return (left + 30 + point[0] * zoom, top + BASE - point[1] * zoom)

        contours = []
        current = []
        for op, points in outline:
            if op == "moveTo":
                if current:
                    contours.append(current)
                current = [place(points[0])]
            elif op == "lineTo":
                current.append(place(points[0]))
            elif op in ("curveTo", "qCurveTo"):
                start = current[-1] if current else place(points[0])
                ends = [place(one) for one in points]
                steps = 8
                control = [start] + ends
                for step in range(1, steps + 1):
                    t = step / steps
                    level = control
                    while len(level) > 1:
                        level = [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
                                 for a, b in zip(level, level[1:])]
                    current.append(level[0])
            elif op in ("closePath", "endPath"):
                if current:
                    contours.append(current)
                current = []
        if current:
            contours.append(current)
        # Even-odd fill through a mask keeps counters open, the bowl of a and the eye of e.
        mask = Image.new("1", sheet.size, 0)
        painter = ImageDraw.Draw(mask)
        for contour in contours:
            if len(contour) > 2:
                layer = Image.new("1", sheet.size, 0)
                ImageDraw.Draw(layer).polygon(contour, fill=1)
                mask = Image.frombytes("1", sheet.size, bytes(a ^ b for a, b in zip(mask.tobytes(), layer.tobytes())))
        sheet.paste(0, mask=mask)
        del polygon, painter
    sheet.save(out)


def main():
    stem, want, out = sys.argv[1], sys.argv[2], sys.argv[3]
    zoom = float(sys.argv[4]) if len(sys.argv) > 4 else 0.09
    cells = []
    for name, font in fonts_of(stem, want).items():
        short = name.split("+")[-1][-8:]
        for code, glyph, width, outline in glyphs(font):
            cells.append((short, code, glyph, width, outline))
    draw(cells, out, zoom)
    print("%d glyphs drawn to %s" % (len(cells), out))


if __name__ == "__main__":
    main()
