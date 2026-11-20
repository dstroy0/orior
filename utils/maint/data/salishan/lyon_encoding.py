#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Turn the TeX font's encoding back into the orthography, as far as it goes.
#
#   Usage:  from lyon_encoding import drafted
#
# 19-Lyon_ICSNL50_final-78 and 2013_Lindley_Lyon use NimbusRomNo9L and TeX-xipa, with a custom encoding and no ToUnicode
# map. What pypdf passes back is glyph codes read as ASCII. Page 1 of Lindley prints `q̓sápi ɬaʔ ct̓ʕapənwíxʷ`, and the
# text holds `’qsápi ìaP c’tQap@nwíxw`.
#
# WHAT THIS IS AND IS NOT
#
# It is a draft. Each rule below comes from a rendered page, and holds on the pages checked. The output must still pass
# a check against the page before it counts as the paper. It exists because a retype of 148 pages by hand brings its own
# errors. A draft that a person corrects is more accurate than a page typed from nothing.
#
# WHAT IT CANNOT DO
#
# w is two characters. Page `kʷukʷ` and page `wist` both arrive as w, and nothing in the text separates them. The rule
# below labializes a w that follows one of the consonants that take it. That is right for kʷ, qʷ, xʷ and x̌ʷ, and wrong
# wherever a real w follows a consonant.
#
# Word boundaries are the other one. The PDF puts a space in front of a character that carries a mark. `s ’plá ’ks@lx`
# is one word, `iP ’kl` is two, and both of them are a space in front of a marked character. Page 25 settles the first
# as `sp̓lák̓səlx`, and page 24 the second as `iʔ k̓l`. `wa’y` and `Lyon’s` are the same case, with the space absent
# instead of inserted.
#
# Those are the sites to read first on any page. They are why this file is not a repair in papers.py. The code applies a
# repair and trusts it, and this needs a check.

import re
import unicodedata

# The marks that the extraction prints in front of the character that they belong over, against the diacritic that each
# one is. The mark ’ is glottalization, and is the majority of them.
#
# The mark ´ occurs only where the font has no precomposed character to carry the accent. The vowels á é í ó ú arrive
# whole, and the schwa does not. `Lasy´@t` is the `lasyə́t` of the page, and is the only word in this paper that needs
# it.
MOVED = {"’": "̓", "´": "́"}

# What the glyph codes are for, one for one, wherever they appear in a Salish token. ; was in this
# table until níkmən. Showed that it is not one for one: see lengthened().
CHARACTERS = (
    ("@", "ə"),
    ("ì", "ɬ"),
    ("Q", "ʕ"),
    ("ň", "ƛ"),
)

# The wedge arrives before its character as well, and only ever sits on x here.
WEDGE = (("ˇx", "x̌"),)

# One place where the inserted space is not a question. The mark √ opens a root on a parse line, and the root follows it
# immediately. It never ends a word, and a space after it is always the PDF's. The roots that start with a marked
# character get one: `√ q̓ʷʕay=lqs` is `√q̓ʷʕay=lqs` on the page.
AFTER_ROOT = ("√ ", "√")

# The consonants that a w after them labializes. A w after anything else, or at the front of a word, is the character w:
# `wist`, `wa’y`, `nwíwpəm`.
LABIALIZED = "kqxgɣʕǰč"


def moved_marks(token):
    """One token with each spacing mark carried onto the character that it sat in front of.

    Only a token that salish() answered for comes here. In the English the same ’ is an apostrophe and follows its
    character, and a move of it turns `Lyon’s` into `Lyons̓`.
    """
    out = []
    at = 0
    while at < len(token):
        symbol = token[at]
        combining = MOVED.get(symbol)
        if (
            (combining is not None)
            and ((at + 1) < len(token))
            and token[at + 1].isalpha()
        ):
            out.append(token[at + 1])
            out.append(combining)
            at += 2
            continue
        out.append(symbol)
        at += 1
    return "".join(out)


# The characters of the extraction that only the language uses. A token that holds one of them is Salish. These are the
# codes as they arrive, and not what they become. A list of ə ɬ ʕ ƛ would never meet the test, and `s’tmQa’lt` would
# come through with its ejective marks still in front of their characters.
#
# The first two characters are the wedge. drafted() runs WEDGE over the whole line before any of this. A wedge that was
# on an x is a caron diacritic by the time the test reads it. The standalone ˇ catches one that was anywhere else. The
# pair as the string "x̌" would put a bare x in the set, and make every English word that holds one Salish.
#
# The mark √ opens a root on a parse line, and appears in no English word. A token that carries one is Salish. Without
# it, `in_ks_√’ma` comes through with the mark still in front of the m. That is the majority of the parse lines in the
# five-line format.
MARKS = "̌ˇ@ìQň·√áéíóú"

