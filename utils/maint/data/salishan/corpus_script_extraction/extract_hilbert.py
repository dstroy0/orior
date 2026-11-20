#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Lushootseed of Vi taqʷšəblu Hilbert from ICSNL 1983, in the order of that paper's own structure.
#
#   Usage:  python maint/data/salishan/corpus_script_extraction/extract_hilbert.py
#
# This file serves one paper. `Poking Fun in Lushootseed` is Hilbert's essay, in English, about humor that Hilbert's
# students often miss, with forty numbered examples in Lushootseed set into it. The paper prints each example twice
# under the same number: the Lushootseed first, then Hilbert's English for it. The number is therefore the pair, and the
# order decides which is which. Nothing else needs to.
#
# THIS FILE DOES NOT REPAIR THE ORTHOGRAPHY.
#
# This is a 1983 typescript, and its scan is badly damaged. The glottal stop ʔ arrives as ?, ə as ~ and J and G, and ʷ
# as V and v. The name of the author arrives as `VI [!.aq liS'"} blu] lIil bert`. The Lyon papers had damage like this,
# and a repair works there, but only because a test of a candidate table is possible. The table moves attested tokens
# from 1 of 3599 to 811 against Lyon's later papers on the same language.
#
# The same test on this paper says nothing. Six modern Lushootseed papers yield 1068 distinct Lushootseed tokens between
# them. Of the 103 damaged tokens here, 0 match before a candidate mapping, and 0 after. Hilbert's vocabulary is Raven
# and Bear and Marblemount and the names of houses, and theirs is grammar. The reference does not share enough with this
# paper to decide anything. font_substitution.py names that case as a test that says nothing either way.
#
# The text therefore comes out as it arrived. A guessed table applied to Vi Hilbert's words would put forms into a
# corpus that nobody said, and nothing downstream would ever question them. Marked damaged and left alone, the words
# stay Hilbert's. The repair stays available to whoever finds a reference that shares this vocabulary.
#
# WHERE THE ENGLISH ENDS, WHICH THIS FILE GETS RIGHT NINETEEN TIMES IN TWENTY-ONE
#
# The typescript sets each example in an indented column, and the essay at full width, and the extraction dropped every
# leading space. The width survived, and separates the two, and COLUMN below reads it. It does not separate an example's
# English from a story summary under it. Those share the same indented column, and the extraction lost the blank line
# between them too.
#
# The proportion rule catches eight of the ten that run on. The English of an example is about as long as the example,
# and never more than one line longer. Examples 15 and 18 run on by exactly one line, and pass it. The hand extraction,
# the control for this file, records both correctly, and neither reaches the pure stream, because only the Lushootseed
# does.
#
# The paper asks that nobody ever explain the moral of a story. That is not for this file to keep or break. It is the
# reason that the file keeps the essay whole beside the examples, and does not throw it away as apparatus. The essay is
# where Hilbert says what the paper does, and why.

import io
import os
import re
import subprocess
import sys

from salish_marking import (
    DERIVED,
    SPOKEN,
    UNCLASSIFIED,
    rendered,
    switches,
    tagged_spans,
)
from salish_unsorted import UNKNOWN_KIND, covered_tokens, unreached, write_unsorted


def _repository_root():
    """This repository, asked of git and not inferred from a marker directory.

    A marker that the repository produces, such as build/, is absent from a linked worktree and from a clone with no
    build. A climb to it can pass this root and land in another checkout whose paths look valid. A marker infers the
    root. Git answers it. The climb below serves only an exported tree with no git directory. It looks for
    archive/src/python, which the repository tracks and every checkout of it holds.

    The function clears the variables of git first. Inside a hook, git exports GIT_DIR. A rev-parse that inherits it
    answers about that repository, and not about the directory that the caller asks from. It then returns the current
    directory instead of the root.
    """
    start = os.path.dirname(os.path.abspath(__file__))
    environment = dict(os.environ)
    for key in (
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_INDEX_FILE",
        "GIT_PREFIX",
        "GIT_COMMON_DIR",
    ):
        environment.pop(key, None)

    try:
        said = subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"],
            cwd=start,
            stderr=subprocess.PIPE,
            env=environment,
        )
    except (OSError, subprocess.CalledProcessError):
        said = b""

    top = said.decode("utf-8", "replace").strip()
    if top and os.path.isdir(top):
        return os.path.abspath(top)

    climbed = start
    while (climbed != os.path.dirname(climbed)) and not os.path.isdir(
        os.path.join(climbed, "src", "cu", "engine")
    ):
        climbed = os.path.dirname(climbed)
    return climbed


