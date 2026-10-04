"""The ops of 2012_Bird: Sonya Bird's Cool thing about ultrasound #17: now I can pronounce /hiqət/!,
an ultrasound study of one fluent SENĆOŦEN speaker's /qi/ and /iq/. The speaker rolls his tongue
forward along the palate in /qi/ and backwards in /iq/ during the /q/ closure, a tongue looping not
reported before for uvulars, and the paper proposes ultrasound as a tool for teaching pronunciation.

Page text read by glyph rows, SILDoulosIPA's legacy codes mapped by page_text's PAPER_CIPHERS. The
paper has no glossed examples: (1) to (3) are lists, (2)'s items SENĆOŦEN pronunciations. Figure
1, the consonant inventory, and Table 1, the stimuli, set k’ʷ, t’ᶿ and the rest with the raised
letter drawn small at the body's size, which page_text cannot see; their cells are set from 300 dpi
renders of pages 3 and 5. Tables 2 and 3 are the strategies, and Examples 1 to 4 are ultrasound
stills, images the layer does not carry, with their captions and the labels under the frames.
"""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "SENĆOŦEN"
AUTHORS = ["Sonya Bird"]
paper = gen.Paper("2012_Bird", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Janet Leonard", "thanked; Bird and Leonard (2009)"), ("Scott Moisik", "thanked; Moisik et al. (2010)"),
         ("Sarah Smith", "thanked"), ("Linda Smith", "p.c., on the Tsilhqut’in pattern"),
         ("Speaker 1", "a fluent SENĆOŦEN speaker from the Tsartlip reserve, West Saanich"),
         ("Speaker 2", "a fluent SENĆOŦEN speaker from the Tsawout reserve, East Saanich"),
         ("Gick", "Bryan Gick, articulatory conflict with Wilson (2006), ultrasound fieldwork (2002)"),
         ("Wilson", "Ian Wilson, with Gick (2006)"), ("Cook", "Eung-Do Cook, Chilcotin flattening (1993)"),
         ("Stone", "Maureen Stone, analyzing tongue motion from ultrasound (2005)"),
         ("Montler", "Timothy Montler, Saanich (1986) and the classified word list (1991)"),
         ("Mooshammer", "Christine Mooshammer et al., loops (1995)"),
         ("Kent", "Ray Kent and Kenneth Moll, cinefluorographic analyses (1972)"),
         ("Blevins", "Juliette Blevins, Evolutionary Phonology (2004)"),
         ("Bernhardt", "Barbara Bernhardt et al., ultrasound in speech therapy (2005)"),
         ("Adler-Bock", "Marcy Adler-Bock et al., ultrasound in remediation of /r/ (2007)")]
