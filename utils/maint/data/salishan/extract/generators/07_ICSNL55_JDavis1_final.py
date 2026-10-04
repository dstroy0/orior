"""The ops of 07_ICSNL55_JDavis1_final: John Hamilton Davis on telling apart the {-Vm} suffixes of
Mainland Comox, /-əm/ in place names, intransitives and reflexives, /-ʔəm/ on transitives, /-ʔam/
impending, /-am/ of relative position and plural, and /-igan/.

The examples are set in columns, a. form, b. form side by side, or several numbered examples to a
line, with an orthographic form, a phonetic [..], a phonemic /../ or morphological {..} form, a
gloss and a translation under each: gen.Paper.columns reads each word into the column it stands
under on the page. The paper numbers two sections 3 and opens some headings on the suffix, and its
headings are named here; its references are Sources consulted.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

LANGUAGE = "Mainland Comox"
paper = gen.Paper("07_ICSNL55_JDavis1_final", authors="John Hamilton Davis", language=LANGUAGE)

NAMES = [("Bill Galligos", "Mainland Comox speaker, born 1908"),
         ("Noel George Harry", "Mowheyalas, Homalco speaker and storyteller"), ("Noel Harry", "Nuwa"),
         ("Tommy Paul", "Swinney, Yapawtwh, storyteller"), ("Ambrose Wilson", "Homalco speaker"),
         ("Mary George", "Sliammon speaker"), ("Elsie Paul", "Sliammon speaker"),
         ("Adeline Francis", "Homalco speaker, Ambrose Wilson's sister"),
         ("Jimmy Wilson", "son of Ambrose Wilson"), ("Marion Harry", "remarked on the twins story"),
         ("Marshall Dominic", "brought the herrings, in Marion Harry's account"),
         ("Chichila", "Mary George's great-grandmother"), ("Beaumont", "Ronald C. Beaumont, the Sechelt dictionary"),
         ("Hinkson", "Mercedes Quesney Hinkson, Salishan lexical suffixes")]
LANGUAGES = [(LANGUAGE, "ʔayʔaǰuθəm, Comox-Sliammon, Central Salish, ISO 639-3 coo"),
             ("Sliammon", "a dialect of Mainland Comox"), ("Homalco", "a dialect of Mainland Comox"),
             ("Sechelt", "Central Salish, Beaumont's dictionary"), ("Puget Salish", "Central Salish"),
             ("Comox", "Mainland Comox")]

notes = paper.page_footnotes()
skip = {one for parts, _ in notes.values() for one in parts}
end = paper.find(r"^Sources consulted$")
headings, blocks = {}, {}
for number in range(1, end):
    if number in skip:
        continue
    text = paper.text(number)
    heading = re.match(r"^(\d{1,2}) (?=[A-Z/])", text)
    if heading and not gen.EXAMPLE.match(text) and len(text) < 80 and not text.endswith("."):
        headings[number] = heading.group(1)
    if gen.EXAMPLE.match(text):
        blocks[number] = lambda first, where: paper.columns(first, skip=skip)
paper.standard(["John Hamilton Davis"], NAMES, LANGUAGES, front_languages=1, blocks=blocks,
               headings=headings, references=r"^Sources consulted$")
# A footnote mark a column keeps on its row, ‘urinate’6 or rain-ʔam-appearance7, comes off the form,
# and the row's gloss records it.
for row in paper.rows:
    mark = re.search(r"(?:(?<=[^\W\d_])|(?<=’))(\d{1,2})$", row[3]) if "in columns" in row[4] else None
    if mark and mark.group(1) in notes:
        row[3] = row[3][:mark.start()]
        row[4] += ", carries footnote " + mark.group(1)
paper.write()
