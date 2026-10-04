"""The ops of 10_ICSNL55_Greymorning_final: Neyooxet Greymorning on Accelerated Second Language
Acquisition (ASLA), its history, what his Arapaho classes at the University of Montana learned in
2008, 2013 and 2020, the resistance of language teachers to it, and the theory behind it.

The paper is prose with a few Arapaho sentences in its paragraphs and eleven block quotations. Each
quotation is set in from the margin on every line (gen.Paper.indented_runs) and is a note given to
whoever spoke or wrote it, the source the paper cites after it a citation row: Sterling
HolyWhiteMountain's words come from Greymorning 2019, and Jeff's from the NSILC video of 2017.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Arapaho"
paper = gen.Paper("10_ICSNL55_Greymorning_final", authors="Neyooxet Greymorning", language=LANGUAGE)

NAMES = [("Neyooxet Greymorning", "the author, University of Montana"),
         ("Steinhauers", "used ASLA to help their son Ty become a fluent speaker of Cree"),
         ("Ty", "the Steinhauers' son"), ("Zdnek Salzmann", "linguist, the Arapaho Dictionary of 1981"),
         ("Jon Roberts", "Language Teacher Education (1998)"), ("Mitchell and Martin", "rote learning (1997)"),
         ("Richards and Lockhart", "reflective teaching (1994)"), ("Salmon", "Psychology in the Classroom (1995)"),
         ("Crane, Yeager, and Whitman", "An Introduction to Linguistics (1981)"),
         ("Fromkin, Rodman, and Hyams", "An Introduction to Language (2011)"),
         ("Robert Hall", "Blackfoot, taught Blackfoot in the author's methods class in 2014"),
         ("Sterling HolyWhiteMountain", "Blackfoot, learned Blackfoot through ASLA"),
         ("Edward North Peigan", "Blackfoot speaker from southern Alberta"),
         ("Stephen Krashen", "Krashen's theory of second language acquisition (1998)"),
         ("Jeff", "a graduate student in the author's class in 2011"), ("J. P.", "the child of Fromkin et al.'s example")]
LANGUAGES = [(LANGUAGE, "Algonquian, taught by the author at the University of Montana"),
             ("Cree", "Algonquian, Ty Steinhauer's language"), ("Algonquian", "the family of Arapaho"),
             ("Blackfoot", "Algonquian, taught by Robert Hall"), ("English", "the learners' first language"),
             ("Spanish", "Jeff's classes in high school and college"), ("Chinese", "a language class compared"),
             ("French", "a language class compared"), ("German", "a language class compared"),
             ("Russian", "a language class compared")]
# The speaker or writer of each quotation, by its first words.
QUOTED = {"For anyone": "Jon Roberts", "By the age": "Crane, Yeager, and Whitman",
          "Most children": "Fromkin, Rodman, and Hyams", "During this stage": "Fromkin, Rodman, and Hyams",
          "The correct use": "Fromkin, Rodman, and Hyams", "Probably within": "Sterling HolyWhiteMountain",
          "For the first time": "Sterling HolyWhiteMountain", "Language acquisition": "Stephen Krashen",
          "The best methods": "Stephen Krashen", "In certain important": "Fromkin, Rodman, and Hyams",
          "I’m fluent": "Jeff"}
CITED = re.compile(r"\s*(\((?:[A-Z][^()]*?)\d{4}(?::\d+)?\))$")

ENGLISH = {"mama", "papa", "my", "pretty", "more", "dog", "milk", "mommy", "I love you", "I am hungry",
           "I am thirsty", "can I get a drink of water", "can I go to the bathroom", "Get mommy’s keys",
           "No, no, my keys", "No, no", "Bring mommy her keys", "daddy", "Say momma", "Say dada", "mamma", "dada"}


def form_language(run):
    """The language of an italic run: the English words a child or a learner says, cited as
    English; the Arapaho sentences, defined with 3 for the interdental and ’ for the glottal stop,
    and betii ‘mouth’; the titles of works are no forms."""
    if run in ENGLISH:
        return "English"
    if re.search(r"3|’", run) or run == "betii":
        return LANGUAGE
    return None


paper.form_language = form_language
skip = {one for parts, _ in paper.page_footnotes().values() for one in parts}
start = paper.find(r"^1\s+Finding the Path$")
runs = paper.indented_runs(start, paper.find(r"^References$"), skip=skip)


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


paper.standard(["Neyooxet Greymorning"], NAMES, LANGUAGES, front_languages=1,
               blocks={run[0]: quotation(run) for run in runs})
paper.write()
