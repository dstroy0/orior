// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_hunks.c: the changes of a view against its last commit's file: a line put in, one taken out and one changed are
// each one change with its mark; the change a line stands in is found, and for lines taken out the line above them;
// the revert of each type leaves the text the last commit's; stepping wraps both ways; the change shown carries its
// sentence, its lines and its settle; Escape closes it; and no key reads no change
#include "ui/cli/src/editor/hunks.h"
#include "ui/cli/src/command_line/pool.h"
#include "ui/cli/src/editor/view_edits.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_key[] = "hunks_check";
static const char s_head[] = "one\ntwo\nthree\nfour\n";

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = NULL);
}

// `now` stored under the key with a view open on it, and its changes read against the head text
static void read_against(const char *now)
{
    CliShape shape;
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = now,
                                .size = cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_POOL_BYTES}),
                                .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/hunks.txt"));
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = s_key, .text = s_head, .size = sizeof(s_head) - 1u);
}

// the change at `line` and its mark asserted
static void mark_is(unsigned long long line, unsigned int at, unsigned int mark)
{
    unsigned int found = CLI_HUNKS_MARK_NONE;
    TEST_ASSERT_EQUAL_UINT(at, EMBED_CALL(cli_hunks.at_line, CliHunksCfg, .line = line, .mark_out = &found));
    TEST_ASSERT_EQUAL_UINT(mark, found);
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

// the change shown reverted through the view as one step
static void reverted(void)
{
    CliEdit edit;
    unsigned int count = 0u;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.revert, CliHunksCfg, .edit_out = &edit, .edit_count = &count,
                                .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(1u, count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = s_key, .edit = &edit, .count = count,
                                .err_msg = &err_msg));
}

void test_a_line_put_in_is_one_change_marked_added(void)
{
    read_against("one\ntwo\nnew\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    mark_is(2ULL, 0u, CLI_HUNKS_MARK_ADDED);
    mark_is(1ULL, CLI_HUNKS_NONE, CLI_HUNKS_MARK_NONE);
    mark_is(3ULL, CLI_HUNKS_NONE, CLI_HUNKS_MARK_NONE);
}

void test_a_line_taken_out_marks_the_line_above_it(void)
{
    read_against("one\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    mark_is(0ULL, 0u, CLI_HUNKS_MARK_REMOVED);
    mark_is(1ULL, CLI_HUNKS_NONE, CLI_HUNKS_MARK_NONE);
}

void test_a_line_changed_is_one_change_marked_changed_with_its_two_lines(void)
{
    read_against("one\nTWO\nthree\nfour\n");
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    mark_is(1ULL, 0u, CLI_HUNKS_MARK_CHANGED);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 1ULL));
    unsigned long long under = 0ULL;
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_hunks.row_count, CliHunksCfg, .line_out = &under));
    TEST_ASSERT_EQUAL_UINT64(1ULL, under);
    CliHunkRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.row, CliHunksCfg, .at = 0u, .row = &row));
    TEST_ASSERT_TRUE(row.removed);
    TEST_ASSERT_EQUAL_UINT64(3ULL, row.count);
    TEST_ASSERT_EQUAL_UINT16((unsigned short)'t', row.units[0]);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.row, CliHunksCfg, .at = 1u, .row = &row));
    TEST_ASSERT_FALSE(row.removed);
    TEST_ASSERT_EQUAL_UINT16((unsigned short)'T', row.units[0]);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_hunks.row, CliHunksCfg, .at = 2u, .row = &row));
}

void test_the_change_shown_carries_its_sentence_and_settles(void)
{
    read_against("ONE\ntwo\nthree\nFOUR\n");
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_hunks.title, CliHunksCfg, .length_out = &length);
    TEST_ASSERT_EQUAL_UINT64(0ULL, length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.settled, CliHunksCfg, .at = 0u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 3ULL));
    const char *const title = EMBED_CALL(cli_hunks.title, CliHunksCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("Change 2 of 2", title, length);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_hunks.settled, CliHunksCfg, .at = 0u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 1ULL));
}

void test_stepping_wraps_in_both_directions(void)
{
    read_against("ONE\ntwo\nthree\nFOUR\n");
    unsigned long long line = 99ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.step, CliHunksCfg, .line = 1ULL, .by = 1, .line_out = &line));
    TEST_ASSERT_EQUAL_UINT64(3ULL, line);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.step, CliHunksCfg, .by = 1, .line_out = &line));
    TEST_ASSERT_EQUAL_UINT64(0ULL, line);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.step, CliHunksCfg, .by = -1, .line_out = &line));
    TEST_ASSERT_EQUAL_UINT64(3ULL, line);
    EMBED_CALL(cli_hunks.close, CliHunksCfg, .at = 0u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.step, CliHunksCfg, .line = 0ULL, .by = -1, .line_out = &line));
    TEST_ASSERT_EQUAL_UINT64(3ULL, line);
}

void test_escape_closes_the_change_shown_and_no_other_key_is_taken(void)
{
    read_against("one\nTWO\nthree\nfour\n");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_hunks.key, CliHunksCfg, .name = "Escape"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 1ULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_hunks.key, CliHunksCfg, .name = "Down"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.key, CliHunksCfg, .name = "Escape"));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_hunks.row_count, CliHunksCfg, .line_out = NULL));
}

void test_a_revert_of_a_line_put_in_leaves_the_last_commits_text(void)
{
    read_against("one\ntwo\nnew\nthree\nfour\n");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 2ULL));
    reverted();
    text_is(s_head);
}

void test_a_revert_of_a_line_taken_out_leaves_the_last_commits_text(void)
{
    read_against("one\nthree\nfour\n");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 0ULL));
    reverted();
    text_is(s_head);
}

void test_a_revert_of_a_line_changed_leaves_the_last_commits_text(void)
{
    read_against("one\nTWO\nthree\nfour\n");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 1ULL));
    reverted();
    text_is(s_head);
}

void test_a_revert_of_lines_at_the_end_leaves_the_last_commits_text(void)
{
    read_against("one\ntwo\nthree\nfour\nfive");
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = 4ULL));
    reverted();
    text_is(s_head);
}

void test_no_key_reads_no_change(void)
{
    read_against("one\nTWO\nthree\nfour\n");
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = NULL);
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u));
    mark_is(1ULL, CLI_HUNKS_NONE, CLI_HUNKS_MARK_NONE);
}
