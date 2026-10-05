"""Run the finished papers under paper_sift.py with one recent edit taken back at a time.

usage: python bisect_edits.py

Each variant is written to bisect/paper_sift.py and its drafts to bisect/<stem>.draft.tsv. The
lines where a variant's draft differs from the current draft are the rows that edit moved.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
import tables  # noqa: E402
OUT = os.path.join(WORK, "bisect")
# The papers it runs, each with the authors and language its table file sets.
PAPERS = tuple((stem, tables.of(stem).AUTHORS, tables.of(stem).LANG) for stem in (
    "Kelly_Huijsmans_McCarthy_ICSNL61-1", "Lyon_ICSNL61-1", "Mellesmoen_Trotter_ICSNL61",
    "Phillips_et_al_ICSNL61-1", "Pincott_ICSNL61-1", "Reisinger_ICSNL61-1", "Robertson_ICSNL61-1",
    "Schneider-Gerdts_ICSNL61-1"))
VARIANTS = {
    "closing_ahead": [('verdict == "english" and closing_ahead(at_line))):', 'False)):')],
    "empty body": [("if opened and opened.group(2).strip() and re.match(", "if opened and re.match(")],
    "mid-sentence verdict": [('opened.group(2)[:1].islower() and verdict == "english" and \\',
                              'opened.group(2)[:1].islower() and \\')],
    "english first line": [('elif state == "context" and example[1] == 0 and verdict == "english" and \\',
                            'elif False and \\')],
    "held translation": [("        # The caption is a note of its own", "        held = None\n        # The caption is a note of its own")],
    "tier post-pass": [('if below_segmentation["gloss"] >= 20 and', 'if below_segmentation["gloss"] >= 10**9 and')],
}


def main():
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(HERE, "paper_sift.py"), encoding="utf-8") as handle:
        source = handle.read()
    source = source.replace("HERE = os.path.dirname(os.path.abspath(__file__))",
                            "HERE = os.path.dirname(os.path.abspath(__file__))\n"
                            "sys.path.insert(0, os.path.dirname(HERE))")
    source = source.replace('defects.write(stem, "paper_sift.py", fixed)', "pass")
    chosen = sys.argv[1:] or list(VARIANTS)
    for name in chosen:
        text = source
        for old, new in VARIANTS[name]:
            assert text.count(old) == 1, (name, old)
            text = text.replace(old, new)
        with open(os.path.join(OUT, "paper_sift.py"), "w", encoding="utf-8") as handle:
            handle.write(text)
        print("== without %s" % name)
        for stem, authors, language in PAPERS:
            subprocess.run([sys.executable, os.path.join(OUT, "paper_sift.py"), stem, authors, language],
                           capture_output=True, check=True)
            with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
                now = handle.read().split("\n")
            with open(os.path.join(OUT, stem + ".draft.tsv"), encoding="utf-8") as handle:
                then = handle.read().split("\n")
            gone = [one for one in then if one not in set(now)]
            new = [one for one in now if one not in set(then)]
            if gone or new:
                print("  %s: %d rows differ" % (stem, len(gone) + len(new)))
                for one in gone[:6]:
                    print("    before  " + one[:150])
                for one in new[:6]:
                    print("    now     " + one[:150])


if __name__ == "__main__":
    main()
