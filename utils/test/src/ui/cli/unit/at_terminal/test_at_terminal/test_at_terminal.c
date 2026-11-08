// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_at_terminal.c: a window command runs at the terminal only where the terminal can do it; branches and stashes go through their commands; tests
// run and report each failure and the counts; synced settings are read in and the read undone; a window command with no window open runs at the
// terminal where it can and opens a window where not; and a branch with its own commits is deleted only when forced
#include "ui/cli/src/command_line/at_terminal.h"
#include "ui/cli/src/command_line/command_line.h"
#include "ui/cli/src/machine/settings_sync.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_test_here[] = "import unittest\n\n\nclass Here(unittest.TestCase):\n    def test_passes(self):\n"
                                  "        self.assertTrue(True)\n\n    def test_fails(self):\n"
                                  "        self.assertEqual(1, 2)\n";
static const char s_test_good[] = "import unittest\n\n\nclass Good(unittest.TestCase):\n    def test_passes(self):\n"
                                  "        self.assertTrue(True)\n";

static char s_out[1u << 16u];
static char s_err_buf[1u << 16u];
static char s_root[CLI_PATH_MAX_LEN];
static char s_text[64];

// git run in s_root with its words, ended by NULL, asserted to have ended well
static void git_run_result(const char *const *words)
{
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = s_root, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_msg), words[1]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, run_result->code, words[1]);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
}

// a repository `name` at s_root with one commit of a.txt, a commit here not signed whatever the machine's configuration
static void repository_created(const char *name)
{
    static const char *const s_setup[6][7] = {{"git", "init", "-q", "-b", "main", NULL},
                                              {"git", "config", "user.name", "t", NULL},
                                              {"git", "config", "user.email", "t@t", NULL},
                                              {"git", "config", "commit.gpgsign", "false", NULL},
                                              {"git", "add", ".", NULL},
                                              {"git", "commit", "-q", "-m", "first", NULL}};
    cli_unity_folder(name, s_root);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        git_run_result(s_setup[at]);
    }
    cli_unity_file(s_root, "a.txt", "one\n");
    git_run_result(s_setup[4]);
    git_run_result(s_setup[5]);
}

// `command` with `count` words run on the tree at s_root: 1 with what it printed in s_out, or 0 with why in s_err_buf, each ended by a 0
static int command_run_result(const char *command, const char *const *args, unsigned int count)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    const int run_result = EMBED_CALL(cli_at_terminal_command.run, CliAtTerminalCommandCfg, .root = s_root, .command = command, .request = args, .count = count, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    (void)mmgr_line_finish(&err_msg);
    return run_result;
}

// the text of the file `name` under `root`, empty where it has none, in s_text
static const char *file_text(const char *root, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    return s_text;
}

// whether the listed branches name `text`
static int branches_contain(const char *text)
{
    TEST_ASSERT_TRUE(command_run_result("branches", NULL, 0u));
    return cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_POOL_BYTES, .other = text, .other_cap = CLI_POOL_BYTES}) != NULL;
}

// settings sent from here to between, with their three folders under the test's folder
static void settings_sent(char *base, char *there, char *between)
{
    static char s_sent_out[1024];
    static char s_sent_err[1024];
    char here[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("at_terminal_settings_sync", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "here", .joined = here);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "there", .joined = there);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "between", .joined = between);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = here));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = there));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = between));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = here, .relative = "user.css", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "body { color: red; }\n", .size = 21ull));
    VerbaLinea out = {s_sent_out, sizeof(s_sent_out), 0ull};
    VerbaLinea err_msg = {s_sent_err, sizeof(s_sent_err), 0ull};
    (void)EMBED_CALL(cli_settings_sync_home_to.send, CliSettingsSyncHomeToCfg, .home = here, .to = between, .out = &out, .err_msg = &err_msg);
}

// the sent settings read in on `there`: 1 with what it printed in s_out, or 0
static int settings_read(const char *there, const char *between)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    const int read = EMBED_CALL(cli_at_terminal_count.take, CliAtTerminalCountCfg, .home = there, .from = between, .chosen = NULL, .count = 0u, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    (void)mmgr_line_finish(&err_msg);
    return read;
}

// a repository with the branch side one commit past main, on main; the commit's id in `commit`, which is sized for 64
static void side_created(char *commit)
{
    static const char *const s_side[1] = {"side"};
    static const char *const s_main[1] = {"main"};
    static const char *const s_add[4] = {"git", "add", ".", NULL};
    static const char *const s_commit[6] = {"git", "commit", "-q", "-m", "on the side", NULL};
    repository_created("at_terminal_unmerged");
    (void)command_run_result("new-branch", s_side, 1u);
    cli_unity_file(s_root, "b.txt", "side\n");
    git_run_result(s_add);
    git_run_result(s_commit);
    (void)command_run_result("branch-switch", s_main, 1u);
    TEST_ASSERT_TRUE(command_run_result("cherry-pick", NULL, 0u));
    commit[0] = '\0';
    for (unsigned int at = 0u; (at + 1u < 64u) && (s_out[at] > ' '); at += 1u)
    {
        commit[at] = s_out[at];
        commit[at + 1u] = '\0';
    }
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_gits_tests_and_transferred_settings_commands_run_at_the_terminal(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "push"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "branch-switch"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "stash-drop"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "test-run"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "sync.undo"));
}

