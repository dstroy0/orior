"""Sort a paper's text layer with the anchor_sift engine: English, the language, or residue.

usage: python sift_paper.py <stem> [lines|tokens]

Uses english_sift.sorted_into with its two anchors, the English reference and the pure corpus, on
each repaired line of the paper, or on each token. Prints the verdict, both surprises, and the text.
"""
import os
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from workdir import INSTRUMENT  # noqa: E402
sys.path.insert(0, INSTRUMENT)

import residue  # noqa: E402
from english_sift import english_reference, language_reference, sorted_into, surprise  # noqa: E402
from paper_config import INSERTED_SPACE  # noqa: E402
from repairs import composed, corrected, sequence  # noqa: E402

ENGLISH = english_reference()
LANGUAGE = language_reference()
EDGES = "‘’“”\"'()[]{},.;:!?"


def verdict(text):
    return (sorted_into(text, ENGLISH, LANGUAGE),
            surprise(text, ENGLISH[0], ENGLISH[1]),
            surprise(text, LANGUAGE[0], LANGUAGE[1]))


def repaired_lines(stem):
    repair = residue.paper_repair(stem)
    with open(residue.source_path(stem), encoding="utf-8") as handle:
        for number, raw in enumerate(handle.read().split("\n"), 1):
            yield number, unicodedata.normalize("NFC", repair(raw.rstrip())).strip()


def main():
    stem = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "lines"
    counted = {}
    seen = set()
    for number, text in repaired_lines(stem):
        if not text or text.startswith("====="):
            continue
        units = [text] if mode == "lines" else [one.strip(EDGES) for one in text.split()]
        for unit in units:
            if not unit or unit in seen:
                continue
            if mode == "tokens":
                seen.add(unit)
            where, to_english, to_language = verdict(unit)
            counted[where] = counted.get(where, 0) + 1
            print("%d\t%s\t%.2f\t%.2f\t%s" % (number, where, to_english, to_language, unit))
    print("#", counted, file=sys.stderr)


if __name__ == "__main__":
    main()