# A gloss token is plain ASCII, apart from two things.
#
# The first is the ligatures that the PDF sets its f-words with, where the whole gloss of one morpheme is one token.
# `Go-n-dip.ﬂuid-MID-3SG.POSS` is a single string, and the one ﬂ in it would keep it out of this class. The test would
# then read it as Salish, and its POSS would come out as ʔOSS.
#
# The second is the reduplication mark, which the gloss line of the five-line format uses as well as the parse line.
# Without it, `C1C2.PL•speak-CAUS` comes out as `C1C2.ʔL•speak-CAUS`.
#
# The gloss line of the five-line format also carries the morpheme boundaries + and =. Without them, `+get.read-APPL`
# comes out as `+get.read-AʔʔL`, and `+C1C2.PL•son+DRV` as `+C1C2.ʔL•son+DRV`.
#
# The mark √ stays out. It opens a root on the parse line, appears on no gloss line, and a parse line is Salish. The run
# of two capitals that LABEL asks for keeps a parse token out of this class anyway. A parse is in lower case, and a
# gloss is not.
GLOSS = re.compile(r"^[A-Za-z0-9.()\[\]/,;:'•+=ﬁﬂﬀﬃﬄ-]+$")
LABEL = re.compile(r"[A-Z]{2}")


def a_gloss(token):
    """Whether a token belongs to a gloss line, where its P is the P of RECIP and PASS.

    A gloss is plain ASCII and carries a run of two or more capitals: RECIP, 3PL.ABS, NOM-son-RED.
    One capital on its own is not enough, because that is also iP and Philosophical.
    """
    return bool(GLOSS.match(token)) and bool(LABEL.search(token))


def salish(token):
    """Whether a token is in the language's font. Its P is a glottal stop, and its ’ a mark.

    P and ’ are the two that the encoding cannot decide by itself. P is ʔ throughout the Salish and a capital P
    throughout the English, and both sit on one line. COMP and RECIP head the gloss lines, while Philosophical and
    Penticton run through the notes. The ’ is the ejective mark in the Salish, and the apostrophe in `Lyon’s` and
    `Society’s`.

    A token that carries any other mark of the orthography is Salish. So is one with a P that is not the first
    character, as `iP`, `smsámaP` and `nPaysənúlaPxw` are and Philosophical is not.

    This misses two things. A P-initial Salish word with no other mark on it, as `Pitx`, `Pamn` and `Pasil` are, comes
    through as English. So does a word whose ’ has no inserted space in front of it. `wa’y` and `Lyon’s` are then the
    same shape, and only the page tells them apart. Both are among the first things to look for on a page.
    """
    if a_gloss(token):
        return False
    # Qu that opens a token is the English digraph, as in Quilchena and Queen. Q is the pharyngeal everywhere else, at
    # the front of `Qant` and `QapnáP` too. The test asks for a tail with no other mark in it, and only the English pair
    # comes out. A test for a plain ASCII tail would miss `Quilchena.’` at the end of a translation, where the closing
    # quote is not ASCII. A Salish word that opens ʕu and carries no other mark would read as English here, and neither
    # paper holds one.
    if (token[:2] == "Qu") and not any((mark in token[1:]) for mark in MARKS):
        return False
    # An address, where @ is the at sign. The last line of the paper is `john.lyon@alumni.ubc.ca`, and without this it
    # would get a schwa in the middle. A Salish token is not plain ASCII once its @ counts, and not one holds a dot.
    if token.isascii() and ("@" in token) and ("." in token):
        return False
    if any((mark in token) for mark in MARKS):
        return True
    # A ’ that opens a token is the mark that waits for its character: `’ti` is `t̓i`. One that follows a character is
    # the apostrophe of `Lyon’s` and `Society’s`.
    #
    # An English elision opens with one too. Lyon writes `’til` in the translation of stanza 137, and it comes out as
    # `t̓il`, the same shape as `’ti`. Inside one token, nothing can tell it from that. It is the only one in this
    # paper.
    if token.startswith("’"):
        return True
    # A glottal stop located alone, left by a break the PDF put in front of it, as ixí P is.
    if token == "P":
        return True
    return "P" in token[1:]


