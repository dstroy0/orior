// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_git_history.c: a commit lists the files it changed and a search finds it; each line has the commit that last changed it; each repository of a
// tree tracks its own changes and commits; and changes are stashed and restored, and a commit is cherry-picked from another branch
#include "ui/cli/src/code/git.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliTouchedList s_touched;
static CliGraph s_graph;
static CliLineHistory s_lines;
static CliRepositories s_repos;
static CliChangedList s_changed;
static CliCommits s_commits;
static CliStashes s_stashes;
static char s_out[1u << 16u];
static char s_error[1024];
static char s_root[CLI_PATH_MAX_LEN];
static char s_inner[CLI_PATH_MAX_LEN];

static int s_graph_created;
static char s_first_id[CLI_NAME_MAX_LEN];
static char s_second_id[CLI_NAME_MAX_LEN];
static char s_graph_root[CLI_PATH_MAX_LEN];

static int s_lines_created;
static char s_lines_sub[CLI_PATH_MAX_LEN];
static char s_lines_first[CLI_NAME_MAX_LEN];

static int s_repos_created;
static char s_repos_root[CLI_PATH_MAX_LEN];
static char s_repos_inner[CLI_PATH_MAX_LEN];
static int s_repos_committed;
static unsigned int s_repos_changed_before;
static int s_repos_a_changed;
static int s_repos_b_changed;
static char s_repos_outer_subject[64];
static char s_repos_inner_subject[64];
static unsigned int s_repos_changed_after;

// git run at `root` with the words sent, NULL past the last: 1, its output trimmed into s_out, or 0
static int git_run_result(const char *root, const char *one, const char *two, const char *three, const char *four)
{
    const char *const words[4] = {one, two, three, four};
    unsigned int count = 0u;
    while ((count < 4u) && (words[count] != NULL))
    {
        count += 1u;
    }
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int run_result = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root, .request = words, .count = count, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return run_result;
}

// a repository made at `root` on `branch`, its user `user`, `file` containing `text` committed as its first
static void repository_started(const char *root, const char *branch, const char *user, const char *file, const char *text)
{
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = root);
    TEST_ASSERT_TRUE(git_run_result(root, "init", "-q", "-b", branch));
    TEST_ASSERT_TRUE(git_run_result(root, "config", "user.name", user, NULL));
    TEST_ASSERT_TRUE(git_run_result(root, "config", "user.email", "t@t", NULL));
    TEST_ASSERT_TRUE(git_run_result(root, "config", "commit.gpgsign", "false", NULL));
    if (file != NULL)
    {
        cli_unity_file(root, file, text);
        (void)git_run_result(root, "add", file, NULL, NULL);
        (void)git_run_result(root, "commit", "-q", "-m", "first");
    }
}

