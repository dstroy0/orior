"""The who table: every C and C++ source of the tree numbered in hex, the number a who code carries in place of the
source's path (who.h, who_here).

    who_table.py <top> <table>

The table at <table> is read and printed again with every source the tree holds that it does not, each given the
number after the highest it holds, in the order of their paths. A number once given is never given again and never
taken back: a source the tree no longer holds keeps its row, so that every code written before still decodes.
"""

import pathlib
import subprocess
import sys

SUFFIXES = (".c", ".h", ".cu", ".cuh", ".cpp", ".hpp")
ROOTS = ("src", "utils", "examples")
HEAD = "number\tsource"


def sources(top):
    """every source of the tree under ROOTS, tracked or not and not ignored, by its path from the top"""
    listed = subprocess.run(["git", "-C", str(top), "ls-files", "--cached", "--others", "--exclude-standard", "--",
                             *ROOTS], capture_output=True, text=True, check=True).stdout.splitlines()
    return sorted({path for path in listed if path.endswith(SUFFIXES) and (top / path).is_file()})


def held(table):
    """the rows the table holds, number and source, in its order"""
    if not table.is_file():
        return []
    rows = []
    for line in table.read_text(encoding="utf-8").splitlines():
        if (line == HEAD) or (line.strip() == ""):
            continue
        number, source = line.split("\t", 1)
        rows.append((int(number, 16), source))
    return rows


def main():
    top = pathlib.Path(sys.argv[1])
    table = pathlib.Path(sys.argv[2])
    rows = held(table)
    numbered = {source for _, source in rows}
    highest = max((number for number, _ in rows), default=0)
    for source in sources(top):
        if source not in numbered:
            highest += 1
            rows.append((highest, source))
    if highest > 0xffff:
        sys.exit("who_table.py: more sources than a who code's sixteen bits number")
    # the table's lines end in a newline alone on every system, the one a who read finds them by
    sys.stdout.reconfigure(newline="\n")
    sys.stdout.write(HEAD + "\n")
    for number, source in rows:
        sys.stdout.write("0x%04x\t%s\n" % (number, source))


main()
