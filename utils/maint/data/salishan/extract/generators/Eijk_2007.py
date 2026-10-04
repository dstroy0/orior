"""The ops of Eijk_2007: Jan P. van Eijk's Súnułqaz': The quest continues, further sources on the
two-headed serpent of Salish and Northwest Coast cosmology after van Eijk (1992) and (2001).

The paper is a scan and its text layer an OCR; residue's CORRECTIONS set each misread word as printed.
The OCR keeps no italics, and the cited forms are named here with the language each is cited from:
Lillooet súnułqaz', Klamath may, biblant n'os gitk and la·ba n'os gitk from Barker (1963), and
Quileute t'abale from Powell and Jensen (1976). The abstract has no heading and no keywords, and
gen.front writes its lines each a note; the post-pass joins them. The paper numbers both its second
and third sections 2.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper("Eijk_2007", authors=AUTHORS[0], language="Lillooet")
NAMES = [("Compton", "Compton et al. (1993), on Shuswap ethnoherpetology"),
         ("Johnson", "E. Pauline Johnson, Legends of Vancouver (1961)"),
         ("Chief Joe Capilano", "who told Johnson the Squamish legend The Sea-Serpent"),
         ("Sagalie Tyee", "God, in the Sea-Serpent legend"), ("Tenas Tyee", "Little Chief, the young brave of the legend"),
         ("Barker", "Muhammad A. R. Barker, Klamath Dictionary (1963)"),
         ("Powell and Jensen", "Jay Powell and Vicky Jensen, Quileute (1976)")]
LANGUAGES = [("Lillooet", "Northern Interior Salish, the language of súnułqaz’"),
             ("Salish", "the family"), ("Wakashan", "the neighboring family, its cosmology"),
             ("Shuswap", "Northern Interior Salish, Secwépemc"), ("Secwépemc", "Shuswap, in its speakers' name"),
             ("Squamish", "Central Salish, the legend's source"), ("Klamath", "Barker's dictionary"),
             ("Quileute", "Chimakuan, Powell and Jensen's description")]
# Each cited form: the paragraph it follows, by its opening words, and who it is cited from.
CITED = [("In van Eijk (1992)", "súnułqaz’", "Lillooet", "the two-headed serpent"),
         ("Barker’s (1963)", "may", "Klamath", "‘two-headed snake’"),
         ("Barker’s (1963)", "biblant n’os gitk", "Klamath", "“having-a-head-on-both-ends”"),
         ("Barker’s (1963)", "la·ba n’os gitk", "Klamath", "“having-two-heads,” carries footnote 1"),
         ("Barker’s (1963)", "may", "Klamath", "homophonous with the word for ‘tule’"),
         ("Barker’s (1963)", "may", "Klamath", "applies to may"),
         ("Powell and Jensen", "t’abale", "Quileute", "a two-headed lizard or salamander")]
# The OCR keeps no indents: the paragraphs open on these lines, read off the page.
paper._starts = {5, 9, 18, 21, 26, 36, 46, 51, 68, 70}
# The third section prints its number 2 again; it is §3 here.
paper.standard(AUTHORS, NAMES, LANGUAGES, headings={8: "1", 20: "2", 69: "3"})
paper.rows = [row for row in paper.rows if row[2] != "cited form"]
# The reference reader runs Powell and Jensen (1976), both Van Eijk entries and the author's address
# on page 3 together. The address closes the paper and is a note.
at = next(index for index, row in enumerate(paper.rows) if row[2] == "reference" and row[3].startswith("Powell"))
powell, rest = paper.rows[at][3].split(" Van Eijk, Jan P. 1992. ")
first, rest = rest.split(" Van Eijk, Jan P. 2001. ")
second, address = rest.split(" jvaneijk@")
paper.rows[at][3] = powell
paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + ["Van Eijk, Jan P. 1992. " + first, "page 3"],
                             paper.rows[at][:3] + ["Van Eijk, Jan P. 2001. " + second, "page 3"],
                             [paper.rows[at][0], A, "note", "jvaneijk@" + address, "page 3, the author's address"]]
for opening, form, who, gloss in reversed(CITED):
    at = next(index for index, row in enumerate(paper.rows) if row[2] == "note" and opening in row[3])
    paper.rows[at + 1:at + 1] = [[paper.rows[at][0], who, "cited form", form, "%s, %s" % (paper.rows[at][4], gloss)]]
# The abstract, three lines under the university, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Jan P. van Eijk"][1:]
paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
paper.rows[lines[0]][4] = "page 1, the abstract"
paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
paper.write()