// the file `name` of the tree `root` is there
static int file_there(const char *root, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// a touched file `path state was` is among those found
static int touched_contains(const char *required)
{
    for (unsigned int at = 0u; at < s_touched.count; at += 1u)
    {
        const CliTouched *const one = &s_touched.item[at];
        char joined[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {joined, sizeof(joined), 0ull};
        mmgr_line_put(&line, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_touched.text, .at = one->path));
        mmgr_line_ch(&line, ' ');
        mmgr_line_ch(&line, one->state);
        if (one->has_was)
        {
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_touched.text, .at = one->was));
        }
        (void)mmgr_line_finish(&line);
        if (cellul.eq(&(CatenaFinitaCfg){.src = joined, .other = required, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// `text` copied into `out`, which is sized for `capacity`
static void text_stored(char *out, unsigned long long capacity, const char *text)
{
    VerbaLinea line = {out, capacity, 0ull};
    mmgr_line_put(&line, text);
    (void)mmgr_line_finish(&line);
}

// a repository of two commits, the second renaming, removing and adding, made once for the process
static void graph_created(void)
{
    if (s_graph_created)
    {
        return;
    }
    s_graph_created = 1;
    cli_unity_folder("git_graph", s_graph_root);
    repository_started(s_graph_root, "main", "Grace", NULL, NULL);
    cli_unity_file(s_graph_root, "a.txt", "one\ntwo\nthree\nfour\n");
    cli_unity_file(s_graph_root, "b.txt", "b\n");
    (void)git_run_result(s_graph_root, "add", ".", NULL, NULL);
    (void)git_run_result(s_graph_root, "commit", "-q", "-m", "first");
    (void)git_run_result(s_graph_root, "mv", "a.txt", "c.txt", NULL);
    (void)git_run_result(s_graph_root, "rm", "-q", "b.txt", NULL);
    cli_unity_file(s_graph_root, "d.txt", "d\n");
    (void)git_run_result(s_graph_root, "add", "-A", NULL, NULL);
    (void)git_run_result(s_graph_root, "commit", "-q", "-m", "Second, renamed");
    (void)git_run_result(s_graph_root, "rev-parse", "HEAD~1", NULL, NULL);
    text_stored(s_first_id, sizeof(s_first_id), s_out);
    (void)git_run_result(s_graph_root, "rev-parse", "HEAD", NULL, NULL);
    text_stored(s_second_id, sizeof(s_second_id), s_out);
}

// the graph read for the search `words`
static void graph_read(const char *words, int authors)
{
    graph_created();
    g_cli_guard->persistent_buf_release(s_graph.text.bytes, g_cli_git_owner);
    s_graph.text.bytes = NULL;
    s_graph.text.used = 0ULL;
    s_graph.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_query.graph, CliGitRootQueryCfg, .root = s_graph_root, .query = words, .authors = authors, .found_graph_pointer = &s_graph);
}

// a file of two commits in a sub-folder of a repository, made once for the process
static void lines_created(void)
{
    char base[CLI_PATH_MAX_LEN];
    if (s_lines_created)
    {
        return;
    }
    s_lines_created = 1;
    cli_unity_folder("git_lines", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "sub", .joined = s_lines_sub);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_lines_sub);
    repository_started(base, "main", "Grace", NULL, NULL);
    cli_unity_file(s_lines_sub, "a.txt", "one\ntwo\n");
    (void)git_run_result(base, "add", ".", NULL, NULL);
    (void)git_run_result(base, "commit", "-q", "-m", "first");
    cli_unity_file(s_lines_sub, "a.txt", "one\nsecond\n");
    (void)git_run_result(base, "commit", "-q", "-am", "second");
    (void)git_run_result(base, "rev-parse", "HEAD~1", NULL, NULL);
    text_stored(s_lines_first, sizeof(s_lines_first), s_out);
}

// the line history of the file with a third line uncommitted
static void line_history_read(void)
{
    static const char s_text[] = "one\nsecond\nnew\n";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    lines_created();
    g_cli_guard->persistent_buf_release(s_lines.text.bytes, g_cli_git_owner);
    s_lines.text.bytes = NULL;
    s_lines.text.used = 0ULL;
    s_lines.text.capacity = 0ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_length.line_history, CliGitLengthCfg, .root = s_lines_sub, .file = "a.txt", .text_text = s_text, .length = sizeof(s_text) - 1u, .found_line_history_pointer = &s_lines, .err_msg = &err_msg));
}

// the subject of the commit that last changed line `line` of s_lines, "-" for none
static const char *line_subject(unsigned int line)
{
    const int at = s_lines.line[line];
    return (at < 0) ? "-" : EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = s_lines.commit[at].subject);
}

// whether the changed files include `path` in `state`
static int changed_contains(const char *path, char state)
{
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text, .at = s_changed.item[at].path), .other = path, .cap = CLI_POOL_BYTES}) && (s_changed.item[at].state == state))
        {
            return 1;
        }
    }
    return 0;
}

