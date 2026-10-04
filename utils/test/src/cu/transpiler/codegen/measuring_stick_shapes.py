# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# measuring_stick_shapes.py: the measuring stick's nvcc listing cut to the first instruction of each shape, an
# operation and each operand's kind (a register, a uniform register, a predicate, a number, a constant, an address, a
# special register, a label), in cuobjdump's own layout with both words of its encoding. The machine file takes a form
# for each shape it lacks (interface_sass_take.sh), and a listing of one instruction a shape is within what it reads.
#
#     python measuring_stick_shapes.py <cuobjdump -sass listing> <listing of shapes>
import re
import sys


def kind(operand):
    operand = operand.strip()
    marked = operand.lstrip("-~!|")
    if re.match(r"^U?R(Z|\d+)", marked):
        return "U" if marked.startswith("U") else "R"
    if re.match(r"^U?P(T|\d+)", marked):
        return "P"
    if marked.startswith("c["):
        return "C"
    if marked.startswith(("[", "desc[")):
        return "A"
    if marked.startswith("SR_"):
        return "S:" + marked
    if marked.startswith("`"):
        return "L"
    return "I"


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("measuring_stick_shapes.py <listing> <listing of shapes>\n")
        return 2
    lines = open(sys.argv[1], encoding="utf-8", errors="replace").read().split("\n")
    seen = set()
    kept = []
    for at, line in enumerate(lines):
        found = re.match(r"\s*/\*[0-9a-f]{4}\*/\s+(.*?)\s*;\s*/\*\s*0x[0-9a-f]{16}\s*\*/", line)
        if (found is None) or (at + 1 >= len(lines)):
            continue
        text = re.sub(r"^@!?U?P[T0-9]+\s+", "", found.group(1))
        parts = text.split(None, 1)
        operands = parts[1].split(",") if len(parts) > 1 else []
        shape = (parts[0], tuple(kind(operand) for operand in operands))
        if shape in seen:
            continue
        seen.add(shape)
        kept.append(line)
        kept.append(lines[at + 1])
    with open(sys.argv[2], "w", encoding="utf-8", newline="\n") as out:
        out.write("\tcode for sm_86\n\t\tFunction : measuring_stick_shapes\n")
        out.write("\n".join(kept) + "\n")
    print("measuring stick: %u shapes" % len(seen))
    return 0


if __name__ == "__main__":
    sys.exit(main())
