"""The ops of 2011_Noguchi: Masaki Noguchi's A preliminary study of intonation in Kwak'wala, on word
stress, phonological phrasing and default intonation, then post-focus Destress Given in answers to
subject and object Wh-questions.

Page text read by glyph rows, with the clipped draft under Figure 13 on page 19 left out and the
Wingdings arrow of (5) mapped in page_text.py. The spectrograms and F0 plots are images; each
figure's caption is a note, and the phrasing, the Kwak'wala lines and the bracketing the text layer
holds in Figures 6 to 11, 14 and 15 are rows of their own. Tables 1 to 6 are read cell by cell, the
cells as the rows set them, checked against renders of pages 3, 7, 21, 23 and 25.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Kwak’wala"
AUTHORS = ["Masaki Noguchi"]
paper = gen.Paper("2011_Noguchi", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Zec", "Draga Zec (1988, 1995), the Kwak’wala stress analysis"),
         ("Boas", "Franz Boas (1947), the Kwakiutl grammar"),
         ("Kalmar", "Mary Kalmar (2003), the non-moraic schwa"),
         ("Selkirk", "Elisabeth Selkirk, prosodic structure and End-based theory"),
         ("Truckenbrodt", "Hubert Truckenbrodt (1999), syntax-phonology mapping"),
         ("Anderson", "Stephen Anderson (1984, 2005), Kwak’wala clitics"),
         ("Rooth", "Mats Rooth (1992), Alternative Semantics"),
         ("Kiss", "Katalin Kiss (1998), identificational and information focus"),
         ("Lieberman", "Philip Lieberman et al. (1985), the declination line by regression"),
         ("Black", "Anna Black (2011), the determiner /da/")]
LANGUAGES = [(LANGUAGE, "Northern Wakashan, spoken on northern Vancouver Island and the adjoining mainland"),
             ("Oowekyala", "Northern Wakashan, the comparative forms of (2)"),
             ("Nɬeʔkepmxcin", "Salish, Koch (2008) on focus and intonation"),
             ("Musqueam", "Salish, Shaw et al. (1999) on stress"),
             ("Japanese", "the two phonological phrases of Beckman and Pierrehumbert"),
             ("English", "focus and deaccentuation"),
             ("Chichewa", "rephrasing after focus"),
             ("Italian", "the fixed nuclear pitch accent"),
             ("Spanish", "resisting deaccentuation"),
             ("Romanian", "resisting deaccentuation")]
OOWEKYALA = "Oowekyala"
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
PHRASING = re.compile(r"^\(.*\)\s?(?:IP|PPh|PWd)$")
CAPTION = re.compile(r"^Figure \d+\.")


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def caption(first, where):
    """A figure's caption, run on where it wraps, Figure 17's (O) constructions; the figure over it
    is an image the text layer does not hold."""
    label = re.match(r"^(Figure \d+)", paper.text(first)).group(1)
    parts, line = [paper.text(first)], after(first)
    while not CAPTION.match(paper.text(line)) and paper.text(line).startswith("("):
        parts.append(paper.text(line))
        line = after(line)
    paper.add(label, A, "note", " ".join(parts), "page %d, the caption under a figure the text layer does not hold"
              % paper.page(first))
    return line


# The Kwak'wala lines under the phrasing of each figure, a tier to each printed line; a line that
# wraps, Figure 10's g ʲ uk and house, runs onto the tier above it.
FIGURE_TIERS = {"6": ("segmentation",), "7": ("segmentation",), "8": ("segmentation",),
                "9": ("transcription", "segmentation", "gloss"),
                "10": ("transcription", "segmentation", "+", "gloss", "+"),
                "11": ("transcription", "segmentation", "gloss"),
                "14": ("transcription", "segmentation", "gloss"),
                "15": ("transcription", "segmentation", "gloss")}


def figure(first, where):
    """A figure of phrasing: the phrases of each level drawn as parentheses over the Kwak’wala line,
    the line split at its morphemes and glossed, the translation, and for Figures 6 to 8 the
    syntactic bracketing under it. The caption closes it."""
    end = paper.find(r"^Figure \d+\.", first)
    label = re.match(r"^Figure (\d+)", paper.text(end)).group(1)
    lines = []
    line = first
    while line < end:
        if printed(line):
            lines.append(line)
        line += 1
    phrasing = [one for one in lines if PHRASING.match(paper.text(one))]
    rest = [one for one in lines if one not in phrasing]
    tiers = FIGURE_TIERS[label]
    rows = []
    for one, kind in zip(rest, tiers):
        if kind == "+":
            rows[-1][1] += " " + paper.text(one)
        else:
            rows.append([kind, paper.text(one), paper.page(one)])
    under = rows[0][1]
    for one in phrasing:
        paper.add("Figure " + label, A, "note", paper.text(one),
                  "page %d, the phrasing drawn over the line under it: %s over %s"
                  % (paper.page(one), re.search(r"(IP|PPh|PWd)$", paper.text(one)).group(1), under))
    count = 0
    for kind, text, page in rows:
        count += 1
        why = "page %d" % page
        if label == "10" and kind in ("segmentation", "gloss"):
            why += ", wrapping to a second line, g ʲ uk and house"
        if label == "9" and kind == "gloss":
            why += ", Pat-CaseJohn-Case-Poss run together as printed"
        paper.add("Figure %s line %d" % (label, count), L, kind, text, why)
    for one in rest[len(tiers):]:
        text = paper.text(one)
        count += 1
        if text.startswith("‘"):
            paper.add("Figure %s line %d" % (label, count), A, "translation", text, "page %d" % paper.page(one))
        else:
            assert text.startswith("["), (label, text)
            paper.add("Figure " + label, A, "note", text,
                      "page %d, the syntactic bracketing under the line" % paper.page(one))
    paper.add("Figure " + label, A, "note", paper.text(end), "page %d, the figure's caption; the F0 contour is an image"
              % paper.page(end) if label not in ("6", "7", "8") else "page %d, the figure's caption" % paper.page(end))
    return after(end)


def figure_13(first, where):
    """Figure 13: three columns, each a kind of intonation over the category it marks and the
    phrasing drawn under it."""
    page = paper.page(first)
    columns = (("Default intonation", "Discourse-newness", "(   )(   )"),
               ("Stress Focus", "Contrastive focus", "(Focus)(   )"),
               ("Destress Given", "Discourse-givenness", "(Given)(   )"))
    for head, category, phrases in columns:
        paper.add("Figure 13", A, "note", head, "page %d, a column's head" % page)
        paper.add("Figure 13", A, "note", category, "page %d, under %s" % (page, head))
        paper.add("Figure 13", A, "note", phrases, "page %d, the phrasing drawn under %s" % (page, category))
    end = paper.find(r"^Figure 13\.", first)
    paper.add("Figure 13", A, "note", paper.text(end), "page %d, the figure's caption" % page)
    return after(end)


def two(first, where):
    """(2): an Oowekyala form beside its Kwak'wala form and the meaning, the columns headed by the
    language; the source stands at the top of the next page, under the footnote."""
    page = paper.page(first)
    line = after(first)
    for letter in "ab":
        found = re.match(r"^%s\. (\S+) (\S+) (‘[^’]*’)$" % letter, paper.text(line))
        assert found, paper.text(line)
        paper.add("(2%s) line 1" % letter, OOWEKYALA, "transcription", found.group(1), "page %d, under Oowekyala" % page)
        paper.add("(2%s) line 2" % letter, L, "transcription", found.group(2), "page %d, under Kwak’wala" % page)
        paper.add("(2%s) line 3" % letter, A, "translation", found.group(3), "page %d" % page)
        line = after(line)
    assert paper.text(line) == "(Bach et al. 2005)", paper.text(line)
    paper.add("(2)", A, "citation", paper.text(line), "page %d, the source, at the top of the page" % paper.page(line))
    return after(line)


def three(first, where):
    """(3): a noun, its meaning, its plural and the plural's meaning, side by side."""
    line = first
    for letter in "ab":
        text = re.sub(r"^\(3\)\s+", "", paper.text(line))
        found = re.match(r"^%s\. (\S+) (\S+) (\S+) (\S+)$" % letter, text)
        assert found, text
        page = paper.page(line)
        paper.add("(3%s) line 1" % letter, L, "transcription", found.group(1), "page %d, the noun" % page)
        paper.add("(3%s) line 2" % letter, A, "translation", found.group(2), "page %d, the noun's meaning" % page)
        paper.add("(3%s) line 3" % letter, L, "segmentation", found.group(3), "page %d, the plural, reduplicated" % page)
        paper.add("(3%s) line 4" % letter, A, "translation", found.group(4), "page %d, the plural's meaning" % page)
        line = after(line)
    return line


