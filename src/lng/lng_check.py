#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The three language tables held against the tree and against each other.

    python src/lng/lng_check.py

The tables are the oracle. A word the tree uses that no table holds is a defect, and so is a table
that contradicts itself: a complement that does not point back, a cross into the transpiler that
disagrees on width or sign, one transition given two mnemonics in one matrix. Each is printed and
the run exits 1. A finding is a cell worth a reader's eye and does not fail the run: two words of
one kind meaning the same operators, one word of two kinds, a doing word whose form writes none of
the operators it claims, an operator the form writes that no word of its name claims, an operator
the high order language has no word for, one mnemonic over transitions that do different things to
the pair.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SELECTED = [
    "selected_by_process",
    "selected_by_step",
    "selected_by_branch",
    "selected_by_fulltrace",
]
KINDS = {"thing", "doing", "gluing", "record", "ksc", "selection"}
HOL_KINDS = {"type", "plain", "core op"}
CHOSEN = {"industry", "base", "compound", "picked", "doug"}
COMPLEMENT = {
    "<": ">=",
    ">=": "<",
    "<=": ">",
    ">": "<=",
    "==": "!=",
    "!=": "==",
    "==0": "!=0",
    "!=0": "==0",
}
PUNCTUATORS = [
    "<<=",
    ">>=",
    "&&",
    "||",
    "<<",
    ">>",
    "<=",
    ">=",
    "==",
    "!=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "|=",
    "^=",
    "->",
    "++",
    "--",
    "+",
    "-",
    "*",
    "/",
    "%",
    "&",
    "|",
    "^",
    "<",
    ">",
    "=",
    "!",
    "~",
    "?",
    ":",
    "(",
    ")",
    "[",
    "]",
    "{",
    "}",
    ";",
    ",",
    ".",
]
FAMILY = {
    "==0": "==",
    "!=0": "!=",
    "<0": "<",
    "-x": "-",
    "~x": "~",
    "!x": "!",
    "?:": "?",
    "*p": "*",
    "*p=": "=",
}
CHECKED = {
    "+",
    "-",
    "*",
    "/",
    "%",
    "&",
    "|",
    "^",
    "<<",
    ">>",
    "<",
    "<=",
    ">",
    ">=",
    "==",
    "!=",
    "&&",
    "||",
    "~",
    "!",
    "?",
}
SEARCHED = ["src", "utils"]
SCHEMA = "src/cu/engine/rmc/machine_ir_types.h"
GNASCOR = "src/cu/transpiler/gnascor.md"
HOL_BITS = ("8", "16", "32", "64")
# the operators the high order language writes as C writes them, with no word of its own
HOL_AS_C = {
    "f()",
    "(T)",
    "&&",
    "||",
    "*p",
    "*p=",
    ".",
    "//",
    "T x",
    "const",
    "atomic()",
    "{}",
    "==0",
    "!=0",
    "<0",
    "?:",
    "if",
    "switch",
    "goto",
    "return",
    "label:",
}


def table_read(text):
    """The head and the rows of a tsv, each row a dict of the head's columns; the defects of its form."""
    lines = [one for one in text.split("\n") if one != ""]
    head = lines[0].split("\t")
    rows, defects = [], []
    for number, line in enumerate(lines[1:], 2):
        cells = line.split("\t")
        if len(cells) != len(head):
            defects.append("line %d holds %d cells where the head holds %d" % (number, len(cells), len(head)))
            continue
        rows.append(dict(zip(head, cells)))
    return head, rows, defects


def operators_of(head, row):
    """The operator columns a row marks."""
    start = head.index(SELECTED[-1]) + 1
    return frozenset(
        column
        for column in head[start:]
        if row.get(column) == "1" and column not in ("bits", "signed", "c_type", "complement", "transpiler")
    )


def words_of(name):
    """The words an identifier is compounded of."""
    return [one for one in name.split("_") if one != "" and not one.isdigit()]


