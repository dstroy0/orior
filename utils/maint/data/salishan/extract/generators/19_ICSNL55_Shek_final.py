"""The ops of 19_ICSNL55_Shek_final: Madeleine Shek on Accelerated Second Language Acquisition (ASLA),
the theories of first and second language acquisition it draws on (Behaviorism, Mentalism,
Interactionism, Krashen's five hypotheses, Asher's Total Physical Response), her own Arapaho class
under Neyooxet Greymorning, and Robert Hall's thoughts as a student and teacher of the method.

The paper is prose with one Arapaho sentence in its paragraphs and five block quotations. Each
quotation is set in from the margin on every line (gen.Paper.indented_runs) and is a note given to
whoever spoke or wrote it, the source the paper cites after it a citation row: Richard Littlebear's
words come from Cantoni 2007, and Robert Hall's four from a personal communication.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Arapaho"
AUTHORS = ["Madeleine Shek"]
paper = gen.Paper("19_ICSNL55_Shek_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Madeleine Shek", "the author, University of Montana"),
         ("Neyooxet Greymorning", "developed the ASLA method, the author's Arapaho teacher"),
         ("Robert Hall", "Director of Native American Studies at Browning Public Schools, taught Blackfoot by ASLA"),
         ("Richard Littlebear", "Cheyenne speaker, elder, teacher, and activist"),
         ("B. F. Skinner", "the Behaviorist theory (1957)"), ("Lev Vygotsky", "socio-cultural theory"),
         ("Chomsky", "Mentalism or Innatism, the Language Acquisition Device"),
         ("Krashen", "the Five Hypotheses About SLA (1982)"),
         ("John Asher", "the Total Physical Response method (1977)"),
         ("Evan Gardner", "Where Are Your Keys?"),
         ("Ty Steinhauer", "learned Cree, his ancestral language, through ASLA"),
         ("William Big Bull", "Blackfeet elder, devised the Big Bull writing system")]
LANGUAGES = [(LANGUAGE, "Algonquian, the language the author learned through ASLA"),
             ("Hinono’eitiit", "the Arapaho name of Arapaho"),
             ("Cheyenne", "Algonquian, Richard Littlebear's language"),
             ("Tsėhesenėstsestotse", "the Cheyenne name of Cheyenne"),
             ("Blackfoot", "Algonquian, Robert Hall's heritage language"),
             ("Niitsii•ṗo′•″sin", "the Blackfoot name of Blackfoot, in the Big Bull writing system"),
             ("Cree", "Algonquian, Ty Steinhauer's ancestral language"),
             ("English", "the learners' first language"), ("French", "a language the author studied"),
             ("Mandarin", "a language the author studied"), ("Japanese", "a language with a speech community"),
             ("German", "a language with a speech community")]
# The speaker or writer of each quotation, by its first words.
QUOTED = {"Richard Littlebear thought": "Richard Littlebear", "Imagine if": "Robert Hall",
          "I think the power": "Robert Hall", "Learning an endangered": "Robert Hall", "Yes. I’m using": "Robert Hall"}
CITED = re.compile(r"\s*(\((?:[A-Z][^()]*?)(?:\d{4}|p\.c\.)(?::\d+)?\))$")


def form_language(run):
    """The language of an italic run: the Arapaho sentence of §6; the titles of works are no forms."""
    if run.startswith("Hisei"):
        return LANGUAGE
    return None


paper.form_language = form_language
skip = {one for parts, _ in paper.page_footnotes().values() for one in parts}
start = paper.find(r"^1\s+Introduction$")
# Each quotation ends on the source cited after it. The paragraph after Littlebear's opens on a line
# set in as far, Before taking up Arapaho, and is no part of the quotation.
runs = []
for run in paper.indented_runs(start, paper.find(r"^References$"), skip=skip):
    ends = [index for index, number in enumerate(run) if CITED.search(paper.text(number).strip())]
    runs.append(run[:ends[0] + 1] if ends else run)


def quotation(lines):
    def write(first, where):
        body = paper.joined(lines)
        who = next(name for opening, name in QUOTED.items() if body.startswith(opening))
        cited = CITED.search(body)
        gloss = "page %d, a block quotation" % paper.page(first)
        paper.add(where, who, "note", body, gloss + (", cited %s" % cited.group(1) if cited else ""))
        paper.mentions(where, body, LANGUAGES, "language")
        if cited:
            paper.add(where, A, "citation", cited.group(1)[1:-1], "the source of the quotation above")
        return lines[-1] + 1
    return write


paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1,
               blocks={run[0]: quotation(run) for run in runs})

# Big Bull, William opens its entry on a surname of two words, which references() does not take for
# an entry's opening; the Asher entry above holds it.
BIG_BULL = " Big Bull, William. 2018."
asher = next(row for row in paper.rows if row[2] == "reference" and BIG_BULL in row[3])
at = asher[3].index(BIG_BULL)
paper.rows.insert(paper.rows.index(asher) + 1, [asher[0], asher[1], "reference", asher[3][at + 1:], asher[4]])
asher[3] = asher[3][:at]
paper.write()
