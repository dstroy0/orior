#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# What a contribution looks like before a person reads it.
#
#   Usage:  `python utils/maint/prose/submission_check.py <path> [<path> ...]`
#           `python utils/maint/prose/submission_check.py --tree <repository root>`
#
# THE RULE THIS SERVES
#
# CONTRIBUTING.md:8 states a condition as non-negotiable, and it is the only one there. A tool for language that comes
# out of this work requires a human to review its output. The project does not accept a contribution that removes a
# person from that loop. CONTRIBUTING.md:30 then lists three checks, prose and the kernel and the ports, and not one
# looks at the condition that comes first. A person who pastes generated text into a Salishan form is a person removed
# from the loop. No other part of the tree catches it.
#
# IT REPORTS. IT DOES NOT DECIDE, AND NOBODY MAY WIRE IT AS ONE
#
# A scanner that errors on a contribution on its own automates away the human that CONTRIBUTING.md:8 requires, in the
# name of that same rule. Nothing here prints pass, fail, accept or reject. The exit status reports what the scan found,
# and never what to do about it. Do not put this in a hook that rejects. The next person to wire it up may not know this
# rule, and this comment states it where that person will look.
#
# The numbers below are evidence for a person, and a person can look at any of them and say that the contributor writes
# that way. A contributor who does is not at fault.
#
# TWO SURFACES
#
# A name. Generated text pasted in whole tends to carry the vendor's own name somewhere: in a preamble, an error, or a
# stray line of chat. That is the cheap surface, and it catches the careless case only.
#
# A register, and this surface carries the measurement. The tool utils/maint/prose/docs_check holds 285 patterns. For
# 121 of them, it holds the human rate: the rate in 759,815 words of the papers under build/papers. This file imports
# those rates from that file, and never restates them. Their sum is the rate that a human carries for the whole list.
# The baseline here comes from the same table as the findings, and cannot drift away from it.
#
# WHAT THE RATES COUNT OVER, WHICH DECIDES WHETHER THEY MEAN ANYTHING
#
# A rate is per hundred thousand words. The denominator here is a count of character runs in the prose of the
# submission, after docs_check.prose_only removes code, math and markup. The human rates count over the output of
# english_gate instead, which is stricter. It drops the interlinear glosses, the orthography and the tables that fill a
# Salishan paper.
#
# For a submission that is English prose, the two denominators are close, and the ratio is worth a read. For a
# submission that is mostly a table, a word list or IPA, this count runs high and every rate runs low. The comparison
# then says little. The output prints the word count for that reason. Read it before the ratio.
#
# A LOW NUMBER IS NOT EVIDENCE OF ANYTHING
#
# The rates are a floor and only a floor. A contributor who knows the list can write around it. This tree's own prose
# also suppresses the phrases, and the human corpus does not. A submission with a score near the human rate says
# nothing, except that its score is near the human rate.

import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from docs_check import (
    BANNED,
    CHECKED,
    HUMAN_RATE,
    retained_dirs,  # noqa: E402
    banned_hits,
    prose_only,
    quieted,
    stage_of,
)

# The rate a human research writer carries for the whole banned list, per hundred thousand words.
# Summed from the table instead of written down. The two cannot disagree. It comes to 387.9.
HUMAN_WHOLE = sum(HUMAN_RATE.values())

# What a submission arrives as. docs_check reads the five that this repository writes in. A contribution also arrives as
# a plain page or a transcribed paper, and neither has a comment marker to find prose behind. The tool reads both as
# prose all the way down.
PLAIN = (".txt", ".rst", ".markdown", ".text", ".org")
SUBMITTED = CHECKED + PLAIN

# Names that identify a vendor or a product. None of these is also an ordinary English word. The tool matches each one
# anywhere on a line, case folded. The PHRASES below need more care.
NAMES = ("anthropic", "claude", "openai", "chatgpt")

