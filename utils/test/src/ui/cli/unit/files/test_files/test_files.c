// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files.c: a paste copies or moves and writes over nothing; a file and a folder go to a tree on another machine a
// part at a time; a file and a folder brought in are copied whole, each file reporting its end; a tree in a folder its
// repository ignores lists its files from the disk; a file in a folder made after git's view was read is tracked, and
// one in an ignored folder is not
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/git.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/command_line/commands.h"
#include "ui/cli/src/editor/files_internal.h"
#include "ui/cli/src/editor/files_search_hits.h"
#include "ui/cli/src/text/base64.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What the progress of a copy reports: each path, the bytes done and its size
typedef struct
{
    char path[64][CLI_PATH_MAX_LEN];
    unsigned long long done[64];
    unsigned long long size[64];
    unsigned int count;
} FilesProgress;

static const char *const s_init[2] = {"init", "-q"};

static FilesProgress s_progress;
static unsigned char s_big[3u << 20u];
static unsigned char s_read[(3u << 20u) + 1u];
static unsigned char s_part[2u << 20u];
static char s_out[1u << 16u];
static char s_found[1u << 20u];
static char s_needles[64u * 12u];
static char s_there[CLI_PATH_MAX_LEN];
static char s_written[4096];
static char s_root[CLI_PATH_MAX_LEN];
static char s_error[1024];

// a file past the size the window pages, in lines of a hundred bytes

static int s_over_run_result;
static int s_over_fetched;
static char s_over_tos[256];
static unsigned long long s_over_size;
static int s_over_intact;
static int s_over_note;
static int s_over_first;
static int s_over_part_file;
static unsigned int s_over_reports;
static int s_over_end_reported;
static int s_over_late_part;

static int s_fetch_run_result;
static int s_fetch_fetched;
static char s_fetch_tos[256];
static unsigned long long s_fetch_size;
static int s_fetch_note;
static int s_fetch_first;
static int s_fetch_big_end;
static int s_fetch_note_end;

// a file of the tree `root` written at `name`, `size` bytes of `bytes`
static void bytes_written(const char *root, const char *name, const void *bytes, unsigned long long size)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                        .bytes = bytes, .size = size),
                             name);
}

// the folder `name` of the tree `root` made with every folder above it, its path in `path`
static void folder_created(const char *root, const char *name, char *path)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(root) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
}

// a repository whose ignored folder holds a tree, the tree's path in `tree`
static void ignored_tree_written(char *tree)
{
    char repo[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_listing", repo);
    folder_created(repo, "runs/tree/src", path);
    bytes_written(repo, ".gitignore", "runs/\n", 6ull);
    bytes_written(repo, "runs/tree/a.js", "one", 3ull);
    bytes_written(repo, "runs/tree/src/b.js", "two", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = repo,
                                .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = repo, .relative = "runs/tree", .joined = tree);
}

// a repository with an ignored folder, and folders made after git reads its view, at s_root
static void tracked_tree_written(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_tracked", s_root);
    folder_created(s_root, "made", path);
    bytes_written(s_root, ".gitignore", "out/\n", 5ull);
    bytes_written(s_root, "a.txt", "one", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root,
                                .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    cli_tree_forgotten();
    (void)EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "a.txt");
    bytes_written(s_root, "made/new.txt", "two", 3ull);
    folder_created(s_root, "made/deeper", path);
    bytes_written(s_root, "made/deeper/more.txt", "three", 5ull);
    folder_created(s_root, "out/deep", path);
    bytes_written(s_root, "out/deep/x.o", "four", 4ull);
}

// whether `text` contains `part`
static int text_contains(const char *text, const char *part)
{
    const unsigned long long length = CLI_UNITY_LENGTH(part);
    for (const char *at = text; *at != '\0'; at += 1)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = part, .cap = length}))
        {
            return 1;
        }
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    cli_tree_forgotten();
}

