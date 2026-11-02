// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_git.c: a clone is named as git names it, a partly staged file commits that part alone, a branch is made, switched to, renamed, merged and
// deleted, a merged branch has its own column and joins where it began, a repository owns the deepest folder that contains a path, and each
// status has one letter
#include "ui/cli/src/code/git_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_part_text[] = "one\nTWO\nthree\nfour\n";
static const char *const s_repos[3] = {"", "lib", "lib/inner"};

static CliBranches s_branches;
static CliGraph s_graph;
static char s_out[1u << 16u];
static char s_root[CLI_PATH_MAX_LEN];
static char s_error[1024];
static char s_strokes[256];
static char s_answer[1024];

// git run at the root with the words sent, NULL past the last: 1, its output trimmed into s_out, or 0
static int git_run_result(const char *one, const char *two, const char *three, const char *four)
{
    const char *const words[4] = {one, two, three, four};
    unsigned int count = 0u;
    while ((count < 4u) && (words[count] != NULL))
    {
        count += 1u;
    }
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int run_result = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .request = words, .count = count, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return run_result;
}

// a repository made at the folder `name`, its user t
static void repository_created(const char *name)
{
    cli_unity_folder(name, s_root);
    TEST_ASSERT_TRUE(git_run_result("init", "-q", "-b", "main"));
    TEST_ASSERT_TRUE(git_run_result("config", "user.name", "t", NULL));
    TEST_ASSERT_TRUE(git_run_result("config", "user.email", "t@t", NULL));
}

// the file `name` of the tree read into s_out
static const char *file_read(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_out, sizeof(s_out) - 1u);
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    s_out[size] = '\0';
    return s_out;
}

// a clone of `url` under work makes the folder work/orior
static void assert_clone_folder(const char *url)
{
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_git_err_msg_folder.clone_folder, CliGitErrMsgFolderCfg, .url = url, .parent = "work", .folder = folder, .err_msg = &err_msg), url);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("work/orior", folder, url);
}

// a clone of `url` names no folder
static void assert_no_clone_folder(const char *url)
{
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(cli_git_err_msg_folder.clone_folder, CliGitErrMsgFolderCfg, .url = url, .parent = "work", .folder = folder, .err_msg = &err_msg), url);
}

// a commit of a file and its next edit, with another file staged apart
static void part_ready(void)
{
    repository_created("git_parts");
    cli_unity_file(s_root, "a.txt", "one\ntwo\nthree\nfour\n");
    (void)git_run_result("add", ".", NULL, NULL);
    (void)git_run_result("commit", "-q", "-m", "first");
    cli_unity_file(s_root, "a.txt", "one\nTWO\nthree\nFOUR\n");
    cli_unity_file(s_root, "other.txt", "staged apart\n");
    (void)git_run_result("add", "other.txt", NULL, NULL);
}

// the part of a.txt committed: whether it was, the reason in s_error
static int part_committed(const char *message)
{
    const CliGitPart part = {"a.txt", s_part_text, sizeof(s_part_text) - 1u};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {s_answer, sizeof(s_answer), 0ull};
    const int committed = EMBED_CALL(cli_git_root.commit_parts, CliGitRootCfg, .root = s_root, .message = message, .whole = NULL, .full_files_count = 0u, .parts = &part, .part_count = 1u, .out = &line, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return committed;
}

// the branch act `act` on `name`: 1, or 0
static int branch_acted(const char *act, const char *name, const char *to)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {s_answer, sizeof(s_answer), 0ull};
    return EMBED_CALL(cli_git_root_out.branch_act, CliGitRootOutCfg, .root = s_root, .act = act, .name_read = name, .to = to, .out = &line, .err_msg = &err_msg);
}

// a repository on the branch topic, one commit past main
static void topic_created(void)
{
    repository_created("git_branches");
    cli_unity_file(s_root, "a.txt", "one");
    (void)git_run_result("add", ".", NULL, NULL);
    (void)git_run_result("commit", "-q", "-m", "first");
    TEST_ASSERT_TRUE(branch_acted("create", "topic", ""));
    cli_unity_file(s_root, "a.txt", "two");
    (void)git_run_result("commit", "-q", "-am", "second");
}

// a commit of the graph added with its parents
static void commit_added(const char *id, const char *first, const char *second)
{
    CliDrawn *const drawn = &s_graph.item[s_graph.count];
    memor.set(&(MemoriaCfg){.dst = drawn, .val = 0, .bytes = sizeof(*drawn)});
    drawn->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &s_graph.text, .from = id, .length = CLI_UNITY_LENGTH(id));
    const char *const parents[2] = {first, second};
    for (unsigned int at = 0u; (at < 2u) && (parents[at] != NULL); at += 1u)
    {
        drawn->parent[at] = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &s_graph.text, .from = parents[at], .length = CLI_UNITY_LENGTH(parents[at]));
        drawn->parents = at + 1u;
    }
    s_graph.count += 1u;
}

