"""The ops of 2012_Galloway: Brent Galloway's Work on the Nooksack file slips of Paul Fetzer, a report
on proofing the 7000 file slips Fetzer wrote in 1950 with George Swanaset, adding to each the
corrected IPA and the practical orthography, and what the slips show of Nooksack (Lhéchalosem).

The paper sets forty entries from the slips as LEXWARE bands, a label and its line: .o, Fetzer's IPA
as the research assistants entered it; Ipac, the IPA with the typing corrected; poc, the practical
orthography; TR, the English translation; dim, pl and dimpl; L and S, the linguist's and speaker's
initials; N, the file slip number; CM, the comments; and D, the date on the slip. The prose cites
Nooksack in the practical orthography in angle brackets and Fetzer's transcriptions in square
brackets, and Upriver Halkomelem after UHK.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Nooksack"
AUTHORS = ["Brent Galloway"]
paper = gen.Paper("2012_Galloway", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Fetzer", "Paul Fetzer, who wrote the Nooksack file slips in 1950 and died in 1952"),
         ("Melville Jacobs", "Fetzer's teacher, whose collection at the University of Washington holds the slips"),
         ("Elmendorf", "William Elmendorf, Fetzer's teacher, who used the modern Americanist IPA"),
         ("Amoss", "Pamela Amoss, whose Nooksack field notes and box of slips came to Galloway"),
         ("Efrat", "Barbara Efrat, whose Nooksack field notes came to Galloway"),
         ("Thompson", "Laurence C. (Larry) Thompson, whose Nooksack field notes came to Galloway"),
         ("Swanaset", "George Swanaset, one of the last two speakers of Nooksack, Fetzer's speaker"),
         ("Sindick Jimmy", "one of the last two speakers of Nooksack, whom Galloway recorded"),
         ("George Adams", "a member of the Nooksack Tribe who learned Lhéchalosem and teaches it"),
         ("Richardson", "Allan Richardson, with Galloway the book on Nooksack place names"),
         ("Kuipers", "Aert H. Kuipers, who developed the Classified Word List for B.C. Indian Languages"),
         ("Bouchard", "Randy Bouchard, who completed the Classified Word List for many B.C. languages"),
         ("Hsu", "Bob Hsu, whose LEXWARE program makes the dictionary from the bands"),
         ("Louisa George", "a Nooksack speaker Galloway heard, who did not stress the end of a word"),
         ("Renteria", "Catalina Renteria, with Galloway and Adams papers on Nooksack stories"),
         ("Suttles", "Wayne Suttles, whose Nooksack field notes and tapes of 1958 are cited"),
         ("Lottie Tom", "recorded with Swanaset by Suttles in 1958"),
         ("Boas", "Franz Boas, whose era's IPA Fetzer learned from Jacobs")]
LANGUAGES = [(LANGUAGE, "Central Salish, of northwest Washington, the language of the paper"),
             ("Lhéchalosem", "the Nooksack language in its own name"),
             ("Upriver Halkomelem", "Central Salish, the language the last Nooksack speakers also spoke"),
             ("Halkomelem", "Central Salish"),
             ("Proto-Salish", "the ancestor whose cognates help settle Nooksack forms"),
             ("Samish", "a Straits Salish dialect whose cognates help settle Nooksack forms"),
             ("English", "the language of the translations")]

# A form after UHK, or after Upriver Halkomelem, is Upriver Halkomelem's, UHK <ṓ kw'elmexw> in (36).
paper.language_before = lambda text: "Upriver Halkomelem" if re.search(r"(?:UHK|Halkomelem)\s*[<\[]?$", text) \
    else None

# The paper sets no form in italics: Nooksack stands in angle brackets in the practical orthography,
# and Fetzer's transcriptions and single sounds in square brackets. Each bracketed form is read as a
# run of its page. A square bracket holding English, [with some typos] or [lit. “black of eye”], is
# no form: every word of a form there holds a letter past plain ASCII, or the form is one word.
BRACKETED = {}
pages = {}
for number in range(1, paper.last + 1):
    if paper.text(number):
        pages.setdefault(paper.page(number), []).append(paper.text(number))
italics = paper.italics()
for page, texts in pages.items():
    # An angle bracket stands inside a square one, <ṓ kw’elmexw> in the gloss of (36), and each kind
    # is found apart.
    matches = list(re.finditer(r"<([^<>]+)>()", " ".join(texts))) + \
        list(re.finditer(r"()\[([^\[\]]+)\]", " ".join(texts)))
    for match in sorted(matches, key=lambda one: one.start()):
        run = (match.group(1) or match.group(2)).strip()
        if match.group(2) and (re.search(r"[,“”<]|\blit\.|^[A-Z]", run) or
                               (" " in run and not all(re.search(r"[^A-Za-z.]", one) for one in run.split()))):
            continue
        # Four forms in one bracket, <kwin, qwin, kw'in, or qw'in> on page 24, are four forms.
        for one in re.split(r",\s*(?:or\s+)?|\s+or\s+", run) if match.group(1) else [run]:
            BRACKETED.setdefault(one, "in angle brackets" if match.group(1) else "in square brackets")
            italics.setdefault(page, []).append(one)
# The practical orthography defines Nooksack in plain letters, <kwul> and <ay>, which the common test
# for a form refuses; a bracketed run is a form for the brackets.
paper.form_language = lambda run: L if run in BRACKETED or gen.orthographic(run) else None

BANDS = {".o": ("transcription", "the .o band, Fetzer’s IPA as the research assistants entered it"),
         "Ipac": ("transcription", "the Ipac band, the IPA with the typing corrected"),
         "poc": ("transcription", "the poc band, the practical orthography with corrections"),
         "TR": ("translation", "the TR band, the English translation"),
         "dim": ("transcription", "the dim band, the diminutive"),
         "pl": ("transcription", "the pl band, the plural"),
         "dimpl": ("transcription", "the dimpl band, the diminutive plural"),
         "L": ("note", "the L band, the linguist’s initials"),
         "S": ("note", "the S band, the speaker’s initials"),
         "N": ("citation", "the N band, the file slip number"),
         "CM": ("note", "the CM band, comments"),
         "D": ("note", "the D band, the date on the file slip")}
BAND = re.compile(r"^(\.o|Ipac|poc|TR|dimpl|dim|pl|L|S|N|CM|D)(?:\s+(.*))?$")
SKIP = paper.running_numbers_set()


def entry(start, where):
    """An entry from the file slips, each band a row: a band's wrapped lines run on until the next
    label. The entry ends at its D band, or where no D follows, after its comment closes."""
    label = re.match(r"\((\d+)\)", paper.text(start)).group(0)
    bands = []
    number = start
    while number <= paper.last:
        if number in SKIP or not paper.text(number):
            number += 1
            continue
        text = re.sub(r"^\(\d+\)\s+", "", paper.text(number)) if number == start else paper.text(number)
        band = BAND.match(text)
        if band:
            bands.append([band.group(1), [band.group(2) or ""], number])
        elif bands[-1][0] in ("CM", "D", "L", "S", "N") and (bands[-1][0] != "CM" or not bands[-1][1][-1] or
                                                           re.search(r"(?:\)|\]BG|\])$", bands[-1][1][-1])):
            break
        else:
            bands[-1][1].append(text)
        number += 1
        if bands[-1][0] == "D":
            break
    for name, texts, at in bands:
        value = " ".join(one for one in texts if one).strip()
        if not value:
            continue
        kind, gloss = BANDS[name]
        page = "page %d, %s" % (paper.page(at), gloss)
        if name in ("pl", "dim", "dimpl"):
            # The plural sets Fetzer's IPA and then the practical orthography in angle brackets, two
            # of them in (27), <aytl’étl’ex̱wítel> or <ay tl’éx̱wtl’ex̱wítel>.
            paper.add(label, L, kind, value.split("<")[0].strip(), page + ", Fetzer’s IPA")
            for practical in re.findall(r"<([^>]*)>", value):
                paper.add(label, L, kind, practical, page + ", the practical orthography in angle brackets")
                paper.cited_done.add(practical)
        else:
            paper.add(label, L if kind == "transcription" else A, kind, value, page)
        if name == "CM":
            paper.cited(label, value, [paper.page(at)])
            paper.mentions(label, value, NAMES, "name")
            paper.mentions(label, value, LANGUAGES, "language")
    return number


BLOCKS = {number: entry for number in range(1, paper.last + 1)
          if re.match(r"^\(\d+\)\s+\.o\b", paper.text(number))}
# The front matter sets the title, the author and his post, and an abstract with no heading over the
# first section; the section numbers stand in bold at the margin, where the common reader takes
# them for footnotes, and the paper is walked here part by part.
title = paper.find(r"^Work on the Nooksack file slips")
paper.add("front", A, "title", paper.text(title), "page 1")
paper.add("front", A, "name", AUTHORS[0], "author")
paper.mentioned.add(("name", AUTHORS[0]))
paper.add("front", A, "note", paper.text(title + 2), "page 1, under %s" % AUTHORS[0])
intro = paper.find(r"^1 Introduction$")
abstract = paper.joined(range(title + 3, intro))
paper.add("front", A, "note", abstract, "page 1, the abstract")
paper.cited("front", abstract, [1])
paper.mentions("front", abstract, NAMES, "name")
for name, why in LANGUAGES[:1]:
    paper.add("front", A, "language", name, why)
    paper.mentioned.add(("language", name))
paper.language_names = {name for name, _ in LANGUAGES}
references = paper.find(r"^References$")
paper.flow(intro, references - 1, "front", headings=paper.headings(intro, references - 1), blocks=BLOCKS,
           names=NAMES, languages=LANGUAGES)
paper.add("references", A, "heading", paper.text(references), "page %d" % paper.page(references))
address = paper.find(r"^Brent Galloway$", references)
for reference, at in paper.references(references + 1, address - 1):
    paper.add("references", A, "reference", reference, "page %d" % at)
paper.add("end", A, "note", paper.joined(range(address, paper.last + 1)),
          "page %d, the author's address" % paper.page(address))
for row in paper.rows:
    if row[2] == "cited form" and ", in italics" in row[4]:
        form = row[3].strip("*-–√…")
        row[4] = row[4].replace("in italics", BRACKETED.get(row[3]) or BRACKETED.get(form) or "in brackets")
# The common reader takes <kwenkwenát> of (36)'s comment for the head of <kwenkwenát-s> in the prose
# above it, with the hyphen after; the form stands alone in the comment, and its row goes there.
for row in list(paper.rows):
    bare = row[3].rstrip("-")
    if row[2] == "cited form" and row[3].endswith("-") and row[3] not in BRACKETED and bare in BRACKETED:
        home = next((one for one in paper.rows if one[2] == "note" and "<%s>" % bare in one[3]), None)
        if home:
            paper.rows.remove(row)
            row[0], row[3] = home[0], bare
            paper.rows.insert(paper.rows.index(home) + 1, row)
paper.write()
