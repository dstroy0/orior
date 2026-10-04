"""The ops of 08_ICSNL55_JDavis_2_final: John Hamilton Davis on the collective in Mainland Comox, the
{C1əC2-} plural, the collective plural suffix {-VW} as /-iw/ and /-ig-/, collectives by the root
vowel, the article system, and the collective article lhew [ɬuʊ].

The examples are set as in 07_ICSNL55_JDavis1_final, a. form and b. form side by side or one
example to the width of the page, each with an orthographic form, a phonetic [..], a phonemic /../
or morphological {..} form, a gloss and a translation: gen.Paper.columns reads each word into the
column it stands under. One heading opens on a morphological form, 2 {C1əC2-} plural, and the
headings are named here; the references are Sources consulted.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

LANGUAGE = "Mainland Comox"
paper = gen.Paper("08_ICSNL55_JDavis_2_final", authors="John Hamilton Davis", language=LANGUAGE)

NAMES = [("Noel George Harry", "Homalco speaker and storyteller, born circa 1890"),
         ("Bill Galligos", "Sliammon speaker, born 1908"), ("Mary George", "Mainland Comox speaker, born 1924"),
         ("Tommy Paul", "Homalco storyteller"), ("Marion Harry", "commented in Davis 2019 §9"),
         ("Chichila", "Mary George's great-grandmother"), ("Thanch", "Pileated Woodpecker, a shaman in the story"),
         ("Raven", "P'ah, in the story of the transformation of the birds"), ("Transformer", "in Tommy Paul's story"),
         ("Tarpent", "Marie-Lucie Tarpent, them Fred in Nishga English (1982)"),
         ("Gleason", "H. A. Gleason, Jr., morphophonemes (1961)"), ("Sapir", "Edward Sapir, Comox reduplication (1915)"),
         ("Boas", "Franz Boas, the Island Comox file slips"), ("Seymour", "Peter J. Seymour, Colville storyteller (2015)"),
         ("Beaumont", "Ronald C. Beaumont, Sechelt articles and dictionary"), ("Davis", "John H. Davis 2019")]
LANGUAGES = [(LANGUAGE, "ʔayʔaǰuθəm, Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("Homalco", "a dialect of Mainland Comox"), ("Klahoose", "a dialect of Mainland Comox"),
             ("Sliammon", "a dialect of Mainland Comox"), ("Salish", "the family"),
             ("Nishga", "Nisga'a, Tsimshianic; them Fred in its speakers' English"),
             ("English", "the contact language"), ("Sechelt", "Central Salish, the cognate /-aw/"),
             ("Island Comox", "Central Salish, /ʔawḱʷ/ 'all, every'"), ("Colville", "Southern Interior Salish, [həɬ=]"),
             ("Comox", "Mainland and Island Comox"), ("Tsimshian", "the family of Nishga")]

skip = {one for parts, _ in paper.page_footnotes().values() for one in parts}
end = paper.find(r"^Sources consulted$")
headings, blocks, seen = {}, {}, set()
for number in range(1, end):
    if number in skip:
        continue
    text = paper.text(number)
    heading = re.match(r"^(\d{1,2}) (?=[A-Z/{])", text)
    if heading and not gen.EXAMPLE.match(text) and len(text) < 80 and not text.endswith("."):
        headings[number] = heading.group(1)
    # An example's header stands over a bracketed tier; See also (33) below. is prose.
    under = next((one for one in range(number + 1, end) if paper.text(one) and one not in skip), None)
    if gen.EXAMPLE.match(text) and under and paper.text(under)[:1] in "[/{":
        label = "35:2" if text.startswith("(35)") and "35" in seen else None
        seen.add(gen.EXAMPLE.match(text).group(1))
        blocks[number] = lambda first, where, label=label: paper.columns(first, skip=skip, label=label)
paper.standard(["John Hamilton Davis"], NAMES, LANGUAGES, front_languages=1, blocks=blocks,
               headings=headings, references=r"^Sources consulted$")
paper.write()
