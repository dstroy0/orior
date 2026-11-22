#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Okanagan of three speakers from ICSNL 50, with a repair of the font substitution first.
#
#   Usage:  python maint/data/salishan/corpus_script_extraction/extract_lyon_priests.py
#
# This file serves one paper, and that paper is the only one whose characters need a repair before a reader can use them
# at all. Its font wrote plain characters in place of the orthography. The text arrives as `iP naPì ʼqwQaylqs` where it
# should read `iʔ naʔɬ ʼqwʕaylqs`.
#
# A test proves the mapping, and nothing assumes it. `font_substitution.py` applies a candidate table to the damaged
# tokens. It counts how many become forms attested in the recent papers of Lyon on the same language, whose extraction
# kept its characters. Before the mapping, 1 token of 3599 matches an attested form, and after it 811 do. The same table
# on the other damaged Lyon paper moves 2 of 4332 to 965. A wrong mapping cannot do that, because a wrong substitution
# produces strings that the language does not contain.
#
# The table records with it what it does not cover. This font writes each caron entry as a separate character before its
# character. X̌ arrives as ˇx, and the order of replacement matters.
#
# There are three speakers, and the paper names the permissions:
#   - George Lezard of the Penticton Indian Reserve told Conversation with the priest in 1966, at the age of
#     eighty-five. Randy Bouchard recorded it, and Larry Pierre transcribed it in 1970. Lyon updated that transcription
#     with the permission of Arnie Baptiste, Larry Pierre's son.
#   - Smokey and the priest is the story of Nellie. The paper reprints it with the permission of Lynne Jorgesen of the
#     Upper Nicola Indian Band, Nellie's great-granddaughter.
#   - the introduction of the third story names its teller. This file reports that name, and the attribution then comes
#     from the paper.

import io
import os
import re
import subprocess
import sys

from page_text import (
    carries_orthography,
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
    UNCLASSIFIED,
    is_mixed,
    rendered,
    switches,
    tagged_spans,
    unligatured,
)
from salish_unsorted import UNKNOWN_TYPE, covered_tokens, unreached, write_unsorted
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
# corpus built on that is not the language. `draft_page_text.py` writes it back into the orthography, and this reader
# applies no substitution of its own.
SOURCE = os.path.join(PAPERS, "19-Lyon_ICSNL50_final-78.page.txt")

# <spoken by>_<original paper>_<who wrote it down>_Salish_<language without accents>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "GeorgeLezard-NellieGuitterez-AndrewMcGinnis_ThreeOkanaganStoriesAboutPriests_Lyon"
    "_Salish_nsyilxcen_2015_nomixed.txt",
)

MARKS = MARKED + "ʷ̓’ʼ"

PAGE = re.compile(r"^===== page \d+ =====$")
HEADING = re.compile(r"^(\d(?:\.\d)?)\s+(\S.*)$")
DOTTED = re.compile(r"\.\s*\.\s*\.")
NUMBERED_BLOCK = re.compile(r"^\((\d{1,4})\)\s*(.*)$")
# The free translation closes a block, and this paper writes it inside single quotes. The pattern does not accept the
# double quote. These are stories full of speech, and a transcription of quoted speech opens with one. A match on it
# ends the block at the first line that somebody speaks in. Every entry after that line then goes into the leftovers.
QUOTED = re.compile(r"^['‘]")

# A segmentation line joins its morphemes with a hyphen or an equals sign, and the font repair below changes neither
# character. A test for one keeps a wrapped transcription line or a page number out of the record as segmentation, where
# nothing else matched it.
SEGMENTED = re.compile(r"[-=]")

# The extraction ran the first two columns of a word together wherever the segmentation opens at the root, and gives
# `’qwQaylqs√ ’qwQay=lqs` on one line. A segmentation of its own can also hold √, as `in-ks-√ ’ma ’y-ìt-ím` does. What
# is before the root tells the two apart. A merged line has a bare word there, and a segmentation has morpheme
# separators. This paper separates with + as well as with a hyphen or an equals sign, as `s+√na ’qw` and `c+n+√ʔuɬxw-s`
# do. The class holds + with them, and without it the front of a segmentation reads as a word of the story.
MERGED = re.compile(r"^([^-=•+√]+?)\s*(√.*)$")

