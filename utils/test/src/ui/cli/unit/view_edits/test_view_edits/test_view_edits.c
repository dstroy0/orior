// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_view_edits.c: edits the CLI answers written into a view as one step: two edits on two lines are written, the
// cursor at the first's start; one undo takes both back and one redo brings both again; an edit after them is a step
// of its own; and a key with no view open is refused. A tidy takes the blanks off the lines' ends and puts a line end
// after the last line as one step, and leaves a text with nothing to change as it is; a replace writes another text
// over the lines that differ alone, to the text's end where the last lines differ, and nothing where the texts read
// the same
#include "ui/cli/src/editor/view_edits.h"
#include "ui/cli/src/text/edits.h"
#include "ui/cli/src/editor/view_tidy.h"
#include "ui/cli/src/command_line/pool.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_key[] = "view_edits_check";
static const char s_text[] = "alpha\nbeta\ngamma\n";
static char s_why[CLI_ERR_MSG_MAX_LEN];

void setUp(void)
{
    cli_unity_begun();
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_text,
                                .size = sizeof(s_text) - 1u, .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/edits.txt"));
}

void tearDown(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
}

// the whole text asserted to read as `required`
static void text_is(const char *required)
{
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = s_key,
                                  .owner = g_checks_owner, .size_pointer = &size);
    TEST_ASSERT_NOT_NULL(text);
    CLI_TEST_ASSERT_TEXT(required, text, size);
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
}

// the key `name` sent to the view
static void keyed(const char *name)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.type = CLI_VIEW_KEY;
    input.name = name;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &input, .err_msg = &err_msg));
}

// two edits, the first line's word and the third's, written as one step
static void two_written(void)
{
    const CliEdit edits[2] = {{{0ULL, 0ULL}, {0ULL, 5ULL}, "ALPHA", 5ULL}, {{2ULL, 0ULL}, {2ULL, 5ULL}, "G", 1ULL}};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = s_key, .edit = edits, .count = 2u,
                                .err_msg = &err_msg));
}

void test_two_edits_are_written_with_the_cursor_at_the_first(void)
{
    two_written();
    text_is("ALPHA\nbeta\nG\n");
    CliPosition cursor;
    CliPosition anchor;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key, .cursor = &cursor, .anchor = &anchor));
    TEST_ASSERT_EQUAL_UINT64(0ULL, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(0ULL, cursor.col);
}

void test_one_undo_takes_both_back_and_one_redo_brings_both_again(void)
{
    two_written();
    keyed("Mod+Z");
    text_is(s_text);
    keyed("Mod+Shift+Z");
    text_is("ALPHA\nbeta\nG\n");
}

void test_an_edit_after_them_is_a_step_of_its_own(void)
{
    two_written();
    const CliEdit edit = {{1ULL, 0ULL}, {1ULL, 4ULL}, "B", 1ULL};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = s_key, .edit = &edit, .count = 1u,
                                .err_msg = &err_msg));
    text_is("ALPHA\nB\nG\n");
    keyed("Mod+Z");
    text_is("ALPHA\nbeta\nG\n");
    keyed("Mod+Z");
    text_is(s_text);
}

// the view closed and opened again on `text`, `size` bytes
static void reopened(const char *text, unsigned long long size)
{
    tearDown();
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = text, .size = size,
                                .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/edits.txt"));
}

// the view tidied as the two switches ask
static void tidied(int trim, int final_newline)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_tidy_table.tidy, CliViewTidyCfg, .key = s_key, .trim = trim,
                                .final_newline = final_newline, .err_msg = &err_msg));
}

// the view's text replaced by `text`, `size` bytes
static void replaced(const char *text, unsigned long long size)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_replace_table.replace, CliViewReplaceCfg, .key = s_key, .text = text,
                                .length = size, .err_msg = &err_msg));
}

void test_a_tidy_takes_the_blanks_off_the_lines_ends_and_puts_a_line_end_after_the_last_as_one_step(void)
{
    static const char s_untidy[] = "alpha  \nbeta\t\ngamma";
    reopened(s_untidy, sizeof(s_untidy) - 1u);
    tidied(1, 1);
    text_is("alpha\nbeta\ngamma\n");
    keyed("Mod+Z");
    text_is(s_untidy);
}

void test_a_tidy_with_nothing_to_change_leaves_the_text_and_its_steps(void)
{
    tidied(1, 1);
    text_is(s_text);
    keyed("Mod+Z");
    text_is(s_text);
}

