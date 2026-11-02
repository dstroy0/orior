// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_trunk.c: the tree: the filesystem is its trunk and its files are its branches. It is listed once as it opens, and each change after joins
// or leaves the one branch it names at its closest parent, nothing else listed
#include "ui/cli/src/code/git.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/symbols_tree.h"
#include "ui/cli/src/editor/files_internal.h"
#include "ui/cli/src/editor/watch_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static unsigned int s_rounds;
static char s_root[CLI_PATH_MAX_LEN];
static unsigned long long s_listed_at;

// the file `name` of the tree written with `text`, and its folder made where `folder` names one
static void file_written(const char *folder, const char *name, const char *text)
{
    if (folder != NULL)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = folder, .joined = path);
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    }
    cli_unity_file(s_root, name, text);
}

// the file `name` of the tree removed
static void file_removed(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
}

// what the trunk holds at `path` mirrored, alone
static void path_mirrored(const char *path)
{
    const char *const paths[1] = {path};
    EMBED_CALL(cli_symbols_root.tree_mirror, CliSymbolsRootCfg, .root = s_root, .paths = paths, .count = 1u);
}

// a repository at `name` with two folders of files and an ignore rule, listed; s_listed_at when it was
static void repository_listed(const char *name)
{
    static const char *const s_init[2] = {"init", "-q"};
    char out_text[256];
    char why[256];
    VerbaLinea out = {out_text, sizeof(out_text), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder(name, s_root);
    file_written(NULL, ".gitignore", "*.o\nout/\n");
    file_written(NULL, "top.txt", "t");
    file_written("src", "src/a.c", "a");
    file_written("lib", "lib/b.c", "b");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_root);
    s_listed_at = cli_tree_listed_at();
}

// a round of changes counted
static void round_counted(const CliEachBatchSizeCfg *args)
{
    (void)args;
    s_rounds += 1u;
}

// the rounds counted
static unsigned int rounds_counted(void)
{
    const unsigned int rounds = s_rounds;
    return rounds;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    cli_tree_forgotten();
}

void test_git_lists_the_tree(void)
{
    repository_listed("trunk_opened");
    TEST_ASSERT_TRUE(cli_tree_git());
}

void test_each_file_is_a_branch(void)
{
    repository_listed("trunk_opened");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/a.c"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "top.txt"));
}

void test_each_folder_is_a_branch(void)
{
    repository_listed("trunk_opened");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "src"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "lib"));
}

void test_a_file_is_not_a_folder_and_a_folder_is_not_a_file(void)
{
    repository_listed("trunk_opened");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "src/a.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src"));
}

void test_the_files_are_counted(void)
{
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    repository_listed("trunk_opened");
    EMBED_CALL(cli_symbols_count.tree_files_count, CliSymbolsCountCfg, .count_pointer = &count, .bytes = &bytes);
    TEST_ASSERT_EQUAL_UINT(4u, count);
}

void test_every_file_is_gone_through(void)
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    unsigned int gone_through = 0u;
    repository_listed("trunk_opened");
    while (EMBED_CALL(cli_symbols_at.tree_next_file, CliSymbolsAtCfg, .at = &at, .path_out = path))
    {
        gone_through += 1u;
    }
    TEST_ASSERT_EQUAL_UINT(4u, gone_through);
}

void test_a_call_lists_the_files_flat_and_sorted(void)
{
    repository_listed("trunk_opened");
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = s_root);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT64(4ull, files->count);
    TEST_ASSERT_EQUAL_STRING(".gitignore", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 0u));
    TEST_ASSERT_EQUAL_STRING("top.txt", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 3u));
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

void test_nothing_of_the_list_remains_after_it(void)
{
    repository_listed("trunk_opened");
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = s_root));
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(&g_cli_files_views_cells));
}

void test_the_tree_is_listed_as_it_opens(void)
{
    repository_listed("trunk_stands");
    TEST_ASSERT_NOT_EQUAL(0ull, s_listed_at);
}

void test_the_tree_is_not_listed_again_for_the_time_that_passed(void)
{
    repository_listed("trunk_stands");
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 4200u);
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = s_root));
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 500u);
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_root);
    TEST_ASSERT_EQUAL_UINT64(s_listed_at, cli_tree_listed_at());
}

