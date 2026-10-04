"""Print set ops that move the cited forms a footnote holds out of the section the engine gave them.

The engine gives a candidate the where of the section on its page, footnote words included. A
candidate whose form no row of its section holds, and one footnote on the page does, is that
footnote's.

  python footnote_where.py STEM
"""
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import ORACLES  # noqa: E402
CANDIDATES = ("cited form", "cited affix", "root", "language", "notation", "name")


def main():
    stem = sys.argv[1]
    with open(os.path.join(ORACLES, stem + ".oracle.tsv"), encoding="utf-8") as handle:
        rows = list(csv.reader(handle, delimiter="\t", quoting=csv.QUOTE_NONE))[1:]

    def page_of(gloss):
        found = re.search(r"page (\d+)", gloss)
        return found.group(1) if found else None

    def holds(text, form):
        return re.search(r"(?<![^\W\d_])%s(?![^\W\d_])" % re.escape(form), text) is not None

    seen = set()
    for where, who, kind, form, gloss in rows:
        if kind not in CANDIDATES or not where.startswith("§") or (where, form) in seen:
            continue
        page = page_of(gloss)
        in_section = any(one[0] == where and one[2] not in CANDIDATES and holds(one[3], form) for one in rows)
        notes = {one[0] for one in rows if one[0].startswith("footnote ") and " line " not in one[0]
                 and page_of(one[4]) == page and holds(one[3], form)}
        if not in_section and len(notes) == 1:
            seen.add((where, form))
            print("set %s | %s | where=%s" % (where, form, notes.pop()))


if __name__ == "__main__":
    main()