void test_a_file_written_where_a_folder_exists_is_refused_with_the_systems_reason(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("files_refused", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "taken", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = s_root,
                                 .file = "taken", .text = "x", .length = 1ull, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    CLI_TEST_ASSERT_CONTAINS("(os error ", why);
}

void test_a_search_finds_every_line_in_cells_grown_end_their_first_capacity_each_released(void)
{
    char many[100u * 12u];
    const CliPatterns none = {0};
    VerbaLinea lines = {many, sizeof(many), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_search", s_root);
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        mmgr_line_put(&lines, "a needle\n");
    }
    bytes_written(s_root, "a.txt", many, lines.at);
    bytes_written(s_root, "b.txt", "no\nthe needle\n", 14ull);
    cli_tree_forgotten();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root,
                                .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out,
                                .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    unsigned int hits = 0u;
    for (const char *at = cellul.find(
             &(CatenaFinitaCfg){.src = s_out, .cap = CLI_POOL_BYTES, .other = "\"line\"", .other_cap = CLI_POOL_BYTES});
         at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                         .src = at + 1, .cap = CLI_POOL_BYTES, .other = "\"line\"", .other_cap = CLI_POOL_BYTES}))
    {
        hits += 1u;
    }
    TEST_ASSERT_EQUAL_UINT(101u, hits);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_search_owner));
    TEST_ASSERT_GREATER_THAN_UINT64(64ull * sizeof(CliSearchHit), mmgr_owner_hw(g_cli_search_owner));
}

// the tree `s_root` written with `files` files named f00.txt, f01.txt and on, each of `lines` lines the needle is in
static void needle_files_written(unsigned int files, unsigned int lines)
{
    VerbaLinea text = {s_needles, sizeof(s_needles), 0ull};
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        mmgr_line_put(&text, "a needle\n");
    }
    cli_unity_folder("files_search_workers", s_root);
    for (unsigned int file = 0u; file < files; file += 1u)
    {
        char name[8] = {'f', (char)('0' + (file / 10u)), (char)('0' + (file % 10u)), '.', 't', 'x', 't', '\0'};
        bytes_written(s_root, name, s_needles, text.at);
    }
    cli_tree_forgotten();
}

// The tree `s_root` searched for the needle, and the lines it returns counted. Each file's lines come together and
// the files come in their order. The number of the last file among them goes into `last`
static unsigned int needle_lines_found(unsigned int *last)
{
    const CliPatterns none = {0};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root,
                                .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out,
                                .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    unsigned int lines = 0u;
    unsigned int number = 0u;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){
             .src = s_found, .cap = CLI_POOL_BYTES, .other = "\"path\":\"f", .other_cap = CLI_POOL_BYTES});
         at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                         .src = at + 1, .cap = CLI_POOL_BYTES, .other = "\"path\":\"f", .other_cap = CLI_POOL_BYTES}))
    {
        const unsigned int read = ((unsigned int)(at[9] - '0') * 10u) + (unsigned int)(at[10] - '0');
        TEST_ASSERT_GREATER_OR_EQUAL_UINT(number, read);
        number = read;
        lines += 1u;
    }
    *last = number;
    return lines;
}

void test_workers_searching_many_files_lose_no_line_and_return_them_in_the_order_of_the_files(void)
{
    needle_files_written(64u, 16u);
    for (unsigned int round = 0u; round < 32u; round += 1u)
    {
        unsigned int last = 0u;
        TEST_ASSERT_EQUAL_UINT(64u * 16u, needle_lines_found(&last));
        TEST_ASSERT_EQUAL_UINT(63u, last);
        TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_search_owner));
    }
}

void test_a_search_finding_more_lines_than_it_returns_returns_the_first_in_the_order_of_the_files(void)
{
    needle_files_written(64u, 64u);
    for (unsigned int round = 0u; round < 8u; round += 1u)
    {
        unsigned int last = 0u;
        TEST_ASSERT_EQUAL_UINT(CLI_HITS_MAX, needle_lines_found(&last));
        TEST_ASSERT_EQUAL_UINT((CLI_HITS_MAX - 1u) / 64u, last);
        TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_search_owner));
    }
}

