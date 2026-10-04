#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Write the figures of the Salishan research paper's chapter "The corpus under the instrument" as
# TeX macros, from the checks that measure them.
#
#   Usage:  python utils/maint/data/salishan/instrument_figures.py
#
# The chapter states no number of its own. Every figure in it is a macro this file writes to
# chapters/figures_orior_salishan.tex, and each macro is read from the check that measures it:
#   coverage_check.py   the hand extractions, the automatic extractors and the papers fully accounted for;
#   sift_extract.py     the papers screened, the lines nearer the language and the residue;
#   corpus_admit.py     the admission table and the languages held only as candidates;
#   boundary_check.py   how much more text the whole-distribution comparison needs at the dialect border;
#   gold_readings.cu    every distance, split-half distance, verdict and median, exact on the record
#                       machine, read from build/salishan_gold/gold_figures.tex, which
#                       examples/Salishan/4_measure/gold_readings.sh writes and has to have run first;
#                       the papers read again from their tool extractions, from gold_figures_sifted.tex
#                       beside it, each of its paper figures named Sifted in place of Gold.
# The whole sifted set added to a refused corpus is measured here, with orior.py's functions, since
# no check prints it.
#
# A check is run and read and not reimplemented, as corpus_derivation.py reads its checks, and a
# figure is found by the check's own words for it. A reworded check stops this file instead of
# drifting.
#
# Some sentences of the chapter rest on the shape of a result as well as its figures: one pair of
# corpora that does not read, and that pair's wider split-half distance being Lushootseed's; the
# midpoint split-half distance above the alternating one for every corpus; every pair of languages
# reading at the alternating split; no paper reading from its screened lines, and none too small to
# ask; at least one paper reading from its tool extraction, and every one that reads agreeing with its
# prose; Lushootseed admitting every candidate; at least one corpus refusing all of its own. Each is held here, and where one fails nothing is written and the file
# exits 1, since a figure set into a sentence that no longer holds is wrong.
#
# The macros file is rewritten only where its text changes, and each path rewritten is printed as
# "  wrote <path>" for the build to show.

import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
for _category in os.scandir(HERE):
    if _category.is_dir():
        sys.path.insert(0, _category.path)
sys.path.insert(0, HERE)
_at = HERE
while (_at != os.path.dirname(_at)) and not os.path.isdir(os.path.join(_at, "src", "python")):
    _at = os.path.dirname(_at)
ROOT = _at
sys.path.insert(0, os.path.join(ROOT, "src", "python", "engine", "nbody", "orior", "instrument"))

import boundary_check  # noqa: E402
import corpus_admit  # noqa: E402
import coverage_check  # noqa: E402
import sift_extract  # noqa: E402
from corpus_derivation import reported  # noqa: E402
from orior import self_distance, squash, support  # noqa: E402

GOLD = os.path.join(ROOT, "build", "salishan_gold", "gold_figures.tex")
SIFTED = os.path.join(ROOT, "build", "salishan_gold", "gold_figures_sifted.tex")
# The figures of the tool-extraction reading the chapter sets: the ones about papers.
SIFTED_FIGURES = re.compile(r"^Gold(Papers\w*|Median\w*|NoiseTimes)$")
TARGET = os.path.join(ROOT, "theory", "theory", "Salishan", "chapters", "figures_orior_salishan.tex")

# The corpus whose wider split-half distance the chapter's sentences about the unread pair, the
# dialect border and the pooled profile name.
NAMED = "Lushootseed"

COVERED = re.compile(r"(\d+) of (\d+) papers have every token of the language accounted for")
MISSING = re.compile(r"^\s+\S+\s+missing a file\s*$", re.MULTILINE)
SCREENED = re.compile(r"(\d+) papers written to")
NEARER = re.compile(r"(\d+) lines nearer the language, (\d+) residue")
NAMED_IN = re.compile(r"(\d+) of the (\d+) carry the language")
ADMITTED = re.compile(r"^  (.{16}) (\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s*$", re.MULTILINE)
HELD_ONLY = "languages with candidates and no pure corpus to grow"
HELD_ROW = re.compile(r"^    (.+?)\s+(\d+)\s*$")
NEEDS = re.compile(r"byte pairs\s+([\d.]+)\s+x")
MACRO = re.compile(r"\\newcommand\{\\(\w+)\}\{([^}]*)\}")


class Stop(Exception):
    """A check whose words no longer match, or a result whose shape the chapter's sentences rest on."""


def found(pattern, text, what):
    """The first match of a check's own words, or a stop naming what was looked for."""
    match = pattern.search(text)
    if match is None:
        raise Stop("no %s in the check's output" % what)
    return match


def held(condition, what):
    """A shape the chapter's sentences rest on, or a stop saying which."""
    if not condition:
        raise Stop("the chapter says %s, and the measurement no longer does" % what)


def gold_macros(path=GOLD):
    """The exact program's figures, as it wrote them."""
    if not os.path.isfile(path):
        raise Stop("no %s; run examples/Salishan/4_measure/gold_readings.sh first" % path)
    with open(path, encoding="utf-8") as handle:
        return dict(MACRO.findall(handle.read()))


