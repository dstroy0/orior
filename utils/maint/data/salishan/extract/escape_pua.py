"""Write each private-use character of a source file as its \\uXXXX escape, in place.

usage: python escape_pua.py <file> [file ...]

A private-use glyph prints as nothing in an editor or a terminal, and a table keyed on one reads
as empty strings. The escape keeps the same string in Python and can be read.
"""
import re
import sys

for path in sys.argv[1:]:
    with open(path, encoding="utf-8", newline="") as handle:
        text = handle.read()
    escaped = re.sub("[-]", lambda found: "\\u%04x" % ord(found.group()), text)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(escaped)
    print("%s: %d escaped" % (path, len(re.findall("[-]", text))))
