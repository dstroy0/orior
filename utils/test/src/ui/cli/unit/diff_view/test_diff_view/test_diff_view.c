// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_diff_view.c: two texts of a file side by side: the file against the last commit lays a row each line, a line
// changed on both sides, a line put in across from a gap, the lines both share level, with the bar's sentences and the
// steps wrapping at the ends and found by the row in view; a file the last commit lacks puts every line in; a file
// that does not differ lays no row and answers its sentence; a commit against the file, two commits the older first, a
// commit against its first parent, the clipboard against the file and two files each pair their texts and sentences;
// a snapshot not stored is refused with why; white space set aside lays lines that differ only in it level, its box
// pressed off while structure is on, the setting saved in orior's folder; and texts too far apart answer their sentence
#include "ui/cli/src/code/diff_view.h"
#include "ui/cli/src/code/git.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];
static char s_home[CLI_PATH_MAX_LEN];
static char s_out[1u << 16u];
static char s_error[1024];
static char s_first[64];
static char s_second[64];
static char s_far[2u][2200u * 8u];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_diff_view_source_err_msg.close, CliDiffViewSourceErrMsgCfg, .root = NULL);
}

// git run at the root with the words sent, NULL past the last: 1, or 0
static int git_ran(const char *one, const char *two, const char *three, const char *four)
{
    const char *const words[4] = {one, two, three, four};
    unsigned int count = 0u;
    while ((count < 4u) && (words[count] != NULL))
    {
        count += 1u;
    }
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ULL};
    VerbaLinea out = {s_out, sizeof(s_out), 0ULL};
    const int ran = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root,
                               .request = words, .count = count, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return ran;
}

// a repository at the folder `name` with a.txt committed as `text`
static void repository_created(const char *name, const char *text)
{
    cli_unity_folder(name, s_root);
    TEST_ASSERT_TRUE(git_ran("init", "-q", "-b", "main"));
    TEST_ASSERT_TRUE(git_ran("config", "user.name", "t", NULL));
    TEST_ASSERT_TRUE(git_ran("config", "user.email", "t@t", NULL));
    cli_unity_file(s_root, "a.txt", text);
    TEST_ASSERT_TRUE(git_ran("add", ".", NULL, NULL));
    TEST_ASSERT_TRUE(git_ran("commit", "-q", "-m", "first"));
}

// the id of the last commit into `id`, sized for 64 bytes
static void last_id(char *id)
{
    TEST_ASSERT_TRUE(git_ran("rev-parse", "HEAD", NULL, NULL));
    unsigned long long length = 0ULL;
    while ((length < 63ULL) && (s_out[length] != '\0') && (s_out[length] != '\n'))
    {
        id[length] = s_out[length];
        length += 1ULL;
    }
    id[length] = '\0';
}

// the view opened from `source` on `path`: what open answered
static int opened(unsigned int source, const char *path, const char *other_path, const char *id, const char *other_id)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ULL};
    return EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = s_root, .source = source,
                      .path = path, .other_path = other_path, .id = id, .other_id = other_id, .err_msg = &err_msg);
}

// the row `at` is `type`, its left line `left`, NULL for none, and its right `right`
static void row_is(unsigned int at, unsigned int type, const char *left, const char *right)
{
    CliDiffViewRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_diff_view_at_row_length.row, CliDiffViewAtRowLengthCfg, .at = at, .row = &row));
    TEST_ASSERT_EQUAL_UINT(type, row.type);
    TEST_ASSERT_EQUAL_INT(left != NULL, row.has_left);
    TEST_ASSERT_EQUAL_INT(right != NULL, row.has_right);
    const char *const texts[2] = {left, right};
    const unsigned short *const units[2] = {row.left_units, row.right_units};
    const unsigned long long counts[2] = {row.left_count, row.right_count};
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        if (texts[side] == NULL)
        {
            continue;
        }
        TEST_ASSERT_EQUAL_UINT64(CLI_UNITY_LENGTH(texts[side]), counts[side]);
        for (unsigned long long unit = 0ULL; unit < counts[side]; unit += 1ULL)
        {
            TEST_ASSERT_EQUAL_UINT((unsigned char)texts[side][unit], units[side][unit]);
        }
    }
}

// the bar's text `part` reads `text`
static void bar_is(unsigned int part, const char *text)
{
    unsigned long long length = 0ULL;
    const char *const read =
        EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg, .part = part, .length_out = &length);
    CLI_TEST_ASSERT_TEXT(text, read, length);
}

