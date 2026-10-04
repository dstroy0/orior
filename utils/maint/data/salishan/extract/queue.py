"""List the index's papers in order, marking which have an oracle table and a text layer on disk."""
import os
import sys

corpus = sys.argv[1]
papers = os.path.join(corpus, "papers")
oracles = os.path.join(corpus, "oracles")
have = set(name.split(".oracle.")[0] for name in os.listdir(oracles))
rows = open(os.path.join(papers, "icsnl_index.tsv"), encoding="utf-8").read().splitlines()[1:]
todo = 0
for number, row in enumerate(rows):
    cells = row.split("\t")
    stem = os.path.splitext(cells[2].rsplit("/", 1)[-1])[0]
    stem_disk = stem.replace(".", "_") if not os.path.exists(os.path.join(papers, stem + ".pdf")) else stem
    done = stem in have or stem_disk in have or stem.replace(".", "_") in have
    text = os.path.exists(os.path.join(papers, stem + ".txt")) or os.path.exists(os.path.join(papers, stem_disk + ".txt"))
    if not done:
        todo += 1
    print("%3d %s %s %s %s" % (number + 1, "done" if done else "TODO", "txt" if text else "NO-TXT", cells[1], stem))
print("rows", len(rows), "todo", todo)