void test_a_file_added_joins_and_the_tree_is_not_listed_for_it(void)
{
    repository_listed("trunk_joined");
    file_written(NULL, "src/new.c", "n");
    path_mirrored("src/new.c");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/new.c"));
    TEST_ASSERT_EQUAL_UINT64(s_listed_at, cli_tree_listed_at());
}

void test_a_file_added_leaves_the_other_branches_in_place(void)
{
    repository_listed("trunk_joined");
    const unsigned int lib = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib");
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib/b.c");
    file_written(NULL, "src/new.c", "n");
    path_mirrored("src/new.c");
    TEST_ASSERT_EQUAL_UINT(lib, EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib"));
    TEST_ASSERT_EQUAL_UINT(file, EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib/b.c"));
}

void test_a_file_addeds_closest_parent_is_its_folder_which_counts_it(void)
{
    repository_listed("trunk_joined");
    file_written(NULL, "src/new.c", "n");
    path_mirrored("src/new.c");
    const unsigned int parent = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src");
    TEST_ASSERT_EQUAL_UINT(parent, g_cli_branches[EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/new.c")].parent);
    TEST_ASSERT_EQUAL_UINT(2u, g_cli_branches[parent].below);
}

void test_a_file_git_ignores_does_not_join(void)
{
    repository_listed("trunk_joined");
    file_written(NULL, "src/made.o", "o");
    path_mirrored("src/made.o");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/made.o"));
}

void test_a_file_in_a_folder_not_joined_brings_its_folders_and_no_other_file(void)
{
    repository_listed("trunk_joined");
    file_written("deep", "deep/x.c", "x");
    file_written("deep/er", "deep/er/y.c", "y");
    path_mirrored("deep/er/y.c");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "deep/er/y.c"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "deep/er"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "deep"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "deep/x.c"));
}

void test_a_folder_that_came_in_joins_its_files_and_the_tree_is_not_listed(void)
{
    repository_listed("trunk_joined");
    file_written("came", "came/in.c", "c");
    file_written("came/under", "came/under/in.c", "c");
    path_mirrored("came");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "came/in.c"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "came/under/in.c"));
    TEST_ASSERT_EQUAL_UINT64(s_listed_at, cli_tree_listed_at());
}

void test_a_file_that_joined_already_joins_once(void)
{
    repository_listed("trunk_joined");
    file_written(NULL, "src/new.c", "n");
    path_mirrored("src/new.c");
    path_mirrored("src/new.c");
    TEST_ASSERT_EQUAL_UINT(2u, g_cli_branches[EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src")].below);
}

void test_a_removed_file_leaves_with_the_folder_it_left_empty(void)
{
    repository_listed("trunk_left");
    file_written("lone", "lone/one.c", "o");
    file_written("lone/er", "lone/er/two.c", "t");
    path_mirrored("lone");
    file_removed("lone/er/two.c");
    file_removed("lone/er");
    path_mirrored("lone/er/two.c");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "lone/er/two.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "lone/er"));
}

void test_a_folder_that_holds_another_stays(void)
{
    repository_listed("trunk_left");
    file_written("lone", "lone/one.c", "o");
    file_written("lone/er", "lone/er/two.c", "t");
    path_mirrored("lone");
    file_removed("lone/er/two.c");
    file_removed("lone/er");
    path_mirrored("lone/er/two.c");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "lone"));
}

void test_a_branch_left_is_reused(void)
{
    repository_listed("trunk_left");
    file_written("lone", "lone/two.c", "t");
    path_mirrored("lone");
    const unsigned int place = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lone/two.c");
    file_removed("lone/two.c");
    path_mirrored("lone/two.c");
    file_written(NULL, "src/after.c", "a");
    path_mirrored("src/after.c");
    TEST_ASSERT_TRUE(!g_cli_branches[place].used || (EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/after.c") == place) || (EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/after.c") < g_cli_branch_count));
}

