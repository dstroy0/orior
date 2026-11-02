// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_watch.c: a batch reports which folders and files changed, the system reports a written file and a new one on Windows and on
// Linux, and a poll of the tree finds what changed since the last
#include "ui/cli/src/editor/watch_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const unsigned int s_kinds[5] = {CLI_CHANGE_ADDED, CLI_CHANGE_MODIFIED, CLI_CHANGE_MODIFIED, CLI_CHANGE_REMOVED, CLI_CHANGE_ADDED};
static const char *const s_paths[5] = {"src/b.rs", "src/a.rs", "src", "old.txt", ".git/objects/ab/cdef"};

static char s_out[1u << 16u];
static CliLock s_lock = CLI_LOCK_INIT;
static char s_batches[1u << 16u];
static unsigned long long s_batches_at;
static char s_seen[1u << 14u];
static VerbaLinea s_seen_line = {s_seen, sizeof(s_seen), 0ull};
static char s_root[CLI_PATH_MAX_LEN];
static unsigned long long s_temporary_mark;
static unsigned long long s_lists_in_use;
static int s_watch_ran;

// the tree `name`, its src/a.rs written, into s_root
static void tree_written(const char *name)
{
    char src[CLI_PATH_MAX_LEN];
    cli_unity_folder(name, s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src", .joined = src);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src));
    cli_unity_file(s_root, "src/a.rs", "one");
}

// the batch of `count` changes written into s_out, NUL-terminated
static void batch_written(const char *const *paths, const unsigned int *kinds, unsigned int count)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_watch_paths_internal.gathered_get, CliWatchPathsCtx, .root = s_root, .paths = paths, .kinds = kinds, .count = count, .out = &out);
    s_out[(out.at < sizeof(s_out)) ? out.at : sizeof(s_out) - 1u] = '\0';
}

// a batch received, appended to the others, a line each
static void batch_received(const CliEachBatchSizeCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_batches_at + args->size + 1ull < sizeof(s_batches))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_batches + s_batches_at, .src = args->batch, .bytes = args->size});
        s_batches[s_batches_at + args->size] = '\n';
        s_batches_at += args->size + 1ull;
        s_batches[s_batches_at] = '\0';
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// whether the batches received name `text`
static int batches_name(const char *text)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const int named = cellul.find(&(CatenaFinitaCfg){.src = s_batches, .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL}) != NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return named;
}

// a watch run over a written file and a new file, once for the process: what the tests read of it is stored
static void watch_ran(void)
{
    if (s_watch_ran)
    {
        return;
    }
    s_watch_ran = 1;
    tree_written("watch_system");
    EMBED_CALL(cli_watch.start, CliWatchCfg, .root = s_root, .take = batch_received, .context = NULL);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 200u);
    // the watch's threads run as long as the watch does and store nothing in the temporary tier, whose cells last one call
    const unsigned long long settled = cli_clock_milliseconds() + 2000ull;
    while ((g_cli_cells->temporary_buf_mark() != CLI_CELLS_POOL) && (cli_clock_milliseconds() < settled))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
    s_temporary_mark = g_cli_cells->temporary_buf_mark();
    cli_unity_file(s_root, "src/a.rs", "two");
    cli_unity_file(s_root, "src/b.rs", "three");
    const unsigned long long until = cli_clock_milliseconds() + 20000ull;
    while (!(batches_name("\"src/a.rs\"") && batches_name("\"src/b.rs\"")) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 50u);
    }
    // the lists a round used stay allocated for the next: read once the changes are done and the last round has had longer than a
    // round's longest to end
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1500u);
    s_lists_in_use = mmgr_owner_in_use(&g_cli_watch_cells);
    cli_watch_stop();
}

