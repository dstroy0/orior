"""The ops of 2011_Gerdts_Peter: Donna B. Gerdts and Ruby M. Peter's The form and function of
nativized names in Hul’q’umi’num’. The paper sets out how English given names were made
Hul’q’umi’num’ nicknames, by the phonological accommodation of other loanwords and by truncation,
affixation and diminutivization, in thirteen tables, and what the names did for the people who
used them. An appendix gives Ruby Peter's story About White Man’s Names Becoming Hul’q’umi’num’,
(1) to (30), each sentence its transcription, a segmentation and gloss in turn, and a translation.

Page text read by glyph rows; page_text's PAPER_CIPHERS reads the codes of the Straight face the
forms are set in, upright against the Times of the prose; italic_runs reads that face as their
italics, and the English words the names come from are the paper's italics.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Hul’q’umi’num’"
AUTHORS = ["Donna B. Gerdts", "Ruby M. Peter"]
paper = gen.Paper("2011_Gerdts_Peter", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Ruby Peter", "Ruby M. Peter, the second author, of Quamichan, the speaker all the data are from"),
         ("Donna Gerdts", "Donna B. Gerdts, the first author, named tanəʔaqʷ by the Nanoose elders"),
         ("Suttles", "Wayne Suttles, Musqueam reference grammar (2004)"),
         ("Hukari", "Thomas E. Hukari, Halkomelem with Gerdts (to appear) and the Cowichan dictionary (1995)"),
         ("Haugen", "Einar Haugen, the analysis of linguistic borrowing (1950)"),
         ("Jurafsky", "Daniel Jurafsky, the semantics of the diminutive (1996)"),
         ("Thomason and Thomason", "Sarah and Lucy Thomason, truncation in Montana Salish"),
         ("Kennedy", "Dorothy I. D. Kennedy, kinship in the Coast Salish social network (2000)"),
         ("Grant et al.", "Larry Grant, Susan J. Blake and Ulrich C. Teucher, Musqueam ancestral names (2004)"),
         ("French and French", "David H. and Kathrine French, personal names (1996)")]
LANGUAGES = [(LANGUAGE, "Island Halkomelem, Central Coast Salish, spoken on Vancouver Island"),
             ("Halkomelem", "the language, its Upriver, Downriver and Island dialects"),
             ("English", "the source of the nativized names"),
             ("Chinook Jargon", "a route of borrowing, footnote 5"),
             ("Montana Salish", "its truncation, from Thomason and Thomason"),
             ("French", "the source of ləša:n and ləpen")]
# The sounds the prose and notes discuss, /k/ and /kʷ/, set in the face of the forms, and a footnote's
# mark set in italics: no forms. The italics hold the English words the loans and names come from,
# boat > put and Christine > kəstin, and two French ones.
SOUNDS = {"k", "kʷ", "č", "ə", "y", "s", "l", "p", "f", "HUL’Q’UMI’NUM"}
ENGLISH = {"boat", "doctor", "gumboots", "jam", "coffee", "shovel", "stove", "pheasant", "rum", "railroad",
           "gold", "apron", "turkey", "sweater", "nurse", "sister", "nun", "pear", "quail", "gasoline",
           "sugar"}
FRENCH = {"le châle", "la pelle"}


def form_language(run):
    if run in SOUNDS or run.isdigit():
        return None
    if run in FRENCH:
        return "French"
    if run in ENGLISH or re.fullmatch(r"[A-Z][a-z]+(?:[/ ,]+[A-Z][a-z]+)*", run):
        return "English"
    return L


paper.form_language = form_language

# The tables: each column of forms by what it holds, the name or gloss beside a form going with it.
# The page sets each caption under its table.
ROLES = {"1": ["the nativized name"], "2": ["the nativized name"], "3": ["the nativized name"],
         "4": ["the nativized name"], "5": ["the nativized full name", "the nativized truncated name"],
         "6": ["the full kin term", "the truncated kin term"],
         "7": ["the nativized full name", "the nativized truncated name"],
         "8": ["the plain word", "the word with the endearment suffix"],
         "9": ["the plain word", "the diminutive"], "10": ["the plain loanword", "the diminutive"],
         "11": ["the plain nativized name", "the diminutive"], "12": ["the plain nativized name", "the diminutive"],
         "13": ["the plain nativized name", "the diminutive", "the double diminutive"]}
STARTS = [r"^ENGLISH\s+HUL", r"^Philomena\s+", r"^Ramona\s+", r"^Abner\s+", r"^FULL\s+TRUNCATED$",
          r"^FULL\s+TRUNCATED$", r"^NAME\s+FULL\s+TRUNCATED$", r"^ʔim̓\s+", r"^PLAIN\s+DIMINUTIVE$",
          r"^PLAIN\s+DIMINUTIVE$", r"^NAME\s+PLAIN\s+DIMINUTIVE$", r"^NAME\s+PLAIN\s+DIMINUTIVE$",
          r"^NAME\s+PLAIN\s+DIMINUTIVE\s+DOUBLE$"]


def table(first, where):
    caption = paper.find(r"^Table \d+:", first)
    label = re.match(r"^Table (\d+):", paper.text(caption)).group(1)
    here, roles = "Table " + label, ROLES[label]
    open_cell, name = None, None
    for number in range(first, caption):
        text, page = paper.text(number), paper.page(number)
        if not text or paper.lines[number][2]:
            continue
        cells = re.split(r"\s{3,}", paper.spaced[number].strip())
        if all(re.match(r"^[A-Z’]+$", one.replace("HUL’Q’UMI’NUM’", "X")) for one in cells) or text == "DIMINUTIVE":
            paper.add(here, A, "note", text, "page %d, the table's column heads" % page)
            continue
        # A correspondence beside Table 1's rows, b > p, stands at the left of the first of them.
        if re.match(r"^\S\s*>\s*\S$", cells[0]):
            paper.add(here, A, "note", " ".join(cells.pop(0).split()), "page %d, the correspondence of the rows beside it" % page)
            if not cells:
                continue
        # A gloss the cell wraps, ‘grandparent / (diminutive) in Table 6, runs on the gloss above it
        # the line leaves unclosed.
        if text.startswith("(") and len(cells) == 1:
            open_gloss = [one for one in glossed if not paper.rows[one][3].endswith("’")]
            paper.rows[(open_gloss or glossed)[0]][3] += " " + text
            continue
        glossed = []
        letter = cells.pop(0) if re.match(r"^[a-h]\.$", cells[0]) else None
        column, held_name, name = 0, name, None
        # A cell the line above left on a comma, čiʔče:k, in Table 13, runs on in the line under it,
        # the name of that line's its name.
        if len(cells) == 1 and open_cell is not None:
            column, name = open_cell, held_name
        # Pauline in Table 13 has no plain name.
        if label == "13" and cells[0] == "Pauline":
            column = 1
        open_cell = None
        for cell in cells:
            # A gloss in quotes follows its form, a translation of it.
            if cell.startswith("‘"):
                paper.add(here, A, "translation", cell, "page %d, the gloss of %s" % (page, paper.rows[-1][3]))
                glossed.append(len(paper.rows) - 1)
                continue
            if re.match(r"^[A-Z][a-z]+\d*$", cell) and label not in ("6", "8", "9", "10"):
                name = cell.rstrip("0123456789")
                # A name carrying a footnote's mark, Johnson23 in Table 11, is a note of its own.
                if name != cell:
                    paper.add(here, A, "note", cell, "page %d, the name, carries footnote %s" % (page, cell[len(name):]))
                continue
            role = roles[min(column, len(roles) - 1)]
            what = "%s of %s" % (role, name) if name else role
            if letter:
                what += ", row %s" % letter
            for form in [one.strip() for one in cell.split(",") if one.strip()]:
                paper.add(here, L, "cited form", form, "page %d, %s" % (page, what))
            if cell.endswith(","):
                open_cell = column
            column += 1
    paper.add(here, A, "note", paper.text(caption), "page %d, the table's caption" % paper.page(caption))
    return caption + 1


# The marks the tables set on a form, ʔayli:n7 in Table 3.
GLUED = {"7": "ʔayli:n", "8": "flimun", "9": "katn", "14": "ʔem", "15": "ʔem", "16": "flaʔ", "17": "laʔ",
         "22": "təten̓yəl̓", "24": "če:k", "25": "čiʔčəm̓s"}
BLOCKS = {}
at = 1
for pattern in STARTS:
    at = paper.find(pattern, at)
    BLOCKS[at] = table
    at += 1
found = paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=BLOCKS, appendix=r"^Donna Gerdts$",
                      notes_title=("1",), glued=GLUED)
# Under the references, the authors' addresses; then the appendix, Ruby Peter's story.
tail = paper.find(r"^Donna Gerdts$")
opens = paper.find(r"^Appendix$", tail)
for number in range(tail, opens):
    if paper.text(number) and not paper.lines[number][2] and paper.text(number) != str(paper.page(number) + 80):
        paper.add("end", A, "note", paper.text(number), "page %d, the authors' addresses, under the references"
                  % paper.page(number))
paper.add("appendix", A, "heading", paper.text(opens), "page %d" % paper.page(opens))
paper.add("appendix", A, "heading", paper.text(opens + 1), "page %d, the appendix's title" % paper.page(opens))
paper.add("appendix", A, "title", paper.text(opens + 2), "page %d, the story's title, carries footnote 28" % paper.page(opens))


def write(mark):
    parts, at = found[mark]
    paper.footnote(mark, parts, at, NAMES, LANGUAGES, gloss="page %d, footnote %s" % (at, mark))


write("28")
skip = {one for parts, _ in found.values() for one in parts}
paper.flow(opens + 3, paper.last, "appendix", skip=skip, names=NAMES, languages=LANGUAGES)
# A sentence of the story can wrap its transcription onto a second line, (4) ... ʔə kʷsəs / lamtəl.,
# over a segmentation and gloss in pairs: where the lines over the translation are even in number,
# the first two are the transcription.
examples = {}
for index, row in enumerate(paper.rows):
    found_line = re.match(r"^\((\d+)\) line \d+$", row[0])
    if found_line:
        examples.setdefault(found_line.group(1), []).append(index)
drop = set()
for label, indexes in examples.items():
    tiers = [one for one in indexes if paper.rows[one][2] in ("transcription", "segmentation", "gloss")]
    if len(tiers) % 2 == 0:
        paper.rows[tiers[0]][3] += " " + paper.rows[tiers[1]][3]
        drop.add(tiers[1])
        for step, one in enumerate(tiers[2:]):
            paper.rows[one][2] = ("segmentation", "gloss")[step % 2]
    for count, one in enumerate([one for one in indexes if one not in drop]):
        paper.rows[one][0] = "(%s) line %d" % (label, count + 1)
paper.rows = [row for index, row in enumerate(paper.rows) if index not in drop]
# The translation of (7) wraps at a parenthesis, him / (by his native name) / Xitsulenuhw,', and the
# example reads the parenthesis as a note and the line under it as the story's prose: one translation.
index = next(at for at, row in enumerate(paper.rows) if row[0] == "(7) line 7")
paper.rows[index][3] += " " + paper.rows[index + 1][3] + " " + paper.rows[index + 2][3]
del paper.rows[index + 1:index + 3]
# The prose sets a list of forms in one run, kul, kʷul and Dorothy, Henry: a row to each form.
# Footnote 1's thanks, hay ceep q̓a’, sii’em’, sets its q̓ in Straight and the rest in italics, and
# the run's edges drop the glottal marks the practical orthography closes its words on. The suffix
# =aqʷ sets its = in the roman of the prose, and the run's edges drop the colon of ka: in footnote 3.
split = []
for row in paper.rows:
    if row[2] == "cited form" and not row[0].startswith("Table") and re.search(r", ?|\. ", row[3]):
        split.extend([row[0], row[1], row[2], one, row[4]] for one in re.split(r", ?|\. ", row[3]))
    else:
        split.append(row)
paper.rows = split
for index, row in enumerate(paper.rows):
    if row[0] == "footnote 1" and row[2] == "cited form" and row[3] == "hay ceep":
        paper.rows[index][3] = "hay ceep q̓a’"
    elif row[0] == "footnote 1" and row[2] == "cited form" and row[3] == "sii’em":
        paper.rows[index][3] = "sii’em’"
    elif row[2] == "cited form" and row[3] == "aqʷ":
        paper.rows[index][3] = "=aqʷ"
    elif row[0] == "footnote 3" and row[2] == "cited form" and row[3] == "ka":
        paper.rows[index][3] = "ka:"
paper.rows = [row for row in paper.rows if not (row[0] == "footnote 1" and row[2] == "cited form" and row[3] in ("q̓a", "q̓a’"))]
waiting = [mark for mark in found if not any(row[0] == "footnote " + mark for row in paper.rows)]
paper.place_footnotes(waiting, write, placed=[mark for mark in found if mark not in waiting])
paper.write()
