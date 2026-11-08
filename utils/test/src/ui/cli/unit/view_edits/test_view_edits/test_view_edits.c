// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_view_edits.c: edits the CLI answers written into a view as one step: two edits on two lines are written, the
// cursor at the first's start; one undo takes both back and one redo brings both again; an edit after them is a step
// of its own; and a key with no view open is refused
#include "ui/cli/src/editor/view_edits.h"
#include "ui/cli/src/command_line/pool.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#include <string.h>

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

void test_a_key_with_no_view_is_refused(void)
{
    const CliEdit edit = {{0ULL, 0ULL}, {0ULL, 0ULL}, "x", 1ULL};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ULL};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = "no_such_view", .edit = &edit,
                                 .count = 1u, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ULL, err_msg.at);
}
