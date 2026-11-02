// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_leaves.c: what a run leaves, shown before it is written: a diff names what changed and folds the rest, a record file is
// shown by its sizes, and a refused file is restored as the tree stores it
#include "ui/query/src/query_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[QUERY_PATH_MAX_LEN];
static char s_shown[1u << 16u];
static char s_printed[4096];
static QueryRun s_run;
static int s_run_finished;
static int s_refused_restored;
static int s_accepted_left;

// `tree` against `stored` shown as the run's file `path`, into s_shown
static const char *shown(const char *path, const char *tree, const char *stored)
{
    VerbaLinea line = {s_shown, sizeof(s_shown), 0ull};
    EMBED_CALL(query_path_tree_internal.leaf_show, QueryPathTreeCtx, .path = path, .tree = (const unsigned char *)tree, .tree_size = CLI_UNITY_LENGTH(tree), .stored = (const unsigned char *)stored, .stored_size = CLI_UNITY_LENGTH(stored), .shown = &line);
    (void)mmgr_line_finish(&line);
    return s_shown;
}

// the lines of s_shown starting with `lead`, and every line in `all`
static unsigned int lines_led_by(char lead, unsigned int *all)
{
    unsigned int count = 0u;
    *all = 0u;
    for (const char *line = s_shown; line[0] != '\0';)
    {
        count += (line[0] == lead) ? 1u : 0u;
        *all += 1u;
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        line = (end != NULL) ? (end + 1) : (line + CLI_UNITY_LENGTH(line));
    }
    return count;
}

// twenty lines against twenty with the eleventh changed, shown
static void one_line_changed_shown(void)
{
    char before[256];
    char after[256];
    VerbaLinea one = {before, sizeof(before), 0ull};
    VerbaLinea two = {after, sizeof(after), 0ull};
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        mmgr_line_put(&one, "same\n");
        mmgr_line_put(&two, (at == 10u) ? "changed\n" : "same\n");
    }
    (void)mmgr_line_finish(&one);
    (void)mmgr_line_finish(&two);
    (void)shown("a.txt", before, after);
}

// what is printed, appended with a line end
static void text_printed(const CliEachLineSizeCfg *args)
{
    VerbaLinea line = {s_printed, sizeof(s_printed), CLI_UNITY_LENGTH(s_printed)};
    mmgr_line_put_n(&line, args->line, args->size);
    mmgr_line_ch(&line, '\n');
    (void)mmgr_line_finish(&line);
}

// the answers in turn: no, then all
static QueryAnswer answered(const CliEachPathCfg *args)
{
    static const QueryAnswer s_answers[2] = {QUERY_ANSWER_NO, QUERY_ANSWER_ALL};
    unsigned int *const answer_index = args->context;
    const QueryAnswer answer = s_answers[(*answer_index < 2u) ? *answer_index : 1u];
    *answer_index += 1u;
    return answer;
}