// the where sentence reads `text`
static void where_is(const char *text)
{
    unsigned long long length = 0ULL;
    const char *const read =
        EMBED_CALL(cli_diff_view_at_row_length.where, CliDiffViewAtRowLengthCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT(text, read, length);
}

// the body's sentence in place of rows reads `text`
static void empty_is(const char *text)
{
    unsigned long long length = 0ULL;
    const char *const read =
        EMBED_CALL(cli_diff_view_at_row_length.empty, CliDiffViewAtRowLengthCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT(text, read, length);
}

static unsigned int rows(void)
{
    return EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u);
}

static unsigned int stepped(int direction, unsigned int at)
{
    return EMBED_CALL(cli_diff_view_at_row_length.step, CliDiffViewAtRowLengthCfg, .direction = direction, .at = at);
}

// the sides sentence of commit `older` against commit `newer`, or against the file as it stands for NULL, into `into`,
// sized for 256 bytes
static void commit_sides(char *into, const char *older, const char *newer)
{
    VerbaLinea line = {into, 256ULL, 0ULL};
    mmgr_line_put_n(&line, older, 7u);
    mmgr_line_put(&line, " by t, then ");
    if (newer != NULL)
    {
        mmgr_line_put_n(&line, newer, 7u);
        mmgr_line_put(&line, " by t");
    }
    else
    {
        mmgr_line_put(&line, "as it stands");
    }
    (void)mmgr_line_finish(&line);
}

void test_the_file_against_the_last_commit_lays_a_row_each_line(void)
{
    repository_created("diff_view_head", "one\ntwo\nthree\nfour\nfive\n");
    cli_unity_file(s_root, "a.txt", "one\nTWO\nthree\nfour\nFIVE\nsix\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "a.txt", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(7u, rows());
    row_is(0u, CLI_SAME, "one", "one");
    row_is(1u, CLI_CHANGED, "two", "TWO");
    row_is(2u, CLI_SAME, "three", "three");
    row_is(4u, CLI_CHANGED, "five", "FIVE");
    row_is(5u, CLI_DIFF_VIEW_ADDED, NULL, "six");
    row_is(6u, CLI_SAME, "", "");
    CliDiffViewRow past;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_diff_view_at_row_length.row, CliDiffViewAtRowLengthCfg, .at = 7u, .row = &past));
    bar_is(CLI_DIFF_VIEW_BAR_LABEL, "a.txt");
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, "last commit, then as it stands");
    bar_is(CLI_DIFF_VIEW_BAR_SPACE, "Set white space aside");
    bar_is(CLI_DIFF_VIEW_BAR_STRUCTURE, "Compare by structure");
    where_is("change 1 of 2");
    empty_is("");
    unsigned long long length = 9ULL;
    (void)EMBED_CALL(cli_diff_view_at_row_length.summary, CliDiffViewAtRowLengthCfg, .length_out = &length);
    TEST_ASSERT_EQUAL_UINT64(0ULL, length);
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_diff_view_at_row_length.marks, CliDiffViewAtRowLengthCfg, .at = 1u));
}

void test_the_steps_wrap_and_follow_the_row_in_view(void)
{
    repository_created("diff_view_steps", "one\ntwo\nthree\nfour\nfive\n");
    cli_unity_file(s_root, "a.txt", "one\nTWO\nthree\nfour\nFIVE\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "a.txt", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(1u, stepped(0, 0u));
    TEST_ASSERT_EQUAL_UINT(4u, stepped(1, 0u));
    where_is("change 2 of 2");
    TEST_ASSERT_EQUAL_UINT(1u, stepped(1, 0u));
    where_is("change 1 of 2");
    TEST_ASSERT_EQUAL_UINT(4u, stepped(-1, 0u));
    TEST_ASSERT_EQUAL_UINT(1u, stepped(0, 3u));
    where_is("change 1 of 2");
    TEST_ASSERT_EQUAL_UINT(4u, stepped(0, 5u));
    where_is("change 2 of 2");
}

void test_a_file_the_last_commit_lacks_puts_every_line_in(void)
{
    repository_created("diff_view_new", "one\n");
    cli_unity_file(s_root, "b.txt", "new\nlines\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "b.txt", NULL, NULL, NULL));
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, "not in the last commit");
    TEST_ASSERT_EQUAL_UINT(3u, rows());
    row_is(0u, CLI_DIFF_VIEW_ADDED, NULL, "new");
    row_is(2u, CLI_DIFF_VIEW_ADDED, NULL, "");
    where_is("change 1 of 1");
}

void test_a_file_that_does_not_differ_lays_no_row(void)
{
    repository_created("diff_view_same", "one\ntwo\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "a.txt", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(0u, rows());
    empty_is("No change from the last commit.");
    where_is("");
    TEST_ASSERT_EQUAL_UINT(0u, stepped(1, 0u));
}

void test_commits_parents_the_clipboard_and_files_pair_their_texts(void)
{
    repository_created("diff_view_commits", "one\n");
    last_id(s_first);
    cli_unity_file(s_root, "a.txt", "two\n");
    TEST_ASSERT_TRUE(git_ran("commit", "-q", "-am", "second"));
    last_id(s_second);
    cli_unity_file(s_root, "a.txt", "three\n");
    char sides[256];

    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_COMMIT, "a.txt", NULL, s_first, NULL));
    row_is(0u, CLI_CHANGED, "one", "three");
    commit_sides(sides, s_first, NULL);
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, sides);

    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_COMMITS, "a.txt", NULL, s_second, s_first));
    row_is(0u, CLI_CHANGED, "one", "two");
    commit_sides(sides, s_first, s_second);
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, sides);

    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_PARENT, "a.txt", NULL, s_second, NULL));
    row_is(0u, CLI_CHANGED, "one", "two");
    VerbaLinea line = {sides, sizeof(sides), 0ULL};
    mmgr_line_put_n(&line, s_first, 7u);
    mmgr_line_put(&line, ", then ");
    mmgr_line_put_n(&line, s_second, 7u);
    mmgr_line_put(&line, " by t");
    (void)mmgr_line_finish(&line);
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, sides);

    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = s_root,
                                .source = CLI_DIFF_VIEW_CLIPBOARD, .path = "a.txt", .text = "three\n", .size = 6ULL,
                                .err_msg = &err_msg));
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, "the clipboard, then the file");
    empty_is("The two are the same.");

    cli_unity_file(s_root, "b.txt", "three\nmore\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_FILES, "a.txt", "b.txt", NULL, NULL));
    bar_is(CLI_DIFF_VIEW_BAR_LABEL, "a.txt \xc2\xb7 b.txt");
    bar_is(CLI_DIFF_VIEW_BAR_SIDES, "a.txt, then b.txt");
    row_is(1u, CLI_DIFF_VIEW_ADDED, NULL, "more");

    TEST_ASSERT_FALSE(EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = s_root,
                                 .source = CLI_DIFF_VIEW_SNAPSHOT, .path = "a.txt", .at_ms = 1ULL,
                                 .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ULL, err_msg.at);
    TEST_ASSERT_EQUAL_UINT(0u, rows());
}