# A page number survives inside a block, and it would put the four-line cycle out of step by one for every word after
# it. This orthography writes nothing with digits alone.
PAGE_NUMBER = re.compile(r"^\d{1,4}$")

CATEGORIES = re.compile(
    r"\b(?:ABS|APPL|AUT|C1C2|C1|C2|CAUS|CHAR|CISL|CONJ|CUST|DEON|DEV|DIM|DIR|DRV|DUB|EMPH|"
    r"EPIS|EVID|INCEPT|INCH|INDEP|INTERJ|INT|LC|LOC|MID|OCC|RED|STAT|UPOSS|"
    r"DET|DEM|ERG|OBJ|POSS|PL|SG|SBJ|NOM|OBL|IPFV|NEG|TR|1SG|2SG|3SG|1PL|2PL|3PL)\b"
)

# Taken from each story's own introduction, which names its teller and the tape
SPEAKER = {
    "1": "George Lezard, Penticton Indian Reserve, told 1966",
    "2": "Nellie Guitterez, Upper Nicola Indian Band, told 1978 or 1979",
    "3": "Kiláwnaʔ (Andrew McGinnis), Penticton Indian Reserve, told 9 October 2014",
}

LAYER = {
    "running speech": SPOKEN,
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "segmentation": DERIVED,
    "gloss": DERIVED,
    "word gloss": DERIVED,
    # Retained and marked, excluded from the ingestion stream until someone classified it.
    UNCLASSIFIED: DERIVED,
}


def carries_language(text):
    """Whether a line holds any character this paper writes the language with."""
    return any(mark in text for mark in MARKS)


def type_by_notation(text):
    """What a line is, from the notation that this paper defines, where its position is unavailable.

    Only a block whose cycle slipped uses this. Position is the better evidence, and here it is absent. The rest is what
    Lyon states about the notation. A run of two or more capitals is a gloss label, and √ marks a root. A
    segmentation carries a root mark, and a spoken word does not.

    This function decides nothing else. A line with neither is a word as spoken, or the segmentation of a word with no
    affixes to mark. In this language those are often the same string: block 1 opens with p and then p again. A name for
    one of them would invent the answer.
    """
    if CAPS_RUN.search(text):
        return "gloss"
    if "√" in text:
        return "segmentation"
    return UNCLASSIFIED


def split_merged(text):
    """Put back the space between two columns that the extraction ran together.

    This applies where the reader cannot read a block by position, and keeps its lines as they arrived. The line still
    holds `ʔacəcqáʔ√ʔac•c•qáʔ-wi` as one token there. A reader who looks for the word `ʔacəcqáʔ` in the record does not
    find it inside that. The rule is the same one that four_line_words uses.
    """
    out = []
    for token in text.split():
        found = MERGED.match(token)
        if found:
            out.append(found.group(1))
            out.append(found.group(2))
        else:
            out.append(token)
    return " ".join(out)


