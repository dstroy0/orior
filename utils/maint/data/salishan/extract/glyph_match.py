"""Name the glyphs of a paper's simple CFF fonts by matching each outline against the system face the
font was subset from.

usage: python -B glyph_match.py stem [font_substring]

Word's PDF export can give a subset font two copies of a letter: one under its name, which the
ToUnicode maps, and one under /gN, which it does not, and pdfium reads that code as a control
character. Each glyph is drawn at 120 px to the em about its own origin and compared with every glyph
of the system face whose advance is within 1.5% of its own. The glyphs the ToUnicode already maps
are matched the same way, as the check on the method: every one of them must come back as its own
letter. Prints, per font, the mapped codes that disagree, then each unmapped code with its best
letter, the overlap and the runner-up, and last a PAPER_TOUNICODE entry of the unmapped codes.
"""
import io
import os
import re
import sys

import pypdf
from fontTools.cffLib import CFFFontSet
from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from PIL import Image, ImageChops, ImageDraw

sys.stdout.reconfigure(encoding="utf-8")
from workdir import CORPUS  # noqa: E402
FONTS = "C:/Windows/Fonts"
REFERENCE = {"TimesNewRomanPSMT": "times.ttf", "TimesNewRomanPS-BoldMT": "timesbd.ttf",
             "TimesNewRomanPS-ItalicMT": "timesi.ttf", "TimesNewRomanPS-BoldItalicMT": "timesbi.ttf",
             "Arial": "arial.ttf", "DejaVuSerif": "DejaVuSerif.ttf", "DejaVuSerif-Bold": "DejaVuSerif-Bold.ttf",
             "DejaVuSerif-Italic": "DejaVuSerif-Italic.ttf",
             "DejaVuSerif-BoldItalic": "DejaVuSerif-BoldItalic.ttf", "DejaVuSans": "DejaVuSans.ttf"}
EM = 120
SIZE = (3 * EM, 2 * EM)
BASE = int(1.4 * EM)
LEFT = EM


