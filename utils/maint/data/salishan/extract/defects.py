"""The table of every text layer defect a tool repaired, one row per repair, in defects.tsv.

Each tool rewrites its own rows for a paper on every run, and a rerun replaces what it wrote
before and the table holds one current row per repair:

    paper   page   type   raw   repaired   tool

The types are counted across papers with python defects.py, which prints how often each type
has turned up and in how many papers.
"""
import collections
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import PRIVATE  # noqa: E402
TABLE = os.path.join(PRIVATE, "defects.tsv")
HEAD = "paper\tpage\ttype\traw\trepaired\ttool\n"


def read():
    if not os.path.exists(TABLE):
        return []
    with open(TABLE, encoding="utf-8") as handle:
        return [line.rstrip("\n").split("\t") for line in handle][1:]


def write(stem, tool, entries):
    """Replace this tool's rows for this paper with entries, each (page, type, raw, repaired).
    Sessions extracting different papers write at once, and the read and the rewrite hold a lock
    file, and the table is replaced whole from a finished copy."""
    lock = TABLE + ".lock"
    for _ in range(1200):
        try:
            os.close(os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
            break
        except FileExistsError:
            # A lock older than two minutes is a writer that died holding it.
            try:
                if time.time() - os.path.getmtime(lock) > 120:
                    os.remove(lock)
            except OSError:
                pass
            time.sleep(0.1)
    else:
        raise RuntimeError("defects.tsv stayed locked; remove %s if no tool is running" % lock)
    try:
        retained = [one for one in read() if not (one[0] == stem and one[5] == tool)]
        for page, type, raw, repaired in entries:
            retained.append([stem, str(page), type, " ".join(str(raw).split()),
                         " ".join(str(repaired).split()), tool])
        with open(TABLE + ".new", "w", encoding="utf-8", newline="") as handle:
            handle.write(HEAD)
            for one in retained:
                handle.write("\t".join(one) + "\n")
        os.replace(TABLE + ".new", TABLE)
    finally:
        os.remove(lock)


def main():
    rows = read()
    count = collections.Counter(one[2] for one in rows)
    papers = collections.defaultdict(set)
    for one in rows:
        papers[one[2]].add(one[0])
    for type, seen in count.max_common():
        print("%5d  %3d papers  %s" % (seen, len(papers[type]), type))
    if len(sys.argv) > 1:
        for one in rows:
            if one[2] == sys.argv[1]:
                print("\t".join(one))


if __name__ == "__main__":
    main()
