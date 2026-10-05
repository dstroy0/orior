"""The runs of italic text on each page, for the forms a paper cites in its prose by setting them in
italics: aa, nee dii and k'ap/ap in Hill and Matthewson, which are plain letters and read as English
to the engine.

usage: python italic_runs.py <stem> [first page] [last page]

A run is the italic glyphs between two roman ones, with the spaces inside it kept; one line per run,
page first. Punctuation and a dash at either end are set aside. cited_rows turns the runs that
stand in the prose into cited form rows for a context file's ADD.
"""
import os
import re
import unicodedata
import sys

import pypdfium2 as pdfium

import page_text
import tables

EDGES = " .,;:!?()[]‘’“”–"
# A paper that sets its forms upright in a face of their own has that face read as its italics:
# van Eijk's AboriginalSans against the AboriginalSerif of his prose. Its runs take the letters of
# the paper's private-use glyphs and write each raised letter as its modifier, as the page text does.
FORM_FACES = tables.gather("FORM_FACES")


def italic_runs(stem, first=1, last=None):
    """(page, run) for each italic run of the paper, in page order."""
    # The fonts a ToUnicode left blank are read mended, as page_text reads them.
    document = page_text.paper_document(stem)[0]
    runs = []
    face = FORM_FACES.get(stem)
    mapping = page_text.PRIVATE_USE.get(stem, {}) if face else {}
    ciphers = page_text.PAPER_CIPHERS.get(stem) if face else None
    for number in range(first - 1, last or len(document)):
        textpage = document[number].get_textpage()
        raised = set()
        if face:
            # The glyphs page_text.raised_letters raises, counted over the same glyphs.
            _, lifted = page_text.raised_letters(textpage, mapping, ciphers)
            at = 0
            for index in range(textpage.count_chars()):
                symbol = textpage.get_text_range(index, 1)
                if not symbol or symbol.isspace() or symbol == "￾":
                    continue
                if ciphers:
                    symbol = page_text.deciphered(symbol, textpage, index, ciphers)
                if at in lifted:
                    raised.add(index)
                at += len(mapping.get(symbol, symbol))
        # The stream breaks the line either side of a raised letter, set on a baseline of its own,
        # k̓ ʷ zús; the page sets it inside its word. The break after it stands for the word space as
        # well, n̓á:n̓atxʷ ʔác̓x̌-n-əm, and is kept where the next glyph stands more than 0.8 em past
        # the letter the raised one sits on: 10.3 points at a word space in van Eijk, 6.1 at most inside
        # a word.
        apart = set()
        for index in raised:
            for step in (-1, 1):
                near, breaks = index + step, []
                while 0 <= near < textpage.count_chars() and textpage.get_text_range(near, 1) in ("\r", "\n"):
                    breaks.append(near)
                    near += step
                base = index - 1
                while base >= 0 and textpage.get_text_range(base, 1) in ("\r", "\n"):
                    base -= 1
                if step == 1 and breaks and near < textpage.count_chars() and base >= 0:
                    size = pdfium.raw.FPDFText_GetFontSize(textpage, base)
                    if textpage.get_charbox(near)[0] - textpage.get_charbox(base)[2] > 0.8 * size:
                        continue
                apart.update(breaks)
        # A paper that draws the glottal mark as an apostrophe over its letter has it set on the
        # letter, as page_text does, and the apostrophe left out of the run with the space the stream
        # sets after it: stsq̓ey̓.
        marks, taken = page_text.overset_marks(textpage) if stem in page_text.PAPER_OVERSET else ({}, set())
        # Gerdts and Peter set a letter of a form in Times now and then, the i of ʔəmilyə in italic
        # and the t of tenəs upright: the letters of a word that holds a glyph of the face are the
        # form's. A footnote's mark on the word, and a space the face sets, are not. Where the paper
        # names two faces, Gerdts and Peter's Straight and italic, a run ends where the face changes:
        # boat > put is two forms. inside maps each glyph of a form to its face.
        inside = {}
        if face:
            count = textpage.count_chars()
            symbols = [textpage.get_text_range(index, 1) for index in range(count)]
            for index in range(count):
                found = re.search(face, page_text.font_name(textpage, index))
                if found and not symbols[index].isspace():
                    inside[index] = found.group(0)
            # A paper with one face of forms, van Eijk's AboriginalSans, sets every letter of a form
            # in it, and its runs are its glyphs.
            for index in sorted(inside) if "|" in face else ():
                for step in (-1, 1):
                    near = index + step
                    while 0 <= near < count and near not in inside and \
                            (symbols[near].isalpha() or unicodedata.combining(symbols[near][:1] or " ")):
                        inside[near] = inside[index]
                        near += step
            # A word's glyphs, a footnote's mark aside, take the face most of them are set in: the
            # italic i of ʔəmilyə is Straight's, and so is the italic ti:t of ti:t,tiʔtəm.
            start = 0 if "|" in face else count
            while start < count:
                end = start
                while end < count and not symbols[end].isspace():
                    end += 1
                faces = [inside[index] for index in range(start, end) if index in inside]
                # Two faces set as often go to the one the word opens on: the k of Lyon's 2011 k̓ʷúl-n
                # is italic and its comma above slanted TeX-xipa, the k̓ the stream sets apart.
                if faces:
                    most = max(faces, key=faces.count)
                    for index in range(start, end):
                        if index in inside and not symbols[index].isdigit():
                            inside[index] = most
                start = end + 1
        run, run_face, tie, last = [], None, False, None
        for index in range(textpage.count_chars()):
            symbol = textpage.get_text_range(index, 1)
            if index in taken or (index - 1 in taken and symbol == " "):
                continue
            symbol += marks.get(index, "")
            if index in apart:
                continue
            if symbol in ("\r", "\n", " "):
                if run:
                    run.append(" ")
                continue
            if face:
                coded = symbol
                if ciphers:
                    symbol = page_text.deciphered(symbol, textpage, index, ciphers)
                # A tie bar a cipher's font sets before the two letters it joins goes after the
                # first of them, as page_text sets it: maʔt͡s. The stream breaks the line either side
                # of the raised bar, and the break stands for no space where the letter after the
                # bar stands within 0.3 em of the letter before it.
                if symbol == "͡" and coded != symbol:
                    tie = True
                    continue
                if tie and run and run[-1] == " " and last is not None:
                    size = pdfium.raw.FPDFText_GetFontSize(textpage, last)
                    if textpage.get_charbox(index)[0] - textpage.get_charbox(last)[2] < 0.3 * size:
                        while run and run[-1] == " ":
                            run.pop()
                symbol = page_text.MODIFIER[symbol] if index in raised else mapping.get(symbol, symbol)
                if tie:
                    symbol, tie = symbol + "͡", False
            last = index
            font = page_text.font_name(textpage, index)
            if face and index in inside and run and inside[index] != run_face:
                text = " ".join("".join(run).split()).strip(EDGES + "—>")
                if text:
                    runs.append((number + 1, text))
                run = []
            # A glyph of a symbol font the writer shears with the italics round it, the Wingdings
            # arrow of Sardinha's 2011 ‘ya..’ → ‘hiýa…’, is a sign between two forms.
            if index in inside if face else page_text.italic(font) or \
                    stem in page_text.PAPER_SHEARED and page_text.sheared(textpage, index) and \
                    unicodedata.category(symbol[:1]) != "Co":
                run.append(symbol)
                run_face = inside.get(index)
                continue
            # The face of the forms sets the dash that opens a secondary derivation, U+2014 pálʔ-ac-min̓.
            text = " ".join("".join(run).split()).strip(EDGES + ("—" if face else ""))
            if text:
                runs.append((number + 1, text))
            run = []
        text = " ".join("".join(run).split()).strip(EDGES)
        if text:
            runs.append((number + 1, text))
    return runs