// a repository with another inside it, each changed then committed across both, recorded once for the process
static void repositories_created(void)
{
    static const char *const s_paths[2] = {"a.txt", "libs/inner/b.txt"};
    char libs[CLI_PATH_MAX_LEN];
    char out[1024];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    if (s_repos_created)
    {
        return;
    }
    s_repos_created = 1;
    cli_unity_folder("git_repos", s_repos_root);
    repository_started(s_repos_root, "main", "t", "a.txt", "one\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_repos_root, .relative = "libs", .joined = libs);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = libs);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_repos_root, .relative = "libs/inner", .joined = s_repos_inner);
    repository_started(s_repos_inner, "trunk", "t", "b.txt", "two\n");
    g_cli_guard->persistent_buf_release(s_repos.text.bytes, g_cli_git_owner);
    s_repos.text.bytes = NULL;
    s_repos.text.used = 0ULL;
    s_repos.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = s_repos_root, .found_repositories_pointer = &s_repos);
    cli_unity_file(s_repos_root, "a.txt", "one more\n");
    cli_unity_file(s_repos_inner, "b.txt", "two more\n");
    g_cli_guard->persistent_buf_release(s_changed.text.bytes, g_cli_git_owner);
    s_changed.text.bytes = NULL;
    s_changed.text.used = 0ULL;
    s_changed.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = s_repos_root, .repos = NULL, .count = 0u, .named = 0, .found_changed_list_pointer = &s_changed);
    s_repos_changed_before = s_changed.count;
    s_repos_a_changed = changed_contains("a.txt", 'M');
    s_repos_b_changed = changed_contains("libs/inner/b.txt", 'M');
    s_repos_committed = EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = s_repos_root, .message = "both", .paths = s_paths, .count = 2u, .out = &line, .err_msg = &err_msg);
    (void)git_run_result(s_repos_root, "log", "-1", "--format=%s", NULL);
    text_stored(s_repos_outer_subject, sizeof(s_repos_outer_subject), s_out);
    (void)git_run_result(s_repos_inner, "log", "-1", "--format=%s", NULL);
    text_stored(s_repos_inner_subject, sizeof(s_repos_inner_subject), s_out);
    g_cli_guard->persistent_buf_release(s_changed.text.bytes, g_cli_git_owner);
    s_changed.text.bytes = NULL;
    s_changed.text.used = 0ULL;
    s_changed.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = s_repos_root, .repos = NULL, .count = 0u, .named = 0, .found_changed_list_pointer = &s_changed);
    s_repos_changed_after = s_changed.count;
}

// a stash act: 1, or 0
static int stash_acted(const char *act, const char *name, const char *message)
{
    char why[1024];
    char out[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    return EMBED_CALL(cli_git_root_out_message.stash_act, CliGitRootOutMessageCfg, .root = s_root, .act = act, .name_read = name, .message = message, .out = &line, .err_msg = &err_msg);
}

// a repository with a changed file and a new one, stashed
static void changes_stashed(void)
{
    cli_unity_folder("git_stash", s_root);
    repository_started(s_root, "main", "t", "a.txt", "one\n");
    cli_unity_file(s_root, "a.txt", "changed\n");
    cli_unity_file(s_root, "b.txt", "new\n");
    TEST_ASSERT_TRUE(stash_acted("push", "", "half done"));
}

// the stashes listed
static void stashes_read(void)
{
    g_cli_guard->persistent_buf_release(s_stashes.text.bytes, g_cli_git_owner);
    s_stashes.text.bytes = NULL;
    s_stashes.text.used = 0ULL;
    s_stashes.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_found_stashes.stashes, CliGitRootFoundStashesCfg, .root = s_root, .found_stashes_pointer = &s_stashes);
}