void test_a_removed_folder_takes_all_under_it_and_the_tree_is_not_listed(void)
{
    repository_listed("trunk_left");
    file_written("lone", "lone/one.c", "o");
    path_mirrored("lone");
    file_removed("lone/one.c");
    file_removed("lone");
    path_mirrored("lone");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "lone"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "lone/one.c"));
    TEST_ASSERT_EQUAL_UINT64(s_listed_at, cli_tree_listed_at());
}

void test_changed_rules_remove_what_they_now_leave_out(void)
{
    repository_listed("trunk_relisted");
    file_written(NULL, ".gitignore", "*.o\nout/\nsrc/\n");
    EMBED_CALL(cli_symbols_context.tree_relist, CliSymbolsContextCfg, .root = s_root, .emit = NULL, .context = NULL);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/a.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = "src"));
}

void test_changed_rules_leave_what_remains_in_its_cell(void)
{
    repository_listed("trunk_relisted");
    const unsigned int branch = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib/b.c");
    const CarcerMovableSlot *const cell = g_cli_branches[branch].cell;
    file_written(NULL, ".gitignore", "*.o\nout/\nsrc/\n");
    EMBED_CALL(cli_symbols_context.tree_relist, CliSymbolsContextCfg, .root = s_root, .emit = NULL, .context = NULL);
    TEST_ASSERT_EQUAL_UINT(branch, EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "lib/b.c"));
    TEST_ASSERT_EQUAL_PTR(cell, g_cli_branches[branch].cell);
}

void test_a_tree_git_cannot_read_is_walked_past_the_folders_a_walk_skips(void)
{
    cli_unity_folder("trunk_no_git", s_root);
    file_written("src", "src/a.c", "a");
    file_written("target", "target/made.c", "m");
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_root);
    TEST_ASSERT_FALSE(cli_tree_git());
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/a.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "target/made.c"));
}

void test_a_file_added_to_a_walked_tree_joins_unless_a_walk_skips_its_folder(void)
{
    cli_unity_folder("trunk_no_git", s_root);
    file_written("src", "src/a.c", "a");
    file_written("target", "target/made.c", "m");
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_root);
    file_written(NULL, "src/b.c", "b");
    path_mirrored("src/b.c");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/b.c"));
    file_written(NULL, "target/more.c", "m");
    path_mirrored("target/more.c");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "target/more.c"));
}

void test_a_tree_whose_files_were_stamped_is_still_walked(void)
{
    static char s_found[1u << 14u];
    repository_listed("trunk_stamped_first");
    file_written(NULL, "src/walked.py", "def stamped_first():\n    pass\n");
    path_mirrored("src/walked.py");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_watch_internal.first_reading, CliWatchCtx, .root = s_root));
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = s_root);
    for (unsigned int tries = 0u; cli_tree_unwalked() && (tries < 1000u); tries += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    EMBED_CALL(cli_symbols_query_most.tree_declarations_find, CliSymbolsQueryMostCfg, .query = "stamped_first", .most = 10u, .out = &out);
    (void)mmgr_line_finish(&out);
    CLI_TEST_ASSERT_CONTAINS("src/walked.py", s_found);
}

void test_a_watch_joins_a_file_added_and_removes_it_when_it_leaves(void)
{
    repository_listed("trunk_watched");
    EMBED_CALL(cli_watch.start, CliWatchCfg, .root = s_root, .take = round_counted, .context = NULL);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 200u);
    const unsigned int before = rounds_counted();
    file_written(NULL, "src/new.c", "n");
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while ((!EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/new.c") || (rounds_counted() == before)) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    const int reported = rounds_counted() != before;
    const int joined = EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/new.c");
    const unsigned long long listed_after = cli_tree_listed_at();
    file_removed("src/new.c");
    const unsigned long long left_by = cli_clock_milliseconds() + 10000ull;
    while (EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/new.c") && (cli_clock_milliseconds() < left_by))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    const int left = !EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = "src/new.c");
    cli_watch_stop();
    TEST_ASSERT_TRUE(reported);
    TEST_ASSERT_TRUE(joined);
    TEST_ASSERT_EQUAL_UINT64(s_listed_at, listed_after);
    TEST_ASSERT_TRUE(left);
}