# Words that are ordinary English on their own. The tool matches each only where a version number sits beside it. The
# word opus is a work, and sonnet and haiku are poems, and all three are model names.
#
# A measurement sets two details here. The leading \b keeps octopus 4 out, which a Salishan word list supplies. The
# pattern requires the separator, and does not make it optional, because opus1.com is an address inside an RFC. An
# optional separator matches it.
PHRASES = (
    r"\bopus[\s._-]+\d",
    r"\bsonnet[\s._-]+\d",
    r"\bhaiku[\s._-]+\d",
    r"\bgpt[\s._-]*\d",
)

# Phrases that hold one of those names and are not a vendor reference. The first is an antimony oxide mineral, which
# sits in the crystallography oracle beside senarmontite and valentinite.
ALLOWED = ("claudetite",)

# One of the vendor's model names is also a given name. A Salishan bibliography is full of people who carry it:
# Hagege, Levi-Strauss, Vaucher, Bacqueville de la Potherie. Shannon is one entry away from them. An exact phrase for
# each would need a new entry for every new paper, and the entry would be absent on the day it mattered.
#
# The tool finds a person by shape instead. A surname follows the given name, or the given name trails one. A scanned
# page separates the two with whatever the scanner made of the comma.
#
# The pattern captures the surname, and does not only match it, because its first character separates a person from a
# model. A person's surname carries a capital there, and the word "output" after a model name does not. The test runs on
# the line as written and never on a folded copy, because a fold destroys the capital that it reads.
#
# The character class admits diacritics. A scanned bibliography stores Levi-Strauss decomposed, as L, e, U+0301. A bare
# \w stops at the mark, and falls through to a lowercase start further along. That would report one bibliography entry
# as a vendor reference.
#
# These do not clear a line. They move it to a count that prints and errors nothing, and the lines themselves print
# under --people. A hit that the tool drops quietly is a hit that nobody reads.
CHARACTER = r"[^\W\d_]"
MARK = chr(0x0300) + chr(0x002D) + chr(0x036F)
PEOPLE = (
    re.compile(
        r"(?i:claude)\s+(?:de\s+la\s+|de\s+|van\s+|von\s+)?(?P<surname>"
        + CHARACTER
        + r"[\w'~"
        + MARK
        + r"\-]{2,})"
    ),
    re.compile(
        r"(?P<surname>" + CHARACTER + r"[\w'~." + MARK + r"\-]{2,})\s*[,.•] *(?i:claude)\b"
    ),
)

# Words that follow the given name in a model's name, and never in a person's. A line that holds one of these beside the
# name is a model reference, whatever else its shape looks like.
MODELS = ("opus", "sonnet", "haiku", "instant", "code", "next")

# Extensions read as bytes. A match inside one would be unreadable in a report.
BINARY = (
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".webp",
    ".bmp",
    ".ico",
    ".pdf",
    ".zip",
    ".gz",
    ".xz",
    ".tar",
    ".7z",
    ".wav",
    ".mp3",
    ".flac",
    ".ogg",
    ".ttf",
    ".otf",
    ".woff",
    ".woff2",
    ".so",
    ".dll",
    ".exe",
    ".dylib",
    ".o",
    ".a",
    ".pyc",
    ".npy",
    ".npz",
    ".bin",
)

# Bytes above which the report names a file as skipped, and does not read it. A silent skip reads as a clean result. The
# foot of the report names every skipped file.
LARGE = 4 * 1024 * 1024

MATCHERS = tuple(
    [(one, re.compile(re.escape(one))) for one in NAMES]
    + [(one, re.compile(one)) for one in PHRASES]
)

# A character run. Digits, punctuation and the markup that docs_check leaves behind are not words. A count of them would
# lower every rate, because the denominator would grow.
WORD = re.compile(r"[^\W\d_][\w'’-]*")

TRIM = 92