// a repository with a commit on topic, on main: the commit's id in `picked`, which is sized for CLI_NAME_MAX_LEN
static void topic_committed(char *picked)
{
    cli_unity_folder("git_pick", s_root);
    repository_started(s_root, "main", "t", "a.txt", "one\n");
    (void)git_run_result(s_root, "switch", "-q", "-c", "topic");
    cli_unity_file(s_root, "c.txt", "picked\n");
    (void)git_run_result(s_root, "add", ".", NULL, NULL);
    (void)git_run_result(s_root, "commit", "-q", "-m", "on topic");
    (void)git_run_result(s_root, "rev-parse", "HEAD", NULL, NULL);
    text_stored(picked, CLI_NAME_MAX_LEN, s_out);
    (void)git_run_result(s_root, "switch", "-q", "main", NULL);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_commit_lists_the_files_it_changed_a_rename_with_the_name_it_had(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    graph_created();
    g_cli_guard->persistent_buf_release(s_touched.text.bytes, g_cli_git_owner);
    s_touched.text.bytes = NULL;
    s_touched.text.used = 0ULL;
    s_touched.text.capacity = 0ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = s_graph_root, .id = s_second_id, .found_touched_list_pointer = &s_touched, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(3u, s_touched.count);
    TEST_ASSERT_TRUE(touched_contains("b.txt D"));
    TEST_ASSERT_TRUE(touched_contains("c.txt R a.txt"));
    TEST_ASSERT_TRUE(touched_contains("d.txt A"));
}

void test_the_first_commit_lists_the_files_it_added(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    graph_created();
    g_cli_guard->persistent_buf_release(s_touched.text.bytes, g_cli_git_owner);
    s_touched.text.bytes = NULL;
    s_touched.text.used = 0ULL;
    s_touched.text.capacity = 0ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = s_graph_root, .id = s_first_id, .found_touched_list_pointer = &s_touched, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(2u, s_touched.count);
}

void test_an_id_that_is_not_hex_is_refused(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    graph_created();
    g_cli_guard->persistent_buf_release(s_touched.text.bytes, g_cli_git_owner);
    s_touched.text.bytes = NULL;
    s_touched.text.used = 0ULL;
    s_touched.text.capacity = 0ULL;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = s_graph_root, .id = "not-hex", .found_touched_list_pointer = &s_touched, .err_msg = &err_msg));
}

void test_the_graph_contains_every_commit_its_branch_and_its_author(void)
{
    int main_ref = 0;
    graph_read("", 0);
    TEST_ASSERT_EQUAL_UINT(2u, s_graph.count);
    for (unsigned int at = 0u; at < s_graph.item[0].refs; at += 1u)
    {
        main_ref = main_ref || (cellul.find(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_graph.text, .at = s_graph.item[0].ref[at]), .cap = CLI_POOL_BYTES, .other = "main", .other_cap = CLI_POOL_BYTES}) != NULL);
    }
    TEST_ASSERT_EQUAL_STRING("Second, renamed", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_graph.text, .at = s_graph.item[0].subject));
    TEST_ASSERT_EQUAL_STRING("first", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_graph.text, .at = s_graph.item[1].subject));
    TEST_ASSERT_TRUE(main_ref);
    TEST_ASSERT_EQUAL_STRING("Grace", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_graph.text, .at = s_graph.item[0].author));
}

void test_a_search_finds_a_commit_each_word_in_any_case(void)
{
    graph_read("RENAMED grace", 1);
    TEST_ASSERT_EQUAL_UINT(1u, s_graph.count);
    TEST_ASSERT_EQUAL_STRING(s_second_id, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_graph.text, .at = s_graph.item[0].id));
}

// the author's name holds characters no hex id holds: a commit's id cannot match the name by chance
void test_an_author_is_matched_only_where_authors_show(void)
{
    graph_read("RENAMED grace", 0);
    TEST_ASSERT_EQUAL_UINT(0u, s_graph.count);
}

void test_a_search_matches_a_subject_alone(void)
{
    graph_read("RENAMED", 0);
    TEST_ASSERT_EQUAL_UINT(1u, s_graph.count);
}

