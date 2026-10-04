"""The ops of 2013_Palmer: Andie Diane Palmer's Wearing Mountain Goat's robe, the late Vi Hilbert,
taqʷšəblu, read as Young-Leslie's ecographer through the Lushootseed teachings of mountain goat
power, the wealth it attracts, siʔab, and its redistribution.

Page text read by glyph rows; page_text's PAPER_CIPHERS reads the Lushootseed font.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Lushootseed"
AUTHORS = ["Andie Diane Palmer"]
paper = gen.Paper("2013_Palmer", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Vi Hilbert", "taqʷšəblu, Lushootseed Elder (1918-2008), founder of Lushootseed Research"),
         ("Young-Leslie", "Heather E. Young-Leslie, the concept of the ecographer (2007)"),
         ("Halvaksz", "Jamon Halvaksz, with Young-Leslie, the ecographer (2007, 2008)"),
         ("Serres", "Michel Serres, the fold and the baker (1995)"),
         ("Ingold", "Tim Ingold, enskilment and the education of attention (2000)"),
         ("Feit", "Harvey A. Feit, the Waswanipi Cree ethical position (2004)"),
         ("Ron Hilbert", "čadəsq̓idəb, Vi Hilbert's son, Seattle-based artist and illustrator (1953-2006)"),
         ("Zalmai ‘Zeke’ Zahir", "ʔəswili, a teacher of Lushootseed"),
         ("Bruce Ruddell", "composer of The Healing Heart of the First People of this Land (2006)"),
         ("Bob Keller", "interviewer of Vi Hilbert (1996)"),
         ("Maggie Emery", "with Amelia Douglas, told the Story of the mountain goat people of Cheam"),
         ("Amelia Douglas", "with Maggie Emery, told the Story of the mountain goat people of Cheam"),
         ("Leon Metcalfe", "was told the Story of the Great Flood by Dora Solomon in 1975"),
         ("Dora Solomon", "Skagit and Lummi, told the Story of the Great Flood (1975)"),
         ("Chamberlain", "Rebecca Chamberlain, Mountain Goat Power (1993)"),
         ("Bruce subiyay Miller", "the late Skokomish longhouse leader (1999)"),
         ("Edgar Heap of Birds", "the installation Day for Night"),
         ("Buster Simpson", "the Seattle George monument"),
         ("Michael CHiXapkaid Pavel", "collected the mountain goat wool of the robe"),
         ("Susan sa’hLa mitSa Pavel", "spun and wove the robe"),
         ("Mimi Gates", "hosted the celebration of the robe's installation"),
         ("Bates", "Dawn Bates, with Hess and Hilbert, the Lushootseed Dictionary"),
         ("Mark Ebert", "thanked for commentary and technical support")]
LANGUAGES = [(LANGUAGE, "Central Salish, the language of Vi Hilbert and of the paper's words"),
             ("dxʷləšucid", "Lushootseed in its own name"), ("Salish", "the family"),
             ("English", "the language of the paper")]
# Headings 1 and 3 wrap onto a second line; each is read whole on its first.
for pattern in (r"^1\s+Working through", r"^3\s+On obtaining"):
    number = paper.find(pattern)
    whole = paper.text(number) + " " + paper.text(number + 1)
    paper.lines[number] = (whole,) + tuple(paper.lines[number][1:])
    paper.spaced[number] = whole
    paper.lines[number + 1] = ("",) + tuple(paper.lines[number + 1][1:])
    paper.spaced[number + 1] = ""
HEADINGS = {paper.find(pattern): label for pattern, label in
            ((r"^1\s+Working through", "1"), (r"^2\s+taqʷšəblu$", "2"), (r"^3\s+On obtaining", "3"),
             (r"^4\s+Sharing Wealth", "4"), (r"^5\s+Sacred Change", "5"))}
REFERENCES = paper.find(r"^References Cited$")
# The Lushootseed words stand upright in the paper's Lushootseed font, not in italics; each is read
# as an italic run of its page, for cited() to find. The names of people, subiyay and CHiXapkaid,
# stand in Times.
SIGNS = re.compile(r"[əʔʷšłč]|x̌|q̓|l̓")
italics = paper.italics()
PRINTED = {run for runs in italics.values() for run in runs}
TAIL = paper.find(r"^Acknowledgements$", REFERENCES)
for number in list(range(1, REFERENCES)) + list(range(TAIL + 1, paper.last + 1)):
    # Lushootseed words set side by side are one run, uʔdahadəbš čəxʷ.
    word = r"[\w’ʔəšłčʷ̓̌]*(?:[əʔʷšłč]|x̌|q̓|l̓)[\w’ʔəšłčʷ̓̌]*"
    for token in re.findall(r"%s(?: %s)*" % (word, word), paper.text(number) or ""):
        if SIGNS.search(token) and token not in italics.setdefault(paper.page(number), []):
            italics[paper.page(number)].append(token)
LUSHOOTSEED = {run for runs in italics.values() for run in runs if SIGNS.search(run)} | {"Seyowin"}
paper.form_language = lambda run: L if run in LUSHOOTSEED else None


def panel(start, where):
    """Heap of Birds's right panel on page 8: the Lushootseed and, on its reverse, the English, each
    under a line that names it."""
    for line, kind in enumerate(("note", "transcription", "note", "translation"), 1):
        paper.add("%s panel line %d" % (where, line), L if kind == "transcription" else A, kind,
                  paper.text(start + line - 1), "page %d" % paper.page(start))
    return start + 4


paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=2, spaced={"1": "Leslie 2007)"}, headings=HEADINGS,
               references=r"^References Cited$", blocks={paper.find(r"^Lushootseed, right panel:$"): panel},
               appendix=r"^Other Sources Consulted$")
# Two sources consulted under their own heading; the acknowledgements, a paragraph with its names and
# its Lushootseed; the author's name and e-mail at the end.
OTHER = paper.find(r"^Other Sources Consulted$", REFERENCES)
paper.add("references", A, "heading", paper.text(OTHER), "page %d" % paper.page(OTHER))
for entry, at in paper.references(OTHER + 1, TAIL - 1):
    paper.add("references", A, "reference", entry, "page %d" % at)
paper.add("acknowledgements", A, "heading", paper.text(TAIL), "page %d" % paper.page(TAIL))
SIGN_OFF = paper.find(r"^Andie Diane Palmer$", TAIL)
paper.flow(TAIL + 1, SIGN_OFF - 1, "acknowledgements", names=NAMES + [
    ("Micheal CHiXapkaid Pavel", "thanked for continuing the traditions of his uncle subiyay's longhouse"),
    ("Heather Young-Leslie", "thanked for the ecographer concept and comments on the draft")], languages=LANGUAGES)
paper.add("end", A, "note", paper.joined([SIGN_OFF, SIGN_OFF + 1]),
          "page %d, the author's name and e-mail" % paper.page(SIGN_OFF))
# The Lushootseed words stand in the Lushootseed font, and one in Times with 3 for ə, as printed.
for row in paper.rows:
    if row[2] == "cited form" and row[3] not in PRINTED:
        row[4] = row[4].replace("in italics", "as printed, with 3 for ə" if "3" in row[3]
                                else "upright, in the Lushootseed font")
paper.write()
