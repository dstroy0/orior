"""Write the generator that rebuilds a paper's oracle a person read off the page.

usage: python hand_generator.py <stem>

Each row of examples/Salishan/oracles/<stem>.oracle.tsv becomes a row of the generator, in order and
with its fields as they stand, and with the line or lines of the page text its form stands on. The
generator writes the oracle through gen.Paper.write_oracle, holding each form to its lines through
gen.Paper.on and stopping where the page text no longer holds it. A form the page text
does not hold on a run of WINDOW lines or fewer, one the text layer set apart or ran together, is
written as the oracle holds it, with no line. The generator goes to the closed corpus's
generators/<stem>.py, and an existing one is left in place.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.stdout.reconfigure(encoding="utf-8")
from workdir import GENERATORS, ORACLES  # noqa: E402
import gen  # noqa: E402

WINDOW = 12


def spans(paper):
    """[{(first, last): (text joined by spaces, text joined by none)}] for runs of 1 to WINDOW
    lines, each opening and closing on a line of text."""
    held = [{} for _ in range(WINDOW)]
    for first in range(1, paper.last + 1):
        if not paper.text(first):
            continue
        texts = []
        for last in range(first, min(first + WINDOW, paper.last + 1)):
            texts.append(paper.text(last))
            if paper.text(last):
                held[last - first][(first, last)] = (" ".join(texts), "".join(texts))
    return held


def anchor(form, held, after):
    """The shortest run holding form, the nearest from line after where there is one, and the
    nearest before it where there is not; None where no run holds it."""
    flat = " ".join(form.split())
    if not flat:
        return None
    for size in held:
        found = [key for key, (spaced, closed) in size.items() if flat in spaced or flat in closed]
        if found:
            return min(found, key=lambda key: (key[0] < after, abs(key[0] - after)))
    return None


def main():
    stem = sys.argv[1]
    target = os.path.join(GENERATORS, stem + ".py")
    if os.path.exists(target):
        raise SystemExit("%s has a generator already" % stem)
    with open(os.path.join(ORACLES, stem + ".oracle.tsv"), encoding="utf-8", newline="") as handle:
        lines_of = [line.rstrip("\n").split("\t") for line in handle]
    header, rows = lines_of[0], lines_of[1:]
    paper = gen.Paper(stem)
    held = spans(paper)
    out = ['"""The oracle of %s, its rows as a person read them off the page, each form held to the page' % stem,
           'line it stands on where the page text holds it."""',
           "import os", "import sys", "", 'sys.path.insert(0, os.environ["SALISHAN_TOOLS"])',
           'sys.stdout.reconfigure(encoding="utf-8")', "import gen  # noqa: E402", "",
           "paper = gen.Paper(%r)" % stem, "HEADER = %r" % (tuple(header),),
           "# The row, and the line or lines its form stands on.", "ROWS = ("]
    after, loose = 1, 0
    for row in rows:
        at = anchor(row[3], held, after) if len(row) > 3 else None
        if at is None:
            loose += 1
            lines = None
        else:
            lines = at[0] if at[0] == at[1] else at
            after = at[0]
        out.append("    (%r, %r)," % (tuple(row), lines))
    out += [")", "for row, lines in ROWS:", "    paper.keep(row, lines)", "paper.write_oracle(HEADER)", ""]
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(out))
    print("%s: %d rows, %d held to no line" % (target, len(rows), loose))


if __name__ == "__main__":
    main()