void test_a_tree_its_repository_ignores_lists_its_files_from_the_disk(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const CliFiles *const files = EMBED_CALL(cli_files_view_internal.view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT64(2ull, files->count);
    TEST_ASSERT_EQUAL_STRING("a.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 0u));
    TEST_ASSERT_EQUAL_STRING("src/b.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 1u));
    EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
}

// What a view passes the suite's callback, copied out. It holds the calls, the files' count, the first path, and
// the files module's cells in use during the call
typedef struct
{
    unsigned int calls;
    unsigned int count;
    char first[CLI_PATH_MAX_LEN];
    unsigned long long in_use;
} FilesSeen;

// the view the files module passes, copied into the suite's record
static void files_seen(const CliFilesEachCfg *args)
{
    FilesSeen *const seen = args->context;
    seen->calls += 1u;
    seen->count = args->files->count;
    seen->in_use = mmgr_owner_in_use(g_cli_files_views_owner);
    (void)EMBED_CALL(cli_files_view_read.at, CliFilesViewCfg, .view = args->files, .index = 0u,
                     .path_out = seen->first);
}

void test_the_files_go_to_the_callback_once_and_their_cell_goes_back_before_the_return(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const unsigned long long before = mmgr_owner_in_use(g_cli_files_views_owner);
    FilesSeen seen = {0u, 0u, {0}, 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_paths.each, CliFilesRootPathsCfg, .root = tree, .each = files_seen,
                                .context = &seen));
    TEST_ASSERT_EQUAL_UINT(1u, seen.calls);
    TEST_ASSERT_EQUAL_UINT(2u, seen.count);
    TEST_ASSERT_EQUAL_STRING("a.js", seen.first);
    TEST_ASSERT_TRUE(seen.in_use > before);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_files_views_owner));
}

void test_the_types_view_records_each_files_name_and_ending_once(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const CliFiles *const types = EMBED_CALL(cli_files_view_internal.types_view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(types);
    TEST_ASSERT_EQUAL_UINT64(3ull, types->count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = ".js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = "a.js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = "b.js"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = ".lean"));
    EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = types);
}

void test_a_file_git_would_track_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "a.txt"));
}

void test_a_file_in_a_folder_created_since_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/new.txt"));
}

void test_a_file_a_folder_further_down_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/deeper/more.txt"));
}

void test_a_file_under_a_folder_git_ignores_is_not_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_FALSE(
        EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "out/deep/x.o"));
}

void test_the_files_report_what_the_views_and_their_lists_use_of_the_series_cellularum(void)
{
    char tree[CLI_PATH_MAX_LEN];
    char report[256];
    VerbaLinea report_line = {report, sizeof(report), 0ull};
    ignored_tree_written(tree);
    EMBED_CALL(cli_files_let_internal.let, CliFilesCfg,
               .files = EMBED_CALL(cli_files_view_internal.view, CliFilesRootPathsCfg, .root = tree));
    EMBED_CALL(cli_files_root_out_dir.cells_usage_write, CliFilesRootOutDirCfg, .out = &report_line);
    (void)mmgr_line_finish(&report_line);
    const char *const mark = cellul.chr(&(CatenaFinitaCfg){.src = report, .cap = CLI_POOL_BYTES, .byte = ' '});
    TEST_ASSERT_NOT_NULL_MESSAGE(mark, report);
    TEST_ASSERT_NOT_EQUAL_MESSAGE('0', mark[1], report);
}

void test_a_folder_and_a_file_are_created_with_the_folders_above_them(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    cli_unity_folder("files_created", s_root);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_dir.create_folder, CliFilesRootOutDirCfg, .root = s_root,
                                .dir = "made/deep/er", .err_msg = &err_msg));
    // the call refuses a folder there already, with why
    err_msg.at = 0ull;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_out_dir.create_folder, CliFilesRootOutDirCfg, .root = s_root,
                                 .dir = "made/deep/er", .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_err_msg_text.create_file, CliFilesRootErrMsgTextCfg, .root = s_root,
                                .file = "made/deep/er/x.txt", .err_msg = &err_msg));
    // the call refuses a file there already and keeps its text
    bytes_written(s_root, "made/deep/er/x.txt", "one", 3ull);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_err_msg_text.create_file, CliFilesRootErrMsgTextCfg, .root = s_root,
                                 .file = "made/deep/er/x.txt", .err_msg = &err_msg));
    cli_tree_forgotten();
    const CliFiles *const files = EMBED_CALL(cli_files_view_internal.view, CliFilesRootPathsCfg, .root = s_root);
    TEST_ASSERT_NOT_NULL(files);
    int listed = 0;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        listed = listed ||
                 cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at),
                                              .other = "made/deep/er/x.txt",
                                              .cap = CLI_PATH_MAX_LEN});
    }
    EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
    TEST_ASSERT_TRUE(listed);
}