void test_the_windows_own_commands_do_not_run_at_the_terminal(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "word-wrap"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "preferences"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = "debug"));
}

void test_word_wrap_reports_it_runs_in_a_window(void)
{
    repository_created("at_terminal_refused");
    TEST_ASSERT_FALSE(command_run_result("word-wrap", NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("word-wrap runs in a window", s_err_buf);
}

void test_branch_switch_reports_it_needs_a_branch(void)
{
    repository_created("at_terminal_refused");
    TEST_ASSERT_FALSE(command_run_result("branch-switch", NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("branch-switch takes a branch", s_err_buf);
}

void test_a_new_branch_is_the_one_the_tree_is_on(void)
{
    static const char *const s_created[1] = {"made"};
    repository_created("at_terminal_git");
    (void)command_run_result("new-branch", s_created, 1u);
    TEST_ASSERT_TRUE(branches_contain("* made"));
}

void test_a_branch_is_renamed(void)
{
    static const char *const s_created[1] = {"made"};
    static const char *const s_main[1] = {"main"};
    static const char *const s_rename[2] = {"made", "named"};
    repository_created("at_terminal_git");
    (void)command_run_result("new-branch", s_created, 1u);
    (void)command_run_result("branch-switch", s_main, 1u);
    (void)command_run_result("branch-rename", s_rename, 2u);
    TEST_ASSERT_TRUE(branches_contain("named"));
}

void test_a_branch_is_deleted(void)
{
    static const char *const s_created[1] = {"made"};
    static const char *const s_main[1] = {"main"};
    repository_created("at_terminal_git");
    (void)command_run_result("new-branch", s_created, 1u);
    (void)command_run_result("branch-switch", s_main, 1u);
    (void)command_run_result("branch-delete", s_created, 1u);
    TEST_ASSERT_FALSE(branches_contain("made"));
}

void test_a_change_is_stashed(void)
{
    static const char *const s_append[2] = {"put", "aside"};
    repository_created("at_terminal_stash");
    cli_unity_file(s_root, "a.txt", "two\n");
    (void)command_run_result("stash", s_append, 2u);
    TEST_ASSERT_EQUAL_STRING("one\n", file_text(s_root, "a.txt"));
}

void test_a_stash_is_listing(void)
{
    static const char *const s_append[2] = {"put", "aside"};
    repository_created("at_terminal_stash");
    cli_unity_file(s_root, "a.txt", "two\n");
    (void)command_run_result("stash", s_append, 2u);
    TEST_ASSERT_TRUE(command_run_result("stashes", NULL, 0u));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "stash@{0}", .cap = CLI_POOL_BYTES}));
}

void test_a_stash_popped_restores_the_change_and_leaves_the_list(void)
{
    static const char *const s_append[2] = {"put", "aside"};
    static const char *const s_stash[1] = {"stash@{0}"};
    repository_created("at_terminal_stash");
    cli_unity_file(s_root, "a.txt", "two\n");
    (void)command_run_result("stash", s_append, 2u);
    (void)command_run_result("stash-pop", s_stash, 1u);
    TEST_ASSERT_EQUAL_STRING("two\n", file_text(s_root, "a.txt"));
    TEST_ASSERT_TRUE(command_run_result("stashes", NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

void test_a_test_run_reports_each_failure_and_the_counts(void)
{
    static const char *const s_here[1] = {"test_here.py"};
    repository_created("at_terminal_tests");
    cli_unity_file(s_root, "test_here.py", s_test_here);
    TEST_ASSERT_FALSE(command_run_result("test-run", s_here, 1u));
    CLI_TEST_ASSERT_CONTAINS("FAILED ", s_err_buf);
    CLI_TEST_ASSERT_CONTAINS("test_fails", s_err_buf);
    CLI_TEST_ASSERT_CONTAINS("1 passed, 1 failed", s_err_buf);
}

void test_a_test_run_with_none_failed_reports_its_counts(void)
{
    static const char *const s_good[1] = {"test_good.py"};
    repository_created("at_terminal_tests");
    cli_unity_file(s_root, "test_good.py", s_test_good);
    TEST_ASSERT_TRUE(command_run_result("test-run", s_good, 1u));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "1 passed, 0 failed", .cap = CLI_POOL_BYTES}));
}

void test_a_test_run_that_run_result_no_test_fails(void)
{
    static const char *const s_none[1] = {"test_none.py"};
    repository_created("at_terminal_tests");
    cli_unity_file(s_root, "test_none.py", "x = 1\n");
    TEST_ASSERT_FALSE(command_run_result("test-run", s_none, 1u));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_err_buf, .other = "0 passed, 0 failed", .cap = CLI_POOL_BYTES}));
}

