// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_changes.c: the changes of a text against its last commit's file, answered off the store's diff and saved until the text moves: a line put in,
// one taken out and one changed are each one change with its mark; the change a line stands in is found, and for lines
// taken out the line above them; the marks of a run of lines; stepping from a line and from a change wraps both ways;
// and the revert of each type leaves the text the last commit's
#include "ui/cli/src/editor/document_changes.h"
#include "ui/cli/src/command_line/pool.h"
#include "ui/cli/src/editor/view_edits.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_key[] = "changes_check";
static const char s_head[] = "one\ntwo\nthree\nfour\n";

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
}

// the store told nothing of a change: the head alone is stored by the call
static void head_stored_each(const CliEachHunkCfg *args)
{
    (void)args;
}

// `now` stored under the key with a view open on it, and the head text stored beside it
static void stored_against(const char *now)
{
    CliShape shape;
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = now,
                                .size = cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_POOL_BYTES}),
                                .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/changes.txt"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = s_key,
                                .head_given = 1, .head = s_head, .head_size = sizeof(s_head) - 1u,
                                .each = head_stored_each, .context = NULL));
}

// the change at `line` and its mark asserted
static void mark_is(unsigned long long line, unsigned int at, unsigned int mark)
{
    CliChange change;
    unsigned int found_at = CLI_CHANGE_NONE;
    const int found = EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_key, .at = CLI_CHANGE_NONE,
                                 .line = line, .change_out = &change, .at_out = &found_at);
    if (at == CLI_CHANGE_NONE)
    {
        TEST_ASSERT_FALSE(found);
        return;
    }
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT(at, found_at);
    TEST_ASSERT_EQUAL_UINT(mark, EMBED_CALL(cli_document_change.mark, CliChangeCfg, .change = &change));
}

// how many changes stand
static unsigned int count_is(void)
{
    unsigned int count = 0u;
    (void)EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_key, .at = 0u, .count_out = &count);
    return count;
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

// the change at `line` reverted through the view as one step
static void reverted(unsigned long long line)
{
    CliChange change;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_key, .at = CLI_CHANGE_NONE,
                                .line = line, .change_out = &change));
    CliEdit edit;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.revert, CliChangeCfg, .key = s_key, .change = &change,
                                .edit_out = &edit, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = s_key, .edit = &edit, .count = 1u,
                                .err_msg = &err_msg));
    EMBED_CALL(cli_document_change.revert_end, CliChangeCfg, .edit_out = &edit);
}

void test_a_line_put_in_is_one_change_marked_added(void)
{
    stored_against("one\ntwo\nextra\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, count_is());
    mark_is(2ULL, 0u, CLI_CHANGE_MARK_ADDED);
    mark_is(1ULL, CLI_CHANGE_NONE, CLI_CHANGE_MARK_NONE);
    mark_is(3ULL, CLI_CHANGE_NONE, CLI_CHANGE_MARK_NONE);
}

void test_a_line_taken_out_marks_the_line_above_it(void)
{
    stored_against("one\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, count_is());
    mark_is(0ULL, 0u, CLI_CHANGE_MARK_REMOVED);
    mark_is(1ULL, CLI_CHANGE_NONE, CLI_CHANGE_MARK_NONE);
}

void test_a_line_changed_is_one_change_marked_changed_with_its_two_lines(void)
{
    stored_against("one\nTWO\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, count_is());
    CliChange change;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_key, .at = CLI_CHANGE_NONE,
                                .line = 1ULL, .change_out = &change));
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_CHANGED, EMBED_CALL(cli_document_change.mark, CliChangeCfg, .change = &change));
    TEST_ASSERT_EQUAL_UINT(1u, change.now_from);
    TEST_ASSERT_EQUAL_UINT(2u, change.now_to);
    TEST_ASSERT_EQUAL_UINT(1u, change.then_from);
    TEST_ASSERT_EQUAL_UINT(2u, change.then_to);
}

void test_the_marks_of_a_run_of_lines(void)
{
    stored_against("one\nTWO\nthree\nextra\nfour\n");
    unsigned char marks[5];
    EMBED_CALL(cli_document_change.marks, CliChangeCfg, .key = s_key, .line = 0ULL, .count = 5u, .marks_out = marks);
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_NONE, marks[0]);
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_CHANGED, marks[1]);
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_NONE, marks[2]);
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_ADDED, marks[3]);
    TEST_ASSERT_EQUAL_UINT(CLI_CHANGE_MARK_NONE, marks[4]);
}

void test_stepping_wraps_in_both_directions(void)
{
    stored_against("one\nTWO\nthree\nextra\nfour\n");
    TEST_ASSERT_EQUAL_UINT(2u, count_is());
    CliChange change;
    unsigned int at = CLI_CHANGE_NONE;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = s_key, .at = CLI_CHANGE_NONE,
                                .line = 1ULL, .by = 1, .change_out = &change, .at_out = &at));
    TEST_ASSERT_EQUAL_UINT(0u, at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = s_key, .at = at, .by = 1,
                                .change_out = &change, .at_out = &at));
    TEST_ASSERT_EQUAL_UINT(1u, at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = s_key, .at = at, .by = 1,
                                .change_out = &change, .at_out = &at));
    TEST_ASSERT_EQUAL_UINT(0u, at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = s_key, .at = at, .by = -1,
                                .change_out = &change, .at_out = &at));
    TEST_ASSERT_EQUAL_UINT(1u, at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = s_key, .at = CLI_CHANGE_NONE,
                                .line = 0ULL, .by = -1, .change_out = &change, .at_out = &at));
    TEST_ASSERT_EQUAL_UINT(1u, at);
}

void test_a_revert_of_a_line_put_in_leaves_the_last_commits_text(void)
{
    stored_against("one\ntwo\nextra\nthree\nfour\n");
    reverted(2ULL);
    text_is(s_head);
}

void test_a_revert_of_a_line_taken_out_leaves_the_last_commits_text(void)
{
    stored_against("one\nthree\nfour\n");
    reverted(0ULL);
    text_is(s_head);
}

void test_a_revert_of_a_line_changed_leaves_the_last_commits_text(void)
{
    stored_against("one\nTWO\nthree\nfour\n");
    reverted(1ULL);
    text_is(s_head);
}

void test_a_revert_of_lines_at_the_end_leaves_the_last_commits_text(void)
{
    stored_against("one\ntwo\nthree\n");
    reverted(2ULL);
    text_is(s_head);
}

void test_the_changes_read_again_stand_until_the_text_moves(void)
{
    stored_against("one\nTWO\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, count_is());
    TEST_ASSERT_EQUAL_UINT(1u, count_is());
    mark_is(1ULL, 0u, CLI_CHANGE_MARK_CHANGED);
    reverted(1ULL);
    TEST_ASSERT_EQUAL_UINT(0u, count_is());
    mark_is(1ULL, CLI_CHANGE_NONE, CLI_CHANGE_MARK_NONE);
}

void test_no_head_reads_no_change(void)
{
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = "one\n", .size = 4ULL,
                                .shape = &shape));
    TEST_ASSERT_EQUAL_UINT(0u, count_is());
    mark_is(0ULL, CLI_CHANGE_NONE, CLI_CHANGE_MARK_NONE);
}
