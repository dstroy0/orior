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

void test_each_token_is_positioned_in_utf16_on_its_line(void)
{
    static const char s_text[] = "let x_1 = 3.5 + \"a \\\" b\";\n  \xC3\xA9(y)";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_text, .size = sizeof(s_text) - 1u, .side = &s_side));
    const CliToken *const last = &s_side.token[s_side.tokens - 1u];
    TEST_ASSERT_EQUAL_UINT(1u, last->line);
    TEST_ASSERT_EQUAL_UINT(5u, last->from);
    TEST_ASSERT_EQUAL_UINT(6u, last->to);
}

