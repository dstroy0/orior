"""The ops of 09_ICSNL55_Galligos_et_al._final: Karen Galligos, D. K. E. Reisinger, Gloria M.
Mellesmoen, Marianne Huijsmans and Laura Sehyun Griffin on an ʔayʔaǰuθəm telling of Aesop's fable
The Fisher and the Little Fish, told by the late Karen Galligos in September 2019.

The paper gives the fable in English (§2), the ʔayʔaǰuθəm text in the orthography (§3), a direct
English translation (§4) and the text glossed line by line (§5): each glossed line the orthographic
line, its morphophonemic segmentation, a gloss and a translation, which gen.Paper.example reads. The
§3 text is a transcription row to each sentence, a quotation kept with the words that say who
spoke it; the §4 translation is a translation row for its title and one for its paragraph.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

LANGUAGE = "ʔayʔaǰuθəm"
AUTHORS = ["Karen Galligos", "D. K. E. Reisinger", "Gloria M. Mellesmoen", "Marianne Huijsmans",
           "Laura Sehyun Griffin"]
paper = gen.Paper("09_ICSNL55_Galligos_et_al._final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Karen Galligos", "the teller, ʔayʔaǰuθəm speaker (1954–2020)"),
         ("Betty Wilson", "ʔayʔaǰuθəm speaker who helped with the transcription"),
         ("Daniel Reisinger", "elicited the line-by-line translation"), ("Laura Griffin", "elicited the line-by-line translation"),
         ("Aesop", "the fable, in Joseph Jacobs's edition (1922)"), ("Joseph Jacobs", "editor of The Fables of Æsop"),
         ("Watanabe", "Honoré Watanabe, the grammar of Sliammon (2003) and the aspectual suffix (2016)")]
LANGUAGES = [(LANGUAGE, "Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("Comox Sliammon", "ʔayʔaǰuθəm"), ("Comox-Sliammon", "ʔayʔaǰuθəm"), ("Central Salish", "the branch"),
             ("Salish", "the family"), ("English", "the fable's prompt and the translations"),
             ("Sliammon", "ʔayʔaǰuθəm, as Watanabe names it")]
TEXT = paper.find(r"^3 ʔayʔaǰuθəm Text:")


def text(start, where):
    """§3: the telling in the orthography, a transcription row to each sentence."""
    number, lines = start, []
    while paper.text(number) and not gen.HEADING.match(paper.text(number)):
        lines.append(number)
        number += 1
    for index, sentence in enumerate(gen.sentences(paper.joined(lines)), 1):
        paper.add("§3 sentence %d" % index, LANGUAGE, "transcription", sentence,
                  "page %d, the telling in the orthography" % paper.page(start))
    return number


def translation(start, where):
    """§4: the paper's own translation of the telling, its title and its paragraph."""
    number, words = start, []
    while not gen.HEADING.match(paper.text(number)):
        if paper.text(number).strip():
            words.append(number)
        number += 1
    paper.add("§4 line 1", gen.A, "translation", paper.text(words[0]), "page %d, the title" % paper.page(words[0]))
    paper.add("§4 line 2", gen.A, "translation", paper.joined(words[1:]),
              "page %d, the telling in English" % paper.page(words[1]))
    return number


TRANSLATION = paper.find(r"^4 Direct English Translation$")
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks={TEXT + 1: text, TRANSLATION + 1: translation})
paper.write()