def allowed_at(folded, start, stop):
    """Whether an allowed phrase covers the match running from start to stop."""
    for phrase in ALLOWED:
        at = folded.find(phrase)
        while at != -1:
            if (at <= start) and ((at + len(phrase)) >= stop):
                return True
            at = folded.find(phrase, at + 1)
    return False


def person_line(text):
    """Whether the given name on one line belongs to a person and not to a model."""
    folded = text.lower()
    for word in MODELS:
        if word in folded:
            return False
    for matcher in PEOPLE:
        found = matcher.search(text)
        if found and found.group("surname")[:1].isupper():
            return True
    return False


def names_in(text):
    """The vendor names on one line, and whether they are one person's name instead.

    A line is a person's when the only name is the given name and it sits beside a surname. Every
    other line is a vendor reference until somebody reads it and says otherwise.
    """
    folded = text.lower()
    found = []
    for name, matcher in MATCHERS:
        for one in matcher.finditer(folded):
            if allowed_at(folded, one.start(), one.end()):
                continue
            if name not in found:
                found.append(name)
            break
    if tuple(found) == ("claude",) and person_line(text):
        return (("claude",), True)
    return (tuple(found), False)


def displayed(text):
    """One matched line, trimmed to retain a report line from wrapping."""
    flat = " ".join(text.split())
    if len(flat) <= TRIM:
        return flat
    return flat[: TRIM - 3] + "..."


def text_of(path):
    """One submitted file as text, or None where the read failed."""
    try:
        if os.path.getsize(path) > LARGE:
            return None
        with open(path, "rb") as handle:
            raw = handle.read()
    except OSError:
        return None
    if b"\x00" in raw:
        return None
    return raw.decode("utf-8", "replace")


def prose_of(path, lines):
    """The prose of one submitted file, with code, math and markup taken out.

    docs_check knows the five extensions that this repository writes in. A plain page or a transcribed paper has no
    comment marker to find prose behind. The function reads it whole, and honors only its quiet blocks.
    """
    if path.endswith(CHECKED):
        return prose_only(path, lines)
    return quieted(lines)


def walk(roots):
    """Every submitted file under the given paths, with a file argument taken as itself."""
    found = []
    for root in roots:
        if os.path.isfile(root):
            found.append(root)
            continue
        for here, dirs, names in os.walk(root):
            dirs[:] = retained_dirs(here, dirs)
            found.extend(os.path.join(here, name) for name in sorted(names))
    return sorted(found)


