// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.c: the changes the system tells of, or a reading of the tree finds, gathered in batches and handed to the
// window, less those the watching's patterns hide
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/patterns.h"
#include "../text/text.h"
#include "files_internal.h"
#include "watch_internal.h"

// how long a batch waits for another change before it goes, the longest it gathers changes for, in milliseconds; the
// most changes waiting and gathered in one batch; and the most bytes of a batch written
#define CLI_WATCH_QUIET 150ull
#define CLI_WATCH_BATCH_MOST 1000ull
#define CLI_WATCH_QUEUE 4096u
#define CLI_WATCH_GATHERED 16384u
#define CLI_WATCH_JSON (1u << 24u)

// The watch: the changes waiting, from `head`, `count` of them, and whether more came than they hold; whether it is
// stopping, whether it reads the tree itself, how many of its threads have ended; the tree, who takes the batches, and
// the threads
typedef struct
{
    char path[CLI_WATCH_QUEUE][CLI_PATH_LONGEST];
    unsigned char kind[CLI_WATCH_QUEUE];
    unsigned int head;
    unsigned int count;
    int overflowed;
    int stop;
    int running;
    int polling;
    unsigned int ended;
    char root[CLI_PATH_LONGEST];
    CliTake take;
    void *context;
    CliThread batcher;
    CliThread poller;
    CliWake wake;
} CliWatching;

// One batch as it is gathered: its changes, the folders and files it tells of, lists of cells counted to the files made
// at the batch's own size and given back as it is written, and the room it is written in; a cell the batcher holds
// while it runs
typedef struct
{
    char path[CLI_WATCH_GATHERED][CLI_PATH_LONGEST];
    unsigned char kind[CLI_WATCH_GATHERED];
    unsigned int count;
    int lost;
    CliPatterns patterns;
    CliFiles folders;
    CliFiles written;
    char json[CLI_WATCH_JSON];
} CliBatch;

static CliWatching s_watching = {.wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;

int cli_watch_stopping(void)
{
    cli_lock_take(&s_lock);
    const int stop = s_watching.stop;
    cli_lock_give(&s_lock);
    return stop;
}

void cli_watch_told(void *context, const char *path, unsigned int kind)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (!s_watching.stop)
    {
        if (s_watching.count < CLI_WATCH_QUEUE)
        {
            const unsigned int at = (s_watching.head + s_watching.count) % CLI_WATCH_QUEUE;
            memor.cpy(&(MemoriaCfg){.dst = s_watching.path[at],
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            s_watching.kind[at] = (unsigned char)kind;
            s_watching.count += 1u;
        }
        else
        {
            s_watching.overflowed = 1;
        }
        cli_wake_all(&s_watching.wake);
    }
    cli_lock_give(&s_lock);
}

// the changes waiting taken into the batch, the lock held
static void cli_waiting_taken(CliBatch *batch)
{
    while (s_watching.count > 0u)
    {
        const unsigned int at = s_watching.head;
        if (batch->count < CLI_WATCH_GATHERED)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = batch->path[batch->count], .bytes = CLI_PATH_LONGEST, .src = s_watching.path[at]});
            batch->kind[batch->count] = s_watching.kind[at];
            batch->count += 1u;
        }
        else
        {
            batch->lost = 1;
        }
        s_watching.head = (s_watching.head + 1u) % CLI_WATCH_QUEUE;
        s_watching.count -= 1u;
    }
    batch->lost = batch->lost || s_watching.overflowed;
    s_watching.overflowed = 0;
}

