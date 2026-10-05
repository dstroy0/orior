"""List the words a paper's text layer split with a space, as page-read correction candidates.

usage: python splits.py <stem>

A pair of neighboring tokens is a candidate where the two joined make a word the same paper prints
whole somewhere else: Secwepemcts ín beside Secwepemctsín. The repair runs first, and a split the
repair already closes is not listed. Each candidate prints with how often the paper holds the
whole word, for a person to read against the page before it goes into the CORRECTIONS of the paper's table file.
"""
import collections
import sys
import unicodedata

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
import residue  # noqa: E402

EDGES = "‘’“”\"'()[]{},.;:!?*"

stem = sys.argv[1]
repair = residue.paper_repair(stem)
with open(residue.source_path(stem), encoding="utf-8") as handle:
    lines = [unicodedata.normalize("NFC", repair(one.rstrip())) for one in handle]

whole = collections.Counter(token.strip(EDGES) for line in lines for token in line.split())
found = collections.Counter()
for line in lines:
    tokens = line.split()
    for left, right in zip(tokens, tokens[1:]):
        joined = (left + right).strip(EDGES)
        if len(joined) > 3 and whole.get(joined) and left.strip(EDGES) and right.strip(EDGES) \
                and not whole.get(left.strip(EDGES), 0) > whole[joined] \
                and any(not one.isascii() for one in joined):
            found[(left + " " + right, left + right)] += 1
for (split, joined), count in found.most_common():
    print("%d\t%s\t%s\t(whole %d)" % (count, split, joined, whole[joined.strip(EDGES)]))
