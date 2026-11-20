#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Grade a reader against the hand extraction that it must reproduce.
#
#   Usage:  python maint/data/salishan/hand_extraction/reader_check.py
#
# The file coverage_check.py asks whether every token of the language got out of the paper. That is the easier question.
# A reader can answer it perfectly, and still be wrong about everything that matters. It can file a rejected tableau
# candidate as a word, or put a Squamish cognate in a Lushootseed corpus. It can read the two columns of Table 8 as one
# dialect. Every token would still have its place in the count.
#
# This asks the harder question. The hand extraction says what each form is. The check grades the reader against it form
# by form, on three things. They are whether it found the form at all, whether it says the same type, and whether it
# says the same dialect.
#
# The one that it must not fail is type, because type decides the pure corpus. A candidate filed as a citation puts a
# form that the paper rejects into the corpus, and nothing downstream ever asks again.

import io
import os
import re
import subprocess
import sys
import unicodedata


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
CORPORA = os.path.join(ROOT, "build", "corpora")
HERE = os.path.dirname(os.path.abspath(__file__))

# The hand extractions live in the research body, beside the prose that cites them. They are the
# speakers' words written down and they are evidence instead of tooling.
ORACLES = os.path.join(ROOT, "examples", "Salishan", "oracles")

sys.path.insert(0, HERE)

from papers import EVERY  # noqa: E402

# What the hand extraction records, but a reader need not reproduce. A person names a segment cited on its own, a
# constraint name, a template and a bibliography entry in a read of a paper. The record then misses nothing. None of
# them is a form to extract, and the record holds their lines as prose.
NOT_ASKED = (
    "segment",
    "notation",
    "mention",
    "reference",
    "damage",
    "cluster",
    # An English word that a paper's damaged mark set reads as a form, and a line of the next paper bound into the same
    # PDF. The record writes both down, so that the count of what a paper holds is honest. Neither is something to
    # extract.
    "english",
    "foreign",
)

SOUTHERN = "southern"
NORTHERN = "northern"

# Types that make the same claim about a form under two names. A paper that prints its story as unnumbered prose gets
# running speech. A paper that prints it as numbered lines gets transcription. Both say that the speaker said this in
# the target language, and the pure stream takes both.
SAME = {"running speech": "transcription", "transcription": "transcription"}


def normalized(text):
    return unicodedata.normalize("NFC", " ".join(text.split()))


def oracle_rows(path):
    held = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if (len(fields) < 4) or (fields[0] == "where"):
                continue
            held.append((fields[0], fields[1], fields[2], normalized(fields[3])))
    return held


# One span of a record's content column, written by salish_marking as language.layer.type:{text}. A line that the
# speaker mixed carries several of these, joined with a comma. The check reads a row as spans, and not as one string.
#
# The type may hold a space, and nine of the eleven readers write one that does. Those types are symbol note, running
# speech, stage direction, word gloss, free translation, cited example, morpheme entry, speaker comment and orthography
# chart. A pattern that excludes whitespace would hide every one of those spans from this check. The check would then
# report the forms in them as forms that the reader never wrote.
SPAN = re.compile(r"([TN])((?:\.[^:{}]*)*):\{")


def spans_of(content):
    """Every span of one content column, as its mark, its type, and the text inside its braces."""
    held = []
    for found in SPAN.finditer(content):
        at = found.end()
        depth = 1
        while (at < len(content)) and depth:
            if content[at] == "{":
                depth += 1
            elif content[at] == "}":
                depth -= 1
            at += 1
        parts = [one for one in found.group(2).split(".") if one]
        held.append(
            (found.group(1), parts[-1] if parts else "", content[found.end() : at - 1])
        )
    return held


def record_rows(path):
    """Every item that a reader wrote, as where, who, type, form.

    The type comes out of the span marker. The eleven records do not share a column layout. Three of them have no type
    column at all, and the rest disagree on where it sits. Every one of them writes the marker, because salish_marking
    writes it.

    The function reads the speaker or dialect from a column, where the record has one under that name. Several records
    do not have one, and their papers never establish who speaks.
    """
    held = []
    columns = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if not columns:
                columns = fields
                continue
            where = fields[1] if len(fields) > 1 else ""
            who = ""
            for name in ("speaker", "dialect"):
                if (name in columns) and (columns.index(name) < len(fields)):
                    who = fields[columns.index(name)]
                    break
            for mark, type, text in spans_of(fields[-1]):
                held.append((where, who, type, normalized(text)))
    return held, any((one in columns) for one in ("speaker", "dialect"))


