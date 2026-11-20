#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Lushootseed of Mellesmoen and Kye, ICSNL 61, in the order of that paper's own structure.
#
#   Usage:  python maint/data/salishan/corpus_script_extraction/extract_mellesmoen_kye.py
#
# This file serves one paper, and that paper is not a narrative. It is a phonological analysis of stress in the two
# dialects of Lushootseed. Its language material arrives in five shapes:
#   - fifteen tables of citation forms.
#   - sixteen optimality-theoretic tableaux.
#   - six lines of Annie Jack's connected speech in Appendix A.
#   - thirty-odd phonotactic bullets in Appendix B.
#   - forms cited inline with a gloss in quotes throughout the prose.
#
# WHICH FORMS COUNT AS A WORD HERE
#
# This is the main difficulty of the paper, and the reason that a reader built on characters alone would poison the
# corpus. A tableau prints five candidate forms of one word, and the analysis rejects four of them. An input in slashes
# is an abstraction that the analysis posits, and not a form that anybody said. A starred form in Appendix B is there
# because the language does not have it. Each of those carries the same characters as a real word, sits in the same
# paragraph, and would pass any test built on the alphabet.
#
#   attested       Table 9 č̓ə́ƛ̓aʔ, tableau (34b) marked ☞, Appendix B č̓əƛ̓aʔ, Annie Jack's sentences
#   not attested   tableau (34a) (č̓ƛ̓áʔ) and (34c) (č̓əƛ̓áʔ), input /č̓ƛ̓aʔ/, Appendix B *c̓ƛ̓aʔ
#
# The reader therefore types every line by where the paper put it. Only citation, surface and utterance in a Lushootseed
# dialect reach the pure stream. The candidates that lose, the inputs, the starred forms and the cognates from four
# other languages stay in the record and out of the corpus.
#
# THE DIALECTS
#
# The paper prints Northern and Southern Lushootseed side by side in Tables 8, 9, A1 and A2 and in example (18). Each
# column holds the same gloss with a different form. The whole subject of the paper is that the two differ. A corpus
# that loses the column has ǰə́šəd and ǰə́səd as two unrelated words of one language. The dialect comes from the table's
# own heading or from the tableau's own title, never from the shape of the form.
#
# WHAT EACH CHECK COMPARES
#
# The control is the hand extraction in examples/Salishan/oracles/Mellesmoen_Kye_ICSNL61.oracle.tsv. A person reads
# that file off the paper and verifies it against the paper in both directions. This repository keeps the oracle files
# and not the papers. The paper text comes from the private corpus repository into build/papers, and this file reads
# it there.
#
# `reader_check.py` grades this file against it. `coverage_check.py` asks the easier question apart from that, which is
# whether each token got out of the paper at all.

import io
import os
import re
import subprocess
import sys

from mellesmoen_kye_repair import repaired
from salish_marking import DERIVED, SPOKEN, UNCLASSIFIED, tagged_spans
from salish_unsorted import UNKNOWN_KIND, covered_tokens, unreached, write_unsorted


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

SOURCE = os.path.join(PAPERS, "Mellesmoen_Kye_ICSNL61.txt")

# <spoken by>_<original paper>_<who wrote it down>_Salish_<language without accents>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "MarthaLamont-AnnieJack_AComparativeAnalysisOfStressInNorthernAndSouthernLushootseed"
    "_MellesmoenKye_Salish_lushootseed_2026_mixed.txt",
)

PAGE = re.compile(r"^===== page \d+ =====$")

# The alphabet that this paper writes Lushootseed with, after NFC. It is wider than the marks that the nine other papers
# share, and it must be. `yidád` ‘fish trap’, `báyac` ‘meat’ and `x̌il` ‘lost’ carry no glottal stop, schwa or lateral,
# and a test built on those three reads them as English.
#
# The alphabet has no bare x. The language writes x̌ and xʷ, and the caron and ʷ find both of those. A bare x
# here would make examples, exception, complex, prefix and forty other English words into forms of Lushootseed.
ALPHABET = "ʔəɬƛʷščǰᶻθáíúàìù" + "̓̌́̀"

# What only a Salish orthography writes, without the accented vowels that the alphabet above also holds. The reference
# list uses it, where a title carries Skwxwú7mesh and nɬeʔkepmxcín beside the Yoruba name Olúṣẹ̀yẹ Adéṣọlá. An accent is
# what all three have in common.
SALISH = "ʔəɬƛʷščǰᶻθ7" + "̓̌"

# A form followed by its gloss in single quotes. The paper cites a word this way in prose, in Appendix B, and inside the
# cells of Table 10 and example (1).
FORM_GLOSS = re.compile(r"(\S+)\s+[‘']([^’']*)[’']")