def lengthened(line):
    """One line with ; read as the length mark where the character that it lengthens follows it.

    Lyon writes length with a raised dot, and the font gives it the code of the semicolon. `Ya;Qt` is `ya·ʕt`, and
    `’qsá;;;pi` is `q̓sá···pi`. Lyon also ends a clause with a semicolon, and the page prints
    `ník ’m@n; iP k ’wúl ’m@ns`. There a map with no question makes a word of `níkmən·`.

    This runs over the whole line, before the split into tokens, because the mark survives the inserted space and the
    test must see across it. `qw@mí;; ’wt` is one word: a space follows the run, and then the ’ that the space sits in
    front of. `níkmən;` has a space and a plain character after it, and the sentence goes on.
    """
    out = []
    at = 0
    while at < len(line):
        if line[at] != ";":
            out.append(line[at])
            at += 1
            continue
        end = at
        while (end < len(line)) and (line[end] == ";"):
            end += 1
        after = line[end : end + 2]
        # The character that it lengthens comes next, and one of two things can sit between. One is a spacing mark that
        # waits for that character, as `’kwu;’l-s` has. The other is an inserted space in front of that mark, as
        # `qw@mí;; ’wt` has. A space with a plain character after it is the sentence that goes on, and that keeps
        # `níkmən; iP` apart.
        carries = bool(after) and (
            after[0].isalpha()
            or (after[0] in MOVED)
            or ((after[0] == " ") and (len(after) > 1) and (after[1] in MOVED))
        )
        out.append(("·" * (end - at)) if carries else line[at:end])
        at = end
    return "".join(out)


# What the look-back steps over on its way to the consonant. Lyon brackets a segment under reconstruction, and puts a
# restored one in parentheses, and the page labializes across both. `[k̓]w` is `[k̓]ʷ` in stanza 176, and `n-t̓ək̓[w]`
# is `n-t̓ək̓[ʷ]` in stanza 6. The look-back also steps over a diacritic, because the diacritic sits on the consonant
# that labializes.
TRANSPARENT = "[]()"


def labialized(line):
    """One line with w read as ʷ where it follows a consonant that takes labialization."""
    out = []
    for symbol in line:
        if (symbol == "w") and out:
            at = len(out) - 1
            while (at >= 0) and (
                (out[at] in TRANSPARENT) or unicodedata.combining(out[at])
            ):
                at -= 1
            if (at >= 0) and (out[at].lower() in LABIALIZED):
                out.append("ʷ")
                continue
        out.append(symbol)
    return "".join(out)


def drafted(line):
    """One line of the extraction as a draft of what the page says, for a check against the page.

    Token by token, because a line mixes the two. One gloss line reads `COMP DET native.person`, and the transcription
    above it reads `ɬaʔ iʔ sqilxʷ`, and the two want opposite answers about P and ’.

    The wedge goes first, over the whole line. The ˇx is x̌ in the Salish and occurs nowhere else, and a token needs it
    to look Salish to the test that follows.

    The length mark goes over the whole line too, for the same reason. lengthened() must see across the inserted space
    to tell `qw@mí;; ’wt` from `níkmən; iP`.

    CHARACTERS runs on a Salish token only. Its codes are ordinary characters elsewhere, and ; shows it. Lyon ends a
    clause with a semicolon in the English, and a map of the whole line with no question would turn
    `long ago over there; we came` into `over there· we came`.

    labialized runs over the whole joined line. That is a known defect, and the two paragraphs below give its cost on
    both sides. Line-wide, it reads the w of an English gloss as labial wherever a k, a q or an x is in front of it.
    `Backwards` drafts as `backʷards`, and `he.fell.off.backwards` as `he.fell.off.backʷards`. The check then reports
    the hand extraction as in disagreement with the paper over a word of English.

    A gate on salish() is worse. It drops far more of the ʷ that the pages print than it saves, and the disagreements of
    the tree rise several times over. The test salish() asks for a mark of the orthography or a medial P, and `kwukw`
    carries neither. The tokens that most need the labial mark are exactly the ones that the gate turns off.

    a_gloss() does not separate them either: it wants a run of two capitals, and `backwards` has none. What actually
    separates them is the line, not the token, because the five-line interlinear puts glosses on a line of their own. A
    decision there changes what this draft says a page reads. It waits on the pages, and not on a heuristic.
    """
    for before, after in WEDGE:
        line = line.replace(before, after)
    line = line.replace(*AFTER_ROOT)
    line = lengthened(line)
    held = []
    for token in line.split(" "):
        if salish(token):
            # CHARACTERS first. moved_marks carries a mark onto the character after it, and asks whether that character
            # is one. The @ is not one: `lasy´@t` keeps its acute in front until the schwa is a schwa.
            for before, after in CHARACTERS:
                token = token.replace(before, after)
            token = moved_marks(token).replace("P", "ʔ")
        held.append(token)
    return unicodedata.normalize("NFC", labialized(" ".join(held)))