void test_the_repository_names_the_paths_it_ignores(void)
{
    static const char *const s_paths[2] = {"a.log", "a.c"};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_ignored_read", s_root);
    bytes_written(s_root, ".gitignore", "*.log\n", 6ull);
    bytes_written(s_root, "a.log", "one", 3ull);
    bytes_written(s_root, "a.c", "two", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root,
                                .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    cli_tree_forgotten();
    unsigned char ignored[2] = {0u, 0u};
    EMBED_CALL(cli_files_root_paths.ignored_read, CliFilesRootPathsCfg, .root = s_root, .paths = s_paths, .count = 2u,
               .ignored = ignored);
    TEST_ASSERT_EQUAL_UINT(1u, ignored[0]);
    TEST_ASSERT_EQUAL_UINT(0u, ignored[1]);
}

void test_a_path_through_the_repositorys_folder_or_a_target_folder_is_skipped(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .git = 0, .path = ".git/HEAD"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .git = 0, .path = "crate/target/debug/a"));
    // where git decides the rest, the target folder is git's to ignore
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .git = 1, .path = "crate/target/debug/a"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .git = 0, .path = "src/targets.c"));
}

void test_a_search_asked_to_locate_leaves_its_lines_located_by_file(void)
{
    const CliPatterns none = {0};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_located", s_root);
    bytes_written(s_root, "a.txt", "no\nthe needle\n", 14ull);
    bytes_written(s_root, "b.txt", "a needle\nneedle two\n", 20ull);
    cli_tree_forgotten();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root,
                                .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out,
                                .err_msg = &err_msg, .located = 1));
    TEST_ASSERT_EQUAL_UINT(3u, cli_search_located_count());
    TEST_ASSERT_EQUAL_UINT(2u, cli_search_located_files());
    CliStandingLine line;
    TEST_ASSERT_TRUE(cli_search_located_line(&(CliSearchLocatedCfg){.at = 0u, .line_out = &line}));
    TEST_ASSERT_EQUAL_UINT64(2ull, line.line);
    TEST_ASSERT_EQUAL_UINT64(5ull, line.col);
    TEST_ASSERT_EQUAL_UINT(0u, line.file);
    char text[64];
    const unsigned long long copied =
        cli_search_located_text(&(CliSearchLocatedCfg){.line = &line, .out = text, .capacity = sizeof(text)});
    CLI_TEST_ASSERT_TEXT("the needle", text, copied);
    TEST_ASSERT_EQUAL_UINT(0u, cli_search_located_file_first(&(CliSearchLocatedCfg){.file = 0u}));
    TEST_ASSERT_EQUAL_UINT(1u, cli_search_located_file_first(&(CliSearchLocatedCfg){.file = 1u}));
    TEST_ASSERT_EQUAL_UINT(3u, cli_search_located_file_first(&(CliSearchLocatedCfg){.file = 2u}));
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_EQUAL_STRING("b.txt", cli_search_located_path(&(CliSearchLocatedCfg){.file = 1u, .path_out = path}));
    TEST_ASSERT_FALSE(cli_search_located_line(&(CliSearchLocatedCfg){.at = 3u, .line_out = &line}));
    cli_search_located_free();
    TEST_ASSERT_EQUAL_UINT(0u, cli_search_located_count());
}

static char s_listed[1024];
static VerbaLinea s_listed_line = {s_listed, sizeof(s_listed), 0ull};

// a listed path written after the others, `|` between them
static void path_recorded(const CliEachPathLengthCfg *args)
{
    mmgr_line_put(&s_listed_line, (s_listed_line.at > 0ull) ? "|" : "");
    mmgr_line_put_n(&s_listed_line, args->path, args->length);
}

void test_a_folders_entries_are_listed_in_a_cell_and_a_folder_listed_by_the_same_means(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    cli_unity_folder("files_entries", s_root);
    folder_created(s_root, "sub", path);
    bytes_written(s_root, "a.txt", "x", 1ull);
    bytes_written(s_root, "sub/b.txt", "y", 1ull);
    cli_tree_forgotten();
    CliFileEntries *const top = EMBED_CALL(cli_files_listing.entries_alloc, CliFilesEntriesCfg, .root = s_root,
                                           .dir = "", .owner = g_cli_text_owner, .err_msg = &err_msg);
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_UINT(2u, top->count);
    g_cli_guard->persistent_buf_release(top, g_cli_text_owner);
    s_listed_line.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_each.folder_list, CliFilesRootEachCfg, .root = s_root, .folder = "sub",
                                .git = 0, .each = path_recorded, .context = NULL));
    (void)mmgr_line_finish(&s_listed_line);
    TEST_ASSERT_EQUAL_STRING("sub/b.txt", s_listed);
}