class FlattenPen(BasePen):
    """Collects each contour as a list of points, curves cut into straight steps."""

    def __init__(self, glyphset, scale):
        super().__init__(glyphset)
        self.scale, self.contours, self.current = scale, [], []

    def point(self, xy):
        return (LEFT + xy[0] * self.scale, BASE - xy[1] * self.scale)

    def _moveTo(self, pt):
        self.current = [self.point(pt)]

    def _lineTo(self, pt):
        self.current.append(self.point(pt))

    def _curveToOne(self, pt1, pt2, pt3):
        start = self._getCurrentPoint()
        for step in range(1, 9):
            t = step / 8
            x = ((1 - t) ** 3 * start[0] + 3 * (1 - t) ** 2 * t * pt1[0] + 3 * (1 - t) * t * t * pt2[0]
                 + t ** 3 * pt3[0])
            y = ((1 - t) ** 3 * start[1] + 3 * (1 - t) ** 2 * t * pt1[1] + 3 * (1 - t) * t * t * pt2[1]
                 + t ** 3 * pt3[1])
            self.current.append(self.point((x, y)))

    def _qCurveToOne(self, pt1, pt2):
        start = self._getCurrentPoint()
        for step in range(1, 9):
            t = step / 8
            x = (1 - t) ** 2 * start[0] + 2 * (1 - t) * t * pt1[0] + t * t * pt2[0]
            y = (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * pt1[1] + t * t * pt2[1]
            self.current.append(self.point((x, y)))

    def _closePath(self):
        if len(self.current) > 2:
            self.contours.append(self.current)
        self.current = []

    _endPath = _closePath


def raster(glyph, glyphset, units):
    """The glyph drawn even-odd into a bitmap, its origin at (LEFT, BASE)."""
    pen = FlattenPen(glyphset, EM / units)
    glyph.draw(pen)
    image = Image.new("1", SIZE, 0)
    for contour in pen.contours:
        layer = Image.new("1", SIZE, 0)
        ImageDraw.Draw(layer).polygon(contour, fill=1)
        image = ImageChops.logical_xor(image, layer)
    return image


def rank(text):
    """Where a letter stands among outlines drawn alike: Latin and IPA before Greek, Greek before
    Cyrillic and the letterlike numerals, a plain space and hyphen before their variants."""
    point = ord(text[0])
    blocks = ((0x20, 0x7F), (0xA0, 0x250), (0x250, 0x370), (0x1E00, 0x1F00), (0x370, 0x400))
    for order, (low, high) in enumerate(blocks):
        if low <= point < high:
            return (order, point)
    return (len(blocks), point)


def overlap(one, two):
    """Intersection over union of two bitmaps, 1.0 for two empty ones."""
    both = ImageChops.logical_and(one, two).histogram()[-1]
    either = ImageChops.logical_or(one, two).histogram()[-1]
    return both / either if either else 1.0


def reference(face):
    """[(advance in thousandths, text, bitmap)] for every mapped glyph of a system face."""
    path, _, number = face.partition("#")
    font = TTFont(os.path.join(FONTS, path), fontNumber=int(number) if number else -1)
    units = font["head"].unitsPerEm
    glyphset = font.getGlyphSet()
    names = {}
    for point, name in font.getBestCmap().items():
        names.setdefault(name, []).append(point)
    table = []
    for name, points in names.items():
        # A glyph mapped from several points takes the lowest past the private use area.
        points = sorted(points, key=lambda point: (0xE000 <= point <= 0xF8FF, point))
        table.append((font["hmtx"][name][0] * 1000 / units, chr(points[0]), name, glyphset[name], glyphset,
                      units))
    return table


def fonts_in(resources):
    """Each font under resources, form XObjects' own included."""
    stack = [resources]
    while stack:
        here = stack.pop()
        here = here.get_object() if hasattr(here, "get_object") else here
        for ref in (here.get("/Font") or {}).values():
            yield ref
        for ref in (here.get("/XObject") or {}).values():
            thing = ref.get_object()
            if thing.get("/Subtype") == "/Form" and "/Resources" in thing:
                stack.append(thing["/Resources"])


def tounicode(font):
    """{code: text} from a simple font's ToUnicode."""
    if "/ToUnicode" not in font:
        return {}
    data = font["/ToUnicode"].get_object().get_data().decode("latin-1")
    entries = {}
    for block in re.findall(r"beginbfchar(.*?)endbfchar", data, re.S):
        for source, target in re.findall(r"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]*)>", block):
            entries[int(source, 16)] = bytes.fromhex(target).decode("utf-16-be")
    for block in re.findall(r"beginbfrange(.*?)endbfrange", data, re.S):
        for low, high, target in re.findall(r"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", block):
            for step, code in enumerate(range(int(low, 16), int(high, 16) + 1)):
                entries[code] = chr(int(target, 16) + step)
    return entries


def main():
    stem = sys.argv[1]
    want = sys.argv[2] if len(sys.argv) > 2 else ""
    reader = pypdf.PdfReader("%s/papers/%s.pdf" % (CORPUS, stem))
    seen, cache, proposals, bitmaps = set(), {}, {}, {}
    for page in reader.pages:
        for ref in fonts_in(page["/Resources"]):
            if ref.idnum in seen:
                continue
            seen.add(ref.idnum)
            font = ref.get_object()
            name = str(font["/BaseFont"]).lstrip("/").split("+", 1)[-1]
            if want not in name or name not in REFERENCE:
                continue
            if REFERENCE[name] not in cache:
                cache[REFERENCE[name]] = reference(REFERENCE[name])
            table = cache[REFERENCE[name]]
            program = font["/FontDescriptor"].get_object()["/FontFile3"].get_object().get_data()
            cff = CFFFontSet()
            cff.decompile(io.BytesIO(program), None)
            top = cff[cff.fontNames[0]]
            charstrings = top.CharStrings
            units = 1000
            codes, code = {}, 0
            for item in font["/Encoding"].get_object().get("/Differences", []):
                if isinstance(item, int):
                    code = item
                else:
                    codes[code] = str(item).lstrip("/")
                    code += 1
            first = font.get("/FirstChar", 0)
            widths = list(font.get("/Widths", []))
            mapped = tounicode(font)
            # The font's own mapped glyphs come first: Word's /gN copy of a letter draws the same
            # outline as the letter under its name.
            own = []
            for code, glyph in codes.items():
                text = mapped.get(code)
                if text and text.strip() and glyph in charstrings:
                    own.append((text, glyph, raster(charstrings[glyph], charstrings, units)))
            wrong, unmapped = [], []
            for code, glyph in sorted(codes.items()):
                if glyph not in charstrings or glyph == "space":
                    continue
                width = widths[code - first] if 0 <= code - first < len(widths) else None
                bitmap = raster(charstrings[glyph], charstrings, units)
                scored = []
                for entry in table:
                    if width is not None and abs(entry[0] - width) > 5:
                        continue
                    key = (REFERENCE[name], entry[2])
                    if key not in bitmaps:
                        bitmaps[key] = raster(entry[3], entry[4], entry[5])
                    scored.append((overlap(bitmap, bitmaps[key]), entry[1], entry[2]))
                scored.sort(reverse=True)
                # Outlines drawn alike, o and Cyrillic о, tie; the tie goes by rank.
                if scored:
                    top = scored[0][0]
                    tied = sorted((one for one in scored if one[0] >= top - 0.001), key=lambda one: rank(one[1]))
                    scored = tied[:1] + [one for one in scored if one is not tied[0]]
                scored = scored[:12]
                best = scored[0] if scored else (0, "?", "?")
                runner = next((one for one in scored[1:] if one[0] < best[0] - 0.001), (0, "", ""))
                given = mapped.get(code)
                if not (given and given.strip()):
                    twins = sorted(((overlap(bitmap, image), text, "own " + other) for text, other, image in own
                                    if other != glyph), reverse=True)
                    if twins and twins[0][0] >= 0.995:
                        tied = [one for one in twins if one[0] >= twins[0][0] - 0.001]
                        tied.sort(key=lambda one: rank(one[1]))
                        texts = "/".join(sorted({one[1] for one in tied}))
                        best, runner = (tied[0][0], tied[0][1], "own " + texts), best
                if given and given.strip():
                    if given != best[1]:
                        wrong.append("  %02X %-12s given %s (U+%04X) best %s %.3f %s" % (
                            code, glyph, given, ord(given[0]), best[1], best[0], best[2]))
                else:
                    unmapped.append((code, glyph, best, runner))
            print("== %s: %d codes, %d mapped disagree, %d unmapped" % (name, len(codes), len(wrong), len(unmapped)))
            print("\n".join(wrong))
            for code, glyph, best, runner in unmapped:
                print("  %02X %-8s -> %s U+%04X %-14s %.3f   next %s %.3f" % (
                    code, glyph, best[1], ord(best[1][0]), best[2], best[0], runner[1], runner[0]))
                proposals.setdefault(name, {})[code] = best[1]
    for name, found in proposals.items():
        print('"%s": {%s},' % (name, ", ".join("0x%02X: %r" % item for item in sorted(found.items()))))


if __name__ == "__main__":
    main()
