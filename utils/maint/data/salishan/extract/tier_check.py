"""Check the tiers of a paper's examples in its oracle.

usage: python tier_check.py <stem> [stem ...]

residue.py checks that every row's text is the paper's; it cannot see a row given the wrong kind,
a context line read as the transcription and every tier under it shifted one down. This does, for
the rows of numbered examples, by three tests, each run only where the paper keeps the convention
it rests on (most of its rows do):

    a gloss row holds a grammatical label in capitals (DET, 3POSS, D2), and a transcription or a
    segmentation row holds none;
    a translation opens on a quote;
    a segmentation and the gloss under it hold about as many morpheme breaks, - and =.

It prints each row that breaks a test, alone. A row it prints is a row to look at: a
gloss line of lexical words alone, there, keeps no capitals, and a loan in capitals, TV, stands in
a segmentation.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

LABEL = re.compile(r"^((?:footnote \S+ )?(?:§\S+ )?\((?:\d+|[ivx]+)[a-z]*\)) line \d+$")
# The share of a paper's rows that must keep a convention before its test is run.
KEPT = 0.8
# A label of capitals and a digit right after a clitic's or an affix's join, =D2 and =V2 in
# Sardinha's papers, and cut-INST1 in McKay's, its subscript set inline. A V1 standing free in a
# gloss row's prose is no label.
DIGIT_LABEL = re.compile(r"(?<=[=\-])[A-Z]{1,5}[1-9](?=$|[\s=\-.])")
# A reduplication template a segmentation prints before its base, CV~ and CVC~.
TEMPLATE = re.compile(r"(?:^|(?<=\s))[CV]+~")


def labeled(gloss):
    """Whether a gloss row holds a label: one gen.is_gloss finds, or a capital and a digit."""
    return gen.is_gloss(gloss) or bool(DIGIT_LABEL.search(gloss))


def passes(gloss):
    """Whether a gloss row passes the test of labels, where the paper keeps it: a labeled row, one
    whose label a reduplicant's boundary bounds as a hyphen does, DIM•one and PL•black•INCH in
    Mellesmoen's infix paper, or a gloss of one plain word, one under paʔa in its (13). Whether the
    paper keeps the convention is still decided by labeled() alone."""
    # A label behind a star's parenthesis, come to=here (*NMLZ-)John in H. Davis's proper names, is
    # bounded as one behind a plain parenthesis; a gloss of one word can join its English with
    # stops, go.home under úxwal’ in the same paper.
    # Two labels a colon joins as one morpheme's, club-TR:REFL in Turner's (9c), are bounded as a
    # hyphen bounds them.
    return labeled(gloss) or labeled(gloss.replace("•", "-")) or labeled(gloss.replace("(*", "(")) or \
        labeled(gloss.replace(":", "-")) or bool(re.fullmatch(r"[a-z]+(?:\.[a-z]+)*", gloss))


def breaks(form):
    return form.count("-") + form.count("=")


def check(stem):
    path = os.path.join(gen.ORACLES, stem + ".oracle.tsv")
    rows = []
    for line in open(path, encoding="utf-8").read().split("\n")[1:]:
        if line:
            where, who, kind, form = line.split("\t")[:4]
            if LABEL.match(where):
                rows.append((where, kind, form))
    glosses = [form for _, kind, form in rows if kind == "gloss"]
    translations = [form for _, kind, form in rows if kind == "translation"]
    # A translation may open on the closing quote where the page misprints it, U+2019(S)he/they/we
    # hunted you (SG).’ in Sobolak's (7), or on a font's turned comma, ʻHe₁ loves Bill₁ʼs mother.ʼ in
    # Cable's (52a).
    quoted = re.compile(r"^[?#*]*\s*[‘’“ʻ'\"]")
    # A translation under a label in straight quotes, Intended: 'I bought the white one. ' in
    # Forbes's adjectives, stands as LABELED_QUOTE's label stands over curly ones.
    # Its label may open lower-case, attempt at: ‘The child almost got lost.’ in Turner's (45).
    named = re.compile(gen.LABELED_QUOTE.pattern.replace("‘", "[‘']").replace("[A-Z][a-z]+", "[A-Za-z][a-z]+"))
    caps = glosses and sum(map(labeled, glosses)) >= KEPT * len(glosses)
    quotes = translations and sum(bool(quoted.match(one) or named.match(one))
                                  for one in translations) >= KEPT * len(translations)
    faults = []
    for index, (where, kind, form) in enumerate(rows):
        bare = TEMPLATE.sub("", form) if kind == "segmentation" else form
        # A sentence wrapped to a second pair of lines, stéxw=t’u7 q’ix. over really hard in
        # Davis's count-mass paper, can gloss its tail in lexical words alone under a labeled
        # gloss of the same example.
        label = LABEL.match(where).group(1)
        wrapped = index >= 2 and [one[1] for one in rows[index - 2:index]] == ["gloss", "transcription"] and \
            all(LABEL.match(one[0]).group(1) == label for one in rows[index - 2:index]) and \
            passes(rows[index - 2][2])
        # A gloss of lexical morphemes alone under a form with as many breaks, shut-mouth under
        # /qəmč̓-uθin/ in Mellesmoen and Andreotti's (1a).
        lexical = kind == "gloss" and index >= 1 and re.fullmatch(r"[a-z.]+(?:[-=][a-z.]+)+", form) and \
            rows[index - 1][1] in ("segmentation", "phonemic") and breaks(rows[index - 1][2]) == breaks(form) and \
            LABEL.match(rows[index - 1][0]).group(1) == label
        # A root glossed with a suffix of unknown meaning, (do-?) under chá-nem in Jacobs's footnote 11.
        lexical = lexical or kind == "gloss" and re.fullmatch(r"\(?[a-z.]+(?:-\?)+\)?", form)
        # An orthography written in capitals, the SENĆOŦEN alphabet's YÁ, SEN DOQ in Turner's (1a),
        # sets a transcription with no lower-case letter over a segmentation of the example, bar a
        # name, EWES U, SETKT ŦE Katie. in Turner's (53), and the possessive s, ŦE WÁĆs in its (7).
        bare_names = re.sub(r"(?<=\w)s\b", "", re.sub(r"\b[A-Z][a-z]+\b", "", form))
        capitals = kind == "transcription" and bare_names == bare_names.upper() and index + 1 < len(rows) and \
            rows[index + 1][1] == "segmentation" and LABEL.match(rows[index + 1][0]).group(1) == label
        if caps and kind == "gloss" and not passes(form) and not wrapped and not lexical:
            faults.append((where, kind, form, "a gloss with no label in capitals"))
        elif caps and kind in ("transcription", "segmentation") and gen.is_gloss(bare) and not capitals:
            faults.append((where, kind, form, "a form tier holding a label in capitals"))
        elif quotes and kind == "translation" and not (quoted.match(form) or named.match(form)):
            faults.append((where, kind, form, "a translation with no opening quote"))
        if kind == "segmentation" and index + 1 < len(rows):
            below = rows[index + 1]
            same = LABEL.match(below[0]).group(1) == LABEL.match(where).group(1)
            if same and below[1] == "gloss" and abs(breaks(form) - breaks(below[2])) > 1:
                faults.append((where, kind, form, "%d breaks over a gloss with %d" % (breaks(form), breaks(below[2]))))
    return faults


if __name__ == "__main__":
    for stem in sys.argv[1:]:
        for where, kind, form, why in check(stem):
            print("%s | %s | %s | %s | %s" % (stem, where, kind, why, form[:80]))
