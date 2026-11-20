#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Upper Nicola Okanagan of twelve narratives from ICSNL 48, from the drafted page text.
#
#   Usage:  python maint/data/salishan/corpus_script_extraction/extract_lindley_lyon.py
#
# This file serves one paper. Twelve stories, and unlike the others in this set, its subsections carry names: Okanagan,
# Interlinear gloss, Free translation, Commentary. Three of the twelve are three tellings of one story, and three more
# are three tellings of another, each with its own commentary. The section numbers do not line up with the story count,
# and a read of them by position misfiles the versions against each other. The reader therefore matches the subsections
# on their titles.
#
# This paper carries the same font substitution as the other Lyon volume. A test of the same table on this paper moves
# the tokens attested in Lyon's later papers on this language from 2 of 4332 to 965. `draft_page_text.py` writes the
# page text back into the orthography. This reader reads that text, and applies no substitution of its own: every repair
# call in this file returns its line unchanged.
#
# The appendix holds a note on the practice of transcription and gloss, and two pronominal paradigm tables. Those are
# the language, and they are not narrative. The file keeps them, and marks them derived.

import io
import os
import re
import subprocess
import sys

from page_text import (
    language_line,
    repaired,
    repaired_english,
    repaired_line,
    repaired_prose,
)
from salish_marking import (
    CAPS_RUN,
    DERIVED,
    MARKED,
    SPOKEN,
    TEXT_SPACE,
    UNCLASSIFIED,
    is_mixed,
    rendered,
    switches,
    tagged_spans,
    unligatured,
)
from salish_unsorted import UNKNOWN_KIND, covered_tokens, unreached, write_unsorted
from space_repair import joined_words, vocabulary_of, welded


