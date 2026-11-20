// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_watch.c: a batch reports which folders and files changed, the system reports a written file and a new one on
// Windows and on Linux, a poll of the tree finds what changed since the last, an entry the watching's patterns hide
// still marks its folder changed as it comes or goes, and a hidden file's write is left out whole
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/editor/watch_internal.h"
#include "ui/cli/src/text/patterns.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const unsigned int s_types[5] = {CLI_CHANGE_ADDED, CLI_CHANGE_MODIFIED, CLI_CHANGE_MODIFIED, CLI_CHANGE_REMOVED,
                                        CLI_CHANGE_ADDED};
static const char *const s_paths[5] = {"src/b.rs", "src/a.rs", "src", "old.txt", ".git/objects/ab/cdef"};

static char s_out[1u << 16u];
static char s_batches[1u << 16u];
static unsigned long long s_batches_at;
static char s_seen[1u << 14u];
static VerbaLinea s_seen_line = {s_seen, sizeof(s_seen), 0ull};
static char s_root[CLI_PATH_MAX_LEN];
static unsigned long long s_temporary_mark;
static unsigned long long s_lists_in_use;
static int s_watch_run_result;

// the tree `name`, its src/a.rs written, into s_root
static void tree_written(const char *name)
{
    char src[CLI_PATH_MAX_LEN];
    cli_unity_folder(name, s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src", .joined = src);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src));
    cli_unity_file(s_root, "src/a.rs", "one");
}

// a change reported, `path type` appended to the others, `|` between them
static void change_recorded(const CliEachPathTypeCfg *args)
{
    mmgr_line_put(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    mmgr_line_put(&s_seen_line, args->path);
    mmgr_line_ch(&s_seen_line, ' ');
    mmgr_line_decimal(&s_seen_line, args->type, 1u);
}

// the test fixture: a first poll of the tree, one file changed, one file added, then a second poll. The changes go
// into s_seen with a NUL terminator. A tree's first reading stays in memory until its process ends, so the fixture
// runs once per process
static void tree_polled(void)
{
    static int s_polled = 0;
    if (s_polled)
    {
        return;
    }
    s_polled = 1;
    tree_written("watch_poll");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_watch_internal.first_read_state, CliWatchCtx, .root = s_root));
    cli_unity_file(s_root, "src/a.rs", "a longer text");
    cli_unity_file(s_root, "src/b.rs", "three");
    s_seen_line.at = 0ull;
    EMBED_CALL(cli_watch_internal.read_again, CliWatchCtx, .root = s_root, .emit = change_recorded, .context = NULL);
    (void)mmgr_line_finish(&s_seen_line);
}

// the seen changes contain `path` with `type`
static void assert_seen(const char *path, unsigned int type)
{
    char required[CLI_PATH_MAX_LEN];
    VerbaLinea line = {required, sizeof(required), 0ull};
    mmgr_line_put(&line, path);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, type, 1u);
    (void)mmgr_line_finish(&line);
    CLI_TEST_ASSERT_CONTAINS(required, s_seen);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// how many batches a watch started passed to its take
static unsigned int s_live_batches;

static void live_batch_recorded(const CliWatchBatchCfg *args)
{
    (void)args;
    s_live_batches += 1u;
}

void test_a_watch_started_passes_a_change_and_passes_none_once_stopped(void)
{
    tree_written("watch_live");
    s_live_batches = 0u;
    cli_watch_start(&(CliWatchCfg){.root = s_root, .take = live_batch_recorded, .context = NULL});
    cli_unity_file(s_root, "src/c.rs", "four");
    // a system that reports nothing reads the tree every two seconds, and a cold runner spawns git slowly: the wait
    // spans several readings
    const unsigned long long began = cli_clock_milliseconds();
    const unsigned long long until = began + 15000ull;
    while ((s_live_batches == 0u) && (cli_clock_milliseconds() < until))
    {
        CliWake pause = CLI_WAKE_INIT;
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 20ull);
    }
    const unsigned long long waited = cli_clock_milliseconds() - began;
    cli_watch_stop();
    if (s_live_batches == 0u)
    {
        // a run with no batch reports the wait and what a reading of the tree finds now, which names the part that gave
        // none
        s_seen_line.at = 0ull;
        EMBED_CALL(cli_watch_internal.read_again, CliWatchCtx, .root = s_root, .emit = change_recorded,
                   .context = NULL);
        (void)mmgr_line_finish(&s_seen_line);
        (void)fprintf(stderr, "  diag watch_live: no batch in %llu ms; a reading now reports '%s'\n", waited, s_seen);
    }
    TEST_ASSERT_NOT_EQUAL(0u, s_live_batches);
    const unsigned int before = s_live_batches;
    cli_unity_file(s_root, "src/d.rs", "five");
    CliWake rest = CLI_WAKE_INIT;
    (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &rest, .milliseconds_u64 = 500ull);
    TEST_ASSERT_EQUAL_UINT(before, s_live_batches);
}

