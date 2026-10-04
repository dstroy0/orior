"""The ops of 2010_Gerdts: Donna B. Gerdts's Agreement in Halkomelem complex auxiliaries. The positional
words ʔeʔət 'here' and naʔət 'there' serve Island Halkomelem as auxiliaries, pointing out an event in
the present perceptual field; unlike the simple auxiliaries ʔi and niʔ they refuse questions, futures
and first- and second-person subjects, and their determiner element optionally agrees in gender with a
feminine singular subject, ʔeʔəθ and naʔəθ.

Page text read by glyph rows.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Halkomelem"
AUTHORS = ["Donna B. Gerdts"]
paper = gen.Paper("2010_Gerdts", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Margaret James", "Hul’q’umi’num’ speaker thanked for data, footnote 1"),
         ("Ruby Peter", "Hul’q’umi’num’ speaker thanked for data, footnote 1; RP in the examples' sources"),
         ("Theresa Thorne", "Hul’q’umi’num’ speaker thanked for data, footnote 1; TT in the examples' sources"),
         ("Bill Seward", "Hul’q’umi’num’ speaker thanked for data, footnote 1"),
         ("David Potter", "thanked for editorial assistance, footnote 1"),
         ("Sarah Kell", "thanked for editorial assistance, footnote 1"),
         ("Charles Ulrich", "thanked for editorial assistance, footnote 1"),
         ("Mrs. Peter", "Ruby Peter, who explained the difference between (17) and (18)"),
         ("Gerdts and Hukari", "Donna B. Gerdts and Thomas E. Hukari, Halkomelem (to appear)"),
         ("Aissen", "Judith Aissen, Toward a theory of agreement controllers (1990)"),
         ("Perlmutter", "David Perlmutter, Personal vs. impersonal constructions (1983)"),
         ("Jespersen", "Otto Jespersen, A Modern English Grammar on Historical Principles (1936)"),
         ("Fries", "Charles C. Fries, American English Grammar (1940)"),
         ("Kroeber", "Paul D. Kroeber, The Salish Language Family: Reconstructing Syntax (1999)"),
         ("Gerdts", "Donna B. Gerdts, Object and Absolutive in Halkomelem Salish (1988) and Halkomelem gender (2009)")]
LANGUAGES = [(LANGUAGE, "Central Salish, the language of the paper"),
             ("Halkomelem Salish", "the language of the paper"),
             ("Hul’q’umi’num’", "the Island dialect of Halkomelem"),
             ("English", "the translations, and the there-constructions of section 3.2")]
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
# A source at the right of a translation or on the line under it, (RP 22Jun04); (46)'s is printed
# with no opening parenthesis.
SOURCE = re.compile(r"^(.*?’)\s*(\(?(?:RP|TT) [^()]*\))$")


def printed(number):
    return bool(paper.text(number)) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def example(first, where):
    """A transcription over its gloss, the pair wrapping to a second in the longer examples, and the
    translation closing it, run on to the line under it where its source wraps or stands there; the
    source a citation, and the bracketed comment under (27) a note."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    line, count = first, 0
    while not text.startswith("‘"):
        count += 1
        who, kind = ((L, "transcription"), (A, "gloss"))[(count - 1) % 2]
        paper.add("(%s) line %d" % (label, count), who, kind, text, "page %d" % paper.page(line))
        line = after(line)
        text = paper.text(line)
    at, under = line, False
    while text.count("(") > text.count(")"):
        line = after(line)
        text += " " + paper.text(line)
    if not SOURCE.match(text) and re.match(r"^\((?:RP|TT) ", paper.text(after(line))):
        line, under = after(line), True
        text += " " + paper.text(line)
    source = SOURCE.match(text)
    count += 1
    paper.add("(%s) line %d" % (label, count), A, "translation", source.group(1) if source else text,
              "page %d" % paper.page(at))
    if source:
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "citation", source.group(2),
                  "page %d, the source %s the translation" % (paper.page(line), "under" if under else "at the right of"))
    line = after(line)
    if paper.text(line).startswith("["):
        count += 1
        paper.add("(%s) line %d" % (label, count), A, "note", paper.text(line),
                  "page %d, the comment under the translation" % paper.page(line))
        line = after(line)
    return line


# Table 1 on page 8, and Table 2 on page 15, whose rows the text layer runs together in its last row;
# its cells are read off a 400 dpi render of the page.
TABLES = {"1": (["MAN", "WOMAN"], [
              ("SINGULAR", [("tᶿə swəy̓qeʔ", "‘the man’"), ("θə słeniʔ", "‘the woman’")]),
              ("PLURAL", [("tᶿə səw̓əy̓qeʔ", "‘the men’"), ("tᶿə słənłeniʔ", "‘the women’")])],
              r"^The complex auxiliaries encode"),
          "2": (["PROXIMATE", "DISTAL"], [
              ("VERB/AUXILIARY", [("ʔi", "‘be here/now’"), ("niʔ", "‘be there/then’")]),
              ("PRESENTATION VERB/AUXILIARY", [("ʔeʔət", "‘here’"), ("naʔət", "‘there’")]),
              ("SPATIAL/TEMPORAL DEMONSTRATIVE", [("təʔi", "‘here, now, this’"), ("tən̓a", "‘this, this one, here’")]),
              ("PREPOSITIONAL DEMONSTRATIVE", [("tən̓i", "‘from here’"), ("tən̓niʔ", "‘from here’")]),
              ("SPATIAL DEMONSTRATIVE", [("təʔinəł", "‘this, this way, here’"), ("tənanəł", "‘that, that way’")])],
              r"^This is reminiscent")}


def table(first, where):
    label = re.match(r"^Table (\d)\.", paper.text(first)).group(1)
    heads, rows, stop = TABLES[label]
    here, page = "Table " + label, paper.page(first)
    paper.add(here, A, "note", paper.text(first), "page %d, the table's caption" % page)
    paper.add(here, A, "note", "   ".join(heads), "page %d, the table's column heads" % page)
    for name, cells in rows:
        paper.add(here, A, "note", name, "page %d, the row's head" % page)
        for head, (form, meaning) in zip(heads, cells):
            paper.add(here, L, "cited form", form, "page %d, %s, %s" % (page, name, head))
            paper.add(here, A, "translation", meaning, "page %d, the gloss of %s" % (page, form))
    return paper.find(stop, first)


def schema(first, where):
    paper.add("§4", A, "note", paper.text(first), "page %d, the schema of a main clause" % paper.page(first))
    return after(first)


BLOCKS = {}
at = 1
for label in range(1, 64):
    at = paper.find(r"^\(%d\)\s" % label, at)
    BLOCKS[at] = example
    at += 1
BLOCKS[paper.find(r"^Table 1\. ")] = table
BLOCKS[paper.find(r"^Table 2\. ")] = table
BLOCKS[paper.find(r"^AUXILIARY \(SUBJECT PRONOUNS\)")] = schema
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, notes_title=("1",))
for row in paper.rows:
    if row[2] == "title" and row[3].endswith("auxiliaries1"):
        row[3] = row[3][:-1]
        row[4] += ", carries footnote 1"
    # Section 2.1 sets its italic ʔeʔət against a roman that with no space between (render of page
    # 3); the cited form is the italic run alone.
    if row[2] == "cited form" and row[3] == "thatʔeʔət":
        row[3] = "ʔeʔət"
paper.write()
