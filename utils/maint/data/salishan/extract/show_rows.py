"""Print whole draft rows by their view number (the header is view row 1).

usage: python show_rows.py <stem> <n> [<n> ...]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import WORK  # noqa: E402
stem = sys.argv[1]
with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
    rows = [one.rstrip("\n").split("\t") for one in handle]
for number in sys.argv[2:]:
    row = rows[int(number) - 1]
    print("%s | %s" % (number, " | ".join(row)))
    print()
