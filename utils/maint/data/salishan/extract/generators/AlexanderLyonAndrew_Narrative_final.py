"""The ops of AlexanderLyonAndrew_Narrative_final: The story of Jack McDougall, told in St'át'imcets
by Carl Alexander (Qwa7yán'ak) of Bridge River, transcribed and translated by Matt Andrew and John
Lyon: an old gold miner at Keary Creek drowned by two men from Brixton for money they never found,
the younger confessing 22 years later at Bralorne.

The story sets the St'át'imcets on the left and its English on the right, the two columns cut at
the left edge of the English on each page. A sentence pair opens where both columns open a sentence:
the English line opens on a capital after a line that ended one, and the St'át'imcets line opens a
sentence too or follows one that ended. Each pair's St'át'imcets is a transcription and its English
a translation. Carl's free English translation after the story, the prologue and the introduction
are prose.
"""
import os
import collections
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "St’át’imcets"
AUTHORS = ["Carl Alexander", "Matt Andrew", "John Lyon"]
paper = gen.Paper("AlexanderLyonAndrew_Narrative_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Qwa7yán’ak", "Carl Alexander's name"), ("Jack McDougall", "the old gold miner of the story"),
         ("Henry Davis", "helped with the translation; taught the St’át’imcets course"),
         ("Marianne Ignace", "her grant, First Nations Languages in the 21st Century"),
         ("Lisa Matthewson", "worked with Carl on the 2016 narratives"),
         ("Elliott Callahan", "worked with Carl on the 2016 narratives"),
         ("Pat Alec", "of Cácl’ep, St’át’imcets conversation sessions"),
         ("Richard", "left with the policemen"),
         ("Andrei Anghelescu", "an editor of the volume"), ("Michael Fry", "an editor of the volume"),
         ("Marianne Huijsmans", "an editor of the volume"), ("Daniel Reisinger", "an editor of the volume"),
         ("Langergraber", "K. Langergraber, with Lyon and Alexander (2016)")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish (Lillooet)"), ("Lillooet", "St’át’imcets"),
             ("Ucwalmícwts", "the language, in the prologue"), ("English", "the translation")]
RUNNING = paper.running_numbers_set()
FOUND = paper.page_footnotes()
SKIP = {one for parts, _ in FOUND.values() for one in parts}
STORY = paper.find(r"^3\s+The Story$")
FREE = paper.find(r"^4\s+Free Translation")
# A line that ends a sentence, a footnote's mark after the stop, home.1.
ENDED = re.compile(r"(?:[.!?]|\.\.\.)[”’\"]?\d?$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in SKIP


def story(start, where):
    """The story to the free translation, cut at the left edge of the English on each page into
    sentence pairs, each a transcription and its translation."""
    lines = [one for one in range(start, FREE) if printed(one)]
    seconds = collections.defaultdict(list)
    for line in lines:
        words = paper.word_positions(line)
        seconds[paper.page(line)] += [round(left) for left, _ in words if left > words[0][0] + 100]
    cuts = {page: collections.Counter(lefts).most_common(1)[0][0] - 2 for page, lefts in seconds.items()}
    pairs, left_ended, right_ended = [], True, True
    for line in lines:
        words, cut = paper.word_positions(line), cuts[paper.page(line)]
        left = " ".join(word for at, word in words if at < cut)
        right = " ".join(word for at, word in words if at >= cut)
        opens = right and right[0].isupper() and right_ended and (left_ended or not left or left[0].isupper())
        if opens or not pairs:
            pairs.append([[], [], paper.page(line)])
        if left:
            pairs[-1][0].append(left)
            left_ended = bool(ENDED.search(left))
        if right:
            pairs[-1][1].append(right)
            right_ended = bool(ENDED.search(right))
    for count, (lefts, rights, page) in enumerate(pairs, 1):
        here = "sentence %d" % count
        paper.add(here, L, "transcription", " ".join(lefts), "page %d, the left column" % page)
        paper.add(here, A, "translation", " ".join(rights), "page %d, the right column" % page)
    return FREE


blocks = {paper.find(r"\S", STORY + 1): story}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
# The keywords wrap past the title's footnote, Salish, history on a line of their own after it.
KEYWORDS = next(index for index, row in enumerate(paper.rows) if row[3].startswith("Keywords:"))
WRAPPED = next(index for index, row in enumerate(paper.rows) if row[3] == "Salish, history")
paper.rows[KEYWORDS][3] += " " + paper.rows.pop(WRAPPED)[3]
paper.write()