static unsigned int s_folder_calls;

static void folder_changed(void)
{
    s_folder_calls += 1u;
}

// the calls of the folder watch counted while `milliseconds` pass or until `calls` came
static void folder_calls_waited(unsigned int calls, unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while ((s_folder_calls < calls) && (cli_clock_milliseconds() < until))
    {
        CliWake pause = CLI_WAKE_INIT;
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 20ull);
    }
}

void test_a_folder_kept_open_calls_on_a_change_under_it_and_calls_none_once_stopped(void)
{
    tree_written("watch_folder");
    cli_unity_file(s_root, "src/plugin.json", "{}");
    s_folder_calls = 0u;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_folder_watch_internal.start, CliFolderWatchCtx, .folder = s_root, .changed = folder_changed));
    cli_unity_file(s_root, "src/plugin.json", "{\"id\": \"one\"}");
    // a system with no change notice reads the folder every two seconds: the wait spans several readings
    folder_calls_waited(1u, 10000ull);
    TEST_ASSERT_NOT_EQUAL(0u, s_folder_calls);
    cli_folder_watch_stop();
    const unsigned int before = s_folder_calls;
    cli_unity_file(s_root, "src/other.json", "{}");
    folder_calls_waited(before + 1u, 500ull);
    TEST_ASSERT_EQUAL_UINT(before, s_folder_calls);
}

void test_a_burst_of_writes_under_a_folder_kept_open_calls_once(void)
{
    tree_written("watch_burst");
    s_folder_calls = 0u;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_folder_watch_internal.start, CliFolderWatchCtx, .folder = s_root, .changed = folder_changed));
    cli_unity_file(s_root, "src/plugin.json", "{}");
    cli_unity_file(s_root, "src/sample.txt", "a");
    cli_unity_file(s_root, "src/other.json", "{}");
    folder_calls_waited(1u, 10000ull);
    folder_calls_waited(2u, 1000ull);
    cli_folder_watch_stop();
    TEST_ASSERT_EQUAL_UINT(1u, s_folder_calls);
}

static char s_folders[1024];
static unsigned int s_folders_count;
static unsigned int s_files_count;

// a batch's folders recorded, `|` between them, and its folders and files counted
static void batch_recorded(const CliWatchBatchCfg *args)
{
    VerbaLinea line = {s_folders, sizeof(s_folders), 0ull};
    for (unsigned int at = 0u; at < args->folders->count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->folders, .index = at));
    }
    (void)mmgr_line_finish(&line);
    s_folders_count = args->folders->count;
    s_files_count = args->files->count;
}

// gather the `count` changes in `paths`, with their types in `types`, into one batch, the ignore patterns set to hide
// `hidden`. Return 1 where a change remains in the batch
static int hidden_gathered(const char *hidden, const char *const *paths, const unsigned int *types, unsigned int count)
{
    const char *const lines[1] = {hidden};
    tree_written("watch_hidden");
    (void)EMBED_CALL(cli_patterns_lines_count.set, CliPatternsLinesCountCfg, .lines = lines, .count = 1u,
                     .part = CLI_PART_WATCHING);
    s_folders[0] = '\0';
    s_folders_count = 0u;
    s_files_count = 0u;
    const int any = EMBED_CALL(cli_watch_paths_internal.gathered_get, CliWatchPathsCtx, .root = s_root, .paths = paths,
                               .types = types, .count = count, .take = batch_recorded, .context = NULL);
    (void)EMBED_CALL(cli_patterns_lines_count.set, CliPatternsLinesCountCfg, .lines = lines, .count = 0u,
                     .part = CLI_PART_WATCHING);
    return any;
}

void test_a_hidden_folder_removed_marks_the_folder_it_was_in_changed(void)
{
    static const char *const s_gone[1] = {"deps"};
    static const unsigned int s_gone_types[1] = {CLI_CHANGE_REMOVED};
    TEST_ASSERT_TRUE(hidden_gathered("deps", s_gone, s_gone_types, 1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_folders_count);
    TEST_ASSERT_EQUAL_STRING("", s_folders);
    TEST_ASSERT_EQUAL_UINT(0u, s_files_count);
}

void test_a_hidden_file_written_is_left_out_whole(void)
{
    static const char *const s_written[1] = {"deps/x.txt"};
    static const unsigned int s_written_types[1] = {CLI_CHANGE_MODIFIED};
    TEST_ASSERT_FALSE(hidden_gathered("deps/x.txt", s_written, s_written_types, 1u));
}

void test_a_poll_finds_a_written_file(void)
{
    tree_polled();
    assert_seen("src/a.rs", CLI_CHANGE_MODIFIED);
}

void test_a_poll_finds_a_new_file(void)
{
    tree_polled();
    assert_seen("src/b.rs", CLI_CHANGE_ADDED);
}