def cited_rows(stem, draft, language, skip=()):
    """ADD entries for the italic runs that stand as whole words in a prose note of the draft: one
    cited form row per run and place, where "footnote N" for a run in a footnote, glossed with the
    English in quotes the prose sets after it, Hasaga'y dimin gidaxin (‘I wanna ask you.’). A run
    in skip, or the same as a tier of an example, is left out."""
    tiers = {" ".join(one[3].split()) for one in draft if one[2] in ("transcription", "segmentation")}
    runs = italic_runs(stem)
    added, seen = [], set()
    for page, run in runs:
        if run in skip or run in tiers or run.startswith(("Context", "Comment")):
            continue
        pattern = re.compile(r"(?<![\w'’])%s(?![\w'’])" % re.escape(run))
        # The aa of Halaayin aa? is part of that run, and takes none of its English.
        longer = [one for number, one in runs if number == page and run in one and one != run]
        for where, who, kind, form, gloss in draft:
            if kind != "note" or where in ("references", "front") or where.startswith("(") or \
                    not re.search(r"\bpage %d\b" % page, gloss):
                continue
            covered = [(at.start(), at.end()) for one in longer
                       for at in re.finditer(re.escape(one), form)]
            found = next((one for one in pattern.finditer(form)
                          if not any(start <= one.start() and one.end() <= end for start, end in covered)),
                         None)
            if not found:
                continue
            numbered = re.match(r"^(\d{1,2}|\*) ", form) if "footnote" in gloss else None
            place = "footnote %s" % numbered.group(1) if numbered else where
            if (place, run.lower()) in seen:
                break
            seen.add((place, run.lower()))
            # The English can hold an apostrophe of its own, ‘No, I’m good.’
            english = re.match(r"[?.!]?\s*\(?\s*‘(.*?)’(?![A-Za-z])", form[found.end():])
            note = "page %d, italic in the prose" % page
            if english:
                note += ", ‘%s’" % english.group(1)
            added.append((place, (place, language, "cited form", run, note)))
            break
    return added


if __name__ == "__main__":
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    last = int(sys.argv[3]) if len(sys.argv) > 3 else None
    for page, run in italic_runs(sys.argv[1], first, last):
        print("%d\t%s" % (page, run))