def four_line_words(block):
    """One numbered block of the interlinear, as the four lines that the paper gives for each word.

    The PDF passed the interlinear of this paper over a column at a time. A word arrives as four consecutive lines: the
    word as spoken, its segmentation, its gloss, and an English word for it. The position in that cycle fixes which line
    is which. Content cannot, because a short word in plain characters comes out the same on the first two lines, and
    carries nothing to test. Block 1 opens with p, then p again, and only the order separates the two.

    Two things break the cycle, and the function handles both. Where the extraction ran the first two columns together,
    the line fills both positions at once. The function passes over a page number inside a block, because a count of it
    puts every word after it out of step.

    A third breaks it, and the function cannot handle it. A word with no English gloss takes three lines instead of
    four, and block 44 opens with one. Nothing in the lines themselves marks it, and every word after it lands a
    position early. The function catches that and does not repair it. An uppercase category label belongs to a gloss,
    and cannot occur in a spoken word or its segmentation. One in either of those positions proves that the count
    slipped, and the function tells the caller.

    Returns the words, the free translation that closes the block, any line left over, and whether the cycle slipped.
    """
    words = []
    translation = None
    leftover = []
    slot = 0
    holding = [None, None, None, None]

    def close():
        if any(one is not None for one in holding):
            words.append(tuple(holding))
        holding[0] = holding[1] = holding[2] = holding[3] = None

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
        # The fourth line is an English word for the word above it, and the paper does not always give one. Block 44
        # opens with `ixí P / ixíP / DEM` and no fourth line. Where the fourth line is absent, the first line of the
        # next word is in that place. A read of it as a gloss puts every word after it a position early.
        #
        # A word gloss is English and carries none of the damaged orthography. That separates `you.all` and `priest`
        # from `yaQyá;;Qt` and `t@twít`. A single-character line does not close a block early, though l, p and t are
        # proclitics and never an English gloss. A single character is in that position often enough to matter.
        if (slot == 3) and any(carries_orthography(one) for one in line.split()):
            close()
            slot = 0
        if slot == 0:
            found = MERGED.match(line)
            if found:
                holding[0] = found.group(1)
                holding[1] = found.group(2)
                slot = 2
                continue
        holding[slot] = line
        slot += 1
        if slot == 4:
            close()
            slot = 0
    close()
    # Tested on a run of capitals instead of on the label list. The list matches on word boundaries and
    # there is none inside 3POSS. A block whose gloss line read father-3POSS passed the check and
    # put that into the ingestion stream as something somebody said.
    slipped = any(
        CAPS_RUN.search(one[0] or "") or CAPS_RUN.search(one[1] or "") for one in words
    )
    return words, translation, leftover, slipped