def table_check(head, rows, kinds, key=("word", "kind")):
    """Defects of one table taken alone: a cell outside its column's values, a row held twice, a complement that does not
    point back or does not negate."""
    defects = []
    seen = set()
    for row in rows:
        held = tuple(row[column] for column in key)
        if held in seen:
            defects.append("{} is held twice".format(" ".join(held)))
        seen.add(held)
        if row["kind"] not in kinds:
            defects.append("{} has the kind {}, which no table names".format(row["word"], row["kind"]))
        if row["chosen_by"] not in CHOSEN:
            defects.append("{} is chosen by {}, which no table names".format(row["word"], row["chosen_by"]))
        for column in head[head.index(SELECTED[-1]) + 1 :]:
            if column in ("bits", "signed", "c_type", "complement", "transpiler"):
                continue
            if row[column] not in ("", "1"):
                defects.append(
                    "{} holds {} under {}, where an operator cell holds 1 or nothing".format(
                        row["word"], row[column], column
                    )
                )
    by_word = {row["word"]: row for row in rows if row.get("complement", "") != ""}
    for word, row in by_word.items():
        other = by_word.get(row["complement"])
        if other is None or other["complement"] != word:
            defects.append("{} complements {}, which does not complement it back".format(word, row["complement"]))
            continue
        mine, theirs = operators_of(head, row), operators_of(head, other)
        if {COMPLEMENT.get(one) for one in mine} != set(theirs):
            defects.append(
                "{} complements {}, and {} does not negate {}".format(
                    word,
                    other["word"],
                    " ".join(sorted(theirs)),
                    " ".join(sorted(mine)),
                )
            )
    return defects


def table_findings(head, rows):
    """Findings of one table taken alone: two words of one kind marking the same operators and the same width, sign and
    type, and one word held as two kinds."""
    findings = []
    groups = {}
    for row in rows:
        operators = operators_of(head, row)
        if not operators:
            continue
        held = (
            row["kind"],
            operators,
            row.get("bits", ""),
            row.get("signed", ""),
            row.get("c_type", ""),
        )
        groups.setdefault(held, []).append(row["word"])
    for held, words in sorted(groups.items(), key=lambda item: item[1]):
        if len(words) > 1:
            findings.append(
                "{} are {} words that all mean {}".format(", ".join(words), held[0], " ".join(sorted(held[1])))
            )
    kinds = {}
    for row in rows:
        kinds.setdefault(row["word"], []).append(row)
    for word, held in sorted(kinds.items()):
        if len(held) > 1:
            findings.append(
                "{} is {}".format(
                    word,
                    " and ".join(
                        "{} meaning {}".format(
                            row["kind"],
                            " ".join(sorted(operators_of(head, row))) or "no operator",
                        )
                        for row in held
                    ),
                )
            )
    return findings


def schema_forms(text):
    """The form names the schema lists."""
    return re.findall(r'form_\([A-Z0-9_]+, "([a-z0-9_]+)"', text)


def ruleset_words(path, text):
    """Every word a ruleset uses, each with where it is used: the record a line opens with, the name it gives, its
    parameter names, and the forms a construct body calls."""
    used = {}

    def use(name, where):
        for word in words_of(name):
            used.setdefault(word, where)

    inside = False
    for number, line in enumerate(text.split("\n"), 1):
        if line == "" or line.startswith("#"):
            continue
        where = "%s:%d" % (path, number)
        tokens = line.split("=", 1)[0].split() if line.split()[0] in ("form", "err", "nop") else line.split()
        if inside and tokens[0] != "end":
            use(tokens[0], where)
            for token in tokens[1:]:
                for name in re.findall(r"\{([a-z_]+)(?::\d+)?\}|^([a-z_]+)$", token):
                    use(name[0] or name[1], where)
            continue
        use(tokens[0], where)
        if tokens[0] in ("form", "err", "nop", "construct"):
            for token in tokens[1:]:
                use(token, where)
        elif tokens[0] in ("bank", "fixed", "header") and len(tokens) > 1:
            use(tokens[1], where)
        inside = tokens[0] == "construct" or (inside and tokens[0] != "end")
    return used


def ruleset_lines(text):
    """The lines of a .kdm or a .ksc a ruleset reads, every other line left blank so each keeps its number."""
    kept = []
    for line in text.split("\n"):
        kept.append(line if line.split()[:1] in (["bank"], ["fixed"], ["form"], ["err"], ["nop"]) else "")
    return "\n".join(kept)


