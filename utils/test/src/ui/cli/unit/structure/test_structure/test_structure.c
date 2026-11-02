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

// `then` compared with `now`, read into the shared tree: its node
static unsigned int compared(const char *then, const char *now)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_structure.compare, CliStructureCfg, .then = then, .then_size = CLI_UNITY_LENGTH(then), .now = now, .now_size = CLI_UNITY_LENGTH(now), .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the member `name` of the compare, written compact, is `wanted`
static void assert_member(const char *wanted, unsigned int found, const char *name)
{
    char text[512];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = name), .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT(wanted, text, line.at);
}

// the whole number at `at` of an array, -1 where it is none
static long long number_at(unsigned int array, unsigned int at)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = array, .index = at);
    long long value = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER) ? 0ll : -1ll;
    for (unsigned long long digit = 0ull; (value >= 0ll) && (digit < s_json->node[number].length); digit += 1ull)
    {
        value = value * 10ll + (long long)(s_json->text[s_json->node[number].text + digit] - '0');
    }
    return value;
}

// every mark of the array `name`, where `line` is not -1 those on that line, is of `kind`
static void assert_marks(long long kind, unsigned int found, const char *name, long long line)
{
    const unsigned int marks = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = name);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = marks, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int mark = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = marks, .index = at);
        if ((line == -1ll) || (number_at(mark, 0u) == line))
        {
            TEST_ASSERT_EQUAL_INT64(kind, number_at(mark, 3u));
        }
    }
}

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
    static const char *const s_wanted[9] = {"let", "x_1", "=", "3.5", "+", "\"a \\\" b\"", ";", "\xC3\xA9", "("};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_text, .size = sizeof(s_text) - 1u, .side = &s_side));
    TEST_ASSERT_EQUAL_UINT(11u, s_side.tokens);
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        CLI_TEST_ASSERT_TEXT(s_wanted[at], s_text + s_side.token[at].start, s_side.token[at].end - s_side.token[at].start);
    }
}

void test_each_token_is_placed_in_utf16_on_its_line(void)
{
    static const char s_text[] = "let x_1 = 3.5 + \"a \\\" b\";\n  \xC3\xA9(y)";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = s_text, .size = sizeof(s_text) - 1u, .side = &s_side));
    const CliToken *const last = &s_side.token[s_side.tokens - 1u];
    TEST_ASSERT_EQUAL_UINT(1u, last->line);
    TEST_ASSERT_EQUAL_UINT(5u, last->from);
    TEST_ASSERT_EQUAL_UINT(6u, last->to);
}

void test_layout_alone_marks_no_token(void)
{
    const unsigned int found = compared(s_layout_then, s_layout_now);
    assert_member("[]", found, "left_marks");
    assert_member("[]", found, "right_marks");
}

void test_layout_alone_leaves_lines_the_same_or_reshaped(void)
{
    const unsigned int found = compared(s_layout_then, s_layout_now);
    const unsigned int right = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "right");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = right, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const long long kind = number_at(right, at);
        TEST_ASSERT_TRUE((kind == CLI_SAME) || (kind == CLI_RESHAPED));
    }
    const unsigned int reshaped = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "reshaped");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = reshaped, .kind = CFG_JSON_NUMBER));
    TEST_ASSERT_NOT_EQUAL('0', s_json->text[s_json->node[reshaped].text]);
}

void test_a_moved_line_reads_as_moved(void)
{
    assert_member("1", compared(s_rename_then, s_rename_now), "moved");
}

void test_a_moved_line_is_marked_on_both_sides(void)
{
    const unsigned int found = compared(s_rename_then, s_rename_now);
    TEST_ASSERT_EQUAL_INT64(CLI_MOVED, number_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "left"), 0u));
    TEST_ASSERT_EQUAL_INT64(CLI_MOVED, number_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "right"), 3u));
}

void test_a_name_changed_everywhere_is_renamed(void)
{
    assert_member("[[\"count\",\"total\",3]]", compared(s_rename_then, s_rename_now), "renames");
}

void test_a_renamed_names_lines_and_tokens_read_as_renamed(void)
{
    const unsigned int found = compared(s_rename_then, s_rename_now);
    const unsigned int right = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "right");
    TEST_ASSERT_EQUAL_INT64(CLI_RENAMED, number_at(right, 1u));
    TEST_ASSERT_EQUAL_INT64(CLI_RENAMED, number_at(right, 2u));
    assert_marks(CLI_RENAMED, found, "right_marks", 1ll);
}

void test_a_moved_block_is_moved_whole(void)
{
    assert_member("3", compared(s_block_then, s_block_now), "moved");
}

void test_a_moved_block_takes_its_closing_sign_along(void)
{
    const unsigned int found = compared(s_block_then, s_block_now);
    const unsigned int left = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "left");
    const unsigned int right = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "right");
    unsigned int moved_right = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = right, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        moved_right += (number_at(right, at) == CLI_MOVED) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_INT64(CLI_MOVED, number_at(left, 0u));
    TEST_ASSERT_EQUAL_INT64(CLI_MOVED, number_at(left, 1u));
    TEST_ASSERT_EQUAL_INT64(CLI_MOVED, number_at(left, 2u));
    // every closing sign is alike: which one moved on the right depends on the lines that stay in place
    TEST_ASSERT_EQUAL_UINT(3u, moved_right);
}

void test_each_token_of_a_moved_block_is_marked_moved(void)
{
    const unsigned int found = compared(s_block_then, s_block_now);
    assert_marks(CLI_MOVED, found, "left_marks", -1ll);
    assert_marks(CLI_MOVED, found, "right_marks", -1ll);
}

void test_a_changed_token_is_marked_on_both_sides(void)
{
    const unsigned int found = compared(s_token_then, s_token_now);
    assert_member("[[0,8,9,1]]", found, "left_marks");
    assert_member("[[0,8,9,1]]", found, "right_marks");
}

void test_changed_lines_are_side_by_side_rows(void)
{
    assert_member("[[0,0],[1,1]]", compared(s_token_then, s_token_now), "rows");
}

void test_a_change_starts_at_its_row(void)
{
    assert_member("[0]", compared(s_token_then, s_token_now), "starts");
}

void test_a_changed_token_renames_nothing(void)
{
    assert_member("[]", compared(s_token_then, s_token_now), "renames");
}

void test_a_name_changed_two_ways_is_not_renamed(void)
{
    assert_member("[]", compared("a(x);\nb(x);\n", "a(y);\nb(z);\n"), "renames");
}

void test_a_name_changed_two_ways_is_marked_changed(void)
{
    const unsigned int found = compared("a(x);\nb(x);\n", "a(y);\nb(z);\n");
    assert_marks(CLI_CHANGED, found, "right_marks", -1ll);
}