void test_trim_alone_retains_a_last_line_without_a_line_end(void)
{
    static const char s_untidy[] = "alpha \nbeta";
    reopened(s_untidy, sizeof(s_untidy) - 1u);
    tidied(1, 0);
    text_is("alpha\nbeta");
}

void test_a_replace_writes_only_the_lines_that_differ_with_the_cursor_before_them(void)
{
    static const char s_other[] = "alpha\nBETA\nbeta two\ngamma\n";
    replaced(s_other, sizeof(s_other) - 1u);
    text_is(s_other);
    CliPosition cursor;
    CliPosition anchor;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key, .cursor = &cursor, .anchor = &anchor));
    TEST_ASSERT_EQUAL_UINT64(1ULL, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(0ULL, cursor.col);
    keyed("Mod+Z");
    text_is(s_text);
}

void test_a_replace_reaches_the_texts_end_where_the_last_lines_differ(void)
{
    static const char s_other[] = "alpha\nbeta\nGAMMA";
    replaced(s_other, sizeof(s_other) - 1u);
    text_is(s_other);
}

void test_a_replace_with_the_same_text_changes_nothing(void)
{
    replaced(s_text, sizeof(s_text) - 1u);
    text_is(s_text);
    keyed("Mod+Z");
    text_is(s_text);
}

void test_a_key_with_no_view_is_refused(void)
{
    const CliEdit edit = {{0ULL, 0ULL}, {0ULL, 0ULL}, "x", 1ULL};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = "no_such_view", .edit = &edit,
                                 .count = 1u, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ULL, err_msg.at);
}

void test_a_place_and_an_edit_are_written_as_json_and_a_column_finds_its_byte(void)
{
    char buffer[96];
    VerbaLinea out = mmgr_line_over(buffer, sizeof(buffer));
    EMBED_CALL(cli_edits_position.text_position_write, CliEditsPositionCfg, .place = (CliTextPosition){3u, 7u}, .out = &out);
    CLI_TEST_ASSERT_TEXT("{\"line\":3,\"col\":7}", buffer, out.at);
    out = mmgr_line_over(buffer, sizeof(buffer));
    const CliTextEdit edit = {{1u, 2u}, {1u, 4u}, "a\"b", 3ULL};
    EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &edit, .out = &out);
    CLI_TEST_ASSERT_TEXT("{\"from\":{\"line\":1,\"col\":2},\"to\":{\"line\":1,\"col\":4},\"text\":\"a\\\"b\"}", buffer, out.at);
    // "a", a two byte character, a four byte character of two units, "b": column 2 is byte 3, column 4 is byte 7
    const char line[] = "a\xc3\xa9\xf0\x9f\x98\x80"
                        "b";
    TEST_ASSERT_EQUAL_UINT64(3ULL, EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = line, .size = sizeof(line) - 1u, .col = 2u));
    TEST_ASSERT_EQUAL_UINT64(7ULL, EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = line, .size = sizeof(line) - 1u, .col = 4u));
    TEST_ASSERT_EQUAL_UINT64(8ULL, EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = line, .size = sizeof(line) - 1u, .col = 40u));
}

static CliCompletions s_items;

void test_completion_items_are_put_after_each_other_and_made_unique_by_what_they_show(void)
{
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = &s_items, .label = "get", .label_size = 3ull, .type = "function", .detail = "int get()", .detail_size = 9ull, .insert = "get()", .insert_size = 5ull, .snippet = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = &s_items, .label = "set", .label_size = 3ull, .type = "function", .detail = "", .detail_size = 0ull, .insert = "set($1)", .insert_size = 7ull, .snippet = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = &s_items, .label = "get", .label_size = 3ull, .type = "field", .detail = "", .detail_size = 0ull, .insert = "get", .insert_size = 3ull, .snippet = 0));
    TEST_ASSERT_EQUAL_UINT(3u, s_items.count);
    EMBED_CALL(cli_edits_items.completions_unique, CliEditsItemsCfg, .items = &s_items);
    TEST_ASSERT_EQUAL_UINT(2u, s_items.count);
    TEST_ASSERT_EQUAL_STRING("function", s_items.item[0].type);
    TEST_ASSERT_EQUAL_UINT(5u, s_items.item[0].insert_size);
    TEST_ASSERT_TRUE(s_items.item[1].snippet);
    TEST_ASSERT_FALSE(s_items.full);
}