def admission():
    """The admission table's rows and the languages held only as candidates."""
    text = reported(corpus_admit)
    rows = [
        (name.strip(), pure, candidates, admitted, before, after, cells)
        for name, pure, candidates, admitted, before, after, cells in ADMITTED.findall(text)
    ]
    if not rows:
        raise Stop("no admission rows in corpus_admit's output")
    only = []
    if HELD_ONLY in text:
        for line in text.split(HELD_ONLY, 1)[1].splitlines()[1:]:
            match = HELD_ROW.match(line)
            if match:
                only.append((match.group(1), int(match.group(2))))
    return rows, only


def whole_set(name):
    """A corpus with every one of its sifted candidates added: cells and split-half distance, before and after."""
    pure = corpus_admit.pure_by_language()[name]
    every = pure + corpus_admit.candidates_by_language()[name]
    before, _ = squash(pure)
    after, _ = squash(every)
    return support(before), self_distance(pure), support(after), self_distance(every)


def escape(text):
    """A name as TeX text."""
    return text.replace("\\", r"\textbackslash{}").replace("&", r"\&").replace("%", r"\%").replace("_", r"\_")


def figures():
    """Every macro the chapter reads, as (name, value), after holding each shape its sentences rest on."""
    gold = gold_macros()
    tool = {
        "Sifted" + match.group(1): value
        for name, value in gold_macros(SIFTED).items()
        for match in [SIFTED_FIGURES.match(name)]
        if match
    }
    coverage = reported(coverage_check)
    covered = found(COVERED, coverage, "count of papers fully accounted for")
    missing = len(MISSING.findall(coverage))
    sifted = reported(sift_extract)
    screened = found(SCREENED, sifted, "count of papers screened")
    nearer = found(NEARER, sifted, "count of lines nearer the language")
    named_in = found(NAMED_IN, sifted, "count of papers carrying their named language")
    border = found(NEEDS, reported(boundary_check), "factor the border needs")
    rows, only = admission()

    held(gold.get("GoldPairsUnread") == "1", "one pair of corpora does not read")
    held(gold.get("GoldUnreadWider") == NAMED, "the unread pair fails on %s's split-half distance" % NAMED)
    held(not gold.get("GoldRatioLeast", "0").startswith("0."), "the midpoint figure is the larger every time")
    held(gold.get("GoldLanguagePairsReadAlternating") == gold.get("GoldLanguagePairs"),
         "every pair of languages reads at the alternating split")
    held(gold.get("GoldPapersRead") == "0", "no paper reads from its screened lines")
    held(gold.get("GoldPapersSmall") == "0", "every paper's screened lines hold enough pairs to ask")
    held(tool.get("SiftedPapersRead", "0") != "0", "a paper reads from its tool extraction")
    held(tool.get("SiftedPapersAgreed") == tool.get("SiftedPapersRead"),
         "every paper that reads from its tool extraction agrees with its prose")
    named_row = [row for row in rows if row[0] == NAMED]
    held(bool(named_row) and named_row[0][2] == named_row[0][3], "%s took every candidate" % NAMED)
    refused = [row for row in rows if row[3] == "0"]
    held(bool(refused), "a corpus refused all of its candidates")

    cells_before, split_before, cells_after, split_after = whole_set(refused[0][0])
    table = " \\\\\n".join(
        " & ".join([escape(name), pure, candidates, admitted, before, after, cells])
        for name, pure, candidates, admitted, before, after, cells in rows
    ) + " \\\\"
    refusers = [escape(row[0]) for row in refused]
    only.sort(key=lambda pair: (-pair[1], pair[0]))

    values = [(name, value) for name, value in sorted(gold.items())]
    values += [(name, value) for name, value in sorted(tool.items())]
    values += [
        ("ReaderPapers", str(len(sift_extract.READ))),
        ("HandExtractions", str(int(covered.group(2)) + missing)),
        ("Extractors", covered.group(2)),
        ("ExtractorsAccounted", covered.group(1)),
        ("NoExtractor", str(missing)),
        ("SiftPapers", screened.group(1)),
        ("SiftNearer", nearer.group(1)),
        ("SiftResidue", nearer.group(2)),
        ("SiftNamed", named_in.group(1)),
        ("BorderNeeds", border.group(1)),
        ("AdmissionRows", table),
        ("AdmitNamedCandidates", named_row[0][2]),
        ("AdmitNamedBefore", named_row[0][4]),
        ("AdmitNamedAfter", named_row[0][5]),
        ("AdmitRefused", " and ".join([", ".join(refusers[:-1]), refusers[-1]]) if len(refusers) > 1 else refusers[0]),
        ("WholeName", escape(refused[0][0])),
        ("WholeCellsBefore", str(cells_before)),
        ("WholeCellsAfter", str(cells_after)),
        ("WholeSplitBefore", "%.3f" % split_before),
        ("WholeSplitAfter", "%.3f" % split_after),
        ("CandidateOnlyCount", str(len(only))),
        ("CandidateOnlyList", ", ".join("%s %d" % (escape(name), count) for name, count in only)),
    ]
    return values


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    try:
        values = figures()
    except Stop as stop:
        out.write("  instrument_figures: %s\n" % stop)
        out.flush()
        return 1
    text = "".join("\\newcommand{\\%s}{%s}\n" % (name, value) for name, value in values)
    old = None
    if os.path.isfile(TARGET):
        with open(TARGET, encoding="utf-8") as handle:
            old = handle.read()
    if text != old:
        with open(TARGET, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        out.write("  wrote %s\n" % os.path.relpath(TARGET, ROOT).replace(os.sep, "/"))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