void test_white_space_set_aside_lays_lines_level_and_saves_its_setting(void)
{
    cli_unity_folder("diff_view_home", s_home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                .value = s_home));
    EMBED_CALL(cli_diff_view_option_on.begin, CliDiffViewOptionOnCfg, .entries = NULL);
    repository_created("diff_view_space", "a b\nc\n");
    cli_unity_file(s_root, "a.txt", "a   b \nc\n");
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "a.txt", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(3u, rows());
    int on = 1;
    int enabled = 0;
    EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_SPACE,
               .on_out = &on, .enabled_out = &enabled);
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_TRUE(enabled);
    EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_STRUCTURE,
               .on_out = &on, .enabled_out = &enabled);
    TEST_ASSERT_FALSE(enabled);
    (void)EMBED_CALL(cli_diff_view_option_on.option_toggle, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_SPACE,
                     .at = 1u);
    EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_SPACE,
               .on_out = &on, .enabled_out = &enabled);
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_UINT(0u, rows());
    empty_is("No change from the last commit.");
    CliStateEntries *const entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = s_home, .owner = g_cli_diff_view_owner});
    TEST_ASSERT_NOT_NULL(entries);
    TEST_ASSERT_EQUAL_STRING("aside", cli_state_entry_value(entries, "diff-space"));
    EMBED_CALL(cli_diff_view_option_on.begin, CliDiffViewOptionOnCfg, .entries = entries);
    g_cli_guard->persistent_buf_release(entries, g_cli_diff_view_owner);
    (void)EMBED_CALL(cli_diff_view_option_on.option_toggle, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_SPACE,
                     .at = 0u);
    TEST_ASSERT_EQUAL_UINT(3u, rows());
    row_is(0u, CLI_CHANGED, "a b", "a   b ");
}

void test_texts_too_far_apart_answer_their_sentence(void)
{
    VerbaLinea then = {s_far[0], sizeof(s_far[0]), 0ULL};
    VerbaLinea now = {s_far[1], sizeof(s_far[1]), 0ULL};
    for (unsigned int line = 0u; line < 2200u; line += 1u)
    {
        mmgr_line_put(&then, "a");
        mmgr_line_decimal(&then, line, 1u);
        mmgr_line_put(&then, "\n");
        mmgr_line_put(&now, "b");
        mmgr_line_decimal(&now, line, 1u);
        mmgr_line_put(&now, "\n");
    }
    (void)mmgr_line_finish(&then);
    (void)mmgr_line_finish(&now);
    repository_created("diff_view_far", s_far[0]);
    cli_unity_file(s_root, "a.txt", s_far[1]);
    TEST_ASSERT_TRUE(opened(CLI_DIFF_VIEW_HEAD, "a.txt", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(0u, rows());
    empty_is("The two texts are too far apart to set side by side.");
}

void test_no_file_is_refused(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ULL};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = NULL,
                                 .source = CLI_DIFF_VIEW_HEAD, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    TEST_ASSERT_EQUAL_STRING("No file is shown to compare.", s_error);
    bar_is(CLI_DIFF_VIEW_BAR_LABEL, "");
}
