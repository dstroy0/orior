"""The ops of Martin_2019_ICSNL: Katie Martin on k'am and xsax in Gitksan, the two words translated
only: `k'am exhaustive` and at home with predicates, `xsax not exhaustive`, an information focus marker
after Kiss (1998), and k'am xsax the pair that marks `exhaustive focus` on arguments and adjuncts.

Each example sets its context, then the Gitksan segmented over its gloss and the translation in
quotes, the elicitation method, vf or sf, at the right of the Gitksan; the consultant's comments
follow some. (12), (13) and (18) are English sentences. Table 1 charts where each of k’am, xsax and
k’am xsax can associate.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitksan"
AUTHORS = ["Katie Martin"]
paper = gen.Paper("Martin_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
# The Gitksan line of each example is segmented, its morphemes broken by - and =, over its gloss.
paper.opening = "segmentation"
NAMES = [("Hector Hill", "the author's consultant, a speaker from Gitsegukla"),
         ("Vincent Gogag", "a Gitksan speaker, thanked"), ("Barbara Sennott", "a Gitksan speaker, thanked"),
         ("Jeannie Harris", "a Gitksan speaker, thanked"), ("Lisa Matthewson", "thanked"),
         ("Henry Davis", "thanked"), ("Hotze Rullmann", "thanked"),
         ("Bicevskis", "Katie Bicevskis, Henry Davis and Lisa Matthewson, quantification in Gitksan (2017)"),
         ("Hindle and Rigsby", "a short practical dictionary of the Gitksan language (1973)"),
         ("Rigsby", "Bruce Rigsby, Gitksan grammar (1986)"), ("Hunt", "Katharine D. Hunt (1993)"),
         ("Neeleman", "Ad Neeleman and others, a syntactic typology of topic, focus and contrast (2009)"),
         ("Kiss", "Katalin É. Kiss, identificational and information focus (1998)"),
         ("Szabolcsi", "Anna Szabolcsi (1981)"), ("Krifka", "Manfred Krifka, additive particles (1998)"),
         ("Keupdjio", "Hermann Keupdjio, in preparation")]
LANGUAGES = [(LANGUAGE, "Interior Tsimshianic, spoken in the Skeena River region of British Columbia"),
             ("English", "its only exhaustive")]
METHOD = {"(vf)": "volunteered form, given by the consultant as a translation from English",
          "(sf)": "solicited form, suggested by the elicitor and judged by the consultant"}

# The Gitksan the paper sets in italics is defined in plain letters, k'am, xsax, hox; the italic
# English it discusses, only and also, the propositions p and q, and the sentence [I]F only ate three
# berries, is no Gitksan.
ENGLISH = {"only", "also", "p", "q", "I]F only ate three berries"}
paper.form_language = lambda run: None if run in ENGLISH or len(run) > 30 else L
# Footnote 2 runs on at the head of page 2's notes, its sentence on page 1 closed.
found = paper.page_footnotes()
paper.page_footnotes = lambda stops=(), symbols_on=(1,): dict(
    found, **{"2": (found["2"][0] + list(range(paper.find("^Forms volunteered by the consultant"),
                                                     paper.find("^form\\). I do not gloss") + 1)), found["2"][1])})


def table(start, where):
    """Table 1: the caption and the header notes, then each row's form a cited form glossed with
    its judgment under each header, and its judgments a note."""
    page = paper.page(start)
    paper.add("Table 1", A, "note", paper.text(start), "page %d, the table's caption" % page)
    paper.add("Table 1", A, "note", paper.text(start + 1), "page %d, the table's header: arguments and adjuncts, "
                                                           "predicates, numbers and quantifiers" % page)
    heads = ("arguments and adjuncts", "predicates", "numbers and quantifiers")
    line = start + 2
    while re.match(r"^(?:k’am|xsax)", paper.text(line)):
        cells = [one for one in re.split(r"\s{3,}", paper.spaced[line].strip()) if one]
        judged = ", ".join("%s %s" % (head, cell) for head, cell in zip(heads, cells[1:]))
        paper.add("Table 1", L, "cited form", cells[0], "page %d, Table 1: %s" % (page, judged))
        paper.add("Table 1", A, "note", " ".join(cells[1:]), "page %d, Table 1, the judgments of %s" % (page, cells[0]))
        line += 1
    return line


def english(start, where):
    """(12), (13) and (18): each lettered part an English sentence, a note."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    line, text = start, gen.EXAMPLE.match(paper.text(start)).group(2)
    while True:
        part = gen.SUB.match(text)
        paper.add("(%s%s) line 1" % (label, part.group(1)), A, "note", part.group(2),
                  "page %d, an English sentence set as a lettered part%s" % (
                      paper.page(line), ", # marking it infelicitous" if part.group(2).startswith("#") else ""))
        line += 1
        text = paper.text(line)
        if not gen.SUB.match(text):
            return line


blocks = {paper.find(r"^Table 1: Distribution"): table}
blocks.update({paper.find(r"^\(%d\) a\. " % one): english for one in (12, 13, 18)})
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
rows = paper.rows
# The elicitation method at the right of each Gitksan line, a note after it.
out = []
for row in rows:
    method = re.search(r"\s+(\((?:vf|sf)\))$", row[3])
    if row[1] == L and method:
        out.append(row[:3] + [row[3][:method.start()], row[4]])
        out.append([row[0], A, "note", method.group(1), row[4] + ", at the right, the elicitation method: " +
                    METHOD[method.group(1)]])
    else:
        out.append(row)
paper.rows = out
paper.write()
