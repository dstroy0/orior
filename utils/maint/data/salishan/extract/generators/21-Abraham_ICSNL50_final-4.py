"""The ops of 21-Abraham_ICSNL50_final-4: Marie Amhatsin' Abraham, Lil'wat Elder, on the night she
saw a Sásqets between Charlie Mack's and Andrew Wallace's houses, told in St'át'imc with a glossary.

The story is one paragraph in St'át'imc with no translation, each sentence a transcription. The
glossary sets a form and its English in two columns, cut at the left edge of the English; Figure 1,
a painting by the author, breaks into it on page 2.
"""
import os
import collections
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "St’át’imc"
AUTHORS = ["Marie Amhatsin’ Abraham"]
paper = gen.Paper("21-Abraham_ICSNL50_final-4", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Charlie Mack", "his house, near where the author saw the Sásqets"),
         ("Andrew Wallace", "his house, near where the author saw the Sásqets"),
         ("Lucille", "the author's younger sister")]
LANGUAGES = [(LANGUAGE, "St’át’imcets, Northern Interior Salish (Lillooet)")]

RUNNING = paper.running_numbers_set()
HEADER = set(paper.volume_header())
GLOSSARY = paper.find(r"^2 Glossary$")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in HEADER


def story(start, where):
    """The story, a transcription to each sentence: a sentence ends on its stop or ! before a
    capital, and the spaced dots . . . the story sets between phrases run on."""
    body = paper.joined([one for one in range(start, GLOSSARY) if printed(one)]).replace(". . .", "\0")
    for count, sentence in enumerate(re.split(r"(?<=[.!])\s+(?=[A-Z])", body), 1):
        sentence = sentence.replace("\0", ". . .")
        paper.add("%s sentence %d" % (where, count), L, "transcription", sentence, "page %d, the story" % paper.page(start))
    paper.mentions(where, body, NAMES, "name")
    return GLOSSARY


def glossary(start, where):
    """The glossary to the paper's end, a cited form and its English to each entry, cut at the
    left edge of the English column; Figure 1's caption a note. Most forms are one word, and the
    English column on each page stands where most entries set their second word."""
    lines = [one for one in range(start, paper.last + 1) if printed(one)]
    seconds = {}
    for line in lines:
        words = paper.word_positions(line)
        if len(words) > 1 and not paper.text(line).startswith("Figure 1:"):
            seconds.setdefault(paper.page(line), []).append(round(words[1][0]))
    cuts = {page: collections.Counter(lefts).most_common(1)[0][0] - 2 for page, lefts in seconds.items()}
    count = 0
    for line in lines:
        if re.match(r"^Figure 1:", paper.text(line)):
            paper.add("Figure 1", A, "note", paper.text(line),
                      "page %d, the figure's caption; the figure is a painting" % paper.page(line))
            continue
        count += 1
        words, cut = paper.word_positions(line), cuts[paper.page(line)]
        here, page = "%s line %d" % (where, count), "page %d, the glossary" % paper.page(line)
        paper.add(here, L, "cited form", " ".join(word for left, word in words if left < cut), page)
        paper.add(here, A, "translation", " ".join(word for left, word in words if left >= cut), page)
    return paper.last + 1


blocks = {paper.find(r"^Ats’xenlhkán ta sásqets"): story, GLOSSARY + 1: glossary}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)
paper.write()