void test_a_free_name_is_numbered_a_file_is_written_in_parts_and_a_save_goes_beside_the_file(void)
{
    char path[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    cli_unity_folder("files_free", s_root);
    bytes_written(s_root, "a.txt", "x", 1ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_err_msg.free_in, CliFilesRootErrMsgCfg, .root = s_root, .into = "",
                                .name = "a.txt", .path_out = path, .err_msg = &err_msg));
    TEST_ASSERT_TRUE_MESSAGE(cellul.starts(&(CatenaFinitaCfg){.src = path, .other = "a", .cap = CLI_PATH_MAX_LEN}),
                             path);
    TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = path, .other = "a.txt", .cap = CLI_PATH_MAX_LEN}));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_err_msg.free_in, CliFilesRootErrMsgCfg, .root = s_root, .into = "",
                                .name = "new.txt", .path_out = path, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("new.txt", path);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_err_msg.free_in, CliFilesRootErrMsgCfg, .root = s_root, .into = "none",
                                 .name = "a.txt", .path_out = path, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.write_part, CliFilesRootPathCfg, .root = s_root, .path = "p.txt",
                                .at = 0ull, .data = "hel", .size = 3ull, .last = 0, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.write_part, CliFilesRootPathCfg, .root = s_root, .path = "p.txt",
                                .at = 3ull, .data = "lo", .size = 2ull, .last = 1, .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "p.txt", .joined = full);
    const unsigned long long size = qry_member_file_read(full, s_read, sizeof(s_read) - 1u);
    TEST_ASSERT_EQUAL_UINT64(5ull, size);
    TEST_ASSERT_EQUAL_MEMORY("hello", s_read, 5u);
    char adjacent[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_err_msg.saving_adjacent, CliFilesPathErrMsgCfg,
                                .path = "D:/trees/t/x.txt", .file = "x.txt", .adjacent = adjacent,
                                .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("D:/trees/t/.x.txt.saving", adjacent);
}

void test_a_fuzzy_match_scores_the_querys_characters_found_in_their_order(void)
{
    unsigned int required[8];
    unsigned int displayed[16];
    unsigned int lower[16];
    unsigned int latest[8];
    unsigned int hits[8];
    double score = 0.0;
    unsigned int hit_count = 0u;
    const unsigned int requested = EMBED_CALL(cli_files_query_characters.want, CliFilesQueryLettersCfg, .query = "ab",
                                              .characters = required, .capacity = 8u);
    TEST_ASSERT_EQUAL_UINT(2u, requested);
    const unsigned int count = EMBED_CALL(cli_files_text.characters, CliFilesTextCfg, .text = "xAyb", .capacity = 16u,
                                          .displayed = displayed, .lower = lower);
    TEST_ASSERT_EQUAL_UINT(4u, count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_count.fuzzy_matches, CliFilesCountCfg, .count = count, .score = &score,
                                .hits = hits, .required = required, .requested_count = requested,
                                .displayed_read = displayed, .lower_read = lower, .latest = latest,
                                .hit_count = &hit_count));
    TEST_ASSERT_EQUAL_UINT(2u, hit_count);
    TEST_ASSERT_EQUAL_UINT(1u, hits[0]);
    TEST_ASSERT_EQUAL_UINT(3u, hits[1]);
    TEST_ASSERT_TRUE(score > 0.0);
    const unsigned int reversed = EMBED_CALL(cli_files_query_characters.want, CliFilesQueryLettersCfg, .query = "ba",
                                             .characters = required, .capacity = 8u);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_count.fuzzy_matches, CliFilesCountCfg, .count = count, .score = &score,
                                 .hits = hits, .required = required, .requested_count = reversed,
                                 .displayed_read = displayed, .lower_read = lower, .latest = latest,
                                 .hit_count = &hit_count));
}

