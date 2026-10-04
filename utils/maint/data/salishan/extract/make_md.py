"""Write oracles/<stem>.oracle.md from the run: the method, the residue counts, the alphabet, the columns.

usage: python make_md.py <stem>

The per-paper sections, whose words these are, the letters and the page against the text layer, come
from the closed corpus's finish/<stem>.py, where a person wrote them off the page. The counts come from running
residue.py and reading the table, and nothing in them is typed by hand.
"""
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import ORACLES, WORK  # noqa: E402

from finish import load_context  # noqa: E402

KINDS = (
    ("transcription", "an example tier in the orthography"),
    ("segmentation", "an example tier broken into morphemes"),
    ("phonemic", "an example tier in a phonemic alphabet"),
    ("gloss", "the morpheme gloss tier of an example"),
    ("word gloss", "an English tier under the gloss, word for word"),
    ("translation", "the English of an example"),
    ("rule", "a line of a rule, a derivation or a tree the authors display"),
    ("speaker comment", "a speaker's own comment on an example, in their words"),
    ("cited form", "a word of the language named in the prose, a note or a table"),
    ("cited affix", "an affix named on its own"),
    ("root", "a root named on its own"),
    ("place", "a place name"),
    ("language", "a language name"),
    ("name", "a person or a proper name"),
    ("note", "a paragraph, a context, a table, a footnote or a word that is not the language"),
    ("citation", "a work cited in the text, or the tag at the right of an example"),
    ("reference", "an entry of the reference list, whole"),
    ("heading", "a section heading"),
    ("title", "the title"),
    ("notation", "a note on how the paper sets something, or where two printings disagree"),
    ("symbol note", "a note on which character a mark is"),
    ("damage", "a string the page prints that is itself an error, transcribed as printed"),
)


def wrapped(text, width=100):
    out = []
    for paragraph in text.split("\n\n"):
        line = ""
        for word in paragraph.split():
            if line and len(line) + 1 + len(word) > width:
                out.append(line)
                line = word
            else:
                line = (line + " " + word) if line else word
        out.append(line)
        out.append("")
    return "\n".join(out).rstrip("\n")


def main():
    stem = sys.argv[1]
    context = load_context(stem)
    table = os.path.join(ORACLES, stem + ".oracle.tsv")
    with open(table, encoding="utf-8") as handle:
        rows = [line.rstrip("\n").split("\t") for line in handle][1:]
    kinds = collections.Counter(one[2] for one in rows)

    report = subprocess.run([sys.executable, os.path.join(HERE, "residue.py"), stem],
                            capture_output=True, text=True, encoding="utf-8",
                            env=dict(os.environ, PYTHONIOENCODING="utf-8")).stdout
    counts = [int(one) for one in re.findall(r"^    (\d+) ", report, re.M)]
    asked, taken, unfound, missed = counts[0], counts[1], counts[2], counts[3]
    distinct = int(re.search(r"(\d+) distinct tokens", report).group(1))

    alphabet = []
    with open(os.path.join(WORK, stem + ".alphabet.txt"), encoding="utf-8") as handle:
        for line in list(handle)[1:]:
            symbol, point, outside, inside, share, role, name = line.rstrip("\n").split("\t")
            if role == "letter" and int(outside) > 0:
                alphabet.append(symbol)
    web = sum(1 for _ in open(os.path.join(WORK, stem + ".web.tsv"), encoding="utf-8")) - 1

    corrections = __import__("residue").CORRECTIONS.get(stem, ())
    parts = [
        "# %s.oracle.tsv" % stem,
        "",
        wrapped("Extraction of %s by %s, ICSNL %s." % (context.TITLE, context.BYLINE,
                                                          # A stem without the volume, Trotter_ICSNL, names it in its context.
                                                          getattr(context, "VOLUME", None) or
                                                          re.search(r"ICSNL[_-]?(\d+)", stem).group(1))),
        "",
        wrapped(
            "Drafted by the anchor_sift engine and read against the page by a person. The engine "
            "sorted every line of the text layer with english_sift.sorted_into, first against its "
            "English reference and the pure corpus, then against this paper's own English laid "
            "over that reference. What the paper's English did not account for became the example "
            "tiers and the cited forms. The alphabet was taken from the characters that sit outside "
            "that English, and word_web.web() built %d edges over the language forms. A person then "
            "matched the context: who, kind and gloss for each row, the names, places and languages, "
            "and the notations, read off the page. This file is the control. The reader in "
            "corpus_script_extraction is checked against it, and where they disagree the reader is "
            "wrong until someone reads the paper again and says otherwise." % web),
        "",
        "WHOSE WORDS THESE ARE",
        "",
        wrapped(context.WHOSE),
        "",
        "THE LETTERS",
        "",
        wrapped("Outside the paper's English the engine found these letters and marks: %s. %s"
                % (" ".join(alphabet), context.LETTERS)),
        "",
        "THE PAGE AND THE TEXT LAYER",
        "",
        wrapped(
            ("The forms are in NFC. The page is typed and scanned, and its text layer is OCR that holds "
             "none of the orthography; the check reads the forms against a page text a person "
             "transcribed from the scan, a line for each printed line. %s" % context.PAGE_NOTES)
            if getattr(__import__("tables").of(stem), "TRANSCRIBED_FROM_SCAN", False) else
            "The forms are in NFC, and the check puts the text layer through the same repair: the "
            "space the PDF sets after a stacked mark is closed, except before an opening quote, "
            "then NFC%s. %s"
            % (", then %d page-read correction%s" % (len(corrections), "" if len(corrections) == 1 else "s")
               if corrections else "", context.PAGE_NOTES)),
        "",
        wrapped(
            "anchor_sift's hand_extraction/oracle_check.py was run on this table through residue.py, "
            "leaving out the notation and symbol note rows, whose form is a label and not a string "
            "the paper prints. Of %d rows asked, %d hold a form the repaired paper does not, and %d "
            "a form the repair took out. Of the %d distinct tokens in the paper, %d language tokens "
            "are held by no row." % (asked, unfound, taken, distinct, missed)),
        "",
        "where    the paper's locator: the title, the front matter, a section, a footnote, an example",
        "         line such as (3) line 2, the references, or all for a note about the whole paper",
        "who      the language for an example tier and a cited form; the work cited on an example's",
        "         line, or the speaker for a volunteered translation, for its English; the authors for",
        "         the prose, the tables and the notes",
    ]
    first = True
    for kind, meaning in KINDS:
        if kinds.get(kind):
            parts.append("%s %-14s %s" % ("kind    " if first else "        ", kind, meaning))
            first = False
    parts += [
        "form     as printed, joined where the text layer breaks a word, with no footnote digits",
        "gloss    the page, the paper's English for a form, and what the reader needs to know",
        "",
        wrapped("Only transcription, segmentation, phonemic, cited form, cited affix and root rows "
                "whose who is a language are the language. The gloss tiers are the authors' analysis "
                "in English and labels."),
        "",
    ]
    target = os.path.join(ORACLES, stem + ".oracle.md")
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("\n".join(parts))
    print("wrote %s" % target)
    print(report.strip().splitlines()[-1])


if __name__ == "__main__":
    main()
