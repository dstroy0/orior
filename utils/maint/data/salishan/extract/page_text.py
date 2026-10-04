"""Write a paper's text from the glyph positions on its pages, for a text layer that lost its spaces.

usage: python page_text.py <stem>

pypdfium2 builds each line from the characters' boxes and puts a space where the page leaves a gap,
which recovers the word breaks a text layer glued together (betweenallomorphsissensitiveto:). The
result is written to pagetext/<stem>.txt with the same ===== page N ===== markers, where
residue.source_dir reads it in place of the text layer. Prints how many tokens the two texts share.
"""
import ctypes
import hashlib
import math
import os
import re
import unicodedata
import sys

import io

import pypdf
import pypdfium2 as pdfium
import pypdfium2.raw as pdfium_c
from fontTools.ttLib import TTFont
from pypdf.generic import DecodedStreamObject, NameObject

import defects

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import CORPUS  # noqa: E402
from workdir import PRIVATE  # noqa: E402
from marks import ACUTE, COMMA_ABOVE, DOT_BELOW  # noqa: E402
import tables  # noqa: E402
# The faces a font named but not embedded draws from, Times New Roman in Nater's 2019 paper.
WINDOWS_FONTS = "C:/Windows/Fonts"
SYSTEM_FONTS = {"Times New Roman": "times.ttf", "Times New Roman,Italic": "timesi.ttf",
                "Times New Roman,Bold": "timesbd.ttf", "Times New Roman,BoldItalic": "timesbi.ttf"}
# System faces that hold the substitution and size tables a subset drops, with the face's index in
# its collection: the subscript 𝑒, 𝑡 and ST of McKay's 2019 formulas, and her tall ⟦ ⟧, are glyphs
# the subset's cmap does not reach.
VARIANT_FONTS = {"Cambria Math": ("cambria.ttc", 1)}
_VARIANTS = {}


def variant_letters(name, letters):
    """{glyph id: [code point]} for each glyph of the system face name that its cmap does not reach
    and a GSUB single or alternate substitution, or a MATH size variant, makes from one glyph it
    does: that glyph's code point. Empty where the paper's program, whose cmap is letters, does not
    number its glyphs as the system face does."""
    face, index = VARIANT_FONTS[name]
    if name not in _VARIANTS:
        program = TTFont(os.path.join(WINDOWS_FONTS, face), fontNumber=index)
        order = program.getGlyphOrder()
        cmap = program.getBestCmap()
        points = {glyph: point for point, glyph in cmap.items()}
        reached = {}
        for lookup in program["GSUB"].table.LookupList.Lookup:
            for sub in lookup.SubTable:
                table = sub.ExtSubTable if hasattr(sub, "ExtSubTable") else sub
                pairs = [(source, target) for source, target in (getattr(table, "mapping", None) or {}).items()
                         if isinstance(target, str)]
                pairs += [(source, target) for source, targets in (getattr(table, "alternates", None) or {}).items()
                          for target in targets]
                for source, target in pairs:
                    reached.setdefault(target, set()).add(source)
        variants = program["MATH"].table.MathVariants if "MATH" in program else None
        for coverage, built in ((variants.VertGlyphCoverage, variants.VertGlyphConstruction),
                                (variants.HorizGlyphCoverage, variants.HorizGlyphConstruction)) if variants else ():
            for base, construction in zip(coverage.glyphs if coverage else [], built or []):
                for record in construction.MathGlyphVariantRecord or []:
                    reached.setdefault(record.VariantGlyph, set()).add(base)
        ids = {glyph: gid for gid, glyph in enumerate(order)}
        _VARIANTS[name] = ({point: ids[glyph] for point, glyph in cmap.items()},
                           {ids[glyph]: [points[one] for one in sources]
                            for glyph, sources in reached.items()
                            if glyph not in points and len({points.get(one) for one in sources}) == 1
                            and all(one in points for one in sources)})
    numbered, variant = _VARIANTS[name]
    if any(numbered.get(point, gid) != gid for gid, found in letters.items() for point in found):
        return {}
    return {gid: found[:1] for gid, found in variant.items()}


def tounicode_entries(font):
    """The bfchar and bfrange entries of a font's ToUnicode CMap, as {glyph id: text}."""
    data = font["/ToUnicode"].get_object().get_data().decode("latin-1")
    entries = {}
    for block in re.findall(r"beginbfchar(.*?)endbfchar", data, re.S):
        for source, target in re.findall(r"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]*)>", block):
            entries[int(source, 16)] = bytes.fromhex(target).decode("utf-16-be", "replace")
    for block in re.findall(r"beginbfrange(.*?)endbfrange", data, re.S):
        for low, high, target in re.findall(r"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*(<[0-9A-Fa-f]*>|\[[^\]]*\])",
                                            block):
            low, high = int(low, 16), int(high, 16)
            if target.startswith("<"):
                # A range steps the last UTF-16 unit of its target, which can be the low half of a
                # surrogate pair or the last letter of several, as the CMap specification has it.
                units = bytes.fromhex(target[1:-1] or "0000")
                units = units if len(units) % 2 == 0 and units else b"\x00" + units
                last = int.from_bytes(units[-2:], "big")
                for gid in range(low, high + 1):
                    step = ((last + gid - low) & 0xFFFF).to_bytes(2, "big")
                    entries[gid] = (units[:-2] + step).decode("utf-16-be", "replace")
            else:
                for gid, one in zip(range(low, high + 1), re.findall(r"<([0-9A-Fa-f]*)>", target)):
                    entries[gid] = bytes.fromhex(one).decode("utf-16-be", "replace")
    return entries


def font_letters(font):
    """{glyph id: code points} from the TrueType program a Type0 font draws: its embedded FontFile2,
    or the system face it names. None where there is no program, or its glyph ids are not the CIDs."""
    descendant = font["/DescendantFonts"][0].get_object()
    if descendant.get("/CIDToGIDMap", "/Identity") != "/Identity":
        return None
    descriptor = descendant.get("/FontDescriptor")
    descriptor = descriptor.get_object() if descriptor is not None else {}
    if "/FontFile2" in descriptor:
        program = TTFont(io.BytesIO(descriptor["/FontFile2"].get_object().get_data()))
    else:
        face = SYSTEM_FONTS.get(str(font["/BaseFont"]).lstrip("/").split("+", 1)[-1])
        if face is None or not os.path.isfile(os.path.join(WINDOWS_FONTS, face)):
            return None
        program = TTFont(os.path.join(WINDOWS_FONTS, face))
    if "cmap" not in program:
        return None
    # A glyph the program maps to the private use area alone keeps that code, for PRIVATE_USE to
    # write as the letter it draws; one it also maps to a real letter takes the letter.
    letters, private = {}, {}
    for point, name in (program.getBestCmap() or {}).items():
        target = private if 0xE000 <= point <= 0xF8FF else letters
        target.setdefault(program.getGlyphID(name), []).append(point)
    for gid, points in private.items():
        letters.setdefault(gid, points)
    name = str(font["/BaseFont"]).lstrip("/").split("+", 1)[-1]
    if name in VARIANT_FONTS:
        for gid, points in variant_letters(name, letters).items():
            letters.setdefault(gid, points)
    return letters


def tounicode_stream(entries):
    """A ToUnicode CMap holding entries, {glyph id: text}, one bfchar each."""
    items = sorted(entries.items())
    lines = ["/CIDInit /ProcSet findresource begin", "12 dict begin", "begincmap",
             "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def",
             "/CMapName /Adobe-Identity-UCS def", "/CMapType 2 def",
             "1 begincodespacerange", "<0000> <FFFF>", "endcodespacerange"]
    for start in range(0, len(items), 100):
        block = items[start:start + 100]
        lines.append("%d beginbfchar" % len(block))
        lines.extend("<%04X> <%s>" % (gid, text.encode("utf-16-be").hex().upper()) for gid, text in block)
        lines.append("endbfchar")
    lines += ["endcmap", "CMapName currentdict /CMap defineresource pop", "end", "end"]
    stream = DecodedStreamObject()
    stream.set_data("\n".join(lines).encode("latin-1"))
    return stream


# The 2008 volume's PDFs set their text in CID TrueType subsets with no ToUnicode and a font program
# stripped of its cmap and glyph names, and each page carries its own subset that numbers the glyphs
# afresh: the Times-Roman of Thompson's page 1 and of its page 2 give one letter two codes. The text
# layer is a cipher (2&'1 for this). A glyph is known by its outline instead: the decomposed contours
# and advance width, hashed. A glyph copied from a system face has the same key as the system face's
# own glyph, and OUTLINE_FACES names the face to look in for each font; a glyph of a face the system
# lacks is named in outline_letters.tsv, each outline read by eye off outline_sheet.py's drawing and
# checked in the decoded text. Adobe's Times-Roman, which Windows lacks, is looked for in TeX Gyre
# Termes, the clone of its shapes MiKTeX installs; its outlines are no copy, and outline_match.py
# names those glyphs by raster.
TERMES = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", "MiKTeX", "fonts", "opentype", "public",
                      "tex-gyre", "texgyretermes-")
OUTLINE_FACES = {"TimesNewRomanPSMT": "times.ttf", "TimesNewRomanPS-BoldMT": "timesbd.ttf",
                 "TimesNewRomanPS-ItalicMT": "timesi.ttf", "TimesNewRomanPS-BoldItalicMT": "timesbi.ttf",
                 "Arial": "arial.ttf", "DejaVuSans": "DejaVuSans.ttf", "LucidaSansUnicode": "l_10646.ttf",
                 "TimesNewRoman": "times.ttf", "TimesNewRoman,Bold": "timesbd.ttf",
                 "TimesNewRoman,Italic": "timesi.ttf", "TimesNewRoman,BoldItalic": "timesbi.ttf",
                 "Calibri": "calibri.ttf", "Calibri,Bold": "calibrib.ttf", "Calibri,Italic": "calibrii.ttf",
                 "Calibri,BoldItalic": "calibriz.ttf", "Times-Roman": TERMES + "regular.otf",
                 "Times-Bold": TERMES + "bold.otf", "Times-Italic": TERMES + "italic.otf",
                 "Times-BoldItalic": TERMES + "bolditalic.otf", "Symbol": "symbol.ttf",
                 "SymbolMT": "symbol.ttf", "Helvetica": "arial.ttf", "CourierNewPSMT": "cour.ttf"}
PAPER_OUTLINED = tables.members("PAPER_OUTLINED")
_FACE_KEYS = {}


def outline_key(glyphset, name, advance):
    """A glyph's key: its outline with every component drawn in, and its advance in 2048ths of the
    em, hashed."""
    from fontTools.pens.recordingPen import DecomposingRecordingPen
    pen = DecomposingRecordingPen(glyphset)
    glyphset[name].draw(pen)
    return hashlib.sha1(repr((advance, pen.value)).encode()).hexdigest()[:16]