// a graph of a merged branch of two commits laid out
static void graph_laid_out(void)
{
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_graph.text);
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &s_graph.text);
    s_graph.count = 0u;
    s_graph.strokes = 0u;
    commit_added("m", "a3", "b2");
    commit_added("a3", "a2", NULL);
    commit_added("b2", "b1", NULL);
    commit_added("b1", "a2", NULL);
    commit_added("a2", "a1", NULL);
    commit_added("a1", NULL, NULL);
    EMBED_CALL(cli_git_found_internal.layout_out, CliGitFoundCtx, .found = &s_graph);
}

// the strokes of row `index`, each `from to half color` joined by `|`, in s_strokes
static const char *strokes_of(unsigned int index)
{
    const CliDrawn *const drawn = &s_graph.item[index];
    VerbaLinea line = {s_strokes, sizeof(s_strokes), 0ull};
    for (unsigned int at = 0u; at < drawn->stroke_count; at += 1u)
    {
        const unsigned int *const stroke = s_graph.stroke[drawn->first_stroke + at];
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        for (unsigned int part = 0u; part < 4u; part += 1u)
        {
            mmgr_line_put(&line, (part > 0u) ? " " : "");
            mmgr_line_decimal(&line, stroke[part], 1u);
        }
    }
    (void)mmgr_line_finish(&line);
    return s_strokes;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_clone_of_an_https_url_ending_in_git(void)
{
    assert_clone_folder("https://github.com/dstroy0/orior.git");
}

void test_a_clone_of_an_https_url_ending_in_a_slash(void)
{
    assert_clone_folder("https://github.com/dstroy0/orior/");
}

void test_a_clone_of_an_ssh_address(void)
{
    assert_clone_folder("git@github.com:dstroy0/orior.git");
}

void test_a_clone_of_a_short_ssh_address(void)
{
    assert_clone_folder("git@host:orior");
}

void test_a_clone_of_a_windows_path(void)
{
    assert_clone_folder("D:\\repos\\orior.git");
}

void test_a_url_with_no_path_names_no_folder(void)
{
    assert_no_clone_folder("https://");
}

void test_blanks_name_no_folder(void)
{
    assert_no_clone_folder("  ");
}

void test_a_commit_with_no_message_is_refused(void)
{
    part_ready();
    TEST_ASSERT_FALSE(part_committed("  \n"));
    TEST_ASSERT_EQUAL_STRING("a commit needs a message", s_error);
}

void test_a_commit_releases_its_memory_created_or_refused(void)
{
    part_ready();
    const unsigned long long before = embed_atomic_usize_load(&g_cli_git_cells.in_use);
    (void)part_committed("  \n");
    TEST_ASSERT_EQUAL_UINT64(before, embed_atomic_usize_load(&g_cli_git_cells.in_use));
    TEST_ASSERT_TRUE(part_committed("part of a"));
    TEST_ASSERT_EQUAL_UINT64(before, embed_atomic_usize_load(&g_cli_git_cells.in_use));
}

void test_the_commit_contains_the_part(void)
{
    part_ready();
    TEST_ASSERT_TRUE(part_committed("part of a"));
    TEST_ASSERT_TRUE(git_run_result("show", "HEAD:a.txt", NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("one\nTWO\nthree\nfour", s_out);
}

void test_the_file_stays_as_it_was(void)
{
    part_ready();
    TEST_ASSERT_TRUE(part_committed("part of a"));
    TEST_ASSERT_EQUAL_STRING("one\nTWO\nthree\nFOUR\n", file_read("a.txt"));
}

void test_the_commit_contains_that_file_alone(void)
{
    part_ready();
    TEST_ASSERT_TRUE(part_committed("part of a"));
    TEST_ASSERT_TRUE(git_run_result("show", "--name-only", "--format=", "HEAD"));
    TEST_ASSERT_EQUAL_STRING("a.txt", s_out);
}

void test_what_was_staged_apart_stays_staged(void)
{
    part_ready();
    TEST_ASSERT_TRUE(part_committed("part of a"));
    TEST_ASSERT_TRUE(git_run_result("status", "--porcelain", NULL, NULL));
    // git's output comes trimmed, the first line's leading space with it
    TEST_ASSERT_EQUAL_STRING("M a.txt\nA  other.txt", s_out);
}

void test_the_tree_is_on_a_new_branch_with_its_last_commits_subject(void)
{
    topic_created();
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_branches.text);
    EMBED_CALL(cli_git_root_found_branches.branches, CliGitRootFoundBranchesCfg, .root = s_root, .found_branches_pointer = &s_branches);
    int current = 0;
    int subject = 0;
    for (unsigned int at = 0u; at < s_branches.count; at += 1u)
    {
        const CliBranch *const branch = &s_branches.item[at];
        const int topic = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_branches.text, .at = branch->name), .other = "topic", .cap = CLI_CELLS_POOL});
        current = current || (branch->current && topic);
        subject = subject || (topic && cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_branches.text, .at = branch->subject), .other = "second", .cap = CLI_CELLS_POOL}));
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_branches.text);
    TEST_ASSERT_TRUE(current);
    TEST_ASSERT_TRUE(subject);
}

void test_a_branch_is_renamed_and_another_switched_to(void)
{
    topic_created();
    TEST_ASSERT_TRUE(branch_acted("rename", "topic", "feature"));
    TEST_ASSERT_TRUE(branch_acted("switch", "main", ""));
}