ROOT = _repository_root()
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

SOURCE = os.path.join(PAPERS, "1983_Hilbert.txt")

# <spoken by>_<original paper>_<who wrote it down>_Salish_<language without accents>_<year>_<mixed>
#
# The speaker comes first, because the speaker is who the corpus is of. Vi Hilbert wrote this paper, and the field is
# not Hilbert's. Two people said the twenty-one examples in it: Hilbert's aunt Susie Sampson Peter of the Upper Skagit,
# and Martha LaMont. Leon Metcalf recorded them between 1950 and 1958, and Thom Hess in 1963. Hilbert transcribed and
# translated them afterward.
TARGET = os.path.join(
    CORPORA,
    "SusieSampsonPeter-MarthaLaMont_PokingFunInLushootseed_Hilbert"
    "_Salish_lushootseed_1983_mixed.txt",
)

PAGE = re.compile(r"^===== page \d+ =====$")

# A marker anywhere in a line, and not only at its start. The pattern allows the space that this typewriter left inside
# the bracket on either side of the number. Example 5 ends, and its English begins, on one line:
#
#   `qa~qs. tai ?as~awit. (5 ) And he`
#
# An anchor at the start would put the English into the Lushootseed stream. A space allowed only after the number would
# miss `( 20)`. The whole English translation of example 20 would then read as more of Susie Sampson Peter's sentence.
# It would go into the pure stream as something that Susie Sampson Peter said.
NUMBERED = re.compile(r"\(\s*(\d{1,3})\s*\)")

# A page number the typescript left on its own line, between an example and its translation.
PAGE_NUMBER = re.compile(r"^\d{1,4}$")

# The width of the indented column that the examples sit in.
#
# The typescript indents every numbered example, its Lushootseed and its English alike, and sets the essay at full
# width. The extraction dropped the leading spaces. Both arrive flush left, and nothing in a line says which block it
# belongs to. What survived is the width. Of the 138 lines inside an example block, the longest is 46 characters, and
# 144 of the 308 lines of the essay are longer than that.
#
# Without this, the English of an example runs on into the essay under it. The translation of example 2 is a single
# line, ‘High class, high class was Raven.’. With no width rule, its row would take the next eleven lines of Hilbert's
# commentary, as something that Susie Sampson Peter said. Coverage would still read 100 percent, because every token
# would be in the file, in the wrong row.
COLUMN = 46

# What the damaged typescript writes the language with. Not the modern orthography: ? is the
# glottal stop here, ~ and J and G are the schwa, V and v are labialization. A test built on ʔ and
# ə finds nothing in this file at all.
MARKS = "?~JG@V%]!"

LAYER = {
    "transcription": SPOKEN,
    "citation": SPOKEN,
    # Hilbert's English for what Hilbert's aunt and Martha LaMont said, made from the recordings years afterward. It is
    # Hilbert's, and is not a record of anything either of them uttered.
    "translation": DERIVED,
    "essay": DERIVED,
    # The page of Thomas E. Hukari's Halkomelem and Configuration the PDF carries after this paper.
    "foreign": DERIVED,
    UNCLASSIFIED: DERIVED,
}


def carries_language(text):
    """Whether a line holds a character this typescript writes the language with."""
    return any(mark in text for mark in MARKS)