def ksc_words(path, text):
    """Every word a ksc uses: the record a line opens with, the channel and class names, and the folds a compile line
    records with the parameter each is put for."""
    used = {}
    for number, line in enumerate(text.split("\n"), 1):
        if line == "" or line.startswith("#"):
            continue
        where = "%s:%d" % (path, number)
        tokens = line.split()
        for word in words_of(tokens[0]):
            used.setdefault(word, where)
        if tokens[0] in ("channel", "class", "count", "compile"):
            for word in words_of(tokens[1]):
                used.setdefault(word, where)
        if tokens[0] == "count":
            used.setdefault(tokens[2], where)
        if tokens[0] == "compile":
            for fold, put in re.findall(r"folds the (\S+) put for (\S+)", line):
                for word in words_of(fold) + words_of(put) + words_of(tokens[3].rstrip(":")):
                    used.setdefault(word, where)
    return used


def gnascor_lists(text):
    """The words gnascor's word map lists, by the kind it lists them under."""
    lists = {}
    things = text[text.index("**Thing words.**") : text.index("**Doing words.**")]
    for line in things.splitlines():
        if line.startswith("| ") and "---" not in line and "| kind |" not in line:
            for word in re.findall(r"`([a-z_]+)`", line.split("|")[2]):
                lists.setdefault(word, set()).add("thing")
    doing = text[text.index("**Doing words.**") : text.index("**Gluing words.**")]
    for word in re.findall(r"`([a-z_]+)`", doing.split("\n")[0]):
        lists.setdefault(word, set()).add("doing")
    gluing = text[text.index("**Gluing words.**") :].split("\n")[0]
    for word in re.findall(r"`([a-z_]+)`", gluing.split(".")[1]):
        lists.setdefault(word, set()).add("gluing")
    return lists


def c_operators(text):
    """The checked operators a C text writes, its literals and names stripped."""
    text = re.sub(r"//[^\\]*", "", text.replace("\\n", "\n"))
    text = re.sub(
        r"\{[a-z_]+(?::\d+)?\}|[A-Za-z_][A-Za-z_0-9]*|0x[0-9a-fA-F]+u?|\d+(?:ull|ul|u|ll|l)?",
        " ",
        text,
    )
    found = set()
    at = 0
    while at < len(text):
        if text[at].isspace():
            at += 1
            continue
        for punctuator in PUNCTUATORS:
            if text.startswith(punctuator, at):
                found.add(
                    punctuator.rstrip("=")
                    if punctuator.endswith("=") and len(punctuator) > 1 and punctuator not in ("<=", ">=", "==", "!=")
                    else punctuator
                )
                at += len(punctuator)
                break
        else:
            at += 1
    return {one for one in found if one in CHECKED}


def form_findings(head, rows, forms):
    """Each form of cu.krs held against the operators the words of its name claim: a doing word none of whose checked
    operators the form writes, and a checked operator the form writes that no word of its name claims."""
    findings = []
    claims = {}
    for row in rows:
        claims.setdefault(row["word"], set()).update(operators_of(head, row))
    for name, text in forms:
        words = words_of(name)
        claimed = set()
        for word in words:
            claimed.update(FAMILY.get(one, one) for one in claims.get(word, ()))
        written = c_operators(text)
        for word in words:
            checked = {FAMILY.get(one, one) for one in claims.get(word, ())} & CHECKED
            doing = any(row["word"] == word and row["kind"] == "doing" for row in rows)
            if doing and checked and not checked & written:
                findings.append(
                    "{} claims {} through {}, and its form writes none of it".format(
                        name, " ".join(sorted(checked)), word
                    )
                )
        unclaimed = written - claimed
        if unclaimed:
            findings.append("{} writes {}, which no word of its name claims".format(name, " ".join(sorted(unclaimed))))
    return findings


