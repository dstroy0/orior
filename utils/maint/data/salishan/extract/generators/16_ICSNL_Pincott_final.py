"""The ops of 16_ICSNL_Pincott_final: Ethan Pincott on when the *k > *č shift occurred in Central
Salish, from loanwords shared with Lillooet, Thompson, Wakashan and Chimakuan, the /kʷ/: /č/
correspondences inside the branch, and Halkomelem's labio-velar reduplication.

The paper sets no numbered examples. Its data stand in four tables, each a caption and a note to
each printed line: Table 1 and Table 2 give a gloss and the cognates under a language's
abbreviation, Table 3 a lettered PCS reconstruction with its /kʷ/ and /č/ reflexes, and Table 4 a
count of /č/ forms to each language.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Central Salish"
AUTHORS = ["Ethan Pincott"]
paper = gen.Paper("16_ICSNL_Pincott_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Galloway", "Brent D. Galloway, Proto-Central Salish sound correspondences (1988)"),
         ("Kuipers", "Aert H. Kuipers, Salish etymological dictionary (2002)"),
         ("Kinkade", "M. Dale Kinkade (1993, 2003), and with Czaykowska-Higgins (1997)"),
         ("Czaykowska-Higgins", "Ewa Czaykowska-Higgins, with Kinkade (1997)"),
         ("Suttles", "Wayne P. Suttles, Musqueam reference grammar (2004)"),
         ("Swadesh", "Morris Swadesh (1952, 1955)"), ("Boas", "Franz Boas, with Haeberlin (1927)"),
         ("Haeberlin", "Herman Haeberlin, with Boas (1927)"), ("van Eijk", "Jan P. van Eijk (1997, 2014)"),
         ("Fortescue", "Michael D. Fortescue, Comparative Wakashan dictionary (2007)"),
         ("Andrade", "Manuel J. Andrade, Chemakum and Quileute (1953)"),
         ("Egesdal", "Steven M. Egesdal, with Marita T. Thompson (1996)"),
         ("Kroeber", "Paul D. Kroeber, The Salish language family (1999)"),
         ("Jorgensen", "Joseph G. Jorgensen, Salish language and culture (1969)"),
         ("Squamish Nation Education Department", "the Squamish dictionary (2011)")]
LANGUAGES = [(LANGUAGE, "the branch of Salish the paper dates the shift in"),
             ("Proto-Central Salish", "PCS"), ("Proto-Salish", "PS"), ("Interior Salish", "the division"),
             ("Lillooet", "Li, Northern Interior Salish"), ("Thompson", "Th, Northern Interior Salish"),
             ("Comox", "Cx"), ("Sechelt", "Se"), ("Squamish", "Sq"), ("Chilliwack", "Ck, Upriver Halkomelem"),
             ("Musqueam", "Ms, Downriver Halkomelem"), ("Cowichan", "Cw, Island Halkomelem"),
             ("Halkomelem", "Cowichan, Musqueam and Chilliwack"), ("Saanich", "Sn, Northern Straits"),
             ("Songish", "Sg, Northern Straits"), ("Samish", "Sm, Northern Straits"), ("Klallam", "Kl"),
             ("Northern Straits", "Saanich, Songish and Samish"), ("Nooksack", "Nk"), ("Twana", "Tw"),
             ("Lushootseed", "Ld"), ("Moses-Columbian", "Southern Interior Salish"),
             ("Shuswap", "Northern Interior Salish"), ("Tsamosan", "a branch of Salish"),
             ("Cowlitz", "Tsamosan"), ("Chehalis", "Tsamosan"), ("Tillamook", "a branch of Salish"),
             ("Wakashan", "a neighboring family"), ("Kwak’wala", "Kw, Wakashan"), ("Ditidaht", "Di, Wakashan"),
             ("Chimakuan", "a neighboring family"), ("Chemakum", "Ch, Chimakuan"), ("Quileute", "Qu, Chimakuan")]


def table(start, where):
    """A table: its caption a note and each printed line under it a note, to a page break, a line
    of prose at the full measure, or a blank line with prose or another blank under it. Table 3
    sets a blank line inside its row a., between the letter and the cells."""
    here = re.match(r"^(Table \d+):", paper.text(start)).group(1)
    paper.add(here, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    number, count = start + 1, 0

    def ends(one):
        return one > paper.last or paper.lines[one][2] or len(paper.text(one)) >= 75

    while not ends(number):
        if not paper.text(number).strip():
            if ends(number + 1) or not paper.text(number + 1).strip():
                break
            number += 1
            continue
        count += 1
        paper.add("%s line %d" % (here, count), A, "note", paper.text(number),
                  "page %d, the table's line %d as printed" % (paper.page(number), count))
        number += 1
    return number


REFERENCES = paper.find(r"^References$")
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
# 3.1 opens its title on a slash, /kʷ/: /č/ correspondences, which the heading pattern leaves out.
HEADINGS = {number: re.match(r"^(\d+(?:\.\d+)?)\s", paper.text(number)).group(1)
            for number in range(1, REFERENCES) if number not in AT_FOOT
            and re.match(r"^\d(?:\.\d)? (?:[A-Z]|/kʷ/)", paper.text(number)) and not paper.text(number).endswith(".")}
blocks = {number: table for number in range(1, REFERENCES) if re.match(r"^Table \d+:", paper.text(number))}
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS)
paper.write()