def examples(lines, ends_at):
    """The forty numbered examples, each as its Lushootseed and the English for it.

    A number appears twice. The first time, it opens the Lushootseed, and the second time it opens the translation. Each
    runs on until the next number or a page number interrupts it. Content cannot tell the two apart here, because the
    damage makes both look like neither.
    """
    held = {}
    order = []
    taken = set()
    number = None
    which = None
    # Set when a marker opened a Lushootseed field and carried no text with it. The typescript puts such a marker in the
    # left margin, part way down the English of the previous example. The OCR flattened that into its own line. The
    # English of example 3 runs on for two lines after the bare (4) that interrupts it. Those two lines are English, and
    # not the start of example 4.
    waiting = False
    previous = None

    def open_number(one):
        if one not in held:
            held[one] = {"said": [], "english": []}
            order.append(one)
            return "said"
        return "english"

    for at, line in enumerate(lines):
        # Where this paper ends, and the next one in the volume begins. Its opening lines are short enough to pass for
        # the indented column, and the English of example 21 would take its title.
        if at >= ends_at:
            break
        trimmed = " ".join(line.split())
        if not trimmed or PAGE.match(trimmed) or PAGE_NUMBER.match(trimmed):
            continue

        marks = list(NUMBERED.finditer(trimmed))
        # A line too wide for the indented column is where the essay starts again, and it closes whatever example was
        # open. The test runs before the marker search, because the essay names an example number in prose often enough
        # to matter.
        if len(line.rstrip()) > COLUMN:
            number = None
            which = None
            waiting = False
            continue
        if not marks:
            if (number is None) or (which is None):
                continue
            taken.add(at)
            # A line with none of the typescript's marks, while a Lushootseed field still waits for its first line, is
            # the English of the previous example. That English runs past the marker in the margin.
            if waiting and not carries_language(trimmed) and (previous is not None):
                held[previous]["english"].append(trimmed)
                continue
            if waiting:
                waiting = False
            held[number][which].append(trimmed)
            continue

        # Text before the first marker belongs to whatever was open.
        taken.add(at)
        lead = trimmed[: marks[0].start()].strip()
        if lead and (number is not None) and (which is not None):
            held[number][which].append(lead)
        for index, mark in enumerate(marks):
            if (number is not None) and (which == "english"):
                previous = number
            number = int(mark.group(1))
            which = open_number(number)
            stop = (
                marks[index + 1].start() if (index + 1) < len(marks) else len(trimmed)
            )
            rest = trimmed[mark.end() : stop].strip()
            if rest:
                held[number][which].append(rest)
                waiting = False
            else:
                waiting = which == "said"
    return [(one, held[one]) for one in order], taken