// a change reported, `path kind` appended to the others, `|` between them
static void change_recorded(const CliEachPathKindCfg *args)
{
    mmgr_line_put(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    mmgr_line_put(&s_seen_line, args->path);
    mmgr_line_ch(&s_seen_line, ' ');
    mmgr_line_decimal(&s_seen_line, args->kind, 1u);
}

// a first poll of the tree, a file changed and a file added, and the second poll recorded, NUL-terminated in s_seen, once for the
// process, since a tree's first reading is stored until its process ends
static void tree_polled(void)
{
    static int s_polled = 0;
    if (s_polled)
    {
        return;
    }
    s_polled = 1;
    tree_written("watch_poll");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_watch_internal.first_reading, CliWatchCtx, .root = s_root));
    cli_unity_file(s_root, "src/a.rs", "a longer text");
    cli_unity_file(s_root, "src/b.rs", "three");
    s_seen_line.at = 0ull;
    EMBED_CALL(cli_watch_internal.read_again, CliWatchCtx, .root = s_root, .emit = change_recorded, .context = NULL);
    (void)mmgr_line_finish(&s_seen_line);
}

// the seen changes contain `path` with `kind`
static void assert_seen(const char *path, unsigned int kind)
{
    char wanted[CLI_PATH_MAX_LEN];
    VerbaLinea line = {wanted, sizeof(wanted), 0ull};
    mmgr_line_put(&line, path);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, kind, 1u);
    (void)mmgr_line_finish(&line);
    CLI_TEST_ASSERT_CONTAINS(wanted, s_seen);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_batch_reports_which_folders_and_files_changed(void)
{
    tree_written("watch_gather");
    batch_written(s_paths, s_kinds, 5u);
    TEST_ASSERT_EQUAL_STRING("{\"folders\":[\"\",\"src\"],\"files\":[\"src/a.rs\",\"src/b.rs\"],\"git\":true,\"lost\":false}", s_out);
}

void test_gits_objects_are_not_reported(void)
{
    tree_written("watch_gather");
    batch_written(s_paths + 4, s_kinds + 4, 1u);
    TEST_ASSERT_EQUAL_STRING("{\"folders\":[],\"files\":[],\"git\":false,\"lost\":false}", s_out);
}

void test_gits_index_changes_gits_view(void)
{
    static const char *const s_index[1] = {".git/index"};
    tree_written("watch_gather");
    batch_written(s_index, s_kinds, 1u);
    CLI_TEST_ASSERT_CONTAINS("\"git\":true", s_out);
}

void test_an_overflowed_record_of_changes_is_reported(void)
{
    static const char *const s_lost[1] = {""};
    static const unsigned int s_lost_kind[1] = {CLI_CHANGE_LOST};
    tree_written("watch_gather");
    batch_written(s_lost, s_lost_kind, 1u);
    CLI_TEST_ASSERT_CONTAINS("\"lost\":true", s_out);
}

void test_a_running_watch_stores_nothing_in_the_temporary_tier(void)
{
    watch_ran();
    TEST_ASSERT_EQUAL_UINT64(CLI_CELLS_POOL, s_temporary_mark);
}

void test_a_rounds_lists_stay_allocated_for_the_next_round(void)
{
    watch_ran();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(512ull << 10u, s_lists_in_use);
}

void test_the_system_reports_a_written_file(void)
{
    watch_ran();
    TEST_ASSERT_TRUE(batches_name("\"src/a.rs\""));
}

void test_the_system_reports_a_new_file(void)
{
    watch_ran();
    TEST_ASSERT_TRUE(batches_name("\"src/b.rs\""));
}

void test_the_system_reports_the_folder_of_a_new_file(void)
{
    watch_ran();
    TEST_ASSERT_TRUE(batches_name("\"src\""));
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

void test_changes_and_batches_use_memory_sized_to_them_and_release_it(void)
{
    watch_ran();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_watch_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_watch_cells.hw));
    TEST_ASSERT_LESS_THAN_UINT64(1ull << 20u, embed_atomic_usize_load(&g_cli_watch_cells.hw));
}
