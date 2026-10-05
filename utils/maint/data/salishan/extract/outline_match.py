"""Name by raster the glyphs of a paper's stripped TrueType subsets that no outline key names.

usage: python outline_match.py <stem> [--write]

page_text.outlined_fonts names a glyph of a CID TrueType subset with no ToUnicode by its outline
key, exact to the point, against the system face OUTLINE_FACES gives its font. A subset drawn from
another release of the face misses that key by a point or two. Each such glyph is drawn at 120 px to
the em, as glyph_match.py draws one, and compared with every glyph of the system face whose advance
is within 1.5% of its own. Letters drawn alike, Latin p and Cyrillic р, tie; the tie goes to the one
Windows-1252 holds, the text Word exports, and then to glyph_match.rank's order. A glyph is named
where its best overlap is MATCH or more and the best other letter, ties aside, falls short of it by
MARGIN. Prints each key with its letter, overlap, runner-up and count; with --write, the named ones
go to outline_letters.tsv, the rest to be read by eye off glyph_sheet.py's drawing.
"""
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
import pypdf  # noqa: E402
from fontTools.ttLib import TTFont  # noqa: E402

import glyph_match  # noqa: E402
import page_text  # noqa: E402
from workdir import CORPUS, PRIVATE  # noqa: E402

MATCH, MARGIN, TIE = 0.95, 0.05, 0.002
# A face that only clones the font's shapes, TeX Gyre Termes for Adobe's Times-Roman, draws each
# letter a little apart from the subset's: its best overlap runs 0.92 to 0.99 where the system face's
# own copy gives 0.99, and CLONE_MATCH takes its place.
CLONE_MATCH = 0.90


def windows(text):
    try:
        text.encode("cp1252")
        return 0
    except UnicodeEncodeError:
        return 1


def fonts_under(resources):
    resources = resources.get_object() if resources is not None else {}
    for ref in (resources.get("/Font") or {}).values():
        yield ref.get_object()
    for ref in (resources.get("/XObject") or {}).values():
        inner = ref.get_object()
        if inner.get("/Subtype") == "/Form" and "/Resources" in inner:
            yield from fonts_under(inner["/Resources"])


def main():
    stem = sys.argv[1]
    reader = pypdf.PdfReader(os.path.join(CORPUS, "papers", stem + ".pdf"))
    letters = page_text.outline_letters()
    references, found, seen = {}, {}, set()
    for page in reader.pages:
        for font in fonts_under(page.get("/Resources")):
            if id(font) in seen or font.get("/Subtype") != "/Type0" or "/ToUnicode" in font:
                continue
            seen.add(id(font))
            program = font["/DescendantFonts"][0].get_object()["/FontDescriptor"].get_object().get("/FontFile2")
            if program is None:
                continue
            name = re.sub(r"^[A-Z]{6}\+|\*\d+$", "", str(font.get("/BaseFont", "")).lstrip("/"))
            face = page_text.OUTLINE_FACES.get(name)
            system = page_text.face_keys(face) if face else {}
            if face and face not in references:
                references[face] = [(advance, text, glyph_match.raster(glyph, glyphset, units))
                                    for advance, text, _, glyph, glyphset, units in glyph_match.reference(face)]
            tt = TTFont(io.BytesIO(program.get_object().get_data()))
            glyphset, units = tt.getGlyphSet(), tt["head"].unitsPerEm
            for gid, glyph in enumerate(tt.getGlyphOrder()):
                if gid == 0:
                    continue
                key = page_text.outline_key(glyphset, glyph, tt["hmtx"][glyph][0] * 2048 // units)
                if key in letters or key in system:
                    continue
                if key in found:
                    found[key][-1] += 1
                    continue
                image = glyph_match.raster(glyphset[glyph], glyphset, units)
                if image.getbbox() is None:
                    continue
                advance = tt["hmtx"][glyph][0] * 1000 / units
                scored = sorted(((glyph_match.overlap(image, bitmap), text)
                                 for width, text, bitmap in references.get(face, [])
                                 if abs(width - advance) <= max(15, advance * 0.015)), reverse=True)
                if not scored:
                    found[key] = [name, None, 0.0, None, 0.0, 1]
                    continue
                tied = [text for score, text in scored if score >= scored[0][0] - TIE]
                best = min(tied, key=lambda text: (windows(text), glyph_match.rank(text)))
                runner = next(((score, text) for score, text in scored if text not in tied), (0.0, None))
                found[key] = [name, best, round(scored[0][0], 3), runner[1], round(runner[0], 3), 1]
    named = []
    for key, (name, text, score, runner, other, count) in sorted(found.items(), key=lambda item: -item[1][-1]):
        floor = CLONE_MATCH if page_text.OUTLINE_FACES.get(name, "").startswith(page_text.TERMES) else MATCH
        good = text is not None and score >= floor and score - other >= MARGIN
        print("%s %-26s %-3s %.3f %-3s %.3f %3d%s" % (key, name, text, score, runner, other, count,
                                                       "" if good else "   left for the eye"))
        if good:
            named.append("%s\t%s\t%s\t%s\toutline_match.py: raster overlap %.3f against %s, the next letter %.3f\n"
                         % (key, text, name, stem, score, os.path.basename(page_text.OUTLINE_FACES[name]), other))
    if "--write" in sys.argv[2:] and named:
        with open(os.path.join(PRIVATE, "outline_letters.tsv"), "a", encoding="utf-8", newline="\n") as handle:
            handle.writelines(named)
        print("%d written to outline_letters.tsv" % len(named))


if __name__ == "__main__":
    main()