def hol_check(transpiler_rows, hol_head, hol_rows, transpiler_head):
    """Defects of the cross: a high order type whose transpiler word is missing or disagrees on width or sign, and a
    transpiler register class of a power-of-two width with no high order type at that width. Findings: the operators
    the transpiler means that no high order word means."""
    defects, findings = [], []
    things = {row["word"]: row for row in transpiler_rows if row["kind"] == "thing"}
    words = {row["word"] for row in transpiler_rows}
    for row in hol_rows:
        if row["transpiler"] == "":
            continue
        other = things.get(row["transpiler"])
        if row["transpiler"] not in words:
            defects.append(
                "{} crosses to {}, which the transpiler holds no word for".format(row["word"], row["transpiler"])
            )
        elif other is not None and (other["bits"], other["signed"]) != (
            row["bits"],
            row["signed"],
        ):
            defects.append(
                "{} is {} bits signed {} and crosses to {}, which is {} bits signed {}".format(
                    row["word"],
                    row["bits"],
                    row["signed"],
                    other["word"],
                    other["bits"],
                    other["signed"],
                )
            )
    held = {(row["bits"], row["signed"]) for row in hol_rows if row["kind"] == "type"}
    for word, row in sorted(things.items()):
        if row["bits"] in HOL_BITS and (row["bits"], row["signed"]) not in held:
            findings.append(
                "the transpiler's {} is {} bits signed {}, and no high order type is".format(
                    word, row["bits"], row["signed"]
                )
            )
    hol_means = set()
    for row in hol_rows:
        hol_means |= operators_of(hol_head, row)
    transpiler_means = {}
    for row in transpiler_rows:
        for operator in operators_of(transpiler_head, row):
            transpiler_means.setdefault(operator, []).append(row["word"])
    for operator, words in sorted(transpiler_means.items()):
        if operator not in hol_means and operator not in HOL_AS_C:
            findings.append(
                "the high order language has no word for {}, which the transpiler means by {}".format(
                    operator, ", ".join(sorted(set(words)))
                )
            )
    ops = {}
    for row in hol_rows:
        if row["kind"] == "core op":
            for operator in operators_of(hol_head, row):
                ops.setdefault(operator, set()).add(row["word"])
    for row in transpiler_rows:
        if row["kind"] != "doing":
            continue
        for operator in sorted(operators_of(transpiler_head, row)):
            if operator in ops and row["word"] not in ops[operator]:
                findings.append(
                    "the transpiler names {} {} where the core op is {}".format(
                        operator, row["word"], ", ".join(sorted(ops[operator]))
                    )
                )
    return defects, findings


MIRROR = {"lead": "rite", "rite": "lead"}


def arrow_of(row):
    """The arrow a transition's bits give: *> where a side holds no pair bit, the unknown or a superstate; x> where both
    sides change, a cross; -> where they do not cross, a straight line. A branch pair is no transition and takes none.
    """
    if row["table"] == "the branch pair":
        return ""
    if "-" in (row["left"], row["right"]):
        return "*>"
    if row["left"] != "=" and row["right"] != "=":
        return "x>"
    return "->"


def asm_check(rows):
    """Defects: one transition given two mnemonics in one table, and an arrow its bits do not give. Findings: one
    transition given two mnemonics by two tables, one mnemonic over transitions that do different things to the pair,
    a transition whose mirror, lead and rite trading places, is named otherwise in the same table, and a mnemonic that
    is also a state's name."""
    defects, findings = [], []
    for row in rows:
        if row.get("arrow", arrow_of(row)) != arrow_of(row):
            defects.append(
                "{} to {} in {} is written {} and its bits give {}".format(
                    row["past"], row["present"], row["table"], row["arrow"] or "no arrow", arrow_of(row) or "none"
                )
            )
    states = {row["past"] for row in rows} | {row["present"] for row in rows}
    for mnemonic in sorted({row["mnemonic"] for row in rows} & states):
        findings.append(
            "{} is a state and also the mnemonic of {}".format(
                mnemonic,
                ", ".join(
                    "{} to {} in {}".format(row["past"], row["present"], row["table"])
                    for row in rows
                    if row["mnemonic"] == mnemonic
                ),
            )
        )
    named = {}
    for row in rows:
        named.setdefault((row["table"], row["past"], row["present"]), set()).add(row["mnemonic"])
    for (table, past, present), mnemonics in sorted(named.items()):
        if len(mnemonics) > 1:
            defects.append("{} names {} to {} both {}".format(table, past, present, " and ".join(sorted(mnemonics))))
    across = {}
    for (table, past, present), mnemonics in named.items():
        if table in (
            "the state-transition matrix",
            "the high-energy transition matrix",
        ):
            across.setdefault((past, present), {})[table] = mnemonics
    for (past, present), tables in sorted(across.items()):
        names = set().union(*tables.values())
        if len(tables) > 1 and len(names) > 1 and "sync" not in names:
            findings.append(
                "{} to {} is {}".format(
                    past,
                    present,
                    " in one matrix and ".join(
                        "{}".format(" ".join(sorted(tables[table]))) for table in sorted(tables)
                    ),
                )
            )
    doing = {}
    for row in rows:
        if "-" not in (row["left"], row["right"]):
            doing.setdefault(row["mnemonic"], set()).add((row["left"], row["right"]))
    for mnemonic, pairs in sorted(doing.items()):
        if len(pairs) > 1:
            findings.append(
                "{} does {} to the pair".format(
                    mnemonic,
                    ", ".join("left {} right {}".format(*pair) for pair in sorted(pairs)),
                )
            )
    for (table, past, present), mnemonics in sorted(named.items()):
        mirror = (table, MIRROR.get(past, past), MIRROR.get(present, present))
        if (
            mirror != (table, past, present)
            and mirror in named
            and named[mirror] != mnemonics
            and (past, present) < mirror[1:]
        ):
            findings.append(
                "{} names {} to {} {} and its mirror {} to {} {}".format(
                    table,
                    past,
                    present,
                    " ".join(sorted(mnemonics)),
                    mirror[1],
                    mirror[2],
                    " ".join(sorted(named[mirror])),
                )
            )
    return defects, findings


