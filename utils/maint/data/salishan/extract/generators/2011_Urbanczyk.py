"""The ops of 2011_Urbanczyk: Suzanne Urbanczyk's Evidence from Halkomelem for word-based morphology.
The allomorphs of the Hul'q'umi'num' imperfective, CV-, Cə- and hə́- reduplication, apophony, schwa
deletion, metathesis and stress shift, are chosen by the phonology of the perfective word, and in a
few words by the root; ordering the operations that make them sets constructive approaches an
ordering paradox, while a relational approach takes each imperfective by analogy with the words
beside it.

Page text read by glyph rows; page_text's PAPER_CIPHERS reads the codes of the Straight face the
forms and their English are set in, the face Gerdts and Peter set their forms in, and italic_runs
reads that face and the italics as the paper's.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Halkomelem"
AUTHORS = ["Suzanne Urbanczyk"]
paper = gen.Paper("2011_Urbanczyk", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Tom Hukari", "thanked for discussion and for the electronic Cowichan Dictionary, footnote 1"),
         ("Hukari and Peter", "Thomas E. Hukari and Ruby Peter, the Cowichan Dictionary (1995)"),
         ("Hukari", "Thomas E. Hukari, resonant devoicing (1977) and nonsegmental morphology (1978)"),
         ("Leslie", "Adrian Leslie, the grammar of the Cowichan dialect (1979)"),
         ("Suttles", "Wayne Suttles, the Musqueam reference grammar (2004)"),
         ("Blevins", "James P. Blevins, word-based morphology (2006)"),
         ("Kiparsky", "Paul Kiparsky, lexical phonology (1982)"),
         ("Matthews", "P. H. Matthews, inflectional morphology (1972, 1991)"),
         ("Anderson", "Stephen R. Anderson, A-morphous morphology (1992)"),
         ("Stump", "Gregory T. Stump, inflectional morphology (2001)"),
         ("Embick and Halle", "David Embick and Morris Halle, the status of stems (2005)"),
         ("Halle and Marantz", "Morris Halle and Alec Marantz, Distributed Morphology (1993)"),
         ("Jones", "Michael K. Jones, the Cowichan actual aspect (1978)"),
         ("Leonard and Turner", "Janet Leonard and Claire Turner, SENĆOŦEN imperfectives (2010)"),
         ("Chomsky", "Noam Chomsky, Knowledge of Language (1986)")]
LANGUAGES = [(LANGUAGE, "Central Salish, the language of the paper"),
             ("Hul’q’umi’num’", "the Vancouver Island dialect of Halkomelem, the paper's data"),
             ("Musqueam Halkomelem", "the Downriver dialect, Suttles's grammar"),
             ("Central Salish", "the branch of Halkomelem"), ("Interior Salish", "infixal vowels of plurality and aspect"),
             ("Latin", "research on its paradigms")]
# The italics of the prose are its terms, perfective and imperfective, per se, root and affix, and the
# English words of its analogy, sing:sang, bring, brang, brought, and man, men, *mens; the forms and
# the templates Cə- and fIMP are set in the Straight face.
ENGLISH = {"sing:sang", "bring", "brang", "brought", "man", "men", "mens"}


def form_language(run):
    if run in ENGLISH:
        return "English"
    if re.fullmatch(r"[A-Za-z :]+", run) or run in ("Cə", "fIMP", "ƒIMP"):
        return None
    return L


paper.form_language = form_language
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
# Two English glosses in Straight read letter-spaced off the glyph rows; the 300 dpi renders of pages
# 2 and 15 print them whole.
PRINTED = {"pull of f a layer; cut slabs f ro m wood": "pull off a layer; cut slabs from wood",
           "‘bo tto m’": "‘bottom’"}


def printed(start, stop):
    for number in range(start, stop):
        if paper.text(number) and not paper.lines[number][2] and number not in RUNNING and number not in FOOT:
            yield number, paper.text(number)


def spell(text):
    for spaced, whole in PRINTED.items():
        text = text.replace(spaced, whole)
    return text


def word_list(first, where):
    """An example of lettered words: each a perfective beside its English and the imperfective under
    it, (20c) with two. A line that splits in no column gap sets its form first, xʷk̓ʷat pull it."""
    label, caption = re.match(r"^\((\d+)\)\s+(.*)$", paper.text(first)).groups()
    paper.add("(%s) line 1" % label, A, "note", spell(caption), "page %d, the example's heading" % paper.page(first))
    letter, count, number = None, 0, first + 1
    for number, text in printed(first + 1, paper.last + 1):
        spaced = paper.spaced[number].strip()
        opened = re.match(r"^([a-h])\.\s+(.*)$", spaced)
        rest = opened.group(2) if opened else spaced
        cells = [one for one in re.split(r"\s{3,}", rest) if one.strip()]
        if not opened and len(cells) < 2 and re.fullmatch(r"[A-Za-z,.;:’'()-]+", rest.split()[0]):
            break
        if opened:
            letter, count = opened.group(1), 0
        if len(cells) < 2:
            cells = rest.split(None, 1)
        form, english = cells[0], spell(" ".join(" ".join(cells[1:]).split()))
        here, page = "(%s%s)" % (label, letter), paper.page(number)
        # (24) heads each word pair with its lexical suffix, /-as/ ‘face’.
        if form.startswith("/"):
            paper.add("%s line %d" % (here, count + 1), L, "cited form", form, "page %d, the lexical suffix" % page)
            paper.add("%s line %d" % (here, count + 2), A, "translation", english, "page %d, the suffix's gloss" % page)
            count += 2
            continue
        role = "the perfective" if not any(row[0].startswith(here + " ") and row[2] == "transcription"
                                           for row in paper.rows) else "the imperfective"
        paper.add("%s line %d" % (here, count + 1), L, "transcription", form, "page %d, %s" % (page, role))
        kind = "translation" if english.startswith("‘") else "word gloss"
        paper.add("%s line %d" % (here, count + 2), A, kind, english, "page %d, %s's English" % (page, role))
        count += 2
    else:
        number = paper.last + 1
    return number


# The derivations and charts, to the prose line that follows each: every printed line a note. A
# subscript IMP the glyph rows set on a line of its own under ƒ is that ƒ's, ƒIMP.
TABLES = {"9": r"^Having examined the basic", "12": r"^In order to derive words", "13": r"^Notice that both reduction",
          "17": r"^This sort of situation", "18": r"^Notice that a constructivist", "19": r"^Given the rest of",
          "21": r"^In order to get the", "22": r"^The variation only occurs", "25": r"^The complication associated"}
# The labels (13) and (17) stand alone over their tables, and the glyph rows set each on the prose
# line above it, the end of a sentence (render of pages 12 and 14).
ON_PROSE = {"13", "17"}


def table(first, where):
    label, rest = re.match(r"^\((\d+)\)\s*(.*)$", paper.text(first)).groups()
    stop = paper.find(TABLES[label], first)
    lines = []
    if label in ON_PROSE:
        note = max(index for index, row in enumerate(paper.rows) if row[0] == where and row[2] == "note")
        paper.rows[note][3] += " " + rest
    elif rest:
        lines.append((first, rest, "the example's heading"))
    for number, text in printed(first + 1, stop):
        if text == "IMP" and lines and lines[-1][1].startswith("ƒ "):
            lines[-1] = (lines[-1][0], "ƒIMP" + lines[-1][1][1:], lines[-1][2])
            continue
        lines.append((number, text, "a line of the example"))
    # (12)'s definition wraps onto a second line.
    if label == "12":
        lines[1:3] = [(lines[1][0], lines[1][1] + " " + lines[2][1], "the function's definition")]
    for index, (number, text, what) in enumerate(lines, 1):
        paper.add("(%s) line %d" % (label, index), A, "note", text, "page %d, %s" % (paper.page(number), what))
    return stop


BLOCKS = {}
at = 1
for label in range(1, 26):
    at = paper.find(r"^\(%d\)\s" % label, at)
    BLOCKS[at] = table if str(label) in TABLES else word_list
    at += 1
found = paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, appendix=r"^Suzanne Urbanczyk$")
tail = paper.find(r"^Suzanne Urbanczyk$", paper.find(r"^References$"))
for number, text in printed(tail, paper.last + 1):
    paper.add("end", A, "note", text, "page %d, the author's address, under the references" % paper.page(number))
# The prose cites the root /c̓əq̓ʷ/ and the suffixes /-aləst/ and /-aləs/ in slashes; the run's
# edges drop the hyphen.
for row in paper.rows:
    if row[2] == "cited form" and row[3] in ("aləst", "aləs"):
        row[3] = "-" + row[3]
# Every entry opens on a surname and a comma; the reader takes Archangeli and Pulleyblank's
# Cambridge, MA: MIT Press. and the editors Gerdts and Lisa Matthewson of Urbanczyk (2004), each on a
# line after a stop, for entries of their own.
joined = []
for row in paper.rows:
    if row[2] == "reference" and not re.match(r"^[A-Z][\w’'\-]+, [A-Z][a-z.]", row[3]) and joined[-1][2] == "reference":
        joined[-1][3] += " " + row[3]
    else:
        joined.append(row)
paper.rows = joined
paper.write()
