"""The ops of 04_ICSNL55_HDavis1_final: Henry Davis, The Burning Church at Shalalth, two eyewitness
accounts in St'át'imcets of the 1948 fire at Tsal'álh, Tommy Link's and Carl Alexander's.

Each narrative is set three ways: the St'át'imcets text (2.1, 3.1), written a transcription row a
paragraph; English translations (2.2, 2.3, 3.2), notes; and a fully analyzed version (2.4, 3.3), its
lines numbered as examples, each set segmented over its gloss (opening = "segmentation"). Carl
Alexander's analyzed lines number from (1) again and stand under §3.3. The two appendices after the
references, the glossing abbreviations and the conversion chart, are written a row a printed line.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "St’át’imcets"
paper = gen.Paper("04_ICSNL55_HDavis1_final", authors="Henry Davis", language=LANGUAGE)
paper.opening = "segmentation"

NAMES = [("Tommy Link", "narrator of the first account, recorded around 1964-5"),
         ("Christine L. J. Clark", "recorded Tommy Link's narrative in Lillooet Hospital"),
         ("Carl Alexander", "Qw7ayán’ak, narrator of the second account, recorded 2019"),
         ("Randy Bouchard", "BC Indian Languages Project recordings"),
         ("John Lyon", "the author's transcription partner"),
         ("Sam Mitchell", "Upper St'át'imcets storyteller"),
         ("Lisanne Tevlin", "helped record Carl Alexander's story"),
         ("Cathrena Narcisse", "Lillooet Tribal Council, provided the Tommy Link recording"),
         ("Dez Peters", "Grand Chief Desmond (Dez) Peters Sr. of Ts’k’wáylacw"),
         ("Jan van Eijk", "recorded Sam Mitchell"), ("Dale Kinkade", "worked on Cowlitz"),
         ("Frances Northover", "the last monolingual speaker of Cowlitz"),
         ("Francis Edwards", "Upper St'át'imc storyteller"), ("Nancy Turner", "ethnobotanist"),
         ("Brian Hayden", "archeologist"), ("Marianne Ignace", "co-wrote the first Upper St'át'imcets primer"),
         ("Bob Alexander", "held Tommy Link back from the burning hall"),
         ("P’xus", "dropped the lamp in Carl Alexander's account"),
         ("Súkwi", "carved the church statuary"), ("Frank Durbin", "his wife's money bought the electric bell"),
         ("Bill Casper", "in Carl Alexander's account")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish (Lillooet), ISO 639-3 lil"),
             ("Upper St’át’imcets", "the Fountain dialect"), ("Lower St’át’imcets", "the Mount Currie dialect"),
             ("Cowlitz", "Tsamosan Salish"), ("Nuxalk", "Bella Coola, also with number in its determiners"),
             ("English", "the code-mixing in Tommy Link's narrative"),
             ("Chinook Wawa", "the path of French loans"), ("French", "the source of the la- loans")]


def text_section(start, where):
    """The St'át'imcets text of a narrative, a transcription row to each paragraph, up to the
    next heading."""
    end = paper.find(r"^\d\.\d English", start)
    starts = paper.paragraph_starts()
    running = paper.running_numbers_set()
    paragraph = []

    def flush():
        if paragraph:
            paper.add(where, LANGUAGE, "transcription", paper.joined(paragraph),
                      "page %d, the narrative in St’át’imcets" % paper.page(paragraph[0]))
            del paragraph[:]

    for number in range(start, end):
        if paper.lines[number][2] or not paper.text(number) or number in running:
            continue
        if number in starts:
            flush()
        paragraph.append(number)
    flush()
    return end


blocks = {paper.find(r"^2\.1 St’át’imcets") + 2: text_section,
          paper.find(r"^3\.1 St’át’imcets") + 2: text_section}
found = paper.standard(["Henry Davis"], NAMES, LANGUAGES, front_languages=1, blocks=blocks,
                       restarts={"3.3": "§3.3 "}, appendix=r"^Appendix I ")

# The appendices, a row a printed line; Appendix II's column heads one row, and its footnote after
# its title.
running = paper.running_numbers_set()
notes = {one for parts, _ in found.values() for one in parts}
first = paper.find(r"^Appendix I ")
second = paper.find(r"^Appendix II ")
heads = paper.find(r"^Van Eijk", second)
chart = paper.find(r"^p p q q", heads)
here, count = "Appendix I", 0
number = first
while number <= paper.last:
    text = paper.text(number)
    if paper.lines[number][2] or not text or number in running or number in notes:
        number += 1
        continue
    if number == first:
        paper.add(here, A, "heading", text, "page %d" % paper.page(number))
    elif number == second:
        here, count = "Appendix II", 0
        paper.add(here, A, "heading", paper.joined([second, second + 1]), "page %d" % paper.page(number))
        paper.footnote("11", found["11"][0], found["11"][1], NAMES, LANGUAGES,
                       gloss="page %d, footnote 11" % found["11"][1])
        number = second + 2
        continue
    elif number == heads:
        count += 1
        paper.add("%s line %d" % (here, count), A, "note", paper.joined(range(heads, chart)),
                  "page %d, the column heads, twice over" % paper.page(number))
        number = chart
        continue
    else:
        count += 1
        why = "two entries side by side" if here == "Appendix I" else "van Eijk and (N)APA, two pairs side by side"
        # The retracted consonants' van Eijk letters are underlined on the page, ts, ts', s, l, l';
        # the text layer holds no underline.
        if here == "Appendix II" and text.split()[1] in ("c̣", "c̣̓", "ṣ", "ḷ", "ḷ̕"):
            why += ", the first cell underlined on the page"
        paper.add("%s line %d" % (here, count), A, "note", text, "page %d, %s" % (paper.page(number), why))
    number += 1
paper.write()
