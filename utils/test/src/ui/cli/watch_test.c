// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_test.c: the watch's check: a batch says which folders and files changed, the system tells of a file written
// and one made, and a reading of the tree finds what changed since the last
#include "../../../../../ui/cli/src/editor/watch_internal.h"
#include "cli_check.h"

static char s_out[1u << 16u];
static CliLock s_lock = CLI_LOCK_INIT;
static char s_batches[1u << 16u];
static unsigned long long s_batches_at;
static char s_seen[1u << 14u];
static CliLine s_seen_line = {s_seen, sizeof(s_seen), 0ull};

// the tree `name`, its src/a.rs written, into `root`, which holds CLI_PATH_LONGEST
static void watch_tree(const char *name, char *root)
{
    char path[CLI_PATH_LONGEST];
    cli_check_folder(name, root);
    cli_full(root, "src", path);
    (void)query_folder_made(path);
    cli_full(root, "src/a.rs", path);
    (void)query_file_written(path, "one", 3ull);
}

// the file `name` of the tree written with `text`
static void watch_file(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the batch of `count` changes written into s_out
static void watch_gathered(const char *root, const char *const *paths, const unsigned int *kinds, unsigned int count)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_watch_gathered_of(root, paths, kinds, count, &out);
    s_out[(out.at < sizeof(s_out)) ? out.at : sizeof(s_out) - 1u] = '\0';
}

// a batch says which folders and files changed
static void watch_batch(void)
{
    static const char *const s_paths[5] = {"src/b.rs", "src/a.rs", "src", "old.txt", ".git/objects/ab/cdef"};
    static const unsigned int s_kinds[5] = {CLI_CHANGE_ADDED, CLI_CHANGE_MODIFIED, CLI_CHANGE_MODIFIED,
                                            CLI_CHANGE_REMOVED, CLI_CHANGE_ADDED};
    char root[CLI_PATH_LONGEST];
    watch_tree("watch_gather", root);
    watch_gathered(root, s_paths, s_kinds, 5u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = s_out,
                  .other = "{\"folders\":[\"\",\"src\"],\"files\":[\"src/a.rs\",\"src/b.rs\"],\"git\":true,"
                           "\"lost\":false}",
                  .cap = CLI_CELLS_POOL}),
              "a batch says which folders and files changed");
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
              "a record of changes that ran over is told");
}

// a batch taken, kept after the others, a line each
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

// whether the batches taken name `text`
static int watch_named(const char *text)
{
    cli_lock_take(&s_lock);
    const int named = cellul.find(&(CatenaFinitaCfg){
                          .src = s_batches, .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL}) != NULL;
    cli_lock_give(&s_lock);
    return named;
}

// the system tells of a file written and one made
static void watch_system(void)
{
    char root[CLI_PATH_LONGEST];
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
              "the system tells of a file written and one made");
    cli_check(watch_named("\"src\""), "and of the folder that holds the one made");
}

// a change told kept, `path kind` after the others, `|` between them
static void watch_told(void *context, const char *path, unsigned int kind)
{
    (void)context;
    cli_line_text(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    cli_line_text(&s_seen_line, path);
    cli_line_character(&s_seen_line, ' ');
    cli_line_decimal(&s_seen_line, kind, 1u);
}

// a reading of the tree finds what changed since the last
static void watch_reading(void)
{
    char root[CLI_PATH_LONGEST];
    char wanted[CLI_PATH_LONGEST];
    watch_tree("watch_poll", root);
    cli_check(cli_watch_first_reading(root), "the first reading is taken");
    watch_file(root, "src/a.rs", "a longer text");
    watch_file(root, "src/b.rs", "three");
    cli_watch_read_again(root, watch_told, NULL);
    (void)cli_line_closed(&s_seen_line);
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line, "src/a.rs ");
    cli_line_decimal(&line, CLI_CHANGE_MODIFIED, 1u);
    (void)cli_line_closed(&line);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_seen, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = CLI_CELLS_POOL}) != NULL,
              "a file written is found written");
    line.at = 0ull;
    cli_line_text(&line, "src/b.rs ");
    cli_line_decimal(&line, CLI_CHANGE_ADDED, 1u);
    (void)cli_line_closed(&line);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_seen, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = CLI_CELLS_POOL}) != NULL,
              "and one made found made");
    cli_watch_readings_given();
}

int main(void)
{
    watch_batch();
    watch_system();
    watch_reading();
    return cli_checked();
}
