"""The ops of 2001_Kinley: Sharon R. Kinley on Northern Straits from a Native perspective, the family
trees of Charley Edwards, Lena Daniels and Tommy Bob, and the language name Ləkʷəŋínəŋ they shared.

The paper is a scan and its text layer an OCR; residue's CORRECTIONS set each misread word as printed.
The abstract has no heading and no keywords, and gen.front writes each of its lines a note under the
author; the post-pass joins them into one. Figure 1 is a family tree, each name a name row: the bold
names are the line of descent and the plain names, joined by a double rule, their spouses. xx is the
tree's own mark for a spouse it does not name. The OCR reads the rules of the tree as two stray lines,
II and 1 -1, that the block passes over.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
AUTHORS = ["Sharon R. Kinley"]
paper = gen.Paper("2001_Kinley", authors=AUTHORS[0], language="Northern Straits")
NAMES = [("Wayne Suttles", "author of Some Questions about Northern Straits, in the same volume"),
         ("Charley Edwards", "one of the three people whose family trees the paper gives"),
         ("Lena Daniels", "one of the three people whose family trees the paper gives; a consultant"),
         ("Tommy Bob", "one of the three people whose family trees the paper gives"),
         ("Eva Guerin", "the author's late mother, a consultant"), ("Al Charles", "a consultant"),
         ("Theresa Gibbs", "a consultant"), ("Ivan Morris", "a consultant"), ("Emma Edwards", "a consultant"),
         ("David Tom", "deposed by a notary public on July 7, 1917")]
LANGUAGES = [("Northern Straits", "Central Salish, Straits Salish"),
             ("Ləkʷəŋínəŋ", "the language of the Samish, Lummi, Saanich and Songhees, in its speakers' name"),
             ("Samish", "Northern Straits, the Samish, as Tommy Bob named himself"),
             ("Lummi", "Northern Straits, the Lummi"), ("Saanich", "Northern Straits, the Saanich"),
             ("Songhees", "Northern Straits, the Songhees")]
# Figure 1, a line to a person: the printed line and who the person is in the tree.
TREE = {20: "the head of the tree", 21: "the spouse of So-chew-sum Sitlhe:mEltxw, unnamed",
        22: "a child of So-chew-sum Sitlhe:mEltxw", 23: "a child of So-chew-sum Sitlhe:mEltxw",
        24: "the child of Se-o-won", 25: "the spouse of So-sow-do-maut",
        26: "the child of So-sow-do-maut and Jack ChiElhEq", 27: "the spouse of Boston Tom TsitsilEm",
        28: "a child of Boston Tom TsitsilEm and Whee-wel-so", 29: "the spouse of David Tom",
        31: "the child of David Tom and Cecilia Sam Tom", 32: "the child of Elizabeth Tom",
        34: "a child of Boston Tom TsitsilEm and Whee-wel-so", 35: "the spouse of Cecilia Tom",
        36: "the child of Cecilia Tom and Harry Steele", 37: "a child of So-chew-sum Sitlhe:mEltxw",
        38: "the child of Deeamia", 39: "a spouse of Jack Edwards",
        40: "the child of Jack Edwards and Tskwsa'ba:lh", 41: "a spouse of Jack Edwards",
        42: "the child of Jack Edwards and Xdi'chElwEt", 43: "the child of Bob"}


def tree(start, where):
    """Figure 1, the family tree: a name row a person, then the caption a note."""
    for number, who in TREE.items():
        kind = "note" if paper.text(number) == "xx" else "name"
        paper.add("Figure 1", A, kind, paper.text(number),
                  "page 1, %s%s" % (who, ", the tree's mark for a spouse it does not name" if kind == "note" else ""))
    caption = paper.find(r"^Figure 1: ", start)
    paper.add("Figure 1", A, "note", paper.text(caption), "page 1, the figure's caption")
    return caption + 1


paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=2, blocks={20: tree})
# The abstract, four lines under the college, is one note.
lines = [index for index, row in enumerate(paper.rows) if row[0] == "front" and row[2] == "note"
         and row[4] == "page 1, under Sharon R. Kinley"][1:]
paper.rows[lines[0]][3] = " ".join(paper.rows[index][3] for index in lines)
paper.rows[lines[0]][4] = "page 1, the abstract"
paper.rows = [row for index, row in enumerate(paper.rows) if index not in lines[1:]]
# The reference reader runs two pairs of entries together. Genealogical field notes (n.d.) is a second
# entry under Suttles, Wayne, set indented under the first, and Bureau of Indian Affairs Land Records
# is an entry of its own after Edwards, Emma.
for opening, second, gloss in (("Suttles, Wayne", "Genealogical field notes (n.d.)",
                                "page 2, a second entry under Suttles, Wayne, set indented"),
                               ("Edwards, Emma", "Bureau of Indian Affairs Land Records.", "page 2")):
    at = next(index for index, row in enumerate(paper.rows) if row[2] == "reference" and row[3].startswith(opening))
    paper.rows[at][3] = paper.rows[at][3][:-len(second)].strip()
    paper.rows[at + 1:at + 1] = [paper.rows[at][:3] + [second, gloss]]
paper.write()
