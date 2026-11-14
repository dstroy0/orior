"""Count the glued tokens in a paper's text layer: runs of ASCII characters too long to be one word.

usage: python glued.py <stem> [stem ...]

A text layer that lost its spaces holds tokens like betweenallomorphsissensitiveto:. A paper with
more than a handful wants page_text.py run on it before the engine reads it.
"""
import os
import sys

from workdir import CORPUS  # noqa: E402

for stem in sys.argv[1:]:
    with open(os.path.join(CORPUS, "papers", stem + ".txt"), encoding="utf-8") as handle:
        tokens = handle.read().split()
    glued = [one for one in tokens if len(one) > 22 and not one.startswith("http") and "@" not in one
             and sum(1 for character in one if character.isascii() and character.isalpha()) > 16
             and "-" not in one.strip("-")]
    print("%s\t%d glued\t%s" % (stem, len(glued), " ".join(glued[:4])))