void test_a_search_nothing_contains_finds_nothing(void)
{
    graph_read("nowhere", 0);
    TEST_ASSERT_EQUAL_UINT(0u, s_graph.count);
}

void test_each_line_has_the_commit_that_last_changed_it_a_new_line_none(void)
{
    line_history_read();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(3u, s_lines.lines);
    TEST_ASSERT_EQUAL_STRING("first", line_subject(0u));
    TEST_ASSERT_EQUAL_STRING("second", line_subject(1u));
    TEST_ASSERT_EQUAL_STRING("-", line_subject(2u));
}

void test_a_lines_commit_has_its_author_path_and_parent(void)
{
    line_history_read();
    const CliLineCommit *const second = &s_lines.commit[(s_lines.line[1] < 0) ? 0 : s_lines.line[1]];
    TEST_ASSERT_EQUAL_STRING("Grace", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = second->author));
    TEST_ASSERT_TRUE(second->has_path);
    TEST_ASSERT_EQUAL_STRING("a.txt", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = second->path));
    TEST_ASSERT_TRUE(second->has_parent);
    TEST_ASSERT_EQUAL_STRING(s_lines_first, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = second->parent));
    TEST_ASSERT_TRUE(second->has_was);
    TEST_ASSERT_EQUAL_STRING("a.txt", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = second->was));
}

void test_a_file_git_does_not_have_is_refused(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    lines_created();
    g_cli_guard->persistent_buf_release(s_lines.text.bytes, g_cli_git_owner);
    s_lines.text.bytes = NULL;
    s_lines.text.used = 0ULL;
    s_lines.text.capacity = 0ULL;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_git_length.line_history, CliGitLengthCfg, .root = s_lines_sub, .file = "missing.txt", .text_text = "", .length = 0ull, .found_line_history_pointer = &s_lines, .err_msg = &err_msg));
}

void test_a_trees_repositories_are_listing_with_their_branches(void)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    repositories_created();
    for (unsigned int at = 0u; at < s_repos.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_repos.text, .at = s_repos.item[at].path));
        mmgr_line_ch(&line, ':');
        mmgr_line_put(&line, s_repos.item[at].has_branch ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_repos.text, .at = s_repos.item[at].branch) : "-");
    }
    CLI_TEST_ASSERT_TEXT(":main|libs/inner:trunk", out, line.at);
}

void test_each_repositorys_changes_are_listing_by_their_paths_in_the_tree(void)
{
    repositories_created();
    TEST_ASSERT_EQUAL_UINT(2u, s_repos_changed_before);
    TEST_ASSERT_TRUE(s_repos_a_changed);
    TEST_ASSERT_TRUE(s_repos_b_changed);
}

void test_a_commit_across_repositories_commits_to_each_and_leaves_nothing_changed(void)
{
    repositories_created();
    TEST_ASSERT_TRUE(s_repos_committed);
    TEST_ASSERT_EQUAL_STRING("both", s_repos_outer_subject);
    TEST_ASSERT_EQUAL_STRING("both", s_repos_inner_subject);
    TEST_ASSERT_EQUAL_UINT(0u, s_repos_changed_after);
}

void test_a_files_last_committed_text_is_read_from_its_own_repository(void)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    repositories_created();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = s_repos_root, .file = "libs/inner/b.txt", .out = &line));
    CLI_TEST_ASSERT_TEXT("two more\n", out, line.at);
}

void test_the_commits_that_touched_a_file_and_its_text_at_the_first_are_read(void)
{
    char out[1024];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    repositories_created();
    g_cli_guard->persistent_buf_release(s_commits.text.bytes, g_cli_git_owner);
    s_commits.text.bytes = NULL;
    s_commits.text.used = 0ULL;
    s_commits.text.capacity = 0ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_file_err_msg.commits, CliGitRootFileErrMsgCfg, .root = s_repos_root, .file = "libs/inner/b.txt", .found = &s_commits, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(2u, s_commits.count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_file.text_at, CliGitRootFileCfg, .root = s_repos_root, .file = "libs/inner/b.txt", .id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_commits.text, .at = s_commits.item[1].id), .out = &line, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("two\n", out, line.at);
}

