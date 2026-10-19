// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng_check_test.c: src/lng/lng.c graded on tables whose defects and findings were worked out by hand.
//
//     utils/test/src/lng/lng_check_test.sh
//
// Every expectation below was written by reading the input instead of by running the tool. Each check has a clean
// input it must pass and a broken one it must catch: a check that has never caught anything cannot be told apart from
// one that catches nothing.
#include "../../../../src/lng/lng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_failures;

// the operator columns of the test's tables, in their order
static const char *const s_operators[6] = {"<", ">=", "+", "&", "&&", "-"};

// `got` held against the `count` texts at `wanted`, a failure printed under `label` where they differ
static void expect(const char *label, const LngList *got, const char *const *wanted, unsigned int count)
{
    int same = (got->count == count);
    for (unsigned int at = 0u; same && (at < count); at += 1u)
    {
        same = (strcmp(got->text[at], wanted[at]) == 0);
    }
    if (same)
    {
        return;
    }
    s_failures += 1u;
    printf("FAIL %s: got", label);
    for (unsigned int at = 0u; at < got->count; at += 1u)
    {
        printf(" [%s]", got->text[at]);
    }
    printf(", wanted");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        printf(" [%s]", wanted[at]);
    }
    printf("\n");
}

// `got` held against `wanted`, a failure printed under `label` where they differ
static void expect_count(const char *label, unsigned int got, unsigned int wanted)
{
    if (got != wanted)
    {
        s_failures += 1u;
        printf("FAIL %s: got %u, wanted %u\n", label, got, wanted);
    }
}

// a row of the transpiler's test table, its operator cells 1 for each of `operators`, `count` of them, apart by tabs,
// kept after `lines`
static void blank(LngList *lines, const char *word, const char *kind, const char *bits, const char *sign,
                  const char *c_type, const char *complement, const char *const *operators, unsigned int count)
{
    char cells[6][2] = {"", "", "", "", "", ""};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            cells[at][0] = (strcmp(s_operators[at], operators[one]) == 0) ? '1' : cells[at][0];
        }
    }
    lng_list_printed(lines, "%s\t%s\t\t\t\t\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", word, kind, bits, sign, c_type,
                     complement, cells[0], cells[1], cells[2], cells[3], cells[4], cells[5]);
}

// the head of the transpiler's test table kept as the first of `lines`
static void head(LngList *lines)
{
    lng_list_add(lines, "word\tkind\tnames\tselected_by_process\tselected_by_step\tselected_by_branch\t"
                        "selected_by_fulltrace\tbits\tsigned\tc_type\tcomplement\t<\t>=\t+\t&\t&&\t-");
}

// `lines` read as a table into `table`, the defects of its form kept after `defects`, and `lines` let go
static void table_of(LngList *lines, LngTable *table, LngList *defects)
{
    lng_table_free(table);
    lng_table_read((const char *const *)lines->text, lines->count, table, defects);
    lng_list_free(lines);
}

// the clean table: kept after a head in `lines`
static void clean(LngList *lines)
{
    static const char *const s_below[1] = {"<"};
    static const char *const s_geq[1] = {">="};
    static const char *const s_add[1] = {"+"};
    static const char *const s_and[1] = {"&"};
    head(lines);
    blank(lines, "below", "doing", "", "", "", "geq", s_below, 1u);
    blank(lines, "geq", "doing", "", "", "", "below", s_geq, 1u);
    blank(lines, "add", "doing", "", "", "", "", s_add, 1u);
    blank(lines, "and", "doing", "", "", "", "", s_and, 1u);
    blank(lines, "word", "thing", "32", "0", "unsigned int", "", NULL, 0u);
}

// a row of a high order test table, its cells apart by tabs, kept after `lines`
static void hol_row(LngList *lines, const char *word, const char *kind, const char *bits, const char *sign,
                    const char *crossed, const char *below, const char *plus)
{
    lng_list_printed(lines, "%s\t%s\t\t\t\t\t\t%s\t%s\t%s\t%s\t%s", word, kind, bits, sign, crossed, below, plus);
}