# The note that Tables 8 and 9 put after a Southern form whose /a/ surfaces as schwa. This pattern writes the note out,
# because the note holds a second attested form in square brackets. The t̓ə́q̓ʷab row also has no closing
# parenthesis. Anything that counts brackets stops in the wrong place.
REALIZED = re.compile(r"\(/a/ → \[ə\](?:,\s*so\s*realized as \[([^\]]+)\])?\)?")

# A form the paper offers for comparison at the end of a Table 5 row: dᶻəx̌əx̌bíd (cf. dᶻə́x̌əx̌).
COMPARE = re.compile(r"\(cf\.\s*([^)]*)\)\s*$")

# A tableau's title line, which carries the dialect and the word that the tableau derives.
TABLEAU = re.compile(r"^\((\d{1,2})\)\s+(.*Lushootseed.*)$")

# A tableau's input row: the underlying form, in slashes, at the head of the constraint columns.
INPUT_ROW = re.compile(r"^\s+(/[^/]+/)")

# A tableau's candidate rows, with the hand mark on the one that the paper calls attested.
CANDIDATE = re.compile(r"^([a-h])\.\s+(☞\s+)?(\S+)")

# A section heading, which ends a table that has no blank line after it. Table 13 is such a table.
HEADING = re.compile(r"^(\d+(?:\.\d+)*\s+[A-Z]|Appendix\s+[AB]\b|References\s*$)")

# Which appendix the walk is in, tracked apart from the section heading. Footnote 6 opens with
# `6 Additional glossing abbreviations` and matches the heading pattern above. Without this, the walk leaves Appendix A
# part way down the page, and reads the last three of Annie Jack's six sentences as prose.
APPENDIX = re.compile(r"^Appendix\s+([AB])\b")

# One of Annie Jack's sentences, opened by the condition it illustrates. The colon separates
# these from Appendix B's numbered cluster headings, which state a constraint instead.
INTERLINEAR = re.compile(r"^\((\d)\)\s+\S.*:\s*$")

# An Appendix B bullet's label, which names the cluster type and is not a word: *RRonset:, SL:, NL:
BULLET_LABEL = re.compile(r"^([^‘’]*?):\s+")

SOUTHERN = "southern"
NORTHERN = "northern"

# A form that the paper prints with the language it belongs to, as Appendix A does for every cognate it cites. The paper
# prints yəšən in Sechelt and ǰəšən in ʔayʔaǰuθəm with no gloss at all. The language name is the only thing that marks
# them as forms and not as more of the sentence.
FORM_IN = re.compile(r"(\S+)\s+in\s+([A-Zʔ][^\s(,.;:]*)")

# What the paper calls each language, and what this corpus calls it. The paper cites Squamish, Twana, Sechelt and
# ʔayʔaǰuθəm here for comparison, and none of them is a dialect of Lushootseed. The reader reads the names for that
# reason: čalaš ‘hand’ sits in a Lushootseed paper and is a Twana word.
LANGUAGES = {
    "Southern": SOUTHERN,
    "Northern": NORTHERN,
    "Squamish": "squamish",
    "Twana": "twana",
    "Sechelt": "sechelt",
    "ʔayʔaǰuθəm": "ayajuthem",
}

# A parenthesis, and what marks it as a reference instead of a form. Northern Lushootseed prints its own
# name for itself as (dxʷləšucid), and Bianco 1995:127 sits in the same shape one line away.
PARENTHESIZED = re.compile(r"\(([^()]*)\)")
YEAR = re.compile(r"\b(1[89]|20)\d{2}\b")

LAYER = {
    "citation": SPOKEN,
    "surface": SPOKEN,
    # Named transcription because the other ten readers name it that. One vocabulary across the
    # corpus is worth more than a word that fits this paper slightly better on its own.
    "transcription": SPOKEN,
    # Derived, unlike the nine narratives. Those papers state that the English of each story comes from the person who
    # told it. This paper does not. Leon Metcalf recorded Annie Jack in the 1950s, and the English under the sentences
    # is the authors' reading of them.
    "translation": DERIVED,
    "morphemes": DERIVED,
    "winner": DERIVED,
    "candidate": DERIVED,
    "underlying": DERIVED,
    "impossible": DERIVED,
    "cluster": DERIVED,
    "segment": DERIVED,
    "notation": DERIVED,
    "mention": DERIVED,
    "reference": DERIVED,
    "damage": DERIVED,
    "gloss": DERIVED,
    "essay": DERIVED,
    UNCLASSIFIED: DERIVED,
}

# The types that are words of Lushootseed. Everything else in the record is an abstraction, a
# rejected hypothesis, a form of another language, or English.
PURE = ("citation", "surface", "transcription")

