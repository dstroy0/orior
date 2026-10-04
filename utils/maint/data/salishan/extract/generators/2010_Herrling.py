"""The ops of 2010_Herrling: Summertime, a Halq'emeylem story told by Th'athelex̱wot, Elizabeth
Herrling, to the Stolo Shxweli Halq'emeylem Language Program.

The story is thirty-three numbered sentences, each set as its words over their glosses, word by
word in columns, the pair wrapped onto a second and third pair where the sentence runs long, and a
translation in single quotes under them. The glosses of the first six sentences are set in lower
case (det, 3sub), and gen.example cannot tell the two tiers apart there; the block here reads the
lines above the translation as word and gloss in turn. residue's CORRECTIONS put back the underlined
x̱ the text layer drops, the Ō of (2), and the underscore of (12)'s qw'o_l the layer sets on a line
of its own. The sketches carry no captions. The program's address closes the paper.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
AUTHORS = ["Elizabeth Herrling"]
paper = gen.Paper("2010_Herrling", authors=AUTHORS[0], language="Halq'emeylem")
NAMES = [("Thelma Wenman", "who administered the project"),
         ("Laura Weelaylaq", "a contributor to the Wiki"), ("Stella (Kwosel) Pettis", "a contributor to the Wiki"),
         ("Vange Point", "a contributor to the Wiki"), ("Evelyn Peters", "a contributor to the Wiki"),
         ("Viviane Williams", "a contributor to the Wiki"), ("Jon Williams", "a contributor to the Wiki"),
         ("Judy Douglas", "a contributor to the Wiki"), ("Kasey Chapman", "a contributor to the Wiki"),
         ("Carol Peters", "a contributor to the Wiki"),
         ("Patrick Littell", "who customized Mediawiki and set up the community wiki"),
         ("Strang Burton", "who drew the rough illustrations"),
         ("Jared Deck", "who drew the finished illustration of mosquitos")]
LANGUAGES = [("Halq'emeylem", "Upriver Halkomelem, the language of the story"),
             ("Upriver Halkomelem", "Halq'emeylem, in its English name")]
# The forms a translation cites between slashes, each with what the translation says of it.
CITED = {9: ("/lhemkiya/", "a cast iron pot"), 13: ("/otheqt/", "what she should have said"),
         29: ("/sch'alhtel/", "anything you hang up to dry")}
EXAMPLE = re.compile(r"^\((\d+)\) (.*)$")
running = paper.running_numbers_set()


def usable(number):
    return not paper.lines[number][2] and paper.text(number) and number not in running


paper.add("front", A, "title", paper.text(2), "page 1")
paper.add("front", A, "name", "Elizabeth Herrling", "author")
paper.add("front", A, "name", "Th'athelex̱wot", "Elizabeth Herrling's Halq'emeylem name, in the author line")
paper.mentioned.update({("name", "Elizabeth Herrling"), ("name", "Th'athelex̱wot")})
paper.add("front", A, "note", paper.text(3), "page 1, the author line")
paper.add("front", A, "note", paper.text(4), "page 1, under the author line, carries footnote 1")
for name, why in LANGUAGES[:1]:
    paper.add("front", A, "language", name, why)
    paper.mentioned.add(("language", name))
paper.footnote("1", list(range(20, 28)), 1, names=NAMES, languages=LANGUAGES)
# The paragraph over the first sketch is set as a block, its lines with no indent to part them.
body = paper.joined(range(5, 20))
paper.add("front", A, "note", body, "page 1, the paragraph introducing the story")
paper.mentions("front", body, NAMES, "name")
paper.mentions("front", body, LANGUAGES, "language")

number = 29
while number <= 176:
    opened = EXAMPLE.match(paper.text(number)) if usable(number) else None
    if not opened:
        number += 1
        continue
    label, text = opened.group(1), opened.group(2)
    where = "(%s) line %%d" % label
    count, tier = 0, 0
    while True:
        page = paper.page(number)
        if text.startswith("‘"):
            said = text
            while not said.endswith("’"):
                number += 1
                while not usable(number):
                    number += 1
                said += " " + paper.text(number)
            count += 1
            paper.add(where % count, A, "translation", said, "page %d" % page)
            if int(label) in CITED:
                form, gloss = CITED[int(label)]
                paper.add(where % count, L, "cited form", form, "page %d, in the translation, %s" % (page, gloss))
            break
        count += 1
        paper.add(where % count, L, "gloss" if tier % 2 else "transcription", text, "page %d" % page)
        tier += 1
        number += 1
        while not usable(number):
            number += 1
        text = paper.text(number)
    number += 1

address = [177, 178, 179]
body = paper.joined(address)
paper.add("end", A, "note", body, "page 9, the program's address, closing the paper")
paper.mentions("end", body, NAMES, "name")
paper.write()