def five(first, where):
    """(5): the form with the oblique case marker, an arrow, and the form with the epenthetic t."""
    page = paper.page(first)
    found = re.match(r"^\(5\) (\S+) → (\S+)$", paper.text(first))
    assert found, paper.text(first)
    paper.add("(5) line 1", L, "segmentation", found.group(1), "page %d" % page)
    paper.add("(5) line 2", L, "transcription", found.group(2), "page %d, right of the arrow →" % page)
    line = after(first)
    paper.add("(5) line 3", A, "translation", paper.text(line), "page %d" % paper.page(line))
    line = after(line)
    paper.add("(5) line 4", A, "citation", paper.text(line), "page %d, the source under the translation" % paper.page(line))
    return after(line)


def focus(first, where):
    """(9) and (10): the construction named, a question and its answer each over its translation,
    the answer's phrases in parentheses, the focus bracketed, and what the example predicts at the
    right of the answer."""
    found = re.match(r"^\((\d+)\) (.+)$", paper.text(first))
    label = found.group(1)
    paper.add("(%s)" % label, A, "note", found.group(2), "page %d, the example's caption" % paper.page(first))
    line, count = after(first), 0
    for speaker in "QA":
        text = paper.text(line)
        assert text.startswith(speaker + " "), text
        body = text[2:]
        right = None
        if speaker == "A":
            parts = re.match(r"^(.+?) ((?:post-focus|No post-focus) Destress Given)$", body)
            assert parts, body
            body, right = parts.groups()
        count += 1
        paper.add("(%s) line %d" % (label, count), L, "transcription", body,
                  "page %d, the %s, %s" % (paper.page(line), "question" if speaker == "Q" else "answer", speaker))
        if right:
            paper.add("(%s)" % label, A, "note", right, "page %d, at the right of the answer" % paper.page(line))
        line = after(line)
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "translation", paper.text(line), "page %d" % paper.page(line))
        line = after(line)
    return line