// the batch's changes gathered and written, less those the watching's patterns hide: 1 where any are left
static int cli_gathered(CliBatch *batch, CliLine *out)
{
    const char *const root = s_watching.root;
    cli_patterns_of(CLI_PART_WATCHING, &batch->patterns);
    // each change puts at most its own path in one of the lists, so the batch's paths and their bytes are their size
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < batch->count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = batch->path[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    const int made =
        cli_files_made(&batch->folders, batch->count, bytes) && cli_files_made(&batch->written, batch->count, bytes);
    batch->lost = batch->lost || !made;
    int git = batch->lost;
    int moved = 0;
    for (unsigned int at = 0u; at < batch->count; at += 1u)
    {
        const char *const path = batch->path[at];
        const unsigned int kind = batch->kind[at];
        if (kind == CLI_CHANGE_LOST)
        {
            batch->lost = 1;
            git = 1;
            continue;
        }
        if ((cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ".git/", .cap = CLI_CELLS_POOL})))
        {
            git = git || (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/index", .cap = CLI_CELLS_POOL})) ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/HEAD", .cap = CLI_CELLS_POOL})) ||
                  (cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ".git/refs/", .cap = CLI_CELLS_POOL}));
            continue;
        }
        char full[CLI_PATH_LONGEST];
        cli_full(root, path, full);
        const int dir = query_is_folder(full);
        if ((path[0] == '\0') || cli_patterns_hides(&batch->patterns, path, dir))
        {
            continue;
        }
        const int tracked = cli_files_tracked(root, path);
        git = git || tracked;
        if ((kind == CLI_CHANGE_ADDED) || (kind == CLI_CHANGE_REMOVED))
        {
            const char *const slash =
                cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
            // the slash lies in the path, at or past its start
            cli_files_put(&batch->folders, path, (slash != NULL) ? (unsigned long long)(slash - path) : 0ull);
            moved = moved || tracked;
            if ((kind == CLI_CHANGE_ADDED) && !dir)
            {
                cli_files_put(&batch->written, path,
                              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
            }
        }
        else if (dir)
        {
            cli_files_put(&batch->folders, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            cli_files_put(&batch->written, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
    }
    if (moved || batch->lost)
    {
        cli_files_forget_seen();
    }
    batch->lost = batch->lost || !cli_files_sorted(&batch->folders) || !cli_files_sorted(&batch->written);
    cli_line_text(out, "{\"folders\":[");
    for (unsigned int at = 0u; at < batch->folders.count; at += 1u)
    {
        const char *const folder = cli_files_at(&batch->folders, at);
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, "],\"files\":[");
    for (unsigned int at = 0u; at < batch->written.count; at += 1u)
    {
        const char *const file = cli_files_at(&batch->written, at);
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, git ? "],\"git\":true" : "],\"git\":false");
    cli_line_text(out, batch->lost ? ",\"lost\":true}" : ",\"lost\":false}");
    const int any = (batch->folders.count != 0u) || (batch->written.count != 0u) || git || batch->lost;
    cli_files_emptied(&batch->folders);
    cli_files_emptied(&batch->written);
    return any;
}

// the changes gathered into batches and each handed over, until the watch stops
static void cli_batched(void *context)
{
    (void)context;
    // the batch a cell for as long as the batcher runs; with no room for it the batcher ends at once
    CliBatch *const batch = cli_cell_taken(sizeof(CliBatch), "the watching's batch");
    cli_lock_take(&s_lock);
    while (batch != NULL)
    {
        while ((s_watching.count == 0u) && !s_watching.overflowed && !s_watching.stop)
        {
            cli_wake_wait(&s_watching.wake, &s_lock);
        }
        if (s_watching.stop)
        {
            break;
        }
        batch->count = 0u;
        batch->lost = 0;
        cli_waiting_taken(batch);
        // more changes taken while they come within CLI_WATCH_QUIET of one another, for at most CLI_WATCH_BATCH_MOST
        const unsigned long long began = cli_clock_milliseconds();
        for (;;)
        {
            const unsigned long long spent = cli_clock_milliseconds() - began;
            if ((spent >= CLI_WATCH_BATCH_MOST) || s_watching.stop)
            {
                break;
            }
            const unsigned long long left = CLI_WATCH_BATCH_MOST - spent;
            const unsigned long long quiet = (left < CLI_WATCH_QUIET) ? left : CLI_WATCH_QUIET;
            const unsigned long long until = cli_clock_milliseconds() + quiet;
            while ((s_watching.count == 0u) && !s_watching.overflowed && !s_watching.stop &&
                   (cli_clock_milliseconds() < until))
            {
                cli_wake_wait_for(&s_watching.wake, &s_lock, until - cli_clock_milliseconds());
            }
            if ((s_watching.count == 0u) && !s_watching.overflowed)
            {
                break;
            }
            cli_waiting_taken(batch);
        }
        cli_lock_give(&s_lock);
        CliLine out = {batch->json, sizeof(batch->json), 0ull};
        const int any = cli_gathered(batch, &out);
        if (cli_watch_stopping())
        {
            cli_lock_take(&s_lock);
            break;
        }
        if (any && cli_line_fits(&out))
        {
            s_watching.take(s_watching.context, batch->json, out.at);
        }
        cli_lock_take(&s_lock);
    }
    s_watching.ended += 1u;
    cli_wake_all(&s_watching.wake);
    cli_lock_give(&s_lock);
    cli_cell_given(batch);
}

void cli_watch_stop(void)
{
    cli_lock_take(&s_lock);
    if (!s_watching.running)
    {
        cli_lock_give(&s_lock);
        return;
    }
    s_watching.stop = 1;
    cli_wake_all(&s_watching.wake);
    cli_lock_give(&s_lock);
    cli_watch_system_stop();
    cli_lock_take(&s_lock);
    const unsigned int threads = s_watching.polling ? 2u : 1u;
    while (s_watching.ended < threads)
    {
        cli_wake_wait(&s_watching.wake, &s_lock);
    }
    s_watching.running = 0;
    cli_lock_give(&s_lock);
}

void cli_watch_start(const char *root, CliTake take, void *context)
{
    cli_watch_stop();
    cli_lock_take(&s_lock);
    memor.cpy(&(MemoriaCfg){.dst = s_watching.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_watching.take = take;
    s_watching.context = context;
    s_watching.head = 0u;
    s_watching.count = 0u;
    s_watching.overflowed = 0;
    s_watching.stop = 0;
    s_watching.ended = 0u;
    s_watching.polling = 0;
    s_watching.running = 1;
    cli_lock_give(&s_lock);
    if (!cli_watch_system_start(root, cli_watch_told, NULL))
    {
        s_watching.poller = (CliThread){cli_watch_polled, s_watching.root};
        const int polling = cli_watch_first_reading(root) && cli_thread_started(&s_watching.poller);
        if (!polling)
        {
            cli_watch_readings_given();
        }
        cli_lock_take(&s_lock);
        s_watching.polling = polling;
        cli_lock_give(&s_lock);
    }
    s_watching.batcher = (CliThread){cli_batched, NULL};
    if (!cli_thread_started(&s_watching.batcher))
    {
        cli_lock_take(&s_lock);
        s_watching.ended += 1u;
        cli_lock_give(&s_lock);
    }
}

void cli_watch_poll_ended(void)
{
    cli_lock_take(&s_lock);
    s_watching.ended += 1u;
    cli_wake_all(&s_watching.wake);
    cli_lock_give(&s_lock);
}

int cli_watch_gathered_of(const char *root, const char *const *paths, const unsigned int *kinds, unsigned int count,
                          CliLine *out)
{
    memor.cpy(&(MemoriaCfg){.dst = s_watching.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    CliBatch *const batch = cli_cell_taken(sizeof(CliBatch), "a batch gathered");
    if (batch == NULL)
    {
        return 0;
    }
    batch->count = 0u;
    batch->lost = 0;
    for (unsigned int at = 0u; (at < count) && (at < CLI_WATCH_GATHERED); at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = batch->path[at],
                                .src = paths[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        // a change's kind is one of the few CLI_CHANGE_ names, below 256
        batch->kind[at] = (unsigned char)kinds[at];
        batch->count += 1u;
    }
    const int any = cli_gathered(batch, out);
    cli_cell_given(batch);
    return any;
}