LANGUAGES = [(LANGUAGE, "North Straits Salish (Central Salish), spoken on the Saanich Peninsula"),
             ("North Straits Salish", "SENĆOŦEN"), ("Central Salish", "SENĆOŦEN"), ("Salish", "the family"),
             ("Nuu-chah-nulth", "/qi/ as [qɪ], /iq/ as [iᵊq]"), ("Tsilhqut’in", "/qi/ as [qᵊi], /iq/ as [ɪq]"),
             ("English", "the translations given the speakers"), ("Mandarin", "Moisik et al. (2010)")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
SKIP = FOOT | set(paper.volume_header()) | paper.running_numbers()


def lines_from(number):
    """The body's lines from number on, passing over footnotes, page numbers and page marks."""
    while number <= paper.last:
        if number not in SKIP and not paper.lines[number][2] and paper.text(number).strip():
            yield number
        number += 1


def until(start, ends):
    """[(line, text)] of the body from start to the line before the first matching ends, and that
    line's number."""
    taken = []
    for number in lines_from(start):
        if re.match(ends, paper.text(number)):
            return taken, number
        taken.append((number, paper.text(number)))
    raise ValueError(ends)


def put(where, who, kind, form, gloss):
    paper.add(where, who, kind, re.sub(r"\s+", " ", form).strip(), gloss)


def listing(ends, who, kind, gloss):
    """(1) to (3): a caption wrapped over lines to its a., then the lettered items, each wrapped."""
    def block(start, where):
        label = gen.EXAMPLE.match(paper.text(start)).group(1)
        at = paper.page(start)
        taken, end = until(start, ends)
        texts = [gen.EXAMPLE.match(taken[0][1]).group(2)] + [text for _, text in taken[1:]]
        parts, current = [], []
        for text in texts:
            if re.match(r"^[a-c]\.\s", text):
                parts.append(current)
                current = [text]
            else:
                current.append(text)
        parts.append(current)
        put("(%s) line 1" % label, A, "note", " ".join(parts[0]), "page %d, over the example" % at)
        for part in parts[1:]:
            letter, rest = re.match(r"^([a-c])\.\s+(.*)$", " ".join(part)).groups()
            put("(%s%s) line 1" % (label, letter), who, kind, rest, "page %d, %s" % (at, gloss))
        return end
    return block


# Figure 1's rows off a 300 dpi render of page 3: the w and θ are raised, and q, χ and ɴ bold.
FIGURE_1 = ["p t tʃ (k) kʷ q qʷ", "p’ t’ᶿ t’ tɬ’ tʃ’ k’ʷ q’ q’ʷ ʔ", "θ s ɬ ʃ xʷ χ χʷ h", "m n l y w ɴ",
            "m’ n’ l’ y’ w’ ɴ’"]


def figure_1(start, where):
    """Figure 1: five rows of consonants, a manner a row, and the caption under them."""
    at = paper.page(start)
    taken, end = until(start, r"^Bird & Leonard \(2009\) conducted")
    assert len(taken) == 6 and taken[5][1].startswith("Figure 1."), taken
    for count, row in enumerate(FIGURE_1, 1):
        dot = ", a small dot set low between the y and the ’ of y’" if count == 5 else ""
        put("Figure 1 line %d" % count, L, "transcription", row,
            "page %d, a row of the consonant inventory, the uvulars in bold, its raised letters read off the render%s"
            % (at, dot))
    put("Figure 1", A, "note", taken[5][1], "page %d, the figure's caption" % at)
    return end


# Table 1's words off a 300 dpi render of page 5, by sequence: each word and its English gloss.
TABLE_1 = [("/qi/", [("/sqimək’ʷ/", "octopus"), ("/sqitəw/", "mermaid"), ("/ʃqitəs/", "headband")]),
           ("/ki/", [("/kiŋtʃa:tʃ/", "Canada"), ("/skiŋtʃa:tʃ/", "Canadian")]),
           ("/iq/", [("/t’ᶿiqt/", "ivory billed woodpecker"), ("/hiqət/", "to put something in the oven")]),
           ("/ik/", [("/ʃikəsew’txʷ/", "church"), ("/tʃikmən/", "iron")])]


def table_1(start, where):
    """Table 1: its caption and column heads, then a sequence's words beside the other's, a
    word's gloss wrapped over as many lines as its cell takes."""
    at = paper.page(start)
    taken, end = until(start, r"^3\.3\s")
    put("Table 1", A, "note", taken[0][1], "page %d, the table's caption" % at)
    put("Table 1", A, "note", "Sequence Word English gloss Sequence Word English gloss",
        "page %d, the column heads, English gloss wrapped over two lines" % at)
    read = " ".join(text for _, text in taken[3:])
    for sequence, words in TABLE_1:
        put("Table 1", A, "note", sequence, "page %d, the row's head, the sequence" % at)
        for word, english in words:
            put("Table 1", L, "cited form", word, "page %d, a word with %s, its target sequence in bold" % (at, sequence))
            put("Table 1", A, "translation", english, "page %d, the English gloss of %s" % (at, word))
            for piece in (english.split()):
                assert piece in read, piece
    return end


def strategies(name, ends):
    """Tables 2 and 3: the caption, the heads, then a strategy's instances and words a row."""
    def block(start, where):
        at = paper.page(start)
        taken, end = until(start, ends)
        put(name, A, "note", taken[0][1], "page %d, the table's caption" % at)
        put(name, A, "note", taken[1][1], "page %d, the column heads" % at)
        for _, text in taken[2:]:
            cells = re.match(r"^(.*?)\s+(\d+)(?:\s+(.*))?$", text).groups()
            put(name, A, "note", cells[0], "page %d, the row's head" % at)
            put(name, A, "note", cells[1], "page %d, %s, the number of instances" % (at, cells[0]))
            if cells[2]:
                # Table 3's t'ᶿiqt raises its θ, read off a 300 dpi render of page 7.
                words = cells[2].replace("t’θ", "t’ᶿ")
                put(name, L, "transcription", words,
                    "page %d, %s, the words, their target sequence in bold, and the repetitions out of three" % (at, cells[0]))
        return end
    return block


def example(start, where):
    """Examples 1 to 4: the caption over the ultrasound stills, and under them the frame numbers
    and the labels, a line each as the text layer reads them."""
    name = re.match(r"^(Example \d):", paper.text(start)).group(1)
    at = paper.page(start)
    taken, end = until(start + 1, r"^(Example \d illustrates|Example 3 illustrates|Example 4 also|5\s+Discussion)")
    put(name, A, "note", paper.text(start), "page %d, the example's caption; the ultrasound stills are images" % at)
    for count, (number, text) in enumerate(taken, 1):
        what = "the frame numbers" if re.match(r"^[\d ]+$", text) else "the labels under the frames"
        put("%s line %d" % (name, count), A, "note", text, "page %d, %s" % (paper.page(number), what))
    return end


blocks = {paper.find(r"^\(1\)\s+Possible"): listing(r"^Previous work", A, "note", "a strategy and an illustrative sound change"),
          paper.find(r"^\(2\)\s+Overall"): listing(r"^However, a lot", L, "transcription", "a sequence and its pronunciations"),
          paper.find(r"^\(3\)\s+Two research"): listing(r"^Unfortunately", A, "note", "a research question"),
          paper.find(r"^p\s+t\s+tʃ"): figure_1,
          paper.find(r"^Table 1\. Stimuli"): table_1,
          paper.find(r"^Table 2 Strategies"): strategies("Table 2", r"^Table 3 Strategies"),
          paper.find(r"^Table 3 Strategies"): strategies("Table 3", r"^The following examples")}
for number in range(1, 5):
    blocks[paper.find(r"^Example %d:" % number)] = example
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

rows = paper.rows
# The abstract, set with no heading under the university, is one note.
lines = [index for index, row in enumerate(rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Sonya Bird"][1:]
if lines:
    rows[lines[0]][3] = " ".join(rows[index][3] for index in lines)
    rows[lines[0]][4] = "page 1, the abstract"
paper.rows = rows = [row for index, row in enumerate(rows) if index not in lines[1:]]


def index_of(test):
    return next(index for index, row in enumerate(rows) if test(row))


# Kent and Moll (1972) sets no comma after Kent, and the reference reader runs it on into Gick and
# Wilson (2006).
at = index_of(lambda row: row[2] == "reference" and row[3].startswith("Gick, B. & I. Wilson (2006)"))
gick, kent = re.match(r"^(.*?) (Kent R\. & K\. Moll \(1972\)\..*)$", rows[at][3]).groups()
rows[at][3] = gick
rows.insert(at + 1, ["references", A, "reference", kent, rows[at][4]])
# Wilson and Gick (2006) wraps its publisher onto a line the reader opens as an entry, and the
# author's name and e-mail close page 12 under the references.
at = index_of(lambda row: row[2] == "reference" and row[3].startswith("Sommerville, MA"))
publisher, address = re.match(r"^(.*?Project\.) (Sonya Bird sbird@uvic\.ca)$", rows[at][3]).groups()
rows[at - 1][3] += " " + publisher
rows[at] = ["end", A, "note", address, "page 12, the author's name and e-mail"]
paper.write()