def looks_heading(trimmed):
    """A numbered heading, told apart from the contents listing and from a numbered example."""
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
        # Ligatures come out here, before the loop below. That everything reading these lines
        # sees the same text. This paper holds 76 of them and one reached the corpus as ﬁve.
        lines = [unligatured(one.rstrip("\n")) for one in handle]

    rows = []
    blocks = []
    running_at = []
    intros = {}
    section = None
    story = None
    number = None
    seen_heading = set()

    for line in lines:
        trimmed = " ".join(line.split())
        if PAGE.match(trimmed) or not trimmed:
            continue

        opened = looks_heading(trimmed)
        if opened:
            number_of, title = opened
            # The contents list repeats every heading before the body. A top-level heading opens its section on its
            # second appearance. Dot leaders pad the subsection entries in that list, and those entries already error
            # above. They never register as seen, and a skip of their first appearance would discard every one of them.
            if "." not in number_of:
                if number_of not in seen_heading:
                    seen_heading.add(number_of)
                    continue
            section = number_of
            story = number_of.split(".")[0]
            number = None
            continue

        if section is None:
            continue

        who = SPEAKER.get(story, "")

        if "." not in section:
            # Prose that introduces a story, which is where the paper names its teller
            intros.setdefault(section, []).append(trimmed)
            continue

        if section.endswith(".1"):
            # A footnote or an acknowledgment printed inside a story section is English throughout. The repair hides
            # that, because it puts a glottal stop where the capital P was. The test therefore runs on the line as it
            # arrived. A line of these stories always holds a word with an accented vowel or a glottalization mark in
            # it, and English prose does not.
            if not language_line(trimmed):
                rows.append(
                    ("N", 0, section, UNCLASSIFIED, who, repaired_english(trimmed))
                )
                continue
            # The running text of a story is the language from end to end. Every substitution applies to it, and no
            # token in it needs a guard. The reader does not know yet where the extraction broke a word in two. The
            # interlinear says that, and the reader reads it later. The reader therefore writes the row now to keep the
            # order of the paper, and keeps its place. It puts the words back together below, once the list exists.
            fixed = repaired(trimmed)
            if carries_language(fixed):
                running_at.append(len(rows))
                rows.append(("T", 0, section, "running speech", who, fixed))
            continue

        # The reader gathers a block whole and unrepaired, and reads it afterward. It cannot type the lines one at a
        # time. The position of a line in the block fixes its column, and the column decides which repair the line may
        # take.
        found = NUMBERED_BLOCK.match(trimmed)
        if found:
            number = int(found.group(1))
            rest = found.group(2).strip()
            blocks.append((section, number, who, [rest] if rest else []))
            continue
        if number is None:
            continue
        blocks[-1][3].append(trimmed)

    # Each block read back as words. The sentence is the first column joined in order. It is the only part of a block
    # that a person spoke, and the other three columns are Lyon's analysis of it. The reader reads every block first,
    # and the list of true word forms exists before the reader writes any of them.
    #
    # An entry is one word. The spaces inside it are from the extraction, and a weld of them gives what the word really
    # looks like. Only blocks that read cleanly contribute. An entry from a block whose cycle slipped is not a word.
    # One bad entry in the list joins two real words together everywhere it matches.
    slipped_blocks = 0
    vocabulary = set()
    parsed = []
    for section, number, who, block in blocks:
        read = four_line_words(block)
        parsed.append((section, number, who, block, read))
        if not read[3]:
            vocabulary |= vocabulary_of(
                repaired(welded(one[slot]))
                for one in read[0]
                for slot in (0, 1)
                if one[slot]
            )

    for section, number, who, block, read in parsed:
        words, translation, leftover, slipped = read
        if slipped:
            # The count slipped somewhere in this block. Every column after that point is one line out, and the reader
            # can name nothing in it. The reader flags the whole block. If it emitted the sentence anyway, gloss text
            # such as `know+INCH -manage.to-DIR-3ERG` would go into the ingestion stream as though someone said it.
            slipped_blocks += 1
            for one in block:
                # Which column each line belongs to is the unknown here. The reader chooses the repair from what the
                # line itself turns out to be.
                fixed = repaired_line(split_merged(one))
                type = type_by_notation(fixed)
                rows.append(
                    ("N" if type == "gloss" else "T", number, section, type, who, fixed)
                )
            continue
        said = " ".join(repaired(welded(one[0])) for one in words if one[0])
        if said:
            rows.append(("T", number, section, "transcription", who, said))
        for one in words:
            if one[1]:
                rows.append(
                    (
                        "T",
                        number,
                        section,
                        "segmentation",
                        who,
                        repaired(welded(one[1])),
                    )
                )
            if one[2]:
                rows.append(
                    ("N", number, section, "gloss", who, repaired_english(one[2]))
                )
            if one[3]:
                rows.append(
                    ("N", number, section, "word gloss", who, repaired_english(one[3]))
                )
        if translation:
            rows.append(
                (
                    "N",
                    number,
                    section,
                    "translation",
                    who,
                    repaired_english(translation),
                )
            )
        for one in leftover:
            # What sits after the free translation is a footnote, a page artifact, or prose of Lyon about a form. The
            # English test of the other branches does not apply here. Lyon cites words of the language inside English
            # sentences, and a drop of those lines costs this paper six points of coverage with no gain.
            fixed = joined_words(repaired_line(split_merged(one)), vocabulary)
            if carries_language(fixed):
                rows.append(("T", number, section, UNCLASSIFIED, who, fixed))

    # The running text, with the words that the extraction broke put back together. The reader wrote its rows above to
    # keep the order of the paper. The list that says where the breaks are only exists after the read of the
    # interlinear.
    for at in running_at:
        row = rows[at]
        rows[at] = row[:5] + (joined_words(row[5], vocabulary),)

    # The reader writes the list out beside the record. The coverage check must put the source through the same join
    # before its comparison. A copy of the list that the check builds from the paper differs, and joins words wrongly.
    # The check reads this file instead.
    words_at = TARGET[:-4] + ".words.txt"
    with open(words_at, "w", encoding="utf-8", newline="") as handle:
        handle.write(
            "# The word forms this paper's interlinear gives, one to a line, with the\n"
        )
        handle.write(
            "# spaces the extraction put inside them taken out. Every entry comes from a\n"
        )
        handle.write("# block whose four-line cycle read cleanly.\n")
        for one in sorted(vocabulary):
            handle.write("%s\n" % one)

    def prepared(text):
        """One line put through everything that this reader does before it records the line.

        The work on the source uses this, to find which lines never reached the record. A line outside every section
        still passes all three steps on its way in. A comparison against a source that had only some of them reports a
        hole that is not there.
        """
        return joined_words(repaired_line(split_merged(text)), vocabulary)

    # Every line of the paper that no section reached, added to the record as unclassified. The marked file holds every
    # token of the language that the paper printed. That is the front matter, the prose that introduces each story, and
    # Lyon's discussion. They stay out of the pure stream.
    #
    # The union of every orthography, not this paper's own set. The coverage check counts a token against the union. A
    # finder with a narrower set leaves holes that the check still reports.
    missed = unreached(lines, covered_tokens(one[5] for one in rows), repair=prepared)
    for page, where, reason, missing, text in missed:
        rows.append(("T", 0, "not reached page %d" % page, UNCLASSIFIED, "", text))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write(
            "# Three Okanagan stories about priests. John Lyon, Simon Fraser University.\n"
        )
        handle.write(
            "# Okanagan, also called Nsyílxcən, Colville-Okanagan and Nqílxwcən, a\n"
        )
        handle.write(
            "# southern Interior Salish language. Three different fluent speakers.\n"
        )
        handle.write(
            "# Papers for the International Conference on Salish and Neighboring\n"
        )
        handle.write("# Languages 50, UBCWPL 40, 2015.\n")
        handle.write(
            "# Conversation with the priest: George Lezard, Penticton Indian Reserve,\n"
        )
        handle.write(
            "# told 1966 at eighty-five, recorded by Randy Bouchard, transcribed by Larry\n"
        )
        handle.write(
            "# Pierre 1970, updated by permission of Arnie Baptiste, his son.\n"
        )
        handle.write(
            "# Smokey and the priest: Nellie, reprinted by permission of her\n"
        )
        handle.write(
            "# great-granddaughter Lynne Jorgesen, Upper Nicola Indian Band.\n"
        )
        handle.write("#\n")
        handle.write(
            "# READ FROM THE PAGE. This paper's PDF passes back the font's own alphabet\n"
        )
        handle.write(
            "# and not what the page prints. This reader takes build/papers/\n"
        )
        handle.write(
            "# 19-Lyon_ICSNL50_final-78.page.txt, which draft_page_text.py writes in the\n"
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
        handle.write("# was read off the pages and settles both.\n")
        handle.write("#\n")
        handle.write(
            "# Mark is language.layer.type. T is Okanagan, N is anything else.\n"
        )
        handle.write(
            "# Gloss categories are the paper's own, from its first footnote, unchanged.\n"
        )
        handle.write("line\tsection\ttype\tspeaker\tswitches\tcontent\n")
        for mark, count, sect, type, who, text in rows:
            layer = LAYER[type]
            if mark == "T":
                content = rendered(text, layer, type, MARKS)
                crossings = switches(text)
            else:
                content = "N.%s.%s:{%s}" % (layer, type, text)
                crossings = 0
            handle.write(
                "line#${%d}\t%s\t%s\t%s\t%d\t%s\n"
                % (count, sect, type, who, crossings, content)
            )

    pure = TARGET[:-4] + ".pure.txt"
    retained = 0
    repeated = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, count, sect, type, who, text in rows:
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

    # A file of its own for what the tool could not sort. Each is a line inside a story that none of the tests typed, or
    # a line that no section reached. Here that is the front matter and the prose that introduces each story. The tool
    # puts the source through the same font repair before the comparison. It does not report a word that the repair
    # corrects. It does report what that repair does to English.
    #
    # The repair turns Pierre into ʔierre and Quilchena into ʕuilchena, and those arrive here as unreached.
    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [
        (0, "%s block %d" % (sect, count), UNKNOWN_TYPE, "", text)
        for mark, count, sect, type, who, text in rows
        if (type == UNCLASSIFIED) and not sect.startswith("not reached")
    ]
    flagged.extend(missed)
    stuck_count = write_unsorted(stuck, "Three Okanagan stories about priests", flagged)

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
        "  %d of %d interlinear blocks read cleanly, %d had a word given no English\n"
        "  gloss and were flagged whole\n"
        % (len(blocks) - slipped_blocks, len(blocks), slipped_blocks)
    )

    counted = {}
    for mark, count, sect, type, who, text in rows:
        counted[(sect, type)] = counted.get((sect, type), 0) + 1
    out.write("\n  %-8s %-18s %s\n" % ("section", "type", "lines"))
    for key in sorted(counted):
        out.write("  %-8s %-18s %d\n" % (key[0], key[1], counted[key]))

    out.write(
        "\n  the opening prose of each story, which is where the teller is named\n"
    )
    for section in sorted(intros):
        text = repaired_english(" ".join(intros[section]))
        out.write("    section %s: %s\n" % (section, text[:190]))

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