# Each table by its caption, with how many language columns its rows have and which dialect each column is. These come
# from the table headings in the paper. Table 10 is the odd one: its two columns are both Southern, and each cell is a
# form with its own gloss in quotes. The code handles it apart.
TABLES = {
    1: (NORTHERN,),
    2: (NORTHERN,),
    3: (NORTHERN,),
    4: (NORTHERN,),
    5: (NORTHERN,),
    6: (SOUTHERN,),
    7: (SOUTHERN,),
    8: (SOUTHERN, NORTHERN),
    9: (SOUTHERN, NORTHERN),
    11: (SOUTHERN,),
    12: (SOUTHERN,),
    13: (SOUTHERN,),
    "A1": (SOUTHERN, NORTHERN),
    "A2": (SOUTHERN, NORTHERN),
}

TABLE_CAPTION = re.compile(r"^Table (A?\d+)[:.]")

# How a table knows that one of its rows ended, which is not the same question in every table.
#
# Most rows are one line and hold one form, and a count of forms works for them. Tables 8 and 9 put a note after the
# Southern form that tells how its /a/ surfaces, and the note wraps across as many as three lines. Those rows end when
# two forms appear outside the note. A count cannot work at all in Table A2. Its rows carry lab ‘see’, sbadil
# ‘mountain’, ti and ta, and none of these holds a character of the alphabet above. A row of it is over when its
# parentheses close.
BY_FORMS = "forms"
BY_PARENS = "parentheses"

ENDS = {
    8: BY_FORMS,
    9: BY_FORMS,
    "A1": BY_PARENS,
    "A2": BY_PARENS,
}


def carries_language(text):
    """Whether a string holds a character this paper writes Lushootseed with."""
    return any(one in text for one in ALPHABET)


def language_token(token):
    """Whether one token, stripped of the punctuation around it, is a form of the language."""
    plain = token.strip(".,;:()[]‘’“”'\"…*/→+")
    return bool(plain) and carries_language(plain)


def dialect_named(text):
    """Which dialect a heading or a tableau title names, or nothing where it names neither."""
    if "Southern" in text:
        return SOUTHERN
    if "Northern" in text:
        return NORTHERN
    return ""


def one_dialect(text):
    """The dialect a prose line names, where it names exactly one of them.

    A line that names both is not a line to guess from. Section 4.2.2.1 opens with a contrast of tx̌ʷúd in Northern and
    tə́x̌ʷud in Southern, and either dialect assigned to both forms is wrong. Those come out unstated, and
    reader_check.py counts them. The reader does not invent an answer.
    """
    if ("Southern" in text) and ("Northern" in text):
        return ""
    return dialect_named(text)


def without_note(text):
    """One table row with the note about how its /a/ surfaces taken out, and the form that note contained.

    The t̓ə́q̓ʷab row of Table 9 never closes its parenthesis. The code matches the note by its own words, and does not
    count brackets.
    """
    found = REALIZED.search(text)
    if not found:
        return text, ""
    return (text[: found.start()] + " " + text[found.end() :]).strip(), (
        found.group(1) or ""
    )


def table_rows(lines, at, columns, ending):
    """One table's rows, from the line after its caption to the blank line or heading that ends it.

    Before a row starts, the code skips caption continuations and column headings, because they carry no character of
    the alphabet. Once a row starts, a line without one is a gloss that the typesetter wrapped, and it belongs to the
    current row.
    """
    held = []
    buffer = ""
    started = False
    while at < len(lines):
        trimmed = " ".join(lines[at].split())
        if (not trimmed) or PAGE.match(trimmed) or HEADING.match(trimmed):
            break
        at += 1
        if (not started) and (not carries_language(trimmed)):
            continue
        started = True
        buffer = ("%s %s" % (buffer, trimmed)).strip()
        stripped, surface = without_note(buffer)
        if ending == BY_PARENS:
            done = stripped.count("(") == stripped.count(")")
        elif ending == BY_FORMS:
            done = len([one for one in stripped.split() if language_token(one)]) >= len(
                columns
            )
        else:
            done = bool([one for one in stripped.split() if language_token(one)])
        if done:
            held.append((buffer, stripped, surface))
            buffer = ""
    if buffer:
        held.append((buffer,) + without_note(buffer))
    return held, at