def main():
    out = io.TextIOWrapper(
        sys.stdout.buffer, encoding="utf-8", errors="replace", newline=""
    )
    os.makedirs(CORPORA, exist_ok=True)
    if not os.path.isfile(SOURCE):
        out.write("  no %s\n" % SOURCE)
        out.flush()
        return 1

    with open(SOURCE, encoding="utf-8", errors="replace") as handle:
        lines = [one.rstrip("\n") for one in handle]

    # The PDF carries the first page of the next paper in the volume, Thomas E. Hukari's Halkomelem and Configuration,
    # after this one ends. Its prose is English and holds no Lushootseed, but the damaged mark set reads Victoria and
    # nonconfigurationality? as forms of the language. It would put both into this record as things somebody said.
    #
    # The file keeps that page and names it, and does not cut it. A cut would lose those tokens from the record, and the
    # coverage check would then report this paper as incomplete. It is not incomplete. The words are in the PDF, and
    # they belong to somebody else's paper.
    next_paper = len(lines)
    for at, line in enumerate(lines):
        if line.strip().startswith("Halkome"):
            next_paper = at
            break

    rows = []
    held_lines = []
    cited = set()
    found, taken = examples(lines, next_paper)
    for number, parts in found:
        said = " ".join(parts["said"])
        english = " ".join(parts["english"])
        held_lines.append((0, number, 0, len(parts["said"]), len(parts["english"])))
        if said:
            rows.append(("T", number, "transcription", said))
        if english:
            rows.append(("N", number, "translation", english))

    # Hilbert's essay. It is English and it is Hilbert's, and it is what the examples sit in. The file keeps it, marked
    # derived, and does not drop it as apparatus. The lines that the examples took tell it apart, and no comparison of
    # text does. A line matched against a bag of words matches nothing, and every continuation line would go into the
    # essay a second time.
    for at, line in enumerate(lines):
        trimmed = " ".join(line.split())
        if (
            not trimmed
            or PAGE.match(trimmed)
            or PAGE_NUMBER.match(trimmed)
            or (at in taken)
        ):
            continue
        if at >= next_paper:
            rows.append(("N", 0, "foreign", trimmed))
            continue
        rows.append(("N", 0, "essay", trimmed))
        # Hilbert names four words in the essay and glosses each in quotes. The essay also gives three names: the
        # supernatural `dawi?`, the same name written `~awi?` ten lines later, and the place `~acaladi`. Those are the
        # only Lushootseed outside the numbered examples. A reader that keeps the essay whole and does not pull them out
        # leaves seven forms in prose and out of the record.
        for token in trimmed.split():
            # Only sentence punctuation comes off. The damaged orthography writes with ], [, ' and " as characters.
            # `]u?il ‘happy’` opens with one, and a strip of it gives `u?il`, a word that this paper does not contain.
            plain = token.strip(".,;:")
            if plain and carries_language(plain) and (plain not in cited):
                cited.add(plain)
                rows.append(("T", 0, "citation", plain))

    missed = unreached(lines, covered_tokens(one[3] for one in rows))
    for page, where, reason, missing, text in missed:
        rows.append(("T", 0, UNCLASSIFIED, text))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# Poking Fun in Lushootseed.\n")
        handle.write(
            "# Vi taqʷšəblu Hilbert, University of Washington. Papers for the\n"
        )
        handle.write(
            "# International Conference on Salish and Neighboring Languages, 1983.\n"
        )
        handle.write(
            "# Her essay on humor her students retained missing, with forty numbered\n"
        )
        handle.write("# Lushootseed examples and her own English for each.\n")
        handle.write("#\n")
        handle.write(
            "# ORTHOGRAPHY NOT REPAIRED. This is a 1983 typescript and its scan is\n"
        )
        handle.write(
            "# damaged: ? is for the glottal stop, ~ and J and G for the schwa,\n"
        )
        handle.write(
            "# V and v for labialization. A repair table for it cannot be\n"
        )
        handle.write(
            "# tested: of 103 damaged tokens, 0 are attested in six modern Lushootseed\n"
        )
        handle.write(
            "# papers before a mapping and 0 after, because her story vocabulary and\n"
        )
        handle.write(
            "# their grammar vocabulary do not meet. An untested table applied to her\n"
        )
        handle.write("# words would put forms in that nobody said. None was applied.\n")
        handle.write("#\n")
        handle.write(
            "# Mark is language.layer.type. T is Lushootseed, N is anything else.\n"
        )
        handle.write(
            "# An example is printed twice under one number, the Lushootseed first and\n"
        )
        handle.write(
            "# her English second. The number pairs them and the order names them.\n"
        )
        handle.write("line\ttype\tswitches\tcontent\n")
        for mark, number, type, text in rows:
            # Not span-marked. The damage leaves this paper's Lushootseed in plain ASCII. The
            # span test reads huy, six, tud and Zilid as English and cuts them out of her own
            # sentence. She does not switch languages inside an example: her English is the second
            # block under the same number. An example line is one language from end to end.
            content = "%s.%s.%s:{%s}" % (mark, LAYER[type], type, text)
            handle.write("line#${%d}\t%s\t0\t%s\n" % (number, type, content))

    pure = TARGET[:-4] + ".pure.txt"
    retained = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, number, type, text in rows:
            if (mark != "T") or (type != "transcription"):
                continue
            key = " ".join(text.split())
            if not key or (key in already):
                continue
            already.add(key)
            handle.write("%s\n" % key)
            retained += 1

    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [
        (0, "example %d" % number, UNKNOWN_KIND, "", text)
        for mark, number, type, text in rows
        if type == UNCLASSIFIED
    ]
    flagged.extend(missed)

    # The English of an example and the story summary under it sit in the same indented column. The width rule that
    # separates both from the essay does not separate them from each other. Nothing else in the extracted lines does
    # either. The extraction lost the blank line that the typescript puts between two paragraphs, and both are English
    # prose in the same measure.
    #
    # What the paper does give is proportion. The English for an example runs to about as many lines as the example, and
    # never more than one longer. Example 13 is the exception, because its translation carries two bracketed asides. A
    # translation much longer than that ran on into the summary, and this file names the ones that did. The file trims
    # none at a guessed point.
    for number, said, english in ((one[1], one[3], one[4]) for one in held_lines):
        if english > (said + 1):
            flagged.append(
                (
                    0,
                    "(%d)" % number,
                    UNKNOWN_KIND,
                    "",
                    "the English is %d lines against %d of Lushootseed. It has run on "
                    "into the story summary under it; where it ends is not recoverable "
                    "from the extracted text and the hand extraction has the boundary"
                    % (english, said),
                )
            )
    stuck_count = write_unsorted(stuck, "Poking Fun in Lushootseed", flagged)

    counted = {}
    for mark, number, type, text in rows:
        counted[type] = counted.get(type, 0) + 1
    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write(
        "  %d target-language spans written to\n  %s\n" % (retained, os.path.basename(pure))
    )
    out.write(
        "  %d lines the tool could not sort written to\n  %s\n"
        % (stuck_count, os.path.basename(stuck))
    )
    out.write(
        "\n  by type: %s\n"
        % ", ".join("%s %d" % (one, counted[one]) for one in sorted(counted))
    )
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