def face_keys(face):
    """{outline key: text} for every glyph a system face's cmap maps. Where several code points
    draw one outline, the right single quote and the modifier apostrophe of Times New Roman, the
    text is a character Windows-1252 holds, as Word exports, and failing that the lowest code point
    past the private use area."""
    if face not in _FACE_KEYS:
        font = TTFont(os.path.join("C:/Windows/Fonts", face))
        glyphset, units = font.getGlyphSet(), font["head"].unitsPerEm
        drawn = {}
        for point, name in font.getBestCmap().items():
            key = outline_key(glyphset, name, font["hmtx"][name][0] * 2048 // units)
            drawn.setdefault(key, []).append(point)
        keys = {}
        for key, points in drawn.items():
            points = sorted(points, key=lambda point: (0xE000 <= point <= 0xF8FF, not in_cp1252(point), point))
            keys[key] = chr(points[0])
        _FACE_KEYS[face] = keys
    return _FACE_KEYS[face]


def in_cp1252(point):
    try:
        chr(point).encode("cp1252")
        return True
    except UnicodeEncodeError:
        return False


def outline_letters():
    """{outline key: text} from outline_letters.tsv."""
    letters = {}
    with open(os.path.join(PRIVATE, "outline_letters.tsv"), encoding="utf-8") as handle:
        for line in handle:
            fields = line.rstrip("\n").split("\t")
            if len(fields) >= 2 and not fields[0].startswith("#"):
                letters[fields[0]] = fields[1]
    return letters


def outlined_fonts(path):
    """A paper whose Type0 fonts carry no ToUnicode, written again with a two-byte ToUnicode for each
    built from its glyphs' outline keys. A glyph with no contours and a width is a space. Returns (the
    PDF's bytes, a line per font, and a line per glyph no key names)."""
    reader = pypdf.PdfReader(path)
    writer = pypdf.PdfWriter(clone_from=reader)
    letters = outline_letters()
    seen, report, named, given = set(), [], 0, 0

    def fonts_under(resources):
        resources = resources.get_object() if resources is not None else {}
        for ref in (resources.get("/Font") or {}).values():
            yield ref.get_object()
        for ref in (resources.get("/XObject") or {}).values():
            inner = ref.get_object()
            if inner.get("/Subtype") == "/Form" and "/Resources" in inner:
                yield from fonts_under(inner["/Resources"])

    for number, page in enumerate(writer.pages, 1):
        for font in fonts_under(page.get("/Resources")):
            if id(font) in seen or font.get("/Subtype") != "/Type0" or "/ToUnicode" in font:
                continue
            seen.add(id(font))
            descendant = font["/DescendantFonts"][0].get_object()
            program = descendant["/FontDescriptor"].get_object().get("/FontFile2")
            # The code is the glyph id only where the CIDFont maps them one to one.
            if program is None or str(descendant.get("/CIDToGIDMap", "/Identity")) != "/Identity":
                continue
            # A face embedded twice is told apart by a *1 after its name, TimesNewRoman,Italic*1.
            name = re.sub(r"^[A-Z]{6}\+|\*\d+$", "", str(font.get("/BaseFont", "")).lstrip("/"))
            tt = TTFont(io.BytesIO(program.get_object().get_data()))
            glyphset, units = tt.getGlyphSet(), tt["head"].unitsPerEm
            system = face_keys(OUTLINE_FACES[name]) if name in OUTLINE_FACES else {}
            entries, unnamed = {}, []
            for gid, glyph in enumerate(tt.getGlyphOrder()):
                if gid == 0:
                    continue
                advance = tt["hmtx"][glyph][0] * 2048 // units
                key = outline_key(glyphset, glyph, advance)
                text = letters.get(key) or system.get(key)
                if text is None:
                    from fontTools.pens.recordingPen import DecomposingRecordingPen
                    pen = DecomposingRecordingPen(glyphset)
                    glyphset[glyph].draw(pen)
                    if not pen.value and advance:
                        text = " "
                if text is None:
                    unnamed.append("page %d %s glyph %d: %s" % (number, name, gid, key))
                    continue
                entries[gid] = text
            lines = ["/CIDInit /ProcSet findresource begin", "12 dict begin", "begincmap",
                     "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def",
                     "/CMapName /Adobe-Identity-UCS def", "/CMapType 2 def",
                     "1 begincodespacerange", "<0000> <FFFF>", "endcodespacerange",
                     "%d beginbfchar" % len(entries)]
            lines.extend("<%04X> <%s>" % (gid, text.encode("utf-16-be").hex().upper())
                         for gid, text in sorted(entries.items()))
            lines += ["endbfchar", "endcmap", "CMapName currentdict /CMap defineresource pop", "end", "end"]
            stream = DecodedStreamObject()
            stream.set_data("\n".join(lines).encode("latin-1"))
            font[NameObject("/ToUnicode")] = writer._add_object(stream)
            named += len(entries)
            given += 1
            report.extend(unnamed)
    report.insert(0, "%d fonts given a ToUnicode by outline: %d glyphs named, %d unnamed" % (
        given, named, len(report)))
    data = io.BytesIO()
    writer.write(data)
    return data.getvalue(), report


def mapped_fonts(path, maps, added=False):
    """A paper whose simple fonts carry no ToUnicode and number their glyphs from 1, written again
    with a one-byte ToUnicode for each font maps names, {font name without its subset tag: {code:
    text}}. pdfium reads such a font's codes as control characters, drops U+0002 and U+0003 from
    its text, and sets no character at all for a drawn code 32, the w of Denzer-King's wis; with the
    map it gives the letters. With added, a map adds to the codes the font's own ToUnicode gives.
    Returns (the PDF's bytes, a line for each font given a map)."""
    reader = pypdf.PdfReader(path)
    writer = pypdf.PdfWriter(clone_from=reader)
    seen, report = set(), []

    def fonts_under(resources):
        """Each font of resources and of the form XObjects under them: Denzer-King's pages draw
        everything inside one form each."""
        resources = resources.get_object() if resources is not None else {}
        for ref in (resources.get("/Font") or {}).values():
            yield ref.get_object()
        for ref in (resources.get("/XObject") or {}).values():
            inner = ref.get_object()
            if inner.get("/Subtype") == "/Form" and "/Resources" in inner:
                yield from fonts_under(inner["/Resources"])

    for page in writer.pages:
        for font in fonts_under(page.get("/Resources")):
            name = re.sub(r"^[A-Z]{6}\+", "", str(font.get("/BaseFont", "")).lstrip("/"))
            if id(font) in seen or name not in maps:
                continue
            seen.add(id(font))
            entries = dict(maps[name])
            if added and "/ToUnicode" in font:
                entries = {**tounicode_entries(font), **entries}
            lines = ["/CIDInit /ProcSet findresource begin", "12 dict begin", "begincmap",
                     "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def",
                     "/CMapName /Adobe-Identity-UCS def", "/CMapType 2 def",
                     "1 begincodespacerange", "<00> <FF>", "endcodespacerange",
                     "%d beginbfchar" % len(entries)]
            lines.extend("<%02X> <%s>" % (code, text.encode("utf-16-be").hex().upper())
                         for code, text in sorted(entries.items()))
            lines += ["endbfchar", "endcmap", "CMapName currentdict /CMap defineresource pop", "end", "end"]
            stream = DecodedStreamObject()
            stream.set_data("\n".join(lines).encode("latin-1"))
            font[NameObject("/ToUnicode")] = writer._add_object(stream)
            report.append("%s: a ToUnicode of %d codes written from the paper's cipher" % (name, len(maps[name])))
    data = io.BytesIO()
    writer.write(data)
    return data.getvalue(), report


def mended_fonts(path):
    """A paper whose Type0 fonts give a letter's glyph a space in ToUnicode, pro ides and howe er in
    Nater's 2019 paper and the whole run of Jantzen's small capitals, written again with the letter
    the font program itself maps to that glyph. A font is mended only where its program agrees with
    every single letter the ToUnicode names. Returns (the mended PDF's bytes, or None where no font
    needed it, and a line for each font mended or left alone)."""
    reader = pypdf.PdfReader(path)
    writer = pypdf.PdfWriter(clone_from=reader)
    seen, report, changed = set(), [], False
    # A face whose ToUnicode names no letter to check its program by, the bold italic -mín and -tín
    # of McKay's 2019 headings, is mended where another face of its family in the same paper agrees
    # with its program on ten letters and disagrees on none.
    unchecked, vouched = [], set()
    for page in writer.pages:
        fonts = page.get("/Resources", {}).get("/Font", {})
        for ref in fonts.values():
            font = ref.get_object()
            if id(font) in seen or font.get("/Subtype") != "/Type0" or "/ToUnicode" not in font:
                continue
            seen.add(id(font))
            entries = tounicode_entries(font)
            blank = [gid for gid, text in entries.items() if not text.strip()]
            letters = font_letters(font) if blank else None
            if not letters:
                continue
            agree, disagree = 0, []
            for gid, text in entries.items():
                if len(text) == 1 and text.strip() and gid in letters:
                    agree += ord(text) in letters[gid]
                    if ord(text) not in letters[gid]:
                        disagree.append(gid)
            # A ligature's glyph is given its letters, fi for ﬁ, as a ToUnicode gives them.
            given = {gid: unicodedata.normalize("NFKC", chr(min(letters[gid])))
                     if 0xFB00 <= min(letters[gid]) <= 0xFB06 else chr(min(letters[gid])) for gid in blank
                     if gid in letters and not any(chr(point).isspace() for point in letters[gid])}
            name = str(font["/BaseFont"]).lstrip("/")
            # The program draws the page, and where it agrees with twenty of the ToUnicode's letters
            # for each it does not, the few it does not are the ToUnicode's own faults: „ and ‟ for
            # the ‘ and ’ Laturnus's Times New Roman draws. A program embedded in the paper
            # draws it, and needs no ten letters to agree where none disagrees: Griffin's
            # 2019 bold qəǰi, whose ToUnicode names the ə alone.
            descendant = font["/DescendantFonts"][0].get_object()
            descriptor = descendant.get("/FontDescriptor")
            embedded = descriptor is not None and "/FontFile2" in descriptor.get_object()
            family = name.split("+", 1)[-1].split(",", 1)[0]
            if agree >= 10 and not disagree:
                vouched.add(family)
            if not agree and not disagree and given:
                unchecked.append((font, entries, given, name, family))
                continue
            if agree < 10 and not (embedded and agree and not disagree) or len(disagree) * 20 > agree:
                report.append("%s: left alone, its program agrees with %d letters and not %d"
                              % (name, agree, len(disagree)))
                continue
            given.update((gid, chr(min(letters[gid]))) for gid in disagree)
            if given:
                entries.update(given)
                font[NameObject("/ToUnicode")] = writer._add_object(tounicode_stream(entries))
                changed = True
                report.append("%s: %d glyphs given their letters, %s"
                              % (name, len(given), " ".join(sorted(set(given.values())))))
    for font, entries, given, name, family in unchecked:
        if family not in vouched:
            report.append("%s: left alone, no letter to check its program by" % name)
            continue
        entries.update(given)
        font[NameObject("/ToUnicode")] = writer._add_object(tounicode_stream(entries))
        changed = True
        report.append("%s: %d glyphs given their letters by its family's program, %s"
                      % (name, len(given), " ".join(sorted(set(given.values())))))
    if not changed:
        return None, report
    data = io.BytesIO()
    writer.write(data)
    return data.getvalue(), report


# Papers read before mended_fonts, whose oracles were checked against the fonts as they stand. Each
# has a font that gives one mark or one dotless letter a space, the ̌ of č or the ȷ of ǰ, and the
# mend would change what their ops and hand rows are keyed to. A paper leaves this list when its
# oracle is rebuilt on the mended fonts.
UNMENDED = tables.members("UNMENDED")
_OPENED = {}


def paper_document(stem):
    """(pdfium document, [line per font mended or left alone], whether any font was mended) for a
    paper, its fonts mended by mended_fonts unless it is in UNMENDED. Kept per stem, since gen.py
    opens a paper several times."""
    if stem not in _OPENED:
        path = os.path.join(CORPUS, "papers", stem + ".pdf")
        if stem in PAPER_OUTLINED:
            data, report = outlined_fonts(path)
        elif stem in PAPER_TOUNICODE:
            data, report = mapped_fonts(path, PAPER_TOUNICODE[stem], stem in PAPER_TOUNICODE_ADDED)
        else:
            data, report = (None, []) if stem in UNMENDED else mended_fonts(path)
        _OPENED[stem] = (pdfium.PdfDocument(data if data else path), report, data)
    document, report, data = _OPENED[stem]
    return document, report, data is not None


def layer_text(stem):
    """A paper's text layer, papers/<stem>.txt, pypdf's extract_text of each page. For a paper whose
    fonts were mended, the same extract_text of the mended pages, since the layer holds the spaces
    the fonts gave."""
    data = _OPENED[stem][2] if stem in _OPENED else None
    if data is None:
        with open(os.path.join(CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
            return handle.read()
    out = [""]
    for number, page in enumerate(pypdf.PdfReader(io.BytesIO(data)).pages):
        out.append("===== page %d =====" % (number + 1))
        out.append(page.extract_text())
    return "\n".join(out)


class OnePage(object):
    """One page of a document, shaped as a document of one page for gap_lines."""

    def __init__(self, document, number):
        self.document = document
        self.number = number

    def __len__(self):
        return 1

    def __getitem__(self, index):
        return self.document[self.number]


# Papers whose hand ops are keyed to the layer's spacing, where pdfium's line and the glyph line
# would respace a layer line they read alike: pr eferred in ZenkICSNL60 is preferred on the page.
# Such a paper does not build once respaced, and leaves this list when its ops are moved to the
# page's spacing and its oracle is rebuilt.
LAYER_SPACED = tables.members("LAYER_SPACED")
# Papers typed and scanned, whose page text a person transcribed from the scan, a line for each line
# the page prints and each page under its ===== page N ===== marker.
TRANSCRIBED_FROM_SCAN = tables.members("TRANSCRIBED_FROM_SCAN")
# The two letters a font's ToUnicode gives one glyph, and the letter the glyph draws: ə and the
# Cyrillic ә in one box, in Stewart, Noguchi, Sardinha and Davis's 2011 and 2012 papers.
CLOSEUP_UNJOINED = tables.members("CLOSEUP_UNJOINED")
ONE_GLYPH = {"əә": "ə"}
# The letter a font's ToUnicode gives for each mark, in wlwlmelst and Janzen's paper: ƛ for the
# comma above in the running text and w in the bold title, ə for the acute and x for the dot below.
MARK_OF = {"ƛ": COMMA_ABOVE, "w": COMMA_ABOVE, "ə": ACUTE, "x": DOT_BELOW}


# Glyphs a paper's text layer can drop where pdfium keeps them: the tilde operator of reduplication.
DROPPED = "∼"
# A font that maps the letters it draws to the private use area, which the text layer drops: [o for
# [p̓oq̓] `‘grey’` in Huijsmans. Each code point was read off a 600 dpi render of the page named, with
# pua_crops.py.
PRIVATE_USE = tables.gather("PRIVATE_USE")


# The modifier letter for each letter a page sets small and raised after another letter: the
# labialized [pɔqʷs] and palatalized čʸε of John Hamilton Davis's bracketed forms, which the text
# layer flattens to pɔqws and čyε.
MODIFIER = {"w": "ʷ", "y": "ʸ", "θ": "ᶿ", "ε": "ᵋ", "ɛ": "ᵋ", "o": "ᵒ", "a": "ᵃ", "h": "ʰ", "j": "ʲ",
            "n": "ⁿ", "m": "ᵐ", "ə": "ᵊ", "l": "ˡ", "x": "ˣ", "s": "ˢ", "ʔ": "ˀ", "ʕ": "ˤ", "ɣ": "ˠ",
            "i": "ⁱ", "u": "ᵘ", "e": "ᵉ", "t": "ᵗ", "k": "ᵏ", "p": "ᵖ", "b": "ᵇ", "d": "ᵈ", "g": "ᶢ",
            "r": "ʳ", "v": "ᵛ", "z": "ᶻ", "ɪ": "ᶦ", "ʊ": "ᶷ", "ɔ": "ᵓ", "χ": "ᵡ", "c": "ᶜ", "f": "ᶠ",
            "ɵ": "ᶱ"}
MODIFIER_LETTERS = set(MODIFIER.values())


SUBSCRIPT = dict(zip("0123456789", "₀₁₂₃₄₅₆₇₈₉"))


def raised_letters(textpage, mapping=None, ciphers=None, found=None, lifted=None, mark_base=False,
                   lowered=False):
    """The glyphs of a page with no space among them, and {position: modifier letter} for each
    letter set at under 0.85 of the size of the letter before it and raised a fifth of that size
    over its baseline. A digit is a footnote's mark and stays, and so does a letter raised after
    a digit, the th of 19th. A private-use glyph mapping names is taken as the letters it draws,
    and the ʷ after van Eijk's private-use k̓ is raised after a letter. ciphers, the paper's
    PAPER_CIPHERS entry, gives each glyph as its font means it, and Bird's raised SILDoulosIPA ´
    is read as the ə it draws. found, where given, collects {index in textpage: modifier letter}
    for each raised letter. lifted holds the indexes of the raised letters the glyph rows already
    write as their modifiers: each stands among the glyphs as its modifier and is left out of
    the positions returned. With mark_base, an apostrophe after a letter leaves that letter the
    one a raised letter is measured against, the ʷ of [k’ʷ]. With lowered, a digit set at under
    0.85 of the size of the letter, ∅ or bracket before it and lowered a tenth of that size under
    its baseline is an index, the subscript of Bill₁ and ∅₁ in Cable's co-reference examples."""
    glyphs, raised = [], {}
    base = None
    x, y = ctypes.c_double(), ctypes.c_double()
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        drawn = symbol == " " and ciphers and drawn_space(textpage, index) and \
            deciphered(symbol, textpage, index, ciphers) != " "
        if not symbol or (symbol.isspace() and not drawn) or symbol == "￾":
            continue
        if ciphers:
            symbol = deciphered(symbol, textpage, index, ciphers)
        if mapping and symbol in mapping:
            glyphs.extend(mapping[symbol])
            symbol = mapping[symbol][0]
        elif len(symbol) > 1:
            # A ligature a cipher gives as its letters, Galloway's fi, counts letter by letter.
            glyphs.extend(symbol)
            symbol = symbol[0]
        else:
            glyphs.append(symbol)
        if unicodedata.combining(symbol):
            continue
        pdfium_c.FPDFText_GetCharOrigin(textpage.raw, index, ctypes.byref(x), ctypes.byref(y))
        size = glyph_size(textpage, index)
        if base is not None and symbol in MODIFIER and base[0].isalpha() and size <= 0.85 * base[3] \
                and base[3] * 0.2 <= y.value - base[2] < base[3] and x.value >= base[1]:
            if found is not None:
                found[index] = MODIFIER[symbol]
            if lifted and index in lifted:
                glyphs[-1] = MODIFIER[symbol]
            else:
                raised[len(glyphs) - 1] = MODIFIER[symbol]
            continue
        if lowered and base is not None and symbol in SUBSCRIPT and (base[0].isalpha() or base[0] in "∅])₀₁₂₃₄₅₆₇₈₉") \
                and size <= 0.85 * base[3] and base[3] * 0.1 <= base[2] - y.value < base[3] * 0.6 and x.value >= base[1]:
            raised[len(glyphs) - 1] = SUBSCRIPT[symbol]
            base = (SUBSCRIPT[symbol], base[1], base[2], base[3])
            continue
        if mark_base and symbol in MARK_BASES and base is not None and base[0].isalpha():
            continue
        base = (symbol, x.value, y.value, size)
    return glyphs, raised


def raise_letters(document, merged, mapping=None, ciphers=None, lifted=None, mark_base=False, lowered=False):
    """merged, a page text with its ===== page N ===== markers, with each raised letter of the page
    written as its modifier: the page's glyphs are aligned to the text's letters page by page.
    mapping is the paper's PRIVATE_USE letters and ciphers its PAPER_CIPHERS entry. lifted, where
    given, holds for each page the indexes of the raised letters the glyph rows already wrote as
    their modifiers, and those are not set again. mark_base is raised_letters's. Every read ends
    here, and each pair of ONE_GLYPH is then written as the one letter its glyph draws. Returns the text and a defects row to each
    line changed."""
    import difflib
    out, repairs, page, lines = [], [], 0, []

    def flush():
        if not lines or page < 1 or page > len(document):
            out.extend(lines)
            return
        glyphs, raised = raised_letters(document[page - 1].get_textpage(), mapping, ciphers,
                                        lifted=lifted.get(page - 1) if lifted else None, mark_base=mark_base,
                                        lowered=lowered)
        if not raised:
            out.extend(lines)
            return
        places = [(row, at) for row, line in enumerate(lines) for at, letter in enumerate(line)
                  if not letter.isspace()]
        letters = [lines[row][at] for row, at in places]
        edited = [list(line) for line in lines]
        matcher = difflib.SequenceMatcher(None, glyphs, letters, autojunk=False)
        for first, second, count in matcher.get_matching_blocks():
            for step in range(count):
                if first + step in raised:
                    row, at = places[second + step]
                    edited[row][at] = raised[first + step]
        for line, letters_of in zip(lines, edited):
            changed = "".join(letters_of)
            if changed != line:
                repairs.append((page, "raised letter set on the line", line.strip(), changed.strip()))
            out.append(changed)

    for line in merged:
        marker = re.match(r"^===== page (\d+) =====$", line)
        if marker:
            flush()
            out.append(line)
            page, lines = int(marker.group(1)), []
            continue
        lines.append(line)
    flush()
    # A font whose ToUnicode gives one glyph two letters writes each ə of Stewart's Kwak'wala as ə
    # and the Cyrillic ә, both in the one box: nəәpʔiden for the nəpʔiden the page prints.
    page = 0
    for at, line in enumerate(out):
        marker = re.match(r"^===== page (\d+) =====$", line)
        if marker:
            page = int(marker.group(1))
            continue
        single = one_glyph(line)
        if single != line:
            repairs.append((page, "one glyph given two letters", line.strip(), single.strip()))
            out[at] = single
    return out, repairs


def one_glyph(text):
    """text with each pair of ONE_GLYPH written as the one letter its glyph draws."""
    for doubled, letter in ONE_GLYPH.items():
        text = text.replace(doubled, letter)
    return text


def lettered_glyphs(text, mapping):
    """text with each private-use glyph mapping names written as the letter it draws."""
    return "".join(mapping.get(symbol, symbol) for symbol in text) if mapping else text


def put_back(line, source):
    """line with each DROPPED glyph of source put back where source sets it, line's spaces kept."""
    out, at = [], 0
    for symbol in source:
        if symbol in DROPPED:
            out.append(symbol)
            continue
        if symbol.isspace():
            continue
        while at < len(line) and line[at] != symbol:
            out.append(line[at])
            at += 1
        if at < len(line):
            out.append(line[at])
            at += 1
    return "".join(out) + line[at:]


def misread_mark(symbol, box, size, last):
    """The combining mark a letter glyph stands for, where the font's ToUnicode names a mark as a
    letter, or None. The mark glyphs are 0.12 to 0.18 em wide and under 0.4 em tall, against 0.4 em
    wide and more for the letters themselves."""
    left, bottom, right, top = box
    if last is None or symbol not in MARK_OF or (right - left) >= 0.25 * size or \
            (top - bottom) >= 0.4 * size:
        return None
    mark = MARK_OF[symbol]
    # The comma and the acute sit above the letter before them, the dot on or under its baseline.
    if mark == DOT_BELOW:
        return mark if top < last[1] + 0.15 * size else None
    return mark if bottom > last[1] + 0.4 * size else None


def accent_lost(symbol, box, size):
    """A plain vowel whose glyph stands as tall as an accented one, 0.69 em for the é of weʔxstés
    against 0.47 for e: the font's ToUnicode dropped the acute of a precomposed letter. The i and í
    of the same font stand equally tall, and acute_on_i reads the í from the ink over it."""
    left, bottom, right, top = box
    return symbol in "aeouə" and (top - bottom) > 0.6 * size


INK_DPI = 600

# A typewriter face, by name: the Courier of Lonsdale and Matsushita's parser output. Its fonts
# carry no fixed-pitch flag there.
MONOSPACE = re.compile(r"Courier|Mono(?!type)|Consolas|Menlo|Monaco|LucidaConsole", re.I)


def ink_over(image, box, size, page_height, scale):
    """The dark pixels over a glyph's x-height inside its own box, as (count, width, height)."""
    left, bottom, right, top = box
    region = image.crop((int(left * scale), int((page_height - top - 0.05 * size) * scale),
                         int(right * scale), int((page_height - (bottom + 0.55 * size)) * scale)))
    pixels = region.load()
    dark = [(x, y) for x in range(region.width) for y in range(region.height) if pixels[x, y] < 128]
    if not dark:
        return 0, 0, 0
    xs = [one[0] for one in dark]
    ys = [one[1] for one in dark]
    return len(dark), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def acute_on_i(image, box, size, page_height, scale):
    """Whether an i the text layer gives is an í on the page.

    wlwlmelst and Janzen's font maps í to i and gives both one box, 0.22 em wide. The page prints
    the acute as a slanted stroke 12 to 16 pixels wide at 600 dpi and 11 pt, with 70 to 100 dark
    pixels, and the dot of i 8 to 10 wide with 56 to 69. The dot of a bold i is as wide as an acute
    and holds over 125. Measured over all 1007 i of the paper."""
    ink, wide, tall = ink_over(image, box, size, page_height, scale)
    unit = scale * size
    return wide / unit >= 0.125 and 0.0080 <= ink / unit ** 2 <= 0.0135


def word_gaps(document):
    """The glyph gaps of a paper in em, inside the text layer's words and at its spaces.

    Two neighboring letters on one baseline with no space between them in the text layer are one
    word, and the gap between their boxes is a gap inside a word. With a space between them the gap
    is a word space, or a space the layer inserted inside a word."""
    inside, spaced = [], []
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        last = None
        space = False
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if symbol in ("\r", "\n"):
                last, space = None, False
                continue
            if symbol == " ":
                space = True
                continue
            if not symbol or not symbol.isalpha():
                last, space = None, False
                continue
            box = textpage.get_charbox(index)
            size = glyph_size(textpage, index)
            if last is not None and abs(box[1] - last[1]) < 0.3 * size and box[0] >= last[0]:
                (spaced if space else inside).append((box[0] - last[2]) / size)
            last, space = box, False
    return inside, spaced


def calibrated_share(document):
    """A word-space threshold for this paper's own setting: 0.02 em over the 99.9th percentile of
    its gaps inside words, held between 0.1 and 0.18. Brown's gaps inside words reach 0.113 em at
    the 99.9th and its word spaces open at 0.18, where a justified line sets wil precedes 0.17 apart."""
    inside, spaced = word_gaps(document)
    if not inside:
        return 0.18
    inside.sort()
    return min(0.18, max(0.1, inside[min(len(inside) - 1, int(0.999 * len(inside)))] + 0.02))


def gap_lines(document, share=0.18, marks=None, wide=False, fonts=False, held_wide=False):
    """Each line of every page rebuilt from the characters' boxes, a space where the page leaves one.

    pdfium's own text keeps the spaces its layer holds and no others. Here two neighboring glyphs on
    one line are two words where the gap between their boxes is wider than share of the font size.
    A word space measures about 0.22 em on these pages and the gap inside a word 0.12 em at most,
    at the narrow box of an apostrophe, which finds the space a justified line printed and the
    layer dropped: withinarm’sreach is within arm’s reach on the page. A glyph that starts left of
    the one before it on a lower baseline opens a new line.
    """
    lines = []
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        current = []
        # The box of each letter in current, for a dot below the stream sets after later letters.
        boxes = []
        last = None
        # pdfium breaks the line at a raised mark it misread: nk̓ / y̓ep in the title. The break
        # waits for the next glyph, and stands only if that glyph starts a line of its own.
        held_break = False
        last_font, last_size = None, 0
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            # A glyph with no text behind it, a rule or a picture font's mark, holds no letter.
            if not symbol:
                continue
            if symbol in ("\r", "\n", "￾"):
                if current and symbol != "￾":
                    # A mark can come after the break pdfium set before it: x̣án / ƛ iws.
                    if marks is not None:
                        held_break = True
                        continue
                    lines.append("".join(current))
                    current, boxes, last = [], [], None
                elif symbol == "￾":
                    current.append("-")
                    boxes.append(None)
                continue
            box = textpage.get_charbox(index)
            size = glyph_size(textpage, index)
            if symbol == " ":
                # The layer sets a space of no width after a misread mark; the gap test decides.
                if current and unicodedata.combining(current[-1][0]) and box[2] - box[0] < 0.05 * size:
                    continue
                # Where the font misreads its marks, pdfium sets a space of no width beside each.
                if marks is not None and box[2] - box[0] < 0.05 * size:
                    continue
                if current and current[-1] != " ":
                    current.append(" ")
                    boxes.append(None)
                continue
            mark = misread_mark(symbol, box, size, last) if marks is not None else None
            if mark:
                marks.append(symbol)
                # The mark goes on the letter its box sits over or under, after any mark already
                # on it: the stream can set a dot below after the letters that follow its own.
                middle = (box[0] + box[2]) / 2
                under = [at for at, one in enumerate(boxes) if one is not None and one[0] <= middle <= one[2]]
                at = under[-1] + 1 if under else len(current)
                while at < len(current) and unicodedata.combining(current[at][0]):
                    at += 1
                current.insert(at, mark)
                boxes.insert(at, None)
                continue
            if marks is not None and accent_lost(symbol, box, size):
                marks.append("´")
                symbol = unicodedata.normalize("NFC", symbol + ACUTE)
            left, bottom, right, top = box
            if held_break:
                held_break = False
                if not (last is not None and abs(bottom - last[1]) < 0.3 * size and left >= last[0]):
                    lines.append("".join(current))
                    current, boxes, last = [], [], None
            if last is not None and left < last[0] and bottom < last[1] - size / 2 and current:
                lines.append("".join(current))
                current, boxes, last = [], [], None
            # A letter given back its acute with no precomposed form, ə́, is two characters, and
            # its base letter answers for it.
            base = symbol[0]
            # A combining mark sits over its letter and has no gap of its own.
            # A digit sits narrower than its advance, and (11) is no two words.
            if last is not None and not unicodedata.combining(base) and \
                    not symbol.isdigit() and not (current and current[-1].isdigit()):
                # Punctuation closes on the word before it and opens on the word after it.
                # With fonts, a change of face or size across the gap marks a word space at a
                # narrower gap: the small capitals of EXCL meet the bold of get.forgotten 0.15 em
                # on in Davis's (11), where a word's own gaps reach 0.14 em, St'át'imcets.
                # A letter with a mark can come from a second regular face, z̓ in cunám̓-ən, and
                # only weight, slant and size count, between two letters.
                changed = fonts and base.isalpha() and current[-1].isalpha() and (
                    face(font_name(textpage, index)) != face(last_font) or
                    abs(size - last_size) > 0.1 * size)
                gap_share = 0.08 if changed else share
                # A typewriter face sets each glyph in the same advance, and a narrow letter leaves
                # a wide gap on either side: recognize, never recogni ze, in the Courier of
                # Lonsdale and Matsushita's Figure 1. Between two of its glyphs only the layer's
                # own spaces stand.
                if left - last[2] > gap_share * size and current[-1] != " " and \
                        symbol not in ",.;:!?)]’”" and current[-1] not in "([‘“" and \
                        not (MONOSPACE.search(font_name(textpage, index)) and
                             MONOSPACE.search(font_name(textpage, last_index))):
                    # With wide, a column's gap between two cells of a tier is three spaces.
                    current.append("   " if wide and left - last[2] > 0.6 * size else " ")
                    boxes.append(None)
                # With held_wide, a column's gap is three spaces where the layer holds the space, between
                # the captions (a) and (b) of Menon's side-by-side figures. open_columns reads
                # without it: the widened gaps would part the moras µµ of Mellesmoen and
                # Urbanczyk's (37b) from its row's other cells.
                elif held_wide and current[-1] == " " and left - last[2] > 0.6 * size:
                    current[-1] = "   "
            current.append(symbol)
            boxes.append(box if not unicodedata.combining(base) else None)
            if not unicodedata.combining(base) and right > left:
                last, last_index = box, index
                if fonts:
                    last_font, last_size = font_name(textpage, index), size
        if current:
            lines.append("".join(current))
    return [one.strip() for one in lines if one.strip()]


def glyph_size(textpage, index):
    """The size in points glyph index is set at. pdfium gives the text state's font size, and a
    paper that sets its type at size 1 and scales it by the text matrix, Mellesmoen's ICSNL 57
    paper at 1 by 10.08, reads 1 there; its size is then that times the matrix's scale. A size
    of 4 or more is taken as given, the size every other paper here was read at."""
    size = pdfium_c.FPDFText_GetFontSize(textpage.raw, index) or 10
    if size >= 4:
        return size
    matrix = pdfium_c.FS_MATRIX()
    if not pdfium_c.FPDFText_GetMatrix(textpage.raw, index, ctypes.byref(matrix)):
        return size
    return size * abs(matrix.a * matrix.d - matrix.b * matrix.c) ** 0.5


def font_name(textpage, index):
    """The name of the font glyph index is set in, NimbusRomNo9L-ReguItal or the like."""
    buffer = ctypes.create_string_buffer(128)
    flags = ctypes.c_int()
    pdfium_c.FPDFText_GetFontInfo(textpage.raw, index, buffer, 128, ctypes.byref(flags))
    return buffer.value.decode("latin-1")


# A font whose ToUnicode puts its letters at a fixed offset from their own codes: the name after the
# subset prefix, then the lowest and highest code it uses and the offset to take off. McClay and
# Birdstone set their small-capital gloss labels in Brill-Roman at U+0614 to U+062D, Arabic letters
# in the text layer, IND as U+061C U+0621 U+0617, and pdfium's bidi order scrambles them there.
FONT_CIPHERS = {"Brill-Roman": (0x614, 0x62D, 0x5D3)}
# A paper whose font puts every code it sets at an offset, by the font's name: each range, lowest
# and highest code, with the offset to take off.
PAPER_CIPHERS = tables.gather("PAPER_CIPHERS")
# A paper whose simple fonts carry no map to Unicode, given one before it is read: by the font's
# name without its subset tag, each code and the text its glyph draws.
PAPER_TOUNICODE = tables.gather("PAPER_TOUNICODE")
# Papers whose PAPER_TOUNICODE maps add to the faces' own ToUnicode.
PAPER_TOUNICODE_ADDED = tables.members("PAPER_TOUNICODE_ADDED")
# A paper that draws letters as images, by the first ten hex digits of the SHA-1 of each bitmap's
# pixels and size: the letters it stands for, ' for the comma above. Read in rows mode only.
PAPER_IMAGES = tables.gather("PAPER_IMAGES")
# Papers that draw the glottal mark as an apostrophe over its letter, q and y of stsq̓éy̓ in Ignace,
# Ignace and Lyon: the stream sets it after the line, its middle over the letter and its bottom
# above the letter's top, and the rows would read it into the line above, transfor’mers. Each is
# set on its letter as the comma above. Read in rows mode only.
PAPER_OVERSET = tables.members("PAPER_OVERSET")
# Papers whose word-space share the calibration sets too low, read in rows mode. The calibration
# counts only gaps between two letters.
PAPER_SHARE = tables.gather("PAPER_SHARE")
# Papers read with row_lines's tracked rules. Read in rows mode only.
PAPER_TRACKED = tables.members("PAPER_TRACKED")
# Faces whose word spaces the stream alone sets, by paper, read in rows mode.
PAPER_STREAM_FACES = tables.gather("PAPER_STREAM_FACES")
# Papers that underline a letter with a rule drawn under it, the k̲, x̲ and g̲ of the U'mista
# orthography in Black's k̲ina̲m=ox̲=da ga̲la mix̲a, where the font holds no underlined letter. Each
# rule a letter wide is set on its letter as the macron below. Read in rows mode only.
PAPER_UNDERLINED = tables.members("PAPER_UNDERLINED")
UNDER_RULE = "̱"
# The letters the U'mista orthography underlines, a̲ for schwa, g̲, k̲ and x̲, and the schwa Black's
# phonetic lines underline once, m̉ə̲kwalá in (18b).
UNDERLINED_LETTERS = set("agkxə")


def under_rules(page, glyphs):
    """Set the macron below on each glyph of glyphs a short rule is drawn under: a filled path
    under 0.15 em high whose level is within 0.3 em of the letters' bottoms and below their
    middles, over one to four letters side by side, the letters whose middles it spans. Its
    width is 0.6 to 1.6 times theirs and 0.1 em more: one rule underlines g̲a̲ of Black's
    g̲a̲ls=ox̲=da, and the rule under 'a̲m in (14a) runs under its apostrophe too. A g's
    descender reaches under the rule, which stands on the row's baseline. The corners and stubs
    of the table (32) span no letter's width and are passed over, and the underline of a caption
    runs the length of its words and is left alone. Returns the count set."""
    count = 0
    for one in page.get_objects(filter=[pdfium_c.FPDF_PAGEOBJ_PATH], max_depth=3):
        left, bottom, right, top = one.get_bounds()
        height, level = top - bottom, (top + bottom) / 2
        under = [glyph for glyph in glyphs if glyph[0][:1].isalpha() and height < 0.15 * glyph[2]
                 and left <= (glyph[1][0] + glyph[1][2]) / 2 <= right
                 and abs(level - glyph[1][1]) < 0.3 * glyph[2] and level < (glyph[1][1] + glyph[1][3]) / 2]
        if not 1 <= len(under) <= 4:
            continue
        # A rule under two letters or more underlines a letter of the orthography each, or else a
        # word for emphasis, #-Noun and gukʷ in the schema of Black's (7), which is left alone.
        if len(under) > 1 and not all(glyph[0][:1] in UNDERLINED_LETTERS for glyph in under):
            continue
        span = max(glyph[1][2] for glyph in under) - min(glyph[1][0] for glyph in under)
        if 0.6 * span <= right - left <= 1.6 * span + 0.1 * under[0][2]:
            for glyph in under:
                glyph[0] += UNDER_RULE
            count += len(under)
    return count


# Papers that strike letters out with a rule drawn through them, the deleted hi of Sardinha's 2011
# Table 4, hi sk, and of (ii) on her page 12. Each struck letter takes the long stroke overlay.
# Read in rows mode only.
PAPER_STRUCK = tables.members("PAPER_STRUCK")
STRIKE_RULE = "̶"
# Papers that set a spacing acute alone for a stressed vowel left unwritten, Rude's 2012 Nez Perce
# underlying forms /t´yam/ and /p´qʷn/. Read in rows mode only.
PAPER_LONE_ACUTE = tables.members("PAPER_LONE_ACUTE")
# Papers that draw a glyph back over the stream space set before it, Lyon's 2010 iʔ. Read in rows
# mode only.
PAPER_DRAWN_BACK = tables.members("PAPER_DRAWN_BACK")
# Papers that raise a letter after an apostrophe set on the letter before it, Smith's 2011 ejective
# [k’ʷ] and [q’ʷ]: the apostrophe is passed over and the ʷ is measured against the k. Read in rows
# mode only.
PAPER_MARK_BASE = tables.members("PAPER_MARK_BASE")
MARK_BASES = "’'ʼ"
# Papers that set an index as a lowered digit, Bill₁ and ∅₁ in Cable's co-reference examples, which
# the glyph rows read as a plain one. Read in rows mode only.
PAPER_SUBSCRIPTED = tables.members("PAPER_SUBSCRIPTED")
# Papers whose tables are ruled grids with cells that wrap, Nater's 2013 lexicon: the glyph rows
# read a cell of two lines into the lines of the cells beside it, *ƛ’əp ‘deep (water)’ over
# (Ku02:143) as (*Kƛu’ǝ0p2‘:d14ee3p) (water)’. Each grid is read cell by cell instead, a line to a
# table row. Read in rows mode only.
PAPER_RULED = tables.members("PAPER_RULED")
# A horizontal rule is a path under RULE points high and wider than it is high, a vertical rule
# the other way. A dashed rule is drawn a cell high at a time, its bounds RULE_DASHED points wide
# with the stroke's width: the dashed column rules of Mellesmoen's 2017 tableaux, 2pt wide and 18pt
# high, and it counts where it is RULE_LONG times as long as it is wide. Two rules within RULE_JOIN
# points of each other are one edge: Word draws a cell's corner as a small square of its own beside
# the rule.
RULE = 1.2
RULE_DASHED = 2.5
RULE_LONG = 4
RULE_JOIN = 1.5


def rule_edges(values):
    """The distinct positions among values, each cluster within RULE_JOIN points taken at its middle."""
    out = []
    for value in sorted(values):
        if out and value - out[-1][-1] <= RULE_JOIN:
            out[-1].append(value)
        else:
            out.append([value])
    return [sum(one) / len(one) for one in out]


def ruled_grids(page):
    """Each ruled grid of a page, top first, as (xs, ys): its column edges left to right and its
    row edges top to bottom, in points from the foot of the page. A vertical rule joins the grid
    whose span it touches; a grid's horizontal rules are the ones inside its span."""
    flat, upright = [], []
    for obj in page.get_objects(max_depth=5):
        if obj.type != pdfium.raw.FPDF_PAGEOBJ_PATH:
            continue
        left, bottom, right, top = obj.get_bounds()
        wide, high = right - left, top - bottom
        if high <= RULE < wide or high <= RULE_DASHED and wide >= RULE_LONG * high:
            flat.append((left, bottom, right, top))
        elif wide <= RULE < high or wide <= RULE_DASHED and high >= RULE_LONG * wide:
            upright.append((left, bottom, right, top))
    found = []
    for box in sorted(upright, key=lambda one: -one[3]):
        for grid in found:
            if box[1] <= grid["top"] + RULE_JOIN and box[3] >= grid["bottom"] - RULE_JOIN:
                grid["up"].append(box)
                grid["top"], grid["bottom"] = max(grid["top"], box[3]), min(grid["bottom"], box[1])
                break
        else:
            found.append({"up": [box], "top": box[3], "bottom": box[1]})
    out = []
    for grid in found:
        xs = rule_edges([(one[0] + one[2]) / 2 for one in grid["up"]])
        ys = rule_edges([(one[1] + one[3]) / 2 for one in flat
                         if grid["bottom"] - RULE_JOIN <= (one[1] + one[3]) / 2 <= grid["top"] + RULE_JOIN
                         and one[0] >= xs[0] - RULE_JOIN and one[2] <= xs[-1] + RULE_JOIN])
        if len(xs) > 1 and len(ys) > 1:
            out.append((xs, ys[::-1]))
    return sorted(out, key=lambda one: -one[1][0])


def ruled_tables(page, bold=None, grids=None):
    """Each ruled grid of a page read cell by cell, top first: (xs, ys, rows), each row a list of
    cells and each cell a list of lines. Each glyph goes to the cell its middle stands in. Inside a
    cell the glyphs keep the order the stream sets them in, and a glyph whose top stands below the
    lowest bottom of the line so far opens a new line: a comma after a raised U+2019 stays on its line.
    The spaces are the stream's own; a gap test between boxes reads the side bearings of a narrow 1
    as spaces, Ku02 : 1 43 for Ku02:143. With bold, a pair of strings, each run of glyphs set in a
    bold face is written between them: the reduplicant of each candidate in Mellesmoen's 2017
    tableaux, θo[θ]mɪn with bold ("[", "]"). grids, a list of (xs, ys) as ruled_grids gives them,
    reads a table whose columns no rule parts: Table 1 of the same paper, ruled only above and
    below, its edges taken from the words' boxes."""
    textpage = page.get_textpage()
    found = ruled_grids(page) if grids is None else grids
    # Each cell is a list of lines, each [text, lowest bottom so far, inside a bold run].
    cells = [[[[] for _ in range(len(xs) - 1)] for _ in range(len(ys) - 1)] for xs, ys in found]
    spaced = {}
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        if symbol in ("\r", "\n", ""):
            continue
        box = textpage.get_charbox(index)
        # pdfium gives U+FFFE for a hyphen that ends a line after a letter, and the page prints
        # it: ‘chiseling- / mouth’ in Nater's 2013 line 28, c’ikʷn- ending line 155's form, and C1i-
        # at the end of a line of Table 1's cell in Mellesmoen's 2017 paper.
        if symbol == "￾":
            if box[2] <= box[0]:
                continue
            symbol = "-"
        middle_x, middle_y = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
        for which, (xs, ys) in enumerate(found):
            if not (xs[0] < middle_x < xs[-1] and ys[-1] < middle_y < ys[0]):
                continue
            column = max(one for one in range(len(xs) - 1) if xs[one] <= middle_x)
            row = max(one for one in range(len(ys) - 1) if ys[one] >= middle_y)
            key = (which, row, column)
            lines = cells[which][row][column]
            if symbol == " ":
                spaced[key] = bool(lines)
                break
            # A tie of enclisis stands low between the words it joins, its top under the line's
            # letters, and opens no line. The stream sets a space before it, which the page draws
            # it back into: ʔinutᴗʔiks in Nater's 2013 line 360 streams as ʔinut ᴗʔiks.
            tie = symbol == "ᴗ"
            heavy = bool(bold) and face(font_name(textpage, index))[0]
            if not lines or box[3] < lines[-1][1] and not tie:
                if lines and lines[-1][2]:
                    lines[-1][0], lines[-1][2] = lines[-1][0] + bold[1], False
                lines.append(["", box[1], False])
            elif spaced.get(key) and not tie:
                if lines[-1][2]:
                    lines[-1][0], lines[-1][2] = lines[-1][0] + bold[1], False
                lines[-1][0] += " "
            spaced[key] = False
            if heavy != lines[-1][2]:
                lines[-1][0] += bold[0] if heavy else bold[1]
                lines[-1][2] = heavy
            lines[-1][0] += symbol
            if not tie:
                lines[-1][1] = min(lines[-1][1], box[1])
            break
    return [(xs, ys, [[[(one[0] + (bold[1] if one[2] else "")).strip() for one in cell] for cell in row]
                      for row in table])
            for (xs, ys), table in zip(found, cells)]


def cell_text(cell):
    """A cell's lines joined with a space, as gen joins a paragraph's lines. A line that ends on a
    hyphen keeps it and the space: the page does not show whether the hyphen joins a compound,
    ‘chiseling- / mouth’ in Nater's 2013 line 28, or stands before a space, C1i- / for CVC roots in
    Mellesmoen's 2017 Table 1, and pdfium's stream is the same for both."""
    return " ".join(cell)


def ruled_line(row):
    """A table row as one line of the page text: its cells three spaces apart, as the glyph rows
    set a column's gap, and each cell's lines joined by cell_text."""
    return "   ".join(cell_text(cell) for cell in row if cell).strip()


def strike_rules(page, glyphs):
    """Set the long stroke overlay on each glyph of glyphs a short rule is drawn through: a filled
    path under 0.15 em high whose level stands inside the letters' boxes and 0.15 to 0.6 em over
    their bottoms, through their x-height and the descender of the j of Cable's struck Object in
    (41), over the letters whose middles it spans. Its width is 0.8 to 1.3 times that of the glyphs
    whose middles it spans on the row, brackets and stops among them, and 0.1 em more: the rule
    through Sardinha's hi runs 7.7pt over letters 7.4pt wide, 0.22 em up, and the one through
    Cable's [ S. or R.-ko ]₁ in (44) over its brackets. A table's border runs a cell wide and spans
    no letters' width. Returns the count set."""
    count = 0
    for one in page.get_objects(filter=[pdfium_c.FPDF_PAGEOBJ_PATH], max_depth=3):
        left, bottom, right, top = one.get_bounds()
        height, level = top - bottom, (top + bottom) / 2
        through = [glyph for glyph in glyphs if glyph[0][:1].isalpha() and height < 0.15 * glyph[2]
                   and left <= (glyph[1][0] + glyph[1][2]) / 2 <= right
                   and glyph[1][1] < level < glyph[1][3]
                   and 0.15 * glyph[2] <= level - glyph[1][1] <= 0.6 * glyph[2]]
        if not through:
            continue
        size = through[0][2]
        spanned = [glyph for glyph in glyphs if glyph[1][2] > glyph[1][0]
                   and left <= (glyph[1][0] + glyph[1][2]) / 2 <= right
                   and abs((glyph[1][1] + glyph[1][3]) / 2 - level) < 0.5 * size]
        first, last = min(glyph[1][0] for glyph in spanned), max(glyph[1][2] for glyph in spanned)
        # A rule that runs on through the space to the next glyph or back to the one before, Tom
        # and its space in Cable's struck [ Tom ka Linda ]₁ in (45), spans that space too.
        row = [glyph for glyph in glyphs if glyph[1][2] > glyph[1][0]
               and abs((glyph[1][1] + glyph[1][3]) / 2 - level) < 0.5 * size]
        after = [glyph[1][0] for glyph in row if glyph[1][0] >= last]
        before = [glyph[1][2] for glyph in row if glyph[1][2] <= first]
        if after and min(after) <= right + 0.1 * size:
            last = max(last, min(min(after), right))
        if before and max(before) >= left - 0.1 * size:
            first = min(first, max(max(before), left))
        span = last - first
        if 0.8 * span <= right - left <= 1.3 * span + 0.1 * through[0][2]:
            for glyph in through:
                glyph[0] += STRIKE_RULE
            count += len(through)
    return count


def underlined_runs(document, ciphers=None):
    """The runs of text each page underlines, {page number: [text, ...]}, left to right down the
    page: a rule under 1.2pt high and 3 to 80pt wide drawn up to 3.5pt under the glyphs whose
    middles it spans. Rude's 2012 paper underlines the Nez Perce morphemes of strong vowel harmony,
    /tahay/ in his (3), and the text layer keeps the letters and drops the rule. A table's border
    runs wider than any form and is left out. A glyph set more than 0.2 em after the one before it
    opens a new word."""
    found = {}
    for number in range(len(document)):
        page = document[number]
        textpage = page.get_textpage()
        glyphs = []
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            box = textpage.get_charbox(index)
            if not symbol or symbol in "\r\n " or box[2] <= box[0]:
                continue
            glyphs.append((deciphered(symbol, textpage, index, ciphers), box,
                           pdfium_c.FPDFText_GetFontSize(textpage.raw, index)))
        runs = []
        for one in page.get_objects(filter=[pdfium_c.FPDF_PAGEOBJ_PATH], max_depth=3):
            left, bottom, right, top = one.get_bounds()
            if top - bottom > 1.2 or not 3 <= right - left <= 80:
                continue
            spanned = [glyph for glyph in glyphs if left - 0.5 <= (glyph[1][0] + glyph[1][2]) / 2 <= right + 0.5]
            sitting = [glyph for glyph in spanned if -1.0 <= glyph[1][1] - top <= 3.5]
            if not sitting:
                continue
            # A mark standing clear of the letters, the lone ´ of /c̓´kn/ in Rude's Table 3, is
            # taken with them where its box reaches into their height.
            low, high = min(glyph[1][1] for glyph in sitting), max(glyph[1][3] for glyph in sitting)
            over = sorted((glyph for glyph in spanned if glyph[1][1] < high and glyph[1][3] > low),
                          key=lambda glyph: glyph[1][0])
            # An underline runs the width of its letters, give or take a letter; a table's rule
            # under a row runs out to the cell's edges.
            span = over[-1][1][2] - over[0][1][0]
            if abs(left - over[0][1][0]) > 0.5 * over[0][2] or abs(right - over[-1][1][2]) > 0.5 * over[-1][2]:
                continue
            text, previous = [], None
            for symbol, box, size in over:
                if previous is not None and box[0] - previous[2] > 0.2 * size:
                    text.append(" ")
                text.append(symbol)
                previous = box
            if text:
                runs.append((-top, left, unicodedata.normalize("NFC", "".join(text))))
        if runs:
            found[number + 1] = [text for _, _, text in sorted(runs)]
    return found


def over_letters(glyphs):
    """glyphs with each apostrophe drawn over a letter set on that letter as the comma above. The
    mark's left edge starts inside the letter's box or at its edge, 0.05 em in over the l̓ of
    Sisyúl̓ecw, 0.08 over the italic y̓ of stsq̓ey̓ and 0.00 over the italic k̓ of n7ek̓, where a spacing
    apostrophe starts 0.07 em past it or more, Teit’s and St’át’imcets. Its middle stands over the letter or up to 0.15 em past its right edge,
    and its bottom at most 0.12 em under the letter's top, over the ascender of l, and at most a third
    of an em over it. The glyphs
    left are renumbered in stream order. The stream can carry the mark far from its letter, the ’ of
    skwtut̓s set among the glyphs of a line above, with a space each side of it; the glyph after it
    is left to the gap test."""
    kept = []
    dropped = False
    for glyph in glyphs:
        if dropped:
            glyph[5] = False
            dropped = False
        under = letter_under(glyph, glyphs) if glyph[0] == "’" else None
        if under is not None:
            under[0] += COMMA_ABOVE
            dropped = True
            continue
        kept.append(glyph)
    for order, glyph in enumerate(kept):
        glyph[4] = order
    return kept


def letter_under(mark, glyphs):
    """The glyph of glyphs whose letter the apostrophe glyph mark is drawn over, by over_letters's
    measures, or None."""
    box, size = mark[1], mark[2]
    middle = (box[0] + box[2]) / 2
    under = [one for one in glyphs if one[0][:1].isalpha() and box[0] < one[1][2] + 0.03 * size and
             one[1][0] + 0.2 * (one[1][2] - one[1][0]) <= middle <= one[1][2] + 0.15 * size
             and -0.12 * size <= box[1] - one[1][3] < 0.35 * size
             and box[1] > one[1][1] + 0.4 * size]
    return min(under, key=lambda one: abs((one[1][0] + one[1][2]) / 2 - middle)) if under else None


def overset_marks(textpage):
    """For italic_runs: {the index of each letter an apostrophe is drawn over: the comma above}, and
    the set of those apostrophes' indexes, by over_letters's measures on the page's glyphs."""
    glyphs = []
    for index in range(textpage.count_chars()):
        symbol = textpage.get_text_range(index, 1)
        if symbol.isalpha() or symbol == "’":
            glyphs.append([symbol, textpage.get_charbox(index), glyph_size(textpage, index), None, index, False])
    marks, taken = {}, set()
    for glyph in glyphs:
        under = letter_under(glyph, glyphs) if glyph[0] == "’" else None
        if under is not None:
            marks[under[4]] = marks.get(under[4], "") + COMMA_ABOVE
            taken.add(glyph[4])
    return marks, taken


def image_glyphs(page, images, number=None):
    """(letters, box, size) for each image of page that images names by its bitmap, the box its ink's
    alone: a bitmap's white margin reaches over the letter after it, and its box would close the word
    space there. An image whose letter is drawn by its soft mask over a bitmap of two by two pixels,
    which pdfium does not hand back, is named by its place instead, "page@left,bottom" in whole
    points, number the page's index; its box is the image's and its size two thirds of its height."""
    found = []
    for one in page.get_objects(filter=[pdfium_c.FPDF_PAGEOBJ_IMAGE], max_depth=3) if images else ():
        left, bottom, right, top = one.get_bounds()
        place = "%d@%d,%d" % ((number or 0) + 1, round(left), round(bottom))
        if place in images:
            # Janzen's masked images stand 15.1 points tall over a line of 9.96 point type.
            found.append((unicodedata.normalize("NFC", images[place].replace("'", COMMA_ABOVE)),
                          (left, bottom, right, top), (top - bottom) * 2 / 3))
            continue
        bitmap = one.get_bitmap(render=False).to_pil()
        key = hashlib.sha1(bitmap.tobytes() + bytes(str(bitmap.size), "ascii")).hexdigest()[:10]
        if key not in images:
            continue
        gray = bitmap.convert("L")
        ink = gray.point(lambda value: 255 if value < 195 else 0).getbbox()
        width, height = right - left, top - bottom
        box = (left + ink[0] / gray.width * width, top - ink[3] / gray.height * height,
               left + ink[2] / gray.width * width, top - ink[1] / gray.height * height)
        # The size is the box's height where the box stands a line tall; a bold letter's box ten lines
        # tall would draw the rows round it into one, and there the ink's height gives the size, a
        # letter's ink with its comma about 0.7 of the type size.
        size = min(height, (box[3] - box[1]) / 0.7)
        # A tie's ink is a thin arc under the line, and its middle and size would set it on a row of
        # its own; it takes its height from the image, which stands a line tall, as its tier does.
        if set(images[key]) == {"˽"}:
            box, size = (box[0], bottom, box[2], top), height
        found.append((unicodedata.normalize("NFC", images[key].replace("'", COMMA_ABOVE)), box, size))
    return found


def deciphered(symbol, textpage, index, ciphers=None):
    """symbol as its font means it: a code in a FONT_CIPHERS range less the font's offset, taken
    only where that gives a capital A to Z; any other code, or font, as it is. ciphers, a paper's
    PAPER_CIPHERS entry, takes each code in a range of its font less that range's offset, or, where
    the offset is text, as that text: Galloway's DoulosSIL draws the fi ligature at U+03E7. The
    entry names a font with its subset tag where the paper sets another font of that name in
    plain codes: Galloway's page numbers are in an untagged TimesNewRomanPSMT."""
    if ciphers and len(symbol) == 1:
        font = font_name(textpage, index)
        for low, high, offset in ciphers.get(font, ciphers.get(re.sub(r"^[A-Z]{6}\+", "", font), ())):
            if low <= ord(symbol) <= high:
                return offset if isinstance(offset, str) else chr(ord(symbol) - offset)
    if len(symbol) != 1 or not any(low <= ord(symbol) <= high for low, high, _ in FONT_CIPHERS.values()):
        return symbol
    cipher = FONT_CIPHERS.get(re.sub(r"^[A-Z]{6}\+", "", font_name(textpage, index)))
    if cipher is None or not cipher[0] <= ord(symbol) <= cipher[1]:
        return symbol
    letter = chr(ord(symbol) - cipher[2])
    return letter if "A" <= letter <= "Z" else symbol


def drawn_space(textpage, index):
    """True for a space the page draws with ink, not one pdfium set: Galloway's enciphered Times
    draws its = at the code of a space, (=[¢] on page 5."""
    left, bottom, right, top = textpage.get_charbox(index)
    return not pdfium_c.FPDFText_IsGenerated(textpage.raw, index) and right - left > 0.01 and top - bottom > 0.01


def clip_box(obj):
    """The bounding box of a page object's clip path, (left, bottom, right, top), or None where it
    has none."""
    clip = pdfium_c.FPDFPageObj_GetClipPath(obj)
    if not clip:
        return None
    xs, ys = [], []
    for path in range(pdfium_c.FPDFClipPath_CountPaths(clip)):
        for index in range(pdfium_c.FPDFClipPath_CountPathSegments(clip, path)):
            segment = pdfium_c.FPDFClipPath_GetPathSegment(clip, path, index)
            x, y = ctypes.c_float(), ctypes.c_float()
            pdfium_c.FPDFPathSegment_GetPoint(segment, ctypes.byref(x), ctypes.byref(y))
            xs.append(x.value)
            ys.append(y.value)
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def clipped(textpage, index, box, clips):
    """True for a glyph standing wholly outside its text object's clip, which the page carries and
    does not draw: an earlier draft of Noguchi's page 19 under the one printed, 1,987 glyphs clipped
    to Figure 13's frame. Of the other papers only Schneider's and Thompson's carry any, two glyphs
    each, and both are read from the text layer. clips caches each object's clip box by its
    pointer, one dict per page; 1 pt of slack keeps a glyph whose box only grazes the clip's edge."""
    obj = pdfium_c.FPDFText_GetTextObject(textpage.raw, index)
    if not obj:
        return False
    key = ctypes.cast(obj, ctypes.c_void_p).value
    if key not in clips:
        clips[key] = clip_box(obj)
    clip = clips[key]
    return clip is not None and (box[2] < clip[0] - 1 or box[0] > clip[2] + 1 or
                                 box[3] < clip[1] - 1 or box[1] > clip[3] + 1)


def slant_defects(slanted):
    """defects.tsv rows for the spaces row_lines set at a change of slant or a capital after f."""
    return [(page, "word space narrowed where the slant changes or a capital follows f",
             "%s%s" % (before, after), "%s %s, %.2f em" % (before, after, gap))
            for page, before, after, gap in slanted]


def face(font):
    """The weight and slant of a font by its name: (bold, italic)."""
    return bool(re.search(r"Bold|Bd\b|Black|Heavy|Semibold", font or "")), italic(font or "")


def italic(font):
    """True for a slanted face, NimbusRomNo9L-ReguItal, CMTI10 or Times-Italic."""
    return bool(re.search(r"Ital|Obli|-It\b|CMTI|CMMI|Slant", font))


def sheared(textpage, index):
    """True for a glyph an upright face sets slanted by its text matrix, an oblique the writer made
    of a face with no italic: Sardinha's 2011 FirstNationsUnicode forms, sheared by a quarter of
    their size, təp'ídida bábaGʷəmeχ in her note 11 and the =χa qʷəʔsta of her page 16."""
    matrix = pdfium_c.FS_MATRIX()
    if not pdfium_c.FPDFText_GetMatrix(textpage.raw, index, ctypes.byref(matrix)) or not matrix.d:
        return False
    return abs(matrix.b) < 1e-6 and matrix.c / matrix.d > 0.1


# Papers whose cited forms are sheared upright faces. Elsewhere a writer's synthesized oblique
# covers only part of a form, the həxʔid and doχaλəɬnukʷ of Sardinha's 2013 paper, and splits it.
PAPER_SHEARED = tables.members("PAPER_SHEARED")


def row_lines(document, marks, share=0.18, read_marks=True, slanted=None, stream_spaces=False,
              ciphers=None, images=None, overset=False, underlined=False, struck=False, tracked=False,
              lifted=None, lone_acute=False, drawn_back=False, mark_base=False, ruled=False,
              stream_faces=()):
    """Each page's lines rebuilt by position: the glyphs grouped by baseline and each row read left
    to right, a space where the page leaves a gap and three where it leaves a column's.

    An interlinear set cell by cell, ɬe / ɬ= / REM / e= / DET / the in the stream of wlwlmelst and
    Janzen's (1), comes out as its four tiers. With read_marks, for a font that names its marks as
    letters, the misread marks are set on their letters first, in the stream order the font gives
    them, and each í is read from the ink over it. A heading's number keeps the gap after it, 3.3
    Interlinear, where a digit inside a line keeps none. slanted, where given, collects each space
    set only because a roman word met an italic one or a capital followed f: (page, the word
    before, the letter after, the gap in em). With stream_spaces, a space pdfium sets between two
    glyphs that follow each other in the stream is kept too, where the gap is narrower than a word
    space: of the in Hannon's §4.2, 0.07 em apart on a tight justified line, and Section 2
    presents, 0.22 em on each side of the digit. A space after a letter carrying a mark is left
    to the gap alone. ciphers, where given, is the paper's PAPER_CIPHERS entry, and images its
    PAPER_IMAGES entry, each image it names set among the glyphs as the letters it draws. With
    overset, an apostrophe drawn over a letter is set on it as the comma above, with underlined
    a rule drawn under a letter as the macron below, and with struck a rule drawn through a letter
    as the long stroke overlay. With tracked, a small capital's gap is measured against its row's
    size, a gap beside a Menlo modifier letter is weighed against its wide advance, and a space the
    stream sets inside a bracket or brace is kept. lifted, where given, is filled with each
    page's raised letters, {page index: {index in the text page: modifier}}, and each is written
    as its modifier where it stands: the raised j of -kʲ in Black's table (32) stands over a row
    of its own, and the stream sets the table's cells in another order than its rows, and aligning
    the stream to the rows afterward misses it. Its "lines" holds a defects row to each line given
    a raised letter. With ruled, each ruled grid of a page is read by ruled_tables and set where its
    rows stand, a line to a table row. stream_faces names the faces whose word spaces the stream
    alone sets, a paper's PAPER_STREAM_FACES entry."""
    pages = []
    scale = INK_DPI / 72.0
    # The gap an f's overhang leaves before the next letter of its word stays under 0.08 em, of the
    # apart; a share set over the calibration's 0.18 ceiling for a loose face loosens it by half.
    after_f = 0.08 if share <= 0.18 else share / 2
    for number in range(len(document)):
        textpage = document[number].get_textpage()
        image = document[number].render(scale=scale).to_pil().convert("L") if read_marks else None
        page_height = document[number].get_height()
        glyphs = []
        last = None
        space_before = False
        space_left = 0.0
        after_mark = False
        tie = False
        late = set()
        overprint = False
        clips = {}
        raised_at, unlifted = {}, {}
        if lifted is not None:
            raised_letters(textpage, ciphers=ciphers, found=raised_at, mark_base=mark_base)
            lifted[number] = raised_at
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            # A letter past U+FFFF, the math italic 𝜆 and 𝑥 of Mellesmoen's formulas in Cambria Math,
            # takes two indexes, one per surrogate, and each alone reads as nothing. The first is read
            # with the second; the second stays empty and is passed over.
            if not symbol and 0xD800 <= pdfium_c.FPDFText_GetUnicode(textpage, index) < 0xDC00:
                symbol = textpage.get_text_range(index, 2)
            # A space a cipher's font draws with ink is a letter of that font, Galloway's =.
            drawn = symbol == " " and ciphers and drawn_space(textpage, index) and \
                deciphered(symbol, textpage, index, ciphers) != " "
            # pdfium sets a space of no width after a mark the stream carries late, x a s ̣ í l̓ for
            # x̣asíl̓ in Mellesmoen's (3d); the gap alone decides whether a word space stands there.
            # A space pdfium made up is marked "made", one the stream carries True.
            if symbol == " " and not drawn and not after_mark:
                space_before = "made" if pdfium_c.FPDFText_IsGenerated(textpage.raw, index) else True
                space_left = textpage.get_charbox(index)[0]
            after_mark = False
            if not symbol or (symbol in ("\r", "\n", " ") and not drawn):
                continue
            # pdfium gives U+FFFE for the hyphen of CVC- and n-.
            if symbol == "￾":
                symbol = "-"
            # Each glyph is placed by its box below, which puts a ciphered run back in order.
            coded = symbol
            symbol = raised_at.get(index) or deciphered(symbol, textpage, index, ciphers)
            # A tie bar a cipher's font sets before the two letters it joins, the > of Tammpere,
            # Birdstone and Wiltschko's t͡s, stands over both, raised above the line. It goes after
            # the first letter the stream sets next. A U+0361 the stream itself carries already
            # stands in Unicode order and takes the mark reading below.
            if symbol == "͡" and coded != symbol:
                tie = True
                continue
            box = textpage.get_charbox(index)
            if clipped(textpage, index, box, clips):
                continue
            size = glyph_size(textpage, index)
            mark = misread_mark(symbol, box, size, last) if read_marks else None
            if mark or (len(symbol) == 1 and unicodedata.combining(symbol)):
                mark = mark or symbol
                if mark != symbol:
                    marks.append(symbol)
                middle = (box[0] + box[2]) / 2
                # A mark set above its letter stands over the letter's bottom. The comma above right
                # of Janzen's t̕ and k̕ in the Dawson wordlist stands past its letter's box, its middle
                # over a letter of the line above, whose bottom is higher than the mark's, or at the
                # foot of that line's g; the letter before it in the stream takes it.
                above = unicodedata.combining(mark[0]) >= 230
                under = [one for one in glyphs if one[1][0] <= middle <= one[1][2]
                         and abs(one[1][1] - box[1]) < size and not (above and one[1][1] >= box[1])]
                # With drawn_back the stream sets a line's DejaVu glyphs after its Times ones, and
                # the letter before a mark in the stream can stand anywhere on the line: the comma
                # above right of Lyon's k̕ʷúl follows the stop that ends the line. Where no letter
                # stands under the mark, the letter ending nearest before its middle, within 0.4
                # em on its line, takes it. The comma above right stands past its letter and over
                # the next, t̕uk̕ʷ in Lyon's (11a) with its middle over the u, and always goes to
                # the letter before it.
                if drawn_back and (not under or mark == "̕"):
                    under = sorted((one for one in glyphs if abs(one[1][1] - box[1]) < size and
                                    -0.05 * size <= middle - one[1][2] < 0.4 * size and one[1][2] > one[1][0]),
                                   key=lambda one: one[1][2])[-1:]
                target = under[-1] if under else (glyphs[-1] if glyphs else None)
                if target is not None:
                    target[0] += mark
                    # A mark the stream carries after other glyphs, the caron of *t'əx̌ is at the end
                    # of its line in Denzer-King's §3.1, leaves the letter's own space in the stream.
                    if target is not glyphs[-1]:
                        late.add(target[4])
                        # pdfium makes up a space before such a mark as well: gígiȷ̉atsáɢa in
                        # Black's (14a) streams as á, a space, ̉ and ɢ. That space goes.
                        if space_before == "made":
                            space_before = False
                after_mark = True
                continue
            if read_marks and accent_lost(symbol, box, size):
                marks.append("´")
                symbol = unicodedata.normalize("NFC", symbol + ACUTE)
            # An i after f can sit in the fi ligature, whose hook reaches over the i.
            elif read_marks and symbol == "i" and not (glyphs and glyphs[-1][0][-1:] == "f") and \
                    acute_on_i(image, box, size, page_height, scale):
                marks.append("´")
                symbol = "í"
            if tie:
                symbol, tie = symbol + "͡", False
            # A bold faked by printing a word twice, Upper Chehalis heading Denzer-King's (5) with its
            # second print 0.024 em right of the first: a letter over the same letter less than 0.05
            # em away is the second print and is passed over. Two letters set side by side stand
            # an advance apart, 0.2 em or more. The letters of a ligature share its box and follow
            # each other in the stream, the ff of suffixes in Jantzen's introduction; the glyph just
            # before is never the first print. While a second print is being passed over, the glyph
            # last kept is not the one before in the stream.
            if box[2] > box[0] and any(one[0] == symbol and abs(one[1][0] - box[0]) < 0.05 * size and
                                       abs(one[1][1] - box[1]) < 0.05 * size
                                       for one in (glyphs[-60:] if overprint else glyphs[-60:-1])):
                space_before, overprint = False, True
                continue
            overprint = False
            # With drawn_back, a space the stream sets before a glyph but draws past that glyph's
            # right edge stands after it: Lyon's 2010 Word export sets iʔ as i, a space, and a
            # DejaVu ʔ drawn back over the space, 0.04 em after the i. pdfium makes up a space of
            # its own where the stream steps back, past the ʔ of sámaʔ in (4); that one goes too.
            if drawn_back and space_before and space_left >= box[2] - 0.05 * size:
                space_before = False
            glyphs.append([symbol, box, size, font_name(textpage, index), len(glyphs), space_before])
            if index in raised_at:
                unlifted[id(glyphs[-1])] = deciphered(coded, textpage, index, ciphers)
            space_before = False
            if box[2] > box[0]:
                last = box
        for letters, box, size in image_glyphs(document[number], images, number):
            glyphs.append([letters, box, size, "image", len(glyphs), False])
        if overset:
            glyphs = over_letters(glyphs)
        if underlined:
            under_rules(document[number], glyphs)
        if struck:
            strike_rules(document[number], glyphs)
        # Rows by the glyphs' vertical middles, taken top down: within a line the middles of the
        # letters, the descenders, the parentheses and the = sit close together, and the next line
        # starts after a gap of more than 0.3 em.
        rows = []
        for glyph in sorted(glyphs, key=lambda one: -(one[1][1] + one[1][3]) / 2):
            middle = (glyph[1][1] + glyph[1][3]) / 2
            if rows and rows[-1][0] - middle < 0.3 * glyph[2]:
                rows[-1][0] = middle
                rows[-1][1].append(glyph)
            else:
                rows.append([middle, [glyph]])
        # A row of quote marks alone is the marks of the row beside it: the quotes round a gloss
        # set in small capitals stand clear of its letters, ‘1sg’ in Brown, Forbes and Schwan's
        # (48c), and so do the apostrophes of ’waa-’nu in their footnote 17. Each such row joins
        # the neighbor whose middle is nearer. The ties of enclisis do the same, set low between
        # the words they join: kaᴗcut-iɬᴗc’akʷ in Nater's 2013 §3.
        for place in range(len(rows) - 1, -1, -1):
            middle, members = rows[place]
            if not all(one[0] in "‘’'ʼᴗ" for one in members):
                continue
            beside = [one for one in (place - 1, place + 1) if 0 <= one < len(rows)
                      and not all(glyph[0] in "‘’'ʼᴗ" for glyph in rows[one][1])]
            if not beside:
                continue
            nearest = min(beside, key=lambda one: (abs(rows[one][0] - middle), -one))
            rows[nearest][1].extend(members)
            del rows[place]
        # A row of stops alone is the stop of a row whose other glyphs stand taller: the period of
        # 2. at the head of a line in van Eijk, its middle 3.2 points under the digit's. It joins the
        # neighbor whose baseline it sits on, within 0.2 em.
        for place in range(len(rows) - 1, -1, -1):
            middle, members = rows[place]
            if not all(one[0] in ".,;:" for one in members):
                continue
            bottom = min(one[1][1] for one in members)
            size = max(one[2] for one in members)
            beside = [one for one in (place - 1, place + 1) if 0 <= one < len(rows) and
                      abs(sorted(glyph[1][1] for glyph in rows[one][1])[len(rows[one][1]) // 2] - bottom) < 0.2 * size]
            if not beside:
                continue
            nearest = min(beside, key=lambda one: (abs(rows[one][0] - middle), -one))
            rows[nearest][1].extend(members)
            del rows[place]
        # A row of a note's mark alone is the mark raised over the row under it: the * set high
        # after the title Orbital Clitics in Nxaʔamxčín and before the note it opens, Deep gratitude,
        # in Lyon and Czaykowska-Higgins, or a number set smaller than the row's letters. It joins
        # that row where it stands within an em of it.
        for place in range(len(rows) - 2, -1, -1):
            middle, members = rows[place]
            below_middle, below = rows[place + 1]
            size = sorted(one[2] for one in below)[len(below) // 2]
            text = "".join(one[0] for one in members)
            if not (re.fullmatch(r"[*∗†‡]{1,3}", text) or
                    re.fullmatch(r"\d{1,2}(?:,?\d{1,2}){0,2}", text) and max(one[2] for one in members) < 0.85 * size):
                continue
            if middle - below_middle < size:
                below.extend(members)
                del rows[place]
        # A row of small raised letters alone is the raised letters of the row under it: the ʷ of
        # kxʷans, cǝkʷlátǝn and qʷáy in Davis and Van Eijk's bird names stands clear of its row
        # where no tall letter of that row reaches up to it. It joins the row under it where it
        # stands within an em over it, set smaller than that row's letters. Only a w or an h joins,
        # the h the aspiration of [tɕɪpʰamej:ʊ̀χda] in Black's (39b), or a letter already written as
        # its modifier, the ʲ of -kʲ in Black's (32): a raised digit or suffix standing alone
        # (Crowgey's 331, Thompson's rd) is read where it stands.
        for place in range(len(rows) - 2, -1, -1):
            middle, members = rows[place]
            below_middle, below = rows[place + 1]
            size = sorted(one[2] for one in below)[len(below) // 2]
            if not all(one[0] in "wh" or one[0] in MODIFIER_LETTERS for one in members) or \
                    max(one[2] for one in members) >= 0.85 * size:
                continue
            if middle - below_middle < size:
                below.extend(members)
                del rows[place]
        def standing(glyph):
            """Where glyph stands in its row, its box's left edge. An opening quote goes before the
            glyph after it in the stream where that glyph's box starts left of the quote's and
            reaches over it: the f of ‘feed’ in Robertson's tables starts 0.5pt left of its quote."""
            following = glyphs[glyph[4] + 1] if glyph[0] in "‘“" and glyph[4] + 1 < len(glyphs) else None
            if following is not None and following[1][0] < glyph[1][0] < following[1][2] and \
                    abs(following[1][1] + following[1][3] - glyph[1][1] - glyph[1][3]) < glyph[2]:
                return following[1][0] - 0.01
            return glyph[1][0]

        page_lines = []
        tables = ruled_tables(document[number]) if ruled else []
        written = set()

        def grid_of(lowest):
            return next((which for which, (xs, ys, _) in enumerate(tables) if ys[-1] < lowest < ys[0]), None)

        # The glyphs of a grid's rows that stand left or right of the grid go on the line of the
        # grid row whose height they stand in: the number (3) beside the head of Mellesmoen's 2017
        # tableau, and the pointing hand beside its winner.
        flanking = {}
        for lowest, members in rows:
            inside = grid_of(lowest)
            if inside is None:
                continue
            xs, ys, _ = tables[inside]
            for glyph in members:
                middle_x, middle_y = (glyph[1][0] + glyph[1][2]) / 2, (glyph[1][1] + glyph[1][3]) / 2
                if xs[0] < middle_x < xs[-1]:
                    continue
                row = max((one for one in range(len(ys) - 1) if ys[one] >= middle_y), default=0)
                flanking.setdefault((inside, row, middle_x > xs[-1]), []).append(glyph)

        def outside(inside, row, right):
            glyphs = sorted(flanking.get((inside, row, right), []), key=lambda one: one[1][0])
            return "".join((" " if place and glyph[5] else "") + glyph[0] for place, glyph in enumerate(glyphs))

        for lowest, members in rows:
            inside = grid_of(lowest)
            if inside is not None:
                if inside not in written:
                    written.add(inside)
                    page_lines.extend("   ".join(one for one in (outside(inside, at, False), ruled_line(row),
                                                                  outside(inside, at, True)) if one)
                                      for at, row in enumerate(tables[inside][2]))
                continue
            members.sort(key=standing)
            text = []
            # The letter each raised one written as its modifier stands for, by its place in text,
            # for the defects row.
            swapped = {}
            previous = None
            row_size = max(one[2] for one in members)
            for place, (symbol, box, size, font, order, spaced) in enumerate(members):
                if previous is not None:
                    gap = box[0] - previous[1][2]
                    # A capital set smaller than its row's letters, a word processor's small
                    # capital, keeps the row's letter spacing: the O B L of Sardinha's 2011 glosses
                    # stands 1.1 to 1.3pt apart, 0.16 em of its 7.92pt and 0.13 of the row's 9.84.
                    # Its gap is measured against the row's size.
                    measure = row_size if tracked and symbol[0].isupper() and size < 0.9 * row_size else size
                    digit = symbol[0].isdigit() or previous[0][-1].isdigit()
                    # Where the slant changes the gap takes the word space too: Lyon and Davis set
                    # an italic Clinton against a roman a 0.08 em on, and s against an italic Cultus
                    # 0.10 em on, both inside a word. A narrower space there, marker ji in Brown's §3
                    # at 0.09 em and of Indigenous at 0.05, stands on the stream's space. A word can
                    # change weight inside it, japxwit with its extraction suffix bold.
                    # The box of an f reaches over the letter after it and a j's under the letter
                    # before it: -0.10 em inside after, 0.15 em between of the. The word space
                    # there is narrower than share, and so is the gap it has to beat.
                    # The box of U+2019 is narrower than its advance: t’aan in Brown's (16) sets a after
                    # it 0.15 em on, and a word after it 0.45.
                    # The box of a j also stops short of its advance: rejections in Hannon's §2
                    # sets e 0.12 em after the j, over a word space of 0.112.
                    # A 1 is narrow in a figure's advance: the raised 11 of Lyon and
                    # Czaykowska-Higgins's page 7 sets its 1s 0.27 em apart.
                    # The box of the Straight face's hyphen is narrower than its advance too: the
                    # 202 hyphens of Gerdts and Peter, Urbanczyk and Gerdts's 2010 paper stand 0.10
                    # to 0.18 em before the next letter with no space in the stream, and the renders
                    # print s-xʷənitəm̓-aʔł and š-niʔ-s closed. The face sets its letters loose as
                    # well, 0.15 to 0.16 em apart, and the justified abstract of Gerdts's 2010 paper
                    # tracks naʔət out to 0.18 em. No gap between two glyphs of the face without a
                    # space in the stream passes 0.18 em in the three papers.
                    plain = 0.3 if digit and "1" in (symbol[0], previous[0][-1]) else \
                        0.25 if digit or previous[0][-1] in "’ʼ" or \
                        ("Straight" in previous[2] and (previous[0][-1] == "-" or "Straight" in font)) else \
                        after_f if previous[0][-1] == "f" else 0.1 if symbol[0] == "j" else \
                        max(share, 0.2) if previous[0][-1] == "j" else share
                    limit = plain if plain == 0.25 else \
                        0.02 if previous[0][-1] == "f" and symbol[0].isupper() else plain
                    # A ’ closes a quote on the word before it, and opens a glottalized word a word
                    # space after it, hadiks ’nii’y in Brown's (1), 0.37 em.
                    closing = symbol[0] in ",.;:!?)]}”" or (symbol[0] == "’" and gap <= 0.25 * size)
                    # The box of ’, ∙, ʹ and ʼ also starts well into its advance: Robertson's tables
                    # set the ’ closing a gloss 0.24 to 0.33 em after its letter and hi∙lu∙'s dots
                    # 0.11 to 0.15, with no space in the stream. Where the stream sets none, a gap
                    # up to 0.35 em before one of them is no word space.
                    if symbol[0] in "’∙ʹʼ" and not spaced and gap <= 0.35 * size:
                        closing = True
                    # A spacing acute standing for a stressed vowel the form leaves unwritten, Rude's
                    # 2012 /p´qʷn/ and /s´wn/, draws its stroke to the right of its advance too.
                    # With lone_acute, where the stream sets no space, a gap up to 0.35 em before
                    # one is no word space. Lyon's and Lee's papers set ´ as a mark over the letter
                    # before it, and keep their gaps.
                    if lone_acute and symbol[0] == "´" and not spaced and gap <= 0.35 * size:
                        closing = True
                    # The stress marks ˈ and ˌ draw a hairline in the middle of their advance: Black's
                    # awiˈnag̱wił in (14) sets ˈ 0.17 em after the i and n 0.17 em after it, where its
                    # advance meets both. Where the stream sets no space, a gap up to 0.25 em on either
                    # side of one is no word space.
                    if (symbol[0] in "ˈˌ" or previous[0][-1] in "ˈˌ") and not spaced and gap <= 0.25 * size:
                        closing = True
                    # A modifier letter taken from a monospaced Menlo sits in the middle of an
                    # advance 0.6 em wide: the ʲ of [hʲi-] on Sardinha's 2011 page 26 stands 0.25 em
                    # after its h and 0.27 before its i. Where the stream sets no space, a gap up to
                    # 0.3 em on either side of one is no word space.
                    if tracked and (symbol[0] in MODIFIER_LETTERS and "Menlo" in font or
                            previous[0][-1] in MODIFIER_LETTERS and "Menlo" in previous[2]) and \
                            not spaced and gap <= 0.3 * size:
                        closing = True
                    # A face named in stream_faces sets every word space in the stream: the Calibri
                    # of Mellesmoen's footnote 8 on page 11 opens 0.14 em between the l and l of
                    # following, and her Times 0.14 em between the ] and - of [h]-epenthesis, where
                    # its word spaces stand 0.05 em apart. Between two glyphs of that face with no
                    # space in the stream, a gap under a column's is no word space. A glyph after a
                    # smaller one keeps the gap test: the stream sets no space after the raised 5
                    # opening footnote 5, where the page prints one. The two glyphs can be of two
                    # faces the paper names: Brown's Lucida Sans ː sits 0.14 em before the Times s
                    # of thoːst, over the paper's calibrated 0.127.
                    if not spaced and gap <= 0.35 * size and members[place - 1][2] >= 0.9 * size and \
                            any(face in font for face in stream_faces) and \
                            any(face in previous[2] for face in stream_faces):
                        closing = True
                    # A spaced ellipsis, prompts . . . . in Zenk's §1, sets each stop a word space
                    # from the one before it. A stop with another stop beside it takes the gap test.
                    beside = [one[0][0] for one in members[place - 1:place] + members[place + 1:place + 2]]
                    if symbol[0] == "." and "." in beside:
                        closing = False
                    # pdfium also sets a space after a combining mark the stream carries on its
                    # own, scw̓ éxmx and ƛ̓ ʔék, where the page has none. A letter whose mark came late
                    # keeps a space the stream itself carries after it, *t'əx̌ is in Denzer-King's
                    # §3.1; one pdfium made up there, spiʔx̣ éwt in Hannon's (25), goes.
                    streamed = stream_spaces and spaced and previous[3] == order - 1 and \
                        gap > 0.03 * size and (not unicodedata.combining(previous[0][-1]) or
                                               (previous[3] in late and spaced is True))
                    # The box of an f can reach over the whole word space after it: of pidgins,
                    # of London and of American in Robertson's references stand -0.05 to 0.02 em
                    # apart, where no f inside a word stands more than -0.03 em from its letter.
                    # There the stream's space goes.
                    if previous[0][-1] == "f" and spaced and previous[3] == order - 1 and gap > -0.06 * size:
                        streamed = True
                    # Between two glyphs of a face named in stream_faces, the space the stream itself
                    # carries stands whatever the gap: the italic f of Jules's returned for and off
                    # for in (55) and (45) reaches back over the space to the letter before it.
                    if spaced is True and previous[3] == order - 1 and \
                            any(face in font for face in stream_faces) and \
                            any(face in previous[2] for face in stream_faces):
                        streamed = True
                    # The box of a stop is narrower than its advance, and a stop with a stop or a
                    # letter after it takes the gap test of U+2019: the stops of built... on Lyon and
                    # Davis's page 13 stand 0.15 em apart and the s of stsut.s 0.12 em after its
                    # stop, where a word space after a stop measures 0.33 em at least. Between two
                    # stops the stream's space goes too.
                    if previous[0][-1] == "." and (symbol[0] == "." or symbol[0].isalpha()):
                        closing, limit = False, 0.25
                        streamed = streamed and symbol[0] != "."
                    # A brace or square bracket keeps the word space the stream sets inside it:
                    # Table 5 of Sardinha's 2011 paper prints { =ńd.s } and { = sgi }, the = 0.38 em
                    # after its { and the } 0.41 em after its i, and her (S.1) [ PREP + NP ].
                    braced = tracked and (symbol[0] in "]}" or previous[0][-1] in "[{") and spaced and \
                        gap > limit * measure
                    if gap > 1.2 * size:
                        text.append("   ")
                    elif (gap > limit * measure or streamed) and not closing and \
                            previous[0][-1] not in "([{‘“" or braced:
                        text.append(" ")
                        if slanted is not None and gap <= plain * size:
                            slanted.append((number + 1, "".join(text[:-1]).split()[-1],
                                            symbol, gap / size))
                if id(members[place]) in unlifted:
                    swapped[len(text)] = unicodedata.normalize("NFC", unlifted[id(members[place])] + symbol[1:])
                text.append(unicodedata.normalize("NFC", symbol))
                previous = (symbol, box, font, order)
            line = "".join(text)
            if swapped:
                lifted.setdefault("lines", []).append(
                    (number + 1, "raised letter set on the line",
                     "".join(swapped.get(at, piece) for at, piece in enumerate(text)).strip(), line.strip()))
            # An example's number the page sets lower than its first line, (1) beside ɬe
            # meʔmʔéw̓s, heads that line as the text layer has it.
            if re.fullmatch(r"\(\d+[a-z]?\)", line) and page_lines and \
                    not re.match(r"^\(\d+[a-z]?\)", page_lines[-1]):
                page_lines[-1] = line + "   " + page_lines[-1]
                continue
            page_lines.append(line)
        pages.append(page_lines)
    return pages


def union_spaces(line, spaced):
    """Add to spaced the gaps the text layer line has and it lost: like ɬɛn̓ in, where the glyph
    positions give ɬɛn̓in. A text layer gap after a combining mark with no letter under it is not
    the page's, ⃗ v for ⃗v, and neither is one beside a digit, string. 3 for the footnote mark of
    string.3, or before closing or after opening punctuation."""
    def gaps(text):
        found, count = set(), 0
        for letter in text:
            if letter.isspace():
                found.add(count)
            else:
                count += 1
        return found
    letters = [one for one in spaced if not one.isspace()]
    spaced_gaps = gaps(spaced)
    keep = spaced_gaps | {
        at for at in gaps(line) - spaced_gaps if 0 < at < len(letters)
        and not (unicodedata.category(letters[at - 1]).startswith("M")
                 and (at < 2 or at - 1 in spaced_gaps
                      or not unicodedata.category(letters[at - 2]).startswith("L")))
        and not unicodedata.category(letters[at]).startswith("M")
        and not letters[at].isdigit() and not letters[at - 1].isdigit()
        and letters[at] not in ",.;:!?)]’”" and letters[at - 1] not in "([‘“"}
    out = []
    for at, letter in enumerate(letters):
        if at in keep and at > 0:
            out.append(" ")
        out.append(letter)
    return "".join(out)


def open_columns(line, spaced):
    """Open in line the column gaps spaced shows as three spaces and line runs together:
    almost=1SG.SUBJ=EXCLget.forgotten in Davis's (11), where the page leaves 1.44 em before get."""
    wide, count, run = set(), 0, ""
    for letter in spaced:
        if letter.isspace():
            run += letter
            continue
        if len(run) >= 3:
            wide.add(count)
        run = ""
        count += 1
    opened, count, previous = [], 0, ""
    for letter in line:
        if not letter.isspace():
            if count in wide and count and not previous.isspace():
                opened.append("   ")
            count += 1
        opened.append(letter)
        previous = letter
    return "".join(opened)


def subtract_spaces(line, spaced):
    """Take out of the text layer line the gaps the glyph positions do not show, keeping the width
    of each one left: n ɬeʔkepmxcín demonstrative s is nɬeʔkepmxcín demonstratives on the page, and
    the three spaces between two cells of a tier stay three."""
    kept, count, run = [], 0, ""
    glyph_gaps = set()
    for letter in spaced:
        if letter.isspace():
            glyph_gaps.add(count)
        else:
            count += 1
    count = 0
    for letter in line:
        if letter.isspace():
            run += letter
            continue
        if run and (count in glyph_gaps or count == 0):
            kept.append(run)
        run = ""
        kept.append(letter)
        count += 1
    return "".join(kept)


def base_letters(text):
    """text's letters less their combining marks and spaces: the key a layer line and a glyph row
    share where the rows set a mark apart or drop it, ḵ ̕ux̱ -t̕sa̱ w in the layer and ḵux̱ -tsa̱w in the
    row of Sardinha's (2d)."""
    return "".join(one for one in unicodedata.normalize("NFD", text)
                   if not one.isspace() and not unicodedata.category(one).startswith("M"))


def subtract_base_spaces(line, spaced):
    """subtract_spaces counted on base letters: a space before a combining mark counts after the
    letter the mark belongs to, and the layer's marks are kept."""
    gaps, count = set(), 0
    for letter in unicodedata.normalize("NFD", spaced):
        if letter.isspace():
            gaps.add(count)
        elif not unicodedata.category(letter).startswith("M"):
            count += 1
    kept, count, run = [], 0, ""
    for letter in unicodedata.normalize("NFD", line):
        if letter.isspace():
            run += letter
            continue
        if run and (count in gaps or count == 0) and not unicodedata.category(letter).startswith("M"):
            kept.append(run)
        run = ""
        kept.append(letter)
        if not unicodedata.category(letter).startswith("M"):
            count += 1
    return unicodedata.normalize("NFC", "".join(kept))


MACRON_BELOW = "macron below read as a space"
COMMA_ABOVE_PAIR = "comma above read as two marks"
BAR, COMMA = chr(0x331), chr(0x315)


def italic_mark_words(textpage):
    """The words of one page whose italic marks the text layer misreads, each as (units, kinds): a
    unit is one letter with its marks, as the layer defines it, mended, and the letter's box.

    Sardinha's italic Kwak̕wala sets the macron below as a space glyph 0.6pt tall under the baseline,
    x ux for x̱ux̱, and the comma above as one glyph read as U+0331 U+0315 in one box, ṯ̕sud for
    t̕sud. A bar is a space glyph over 0.3pt and under 1.2pt tall whose top is below the bottom of the
    letter before it, and at least twice as wide as it is tall: a real space has no height, the
    upright tiers' macron is U+0331 itself, and Pincott's dot below, k̓lə̣́m in Table 8 of
    PincottICSNL60, is a space glyph 1.2pt square. pdfium breaks the line before a mark glyph inside a word, t ̓sux̱wa, which the layer does not. A
    comma the stream sets after later letters, tsu ̱̕ x̱wit̕id in Sardinha's note 8, belongs to the
    last letter that ends before it, and the space pdfium sets back after it is no word break."""
    count = textpage.count_chars()
    glyphs = [(textpage.get_text_range(index, 1), textpage.get_charbox(index)) for index in range(count)]

    def pair_at(index):
        return index + 1 < count and glyphs[index][0] == BAR and glyphs[index + 1][0] == COMMA and \
            glyphs[index][1] == glyphs[index + 1][1]

    words, units, kinds, letter = [], [], [], None

    def close():
        if kinds:
            words.append((list(units), list(kinds)))
        del units[:], kinds[:]

    def mark(layer, mended, kind=None, unit=-1):
        if not units:
            units.append(["", "", None])
        units[unit][0] += layer
        units[unit][1] += mended
        if kind and kind not in kinds:
            kinds.append(kind)

    index = 0
    while index < count:
        symbol, (left, bottom, right, top) = glyphs[index]
        if symbol in "\r\n":
            after = index
            while after < count and glyphs[after][0] in "\r\n":
                after += 1
            if not (after < count and unicodedata.category(glyphs[after][0]).startswith("M")):
                close()
                letter = None
            index = after
            continue
        if symbol == " " and letter is not None and 0.3 < top - bottom < 1.2 and top < letter[1] and \
                right - left >= 2 * (top - bottom):
            mark(" ", BAR, MACRON_BELOW)
            # A bar that ends a word stands for its space too, x ux Simonx ‘Simon in pdfium's
            # stream: the word ends where the next glyph stands clear of the barred letter.
            if index + 1 < count and glyphs[index + 1][1][0] - letter[2] > 1.5:
                close()
            letter = None
            index += 1
        elif symbol == " " and index and glyphs[index - 1][0] == COMMA and units and \
                units[-1][2] and left < units[-1][2][0]:
            index += 1
        elif symbol.isspace():
            close()
            letter = None
            index += 1
        elif pair_at(index):
            target = len(units) - 1
            if units and units[-1][2] and left < units[-1][2][0]:
                target = max([number for number, one in enumerate(units)
                              if one[2] and one[2][2] <= left + 1] or [target])
            mark(BAR + COMMA, COMMA, COMMA_ABOVE_PAIR, target)
            index += 2
        elif unicodedata.category(symbol).startswith("M"):
            mark(symbol, symbol)
            index += 1
        else:
            letter = (left, bottom, right, top) if symbol.isalpha() else None
            units.append([symbol, symbol, letter])
            index += 1
    close()
    return words


def mend_italic_marks(lines, words, page):
    """lines, one page of the text layer, with each word of words mended where the layer holds its
    letters in order, taken in reading order. A space the layer sets between two letters where the
    glyphs have none, no g adalat for nog̱adalat, is kept for the closeup to weigh. A word that
    begins or ends at a letter or a bar must not run on into a letter. Returns the lines and a
    defects row to each repair."""
    decomposed = [unicodedata.normalize("NFD", one) for one in lines]
    changed, repairs = set(), []
    at_line, at_column = 0, 0

    def clear(text, column):
        return column < 0 or column >= len(text) or \
            not (text[column].isalpha() or unicodedata.category(text[column]).startswith("M"))

    for units, kinds in words:
        spelled = [unicodedata.normalize("NFD", one[0]) for one in units]
        pattern = re.compile("( ?)".join(re.escape(one) for one in spelled))
        opens = spelled[0][:1].isalnum()
        ends = spelled[-1][-1:].isalnum() or spelled[-1][-1:] in " " + BAR + COMMA
        found = None
        for start_line, start_column in ((at_line, at_column), (0, 0)):
            for number in range(start_line, len(decomposed)):
                text = decomposed[number]
                column = start_column if number == start_line else 0
                for match in pattern.finditer(text, column):
                    if (not opens or clear(text, match.start() - 1)) and \
                            (not ends or clear(text, match.end())):
                        found = (number, match)
                        break
                if found:
                    break
            if found:
                break
        if found is None:
            continue
        number, match = found
        mended = units[0][1] + "".join(space + one[1] for space, one in zip(match.groups(), units[1:]))
        text = decomposed[number]
        decomposed[number] = text[:match.start()] + mended + text[match.end():]
        changed.add(number)
        at_line, at_column = number, match.start() + len(mended)
        for kind in kinds:
            repairs.append((page, kind, unicodedata.normalize("NFC", match.group(0)),
                            unicodedata.normalize("NFC", mended)))
    return [unicodedata.normalize("NFC", decomposed[number]) if number in changed else line
            for number, line in enumerate(lines)], repairs


def write_rows(stem, by_page, document, lifted=None):
    """Write a page text read by glyph rows, with the .rows file residue.paper_repair looks for.
    lifted is row_lines's record of the raised letters it wrote. Returns the file and the lines
    given back a raised letter."""
    merged = []
    for number, page_lines in enumerate(by_page):
        merged.append("===== page %d =====" % (number + 1))
        merged.extend(page_lines)
    merged, raised = raise_letters(document, merged, PRIVATE_USE.get(stem), PAPER_CIPHERS.get(stem), lifted,
                                   mark_base=stem in PAPER_MARK_BASE, lowered=stem in PAPER_SUBSCRIPTED)
    os.makedirs(os.path.join(PRIVATE, "pagetext"), exist_ok=True)
    target = os.path.join(PRIVATE, "pagetext", stem + ".txt")
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(merged))
    # Every space here is a gap between glyphs, and residue.paper_repair reads this file to
    # leave the space after a stacked mark open, xin̓ te.
    with open(os.path.join(PRIVATE, "pagetext", stem + ".rows"), "w", encoding="utf-8") as handle:
        handle.write("spaces set from glyph positions\n")
    # A .layer an earlier closeup read left is no longer this text's.
    layer_file = os.path.join(PRIVATE, "pagetext", stem + ".layer")
    if os.path.isfile(layer_file):
        os.remove(layer_file)
    return target, raised


def drop_other_readings(stem):
    """Remove the .rows and .layer files an earlier rows or closeup read of stem left: residue reads
    a page text with a .rows file as read by glyph rows, and one with a .layer file as closed up, and
    a default read is neither. Mayer's VOT paper kept a .rows past a default read."""
    for suffix in (".rows", ".layer"):
        stale = os.path.join(PRIVATE, "pagetext", stem + suffix)
        if os.path.isfile(stale):
            os.remove(stale)


def join_broken_lines(layer, by_letters, spaced_join=False, columns=None):
    """The layer can break a line the glyphs set on one row: Dery / le Lonsdale in Lonsdale and
    Matsushita's byline, <pre / f-asp2> in their Figure 2, Figu / re 6, and in Stewart's (46) the
    gloss throw= / OBJ=DET. Two layer lines are one where their letters together make a glyph line
    and neither makes one alone. With spaced_join the two meet with a space where the glyph row
    sets one between them, no / NEG=1P.POSS in Stewart's (7); without it they meet with none.
    columns maps the letters of a glyph row to the row read with wide gaps (gap_lines with wide and
    held_wide),
    and two lines a column's gap parts on the row stay two: the captions (a) and (b) of Menon's
    side-by-side figures. Returns the lines and the repairs."""
    joined, repairs, page = [], [], 0
    for line in layer:
        marker = re.match(r"^===== page (\d+) =====$", line)
        if marker:
            page = int(marker.group(1))
        key = "".join(line.split())
        before = "".join(joined[-1].split()) if joined else ""
        if key and before and not marker and not re.match(r"^===== page", joined[-1]) and \
                before + key in by_letters and before not in by_letters and key not in by_letters:
            row = by_letters[before + key]
            # The glyph row's letter count up to the join, and whether a space follows it there.
            count, at = 0, 0
            while count < len(before):
                count += not row[at].isspace()
                at += 1
            gap = " " if spaced_join and at < len(row) and row[at].isspace() else ""
            wide = (columns or {}).get(before + key)
            if wide is not None:
                count, at = 0, 0
                while count < len(before):
                    count += not wide[at].isspace()
                    at += 1
                if wide[at:at + 3] == "   ":
                    joined.append(line)
                    continue
            both = joined[-1].rstrip() + gap + line.lstrip()
            repairs.append((page, "line broken inside a word" if not gap else "line broken inside a row",
                            joined[-1] + " / " + line, both))
            joined[-1] = both
            continue
        joined.append(line)
    return joined, repairs


def main():
    stem = sys.argv[1]
    # A typed page scanned to an image has a text layer of OCR that holds none of the orthography.
    # Its page text is transcribed from the scan by a person, and nothing here reads it again.
    if stem in TRANSCRIBED_FROM_SCAN:
        raise SystemExit("%s: the page text is transcribed from the scan, and page_text.py leaves it" % stem)
    document, mended, _ = paper_document(stem)
    # A font that gives letters a space in ToUnicode is read with the letters its program maps.
    # Its rows go under a name of their own, since each reading below replaces page_text.py's rows.
    defects.write(stem, "page_text.py fonts", [(0, "ToUnicode gives glyphs a space, mended from the font program",
                                                one, "") for one in mended])
    # A paper whose text layer sets each word of an example over its gloss, dim / PROSP / hadiks /
    # swim in Brown's (1), is read by glyph rows on request: python page_text.py <stem> rows. The
    # word-space threshold is the paper's own, from its gaps inside words.
    # A text layer that puts spaces inside words, associa te and C hinook in Zenk's §1, while
    # the glyph rows break words the layer holds whole, mamuk- ˈhihi at a stress mark's narrow box,
    # is read on request as python page_text.py <stem> closeup: each layer line keeps its own
    # spacing less the gaps its glyph row does not show. No space is added, and a layer line with
    # no glyph row of the same letters stays as the layer has it.
    if sys.argv[2:3] == ["closeup"]:
        share = calibrated_share(document)
        by_letters, by_base = {}, {}
        for page_lines in row_lines(document, [], share=share, read_marks=False, slanted=[],
                                    stream_spaces=True):
            for one in page_lines:
                by_letters.setdefault(unicodedata.normalize("NFC", "".join(one.split())), one)
                by_base.setdefault(base_letters(one), one)
        layer = layer_text(stem).split("\n")
        merged, repairs, unmatched, page = [], [], 0, 0
        # An italic mark the layer reads as a space or as two marks, x ux and ṯ̕sud in Sardinha's
        # Kwak̕wala, is mended page by page from the glyphs before the lines meet the rows.
        pages = [(0, [])]
        for line in layer:
            marker = re.match(r"^===== page (\d+) =====$", line)
            if marker:
                pages.append((int(marker.group(1)), []))
            pages[-1][1].append(line)
        layer = pages[0][1]
        for number, lines in pages[1:]:
            lines, found = mend_italic_marks(lines, italic_mark_words(document[number - 1].get_textpage()),
                                             number)
            layer += lines
            repairs += found
        # A paper closed up before closeup joined broken lines keeps its line numbers, its ops keyed
        # to them: Reisinger's modality paper, its table row k̓ ʷa / PAST T. P.
        if stem not in CLOSEUP_UNJOINED:
            layer, found = join_broken_lines(layer, by_letters, spaced_join=True)
            repairs += found
        kept = []
        for line in layer:
            marker = re.match(r"^===== page (\d+) =====$", line)
            if marker:
                page = int(marker.group(1))
                merged.append(line)
                continue
            # A layer that defines á as a and a combining acute, St' át'imcets in Davis's
            # introduction, meets the glyph row's á once both are composed.
            if unicodedata.normalize("NFC", line) != line and \
                    unicodedata.normalize("NFC", "".join(line.split())) in by_letters:
                line = unicodedata.normalize("NFC", line)
            spaced = by_letters.get("".join(line.split()))
            closer = subtract_spaces
            # A glyph row that sets a mark apart from its letter or drops it, ḵ ̕ux̱ in the layer and
            # ḵux̱ in the row of Sardinha's second paper, meets the layer line on base letters.
            if spaced is None and line.strip():
                spaced, closer = by_base.get(base_letters(line)), subtract_base_spaces
            if spaced is None:
                unmatched += bool(line.strip())
                if line.strip():
                    kept.append(line.strip())
                merged.append(line)
                continue
            closed = closer(line, spaced)
            if closed != line:
                repairs.append((page, "space inside a word", line.strip(), closed.strip()))
            merged.append(closed)
        merged, raised = raise_letters(document, merged)
        repairs += raised
        defects.write(stem, "page_text.py", repairs)
        os.makedirs(os.path.join(PRIVATE, "pagetext"), exist_ok=True)
        target = os.path.join(PRIVATE, "pagetext", stem + ".txt")
        with open(target, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(merged))
        rows_file = os.path.join(PRIVATE, "pagetext", stem + ".rows")
        if os.path.isfile(rows_file):
            os.remove(rows_file)
        # The lines kept as the layer has them, for residue.paper_repair: only these can hold the
        # layer's space after a stacked mark. A closed-up line's spaces are the glyph row's own,
        # =ox̱ =da in Sardinha's (6), and closing one there joins two words.
        with open(os.path.join(PRIVATE, "pagetext", stem + ".layer"), "w", encoding="utf-8",
                  newline="\n") as handle:
            handle.write("\n".join(one_glyph(one) for one in kept) + "\n")
        print("%s: %d layer lines closed up from the glyph rows, a word space over %.3f em; %d lines "
              "with no glyph row of the same letters kept as the layer has them"
              % (target, len(repairs), share, unmatched))
        return
    if sys.argv[2:3] == ["rows"]:
        share = PAPER_SHARE.get(stem) or calibrated_share(document)
        slanted, lifted = [], {}
        by_page = row_lines(document, [], share=share, read_marks=False, slanted=slanted, stream_spaces=True,
                            ciphers=PAPER_CIPHERS.get(stem), images=PAPER_IMAGES.get(stem),
                            overset=stem in PAPER_OVERSET, underlined=stem in PAPER_UNDERLINED,
                            struck=stem in PAPER_STRUCK, tracked=stem in PAPER_TRACKED,
                            lifted=lifted, lone_acute=stem in PAPER_LONE_ACUTE,
                            drawn_back=stem in PAPER_DRAWN_BACK, mark_base=stem in PAPER_MARK_BASE,
                            ruled=stem in PAPER_RULED, stream_faces=PAPER_STREAM_FACES.get(stem, ()))
        # A private-use glyph is written as the letter it draws here too, before the raised letters
        # are read: x̌ʷ in van Eijk's nax̌ʷít sets its ʷ after the private-use x̌.
        mapping = PRIVATE_USE.get(stem, {})
        by_page = [[lettered_glyphs(one, mapping) for one in page_lines] for page_lines in by_page]
        # A page set a quarter turn round, a wide table printed sideways, has its glyph rows run
        # across the table's columns; the text layer reads it in order, and that page keeps the
        # layer's lines.
        layer_pages = re.split(r"^===== page \d+ =====$", layer_text(stem), flags=re.M)[1:]
        turned = []
        for number in range(len(document)):
            textpage = document[number].get_textpage()
            angles = [abs(pdfium.raw.FPDFText_GetCharAngle(textpage.raw, index))
                      for index in range(0, textpage.count_chars(), 3)]
            angles = [min(one, abs(math.pi * 2 - one)) for one in angles if one >= 0]
            if angles and sorted(angles)[len(angles) // 2] > math.pi / 4 and number < len(layer_pages):
                by_page[number] = [one.rstrip() for one in layer_pages[number].strip("\n").split("\n")]
                turned.append((number + 1, "page turned a quarter round, kept as the text layer reads it", "", ""))
                # The layer's lines hold their raised letters flat, and raise_letters sets them.
                lifted.pop(number, None)
        pages_turned = {one[0] for one in turned}
        lines_lifted = [one for one in lifted.pop("lines", []) if one[0] not in pages_turned]
        target, raised = write_rows(stem, by_page, document, lifted)
        defects.write(stem, "page_text.py", [(0, "tiers set one cell to a line, read by glyph rows",
                                              "word space over %.3f em" % share, "")] +
                      slant_defects(slanted) + turned + lines_lifted + raised)
        print("%s: read by glyph rows, a word space over %.3f em; turned pages kept from the layer: %s"
              % (target, share, " ".join(str(one[0]) for one in turned) or "none"))
        return
    out = []
    for number in range(len(document)):
        out.append("===== page %d =====" % (number + 1))
        text = document[number].get_textpage().get_text_range()
        out.extend(one.rstrip() for one in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"))
    # pdfium keeps the spaces of running prose and drops the wide column gaps of an example, where
    # it gives ƛ̓ʊxʷegənč. for the text layer's ƛ̓ʊxʷegən č. Neither is the page everywhere. A text
    # layer line keeps its own spacing unless it holds a glued token, and then takes the pdfium
    # line with the same characters once spaces are set aside.
    # A font whose ToUnicode names the comma above as a letter leaves both the text layer and
    # pdfium's text wrong at every glottalized letter, ƛ ƛ qƛ əmcin for ƛ̓q̓əmcín. There the page is
    # read from the glyph positions alone, page by page, with each misread mark set right.
    marks = []
    for number in range(len(document)):
        gap_lines(OnePage(document, number), marks=marks)
    # The accents a lost acute adds count only once the misread marks show the font is at fault.
    if sum(1 for one in marks if one != "´") > 20:
        marks = []
        slanted = []
        by_page = row_lines(document, marks, slanted=slanted)
        merged = []
        for number, page_lines in enumerate(by_page):
            merged.append("===== page %d =====" % (number + 1))
            merged.extend(page_lines)
        merged, raised = raise_letters(document, merged)
        named = {COMMA_ABOVE: "comma above", ACUTE: "acute", DOT_BELOW: "dot below"}
        defects.write(stem, "page_text.py",
                      [(0, "%s read as a letter" % named[MARK_OF[one]], one, MARK_OF[one])
                       for one in sorted(set(marks)) if one in MARK_OF] +
                      [(0, "acute dropped from a vowel, or í given as i", "´",
                        "%d vowels" % marks.count("´"))] +
                      [(0, "marks set right, count", str(len(marks)), "")] +
                      slant_defects(slanted) + raised)
        os.makedirs(os.path.join(PRIVATE, "pagetext"), exist_ok=True)
        target = os.path.join(PRIVATE, "pagetext", stem + ".txt")
        with open(target, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(merged))
        # Every space here is a gap between glyphs, and residue.paper_repair reads this file to
        # leave the space after a stacked mark open, xin̓ te.
        with open(os.path.join(PRIVATE, "pagetext", stem + ".rows"), "w", encoding="utf-8") as handle:
            handle.write("spaces set from glyph positions\n")
        # A .layer an earlier closeup read left is no longer this text's.
        layer_file = os.path.join(PRIVATE, "pagetext", stem + ".layer")
        if os.path.isfile(layer_file):
            os.remove(layer_file)
        print("%s: the font names its marks as the letters %s; %d marks set right, the page read from "
              "the glyph positions" % (target, " and ".join(sorted(set(marks) & set(MARK_OF))), len(marks)))
        return
    # A private-use glyph is written as the letter it draws, in pdfium's lines as in the layer's.
    mapping = PRIVATE_USE.get(stem, {})
    out = [lettered_glyphs(one, mapping) for one in out]
    glyphs = [lettered_glyphs(one, mapping) for one in gap_lines(document)]
    by_letters = {}
    for one in out + glyphs:
        key = "".join(one.split())
        if len(one.split()) > len(by_letters.get(key, "").split()):
            by_letters[key] = one
    # pdfium's own lines by their letters, where it spaces them one way only.
    by_out = {}
    for one in out:
        by_out.setdefault("".join(one.split()), set()).add(" ".join(one.split()))
    by_out = {key: next(iter(found)) for key, found in by_out.items() if len(found) == 1}
    layer = layer_text(stem).split("\n")
    # The layer can drop a glyph pdfium keeps, the ∼ of reduplication in Huijsmans' det=prog∼sing-md
    # and [tə=wu∼wuw-əm]. A layer line without it takes pdfium's line of the same characters once
    # the glyph and the spaces are set aside, with the glyph put back where pdfium sets it. The layer
    # can leave a control character in its place, det=prog\x18sing-md, and that goes.
    kept = {}
    for one in out:
        if any(glyph in one for glyph in DROPPED):
            kept.setdefault(re.sub(r"[\s%s]" % DROPPED, "", one), set()).add(one)
    restored = []
    for at, line in enumerate(layer):
        plain = lettered_glyphs(re.sub(r"[\x00-\x08\x0b-\x1f]", "", line), mapping)
        found = kept.get(re.sub(r"[\s%s]" % DROPPED, "", plain))
        if found and len(found) == 1 and not any(glyph in plain for glyph in DROPPED):
            layer[at] = put_back(plain, next(iter(found)))
            restored.append(at)
        elif plain != line:
            layer[at] = plain
            restored.append(at)
    # A text layer can instead put a space at every change of font, n ɬeʔkepmxcín demonstrative s.
    # The paper shows which it does: count the lines the glyph positions space more than the layer
    # and the lines they space less. Where the second far outnumber the first, the layer's extra
    # gaps come out and none go in.
    by_glyphs = {}
    for one in glyphs:
        by_glyphs.setdefault("".join(one.split()), one)
    more = fewer = 0
    for line in layer:
        spaced = by_glyphs.get("".join(line.split()))
        if spaced is not None:
            more += len(spaced.split()) > len(line.split())
            fewer += len(spaced.split()) < len(line.split())
    inserted = fewer >= 20 and fewer > 3 * more
    if inserted:
        merged = []
        page = 0
        # The same layer can run two cells of a tier together where the page leaves a column's
        # gap between them.
        # A space at a change of font stays, EXCL get.forgotten.
        columns = {}
        for one in gap_lines(document, wide=True, fonts=True):
            columns.setdefault("".join(one.split()), lettered_glyphs(one, mapping))
        # Such a layer breaks a line at a change of font as well, go=1 / SG.SBJ go.home in Turner's
        # (1a), where the gloss sets its small capitals.
        spans = {}
        for one in gap_lines(document, wide=True, fonts=True, held_wide=True):
            spans.setdefault("".join(one.split()), lettered_glyphs(one, mapping))
        layer, repairs = join_broken_lines(layer, by_letters, spaced_join=True, columns=spans)
        for line in layer:
            marker = re.match(r"^===== page (\d+) =====$", line)
            if marker:
                page = int(marker.group(1))
            spaced = columns.get("".join(line.split()))
            if spaced is not None and len(spaced.split()) < len(line.split()):
                closed = subtract_spaces(line, spaced)
                repairs.append((page, "space inside a word", line.strip(), closed.strip()))
                line = closed
            wide = columns.get("".join(line.split()))
            if wide is not None:
                opened = open_columns(line, wide)
                if opened != line:
                    repairs.append((page, "two cells of a tier run together", line.strip(),
                                    opened.strip()))
                    line = opened
            merged.append(line)
        merged, raised = raise_letters(document, merged)
        repairs += raised
        defects.write(stem, "page_text.py", repairs)
        os.makedirs(os.path.join(PRIVATE, "pagetext"), exist_ok=True)
        target = os.path.join(PRIVATE, "pagetext", stem + ".txt")
        with open(target, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(merged))
        drop_other_readings(stem)
        print("%s: the text layer spaces inside words; %d lines closed up from the glyph positions"
              " (%d lines the glyphs space more, left alone); %d lines given back a glyph the layer"
              " dropped" % (target, len(repairs), more, len(restored)))
        return
    merged = []
    taken = 0
    layer, repairs = join_broken_lines(layer, by_letters)
    page = 0
    for line in layer:
        marker = re.match(r"^===== page (\d+) =====$", line)
        if marker:
            page = int(marker.group(1))
        # Of two printings of one line, the one with more spaces lost fewer of the page's gaps.
        spaced = by_letters.get("".join(line.split()))
        # The layer can also set a space inside a word or move one: most ex tensive, b y the and
        # 1Seeww w.tei-c.org in Lonsdale and Matsushita. Where pdfium's line and the glyph line read
        # the same letters alike, their spacing is the page's.
        agreed = by_out.get("".join(line.split()))
        glyph_line = by_glyphs.get("".join(line.split()))
        if spaced and len(spaced.split()) > len(line.split()):
            spaced = union_spaces(line, spaced)
            merged.append(spaced)
            taken += 1
            repairs.append((page, "glued words", line, spaced))
        elif agreed is not None and glyph_line is not None and agreed == " ".join(glyph_line.split()) and \
                agreed != " ".join(line.split()) and stem not in LAYER_SPACED:
            merged.append(agreed)
            taken += 1
            repairs.append((page, "space inside a word", line, agreed))
        else:
            merged.append(line)
    # A line whose marks pdfium sets in another order than the layer, scw̓ éxmx beside scw̓éxmx,
    # matches no pdfium line whole, and its glued tokens stay: information(YY)isabbreviatedasNV in
    # Hannon's footnote *. Each such token takes the spacing of the same characters in pdfium's
    # text of its page, where they stand once with only spaces between them.
    page_texts = {}
    for number in range(len(document)):
        text = document[number].get_textpage().get_text_range()
        page_texts[number + 1] = " ".join(lettered_glyphs(text, mapping).split())
    page = 0
    for index, line in enumerate(merged):
        marker = re.match(r"^===== page (\d+) =====$", line)
        if marker:
            page = int(marker.group(1))
            continue
        tokens = line.split(" ")
        changed = False
        for place, token in enumerate(tokens):
            if len(token) < 8 or token.startswith("http") or \
                    sum(1 for letter in token if letter.isascii() and letter.isalpha()) < 6:
                continue
            # Each run of plain letters is matched alone, nukʷasideforthe... and discus- carry a
            # mark and a line-end hyphen pdfium may set otherwise; a run pdfium prints more than
            # once, past temporal orientation, is taken where every printing is spaced alike.
            def respaced(run):
                pattern = r"\s?".join(re.escape(letter) for letter in run.group(0))
                found = {one.group(0) for one in re.finditer(pattern, page_texts.get(page, ""))}
                if len(found) == 1:
                    return found.pop()
                return run.group(0)
            # The whole token first, a line-end hyphen set aside, for a space next to a mark.
            whole = re.match(r"^(.*?)(-?)$", token)
            spaced = respaced(re.match(r"^.*$", whole.group(1))) + whole.group(2)
            if len(spaced.split()) == 1:
                spaced = re.sub(r"[A-Za-z’']{6,}", respaced, token)
            if spaced != token:
                tokens[place] = spaced
                changed = True
        if changed:
            respaced = " ".join(tokens)
            repairs.append((page, "glued words", line, respaced))
            merged[index] = respaced
    merged, raised = raise_letters(document, merged)
    repairs += raised
    defects.write(stem, "page_text.py", repairs)
    os.makedirs(os.path.join(PRIVATE, "pagetext"), exist_ok=True)
    target = os.path.join(PRIVATE, "pagetext", stem + ".txt")
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(merged))
    drop_other_readings(stem)
    left = sum(1 for line in merged for one in line.split()
               if len(one) > 22 and not one.startswith("http")
               and sum(1 for letter in one if letter.isascii() and letter.isalpha()) > 16)
    print("%s: %d text layer lines respaced from the glyph positions, %d glued tokens left, "
          "%d lines given back a glyph the layer dropped" % (target, taken, left, len(restored)))


if __name__ == "__main__":
    main()