def from_table(number, rows):
    """One table's rows as items, each a locator, dialect, type, form and gloss.

    The forms are the last tokens of the row and the gloss is everything before them, the only
    reading that survives Table A2. Four of its Southern and Northern forms, lab, sbadil, ti and
    ta, carry no character of the alphabet. Nothing about the tokens themselves says which of
    them is a word; their position in the row does.
    """
    held = []
    columns = TABLES[number]
    where = "Table %s" % number
    for whole, stripped, surface in rows:
        if number == 5:
            found = COMPARE.search(stripped)
            if found and carries_language(found.group(1)):
                stripped = stripped[: found.start()].strip()
                held.append((where, columns[0], "citation", found.group(1).strip(), ""))
        tokens = stripped.split()
        if len(tokens) < len(columns):
            held.append((where, "", UNCLASSIFIED, stripped, ""))
            continue
        forms = tokens[-len(columns) :]
        gloss = " ".join(tokens[: -len(columns)])
        for dialect, form in zip(columns, forms):
            # Table A2 prints two Northern alternants for ‘year’ in one cell, dᶻəlč̓/ǰəlč̓.
            for one in (form.split("/") if ("/" in form) else [form]):
                if one:
                    held.append((where, dialect, "citation", one, gloss))
        if surface:
            held.append((where, columns[0], "surface", "[%s]" % surface, gloss))
    return held


def from_paired_cells(where, dialect, lines):
    """Rows whose every cell is a form with its own gloss in quotes.

    Table 10 and example (1) print their forms this way, with a stem beside what it becomes, each labeled.
    """
    held = []
    for line in lines:
        for form, gloss in FORM_GLOSS.findall(line):
            plain = form.strip("()[].,;:")
            if carries_language(plain):
                held.append((where, dialect, "citation", plain, gloss))
    return held


def from_comparison(lines):
    """Example (18), which prints a Southern form, a Northern form and one gloss for both."""
    held = []
    for line in lines:
        trimmed = " ".join(line.split())
        found = FORM_GLOSS.search(trimmed)
        if not found:
            continue
        character = trimmed.split(".")[0].strip()
        where = "(18%s)" % character if (len(character) == 1) else "(18)"
        tokens = trimmed[: found.start(2) - 1].split()
        forms = [one for one in tokens if language_token(one)]
        for dialect, form in zip((SOUTHERN, NORTHERN), forms[-2:]):
            held.append((where, dialect, "citation", form, found.group(2)))
    return held


def from_tableau(number, dialect, title, lines):
    """One tableau: the word it derives, the input it posits, and every candidate it weighs.

    The hand mark is the paper's own mark for the candidate that it calls attested. Nothing else in the block
    separates a form of the language from the four candidates that the analysis exists to rule out.
    """
    where = "(%d)" % number
    held = []
    found = FORM_GLOSS.search(title)
    if found and carries_language(found.group(1)):
        held.append((where, dialect, "citation", found.group(1), found.group(2)))
    for line in lines:
        trimmed = line.rstrip()
        seen = INPUT_ROW.match(trimmed)
        if seen:
            held.append((where, dialect, "underlying", seen.group(1), ""))
            continue
        seen = CANDIDATE.match(
            " ".join(trimmed.split()) if trimmed.startswith(" ") else trimmed
        )
        if seen:
            type = "winner" if seen.group(2) else "candidate"
            held.append(
                ("(%d%s)" % (number, seen.group(1)), dialect, type, seen.group(3), "")
            )
    return held


def from_interlinear(number, block):
    """One of Annie Jack's sentences from Appendix A, in the four lines that the paper prints it on.

    These six are the only connected speech in the paper. Everything else is a word on its own, from a dictionary, from
    Hess, from Bianco, or from elicitation. These six come off a tape from the 1950s, and each carries the story and the
    line that it occurs in.
    """
    where = "App A (%d)" % number
    held = []
    if len(block) < 4:
        return [(where, "", UNCLASSIFIED, " ".join(block), "")]
    said, morphemes, gloss, english = block[:4]
    held.append((where, SOUTHERN, "transcription", said.strip("… "), ""))
    held.append((where, SOUTHERN, "morphemes", morphemes, ""))
    held.append((where, "", "gloss", gloss, ""))
    held.append((where, SOUTHERN, "translation", english, ""))
    return held


def from_bullet(where, text):
    """One Appendix B bullet: what the language permits, and what it does not.

    The starred form is the point of the bullet and is not a word. The paper prints *ylab to show that yəlab is what the
    language has instead. A corpus that holds both holds one word and one sequence that its own source states cannot
    occur.
    """
    held = []
    body = BULLET_LABEL.sub("", text.lstrip("• ").strip(), count=1)
    dialect = ""
    if text.lstrip("• ").startswith("SL:"):
        dialect = SOUTHERN
    elif text.lstrip("• ").startswith("NL:"):
        dialect = NORTHERN
    pairs = FORM_GLOSS.findall(body)
    for form, gloss in pairs:
        plain = form.strip("()[],;:")
        if is_form(plain) and not plain.startswith("*"):
            held.append((where, dialect, "citation", plain, gloss))
    if not pairs:
        for token in body.split():
            plain = token.strip("()[],;:")
            if is_form(plain) and not plain.startswith("*"):
                held.append((where, dialect, "citation", plain, ""))
    for token in body.split():
        # The star is the claim, and it identifies the form. `*ylab`, `*blups` and `*labd` carry no character of the
        # alphabet. Neither do the words that they are impossible versions of. A test on characters finds none of the
        # three.
        if token.startswith("*") and (len(token) > 1):
            held.append((where, dialect, "impossible", token.strip(",;:"), ""))
    return held