void test_a_branch_not_merged_is_not_deleted(void)
{
    topic_created();
    TEST_ASSERT_TRUE(branch_acted("switch", "main", ""));
    TEST_ASSERT_FALSE(branch_acted("delete", "topic", ""));
}

void test_a_branch_is_merged(void)
{
    topic_created();
    TEST_ASSERT_TRUE(branch_acted("switch", "main", ""));
    TEST_ASSERT_TRUE(branch_acted("merge", "topic", ""));
    TEST_ASSERT_EQUAL_STRING("two", file_read("a.txt"));
}

void test_a_merged_branch_is_deleted_and_main_alone_is_left(void)
{
    topic_created();
    TEST_ASSERT_TRUE(branch_acted("switch", "main", ""));
    TEST_ASSERT_TRUE(branch_acted("merge", "topic", ""));
    TEST_ASSERT_TRUE(branch_acted("delete", "topic", ""));
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_branches.text);
    EMBED_CALL(cli_git_root_found_branches.branches, CliGitRootFoundBranchesCfg, .root = s_root, .found_branches_pointer = &s_branches);
    TEST_ASSERT_EQUAL_UINT(1u, s_branches.count);
    TEST_ASSERT_EQUAL_STRING("main", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_branches.text, .at = s_branches.item[0].name));
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_branches.text);
}

void test_a_name_git_refuses_is_refused(void)
{
    topic_created();
    TEST_ASSERT_FALSE(branch_acted("create", "-x", ""));
    TEST_ASSERT_FALSE(branch_acted("create", "bad..name", ""));
}

void test_a_merged_branch_has_its_own_column(void)
{
    static const unsigned int s_lanes[6] = {0u, 0u, 1u, 1u, 0u, 0u};
    graph_laid_out();
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_lanes[at], s_graph.item[at].lane);
    }
}

void test_the_merge_opens_the_column(void)
{
    graph_laid_out();
    TEST_ASSERT_EQUAL_STRING("0 0 1 0|0 1 1 1", strokes_of(0u));
}

void test_each_column_continues_end_anothers_commit(void)
{
    graph_laid_out();
    TEST_ASSERT_EQUAL_STRING("0 0 0 0|1 1 0 1|0 0 1 0|1 1 1 1", strokes_of(1u));
}

void test_a_column_joins_where_it_began(void)
{
    graph_laid_out();
    CLI_TEST_ASSERT_CONTAINS("1 0 1 0", strokes_of(3u));
}

void test_one_column_remains_after_the_join(void)
{
    graph_laid_out();
    TEST_ASSERT_EQUAL_STRING("0 0 0 0|0 0 1 0", strokes_of(4u));
}

void test_the_first_commit_ends_the_column(void)
{
    graph_laid_out();
    TEST_ASSERT_EQUAL_STRING("0 0 0 0", strokes_of(5u));
}

void test_a_branchs_line_has_its_own_color(void)
{
    graph_laid_out();
    TEST_ASSERT_EQUAL_UINT(1u, s_graph.item[2].color);
}

void test_a_file_at_the_top_belongs_to_the_top_repository(void)
{
    TEST_ASSERT_EQUAL_STRING("", EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos, .count = 3u, .path = "a.txt", .length = 5ull));
}

void test_a_file_in_a_repository_belongs_to_it(void)
{
    TEST_ASSERT_EQUAL_STRING("lib", EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos, .count = 3u, .path = "lib/a.txt", .length = 9ull));
}

void test_the_deepest_repository_owner_a_path(void)
{
    TEST_ASSERT_EQUAL_STRING("lib/inner", EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos, .count = 3u, .path = "lib/inner/b/c.txt", .length = 17ull));
}

void test_a_folder_sharing_a_prefix_is_not_the_repository(void)
{
    TEST_ASSERT_EQUAL_STRING("", EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos, .count = 3u, .path = "library/a.txt", .length = 13ull));
}

void test_a_path_no_repository_contains_has_no_owner(void)
{
    TEST_ASSERT_NULL(EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos + 1, .count = 2u, .path = "a.txt", .length = 5ull));
}

void test_each_status_has_one_letter(void)
{
    TEST_ASSERT_EQUAL_CHAR('U', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = '?', .tree = '?'));
    TEST_ASSERT_EQUAL_CHAR('M', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = ' ', .tree = 'M'));
    TEST_ASSERT_EQUAL_CHAR('M', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = 'M', .tree = ' '));
    TEST_ASSERT_EQUAL_CHAR('A', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = 'A', .tree = ' '));
    TEST_ASSERT_EQUAL_CHAR('D', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = ' ', .tree = 'D'));
    TEST_ASSERT_EQUAL_CHAR('R', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = 'R', .tree = ' '));
    TEST_ASSERT_EQUAL_CHAR('C', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = 'U', .tree = 'U'));
    TEST_ASSERT_EQUAL_CHAR('C', EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = 'A', .tree = 'A'));
}

void test_each_lists_texts_are_released(void)
{
    graph_laid_out();
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_graph.text);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_git_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_git_cells.hw));
}
