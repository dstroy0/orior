"""The ops of 1_Expressing-Future-Certainty-in-Comox: John Hamilton Davis on the future enclitic sem
of Comox and the sequence sem plus t, an emphatic future of the speaker's certainty, from sentences
Mary George and other Sliammon and Homalco speakers said from 1969 to 1980 and two of Hagège's; also
Hagège's emphatic periphrasis for a lexical suffix, the voiced stops of Salish emphasis, and the
origin of the name Comox.

Examples (1) and (2) set their parts side by side, a. form and b. form, and gen.Paper.columns reads
each word into the column it stands under. Every other example is one form to the width of the page,
(4a) and (14b) numbered with their letter: the practical orthography, a phonetic line in brackets,
a gloss and a translation, each tier a line under the form. (4a) and (13) run their three tiers on
over a second group of lines set in under the first, each line a tier's continuation in the same
order.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Comox"
AUTHORS = ["John Hamilton Davis"]
paper = gen.Paper("1_Expressing-Future-Certainty-in-Comox", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("George Gibbs", "the earliest documentation of the language, 1857, published 1877"),
         ("Mary George", "Sliammon speaker who taught the author, 1969 to 1980"),
         ("Wayne Suttles", "first noted the analysis of zem"), ("Tommy Paul", "used the word chîanas"),
         ("Marion Harry", "replied with the emphatic future on the phone"),
         ("Hagège", "Claude Hagège, Le comox lhaamen (1981) and lexical suffixes (1978)"),
         ("T’al", "the basket ogress of the story"),
         ("Noel George Harry", "Homalco speaker and storyteller, born circa 1890"),
         ("Ronald Beaumont", "the Sechelt baby talk"), ("Franz Boas", "recorded the name Çatloltq")]
LANGUAGES = [(LANGUAGE, "Mainland Comox, Central Salish, spoken at Sliammon and by the Homalco"),
             ("Homalco", "a dialect of Mainland Comox"), ("Sliammon", "a dialect of Mainland Comox"),
             ("Slavic", "the same [č] to [š] change"), ("Coast Salish", "the branch"),
             ("English", "the contact language"), ("Sechelt", "Central Salish, baby talk with /b/ and /d/"),
             ("Island Comox", "Thalholhtwh, Gibbs' vocabulary"), ("French", "Hagège's translations")]

FOOTNOTES = paper.page_footnotes()
SKIP = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers_set()
REFERENCES = paper.find(r"^References$")
LABEL = re.compile(r"^\((\d{1,2}[a-z]?)\)\s+(.*)$")


def single(start, where):
    """One example to the width of the page from line start: its form after the number, then a line
    for each tier set under the form, and a translation in quotes. A group of lines set in further
    under the first carries each tier on in the same order. The example ends at a new number or a
    line left of the form's column."""
    label, form = LABEL.match(paper.text(start)).groups()
    column = paper.word_positions(start)[1][0]
    tiers, lefts, translation, pages = [form], [column], [], [paper.page(start)]
    number = start + 1
    while number < REFERENCES:
        text = paper.text(number)
        if paper.lines[number][2] or not text or number in RUNNING or number in SKIP:
            number += 1
            continue
        left = paper.word_positions(number)[0][0]
        if LABEL.match(text) or left < column - 8:
            break
        if text.startswith("‘") or translation and not re.search(r"’\W*$", translation[-1]):
            translation.append(text)
        elif left > column + 10:
            # A tier carried on: the first line set in carries the form on, each after it the next tier.
            carried = sum(1 for one in lefts if one > column + 10)
            tiers[carried] += " " + text
            lefts.append(left)
        else:
            tiers.append(text)
            lefts.append(left)
        number += 1
    for count, text in enumerate(tiers, 1):
        kind = "transcription" if count == 1 else "phonetic" if text.startswith("[") else "gloss"
        paper.add("(%s) line %d" % (label, count), gen.L, kind, " ".join(text.split()), "page %d" % pages[0])
    if translation:
        said = " ".join(" ".join(translation).split())
        french = ", Hagège's translation in French" if label in ("17", "18") else ""
        paper.add("(%s) line %d" % (label, len(tiers) + 1), A, "translation", said, "page %d%s" % (pages[0], french))
    return number


# The practical orthography is defined in plain letters, and the italic runs of the prose that are
# its forms are named here. Boas's letter ç in Section 6 is a definition and no form. The enclitic t
# and the proclitic s are cited below: a paragraph that holds one sets it in brackets first, [səm t].
FORMS = {"sem", "zem", "ga", "xigap", "chîanas", "th"}
paper.form_language = lambda run: gen.L if run in FORMS else None

HEADINGS = paper.headings(1, REFERENCES - 1, skip=SKIP)
blocks = {}
for number in range(1, REFERENCES):
    if number in SKIP:
        continue
    found = LABEL.match(paper.text(number))
    if found:
        blocks[number] = (lambda first, where: paper.columns(first, skip=SKIP)) \
            if gen.SUB.match(found.group(2)) else single
# Note 1 is marked on the title, Comox1.
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS,
               notes_title=tuple(gen.TITLE_MARKS) + ("1",))

# yəm-igan-t-as breaks over a line after its first hyphen, and the italic reader keeps the break
# where the note joins the lines: the form and the phonetic form after it are cited here, each with
# its gloss, after the note that holds them.
compare = next(row for row in paper.rows if row[2] == "note" and "Compare yəm-" in row[3])
at = paper.rows.index(compare) + 1
paper.rows[at:at] = [
    [compare[0], gen.L, "cited form", "yəm-igan-t-as", "page 6, in italics, ‘kick-ribs-INTENT-TR-AGENT’"],
    [compare[0], gen.L, "cited form", "[yɪmegᴧtᴧs]", "page 6, ‘he kicked him in the ribs’"]]
for opens, form, page in (("In the data collected from 1969", "t", 4), ("This s is not a prefix", "s", 6)):
    holder = next(row for row in paper.rows if row[2] == "note" and row[3].startswith(opens))
    at = paper.rows.index(holder) + 1
    paper.rows[at:at] = [[holder[0], gen.L, "cited form", form, "page %d, in italics" % page]]
paper.write()