void test_a_files_lines_commits_have_its_path_in_the_tree(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    repositories_created();
    g_cli_guard->persistent_buf_release(s_lines.text.bytes, g_cli_git_owner);
    s_lines.text.bytes = NULL;
    s_lines.text.used = 0ULL;
    s_lines.text.capacity = 0ULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_length.line_history, CliGitLengthCfg, .root = s_repos_root, .file = "libs/inner/b.txt", .text_text = "two more\n", .length = 9ull, .found_line_history_pointer = &s_lines, .err_msg = &err_msg));
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_lines.commits);
    TEST_ASSERT_TRUE(s_lines.commit[0].has_path);
    TEST_ASSERT_EQUAL_STRING("libs/inner/b.txt", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_lines.text, .at = s_lines.commit[0].path));
}

void test_a_new_inner_repository_accepts_its_own_files_first_commit(void)
{
    char out[1024];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    cli_unity_folder("git_fresh", s_root);
    repository_started(s_root, "main", "t", "a.txt", "one\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "inner", .joined = s_inner);
    repository_started(s_inner, "main", "t", NULL, NULL);
    cli_unity_file(s_inner, "inner.py", "x = 1\n");
    const char *const paths[1] = {"inner/inner.py"};
    const int committed = EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = s_root, .message = "inner", .paths = paths, .count = 1u, .out = &line, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    TEST_ASSERT_TRUE_MESSAGE(committed, s_error);
    TEST_ASSERT_TRUE(git_run_result(s_inner, "log", "-1", "--format=%s", NULL));
    TEST_ASSERT_EQUAL_STRING("inner", s_out);
}

void test_every_change_is_stashed_new_files_with_them(void)
{
    changes_stashed();
    TEST_ASSERT_TRUE(git_run_result(s_root, "status", "--porcelain", NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

void test_a_stash_is_listing_with_its_message(void)
{
    changes_stashed();
    stashes_read();
    TEST_ASSERT_EQUAL_UINT(1u, s_stashes.count);
    TEST_ASSERT_EQUAL_STRING("stash@{0}", EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_stashes.text, .at = s_stashes.item[0].name));
    const char *const subject = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_stashes.text, .at = s_stashes.item[0].subject);
    const unsigned long long length = CLI_UNITY_LENGTH(subject);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(9ull, length);
    TEST_ASSERT_EQUAL_STRING("half done", subject + length - 9u);
}

void test_a_stash_applied_restores_its_changes_and_new_files_and_stays(void)
{
    char path[CLI_PATH_MAX_LEN];
    changes_stashed();
    TEST_ASSERT_TRUE(stash_acted("apply", "stash@{0}", ""));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "a.txt", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_out, sizeof(s_out) - 1u);
    CLI_TEST_ASSERT_TEXT("changed\n", s_out, (size != QRY_MEMBER_UNREAD) ? size : 0ull);
    TEST_ASSERT_TRUE(file_there(s_root, "b.txt"));
    stashes_read();
    TEST_ASSERT_EQUAL_UINT(1u, s_stashes.count);
}

void test_a_stash_popped_is_dropped(void)
{
    changes_stashed();
    TEST_ASSERT_TRUE(stash_acted("pop", "stash@{0}", ""));
    stashes_read();
    TEST_ASSERT_EQUAL_UINT(0u, s_stashes.count);
}

void test_a_stash_dropped_is_removed(void)
{
    changes_stashed();
    TEST_ASSERT_TRUE(stash_acted("drop", "stash@{0}", ""));
    stashes_read();
    TEST_ASSERT_EQUAL_UINT(0u, s_stashes.count);
}

void test_what_is_not_a_stash_and_an_act_not_listing_are_refused(void)
{
    changes_stashed();
    TEST_ASSERT_FALSE(stash_acted("apply", "HEAD", ""));
    TEST_ASSERT_FALSE(stash_acted("clear", "stash@{0}", ""));
}

void test_a_commit_of_another_branch_is_listing(void)
{
    char picked[CLI_NAME_MAX_LEN];
    topic_committed(picked);
    g_cli_guard->persistent_buf_release(s_commits.text.bytes, g_cli_git_owner);
    s_commits.text.bytes = NULL;
    s_commits.text.used = 0ULL;
    s_commits.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_file_err_msg.elsewhere, CliGitRootFileErrMsgCfg, .root = s_root, .found = &s_commits);
    TEST_ASSERT_EQUAL_UINT(1u, s_commits.count);
    TEST_ASSERT_EQUAL_STRING(picked, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_commits.text, .at = s_commits.item[0].id));
}

