// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files.c: a paste copies or moves and writes over nothing; a file and a folder go to a tree on another machine a part at a time; a file and a folder
// brought in are copied whole, each file reporting its end; a tree in a folder its repository ignores lists its files from the disk; a file in a folder made
// after git's view was read is tracked, and one in an ignored folder is not
#include "ui/cli/src/code/git.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/editor/files_search_hits.h"
#include "ui/cli/src/text/base64.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What a bringing reported: each path, the bytes done and its size
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
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = bytes, .size = size), name);
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
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = repo, .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = repo, .relative = "runs/tree", .joined = tree);
}

// a repository with an ignored folder, and folders made after git's view was read, at s_root
static void tracked_tree_written(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_tracked", s_root);
    folder_created(s_root, "made", path);
    bytes_written(s_root, ".gitignore", "out/\n", 5ull);
    bytes_written(s_root, "a.txt", "one", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
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
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = s_root, .file = "taken", .text = "x", .length = 1ull, .err_msg = &err_msg));
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
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root, .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    unsigned int hits = 0u;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_POOL_BYTES, .other = "\"line\"", .other_cap = CLI_POOL_BYTES}); at != NULL; at = cellul.find(&(CatenaFinitaCfg){.src = at + 1, .cap = CLI_POOL_BYTES, .other = "\"line\"", .other_cap = CLI_POOL_BYTES}))
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

// the tree `s_root` searched for the needle, the lines returned counted, each file's together and the files in their
// order, and the number of the last file among them into `last`
static unsigned int needle_lines_found(unsigned int *last)
{
    const CliPatterns none = {0};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root, .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    unsigned int lines = 0u;
    unsigned int number = 0u;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){.src = s_found, .cap = CLI_POOL_BYTES, .other = "\"path\":\"f", .other_cap = CLI_POOL_BYTES}); at != NULL; at = cellul.find(&(CatenaFinitaCfg){.src = at + 1, .cap = CLI_POOL_BYTES, .other = "\"path\":\"f", .other_cap = CLI_POOL_BYTES}))
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
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT64(2ull, files->count);
    TEST_ASSERT_EQUAL_STRING("a.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 0u));
    TEST_ASSERT_EQUAL_STRING("src/b.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 1u));
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

void test_the_types_view_records_each_files_name_and_ending_once(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const CliFiles *const types = EMBED_CALL(cli_files_root_paths.types_view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(types);
    TEST_ASSERT_EQUAL_UINT64(3ull, types->count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = ".js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = "a.js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = "b.js"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = types, .path = ".lean"));
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = types);
}

void test_a_file_git_would_track_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "a.txt"));
}

void test_a_file_in_a_folder_created_since_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/new.txt"));
}

void test_a_file_a_folder_further_down_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/deeper/more.txt"));
}

void test_a_file_under_a_folder_git_ignores_is_not_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "out/deep/x.o"));
}

void test_the_files_report_what_the_views_and_their_lists_use_of_the_series_cellularum(void)
{
    char tree[CLI_PATH_MAX_LEN];
    char report[256];
    VerbaLinea report_line = {report, sizeof(report), 0ull};
    ignored_tree_written(tree);
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = tree));
    EMBED_CALL(cli_files_root_out_dir.cells_usage_write, CliFilesRootOutDirCfg, .out = &report_line);
    (void)mmgr_line_finish(&report_line);
    const char *const mark = cellul.chr(&(CatenaFinitaCfg){.src = report, .cap = CLI_POOL_BYTES, .byte = ' '});
    TEST_ASSERT_NOT_NULL_MESSAGE(mark, report);
    TEST_ASSERT_NOT_EQUAL_MESSAGE('0', mark[1], report);
}