def named_after(text, at):
    """The language named right after a gloss, where one is, as in čəwaš ‘wife’ in Squamish."""
    found = re.match(r"\s+in\s+([A-Zʔ][^\s(,.;:]*)", text[at:])
    return found.group(1) if found else ""


def is_morpheme(token):
    """Whether a bare token that the prose cites is a morpheme that this paper discusses.

    A boundary mark alone is not enough. The extraction leaves a space before the hyphen in several English compounds.
    Post -alveolar, non -moraic and left -aligned all arrive with the look of bound morphemes. Length and case separate
    them. The morphemes that this paper cites bare are -il, -ac, -al, -d, -ši and dxʷ-. An English word broken off a
    compound is longer than four characters or carries a capital.
    """
    stem = token.strip("-=")
    if (token == stem) or (not stem) or (len(stem) > 4):
        return False
    return stem.islower() or carries_language(stem)


def is_form(token):
    """Whether a token that the prose cites is a form of a language and not an English word.

    There are two tests, because the paper prints two types of form. Most forms carry a character of the alphabet. The
    bound morphemes do not: -ac ‘tree’, -yalus ‘edge’, -il, -d ‘TR’ and dxʷ- use plain characters. Only the boundary
    mark on them says that they are morphemes and not English.
    """
    return bool(token) and (
        carries_language(token)
        or (token.strip("-=") and (token[0] in "-=" or token[-1] in "-="))
    )


def from_prose(where, text, other="", fallback=""):
    """Every form that the prose cites, in the four ways that this paper cites one.

    A form with its gloss in quotes is the common case. The paper names a cognate by the language it is from, as čəwaš
    ‘wife’ in Squamish. It does the same for a form with no gloss at all, as ǰəšən in ʔayʔaǰuθəm. A dialect's own name
    for itself arrives in parentheses after the English name of it. The code keeps anything else that carries the
    alphabet as a mention. A mention holds it in the record, and does not claim that the paper presents it as a form.

    The dialect comes from the passage where it names one, and otherwise from the section that it sits in. Section 3.2.3
    discusses ɬidálgʷiɬ and q̓ílalič and names no dialect in either sentence. The heading of §3 is Northern Lushootseed
    stress pattern, and that tells the reader. A passage that names both names them about different words, and there
    nothing decides.
    """
    held = []
    dialect = one_dialect(text) or fallback
    claimed = set()

    for found in FORM_GLOSS.finditer(text):
        plain = found.group(1).strip("()[],;:")
        if (not is_form(plain)) or plain.startswith("*") or plain.startswith("/"):
            continue
        claimed.add(plain)
        held.append(
            (
                where,
                LANGUAGES.get(named_after(text, found.end()), dialect),
                "citation",
                plain,
                found.group(2),
            )
        )

    for found in FORM_IN.finditer(text):
        plain = found.group(1).strip("()[],;:")
        language = LANGUAGES.get(found.group(2))
        if (plain in claimed) or (not language) or not is_form(plain):
            continue
        claimed.add(plain)
        held.append((where, language, "citation", plain, ""))

    for found in PARENTHESIZED.finditer(text):
        inside = found.group(1)
        # A bibliographic parenthesis carries a year and names no form, and that retains Bianco
        # and Kuipers out of a corpus of the language. A long parenthesis is a sentence. §6 has a
        # twelve-word aside that happens to mention ʔayʔaǰuθəm.
        if YEAR.search(inside) or (len(inside.split()) > 4):
            continue
        # Section 2.1 gives each dialect's own name for itself. It follows the English name: Northern Lushootseed
        # (dxʷləšucid), Southern Lushootseed (xʷəlšucid or txʷəlšucid). The sentence names both dialects and decides
        # nothing. The words in front of the bracket decide it exactly.
        ahead = one_dialect(" ".join(text[: found.start()].split()[-3:])) or dialect
        for token in inside.split():
            for plain in token.strip("()[],;:").split("/"):
                if (not plain) or (plain in claimed) or plain.startswith("*"):
                    continue
                if not carries_language(plain):
                    continue
                claimed.add(plain)
                held.append((where, LANGUAGES.get(plain, ahead), "citation", plain, ""))

    for token in text.split():
        # Square brackets stay on. The paper puts a cluster in them, [k̓ʷd] and [lč], and a surface realization too. The
        # brackets are how it says which of the two a string is.
        for plain in token.strip("(),;:.‘’“”…").split("/"):
            if (not plain) or (plain in claimed) or plain.startswith("*"):
                continue
            if (plain in LANGUAGES) and carries_language(plain):
                # ʔayʔaǰuθəm is a word of ʔayʔaǰuθəm wherever it occurs, also inside a section about Lushootseed. The
                # dialect of the section must not reach it. The test is that the name is in the language that it names.
                # Squamish, Twana and Sechelt are English words for languages, and are not forms of anything here.
                claimed.add(plain)
                held.append((where, LANGUAGES[plain], "citation", plain, ""))
                continue
            if is_morpheme(plain):
                claimed.add(plain)
                held.append((where, dialect, "citation", plain, ""))
                continue
            # A capital opens the name of an author, never a form. These languages use lower case, apart from a form
            # that opens in ʔ. Bermúdez, Adéṣọlá and Olú all carry an accented vowel that the alphabet holds.
            if plain[0].isupper() or not carries_language(plain):
                continue
            if other and not any(one in plain for one in SALISH):
                continue
            claimed.add(plain)
            held.append(
                (
                    where,
                    other or dialect,
                    "mention" if not other else "reference",
                    plain,
                    "",
                )
            )
    return held