def main():
    out = io.TextIOWrapper(
        sys.stdout.buffer, encoding="utf-8", errors="replace", newline=""
    )
    failed = 0
    waiting = []
    # A paper with a hand extraction and no reader yet. The check does not grade it either. A count of it as graded
    # would report every paper as graded while one has a table and no reader. The two lists stay apart, because the
    # action for each is different.
    unwritten = []
    for name, stem, record, repair, marks, _line_joins in EVERY:
        table = os.path.join(ORACLES, name)
        written = os.path.join(CORPORA, record)
        if not os.path.isfile(table):
            waiting.append(stem)
            continue
        if not os.path.isfile(written):
            out.write("  no record on disk for %s, run its reader first\n" % stem)
            unwritten.append(stem)
            failed += 1
            continue

        every = oracle_rows(table)
        wanted = [one for one in every if one[2] not in NOT_ASKED]
        got, names_who = record_rows(written)

        # What the paper holds at all, from every row of the hand extraction. A segment or a constraint name is not a
        # form for the reader to find. A reader that finds one does not invent something either.
        asked = {one[3] for one in every}
        dialects = {}
        for where, dialect, type, form in every:
            dialects.setdefault((type, form), set()).add(dialect)

        out.write("  %s\n" % name)
        out.write(
            "    %d forms asked for, %d items the reader wrote\n"
            % (len(wanted), len(got))
        )

        by_form = {}
        for where, dialect, type, form in got:
            by_form.setdefault(form, []).append((where, dialect, type))

        # What the reader said that it could not sort. A form that it flagged is a different thing from a form that it
        # got wrong and did not notice. The first is the reader at its job, and the second is the defect that this check
        # exists to find.
        flagged = set()
        stuck = written[:-4] + ".unclassifiable.tsv"
        if os.path.isfile(stuck):
            with open(stuck, encoding="utf-8") as handle:
                columns = []
                for line in handle:
                    fields = line.rstrip("\n").split("\t")
                    if not columns:
                        columns = fields
                        continue
                    # Read by column name. A read by position would break as soon as a paper column appears at the front
                    # of the flag file. Every flagged form would then count as one that the reader silently missed.
                    if ("where" in columns) and (columns.index("where") < len(fields)):
                        flagged.add(fields[columns.index("where")])

        missing = []
        said_so = []
        wrong_type = []
        wrong_dialect = []
        undecided = []
        for where, dialect, type, form in wanted:
            found = by_form.get(form)
            if not found:
                (said_so if (where in flagged) else missing).append((where, type, form))
                continue
            wanted_type = SAME.get(type, type)
            if not any(SAME.get(one[2], one[2]) == wanted_type for one in found):
                wrong_type.append(
                    (where, type, form, sorted({one[2] for one in found}))
                )
                continue
            said = sorted(
                {one[1] for one in found if SAME.get(one[2], one[2]) == wanted_type}
            )
            # Nobody asked a record with no speaker or dialect column who spoke. A grade on an answer that its paper's
            # reader does not give asks a question of the wrong file. The hand extraction still records it.
            if not names_who:
                continue
            if dialect in ("", "unstated", "none"):
                continue
            # The paper prints `čalaš ‘hand’` as Twana and as Squamish in one sentence. Either answer is the paper's
            # answer, and a demand for one of them would be a demand for a coin toss.
            if dialects.get((type, form), set()) & set(said):
                continue
            if dialect in said:
                continue
            # A form the paper states is the same in both dialects is right under either name and
            # right under neither. Table 1 and Table 6 both print ɬúkʷaɬ, and §4.1 says so.
            if (dialect == "both") and (said and set(said) <= {SOUTHERN, NORTHERN, ""}):
                continue
            # A claim of nothing is not the same as a claim of the wrong thing. The reader declines a dialect wherever
            # the passage names both of them. The check counts that, and does not grade it as an error.
            if said == [""]:
                undecided.append((where, type, form, dialect))
                continue
            wrong_dialect.append((where, type, form, dialect, said))

        # Prose that the reader kept whole is not a form that it claims to extract. Neither is a line of somebody else's
        # paper bound into the same PDF.
        ignored = NOT_ASKED + (
            "essay",
            "gloss",
            "note",
            "commentary",
            "free translation",
        )
        # A reader item that opens with a wanted form and runs past it is that form, taken too far. The check reports it
        # once, where the reader flagged or missed it. A second count here, as something that the reader made up, would
        # turn one defect into two.
        overrun = sorted(asked, key=len, reverse=True)
        extra = sorted(
            {
                (type, form)
                for where, dialect, type, form in got
                if (form not in asked)
                and (type not in ignored)
                and not any(form.startswith(one) for one in overrun if len(one) > 12)
            }
        )

        out.write("    %d forms the reader did not find\n" % len(missing))
        for where, type, form in missing:
            out.write("      %-12s %-11s %s\n" % (where, type, form))
        out.write(
            "    %d forms the reader could not bound and flagged as such\n"
            % len(said_so)
        )
        for where, type, form in said_so:
            out.write("      %-12s %-11s %s\n" % (where, type, form[:70]))
        out.write("    %d forms the reader typed differently\n" % len(wrong_type))
        for where, type, form, said in wrong_type:
            out.write(
                "      %-12s %-11s %-26s reader said %s\n"
                % (where, type, form, ", ".join(said))
            )
        out.write(
            "    %d forms the reader put in the wrong dialect\n" % len(wrong_dialect)
        )
        for where, type, form, dialect, said in wrong_dialect:
            out.write(
                "      %-12s %-11s %-26s %s, reader said %s\n"
                % (where, type, form, dialect, ", ".join(one or "none" for one in said))
            )
        out.write("    %d forms the reader invented\n" % len(extra))
        for type, form in extra:
            out.write("      %-11s %s\n" % (type, form))
        out.write(
            "    %d forms whose dialect the reader would not claim\n" % len(undecided)
        )
        for where, type, form, dialect in undecided:
            out.write(
                "      %-12s %-11s %-26s the paper's is %s\n"
                % (where, type, form, dialect)
            )

        failed += len(missing) + len(wrong_type) + len(wrong_dialect) + len(extra)

    out.write(
        "\n  %d of %d readers are graded against a hand extraction\n"
        % (len(EVERY) - len(waiting) - len(unwritten), len(EVERY))
    )
    if waiting:
        out.write("  ungraded until their paper is read by hand:\n")
        for stem in waiting:
            out.write("    %s\n" % stem)
    if unwritten:
        out.write("  read by hand and ungraded until a reader is written:\n")
        for stem in unwritten:
            out.write("    %s\n" % stem)
    out.write(
        "\n  %s\n"
        % (
            "every reader reproduces its hand extraction"
            if not failed
            else "%d disagreements to work through" % failed
        )
    )
    out.flush()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