// whether the run's current file at `path` contains `text`
static int current_is(const char *path, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return EMBED_CALL(query_count_name_internal.writer_current, QueryCountNameCtx, .writer = &s_run.writer, .path = path, .bytes_u8s = &bytes, .size_pointer = &size) && (size == CLI_UNITY_LENGTH(text)) && (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a run over three files, the first refused and the rest accepted, once for the process; the test ignored where the machine has no
// tree or no C compiler
static void run_settled(void)
{
    static const char *const s_names[3] = {"a.khw", "b.krs", "c.krs"};
    static const char *const s_tree[3] = {"tree", "tree", "same"};
    static const char *const s_left[3] = {"run", "run", "same"};
    if (s_run_finished)
    {
        return;
    }
    char found[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    QueryPrograms programs = query_on_path();
    char compiler[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(programs.program, QueryEachRolePathRoomErrMsgCfg, .context = programs.context, .role = QUERY_ROLE_C, .path = compiler, .room = sizeof(compiler), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no C compiler to build the writer with");
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root, .src = found, .bytes = CLI_UNITY_LENGTH(found) + 1ull});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    char base[QUERY_PATH_MAX_LEN];
    char folder[QUERY_PATH_MAX_LEN];
    char files[3][QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/leaves", .joined = base);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = base));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "qry", .joined = folder);
    unsigned int answer_index = 0u;
    const QueryPrompter asker = {answered, &answer_index};
    QueryContext context = {s_root, &programs, {text_printed, NULL}, QUERY_LEAVE_PROMPT, asker, {{0}}, 0u, ""};
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_folder_context_internal.run_start, QueryFolderContextCtx, .run = &s_run, .root = s_root, .folder = folder, .programs = &programs, .context = &context, .err_msg = &err_msg), "a run starts");
    const char *paths[3];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = s_names[at], .joined = files[at]);
        paths[at] = files[at];
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = files[at], .bytes = s_tree[at], .size = CLI_UNITY_LENGTH(s_tree[at])));
        err_msg.at = 0ull;
        (void)EMBED_CALL(query_path_bytes_size_internal.writer_file, QueryPathBytesSizeCtx, .writer = &s_run.writer, .path = files[at], .bytes = s_left[at], .size = CLI_UNITY_LENGTH(s_left[at]), .err_msg = &err_msg);
    }
    EMBED_CALL(query_count_err_msg_internal.run_leave, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = paths, .count = 3u);
    s_printed[0] = '\0';
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_context_err_msg_internal.leaves_settle, QueryContextErrMsgCtx, .run_read = &s_run, .leave = QUERY_LEAVE_PROMPT, .prompter = asker, .context = &context, .err_msg = &err_msg), "the files are settled");
    s_refused_restored = current_is(files[0], "tree");
    s_accepted_left = current_is(files[1], "run");
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_context_err_msg_run_internal.run_stop, QueryContextErrMsgRunCtx, .run = &s_run, .err_msg = &err_msg);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
    s_run_finished = 1;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_diff_names_one_removed_and_one_added_line(void)
{
    unsigned int all = 0u;
    one_line_changed_shown();
    TEST_ASSERT_EQUAL_UINT(1u, lines_led_by('-', &all));
    TEST_ASSERT_EQUAL_UINT(1u, lines_led_by('+', &all));
}

void test_a_diff_shows_the_removed_and_added_text(void)
{
    one_line_changed_shown();
    CLI_TEST_ASSERT_CONTAINS("-same", s_shown);
    CLI_TEST_ASSERT_CONTAINS("+changed", s_shown);
}

void test_an_unchanged_run_folds_to_three_lines_a_count_and_three_lines(void)
{
    unsigned int all = 0u;
    one_line_changed_shown();
    (void)lines_led_by('-', &all);
    CLI_TEST_ASSERT_CONTAINS("... 4 lines the same ...", s_shown);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(17u, all);
}

void test_a_diff_of_the_same_lines_marks_nothing(void)
{
    unsigned int all = 0u;
    (void)shown("a.txt", "a\nb\nc\n", "a\nb\nc\n");
    TEST_ASSERT_EQUAL_UINT(0u, lines_led_by('-', &all));
    TEST_ASSERT_EQUAL_UINT(0u, lines_led_by('+', &all));
}

void test_a_line_added_to_an_empty_file(void)
{
    CLI_TEST_ASSERT_CONTAINS("+a", shown("a.txt", "", "a\n"));
}

void test_a_line_removed_to_an_empty_file(void)
{
    CLI_TEST_ASSERT_CONTAINS("-a", shown("a.txt", "a\n", ""));
}

void test_a_record_file_that_grew_is_shown_by_its_growth(void)
{
    TEST_ASSERT_EQUAL_STRING("sm_86.kqr grew by 2 bytes", shown("a/sm_86.kqr", "12345", "1234567"));
}

void test_a_record_file_that_did_not_grow_is_shown_by_its_sizes(void)
{
    TEST_ASSERT_EQUAL_STRING("sm_86.kqr: 7 bytes in the tree, 5 as the run leaves it", shown("a/sm_86.kqr", "1234567", "12345"));
}

void test_a_refused_file_is_restored_as_the_tree_stores_it(void)
{
    run_settled();
    TEST_ASSERT_TRUE(s_refused_restored);
}

void test_a_file_after_all_is_answered_is_left_as_the_run_leaves_it(void)
{
    run_settled();
    TEST_ASSERT_TRUE(s_accepted_left);
}

void test_each_changed_file_is_shown(void)
{
    run_settled();
    CLI_TEST_ASSERT_CONTAINS("a.khw:", s_printed);
    CLI_TEST_ASSERT_CONTAINS("b.krs:", s_printed);
}

void test_an_unchanged_file_is_skipped(void)
{
    run_settled();
    CLI_TEST_ASSERT_NOT_CONTAINS("c.krs", s_printed);
}
