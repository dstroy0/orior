#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Grade src/lng/lng_check.py on tables whose defects and findings were worked out by hand.
#
#   python utils/test/src/lng/lng_check_test.py
#
# Every expectation below was written by reading the input instead of by running the tool. Each check has
# a clean input it must pass and a broken one it must catch: a check that has never caught anything
# cannot be told apart from one that catches nothing.

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "src", "lng"))

import lng_check

HEAD = (
    ["word", "kind", "names", "chosen_by"]
    + lng_check.SELECTED
    + ["bits", "signed", "c_type", "complement", "<", ">=", "+", "&", "&&", "-"]
)
FAILURES = []


def line(*cells):
    return "\t".join(cells)


def table(*rows):
    return "\n".join([line(*HEAD)] + [line(*row) for row in rows]) + "\n"


def blank(
    word,
    kind,
    chosen_by="base",
    bits="",
    signed="",
    c_type="",
    complement="",
    operators=(),
):
    return [
        word,
        kind,
        "",
        chosen_by,
        "",
        "",
        "",
        "",
        bits,
        signed,
        c_type,
        complement,
    ] + ["1" if operator in operators else "" for operator in ["<", ">=", "+", "&", "&&", "-"]]


def expect(label, got, wanted):
    if got != wanted:
        FAILURES.append(f"{label}: got {got!r}, wanted {wanted!r}")


CLEAN = table(
    blank("below", "doing", complement="geq", operators=["<"]),
    blank("geq", "doing", complement="below", operators=[">="]),
    blank("add", "doing", operators=["+"]),
    blank("and", "doing", operators=["&"]),
    blank("word", "thing", "industry", "32", "0", "unsigned int"),
)
head, rows, defects = lng_check.table_read(CLEAN)
expect("a clean table reads whole", (len(rows), defects), (5, []))
expect(
    "a clean table holds no defect",
    lng_check.table_check(head, rows, lng_check.KINDS),
    [],
)
expect("a clean table holds no finding", lng_check.table_findings(head, rows), [])

head, rows, defects = lng_check.table_read(CLEAN + "short\tthing\n")
expect("a short row is a defect", defects, ["line 7 holds 2 cells where the head holds 18"])

BROKEN = table(
    blank("below", "doing", complement="geq", operators=["<"]),
    blank("geq", "doing", complement="add", operators=[">="]),
    blank("add", "doing", operators=["+"]),
    blank("add", "doing", operators=["+"]),
    blank("word", "noun", "guessed"),
)
head, rows, _ = lng_check.table_read(BROKEN)
expect(
    "each defect of a broken table",
    lng_check.table_check(head, rows, lng_check.KINDS),
    [
        "add doing is held twice",
        "word has the kind noun, which no table names",
        "word is chosen by guessed, which no table names",
        "below complements geq, which does not complement it back",
        "geq complements add, which does not complement it back",
    ],
)

NOT_NEGATED = table(
    blank("below", "doing", complement="geq", operators=["<"]),
    blank("geq", "doing", complement="below", operators=["+"]),
)
head, rows, _ = lng_check.table_read(NOT_NEGATED)
expect(
    "a complement that does not negate",
    lng_check.table_check(head, rows, lng_check.KINDS),
    [
        "below complements geq, and + does not negate <",
        "geq complements below, and < does not negate +",
    ],
)

TWINS = table(
    blank("subtract", "doing", operators=["-"]),
    blank("borrow", "doing", operators=["-"]),
    blank("and", "doing", operators=["&"]),
    blank("and", "gluing", operators=["&&"]),
)
head, rows, _ = lng_check.table_read(TWINS)
expect(
    "synonyms and a homonym",
    lng_check.table_findings(head, rows),
    [
        "subtract, borrow are doing words that all mean -",
        "and is doing meaning & and gluing meaning &&",
    ],
)

expect(
    "words of a compound",
    lng_check.words_of("test_signed_word_below_and"),
    ["test", "signed", "word", "below", "and"],
)
expect("words skip numbers", lng_check.words_of("probe_sm_86"), ["probe", "sm"])

expect(
    "operators of a C text",
    lng_check.c_operators("    {to} = ({left} != 0u) && ({right} << 3u) + {also}[2];\\n"),
    {"!=", "&&", "<<", "+"},
)
expect(
    "a compound assignment is its operator",
    lng_check.c_operators("    {to} -= {left} & 0xffu;\\n"),
    {"-", "&"},
)
expect("a note writes nothing", lng_check.c_operators("// a + b\\n"), set())

head, rows, _ = lng_check.table_read(CLEAN)
expect(
    "a form whose name matches its text",
    lng_check.form_findings(head, rows, [("word_add", "    {to} = {left} + {right};\\n")]),
    [],
)
expect(
    "a form whose name and text disagree",
    lng_check.form_findings(head, rows, [("word_add", "    {to} = {left} & {right};\\n")]),
    [
        "word_add claims + through add, and its form writes none of it",
        "word_add writes &, which no word of its name claims",
    ],
)

HOL_HEAD = ["word", "kind", "names", "chosen_by"] + lng_check.SELECTED + ["bits", "signed", "transpiler", "<", "+"]


def hol_table(*rows):
    return "\n".join([line(*HOL_HEAD)] + [line(*row) for row in rows]) + "\n"


