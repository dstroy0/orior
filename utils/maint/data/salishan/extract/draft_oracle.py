"""Over-scrape a paper's text layer into a draft oracle table, for a person to prune against the page.

usage: python draft_oracle.py <stem> <authors> <language> [out.tsv]

Every line of the text layer lands in some row. Prose is grouped into paragraph rows, each line of a
numbered example block is its own row with a guessed kind, each numbered heading is a heading row,
each reference entry a reference row, and every token carrying a letter outside ASCII is offered
again as a cited form candidate under the section it sits in. The repair is the residue check's: the
space closed after a stacked mark, then NFC. Nothing here is read; every row is a candidate.
"""
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import residue  # noqa: E402
from paper_config import INSERTED_SPACE  # noqa: E402
from repairs import composed, corrected, sequence  # noqa: E402
from workdir import WORK  # noqa: E402

PAGE = re.compile(r"^===== page (\d+) =====$")
HEADING = re.compile(r"^(\d+(?:\.\d+)*)\.?\s+(\S.*)$")
EXAMPLE = re.compile(r"^\((\d+|[ivx]+)\)\s*(.*)$")
SUBEXAMPLE = re.compile(r"^([a-h])\.\s+(.*)$")
FOOTNOTE = re.compile(r"^(\d{1,2}|\*)\s+\S")
PAGE_NUMBER = re.compile(r"^\d{1,4}$")
SMALL_CAPS = set("ᴀʙᴄᴅᴇꜰɢʜɪᴊᴋʟᴍɴᴏᴘʀꜱᴛᴜᴠᴡʏᴢ")
TAG = re.compile(r"\b(?:[123](?:SG|PL)|SG|PL|DET|POSS|SBJ|OBJ|CTR|NCTR|PST|FUT|STAT|CAUS|INTR|"
                 r"TR|PASS|DEM|NEG|Q|OBL|DIM|PROG|IMPF|PERF|REFL|RECP|APPL|NMLZ|LOC|CONJ)\b")
QUOTE_EDGES = "‘’“”\"'()[]{},.;:!?"


def kind_of_example_line(text):
    stripped = text.strip()
    if stripped.startswith(("‘", "“", "'")):
        return "translation"
    if stripped.lower().startswith("context"):
        return "note"
    if any(one in SMALL_CAPS for one in stripped) or TAG.search(stripped):
        return "gloss"
    if re.search(r"[=~<>]|\S-\S", stripped):
        return "segmentation"
    return "transcription"


def marked(token):
    plain = token.strip(QUOTE_EDGES)
    if not plain or not any(one.isalpha() for one in plain):
        return ""
    if all(one.isascii() for one in plain):
        return ""
    return plain


def main():
    stem, authors, language = sys.argv[1], sys.argv[2], sys.argv[3]
    target = sys.argv[4] if len(sys.argv) > 4 else os.path.join(
        WORK, stem + ".draft.tsv")
    repair = sequence(INSERTED_SPACE, composed(), corrected(residue.CORRECTIONS.get(stem, ())))
    with open(os.path.join(residue.CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
        lines = handle.read().split("\n")

    rows = []
    section = "front"
    page = 0
    paragraph = []
    paragraph_where = section
    example = None
    in_references = False
    widest = max((len(one.rstrip()) for one in lines), default=80)
    tokens_seen = set()

    def flush():
        nonlocal paragraph
        if paragraph:
            text = " ".join(paragraph)
            kind = "reference" if in_references else "note"
            rows.append((paragraph_where, authors, kind, text, "page %d" % page))
            paragraph = []

    def offer_tokens(where, text):
        for token in text.split():
            plain = marked(token)
            if plain and (where, plain) not in tokens_seen:
                tokens_seen.add((where, plain))
                rows.append((where, language, "cited form", plain, "candidate token"))

    for raw in lines:
        text = unicodedata.normalize("NFC", repair(raw.rstrip())).strip()
        found = PAGE.match(text)
        if found:
            page = int(found.group(1))
            continue
        if not text or PAGE_NUMBER.match(text):
            if example is not None and not text:
                example = None
            continue
        heading = HEADING.match(text)
        if heading and len(text) < 70 and not text.endswith(".") and not FOOTNOTE.match(text[:3] + " x"):
            flush()
            example = None
            section = "§" + heading.group(1)
            in_references = False
            rows.append((section, authors, "heading", text, "page %d" % page))
            continue
        if text.rstrip(" :").lower() in ("references", "reference", "bibliography", "works cited"):
            flush()
            example = None
            section = "references"
            in_references = True
            rows.append((section, authors, "heading", text, "page %d" % page))
            continue
        opened = EXAMPLE.match(text)
        if opened and not in_references:
            flush()
            example = "(%s)" % opened.group(1)
            number = 0
            body = opened.group(2)
            if body:
                number += 1
                rows.append(("%s line %d" % (example, number), language,
                             kind_of_example_line(body), body, "page %d" % page))
                offer_tokens(example, body)
            continue
        if example is not None:
            number += 1
            sub = SUBEXAMPLE.match(text)
            where = "%s line %d" % (example, number)
            rows.append((where, language, kind_of_example_line(sub.group(2) if sub else text),
                         text, "page %d" % page))
            offer_tokens(example, text)
            continue
        if FOOTNOTE.match(text) and not paragraph:
            flush()
            paragraph_where = "footnote %s" % text.split()[0]
        elif not paragraph:
            paragraph_where = section
        if in_references and paragraph and re.match(r"^[A-ZʔƛŁ][^\s,]*,\s", text):
            flush()
        paragraph.append(text)
        offer_tokens(section, text)
        # A short line that closes a sentence ends the paragraph.
        if text.endswith((".", ":", "!", "?", "”", "’")) and len(raw.rstrip()) < 0.8 * widest:
            flush()
    flush()

    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("where\twho\tkind\tform\tgloss\n")
        for row in rows:
            handle.write("\t".join(" ".join(str(one).split()) for one in row) + "\n")
    print("%d rows to %s" % (len(rows), target))


if __name__ == "__main__":
    main()
