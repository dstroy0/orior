"""Read a table whose columns stand under a line of heads, by glyph position.

usage: python grid_table.py <stem> <page> <first head> [<bottom baseline>]

The heads are the words of the line opening on the first head. Below it, the printed lines run to
the bottom baseline, or to the first line wider than the table where none is given. Lines standing
within CLUSTER points of each other are one visual line: a superscript hyphen and the word it
belongs to, or a label set lower than its cells. A visual line with a word left of the first head
opens a row; one without joins the row above as a second line of its cells. Each word goes under
the head whose middle stands nearest. rows() gives (label, [cell text for each head]).
"""
import sys

import table_cells

CLUSTER = 7.0


def _joined(words):
    """Words of one cell or label, left to right; a lone hyphen joins the word after it."""
    # Two pieces from different printed lines whose boxes touch are one word split by a descender
    # or a raised letter, y and es of yes; a raised hyphen joins the word after it and never the one
    # before, as in (+ -as).
    pieces, edge = [], None
    for left, right, text in sorted(words):
        if pieces and (pieces[-1] == "-" or (text != "-" and left - edge < 1.2)):
            pieces[-1] += text
        else:
            pieces.append(text)
        edge = right
    return " ".join(pieces)


def rows(stem, page, first, bottom=None, top=None):
    """The heads and rows of the table whose head line opens on first."""
    lines = table_cells.word_lines(stem, page)
    start = next(at for at, (baseline, words) in enumerate(lines)
                 if words and words[0][2] == first and (top is None or baseline <= top))
    heads = lines[start][1]
    edge = heads[0][0] - 3
    clusters = []
    for baseline, words in lines[start + 1:]:
        if bottom is not None and baseline < bottom:
            break
        if clusters and clusters[-1][0] - baseline < CLUSTER:
            clusters[-1][1].extend(words)
            clusters[-1][0] = baseline
        else:
            clusters.append([baseline, list(words)])
    table = []
    for baseline, words in clusters:
        label = [word for word in words if word[1] < edge]
        cells = [[] for _ in heads]
        for word in words:
            if word[1] >= edge:
                cells[table_cells.column_of(word, heads)].append(word)
        if label or not table:
            table.append([_joined(label), [[one] for one in cells]])
        else:
            for into, more in zip(table[-1][1], cells):
                into.append(more)
    return [head[2] for head in heads], [
        (label, [" ".join(filter(None, (_joined(part) for part in parts))) for parts in cells])
        for label, cells in table]


if __name__ == "__main__":
    stem, page, first = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    bottom = float(sys.argv[4]) if len(sys.argv) > 4 else None
    names, found = rows(stem, page, first, bottom)
    print(" | ".join(names))
    for label, cells in found:
        print("%-28s %s" % (label, " | ".join(cells)))
