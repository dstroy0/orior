"""The ops of 12_ICSNL55_Ikonnikova_final: Olga Ikonnikova on the verbal origin of nominals in the
Southern Interior Salish languages: the subordinator ł, the articles łuɁ and łiɁe, and the
nominalizer s-, from texts in Kalispel-Spokane-Flathead, Coeur d'Alene, Moses-Columbian and
Colville-Okanagan.

Each example is set under a caption naming its language, Coeur d’Alene:, a segmented form, its gloss
and a translation with its source; the rows of each take the caption's language. (23) is a formula
in brackets, a display. A form cited in the prose takes the language its paragraph names last
before it.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

L = gen.L
LANGUAGE = "Southern Interior Salish"
paper = gen.Paper("12_ICSNL55_Ikonnikova_final", authors="Olga Ikonnikova", language=LANGUAGE)
paper.opening = "auto"

NAMES = [("Olga Ikonnikova", "the author"), ("Carlson", "Barry F. Carlson, A Grammar of Spokan (1972)"),
         ("Black", "Deirdre J. Black, Spokane lexemes (1996)"), ("Kroeber", "Paul Kroeber, the clausal subordinator ł"),
         ("A. Mattina", "Anthony Mattina, Colville grammatical structure (1973), The Golden Woman (1985)"),
         ("N. Mattina", "Nancy Mattina, Moses-Columbia determiner phrases (2006)"),
         ("Reichard", "Gladys A. Reichard, Coeur d’Alene (1938) and her texts"), ("Speck", "Brenda J. Speck (1977)"),
         ("Czaykowska-Higgins", "Eva Czaykowska-Higgins and M. Dale Kinkade (1998)"),
         ("Peter J. Seymour", "the Colville narrator of The Golden Woman"), ("H. Vogt", "Hans Vogt, Kalispel Texts"),
         ("Camp", "Kelsi Camp, seven Kalispel texts (2007)"), ("Margaret Sherwood", "the teller of Badger and Skunk"),
         ("Egesdal", "Steven Egesdal (1991)"), ("Lindley", "Lottie Lindley and John Lyon, 12 Upper Nicola Okanagan Texts"),
         ("Doak", "Ivy Doak and Timothy Montler, Reichard's Coeur d’Alene texts (2006)"),
         ("Willet", "Marie L. Willet, Nxa’amxcin (2003)")]
LANGUAGES = [(LANGUAGE, "the branch the paper reads"),
             ("Kalispel-Spokane-Flathead", "Southern Interior Salish"), ("Spokane", "Kalispel-Spokane-Flathead"),
             ("Kalispel", "Kalispel-Spokane-Flathead"), ("Coeur d’Alene", "Southern Interior Salish"),
             ("Moses-Columbian", "Southern Interior Salish, Nxa’amxcin"), ("Colville-Okanagan", "Southern Interior Salish"),
             ("Okanagan", "Southern Interior Salish"), ("Colville", "Colville-Okanagan"),
             ("Upper Nicola Okanagan", "Okanagan"), ("Lillooet", "Northern Interior Salish"),
             ("Thompson", "Northern Interior Salish"), ("Proto-Salish", "the reconstructed ancestor"),
             ("Interior Salish", "the division"), ("Salish", "the family")]
CAPTIONS = ["Kalispel-Spokane-Flathead", "Coeur d’Alene", "Moses-Columbian", "Colville-Okanagan", "Okanagan"]
# (16) prints its caption without the colon.
paper.captions = CAPTIONS

paper.standard(["Olga Ikonnikova"], NAMES, LANGUAGES, front_languages=1, displays={"23": 2})

# Each example's rows take the language of the caption over it.
label = re.compile(r"^\((\d+)[a-z]*\) line")
captioned = {}
for where, who, kind, form, gloss in paper.rows:
    example = label.match(where)
    caption = re.sub(r":?[\d]*$", "", form)
    if example and kind == "note" and caption in CAPTIONS:
        captioned.setdefault(example.group(1), caption)
for row in paper.rows:
    example = label.match(row[0])
    if example and row[1] == L and example.group(1) in captioned:
        row[1] = captioned[example.group(1)]
print("# uncaptioned:", sorted({label.match(row[0]).group(1) for row in paper.rows
                                if label.match(row[0]) and row[1] == L}), file=sys.stderr)

# A form cited in the prose takes the language its paragraph names last before it.
names = [name for name, _ in LANGUAGES if name != LANGUAGE]
body = ""
for row in paper.rows:
    if row[2] in ("note", "heading"):
        body = row[3]
    elif row[2] == "cited form" and row[1] == L:
        at = body.find(row[3])
        before = body[:at] if at >= 0 else body
        named = [(before.rfind(name), name) for name in names if name in before]
        if named:
            row[1] = max(named)[1]
paper.write()