// the head of a high order test table kept as the first of `lines`
static void hol_head(LngList *lines)
{
    lng_list_add(lines, "word\tkind\tnames\tselected_by_process\tselected_by_step\tselected_by_branch\t"
                        "selected_by_fulltrace\tbits\tsigned\ttranspiler\t<\t+");
}

// the head of a matrix test table kept as the first of `lines`, with the arrow column where `arrowed`
static void asm_head(LngList *lines, int arrowed)
{
    lng_list_printed(lines,
                     "mnemonic\ttable\tpast\tpresent\tpast left\tpast right\tpresent left\tpresent right\tleft\t"
                     "right%s",
                     arrowed ? "\tarrow" : "");
}

// a row of a matrix test table, its `count` cells apart by tabs, kept after `lines`
static void asm_row(LngList *lines, const char *const *cells, unsigned int count)
{
    LngList row = {NULL, 0u, 0u};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        lng_list_add(&row, cells[at]);
    }
    char *const joined = lng_list_joined(&row, "\t");
    lng_list_add(lines, joined);
    free(joined);
    lng_list_free(&row);
}

int main(void)
{
    LngList lines = {NULL, 0u, 0u};
    LngList defects = {NULL, 0u, 0u};
    LngList findings = {NULL, 0u, 0u};
    LngTable table = {{NULL, 0u, 0u}, NULL, 0u, 0u};

    clean(&lines);
    table_of(&lines, &table, &defects);
    expect_count("a clean table reads whole", table.rows, 5u);
    expect("a clean table reads whole", &defects, NULL, 0u);
    lng_table_check(&table, g_lng_kinds, LNG_KINDS, &defects);
    expect("a clean table holds no defect", &defects, NULL, 0u);
    lng_table_findings(&table, &findings);
    expect("a clean table holds no finding", &findings, NULL, 0u);

    clean(&lines);
    lng_list_add(&lines, "short\tthing");
    table_of(&lines, &table, &defects);
    static const char *const s_short[1] = {"line 7 holds 2 cells where the head holds 17"};
    expect("a short row is a defect", &defects, s_short, 1u);
    lng_list_free(&defects);

    {
        static const char *const s_below[1] = {"<"};
        static const char *const s_geq[1] = {">="};
        static const char *const s_add[1] = {"+"};
        head(&lines);
        blank(&lines, "below", "doing", "", "", "", "geq", s_below, 1u);
        blank(&lines, "geq", "doing", "", "", "", "add", s_geq, 1u);
        blank(&lines, "add", "doing", "", "", "", "", s_add, 1u);
        blank(&lines, "add", "doing", "", "", "", "", s_add, 1u);
        blank(&lines, "word", "noun", "", "", "", "", NULL, 0u);
        table_of(&lines, &table, &defects);
        lng_table_check(&table, g_lng_kinds, LNG_KINDS, &defects);
        static const char *const s_wanted[4] = {
            "add doing is held twice",
            "word has the kind noun, which no table names",
            "below complements geq, which does not complement it back",
            "geq complements add, which does not complement it back",
        };
        expect("each defect of a broken table", &defects, s_wanted, 4u);
        lng_list_free(&defects);
    }

    {
        static const char *const s_below[1] = {"<"};
        static const char *const s_plus[1] = {"+"};
        head(&lines);
        blank(&lines, "below", "doing", "", "", "", "geq", s_below, 1u);
        blank(&lines, "geq", "doing", "", "", "", "below", s_plus, 1u);
        table_of(&lines, &table, &defects);
        lng_table_check(&table, g_lng_kinds, LNG_KINDS, &defects);
        static const char *const s_wanted[2] = {
            "below complements geq, and + does not negate <",
            "geq complements below, and < does not negate +",
        };
        expect("a complement that does not negate", &defects, s_wanted, 2u);
        lng_list_free(&defects);
    }

    {
        static const char *const s_minus[1] = {"-"};
        static const char *const s_and[1] = {"&"};
        static const char *const s_both[1] = {"&&"};
        head(&lines);
        blank(&lines, "subtract", "doing", "", "", "", "", s_minus, 1u);
        blank(&lines, "borrow", "doing", "", "", "", "", s_minus, 1u);
        blank(&lines, "and", "doing", "", "", "", "", s_and, 1u);
        blank(&lines, "and", "gluing", "", "", "", "", s_both, 1u);
        table_of(&lines, &table, &defects);
        lng_table_findings(&table, &findings);
        static const char *const s_wanted[2] = {
            "subtract, borrow are doing words that all mean -",
            "and is doing meaning & and gluing meaning &&",
        };
        expect("synonyms and a homonym", &findings, s_wanted, 2u);
        lng_list_free(&findings);
    }

    {
        LngList words = {NULL, 0u, 0u};
        lng_words_of("test_signed_word_below_and", &words);
        static const char *const s_compound[5] = {"test", "signed", "word", "below", "and"};
        expect("words of a compound", &words, s_compound, 5u);
        lng_list_free(&words);
        lng_words_of("probe_sm_86", &words);
        static const char *const s_numbered[2] = {"probe", "sm"};
        expect("words skip numbers", &words, s_numbered, 2u);
        lng_list_free(&words);
    }

    {
        LngList operators = {NULL, 0u, 0u};
        lng_c_operators("    {to} = ({left} != 0u) && ({right} << 3u) + {also}[2];\\n", &operators);
        static const char *const s_written[4] = {"!=", "&&", "+", "<<"};
        expect("operators of a C text", &operators, s_written, 4u);
        lng_list_free(&operators);
        lng_c_operators("    {to} -= {left} & 0xffu;\\n", &operators);
        static const char *const s_compound[2] = {"&", "-"};
        expect("a compound assignment is its operator", &operators, s_compound, 2u);
        lng_list_free(&operators);
        lng_c_operators("// a + b\\n", &operators);
        expect("a note writes nothing", &operators, NULL, 0u);
        lng_list_free(&operators);
    }

    {
        clean(&lines);
        table_of(&lines, &table, &defects);
        LngList names = {NULL, 0u, 0u};
        LngList texts = {NULL, 0u, 0u};
        lng_list_add(&names, "word_add");
        lng_list_add(&texts, "    {to} = {left} + {right};\\n");
        lng_form_findings(&table, &names, &texts, &findings);
        expect("a form whose name matches its text", &findings, NULL, 0u);
        lng_list_free(&texts);
        lng_list_add(&texts, "    {to} = {left} & {right};\\n");
        lng_form_findings(&table, &names, &texts, &findings);
        static const char *const s_wanted[2] = {
            "word_add claims + through add, and its form writes none of it",
            "word_add writes &, which no word of its name claims",
        };
        expect("a form whose name and text disagree", &findings, s_wanted, 2u);
        lng_list_free(&findings);
        lng_list_free(&texts);
        lng_list_free(&names);
    }

    LngTable hol = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    {
        static const char *const s_add[1] = {"+"};
        head(&lines);
        blank(&lines, "word", "thing", "32", "0", "unsigned int", "", NULL, 0u);
        blank(&lines, "add", "doing", "", "", "", "", s_add, 1u);
        table_of(&lines, &table, &defects);
        hol_head(&lines);
        hol_row(&lines, "u32_t", "type", "32", "0", "word", "", "");
        hol_row(&lines, "plus", "plain", "", "", "add", "", "1");
        table_of(&lines, &hol, &defects);
        lng_hol_check(&table, &hol, &defects, &findings);
        expect("a cross that agrees", &defects, NULL, 0u);
        expect("a cross that agrees", &findings, NULL, 0u);

        hol_head(&lines);
        hol_row(&lines, "i32_t", "type", "32", "1", "word", "", "");
        hol_row(&lines, "u16_t", "type", "16", "0", "halfword", "", "");
        table_of(&lines, &hol, &defects);
        lng_hol_check(&table, &hol, &defects, &findings);
        static const char *const s_crossed[2] = {
            "i32_t is 32 bits signed 1 and crosses to word, which is 32 bits signed 0",
            "u16_t crosses to halfword, which the transpiler holds no word for",
        };
        static const char *const s_gaps[2] = {
            "the transpiler's word is 32 bits signed 0, and no high order type is",
            "the high order language has no word for +, which the transpiler means by add",
        };
        expect("a cross that disagrees and a gap", &defects, s_crossed, 2u);
        expect("a cross that disagrees and a gap", &findings, s_gaps, 2u);
        lng_list_free(&defects);
        lng_list_free(&findings);
    }

    {
        static const char *const s_add[1] = {"+"};
        static const char *const s_below[1] = {"<"};
        hol_head(&lines);
        hol_row(&lines, "add", "core op", "", "", "", "", "1");
        hol_row(&lines, "lt", "core op", "", "", "", "1", "");
        hol_row(&lines, "u32_t", "type", "32", "0", "word", "", "");
        table_of(&lines, &hol, &defects);
        head(&lines);
        blank(&lines, "word", "thing", "32", "0", "unsigned int", "", NULL, 0u);
        blank(&lines, "add", "doing", "", "", "", "", s_add, 1u);
        blank(&lines, "below", "doing", "", "", "", "", s_below, 1u);
        table_of(&lines, &table, &defects);
        lng_hol_check(&table, &hol, &defects, &findings);
        static const char *const s_wanted[1] = {"the transpiler names < below where the core op is lt"};
        expect("a doing word named apart from its core op", &defects, NULL, 0u);
        expect("a doing word named apart from its core op", &findings, s_wanted, 1u);
        lng_list_free(&findings);
    }

    static const char s_matrix[] = "the state-transition matrix";
    LngTable matrix = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    {
        static const char *const s_core[10] = {"core", s_matrix, "lead", "lead", "1", "0", "1", "0", "=", "="};
        static const char *const s_rite[10] = {"core", s_matrix, "rite", "rite", "0", "1", "0", "1", "=", "="};
        asm_head(&lines, 0);
        asm_row(&lines, s_core, 10u);
        asm_row(&lines, s_rite, 10u);
        table_of(&lines, &matrix, &defects);
        lng_asm_check(&matrix, &defects, &findings);
        expect("a matrix with nothing to find", &defects, NULL, 0u);
        expect("a matrix with nothing to find", &findings, NULL, 0u);
    }

    {
        static const char *const s_rows[6][10] = {
            {"core", s_matrix, "lead", "lead", "1", "0", "1", "0", "=", "="},
            {"surv", s_matrix, "rite", "rite", "0", "1", "0", "1", "=", "="},
            {"drop", s_matrix, "dual", "lead", "1", "1", "1", "0", "=", "0"},
            {"drop", s_matrix, "dual", "void", "1", "1", "0", "0", "0", "0"},
            {"halt", s_matrix, "dual", "void", "1", "1", "0", "0", "0", "0"},
            {"wait", s_matrix, "busy", "wait", "-", "-", "-", "-", "-", "-"},
        };
        asm_head(&lines, 0);
        for (unsigned int row = 0u; row < 6u; row += 1u)
        {
            asm_row(&lines, s_rows[row], 10u);
        }
        table_of(&lines, &matrix, &defects);
        lng_asm_check(&matrix, &defects, &findings);
        static const char *const s_wanted_defects[1] = {
            "the state-transition matrix names dual to void both drop and halt"};
        static const char *const s_wanted_findings[3] = {
            "wait is a state and also the mnemonic of busy to wait in the state-transition matrix",
            "drop does left 0 right 0, left = right 0 to the pair",
            "the state-transition matrix names lead to lead core and its mirror rite to rite surv",
        };
        expect("each thing a matrix gets wrong", &defects, s_wanted_defects, 1u);
        expect("each thing a matrix gets wrong", &findings, s_wanted_findings, 3u);
        lng_list_free(&defects);
        lng_list_free(&findings);
    }

    {
        static const char *const s_rows[4][11] = {
            {"pass", s_matrix, "lead", "rite", "1", "0", "0", "1", "0", "1", "x>"},
            {"drop", s_matrix, "lead", "void", "1", "0", "0", "0", "0", "=", "->"},
            {"sync", s_matrix, "busy", "dual", "-", "-", "1", "1", "-", "-", "*>"},
            {"core", "the branch pair", "lead", "void", "1", "", "", "0", "-", "-", ""},
        };
        asm_head(&lines, 1);
        for (unsigned int row = 0u; row < 4u; row += 1u)
        {
            asm_row(&lines, s_rows[row], 11u);
        }
        table_of(&lines, &matrix, &defects);
        lng_asm_check(&matrix, &defects, &findings);
        expect("each arrow its bits give", &defects, NULL, 0u);
        expect("each arrow its bits give", &findings, NULL, 0u);
    }

    {
        static const char *const s_rows[3][11] = {
            {"pass", s_matrix, "lead", "rite", "1", "0", "0", "1", "0", "1", "->"},
            {"drop", s_matrix, "lead", "void", "1", "0", "0", "0", "0", "=", "x>"},
            {"core", "the branch pair", "lead", "void", "1", "", "", "0", "-", "-", "*>"},
        };
        asm_head(&lines, 1);
        for (unsigned int row = 0u; row < 3u; row += 1u)
        {
            asm_row(&lines, s_rows[row], 11u);
        }
        table_of(&lines, &matrix, &defects);
        lng_asm_check(&matrix, &defects, &findings);
        static const char *const s_wanted[3] = {
            "lead to rite in the state-transition matrix is written -> and its bits give x>",
            "lead to void in the state-transition matrix is written x> and its bits give ->",
            "lead to void in the branch pair is written *> and its bits give none",
        };
        expect("arrows their bits do not give", &defects, s_wanted, 3u);
        lng_list_free(&defects);
        lng_list_free(&findings);
    }

    {
        // a .ksc of the ksc's own lines, a who line and a block of the ruleset's language: the ksc reads its own
        // lines and the ruleset reads the block's, each line keeping its number
        static KfileLines s_file;
        static const char *const s_lines[7] = {
            "channel probe_sm",
            "who 00000000000000000000000000000000",
            "__krs__(",
            "form word_add w = {to} = {left} + {right};",
            "bank wide 8",
            ")__krs__",
            "count probe_sm three 3",
        };
        for (unsigned int at = 0u; at < 7u; at += 1u)
        {
            kfile_lines_line_add(&s_file, s_lines[at]);
        }
        LngMap used = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
        lng_ksc_words("x.ksc", &s_file, &used);
        static const char *const s_ksc_words[5] = {"channel", "count", "probe", "sm", "three"};
        static const char *const s_ksc_where[5] = {"x.ksc:1", "x.ksc:7", "x.ksc:1", "x.ksc:1", "x.ksc:7"};
        expect("the ksc reads its own lines", &used.key, s_ksc_words, 5u);
        expect("the ksc reads its own lines", &used.value, s_ksc_where, 5u);
        lng_map_free(&used);
        LngList kept = {NULL, 0u, 0u};
        lng_ruleset_lines(&s_file, &kept);
        static const char *const s_kept[7] = {"", "", "", "form word_add w = {to} = {left} + {right};", "bank wide 8",
                                              "", ""};
        expect("the ruleset reads the block's lines", &kept, s_kept, 7u);
        lng_ruleset_words("x.ksc", (const char *const *)kept.text, kept.count, &used);
        static const char *const s_ruleset_words[6] = {"add", "bank", "form", "w", "wide", "word"};
        static const char *const s_ruleset_where[6] = {"x.ksc:4", "x.ksc:5", "x.ksc:4",
                                                       "x.ksc:4", "x.ksc:5", "x.ksc:4"};
        expect("the ruleset reads the block's lines", &used.key, s_ruleset_words, 6u);
        expect("the ruleset reads the block's lines", &used.value, s_ruleset_where, 6u);
        lng_map_free(&used);
        lng_list_free(&kept);
        kfile_lines_free(&s_file);
    }

    lng_table_free(&matrix);
    lng_table_free(&hol);
    lng_table_free(&table);
    printf("%u failures\n", s_failures);
    return (s_failures != 0u) ? 1 : 0;
}