TRANSPILER = table(
    blank("word", "thing", "industry", "32", "0", "unsigned int"),
    blank("add", "doing", operators=["+"]),
)
head, rows, _ = lng_check.table_read(TRANSPILER)
hol_head, hol_rows, _ = lng_check.table_read(
    hol_table(
        ["u32_t", "type", "", "doug", "", "", "", "", "32", "0", "word", "", ""],
        ["plus", "plain", "", "doug", "", "", "", "", "", "", "add", "", "1"],
    )
)
expect("a cross that agrees", lng_check.hol_check(rows, hol_head, hol_rows, head), ([], []))
hol_head, hol_rows, _ = lng_check.table_read(
    hol_table(
        ["i32_t", "type", "", "doug", "", "", "", "", "32", "1", "word", "", ""],
        ["u16_t", "type", "", "doug", "", "", "", "", "16", "0", "halfword", "", ""],
    )
)
expect(
    "a cross that disagrees and a gap",
    lng_check.hol_check(rows, hol_head, hol_rows, head),
    (
        [
            "i32_t is 32 bits signed 1 and crosses to word, which is 32 bits signed 0",
            "u16_t crosses to halfword, which the transpiler holds no word for",
        ],
        [
            "the transpiler's word is 32 bits signed 0, and no high order type is",
            "the high order language has no word for +, which the transpiler means by add",
        ],
    ),
)

hol_head, hol_rows, _ = lng_check.table_read(
    hol_table(
        ["add", "core op", "", "industry", "", "", "", "", "", "", "", "", "1"],
        ["lt", "core op", "", "industry", "", "", "", "", "", "", "", "1", ""],
        ["u32_t", "type", "", "doug", "", "", "", "", "32", "0", "word", "", ""],
    )
)
head, rows, _ = lng_check.table_read(
    table(
        blank("word", "thing", "industry", "32", "0", "unsigned int"),
        blank("add", "doing", operators=["+"]),
        blank("below", "doing", operators=["<"]),
    )
)
expect(
    "a doing word named apart from its core op",
    lng_check.hol_check(rows, hol_head, hol_rows, head),
    ([], ["the transpiler names < below where the core op is lt"]),
)

ASM_HEAD = [
    "mnemonic",
    "table",
    "past",
    "present",
    "past left",
    "past right",
    "present left",
    "present right",
    "left",
    "right",
]


def asm_rows(*rows):
    return lng_check.table_read("\n".join([line(*ASM_HEAD)] + [line(*row) for row in rows]) + "\n")[1]


MATRIX = "the state-transition matrix"
expect(
    "a matrix with nothing to find",
    lng_check.asm_check(
        asm_rows(
            ["core", MATRIX, "lead", "lead", "1", "0", "1", "0", "=", "="],
            ["core", MATRIX, "rite", "rite", "0", "1", "0", "1", "=", "="],
        )
    ),
    ([], []),
)
expect(
    "each thing a matrix gets wrong",
    lng_check.asm_check(
        asm_rows(
            ["core", MATRIX, "lead", "lead", "1", "0", "1", "0", "=", "="],
            ["surv", MATRIX, "rite", "rite", "0", "1", "0", "1", "=", "="],
            ["drop", MATRIX, "dual", "lead", "1", "1", "1", "0", "=", "0"],
            ["drop", MATRIX, "dual", "void", "1", "1", "0", "0", "0", "0"],
            ["halt", MATRIX, "dual", "void", "1", "1", "0", "0", "0", "0"],
            ["wait", MATRIX, "busy", "wait", "-", "-", "-", "-", "-", "-"],
        )
    ),
    (
        ["the state-transition matrix names dual to void both drop and halt"],
        [
            "wait is a state and also the mnemonic of busy to wait in the state-transition matrix",
            "drop does left 0 right 0, left = right 0 to the pair",
            "the state-transition matrix names lead to lead core and its mirror rite to rite surv",
        ],
    ),
)

ARROWED = ASM_HEAD + ["arrow"]


def arrowed_rows(*rows):
    return lng_check.table_read("\n".join([line(*ARROWED)] + [line(*row) for row in rows]) + "\n")[1]


expect(
    "each arrow its bits give",
    lng_check.asm_check(
        arrowed_rows(
            ["pass", MATRIX, "lead", "rite", "1", "0", "0", "1", "0", "1", "x>"],
            ["drop", MATRIX, "lead", "void", "1", "0", "0", "0", "0", "=", "->"],
            ["sync", MATRIX, "busy", "dual", "-", "-", "1", "1", "-", "-", "*>"],
            ["core", "the branch pair", "lead", "void", "1", "", "", "0", "-", "-", ""],
        )
    ),
    ([], []),
)
expect(
    "arrows their bits do not give",
    lng_check.asm_check(
        arrowed_rows(
            ["pass", MATRIX, "lead", "rite", "1", "0", "0", "1", "0", "1", "->"],
            ["drop", MATRIX, "lead", "void", "1", "0", "0", "0", "0", "=", "x>"],
            ["core", "the branch pair", "lead", "void", "1", "", "", "0", "-", "-", "*>"],
        )
    )[0],
    [
        "lead to rite in the state-transition matrix is written -> and its bits give x>",
        "lead to void in the state-transition matrix is written x> and its bits give ->",
        "lead to void in the branch pair is written *> and its bits give none",
    ],
)

for failure in FAILURES:
    print("FAIL " + failure)
print("%d failures" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