def read_paper(lines):
    """Every item the paper holds, and every line of it that carried one, in the paper's own order.

    A single walk. Something that the paper prints for a reader to see opens each structure:
      - a caption opens a table.
      - a title that names a dialect and a glossed word opens a tableau.
      - a numbered condition that ends in a colon opens one of Annie Jack's sentences.
      - a bullet mark opens a phonotactic bullet.

    Anything else is prose. The code keeps prose whole and also mines it for the forms it cites.
    """
    items = []
    essay = []
    taken = set()
    paragraph = []
    at = 0
    section = "front matter"
    appendix = ""
    # The dialect that the last section heading named, kept in a list that flush() can read. The heading of §3 is
    # Northern Lushootseed stress pattern, and of §4 Southern Lushootseed stress pattern. Each subsection under them is
    # about that dialect, even where it does not say so again.
    heading_dialect = [""]

    def flush():
        """Mine the prose gathered so far and start a new paragraph.

        The walk calls this wherever the prose stops: at a blank line, a page break or any structure. A paragraph is the
        unit because the dialect comes from it. A paragraph that names Southern and Northern both names them about
        different words and yields no dialect at all. Two paragraphs run together would make that true of the whole
        paper.
        """
        if paragraph:
            where, text = joined(paragraph)
            items.extend(
                from_prose(
                    where,
                    text,
                    "reference" if where == "References" else "",
                    heading_dialect[0],
                )
            )
            del paragraph[:]

    while at < len(lines):
        line = lines[at]
        trimmed = " ".join(line.split())
        if (not trimmed) or PAGE.match(trimmed):
            flush()
            at += 1
            continue

        found = APPENDIX.match(trimmed)
        if found:
            appendix = found.group(1)
        heading = HEADING.match(trimmed)
        if heading:
            flush()
            section = trimmed
            named = one_dialect(trimmed)
            if named:
                heading_dialect[0] = named
            elif APPENDIX.match(trimmed) or trimmed.startswith("References"):
                heading_dialect[0] = ""

        found = TABLE_CAPTION.match(trimmed)
        if found and (
            found.group(1) in TABLES
            or found.group(1) == "10"
            or found.group(1).lstrip("A").isdigit()
        ):
            number = found.group(1)
            number = number if number.startswith("A") else int(number)
            flush()
            taken.add(at)
            essay.append((at, "Table %s" % number, "essay", trimmed))
            rows, moved = table_rows(
                lines, at + 1, TABLES.get(number, (SOUTHERN,)), ENDS.get(number, "")
            )
            for one in range(at + 1, moved):
                taken.add(one)
                # The row's own line stays beside the forms taken out of it. Table A2 prints two Northern alternants for
                # ‘year’ in one cell, dᶻəlč̓/ǰəlč̓, and the forms come out separately. Without the line as printed, the
                # cell reads as never extracted.
                if " ".join(lines[one].split()):
                    essay.append(
                        (
                            one,
                            "Table %s" % number,
                            "essay",
                            " ".join(lines[one].split()),
                        )
                    )
            if number in TABLES:
                items.extend(from_table(number, rows))
            elif number == 10:
                items.extend(
                    from_paired_cells("Table 10", SOUTHERN, [one[0] for one in rows])
                )
            else:
                for whole, stripped, surface in rows:
                    items.append(("Table %s" % number, "", UNCLASSIFIED, stripped, ""))
            at = moved
            continue

        found = TABLEAU.match(trimmed)
        if found and FORM_GLOSS.search(trimmed):
            number = int(found.group(1))
            dialect = dialect_named(found.group(2))
            flush()
            taken.add(at)
            block = []
            moved = at + 1
            while moved < len(lines):
                ahead = lines[moved].rstrip()
                bare = ahead.strip()
                # Tableau (35) follows (34) with no blank line between them. A title ends a block the same way a blank
                # line does. Without this, the Northern derivation of ‘rock’ reads as four more candidates of the
                # Southern one.
                if (not bare) or PAGE.match(bare) or HEADING.match(bare):
                    break
                if TABLEAU.match(bare) and FORM_GLOSS.search(bare):
                    break
                block.append(ahead)
                taken.add(moved)
                moved += 1
            items.extend(from_tableau(number, dialect, trimmed, block))
            at = moved
            continue

        if (appendix == "A") and INTERLINEAR.match(trimmed):
            number = int(trimmed[1])
            flush()
            taken.add(at)
            block = []
            moved = at + 1
            while (moved < len(lines)) and (len(block) < 4):
                ahead = " ".join(lines[moved].split())
                moved += 1
                if (not ahead) or PAGE.match(ahead):
                    continue
                block.append(ahead)
                taken.add(moved - 1)
            items.extend(from_interlinear(number, block))
            at = moved
            continue

        if trimmed.startswith("•"):
            flush()
            taken.add(at)
            items.extend(
                from_bullet(
                    (
                        section.split()[0] + " " + section.split()[1]
                        if len(section.split()) > 1
                        else section
                    ),
                    trimmed,
                )
            )
            essay.append((at, section, "essay", trimmed))
            at += 1
            continue

        if trimmed.startswith("(1) a.") or trimmed.startswith("b. stə́gʷad"):
            flush()
            taken.add(at)
            items.extend(from_paired_cells("(1)", SOUTHERN, [trimmed]))
            essay.append((at, "(1)", "essay", trimmed))
            at += 1
            continue

        if trimmed.startswith("(18)"):
            flush()
            taken.add(at)
            block = []
            moved = at + 1
            while moved < len(lines):
                ahead = " ".join(lines[moved].split())
                if (not ahead) or PAGE.match(ahead) or not re.match(r"^[a-d]\.", ahead):
                    break
                block.append(ahead)
                taken.add(moved)
                moved += 1
            items.extend(from_comparison(block))
            essay.append((at, "(18)", "essay", trimmed))
            at = moved
            continue

        where = "References" if section.startswith("References") else section
        if paragraph and (paragraph[0][0] != where):
            flush()
        # The code mines prose a paragraph at a time and keeps it a line at a time. The paper breaks a citation across a
        # line as readily as anything else:
        #   - the root čə́bid ends one line, and its gloss ‘douglas fir’ opens the next.
        #   - both Figure 7 captions break sčəbíd-ac across the line at its hyphen.
        #   - the caption of Figure 5 names Southern on one line and Northern on the next, about one word.
        #
        # A read line by line misses the first two, and takes the third for a Southern form.
        paragraph.append((where, trimmed))
        essay.append((at, where, "essay", trimmed))
        at += 1

    flush()
    return items, essay