void test_settings_sent_are_read_in(void)
{
    char base[CLI_PATH_MAX_LEN];
    char there[CLI_PATH_MAX_LEN];
    char between[CLI_PATH_MAX_LEN];
    settings_sent(base, there, between);
    TEST_ASSERT_TRUE(settings_read(there, between));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "Took 1 file", .cap = CLI_POOL_BYTES}));
    TEST_ASSERT_EQUAL_STRING("body { color: red; }\n", file_text(there, "user.css"));
}

void test_settings_read_again_change_nothing(void)
{
    char base[CLI_PATH_MAX_LEN];
    char there[CLI_PATH_MAX_LEN];
    char between[CLI_PATH_MAX_LEN];
    char required[CLI_PATH_MAX_LEN + 64u];
    settings_sent(base, there, between);
    TEST_ASSERT_TRUE(settings_read(there, between));
    VerbaLinea line = {required, sizeof(required), 0ull};
    mmgr_line_put(&line, between);
    mmgr_line_put(&line, " contains the settings this machine has.");
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_TRUE(settings_read(there, between));
    TEST_ASSERT_EQUAL_STRING(required, s_out);
}

void test_settings_read_in_are_undone(void)
{
    char base[CLI_PATH_MAX_LEN];
    char there[CLI_PATH_MAX_LEN];
    char between[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char id[128] = "";
    settings_sent(base, there, between);
    TEST_ASSERT_TRUE(settings_read(there, between));
    static const char s_undo[] = "preferences sync undo ";
    const char *const undo = cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_POOL_BYTES, .other = s_undo, .other_cap = CLI_POOL_BYTES});
    TEST_ASSERT_NOT_NULL(undo);
    const char *const named = undo + sizeof(s_undo) - 1u;
    for (unsigned int at = 0u; (at + 1u < sizeof(id)) && (named[at] > ' '); at += 1u)
    {
        id[at] = named[at];
        id[at + 1u] = '\0';
    }
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    (void)EMBED_CALL(cli_at_terminal.undo, CliAtTerminalCfg, .home = there, .id = id, .out = &out, .err_msg = &err_msg);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = there, .relative = "user.css", .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_a_git_command_with_no_window_runs_at_the_terminal_and_makes_its_branch(void)
{
    static CliOutcome s_outcome;
    repository_created("at_terminal_command_line");
    const char *const branch[5] = {"--root", s_root, "git", "new-branch", "from-the-terminal"};
    EMBED_CALL(cli_command_line_input.run, CliCommandLineInputCfg, .supplied = branch, .count = 5u, .outcome = &s_outcome);
    TEST_ASSERT_FALSE(s_outcome.window);
    TEST_ASSERT_EQUAL_INT(0, s_outcome.code);
    TEST_ASSERT_TRUE(branches_contain("* from-the-terminal"));
}

void test_a_window_command_with_no_window_opens_a_window(void)
{
    static CliOutcome s_outcome;
    repository_created("at_terminal_command_line");
    const char *const wrap[5] = {"--root", s_root, "view", "word-wrap", "on"};
    EMBED_CALL(cli_command_line_input.run, CliCommandLineInputCfg, .supplied = wrap, .count = 5u, .outcome = &s_outcome);
    TEST_ASSERT_TRUE(s_outcome.window);
}

void test_a_side_branchs_commit_is_listing_to_cherry_pick(void)
{
    char commit[64];
    side_created(commit);
    const unsigned long long size = CLI_UNITY_LENGTH(s_out);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(11ull, size);
    TEST_ASSERT_EQUAL_STRING("on the side", s_out + size - 11u);
}

void test_a_branch_with_its_own_commits_is_deleted_only_when_forced(void)
{
    static const char *const s_side[1] = {"side"};
    static const char *const s_forced[2] = {"side", "--force"};
    char commit[64];
    side_created(commit);
    TEST_ASSERT_FALSE(command_run_result("branch-delete", s_side, 1u));
    CLI_TEST_ASSERT_CONTAINS("--force", s_err_buf);
    (void)command_run_result("branch-delete", s_forced, 2u);
    TEST_ASSERT_FALSE(branches_contain("side"));
}

void test_a_commit_is_cherry_picked_onto_main(void)
{
    char commit[64];
    char path[CLI_PATH_MAX_LEN];
    side_created(commit);
    const char *const pick[1] = {commit};
    (void)command_run_result("cherry-pick", pick, 1u);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "b.txt", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}
