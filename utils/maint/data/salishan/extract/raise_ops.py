"""Give a finished paper back the raised letters its text layer flattened: [kwʊtəm] to [kʷʊtəm].

usage: python raise_ops.py <stem> [--write]

page_text.raise_letters writes a letter the page sets small and raised after another letter as its
modifier. It changes one character for one character, and the paper's text before it and after it
line up letter for letter. This reads the text residue.py checks the paper against, raises its
letters, and finds each field of the paper's ops (and of its engine draft, where the ops keep draft
rows) in the old text: a field found only where the raised text reads the same everywhere takes
that reading, and a field found where the raised text differs, kw both orthographic and [kʷ], is
reported and left. Without --write it prints what it would change.
"""
import os
import re
import sys

import pypdfium2 as pdfium

import page_text
import residue

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import CORPUS  # noqa: E402
from workdir import PRIVATE  # noqa: E402
from workdir import WORK  # noqa: E402


def raised_text(stem):
    """(old lines, new lines, the source file) for a paper."""
    source = residue.source_path(stem)
    with open(source, encoding="utf-8") as handle:
        old = handle.read().split("\n")
    document = pdfium.PdfDocument(os.path.join(CORPUS, "papers", stem + ".pdf"))
    new, _ = page_text.raise_letters(document, old)
    return old, new, source


def mapping(fields, old_text, new_text):
    """{field: raised field} for each field the raised text changes the same way wherever the old
    text holds it, and [fields it changes two ways]."""
    changed, torn = {}, []
    for field in fields:
        words = field.split()
        if not words or len(field) < 2:
            continue
        # A residue correction can close a space the text holds inside a row's word, k̀ w for k̀w:
        # the field is found with any spaces between its letters, each letter read where it stands.
        plain = " ".join(words)
        pattern = "".join(r"\s+" if letter == " " else re.escape(letter) + r"[ \t]*" for letter in plain)
        readings = set()
        for found in re.finditer(pattern, old_text):
            at, out = found.start(), []
            for letter in plain:
                while old_text[at] in " \t\n":
                    at += 1
                if letter == " ":
                    out.append(" ")
                    continue
                out.append(new_text[at])
                at += 1
            readings.add("".join(out))
        if not readings or readings == {plain}:
            continue
        if len(readings) == 1:
            changed[field] = readings.pop()
        else:
            torn.append((field, sorted(readings)))
    return changed, torn


def main():
    stem, write = sys.argv[1], "--write" in sys.argv
    old, new, source = raised_text(stem)
    old_text, new_text = "\n".join(old), "\n".join(new)
    ops_path = os.path.join(PRIVATE, "ops", stem + ".ops")
    # An ops file keeps the line endings it was written with, CRLF in the hand-made ones.
    with open(ops_path, encoding="utf-8", newline="") as handle:
        ops = handle.read().split("\n")
    fields = {part.strip() for line in ops if line and not line.startswith("#")
              for part in line.split(" | ")}
    # The first field of an op line carries the op's name before it.
    fields |= {part.split(" ", 1)[1].strip() for line in ops if line and not line.startswith("#")
               for part in line.split(" | ")[:1] if " " in part}
    draft_path = os.path.join(WORK, stem + ".draft.tsv")
    keeps_draft = not any(line.startswith("removewhere .") for line in ops)
    draft = []
    if keeps_draft and os.path.isfile(draft_path):
        with open(draft_path, encoding="utf-8", newline="") as handle:
            draft = handle.read().split("\n")
        fields |= {part for line in draft[1:] for part in line.split("\t")}
    changed, torn = mapping(fields, old_text, new_text)
    for field, raised in sorted(changed.items()):
        print("  %s  ->  %s" % (field[:90], raised[:90]))
    for field, readings in torn:
        print("  TORN %s: %s" % (field[:60], " / ".join(one[:40] for one in readings)))
    print("%s: %d fields raised, %d read two ways and left; the text differs on %d lines; draft rows kept: %s"
          % (stem, len(changed), len(torn), sum(1 for one, two in zip(old, new) if one != two), keeps_draft))
    if not write:
        return

    def raise_line(line, separator):
        if not line or line.startswith("#"):
            return line
        parts = line.split(separator)
        out = []
        for index, part in enumerate(parts):
            head, body = "", part
            if separator == " | " and index == 0 and " " in part:
                head, body = part.split(" ", 1)
                head += " "
            lead = body[:len(body) - len(body.lstrip())]
            trail = body[len(body.rstrip()):]
            core = body.strip()
            out.append(head + lead + changed.get(core, core) + trail)
        return separator.join(out)

    with open(ops_path, "w", encoding="utf-8", newline="") as handle:
        handle.write("\n".join(raise_line(line, " | ") for line in ops))
    if draft:
        with open(draft_path, "w", encoding="utf-8", newline="") as handle:
            handle.write("\n".join([draft[0]] + [raise_line(line, "\t") for line in draft[1:]]))
    # The raised text goes where residue.py reads the paper.
    target = source if os.path.dirname(source) == residue.PAGE_TEXT else os.path.join(residue.PAGE_TEXT, stem + ".txt")
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(new_text)
    print("written: %s, %s%s" % (ops_path, target, ", " + draft_path if draft else ""))


if __name__ == "__main__":
    main()
