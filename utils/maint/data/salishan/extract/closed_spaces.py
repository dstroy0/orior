"""List every place the inserted-space repair closes a space after a combining mark in a page text.

usage: python closed_spaces.py <stem>

Prints the line number, the word before the space and the word after it, and a person can check each
against the page and put the real spaces back with a residue CORRECTION.
"""
import os
import re
import sys
import unicodedata
from workdir import PRIVATE  # noqa: E402

stem = sys.argv[1]
path = os.path.join(PRIVATE, "pagetext", stem + ".txt")
with open(path, encoding="utf-8") as handle:
    lines = handle.read().split("\n")
for number, text in enumerate(lines, 1):
    text = text
    for found in re.finditer(r"(\S*[̀-ͯ]) (\S+)", text):
        before = unicodedata.normalize("NFC", found.group(1))
        after = unicodedata.normalize("NFC", found.group(2))
        print("%d\t%s\t%s" % (number, before, after))
