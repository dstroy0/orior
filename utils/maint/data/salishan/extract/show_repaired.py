"""Print lines of a paper's text layer as they stand after the residue check's repair.

usage: python show_repaired.py <stem> <line> [line ...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import residue  # noqa: E402
from repairs import composed, sequence  # noqa: E402
from paper_config import INSERTED_SPACE  # noqa: E402

stem = sys.argv[1]
repair = residue.paper_repair(stem)
with open(residue.source_path(stem), encoding="utf-8") as handle:
    lines = handle.read().split("\n")
for number in sys.argv[2:]:
    raw = lines[int(number) - 1]
    print("%s raw      %r" % (number, raw))
    print("%s repaired %r" % (number, repair(raw.rstrip())))