def joined(paragraph):
    """One paragraph's lines as a single string, with the words the line break split put together.

    A line ending in a hyphen was broken inside a word. It joins with nothing between. Every
    other line ends a word and joins with a space.
    """
    where = paragraph[0][0]
    out = ""
    for one, text in paragraph:
        if out.endswith("-"):
            out += text
        elif out:
            out += " " + text
        else:
            out = text
    return where, out


def marked(type, dialect, text):
    """One item written in the T and N convention, as language.layer.type:{text}.

    T marks Lushootseed. A cognate from Squamish, Twana, Sechelt or ʔayʔaǰuθəm is language, but not this language. It is
    N, and that keeps čalaš out of a Lushootseed corpus.
    """
    mark = "T" if (dialect in (SOUTHERN, NORTHERN)) else "N"
    return "%s.%s.%s:{%s}" % (mark, LAYER.get(type, DERIVED), type, text)


def switches_in(text):
    """How many times a line crosses between languages."""
    return max(0, len(tagged_spans(text)) - 1)


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
        lines = [repaired(one.rstrip("\n")) for one in handle]

    items, essay = read_paper(lines)

    rows = []
    for number, (where, dialect, type, form, gloss) in enumerate(items, 1):
        rows.append((number, where, dialect, type, form, gloss))
    for at, where, type, text in essay:
        rows.append((0, where, "", type, text, ""))

    missed = unreached(lines, covered_tokens(one[4] for one in rows))
    for page, where, reason, missing, text in missed:
        rows.append((0, "page %d" % page, "", UNCLASSIFIED, text, ""))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write(
            "# A Comparative Analysis of Stress in Northern and Southern Lushootseed.\n"
        )
        handle.write(
            "# Gloria Mellesmoen, University of British Columbia, and Ted Kye,\n"
        )
        handle.write("# University of Washington. ICSNL 61.\n")
        handle.write(
            "# Recordings of Martha Lamont, Northern, and Annie Jack, Southern, made by\n"
        )
        handle.write(
            "# Leon Metcalf in the 1950s and retained by the Burke Museum, with forms also\n"
        )
        handle.write(
            "# from Bates, Hess and Hilbert 1994, Hess 1977 and Bianco 1995.\n"
        )
        handle.write("#\n")
        handle.write(
            "# Mark is language.layer.type. T is Lushootseed, N is anything else, and a\n"
        )
        handle.write(
            "# cognate cited from Squamish, Twana, Sechelt or ʔayʔaǰuθəm is N.\n"
        )
        handle.write("#\n")
        handle.write(
            "# Only citation, surface and utterance are words of the language. A losing\n"
        )
        handle.write(
            "# tableau candidate is a form the paper's own analysis rejects, an\n"
        )
        handle.write(
            "# underlying form in slashes is an abstraction, and a starred form in\n"
        )
        handle.write(
            "# Appendix B is there to say the language does not have it. All three carry\n"
        )
        handle.write(
            "# the same characters as a real word and none reaches the pure\n"
        )
        handle.write("# stream.\n")
        handle.write("#\n")
        handle.write(
            "# The where column carries the paper's own locator, which this paper needs\n"
        )
        handle.write(
            "# and the narrative papers do not: its material is in fifteen tables and\n"
        )
        handle.write(
            "# sixteen tableaux, not in one running story, and Table 8 and Table 9\n"
        )
        handle.write(
            "# print the same gloss twice with a different form under each dialect.\n"
        )
        handle.write("line\twhere\tdialect\ttype\tswitches\tcontent\n")
        for number, where, dialect, type, text, gloss in rows:
            content = marked(type, dialect, text)
            handle.write(
                "line#${%d}\t%s\t%s\t%s\t%d\t%s\n"
                % (number, where, dialect, type, switches_in(text), content)
            )

    pure = TARGET[:-4] + ".pure.txt"
    by_dialect = {SOUTHERN: [], NORTHERN: []}
    retained = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for number, where, dialect, type, text, gloss in rows:
            if (type not in PURE) or (dialect not in by_dialect):
                continue
            key = " ".join(text.split())
            # A bound morpheme is a citation and is not a word. `-ac` ‘tree’, `-yalus` ‘edge’, `-il` and `dxʷ-` are all
            # real, and nobody ever said one of them on its own. They stay in the record and out of a corpus of things
            # people said.
            if key and ((key[0] in "-=") or (key[-1] in "-=")):
                continue
            if not key or (key in already):
                continue
            already.add(key)
            by_dialect[dialect].append(key)
            handle.write("%s\n" % key)
            retained += 1

    for dialect, held in by_dialect.items():
        with open(
            "%s.pure.%s.txt" % (TARGET[:-4], dialect), "w", encoding="utf-8", newline=""
        ) as handle:
            for one in held:
                handle.write("%s\n" % one)

    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [
        (0, one[1], UNKNOWN_KIND, "", one[4]) for one in rows if one[3] == UNCLASSIFIED
    ]
    flagged.extend(missed)
    # The one join that the space repair gets wrong. This names it, and no reader has to find it.
    flagged.append(
        (
            34,
            "App A",
            UNKNOWN_KIND,
            "ǰč",
            "the palatal series is cited in prose as /ǰ č č̓ š/ and the repair joined the "
            "first two, because ǰ ends in a caron with one space after it exactly as a "
            "broken word does",
        )
    )
    stuck_count = write_unsorted(stuck, "Mellesmoen and Kye, ICSNL 61", flagged)

    counted = {}
    for number, where, dialect, type, text, gloss in rows:
        counted[type] = counted.get(type, 0) + 1
    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write(
        "  %d target-language forms written to\n  %s\n" % (retained, os.path.basename(pure))
    )
    out.write(
        "    %d Southern, %d Northern\n"
        % (len(by_dialect[SOUTHERN]), len(by_dialect[NORTHERN]))
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