def table_1(first, where):
    end = paper.find(r"^Table 1\.", first)
    heads = ("Prosodic categories", "Corresponding syntactic constituents")
    rows = (("Intonational phrase (IPh)", ("Syntactic root node, Comma phrase",)),
            ("Phonological phrase (PPh) /Major phrase (MaP)", ("Maximal projection of lexical category (XP)",)),
            ("Phonological phrase (PPh) /Minor phrase (MiP)", ("Syntactically branching constituent",)),
            ("Prosodic word (PWd)", ("Morphosyntactic word",)))
    page = paper.page(end)
    paper.add("Table 1", A, "note", paper.joined((end, after(end))), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 1", A, "note", head, "page %d, a column's head" % page)
    for name, (cell,) in rows:
        paper.add("Table 1", A, "note", name, "page %d, the row's head" % page)
        paper.add("Table 1", A, "note", cell, "page %d, %s, %s" % (page, name, heads[1]))
    return after(after(end))


def table_2(first, where):
    end = paper.find(r"^Table 2\.", first)
    page = paper.page(end)
    paper.add("Table 2", A, "note", paper.text(end), "page %d, the table's caption" % page)
    heads = ("Post-tonic /a/", "Stressed /a/")
    for head in heads:
        paper.add("Table 2", A, "note", head, "page %d, a column's head" % page)
    for line in (after(first), after(after(first))):
        cells = re.findall(r"(\S+) (‘[^’]*’)", paper.text(line))
        assert len(cells) == 2, paper.text(line)
        for head, (form, meaning) in zip(heads, cells):
            paper.add("Table 2", L, "cited form", form, "page %d, under %s, %s" % (page, head, meaning))
            paper.cited_done.add(form)
    return after(end)


def table_3(first, where):
    end = paper.find(r"^Table 3\.", first)
    heads = ("", "Duration (ms)", "Mean F0(Hz)", "Max F0 (Hz)", "Tokens")
    rows = (("Post-tonic /a/", ("149.02 (sd=18.34)", "185.92 (sd=12)", "209.38 (sd=14.23)", "8")),
            ("Stressed /a/", ("172.72 (sd=31.62)", "208.98 (sd=6.67)", "227.01 (sd=7.72)", "8")))
    page = paper.page(end)
    paper.add("Table 3", A, "note", paper.text(end), "page %d, the table's caption" % page)
    for head in heads[1:]:
        paper.add("Table 3", A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add("Table 3", A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads[1:], cells):
            paper.add("Table 3", A, "note", cell, "page %d, %s, %s" % (page, name, head))
    return after(end)


def table_4(first, where):
    """Table 4: a pair of target answers, each a focus over its context question, the target
    sentence, and each one's translation, with the tokens recorded."""
    end = paper.find(r"^Table 4\.", first)
    page = paper.page(end)
    paper.add("Table 4", A, "note", paper.text(end), "page %d, the table's caption" % page)
    for head in ("Pair", "Focus", "Context question and target sentence", "Tokens"):
        paper.add("Table 4", A, "note", head, "page %d, a column's head" % page)
    line, pair, count = after(first), None, 0
    while line < end:
        found = re.match(r"^(?:(\d) )?(Subject|Object) (.+) (\d)$", paper.text(line))
        assert found, paper.text(line)
        if found.group(1):
            pair = found.group(1)
            paper.add("Table 4", A, "note", pair, "page %d, the pair's number" % page)
        paper.add("Table 4", A, "note", found.group(2), "page %d, pair %s, the focus" % (page, pair))
        rows = [(L, "transcription", found.group(3), "the context question")]
        line = after(line)
        rows.append((A, "translation", paper.text(line), "the question's translation"))
        line = after(line)
        rows.append((L, "transcription", paper.text(line), "the target sentence"))
        line = after(line)
        rows.append((A, "translation", paper.text(line), "the target sentence's translation"))
        for who, kind, text, what in rows:
            count += 1
            paper.add("Table 4 line %d" % count, who, kind, text, "page %d, pair %s, %s focus, %s" % (page, pair, found.group(2).lower(), what))
        paper.add("Table 4", A, "note", found.group(4), "page %d, pair %s, %s focus, the tokens" % (page, pair, found.group(2).lower()))
        line = after(line)
    return after(end)


def table_5(first, where):
    end = paper.find(r"^Table 5\.", first)
    heads = ("Verb", "Subject noun", "Object noun")
    rows = (("Object focus", ("2.53 (N=5, sd=15.47)", "35.69 (N=5, sd=8.78)", "41.83 (N=5, sd=8.98)")),
            ("Subject focus", ("19.04 (N=6, sd=12.00)", "38.75 (N=6, sd=7.98)", "33.80 (N=6, sd=10.28)")))
    page = paper.page(end)
    paper.add("Table 5", A, "note", paper.text(end), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 5", A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add("Table 5", A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads, cells):
            paper.add("Table 5", A, "note", cell, "page %d, %s, %s" % (page, name, head))
    return after(end)


def table_6(first, where):
    end = paper.find(r"^Table 6\.", first)
    heads = ("Mean F0 excursion difference (Object-Subject) (Hz)", "Tokens")
    rows = (("Object focus", ("-4.95", "5")), ("Subject focus", ("6.14", "6")))
    page = paper.page(end)
    paper.add("Table 6", A, "note", paper.text(end), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 6", A, "note", head, "page %d, a column's head" % page)
    for name, cells in rows:
        paper.add("Table 6", A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads, cells):
            paper.add("Table 6", A, "note", cell, "page %d, %s, %s" % (page, name, head))
    return after(end)


BLOCKS = {paper.find(r"^\(2\) Oowekyala"): two, paper.find(r"^\(3\) a\."): three,
          paper.find(r"^\(5\) "): five, paper.find(r"^\(9\) Subject focus"): focus,
          paper.find(r"^\(10\) Object focus"): focus,
          paper.find(r"^Prosodic categories"): table_1, paper.find(r"^Post-tonic /a/ Stressed"): table_2,
          paper.find(r"^Duration \(ms\)"): table_3, paper.find(r"^Pair Focus"): table_4,
          paper.find(r"^Verb Subject noun"): table_5, paper.find(r"^Mean F0 excursion difference"): table_6,
          paper.find(r"^Default intonation Stress Focus"): figure_13,
          paper.find(r"^\(1\) Utterance"): lambda first, where: paper.display(first, 6, per_line=True),
          paper.find(r"^\(7\) "): lambda first, where: paper.display(first, 4, per_line=True),
          paper.find(r"^\(8\) "): lambda first, where: paper.display(first, 5, per_line=True)}
# Each figure of phrasing opens on its intonational phrase, the Figure 14 and 15 answers on their
# phrased Kwak'wala line.
for number in range(1, paper.last + 1):
    text = paper.text(number)
    if printed(number) and (re.match(r"^\(\s*\)\s?IP$", text) or re.match(r"^\(həmápoχ\)", text)):
        BLOCKS[number] = figure
    elif printed(number) and CAPTION.match(text) and number not in BLOCKS.values():
        BLOCKS.setdefault(number, caption)
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS)
# XP carries a raised LEX on pages 11 and 26, Align(PPh, L; XPLEX, L); the rows set the LEX on a line
# of its own over the one it stands in. residue.py CORRECTIONS sets it on the XP, and the line of its
# own, read a line early into the paragraph, comes out.
for row in paper.rows:
    if row[2] == "note" and " LEX " in row[3]:
        fixed = re.sub(r" LEX (verb raising|edge alignment)", r" \1", row[3])
        assert fixed != row[3] and "XPLEX, L)" in fixed, row[3]
        row[3] = fixed
# Each entry opens on a surname and an initial; a wrap after an editor's initial, In M. / Zimmerman
# and C. Féry (eds.), runs on the entry above it. Zubizarreta's entry ends with no stop, and the
# reader runs Zwicky's onto it.
paper.merge_references()
last = max(index for index, row in enumerate(paper.rows) if row[2] == "reference")
head, tail = paper.rows[last][3].split(" Zwicky, A. ")
paper.rows[last][3] = head
paper.rows.insert(last + 1, ["references", A, "reference", "Zwicky, A. " + tail, paper.rows[last][4]])
paper.write()
