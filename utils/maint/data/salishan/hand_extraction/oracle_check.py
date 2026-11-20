#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Check a hand extraction against the paper that it comes from.
#
#   Usage:  python maint/data/salishan/hand_extraction/oracle_check.py
#
# The hand extraction is the control that the code grades each reader against. That puts the full weight of the corpus
# on its correctness. A person who reads a thirty-seven page paper into a table skips rows and mistypes marks. Neither
# shows up later as anything but a corpus that quietly disagrees with the paper.
#
# So the code checks the table against the paper both ways:
#   - each form written down must be in the source. That catches a mistyped mark and an invented row.
#   - each word in the source must appear in some form written down. That catches a skipped row, and a table read only
#     halfway.
#
# The second direction finds omissions, and omissions are what a person who reads by hand produces.
#
# Both sides go through the repairs of the paper first, for the reason that coverage_check.py states. A comparison of a
# repaired hand extraction against an unrepaired source reports each correctly repaired word as absent.

import io
import os
import re
import subprocess
import sys
import unicodedata


def _repository_root():
    """This repository, asked of git and not inferred from a marker directory.

    A marker that the repository produces, such as build/, is absent from a linked worktree and from a clone that never
    built. A climb to it can pass this root, and land in another checkout whose paths look valid. A marker infers the
    root, and git answers it. The climb below serves only an exported tree with no git directory. It looks for
    archive/src/python, which the repository tracks and each checkout of it holds.

    The code clears the own variables of git first. Inside a hook, git exports GIT_DIR. A rev-parse that inherits it
    answers about that repository, and not about the directory that the caller asks from. It then returns the current
    directory, and not the root.
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
HERE = os.path.dirname(os.path.abspath(__file__))

# The hand extractions are evidence and not tooling. They live in the research body beside the
# prose that cites them. The language in them belongs to the people who spoke it, and their names
# open every table and the README beside them.
ORACLES = os.path.join(ROOT, "examples", "Salishan", "oracles")

sys.path.insert(0, os.path.join(os.path.dirname(HERE), "corpus_script_extraction"))

# Imported under another name, because this file already binds `joined` as a local, for the token welded out of a run of
# pieces. These are two different joins. To shadow one with the other reads as an absent value at run time. It is a name
# collision.
from line_breaks import joined as joined_lines  # noqa: E402
from salish_unsorted import is_language_token  # noqa: E402

from papers import EVERY, NOT_FAITHFUL, ORTHOGRAPHY_ABSENT, PAGE_TEXT  # noqa: E402

EDGES = '.,!?;:“”"()[]…«»{}/*•→≤≥'

# Hall and Phillips write the null third person clitic with a symbol font, and the extraction carries that glyph through
# as a private use character. It marks where a morpheme has no sound. It is never part of a form. It sits inside a token
# as often as at the end of one: `cw-[n]-t-<glyph>-és=us` is one word with one silent morpheme in its middle.
NULL_CLITIC = ""

# The single quote is not in EDGES, because several of these orthographies write with it. `’` is the glottalization mark
# in Nuxalk and in the Okanagan of Lyon. A strip of it turns the enclitic `˽c’` into `˽c`, a string that neither the
# paper nor the hand extraction holds. It comes off only as one half of a pair around a gloss.
PAIRED = (("‘", "’"), ("'", "'"), ("“", "”"))

# How many tokens one broken word can arrive as. A pair covers the papers that break before a marked character, where
# `các l̓ep` is two. The papers of Lyon break more than once: `nt̓ə k̓ʷt̓í k̓ʷləx` is three pieces of one word, and
# `s- m̓ y̓- m̓ y̓-á y̓-s` is five. For a table that holds the full word, the checker then reports each piece of it as a
# word that no row covers.
#
# Six is where these two papers stop. A higher value costs a longer join list. It also raises the chance that some run
# of tokens by accident matches a form that the table wrote down wrongly. That is the only thing that direction one
# exists to catch.
PIECES = 6

# The mark Lyon opens every parsed root with.
ROOT_MARK = "√"

# What is in front of a √ inside one parse. `yaQ•√yáQt` is a reduplicant and its root, and `n+√ ’ks+tan` a preposition
# and its root: one form each. A √ with anything else in front of it is the point where the extraction ran a surface
# line into the parse line under it.
INSIDE_PARSE = "+-•="


def surface_parse_join(token):
    """Where a token runs a form into its own parse, the position the parse starts at, else -1."""
    at = token.find(ROOT_MARK)
    if (at <= 0) or (token[at - 1] in INSIDE_PARSE):
        return -1
    return at


# The Latin ligatures a PDF sets f-words with. None of these orthographies uses one. Every
# occurrence is the typesetter's and the characters underneath are what the paper says. Lyon's
# translations carry ﬁnish, ﬁll and ﬁrst, and a table typed at a keyboard holds none of them.
LIGATURES = (("ﬁ", "fi"), ("ﬂ", "fl"), ("ﬀ", "ff"), ("ﬃ", "ffi"), ("ﬄ", "ffl"))

# The three forms of one apostrophe, folded to the plain one. A typesetter sets `’` where a person at a keyboard types
# `'`. NFC does not unify them, because they are separate characters and not normalization variants.
# ICSNL58_Givens_Hall_final holds 39 plain apostrophes in its form column against 62 typographic ones in the paper.
# Without the fold, the checker cannot match those rows.
#
# This folds and never strips. `’` is the glottalization mark in Nuxalk and in the Okanagan of Lyon. To take it off is
# the fault that the PAIRED note below names. The fold leaves the mark on the form, and only settles which codepoint
# writes it. A glottalized form must still meet a glottalized form, and can never match a plain one.
QUOTES = (("’", "'"), ("‘", "'"))


# A footnote number, a second closing quote, and a mangled one. All three sit past the own punctuation of the sentence:
#   - `tuʔúʔt.6` puts the marker after the period.
#   - `yéyeʔ?”’` closes the inner quote after the outer one.
#   - `tea.̓` is a `’` that the PDF left as a bare diacritic.
#
# To peel any of them off the end of a word outright takes the glottalization off `˽c’`, and the 3 off a 3OBJ gloss.
# Each comes off only where an EDGES character is in front of it.
def trailing_marker(plain):
    """One token with a footnote marker and a doubled closing quote off the end of it."""
    while len(plain) > 1:
        at = len(plain)
        while (at > 0) and plain[at - 1].isdigit():
            at -= 1
        # A footnote marker set against a word that ends in a stacked mark, as Hall and Phillips'
        # nhén̓4 is. 7 is the glottal stop in the van Eijk orthography. A run that is only 7 is
        # the word's own last character and stays.
        if (
            (at < len(plain))
            and (at > 0)
            and (plain[at] != "7")
            and unicodedata.combining(plain[at - 1])
        ):
            return plain[:at].strip(EDGES)
        # Nothing numeric came off. Look for the quote forms instead.
        if (at == len(plain)) and (
            (plain[-1] == "’") or unicodedata.combining(plain[-1])
        ):
            at -= 1
        # A footnote marker set past a closing quote, as walk.’17 is. The ’ counts as a quote only
        # where the sentence's own punctuation is in front of it, which retains ˽c’ whole.
        elif (at > 1) and (plain[at - 1] == "’") and (plain[at - 2] in EDGES):
            at -= 1
        if (at == len(plain)) or (at == 0) or (plain[at - 1] not in EDGES):
            break
        plain = plain[:at].strip(EDGES)
    return plain


# A footnote number set in front of the word that it marks, as in the `70gagayat` of Mary George. The two numbers stack:
# `(28)140chechlhem` carries the line number and the footnote number both. The strip runs until nothing else comes off.
# 7 is the glottal stop in the van Eijk orthography. `7amash` and `t7u` open with a digit and are whole words. A run of
# one 7 stays, and a run that holds any other digit is the marker.
def leading_marker(plain):
    """One token with the line and footnote numbers off the front of it."""
    while True:
        at = 0
        while (at < len(plain)) and plain[at].isdigit():
            at += 1
        if (at == 0) or (at == len(plain)) or (plain[:at] == "7"):
            return plain
        plain = plain[at:].lstrip(EDGES)


# A footnote marker set against a word that ends in a plain character, as the `zuxʷt5`, `ʕant7` and `ks-cúy-iʔ-səlx11`
# of Lyon are. `trailing_marker` leaves those alone, and it must. A run of digits at the end of a word is the own last
# character of the word in the van Eijk orthography. There, `skúza7` and `Cw7aoz` write the glottal stop as 7.
#
# So the code never applies this. It offers it as a second string to look for, the way it offers a slashed cell and a
# hyphen join. Direction two lets a token through only when the table holds the form without the marker. Both sides must
# then agree before the code skips anything. A paper that writes 7 as a character stays the same, because its rows carry
# the 7.
def without_marker(plain):
    """One token with a final run of digits off the end of it, or the token as it arrived."""
    at = len(plain)
    while (at > 0) and plain[at - 1].isdigit():
        at -= 1
    if (at == len(plain)) or (at == 0) or (not plain[at - 1].isalpha()):
        return plain
    return plain[:at]


# The characters of the paper under check, set once per paper by main.
#
# Module state, because eight places across three functions reach bare(), and the paper is the same for all of them
# within one check. It sits beside EDGES, PAIRED and LIGATURES, which are the same type of fact about how a paper
# writes. This is the part of that fact which changes per paper, and does not hold for all of them.
MARKS = ""


def edges_for(marks):
    """The punctuation to strip, with the own characters of this paper taken out of it.

    Off by default. On for each paper, it raises the disagreements across the tree, and it does not move the paper that
    it aims at. Pass an explicit `marks` to use it for one call.
    """
    return "".join(one for one in EDGES if one not in (marks or ""))


def bare(token, marks=None):
    """One token with the punctuation around it off, and its orthography left alone.

    An opening quote always comes off: no orthography here starts a word with one. A closing quote
    comes off only where an opening one is on the same token, because ’ ends real words in Nuxalk
    and in Lyon's Okanagan.

    `marks` is the paper's own characters, and anything in it is not punctuation for that paper. It
    defaults to the paper under check, which main sets before reading either side of it.
    """
    edges = edges_for(MARKS if marks is None else marks)
    plain = token.replace(NULL_CLITIC, "")
    for ligature, characters in LIGATURES:
        plain = plain.replace(ligature, characters)
    plain = plain.strip(edges)
    for opens, closes in PAIRED:
        if (len(plain) > 1) and plain.startswith(opens) and plain.endswith(closes):
            plain = plain[1:-1].strip(edges)
    while plain and (plain[0] in "‘“"):
        plain = plain[1:].strip(edges)
    plain = leading_marker(trailing_marker(plain))
    # Last, after each strip runs. An earlier fold turns ’form’ into 'form'. PAIRED then takes the quotes off both ends
    # of it, and one of those ends was the glottalization mark. The strips must see the characters that the page
    # printed. Only the comparison needs the two forms settled.
    for typographic, plain_quote in QUOTES:
        plain = plain.replace(typographic, plain_quote)
    return plain


# What a form can contain besides its characters. Each of these is part of how the paper writes a form:
#   - a morpheme boundary, or a clitic boundary.
#   - a reduplication tilde.
#   - the parentheses around a deleted segment.
#
# A split on them compares pieces that the paper never printed apart.
INSIDE = "-=~()"


def pieces(form):
    """One written form as the strings that a source line can hold it as.

    Composed the same way as the source. A hand extraction comes from a keyboard that composes á as one character, while
    the paper sometimes writes it as two. Without this, the code reports each accented form written by hand as one that
    the paper does not hold.
    """
    held = set()
    for token in unicodedata.normalize("NFC", form).split():
        plain = bare(token)
        if plain:
            held.add(plain)
    return held


def oracle_rows(path):
    """Every row of a hand extraction, as where, who, type, form, gloss.

    Twenty-four of the twenty-five files declare that header. Mellesmoen_Kye_ICSNL61 declares
    `dialect` in the second column and fills it with dialect labels instead of the people who
    spoke. That is an open question and not a settled second shape. This reader takes the
    column by position and compares it against nothing, and both files parse.
    """
    held = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if (len(fields) < 4) or (fields[0] == "where"):
                continue
            held.append(
                (
                    fields[0],
                    fields[1],
                    fields[2],
                    fields[3],
                    fields[4] if len(fields) > 4 else "",
                )
            )
    return held


def wrong_width(path):
    """Each row that is not four or five fields wide, as (line number, field count).

    Four is a row with no gloss, and five is a full one. Each oracle on disk is one or the other. Any other width is
    damage, and not a shape that somebody chose.

    oracle_rows drops a short row, and says nothing. A wide one is worse, because oracle_rows keeps it, and all past the
    fifth field goes unread. One tab written where a space belongs turns `n > <null> / n_` into `n > <null>`. It moves
    the conditioning environment into the gloss column, where nothing reads it as one, and drops the name of the rule
    off the end. A conditioned rule becomes an unconditional one, attributed to a named language, and the row still
    looks well formed. Nothing else catches it, because one byte swapped for one byte keeps the file size, the line
    count and the row count.
    """
    held = []
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if fields[0] == "where":
                continue
            if len(fields) not in (4, 5):
                held.append((number, len(fields)))
    return held


def source_forms(path, repair=None, pieces=2, line_joins=False):
    """Each string of a paper that a written form can look for, with the line it sits on.

    pieces is how many tokens one broken word can arrive as, and it is 2 for a paper whose text is the page. Only the
    drafted ones need more. To give more to each paper triples the lookup on the nine that do not need it. That is a lot
    of strings for direction one to agree with by accident.

    Three things beyond the split on spaces:
      - a cell can hold two alternants divided by a slash, as Table A2 does at dᶻəlč̓/ǰəlč̓. The code offers each half
        of a slashed token, and also the whole token. That keeps the constraint name *P/ə in the lookup too.
      - a form can break across a line break, as sčəbíd-ac does in both of the Figure 7 captions. The code also offers a
        line that ends in a hyphen joined to the line after it.
      - all of this widens what a lookup finds, and none of it builds a corpus. An over-join here costs nothing but a
        question not asked.
    """
    held = {}
    # What the paper prints, without the joins and halves offered above. Direction two asks about these. A question
    # about a join makes each accidental pair a hole to answer for.
    printed = set()
    # Each printed token against what it makes when welded to the token beside it. A PDF that breaks
    # a word before a marked character leaves các and l̓epa, and the hand extraction has cácl̓epa. Both
    # halves are then words the paper prints that no row holds, and neither is a row anybody should
    # write: the word is one word.
    welds = {}
    previous = ""
    wrapping = False

    # This file applies the line join of coverage_check.py too. It applies only where the own config of the paper asks
    # for it, the gate that coverage_check has: `if "line joins" in repairs`. A PDF that breaks a word with no hyphen
    # leaves two fragments, as in the epigraph of 2012_Robertson, and line_breaks.joined welds them. Both checks must
    # read the same text. Else each word that one welds and the other does not reads as a hole. line_breaks.py is for
    # the defect of one paper. In a paper without it, it welds words that nobody wrote.
    with open(path, encoding="utf-8", errors="replace") as handle:
        raw = [one.rstrip("\n") for one in handle]
    lines = joined_lines(raw)[0] if line_joins else [one.rstrip() for one in raw]

    for number, line in enumerate(lines, 1):
        if line.startswith("====="):
            continue
        # NFC on both sides, always. Two strings are the same string only after it. Without it, the code reports ǰ typed
        # at a keyboard as absent from a paper that prints ǰ on ten lines.
        #
        # It runs after the repair. To compose a with a separate acute into á first takes that acute out of the set of
        # marks whose next space closes. The repair then misses each word that the PDF split at an accent. The repair
        # ends in NFC for the same reason.
        line = repair(line.rstrip()) if repair else line.rstrip()
        line = unicodedata.normalize("NFC", line)
        reach = [line]
        if previous.endswith("-"):
            # Both readings of the hyphen at a line end. sčəbíd- ac is one Salish form, whose hyphen is a morpheme
            # boundary and stays. The English translations of Lyon are typeset, and hold- ing is one word whose hyphen
            # the typesetter put there.
            tail = previous.split()[-1]
            reach.append("%s%s" % (tail, line.lstrip()))
            reach.append("%s%s" % (tail[:-1], line.lstrip()))
        elif re.search(r"(?:https?://|www\.)\S*$", previous) and re.match(r"[a-z&]", line.lstrip()):
            # A web address the line's end breaks with no hyphen, muckleshoot08m. / html.
            reach.append("%s%s" % (previous.split()[-1], line.lstrip()))
        # The same two readings of a hyphen that closes a table cell whose column wraps to the line below. That line
        # holds fewer cells, parted by runs of spaces: la̱-kin-te-he-me-sa- over num in the word list of Dawson. The
        # code offers the wrapped half against the head of each cell.
        if wrapping:
            for cell in re.split(r"\s{3,}", previous.strip())[:-1]:
                if cell.endswith("-"):
                    tail = cell.split()[-1]
                    for head in re.split(r"\s{3,}", line.strip()):
                        if head:
                            reach.append("%s%s" % (tail, head.split()[0]))
                            reach.append("%s%s" % (tail[:-1], head.split()[0]))
        cells = re.split(r"\s{3,}", line.strip())
        following = lines[number].strip() if number < len(lines) else ""
        wrapping = (len(cells) > 1 and bool(following) and not following.startswith("=====")
                    and len(re.split(r"\s{3,}", following)) < len(cells))
        cell_ends = set()
        if wrapping:
            count = 0
            for cell in cells[:-1]:
                count += len(cell.split())
                cell_ends.add(count - 1)
        previous = line
        for at, one in enumerate(reach):
            tokens = one.split()
            for where, token in enumerate(tokens):
                # The half of a wrapped form left at a line end is not a form to ask about. The join built from it above
                # is. p̓il- ‘flat’ ends in a hyphen too, and is a real prefix. For that reason, the code drops only the
                # last token on a line.
                #
                # It stays in the lookup all the same, because the page does print those characters at that place. The
                # interlinear of Lyon arrives one token per line, and each token in it is the last on its line. To drop
                # them outright loses an-, a-ks- and ʔakɬ-, which are forms that the paper prints on their own. The code
                # drops the half of a wrapped cell the same way.
                wrapped = (
                    (at == 0) and ((where == (len(tokens) - 1)) or (where in cell_ends)) and token.endswith("-")
                )
                # The token itself, its slash-separated halves, and the token joined to the tokens after it. The last of
                # those is for the PDFs that break a word before a marked character. `cácl̓ep` arrives as `các l̓ep`,
                # and the hand extraction records the word. A join offered here only widens what a lookup finds.
                plain = bare(token)
                if plain:
                    held.setdefault(plain, number)
                    # Only the line as printed adds to printed. The hyphen join built above is a lookup candidate. A
                    # count of its tokens makes ł-AUX, the tail of one line welded to the head of the next, a word that
                    # the paper holds.
                    if (at == 0) and not wrapped:
                        printed.add(plain)
                reach = list(token.split("/"))
                # The same token without a footnote marker welded to its last character.
                if plain:
                    reach.append(without_marker(plain))
                # The five-line interlinear of Lyon puts a form on one line and its parse on the next. The extraction
                # runs the two together where the parse opens with the root mark. `cáwt@t,√cáwt-tt` is the word and its
                # analysis in one token. A √ anywhere but the front is that join, and the code offers both sides.
                at_root = surface_parse_join(token)
                if at_root > 0:
                    reach.append(token[:at_root])
                    reach.append(token[at_root:])
                for span in range(2, pieces + 1):
                    if (where + span) > len(tokens):
                        break
                    run = tokens[where : where + span]
                    # The code offers the join with and without a footnote marker on the end of it.
                    # `n-t̓ə k̓[ʷ]-t̓í k̓[ʷ]-ləx2` is one word that carries a 2, and the row holds the word.
                    # `(s)K ékets’a7` ends in the character that van Eijk writes the glottal stop with, and the row
                    # holds the 7. Only the stripped form loses the second one.
                    joined = bare("".join(run))
                    if not joined:
                        continue
                    reach.append("".join(run))
                    # A run can be a surface form and its own parse, broken in the middle as well.
                    # `k̓ʷ l̓ncútn√k̓ʷ l̓-ncút+tn` is `k̓ʷl̓ncútn` and `√k̓ʷl̓-ncút+tn`. The table holds those two
                    # apart, because the paper prints them on two lines.
                    halves = [joined, without_marker(joined)]
                    at_join = surface_parse_join(joined)
                    if at_join > 0:
                        halves = [bare(joined[:at_join]), bare(joined[at_join:])]
                        reach.extend(halves)
                    for piece in run:
                        broken = bare(piece)
                        if broken:
                            welds.setdefault(broken, set()).update(
                                one for one in halves if one
                            )
                for part in reach:
                    plain = bare(part)
                    if plain:
                        held.setdefault(plain, number)
    return held, printed, welds


def main():
    out = io.TextIOWrapper(
        sys.stdout.buffer, encoding="utf-8", errors="replace", newline=""
    )
    failed = 0
    waiting = []
    for name, stem, record, repair, marks, line_joins in EVERY:
        table = os.path.join(ORACLES, name)
        # A paper whose extraction is the encoding of the font gets its check against the drafted page text. The
        # extraction is not what the paper says. A comparison against it only asks whether the table copied the damage
        # correctly.
        source = os.path.join(
            PAPERS, (PAGE_TEXT if stem in NOT_FAITHFUL else "%s.txt") % stem
        )
        if not os.path.isfile(table):
            waiting.append(stem)
            continue
        if not os.path.isfile(source):
            out.write("  no paper on disk for %s\n" % stem)
            failed += 1
            continue

        # Set before the code reads either side. The same rule strips the hand extraction and the paper. A different
        # strip on each is how a form that is in both reads as in neither. Left empty. edges_for records the effect of
        # this when on: more disagreements, and no move in the paper that it aims at. The mechanism is available per
        # call.
        global MARKS
        MARKS = ""

        rows = oracle_rows(table)

        # Width first, because it is structural, and holds whether the content has a grade. A paper in
        # ORTHOGRAPHY_ABSENT returns before the counts below, and a damaged row there is still a damaged row.
        wide = wrong_width(table)
        if wide:
            out.write("    %d row(s) of the wrong width\n" % len(wide))
            for number, count in wide:
                out.write(
                    "      line %-6d %d fields, expected 4 or 5\n" % (number, count)
                )
            failed += len(wide)

        broken = PIECES if stem in NOT_FAITHFUL else 2
        held, printed, welds = source_forms(source, repair, broken, line_joins)
        raw = source_forms(source, None, broken, line_joins)[0]
        out.write(
            "  %s%s\n"
            % (name, "   (against a drafted page text)" if stem in NOT_FAITHFUL else "")
        )
        out.write(
            "    %d rows read by hand, %d distinct tokens in the paper\n"
            % (len(rows), len(held))
        )

        # Direction one. A form written down that the paper does not hold is a mistyped form or an invented row. It puts
        # a word that nobody printed into the corpus.
        #
        # The code also asks against the unrepaired paper. A form that the repair took out is a different thing from a
        # form that nobody wrote. The person read it off the page correctly, and the repair then destroyed it. That is a
        # fact about the repair, and the code reports it as one.
        unfound = []
        cost = []
        written = set()
        for where, who, type, form, gloss in rows:
            for piece in pieces(form):
                written.add(piece)
                if piece in held:
                    continue
                (cost if piece in raw else unfound).append((where, type, form, piece))
        out.write("    %d forms the repair took out of the paper\n" % len(cost))
        for where, type, form, piece in cost:
            out.write("      %-12s %-11s %-28s %s\n" % (where, type, form, piece))
        out.write("    %d written forms the paper does not hold\n" % len(unfound))
        for where, type, form, piece in unfound:
            out.write("      %-12s %-11s %-28s %s\n" % (where, type, form, piece))

        # Direction two. A word in the paper that no row holds is a row that the reader skipped. The code asks only
        # about tokens that carry a character of the language. The rest of the paper is English prose, and the hand
        # extraction is not a transcription of that.
        missed = []
        for token, number in sorted(held.items(), key=lambda one: one[1]):
            if token not in printed:
                continue
            if not is_language_token(token, marks):
                continue
            # Half a word the PDF split. The hand extraction holds the whole word.
            if welds.get(token, set()) & written:
                continue
            # An English possessive on a name in the language. These papers are in English, and put one on
            # `Kʷəɬtəzétkʷu’s`. The word is the name, and the row holds the name.
            if token.endswith(("’s", "'s")) and (token[:-2] in written):
                continue
            # A capital opening a sentence. None of these orthographies tell two words apart by
            # case. The Yéyeʔ that opens a translation is the yéyeʔ a row already holds.
            if token.lower() in written:
                continue
            # A footnote marker welded to the word that it marks, as in the zuxʷt5 of Lyon. The row holds the word. A
            # paper that writes the glottal stop as 7 keeps the 7 in its rows. A row that holds skúza does not let
            # Skúza7 through here.
            marked = without_marker(token)
            if (marked != token) and (marked in written):
                continue
            # A form run together with its own parse, as Lyon's extraction leaves cáwt@t,√cáwt-tt.
            # The table holds the word and the analysis apart, as the paper sets them.
            at_root = surface_parse_join(token)
            if (at_root > 0) and all(
                (one in written)
                for one in (bare(token[:at_root]), bare(token[at_root:]))
                if is_language_token(one, marks)
            ):
                continue
            # A slashed token is two forms printed in one cell, `dᶻəlč̓/ǰəlč̓`, and the hand extraction gives each of
            # them its own row. A question for the whole string back makes a row per accident of print.
            if all(
                (one in written)
                for one in token.split("/")
                if is_language_token(one, marks)
            ):
                continue
            missed.append((number, token))
        out.write(
            "    %d language tokens in the paper that no row holds\n" % len(missed)
        )
        for number, token in missed:
            out.write("      line %-6d %s\n" % (number, token))

        # A paper whose text lost the orthography is not something to grade the table against. The code still prints the
        # counts above, because to hide them hides the size of the gap. But they are not disagreements: the reading is
        # right, and the file that the code compares it with is not the paper.
        if stem in ORTHOGRAPHY_ABSENT:
            out.write(
                "    not counted. This paper's text holds none of its orthography and it has "
                "no page text:\n    the two sides above are not comparable. See "
                "ORTHOGRAPHY_ABSENT in papers.py.\n"
            )
            continue

        failed += len(unfound) + len(missed)

    out.write(
        "\n  %d of %d papers have a hand extraction\n"
        % (len(EVERY) - len(waiting), len(EVERY))
    )
    if waiting:
        out.write("  still to be read by hand:\n")
        for stem in waiting:
            out.write("    %s\n" % stem)
    out.write(
        "\n  %s\n"
        % (
            "every hand extraction agrees with its paper"
            if not failed
            else "%d disagreements to work through" % failed
        )
    )
    out.flush()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
