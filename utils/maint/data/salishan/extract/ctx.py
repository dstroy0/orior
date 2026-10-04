"""Print the lines of a paper's source that hold a string, both sides in NFC.

usage: python ctx.py <stem> <string> [<string> ...]

The draft's forms are NFC and the text layer often is not, and a plain grep misses them.
"""
import sys
import unicodedata

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
from residue import source_path  # noqa: E402

stem = sys.argv[1]
with open(source_path(stem), encoding="utf-8") as handle:
    lines = [unicodedata.normalize("NFC", one.rstrip("\n")) for one in handle]
for wanted in sys.argv[2:]:
    wanted = unicodedata.normalize("NFC", wanted)
    for number, line in enumerate(lines, 1):
        if wanted in line:
            print("%d: %s" % (number, line))