def files_ending(suffix):
    """Every file of the searched trees ending in suffix, as a path from the root."""
    found = []
    for top in SEARCHED:
        for directory, _, names in os.walk(os.path.join(ROOT, top)):
            for name in names:
                if name.endswith(suffix):
                    found.append(os.path.relpath(os.path.join(directory, name), ROOT).replace(os.sep, "/"))
    return sorted(found)


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as file:
        return file.read()


def main():
    defects, findings = [], []
    head, rows, broken = table_read(read("src/lng/transpiler_lng.tsv"))
    defects += broken + table_check(head, rows, KINDS)
    findings += table_findings(head, rows)
    hol_head, hol_rows, broken = table_read(read("src/lng/gnascor_hol_lng.tsv"))
    defects += broken + table_check(hol_head, hol_rows, HOL_KINDS)
    findings += table_findings(hol_head, hol_rows)
    _asm_head, asm_rows, broken = table_read(read("src/lng/gnascor_asm_lng.tsv"))
    defects += broken

    held = {row["word"] for row in rows}
    used = {}
    used_names = schema_forms(read(SCHEMA))
    for name in used_names:
        for word in words_of(name):
            used.setdefault(word, SCHEMA + " " + name)
    for path in files_ending(".krs"):
        for word, where in ruleset_words(path, read(path)).items():
            used.setdefault(word, where)
    for path in files_ending(".ksc"):
        for word, where in ksc_words(path, read(path)).items():
            used.setdefault(word, where)
    for path in files_ending(".ksc") + files_ending(".kdm"):
        for word, where in ruleset_words(path, ruleset_lines(read(path))).items():
            used.setdefault(word, where)
    for word, where in sorted(used.items()):
        if word not in held:
            defects.append(f"{word} is used at {where} and no table holds it")
    listed = gnascor_lists(read(GNASCOR))
    for word, kinds in sorted(listed.items()):
        for kind in sorted(kinds):
            if not any(row["word"] == word and row["kind"] == kind for row in rows):
                defects.append(f"gnascor lists {word} as a {kind} word and the table does not")
    names = set(used_names)
    for row in rows:
        if (
            row["kind"] in ("thing", "doing", "gluing")
            and row["word"] not in used
            and not any(("_" + row["word"] + "_") in ("_" + name + "_") for name in names)
        ):
            findings.append("{} is a {} word nothing in the tree uses".format(row["word"], row["kind"]))

    forms = []
    for line in read("src/cu/transpiler/lstar/coherence/cu.krs").split("\n"):
        if line.startswith("form "):
            name, text = line[len("form ") :].split("=", 1)
            forms.append((name.split()[0], text))
    findings += form_findings(head, rows, forms)
    more_defects, more_findings = hol_check(rows, hol_head, hol_rows, head)
    defects += more_defects
    findings += more_findings
    more_defects, more_findings = asm_check(asm_rows)
    defects += more_defects
    findings += more_findings

    for defect in defects:
        print("defect: " + defect)
    for finding in findings:
        print("finding: " + finding)
    print(
        "%d defects, %d findings, %d words, %d high order words, %d transitions"
        % (len(defects), len(findings), len(rows), len(hol_rows), len(asm_rows))
    )
    return 1 if defects else 0


if __name__ == "__main__":
    sys.exit(main())
