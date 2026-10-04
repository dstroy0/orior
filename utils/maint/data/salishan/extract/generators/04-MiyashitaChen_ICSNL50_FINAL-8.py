"""The ops of 04-MiyashitaChen_ICSNL50_FINAL-8: Mizuki Miyashita and Min Chen on an audio data mining
system that finds the segments of a recording holding a target sound, tested on a Blackfoot
conversation for the phonetic forms [x], [xʷ] and [ç] of /x/.

The paper sets no numbered examples. Its two bulleted lists, the audio features of §2.1 and the three
steps of §2.2, are a note to each item. Figures 1 to 3 are pictures, the Blackfoot transcription
sample, the two equations and the table of results; each caption is a note.

Figure 1 is one embedded bitmap of 445 by 297 pixels, about 105 dpi, which the text layer does not
hold. Its twelve timed lines were read from the image at its own pixels, enlarged, and each is a
symbol note: the Blackfoot line the form, its time, speaker and free translation in the gloss, with
the letter the page highlights as the target sound.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Blackfoot"
AUTHORS = ["Mizuki Miyashita", "Min Chen"]
paper = gen.Paper("04-MiyashitaChen_ICSNL50_FINAL-8", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Shiree Crow Shoe", "provided conversations in summer 2007"),
         ("James Boy", "provided conversations in summer 2007, late")]
LANGUAGES = [(LANGUAGE, "Algonquian, spoken in Alberta, Canada, and Montana, US"),
             ("Lushootseed", "Salish, transcribed and translated by Lonsdale")]

# The volume's header opens on In Papers for the International Conference, which
# gen.Paper.volume_header does not take for its opening.
HEADER_FIRST = paper.find(r"^In Papers for the International Conference", 1, 20)
HEADER = list(range(HEADER_FIRST, paper.find(r"\b(?:19|20)\d\d\.$", HEADER_FIRST, HEADER_FIRST + 2) + 1))
paper.volume_header = lambda: HEADER

# Figure 1's lines as the page prints them: time, speaker, Blackfoot, free translation, and what the
# gloss adds, the highlighted target letter or the doubt of a reading.
FIGURE_1 = [
    ("00.12", "SC", "oki", "hello", None),
    ("00.16", "JB", "oki poohsapokinsists", "shake my hand",
     "the highlighted h of pooh is the target sound; the n of -kinsists read from a 105 dpi bitmap, n or r"),
    ("00.20", "SC", "aa @@@@", "sure, [laughter]", None),
    ("00.37", "SC", "aapooisiikootsim", "this is taking/gathering",
     "after a line of three dots, a gap in the sample"),
    ("00.38", "SC", "or aista maahkohkossksinihsi", "or she wants to know",
     "the highlighted h of kohk is the target sound"),
    ("00.41", "JB", "aa", "yes", None),
    ("00.42", "SC", "ikkamsaakiitaistsoohsii aa", "is there your memory", None),
    ("00.45", "SC", "kitssksiniipii", "what you know", None),
    ("00.47", "SC", "ihtaopaamaahpii pookaiksi", "nursery rhymes for children",
     "the highlighted h of ih is the target sound"),
    ("00.51", "SC", "ki osisskai’mattsiitstsii kii", "and few more", None),
    ("00.53", "SC", "aa-kitsitsi- kitsits-", "you ...(?)", None),
    ("00.54", "SC", "i’nakstssihpii", "it is small", "the highlighted h of sih is the target sound"),
]

FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(HEADER)
RUNNING = paper.running_numbers_set()


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in AT_FOOT \
        and number not in RUNNING


def bullets(start, where):
    """A bulleted list from line start: a note to each item, its wrapped lines run on, to the first
    line after an item's closing stop that opens no bullet."""
    items, line, closed = [], start, False
    while line <= paper.last:
        if not printed(line):
            line += 1
            continue
        text = paper.text(line)
        if text.startswith("•"):
            items.append([text[1:].strip(), line])
        elif closed:
            break
        else:
            items[-1][0] += " " + text
        closed = text.endswith(".")
        line += 1
    for text, at in items:
        paper.add(where, A, "note", text, "page %d, an item of the bulleted list" % paper.page(at))
    return line


def caption(start, where):
    """A figure's caption, the figure itself a picture over it."""
    name = re.match(r"^(Figure \d+) ", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the figure's caption; the figure is a picture"
              % paper.page(start))
    return start + 1


blocks = {}
for number in range(1, paper.last + 1):
    if not printed(number):
        continue
    if paper.text(number).startswith("•") and not paper.text(number - 1).startswith("•") and \
            not any(paper.text(one).startswith("•") for one in range(number - 12, number)):
        blocks[number] = bullets
    if re.match(r"^Figure \d+ [A-Z]", paper.text(number)):
        blocks[number] = caption
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)


# Figure 1's lines go under its caption, the caption's gloss naming the columns.
figure = next(row for row in paper.rows if row[0] == "Figure 1")
figure[4] += (", a table headed Time, SP, Blackfoot and Free Translation, the Blackfoot column in italics"
              " and the target letter highlighted yellow")
held = len(paper.rows)
for number, (time, speaker, blackfoot, free, more) in enumerate(FIGURE_1, 1):
    paper.add("Figure 1 line %d" % number, A, "symbol note", blackfoot,
              "page %d, read from the render, the figure is a picture; Time %s, SP %s, Free Translation: %s%s"
              % (paper.page(paper.find(r"^Figure 1 ")), time, speaker, free, "; " + more if more else ""))
lines = paper.rows[held:]
del paper.rows[held:]
at = paper.rows.index(figure) + 1
paper.rows[at:at] = lines
paper.write()