def _repository_root():
    """This repository, asked of git and not inferred from a marker directory.

    A marker that the repository produces, such as build/, is absent from a linked worktree and from a clone with no
    build. A climb to it can pass this root and land in another checkout whose paths look valid. A marker infers the
    root. Git answers it. The climb below serves only an exported tree with no git directory. It looks for
    src/cu/engine, which the repository tracks and every checkout of it holds.

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

# The drafted page text instead of the extraction. The PDF of this paper passes back the font's own alphabet, and a
# corpus built on that is not the language. Much of what this reader would write from the text of the PDF is a string
# that the page does not print.
SOURCE = os.path.join(PAPERS, "2013_Lindley_Lyon.page.txt")

# <spoken by>_<original paper>_<who wrote it down>_Salish_<language without accents>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "LottieLindley_TwelveMoreUpperNicolaOkanaganNarratives_LindleyLyon"
    "_Salish_nsyilxcen_2013_nomixed.txt",
)

MARKS = MARKED + "ʷ̓’ʼ"

PAGE = re.compile(r"^===== page \d+ =====$")
HEADING = re.compile(r"^(\d{1,2}(?:\.\d+)*)\s+(\S.*)$")
DOTTED = re.compile(r"\.\s*\.\s*\.")
NUMBERED_BLOCK = re.compile(r"^\((\d{1,4})\)\s*(.*)$")
# The free translation closes a block, and this paper writes it inside single quotes. The double quote is deliberately
# absent. These are stories full of people who talk, and a transcription of quoted speech opens with one. A match on it
# would end the block at the first line that somebody speaks in, and send every entry after that into the leftovers.
QUOTED = re.compile(r"^['‘]")

# A segmentation line joins its morphemes with a hyphen or an equals sign, and no repair changes either character. A
# test for one keeps a wrapped transcription line out of the record as segmentation, where it would land only because
# nothing else matches it.
SEGMENTED = re.compile(r"[-=]")

# A page number survives inside a block, and would put the two-line cycle out of step by one for every word after it.
# This orthography writes nothing with digits alone.
PAGE_NUMBER = re.compile(r"^\d{1,4}$")

CATEGORIES = re.compile(
    r"\b(?:ABS|APPL|AUT|C1C2|C1|C2|CAUS|CHAR|CISL|CONJ|CUST|DEON|DEV|DIM|DIR|DRV|DUB|EMPH|"
    r"EPIS|EVID|INCEPT|INCH|INDEP|INTERJ|INT|LC|LOC|MID|OCC|RED|STAT|UPOSS|FUT|IMP|"
    r"DET|DEM|ERG|OBJ|POSS|PL|SG|SBJ|NOM|OBL|IPFV|NEG|TR|1SG|2SG|3SG|1PL|2PL|3PL)\b"
)


# What each named subsection holds. The match is on the title. The numbering does not line up with the story count once
# the count includes the multiple versions and their commentaries.
def type_of(title):
    """What a subsection holds, from its own heading."""
    lowered = title.lower()
    if lowered.startswith("okanagan"):
        return "running speech"
    if "interlinear" in lowered:
        return "interlinear"
    if "free translation" in lowered:
        return "free translation"
    if "commentary" in lowered:
        return "commentary"
    if "paradigm" in lowered or "transcription" in lowered or "abbreviation" in lowered:
        return "appendix"
    return None


LAYER = {
    "running speech": SPOKEN,
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "free translation": SPOKEN,
    "segmentation": DERIVED,
    "gloss": DERIVED,
    "commentary": DERIVED,
    "appendix": DERIVED,
    # Kept and marked, and out of the ingestion stream until someone classifies it.
    UNCLASSIFIED: DERIVED,
}


def carries_language(text):
    """Whether a line holds any character this paper writes the language with."""
    return any(mark in text for mark in MARKS)


def two_line_words(block):
    """One numbered block of the interlinear, as the two lines that this paper gives for each word.

    The PDF passed the interlinear over a column at a time. A word arrives as its form, and then its gloss on the next
    line, and the pair repeats until the free translation closes the block. Position fixes which line is which. Content
    cannot: iʔ and DET are both plain ASCII, and the form of a word with no affixes carries nothing that a gloss does
    not.

    The form line here is the segmented form. It writes sm-sámaʔ and c-’tʕap-nwíxw where the running text of the same
    story writes the surface. A sentence rebuilt from this column is the paper's analysis of the speech, and not a
    record of it.

    The function catches a slipped count, and does not repair it. The other Lyon paper uses the same test: an uppercase
    category label belongs to a gloss, and cannot appear in a form.

    Returns the words, the free translation, any line left over, and whether the cycle slipped.
    """
    words = []
    translation = None
    leftover = []
    holding = [None, None]
    slot = 0

    def close():
        if any(one is not None for one in holding):
            words.append(tuple(holding))
        holding[0] = holding[1] = None

    for line in block:
        if PAGE_NUMBER.match(line):
            continue
        if QUOTED.match(line):
            close()
            slot = 0
            translation = line
            continue
        if translation is not None:
            leftover.append(line)
            continue
        holding[slot] = line
        slot += 1
        if slot == 2:
            close()
            slot = 0
    close()
    # Tested on a run of capitals instead of on the label list. The list matches on word boundaries and
    # there is none inside 3POSS, and a form line reading father-3POSS passed it.
    slipped = any(CAPS_RUN.search(one[0] or "") for one in words)
    return words, translation, leftover, slipped


def looks_heading(trimmed):
    """A numbered heading, told apart from a contents entry and from a numbered example."""
    if NUMBERED_BLOCK.match(trimmed) or DOTTED.search(trimmed):
        return None
    found = HEADING.match(trimmed)
    if not found:
        return None
    if len(trimmed) > 78 or trimmed.endswith("."):
        return None
    return found.group(1), found.group(2).strip()


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
        # Ligatures come out here, before the loop below, so that everything that reads these lines sees the same text.
        # A later removal would leave the unreached check to compare preﬁxsən- against prefixsən-, and to report a hole
        # that is not one.
        lines = [unligatured(one.rstrip("\n")) for one in handle]

    rows = []
    blocks = []
    running_at = []
    stories = {}
    section = None
    story = None
    holds = None
    number = None
    opening = []

    for line in lines:
        trimmed = " ".join(line.split())
        if PAGE.match(trimmed) or not trimmed:
            continue
        if trimmed.startswith("References"):
            section = None
            holds = None
            continue

        opened = looks_heading(trimmed)
        if opened:
            section, title = opened
            story = section.split(".")[0]
            number = None
            if "." not in section:
                stories[story] = repaired_prose(title)
                holds = None
            else:
                holds = type_of(title)
            continue

        if section is None:
            if not stories:
                opening.append(trimmed)
            continue

        if holds is None:
            continue

        name = stories.get(story, "")

        if holds == "running speech":
            # A footnote or an acknowledgment inside a story section is English throughout. The test runs on the line as
            # it arrived, and a line that fails it goes into the record as unclassified.
            if not language_line(trimmed):
                rows.append(
                    ("N", 0, section, name, UNCLASSIFIED, repaired_english(trimmed))
                )
                continue
            # The Okanagan subsection is the language from end to end, and no token in it needs a guard. The interlinear
            # says where the extraction broke a word in two, and this loop has not read the interlinear yet. The row
            # therefore goes in now, to keep the paper's order. The loop remembers its place, and the code below puts
            # the words back together once the list exists.
            fixed = repaired(trimmed)
            if carries_language(fixed):
                running_at.append(len(rows))
                rows.append(("T", 0, section, name, "running speech", fixed))
            continue

        if holds in ("free translation", "commentary", "appendix"):
            # A free translation and a commentary are Lyon's own English. The appendix holds Lyon's pronominal
            # paradigms, which are the language. It takes repaired_line, the call for a line with no column to go on.
            fixed = (
                repaired_line(trimmed)
                if holds == "appendix"
                else repaired_english(trimmed)
            )
            mark = "T" if (holds == "appendix" and carries_language(fixed)) else "N"
            rows.append((mark, 0, section, name, holds, fixed))
            continue

        # The loop gathers a block whole, as it arrived, and reads it afterward. It cannot type the lines one at a time.
        # The position of a line in the block fixes its column, and the call that a line takes depends on that column.
        found = NUMBERED_BLOCK.match(trimmed)
        if found:
            number = int(found.group(1))
            rest = found.group(2).strip()
            blocks.append((section, name, number, [rest] if rest else []))
            continue
        if number is None:
            continue
        blocks[-1][3].append(trimmed)

    # Each block read back as words. The sentence is the form column joined in order. Its mark is segmentation, because
    # that column carries the morpheme boundaries. The surface of the same story is in the Okanagan subsection above,
    # and it reaches the ingestion stream.
    #
    # The code reads every block first, so that the list of true word forms exists before it writes any of them. An
    # entry is one word. The spaces inside it are the extraction's, and a weld of them gives the true look of the word.
    # Only blocks that read cleanly contribute: one bad entry in the list joins two real words together everywhere it
    # matches.
    slipped_blocks = 0
    vocabulary = set()
    parsed = []
    for section, name, number, block in blocks:
        read = two_line_words(block)
        parsed.append((section, name, number, block, read))
        if not read[3]:
            vocabulary |= vocabulary_of(
                repaired(welded(one[0])) for one in read[0] if one[0]
            )

    for section, name, number, block, read in parsed:
        words, translation, leftover, slipped = read
        if slipped:
            # The count slipped. Every column after that point is one line out, and no line in the block has a known
            # column. The code flags the block whole, and writes nothing under a guessed name.
            slipped_blocks += 1
            for one in block:
                # The column of each line is the unknown here. The call repaired_line serves a line with no column to go
                # on.
                fixed = repaired_line(one)
                rows.append(
                    (
                        "T" if carries_language(fixed) else "N",
                        number,
                        section,
                        name,
                        UNCLASSIFIED,
                        fixed,
                    )
                )
            continue
        built = " ".join(repaired(welded(one[0])) for one in words if one[0])
        if built:
            rows.append(("T", number, section, name, "segmentation", built))
        for one in words:
            if one[1]:
                rows.append(
                    ("N", number, section, name, "gloss", repaired_english(one[1]))
                )
        if translation:
            rows.append(
                (
                    "N",
                    number,
                    section,
                    name,
                    "translation",
                    repaired_english(translation),
                )
            )
        for one in leftover:
            # What sits after the free translation is its wrapped tail, a footnote, or Lyon's prose about a form. The
            # English test of the other branches deliberately does not run here. Lyon cites words of the language inside
            # English sentences, and a drop of those lines costs coverage for no gain.
            fixed = repaired_line(one)
            if carries_language(fixed):
                rows.append(("T", number, section, name, UNCLASSIFIED, fixed))

    # The running text, with the words that the extraction broke put back together. The loop above wrote its rows in the
    # paper's order. The list that says where the breaks are exists only now, after the read of the interlinear.
    for at in running_at:
        row = rows[at]
        rows[at] = row[:5] + (joined_words(row[5], vocabulary),)

    # The code writes the list out beside the record. The coverage check must put the source through the same join, and
    # a second list built from the paper would not be the same one.
    words_at = TARGET[:-4] + ".words.txt"
    with open(words_at, "w", encoding="utf-8", newline="") as handle:
        handle.write(
            "# The word forms this paper's interlinear gives, one to a line, with the\n"
        )
        handle.write(
            "# spaces the extraction put inside them taken out. Every entry comes from a\n"
        )
        handle.write("# block whose two-line cycle read cleanly.\n")
        for one in sorted(vocabulary):
            handle.write("%s\n" % one)

    def prepared(text):
        """One line through everything that this reader does to a line before it records it."""
        return joined_words(repaired_line(text), vocabulary)

    # Every line of the paper that no subsection reached, added to the record as unclassified. The marked file holds
    # every token of the language that the paper printed. They stay out of the pure stream, and the flag file lists them
    # for someone to work through. The union of every orthography, and not this paper's own set. The coverage check
    # counts a token against the union. A finder with a narrower set leaves holes that the check still reports.
    missed = unreached(lines, covered_tokens(one[5] for one in rows), repair=prepared)
    for page, where, reason, missing, text in missed:
        rows.append(("T", 0, "not reached", "page %d" % page, UNCLASSIFIED, text))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# 12 more Upper Nicola Okanagan narratives.\n")
        handle.write("# Nsyilxcən, Upper Nicola. Lottie Lindley and John Lyon.\n")
        handle.write(
            "# Papers for the International Conference on Salish and Neighboring\n"
        )
        handle.write("# Languages, UBCWPL, 2013.\n")
        handle.write(
            "# Three of the twelve are three tellings of one story and three more are\n"
        )
        handle.write("# three tellings of another, each with its own commentary.\n")
        handle.write("#\n")
        handle.write(
            "# READ FROM THE PAGE. This paper's PDF passes back the font's own alphabet\n"
        )
        handle.write(
            "# and not what the page prints. This reader takes build/papers/\n"
        )
        handle.write(
            "# 2013_Lindley_Lyon.page.txt, which draft_page_text.py writes in the\n"
        )
        handle.write("# orthography, and applies no substitution of its own.\n")
        handle.write("#\n")
        handle.write(
            "# That text is a draft. Two things in it are settled only by the rendered\n"
        )
        handle.write(
            "# page and are still open here: which w is a labialized consonant, and which\n"
        )
        handle.write(
            "# inserted space is a word boundary. The hand extraction beside this paper\n"
        )
        handle.write("# was read off the pages and says so.\n")
        handle.write("#\n")
        handle.write(
            "# Mark is language.layer.type. T is Nsyilxcən, N is anything else.\n"
        )
        handle.write(
            "# Subsections are matched on their titles, not their numbers, because the\n"
        )
        handle.write(
            "# numbering does not line up with the story count once the versions and\n"
        )
        handle.write("# their commentaries are counted.\n")
        handle.write("line\tsection\tstory\ttype\tswitches\tcontent\n")
        for mark, count, sect, name, type, text in rows:
            layer = LAYER[type]
            if mark == "T":
                content = rendered(text, layer, type, MARKS)
                crossings = switches(text)
            else:
                content = "N.%s.%s:{%s}" % (layer, type, text)
                crossings = 0
            handle.write(
                "line#${%d}\t%s\t%s\t%s\t%d\t%s\n"
                % (count, sect, name[:40], type, crossings, content)
            )

    pure = TARGET[:-4] + ".pure.txt"
    retained = 0
    repeated = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, count, sect, name, type, text in rows:
            if (mark != "T") or (LAYER[type] != SPOKEN):
                continue
            for span, run in tagged_spans(text, MARKS):
                if (span != "T") or (not run.strip()):
                    continue
                key = " ".join(run.split())
                if key in already:
                    repeated += 1
                    continue
                already.add(key)
                handle.write("%s\n" % key)
                retained += 1

    # A file of its own for what the tool could not sort. That is an interlinear line that none of the tests typed, and
    # a line that no subsection reached. The comparison above puts the source through `prepared` first, so that a word
    # that the record holds does not come back as a hole.
    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [
        (0, "%s block %d" % (sect, count), UNKNOWN_KIND, "", text)
        for mark, count, sect, name, type, text in rows
        if (type == UNCLASSIFIED) and (sect != "not reached")
    ]
    flagged.extend(missed)
    stuck_count = write_unsorted(
        stuck, "12 more Upper Nicola Okanagan narratives", flagged
    )

    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write(
        "  %d target-language spans written to\n  %s\n" % (retained, os.path.basename(pure))
    )
    out.write("  %d spans skipped as already written\n" % repeated)
    out.write(
        "  %d lines the tool could not sort written to\n  %s\n"
        % (stuck_count, os.path.basename(stuck))
    )
    out.write(
        "  %d of %d interlinear blocks read cleanly, %d slipped and were flagged whole\n"
        % (len(blocks) - slipped_blocks, len(blocks), slipped_blocks)
    )

    # Two counts per story, because they come from the two printings of it. The running speech is
    # the surface and is what reaches the ingestion stream. The rebuilt blocks are the interlinear.
    out.write("\n  %-40s %-16s %s\n" % ("story", "running speech", "rebuilt blocks"))
    spoken = {}
    built = {}
    for mark, count, sect, name, type, text in rows:
        if type == "running speech":
            spoken[name] = spoken.get(name, 0) + 1
        elif type == "segmentation":
            built[name] = built.get(name, 0) + 1
    for name in sorted(set(spoken) | set(built)):
        out.write(
            "  %-40s %-16d %d\n" % (name[:40], spoken.get(name, 0), built.get(name, 0))
        )

    types = {}
    for mark, count, sect, name, type, text in rows:
        types[type] = types.get(type, 0) + 1
    out.write(
        "\n  by type: %s\n"
        % ", ".join("%s %d" % (one, types[one]) for one in sorted(types))
    )
    out.write("  %d stories found\n" % len(stories))
    if opening:
        out.write(
            "\n  the paper's opening, which names its speaker\n    %s\n"
            % " ".join(opening)[:220]
        )

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