def git(root, *arguments):
    """One git command in one repository, as text, or None where git errored.

    The helper turns core.quotePath OFF for every call, and not only at the two sites that list paths.

    With it on, which is git's default, a path that holds any character outside ASCII comes back
    quoted and octal-escaped. The path `papers/...Kwak’wala.pdf` comes back as
    `"papers/...Kwak\\342\\200\\231wala.pdf"`. tracked() then joins that to the root, and produces a
    path that does not exist. The file drops out of the listing with no error. A screen that
    cannot see a file reports it clean.

    This is not hypothetical in this tree. The Salishan corpus tracks two papers whose names carry
    U+2019, which is ordinary Salishan orthography. A listing without this flag returns 0 of them,
    and one with it returns 2.

    That is the argument for the flag in the helper. A rule in a document relies on the memory of
    whoever writes the next call site. A flag in the one function that every call goes through does
    not.
    """
    try:
        done = subprocess.run(
            ("git", "-c", "core.quotePath=false") + arguments,
            cwd=root,
            capture_output=True,
            timeout=120,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if done.returncode != 0:
        return None
    return done.stdout.decode("utf-8", "replace")


def tracked(root):
    """Every path that git tracks under one repository root, or None where git errored.

    The tree mode asks a question about a contribution too. It asks whether the tree that a contributor works
    inside already carries what the screen looks for.
    """
    listed = git(root, "ls-files")
    if listed is None:
        return None
    return [
        os.path.join(root, one.replace("/", os.sep))
        for one in listed.splitlines()
        if one
    ]


def served(root):
    """Every path that the upstream branch holds, which is every path that a fresh clone receives.

    A tracked file and a served one are different situations, and only one of them can still change quietly. A file
    removed from the working tree later stays in the commits that carry it. A clean listing means only that new clones
    no longer show it in the tree. The history still holds it.
    """
    named = git(root, "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}")
    if not (named and named.strip()):
        named = git(root, "rev-parse", "--abbrev-ref", "origin/HEAD")
    if not (named and named.strip()):
        return (None, frozenset())
    branch = named.strip()
    listed = git(root, "ls-tree", "-r", "--name-only", branch)
    if listed is None:
        return (branch, frozenset())
    return (branch, frozenset([one for one in listed.splitlines() if one.strip()]))


def counted(paths, base, show_people, out, clones=None):
    """Read every file once. Returns the name hits, the per pattern counts and the word count."""
    named = []
    lined = []
    people = []
    unread = []
    tally = {}
    words = 0
    read = 0

    for path in paths:
        shortened = os.path.relpath(path, base).replace("\\", "/")

        found, person = names_in(shortened)
        if found and not person:
            # Where the tree is a repository: whether a fresh clone already receives this path. Nothing tracks a
            # submission, and the output leaves the column off entirely.
            located = (
                None
                if clones is None
                else ("in every clone" if shortened in clones else "local only")
            )
            named.append((shortened, found, located))

        if path.endswith(BINARY) or not path.endswith(SUBMITTED):
            continue
        text = text_of(path)
        if text is None:
            unread.append(shortened)
            continue
        read += 1
        lines = text.splitlines()

        for number, line in enumerate(lines, 1):
            hit, person = names_in(line)
            if not hit:
                continue
            where = (shortened, number, line)
            if person:
                people.append(where)
            else:
                lined.append(where)

        said = prose_of(path, lines)
        words += len(WORD.findall(" ".join(said)))
        for _, pattern, _ in banned_hits(said, quotations=path.endswith(".md")):
            tally[pattern] = tally.get(pattern, 0) + 1

    if named:
        out.write("\n  NAMES IN A PATH\n")
        for shortened, found, located in named:
            if located is None:
                out.write("    %s  [%s]\n" % (shortened, ", ".join(found)))
            else:
                out.write(
                    "    %-15s %s  [%s]\n" % (located, shortened, ", ".join(found))
                )
    if lined:
        out.write("\n  NAMES IN THE TEXT\n")
        for shortened, number, line in lined:
            out.write("    %s:%d\n      %s\n" % (shortened, number, displayed(line)))
    if people and show_people:
        out.write("\n  A PERSON OF THAT NAME, reported and not counted against\n")
        for shortened, number, line in people:
            out.write("    %s:%d\n      %s\n" % (shortened, number, displayed(line)))
    if unread:
        out.write("\n  NOT READ\n")
        for shortened in unread:
            out.write("    %s\n" % shortened)

    return (named, lined, people, tally, words, read)


def register(out, tally, words):
    """The banned list as rates per hundred thousand words, against what a human writer carries."""
    if not words:
        out.write("\n  REGISTER\n    no prose was read. There is no rate to report\n")
        return
    scale = 100000.0 / words

    total = sum(tally.values()) * scale
    phrase = (
        sum(count for pattern, count in tally.items() if stage_of(pattern) == "phrase")
        * scale
    )
    absent = (
        sum(count for pattern, count in tally.items() if pattern not in HUMAN_RATE)
        * scale
    )

    out.write("\n  REGISTER, per hundred thousand words of prose\n")
    out.write(
        "    %-26s %9.1f   human %7.1f   %s\n"
        % ("the whole banned list", total, HUMAN_WHOLE, times(total, HUMAN_WHOLE))
    )
    out.write(
        "    %-26s %9.1f   human %7.1f   %s\n"
        % ("the phrase stage", phrase, phrase_human(), times(phrase, phrase_human()))
    )
    out.write(
        "    %-26s %9.1f   human %7.1f   %s\n"
        % ("patterns humans never use", absent, 0.0, "no human rate to divide by")
    )

    ranked = sorted(
        tally.items(), key=lambda one: excess(one[0], one[1] * scale), reverse=True
    )
    if not ranked:
        return
    out.write(
        "\n    %-46s %5s %8s %8s %9s\n" % ("token", "n", "here", "human", "times")
    )
    for pattern, count in ranked:
        rate = count * scale
        human = HUMAN_RATE.get(pattern, 0.0)
        out.write(
            "    %-46s %5d %8.1f %8.1f %9s\n"
            % (readable(pattern), count, rate, human, times(rate, human))
        )


def phrase_human():
    """What a human writer carries for the phrase stage, summed from the same table."""
    return sum(
        rate for pattern, rate in HUMAN_RATE.items() if stage_of(pattern) == "phrase"
    )


def excess(pattern, rate):
    """How far above a human writer one pattern sits, for the rank order. Absent patterns rank first."""
    human = HUMAN_RATE.get(pattern, 0.0)
    if not human:
        return float("inf") if rate else 0.0
    return rate / human


def times(rate, human):
    """One ratio, or the words for a denominator of zero."""
    if not human:
        return "absent" if rate else "-"
    return "%.1f" % (rate / human)


def readable(pattern):
    """One pattern with its regex furniture removed, which leaves the phrase that a reader can read."""
    flat = pattern.replace(r"\b", "").replace(r"\s?", " ").replace(r"\s+", " ")
    flat = flat.replace("[\\w-]+", "X").replace(".{1,40}", " ... ")
    flat = re.sub(r"\\(.)", r"\1", flat)
    return " ".join(flat.split())[:46]


def main():
    import io

    out = io.TextIOWrapper(
        sys.stdout.buffer, encoding="utf-8", errors="replace", newline=""
    )

    show_people = "--people" in sys.argv
    tree = "--tree" in sys.argv
    given = [one for one in sys.argv[1:] if not one.startswith("-")]
    if not given:
        out.write("  usage: submission_check.py <path> [<path> ...]\n")
        out.write("         submission_check.py --tree <repository root>\n")
        out.flush()
        return 2

    base = os.path.abspath(given[0])
    if os.path.isfile(base):
        base = os.path.dirname(base)

    clones = None
    if tree:
        root = os.path.abspath(given[0])
        paths = tracked(root)
        if paths is None:
            out.write("  %s is not a git repository, or git errored\n" % given[0])
            out.flush()
            return 2
        branch, clones = served(root)
        out.write("  TRACKED IN %s\n" % base)
        out.write("  upstream %s\n" % (branch or "none. Nothing is marked as served"))
    else:
        paths = walk([os.path.abspath(one) for one in given])
        out.write("  SUBMISSION %s\n" % ", ".join(given))

    named, lined, people, tally, words, read = counted(
        paths, base, show_people, out, clones
    )

    out.write(
        "\n  %d file(s) offered, %d read, %s words of prose\n"
        % (len(paths), read, format(words, ","))
    )
    if people and not show_people:
        out.write(
            "  %d line(s) carry the given name of a person. Run with --people to read them\n"
            % len(people)
        )

    register(out, tally, words)

    out.write(
        "\n  %d name(s) in a path, %d in the text, %d register hit(s).\n"
        % (len(named), len(lined), sum(tally.values()))
    )
    out.write("  A person reads this. Nothing here is a verdict.\n")
    out.flush()

    # A read of nothing is never a pass. A run that opens no file and reports success is the failure that nobody sees.
    # That is how a wrong path goes unnoticed, until somebody wonders why the count never moves.
    if read == 0:
        out.write("  no file was read. Nothing was screened. Nothing came back clean\n")
        out.flush()
        return 2
    if named or lined or tally:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