void test_a_commit_is_cherry_picked_onto_this_branch_and_is_no_longer_elsewhere(void)
{
    char picked[CLI_NAME_MAX_LEN];
    char out[1024];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    topic_committed(picked);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_file.cherry_pick, CliGitRootFileCfg, .root = s_root, .id = picked, .out = &line, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(file_there(s_root, "c.txt"));
    TEST_ASSERT_TRUE(git_run_result(s_root, "log", "-1", "--format=%s", NULL));
    TEST_ASSERT_EQUAL_STRING("on topic", s_out);
    g_cli_guard->persistent_buf_release(s_commits.text.bytes, g_cli_git_owner);
    s_commits.text.bytes = NULL;
    s_commits.text.used = 0ULL;
    s_commits.text.capacity = 0ULL;
    EMBED_CALL(cli_git_root_file_err_msg.elsewhere, CliGitRootFileErrMsgCfg, .root = s_root, .found = &s_commits);
    TEST_ASSERT_EQUAL_UINT(0u, s_commits.count);
}

void test_a_word_that_is_no_commit_is_refused(void)
{
    char picked[CLI_NAME_MAX_LEN];
    char out[1024];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea line = {out, sizeof(out), 0ull};
    topic_committed(picked);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_git_root_file.cherry_pick, CliGitRootFileCfg, .root = s_root, .id = "--abort", .out = &line, .err_msg = &err_msg));
}

void test_each_lists_texts_are_in_a_cell_grown_as_they_are_added_and_released(void)
{
    graph_read("", 0);
    g_cli_guard->persistent_buf_release(s_touched.text.bytes, g_cli_git_owner);
    s_touched.text.bytes = NULL;
    s_touched.text.used = 0ULL;
    s_touched.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_graph.text.bytes, g_cli_git_owner);
    s_graph.text.bytes = NULL;
    s_graph.text.used = 0ULL;
    s_graph.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_lines.text.bytes, g_cli_git_owner);
    s_lines.text.bytes = NULL;
    s_lines.text.used = 0ULL;
    s_lines.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_repos.text.bytes, g_cli_git_owner);
    s_repos.text.bytes = NULL;
    s_repos.text.used = 0ULL;
    s_repos.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_changed.text.bytes, g_cli_git_owner);
    s_changed.text.bytes = NULL;
    s_changed.text.used = 0ULL;
    s_changed.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_commits.text.bytes, g_cli_git_owner);
    s_commits.text.bytes = NULL;
    s_commits.text.used = 0ULL;
    s_commits.text.capacity = 0ULL;
    g_cli_guard->persistent_buf_release(s_stashes.text.bytes, g_cli_git_owner);
    s_stashes.text.bytes = NULL;
    s_stashes.text.used = 0ULL;
    s_stashes.text.capacity = 0ULL;
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_git_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_git_owner));
}
