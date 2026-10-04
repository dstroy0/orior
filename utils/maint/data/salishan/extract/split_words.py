"""Print the words a page text splits in two: two neighboring plain lowercase tokens that are no
English words alone and one English word together, rej ections for rejections.

usage: python split_words.py <stem>

The English is the engine's own, cc_english.txt. A split this finds is one to read off the page
before it goes into the CORRECTIONS of the paper's table file.
"""
import os
import re
import sys

import paper_sift
from workdir import PRIVATE  # noqa: E402

stem = sys.argv[1]
english = paper_sift.ordinary_english()
page = 0
with open(os.path.join(PRIVATE, "pagetext", stem + ".txt"),
          encoding="utf-8") as handle:
    for number, line in enumerate(handle.read().split("\n"), 1):
        marker = re.match(r"^===== page (\d+) =====", line)
        if marker:
            page = int(marker.group(1))
            continue
        tokens = line.split()
        for left, right in zip(tokens, tokens[1:]):
            first = re.sub(r"^[(‘“]+", "", left)
            second = re.sub(r"[.,;:!?)’”]+$", "", right)
            if not (re.fullmatch(r"[a-z]+", first) and re.fullmatch(r"[a-z]+", second)):
                continue
            if first + second in english and (first not in english or second not in english):
                print("page %d line %d: %s %s" % (page, number, left, right))
