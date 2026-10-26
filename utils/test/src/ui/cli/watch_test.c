// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_test.c: the watch's check: a batch reports which folders and files changed, the system reports a file written
// and one made, and a reading of the tree finds what changed since the last
#include "../../../../../ui/cli/src/editor/watch_internal.h"
#include "cli_check.h"

static char s_out[1u << 16u];
static CliLock s_lock = CLI_LOCK_INIT;
static char s_batches[1u << 16u];
static unsigned long long s_batches_at;
static char s_seen[1u << 14u];
static VerbaLinea s_seen_line = {s_seen, sizeof(s_seen), 0ull};

// the tree `name`, its src/a.rs written, into `root`, which is sized for CLI_PATH_MAX_LEN
static void watch_tree(const char *name, char *root)
{
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder(name, root);
    cli_full(root, "src", path);
    (void)query_folder_made(path);
    cli_full(root, "src/a.rs", path);
    (void)query_file_written(path, "one", 3ull);
}

// the file `name` of the tree written with `text`
static void watch_file(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the group of `count` changes written into s_out
static void watch_gathered(const char *root, const char *const *paths, const unsigned int *kinds, unsigned int count)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_watch_gathered_of(root, paths, kinds, count, &out);
    s_out[(out.at < sizeof(s_out)) ? out.at : sizeof(s_out) - 1u] = '\0';
}

// a group reports which folders and files changed
static void watch_batch(void)
{
    static const char *const s_paths[5] = {"src/b.rs", "src/a.rs", "src", "old.txt", ".git/objects/ab/cdef"};
    static const unsigned int s_kinds[5] = {CLI_CHANGE_ADDED, CLI_CHANGE_MODIFIED, CLI_CHANGE_MODIFIED,
                                            CLI_CHANGE_REMOVED, CLI_CHANGE_ADDED};
    char root[CLI_PATH_MAX_LEN];
    watch_tree("watch_gather", root);
    watch_gathered(root, s_paths, s_kinds, 5u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = s_out,
                  .other = "{\"folders\":[\"\",\"src\"],\"files\":[\"src/a.rs\",\"src/b.rs\"],\"git\":true,"
                           "\"lost\":false}",
                  .cap = CLI_CELLS_POOL}),
              "a batch reports which folders and files changed");
    watch_gathered(root, s_paths + 4, s_kinds + 4, 1u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_out,
                                           .other = "{\"folders\":[],\"files\":[],\"git\":false,\"lost\":false}",
                                           .cap = CLI_CELLS_POOL}),
              "git's objects are none of it");
    static const char *const s_index[1] = {".git/index"};
    watch_gathered(root, s_index, s_kinds, 1u);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"git\":true", .other_cap = CLI_CELLS_POOL}) != NULL,
              "git's index changes git's view");
    static const char *const s_lost[1] = {""};
    static const unsigned int s_lost_kind[1] = {CLI_CHANGE_LOST};
    watch_gathered(root, s_lost, s_lost_kind, 1u);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"lost\":true", .other_cap = CLI_CELLS_POOL}) != NULL,
              "a record of changes that ran over is reported");
}

// a group taken, recorded after the others, a line each
static void watch_taken(void *context, const char *batch, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (s_batches_at + size + 1ull < sizeof(s_batches))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_batches + s_batches_at, .src = batch, .bytes = size});
        s_batches[s_batches_at + size] = '\n';
        s_batches_at += size + 1ull;
        s_batches[s_batches_at] = '\0';
    }
    cli_lock_give(&s_lock);
}

// whether the groups taken name `text`
static int watch_named(const char *text)
{
    cli_lock_take(&s_lock);
    const int named = cellul.find(&(CatenaFinitaCfg){
                          .src = s_batches, .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL}) != NULL;
    cli_lock_give(&s_lock);
    return named;
}

// the system reports a file written and one made
static void watch_system(void)
{
    char root[CLI_PATH_MAX_LEN];
    watch_tree("watch_system", root);
    cli_watch_start(root, watch_taken, NULL);
    query_paused(200u);
    watch_file(root, "src/a.rs", "two");
    watch_file(root, "src/b.rs", "three");
    const unsigned long long until = cli_clock_milliseconds() + 20000ull;
    while (!(watch_named("\"src/a.rs\"") && watch_named("\"src/b.rs\"")) && (cli_clock_milliseconds() < until))
    {
        query_paused(50u);
    }
    cli_watch_stop();
    cli_check(watch_named("\"src/a.rs\"") && watch_named("\"src/b.rs\""),
              "the system reports a file written and one made");
    cli_check(watch_named("\"src\""), "and of the folder that contains the one made");
}

// a change reported recorded, `path kind` after the others, `|` between them
static void watch_change_capture(void *context, const char *path, unsigned int kind)
{
    (void)context;
    mmgr_line_put(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    mmgr_line_put(&s_seen_line, path);
    mmgr_line_ch(&s_seen_line, ' ');
    mmgr_line_decimal(&s_seen_line, kind, 1u);
}

// a reading of the tree finds what changed since the last
static void watch_reading(void)
{
    char root[CLI_PATH_MAX_LEN];
    char wanted[CLI_PATH_MAX_LEN];
    watch_tree("watch_poll", root);
    cli_check(cli_watch_first_reading(root), "the first reading is taken");
    watch_file(root, "src/a.rs", "a longer text");
    watch_file(root, "src/b.rs", "three");
    cli_watch_read_again(root, watch_change_capture, NULL);
    (void)mmgr_line_finish(&s_seen_line);
    VerbaLinea line = {wanted, sizeof(wanted), 0ull};
    mmgr_line_put(&line, "src/a.rs ");
    mmgr_line_decimal(&line, CLI_CHANGE_MODIFIED, 1u);
    (void)mmgr_line_finish(&line);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_seen, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = CLI_CELLS_POOL}) != NULL,
              "a file written is found written");
    line.at = 0ull;
    mmgr_line_put(&line, "src/b.rs ");
    mmgr_line_decimal(&line, CLI_CHANGE_ADDED, 1u);
    (void)mmgr_line_finish(&line);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_seen, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = CLI_CELLS_POOL}) != NULL,
              "and one made found made");
    cli_watch_readings_given();
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    watch_batch();
    watch_system();
    watch_reading();
    // the changes waiting and a group's are each a few paths, far below the room a fixed queue gave them
    cli_check((g_cli_watch_cells.in_use == 0ull) && (g_cli_watch_cells.hw != 0ull) &&
                  (g_cli_watch_cells.hw < (1ull << 20u)),
              "the changes and a batch are stored in cells sized to them, and given back");
    return cli_checked();
}
