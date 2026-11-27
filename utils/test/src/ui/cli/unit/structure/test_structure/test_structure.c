// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_structure.c: structural compare: tokens are names, numbers, strings and signs, layout alone is no change, a moved line reads
// as moved and a name changed everywhere as renamed, a moved block takes its closing sign along, and a name changed two ways is not
// renamed
#include "ui/cli/src/code/structure.h"
#include "ui/cli/src/code/structure_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_layout_then[] = "f(a, b);\nreturn x;\n";
static const char s_layout_now[] = "f(\n    a,\n    b\n);\n\nreturn   x;\n";
static const char s_rename_then[] = "fn one() {}\nfn two() {}\nlet count = count + 1;\nprint(count);\n";
static const char s_rename_now[] = "fn two() {}\nlet total = total + 1;\nprint(total);\nfn one() {}\n";
static const char s_block_then[] = "function one() {\n  return 1;\n}\nfunction two() {\n  return 2;\n}\n";
static const char s_block_now[] = "function two() {\n  return 2;\n}\nfunction one() {\n  return 1;\n}\n";
static const char s_token_then[] = "let a = 1;\n";
static const char s_token_now[] = "let a = 2;\n";

static CliSide s_side;
static char s_out[1u << 16u];
static const CliJson *s_json;

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_tokens_are_names_numbers_strings_and_signs(void)
{
    static const char s_text[] = "let x_1 = 3.5 + \"a \\\" b\";\n  \xC3\xA9(y)";
    static const char *const s_requested[9] = {"let", "x_1", "=", "3.5", "+", "\"a \\\" b\"", ";", "\xC3\xA9", "("};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_text, .size = sizeof(s_text) - 1u, .side = &s_side));
    TEST_ASSERT_EQUAL_UINT(11u, s_side.tokens);
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        CLI_TEST_ASSERT_TEXT(s_requested[at], s_text + s_side.token[at].start, s_side.token[at].end - s_side.token[at].start);
    }
}

// More names than a run of the names holds, so that the naming merges its runs: 600 lines, each a name of its own and
// a sign. The same text named twice gives each token and line the number of the first naming, and each name a number
// of its own
void test_the_names_of_many_tokens_and_lines_hold_through_the_merges(void)
{
    enum
    {
        MANY = 600u
    };
    static char s_many[MANY * 16u];
    static CliStructureState s_state;
    static CliSide s_old;
    static CliSide s_now;
    static unsigned char s_seen[(CLI_STRUCTURE_NAMES / 2u) + 1u];
    VerbaLinea line = {s_many, sizeof(s_many), 0ULL};
    for (unsigned int at = 0u; at < MANY; at += 1u)
    {
        mmgr_line_put(&line, "n");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, ";\n");
    }
    const size_t size = mmgr_line_finish(&line);
    g_cli_structure = &s_state;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_many, .size = size, .side = &s_old));
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_many, .size = size, .side = &s_now));
    TEST_ASSERT_EQUAL_UINT(2u * MANY, s_old.tokens);
    TEST_ASSERT_TRUE(cli_structure_by_name(&(CliStructureOldCtx){.old = &s_old, .now = &s_now}));
    for (unsigned int at = 0u; at < s_old.tokens; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_old.token_name[at], s_now.token_name[at]);
        // each name a number of its own, and every sign the one number of the sign
        const unsigned int name = s_old.token_name[at];
        TEST_ASSERT_TRUE((at % 2u) == 1u ? (name == s_old.token_name[1]) : (s_seen[name] == 0u));
        s_seen[name] = 1u;
    }
    for (unsigned int at = 0u; at < s_old.lines; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_old.line_name[at], s_now.line_name[at]);
        for (unsigned int before = 0u; before < at; before += 1u)
        {
            TEST_ASSERT_NOT_EQUAL(s_old.line_name[before], s_old.line_name[at]);
        }
    }
    g_cli_structure = NULL;
}

void test_each_token_is_positioned_in_utf16_on_its_line(void)
{
    static const char s_text[] = "let x_1 = 3.5 + \"a \\\" b\";\n  \xC3\xA9(y)";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_text, .size = sizeof(s_text) - 1u, .side = &s_side));
    const CliToken *const last = &s_side.token[s_side.tokens - 1u];
    TEST_ASSERT_EQUAL_UINT(1u, last->line);
    TEST_ASSERT_EQUAL_UINT(5u, last->from);
    TEST_ASSERT_EQUAL_UINT(6u, last->to);
}

