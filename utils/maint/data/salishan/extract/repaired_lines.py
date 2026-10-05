"""Print the page text lines holding each wanted string as residue.py reads them, repaired.

usage: python repaired_lines.py <stem> <string> [string ...]

The strings are matched against the lines before repair, and each line is printed after it with
repr, and a correction can be written against exactly what corrected() sees.
"""
import sys

import residue

stem, wanted = sys.argv[1], sys.argv[2:]
repair = residue.paper_repair(stem)
with open(residue.source_path(stem), encoding="utf-8") as handle:
    for number, line in enumerate(handle, 1):
        if any(one in line for one in wanted):
            print(number, repr(repair(line.rstrip("\n"))))
