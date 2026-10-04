"""Print each example of a draft as the sequence of its row kinds, and flag the ones that look broken.

usage: python draft_check.py <stem> [all]

One line per example: its label and its kinds in order, t for transcription, s segmentation, g gloss,
T translation, c citation, n note, r rule, k speaker comment, p phonemic. An example is flagged when
it has no translation, when a note sits after its first line, or when two translations run together.
With all, every example is printed and not only the flagged ones.
"""
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
LETTER = {"transcription": "t", "segmentation": "s", "gloss": "g", "translation": "T",
          "citation": "c", "note": "n", "rule": "r", "speaker comment": "k", "phonemic": "p"}


def main():
    stem = sys.argv[1]
    every = len(sys.argv) > 2
    with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
        rows = [line.rstrip("\n").split("\t") for line in handle][1:]
    examples = collections.OrderedDict()
    for where, who, kind, form, gloss in rows:
        if " line " in where:
            examples.setdefault(where.split(" line ")[0], []).append((kind, form))
    flagged = 0
    for label, held in examples.items():
        kinds = "".join(LETTER.get(kind, "?") for kind, form in held)
        problems = []
        if "T" not in kinds and "r" not in kinds:
            problems.append("no translation")
        if "n" in kinds[1:]:
            problems.append("note inside")
        if "TT" in kinds:
            problems.append("two translations")
        if problems:
            flagged += 1
        if problems or every:
            print("%-14s %-24s %s" % (label, kinds, "; ".join(problems)))
    print("%d examples, %d flagged" % (len(examples), flagged))


if __name__ == "__main__":
    main()