void test_the_explorer_lists_the_tree_builds_a_rows_menu_and_reports_a_stale_mark_once(void)
{
    static char s_explorer_root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("files_explorer", s_explorer_root);
    folder_created(s_explorer_root, "sub", path);
    bytes_written(s_explorer_root, "a.txt", "x", 1ull);
    bytes_written(s_explorer_root, "sub/b.txt", "y", 1ull);
    cli_tree_forgotten();
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg));
    cli_files_explorer_show(&(CliFilesExplorerCfg){.root = s_explorer_root});
    TEST_ASSERT_EQUAL_UINT(2u, cli_files_explorer_rows());
    TEST_ASSERT_TRUE(cli_files_explorer_menu(&(CliFilesExplorerCfg){.at = 0u}));
    TEST_ASSERT_TRUE(cli_files_explorer_menu(&(CliFilesExplorerCfg){.at = 9u}));
    (void)cli_files_explorer_changed();
    TEST_ASSERT_FALSE(cli_files_explorer_changed());
    cli_files_explorer_stale();
    cli_files_explorer_stale();
    TEST_ASSERT_TRUE(cli_files_explorer_changed());
    TEST_ASSERT_FALSE(cli_files_explorer_changed());
    cli_files_explorer_end();
    TEST_ASSERT_EQUAL_UINT(0u, cli_files_explorer_rows());
    TEST_ASSERT_FALSE(cli_files_explorer_menu(&(CliFilesExplorerCfg){.at = 0u}));
}

void test_a_tree_unread_is_read_behind_the_window_until_it_is_listed(void)
{
    cli_unity_folder("files_behind", s_root);
    bytes_written(s_root, "a.txt", "x", 1ull);
    cli_tree_forgotten();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = s_root));
    EMBED_CALL(cli_files_root_paths.read_behind, CliFilesRootPathsCfg, .root = s_root);
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while (EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = s_root) &&
           (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 5u);
    }
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = s_root));
    // the warm of a tree listed already makes no second listing
    EMBED_CALL(cli_files_root_paths.warm, CliFilesRootPathsCfg, .root = s_root);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 50u);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = s_root));
}

void test_a_workers_lines_are_stored_and_read_back_in_the_order_of_the_files(void)
{
    CliSearchHits stores[2];
    memor.set(&(MemoriaCfg){.dst = stores, .val = 0, .bytes = sizeof(stores)});
    EMBED_CALL(cli_files_search_hits.hits_put, CliFilesSearchHitsCfg, .hits = &stores[1], .file = 1u, .line = 2ull,
               .col = 3ull, .text = (const unsigned char *)"second", .size = 6ull);
    EMBED_CALL(cli_files_search_hits.hits_put, CliFilesSearchHitsCfg, .hits = &stores[0], .file = 0u, .line = 5ull,
               .col = 1ull, .text = (const unsigned char *)"first", .size = 5ull);
    EMBED_CALL(cli_files_search_hits.hits_put, CliFilesSearchHitsCfg, .hits = &stores[0], .file = 0u, .line = 9ull,
               .col = 1ull, .text = (const unsigned char *)"third", .size = 5ull);
    const CliSearchHit *one =
        EMBED_CALL(cli_files_search_hits.hits_next, CliFilesSearchHitsCfg, .hits = stores, .stores = 2u);
    TEST_ASSERT_NOT_NULL(one);
    TEST_ASSERT_EQUAL_UINT(0u, one->file);
    TEST_ASSERT_EQUAL_UINT64(5ull, one->line);
    TEST_ASSERT_EQUAL_MEMORY("first", one->text, 5u);
    one = EMBED_CALL(cli_files_search_hits.hits_next, CliFilesSearchHitsCfg, .hits = stores, .stores = 2u);
    TEST_ASSERT_NOT_NULL(one);
    TEST_ASSERT_EQUAL_UINT64(9ull, one->line);
    one = EMBED_CALL(cli_files_search_hits.hits_next, CliFilesSearchHitsCfg, .hits = stores, .stores = 2u);
    TEST_ASSERT_NOT_NULL(one);
    TEST_ASSERT_EQUAL_UINT(1u, one->file);
    TEST_ASSERT_EQUAL_UINT64(3ull, one->col);
    TEST_ASSERT_NULL(EMBED_CALL(cli_files_search_hits.hits_next, CliFilesSearchHitsCfg, .hits = stores, .stores = 2u));
}
