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
// most changes waiting and gathered in one batch; the changes and the bytes of their paths a list of them is begun
// with, doubling as they come; and the bytes a batch is begun to be written in, doubling as it is
#define CLI_WATCH_QUIET 150ull
#define CLI_WATCH_BATCH_MOST 1000ull
#define CLI_WATCH_QUEUE 4096u
#define CLI_WATCH_GATHERED 16384u
#define CLI_WATCH_CHANGES_FIRST 64u
#define CLI_WATCH_TEXT_FIRST 4096u
#define CLI_WATCH_JSON_FIRST 4096u

CliCellsTally g_cli_watch_cells = CLI_CELLS_TALLY("cli_watch");

// A change: its path's place in the list's texts, and its kind
typedef struct
{
    unsigned long long at;
    unsigned int kind;
} CliChange;

// A list of changes: `count` of them in a cell of `change_bytes`, and their paths, `used` bytes in a cell of `bytes`;
// each cell grown as changes are put and given back as the list is let go
typedef struct
{
    CliChange *change;
    unsigned int count;
    unsigned long long change_bytes;
    char *text;
    unsigned long long used;
    unsigned long long bytes;
} CliChanges;

// The watch: the changes waiting, and whether more came than they hold; whether it is stopping, whether it reads the
// tree itself, how many of its threads have ended; the tree, who takes the batches, and the threads
typedef struct
{
    CliChanges waiting;
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

// One batch as it is gathered: its changes, and the folders and files it tells of, lists of cells counted to the files
// made at the batch's own size and given back as it is written; a cell the batcher holds while it runs
typedef struct
{
    CliChanges changes;
    int lost;
    CliPatterns patterns;
    CliFiles folders;
    CliFiles written;
} CliBatch;

static CliWatching s_watching = {.wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;

// a cell of `*room` bytes, `held` of them in use, made to hold `wanted`: moved into a cell of twice its room, or of
// `first` where it has none, doubled until it does, its bytes copied and the old cell given back. The cell after, or
// NULL where the cellblock could not meet it, the old cell kept
static void *cli_watch_grown(void *cell, unsigned long long held, unsigned long long *room, unsigned long long wanted,
                             unsigned long long first)
{
    if (wanted <= *room)
    {
        return cell;
    }
    unsigned long long grown = (*room == 0ull) ? first : *room;
    while (grown < wanted)
    {
        grown *= 2ull;
    }
    void *const larger = cli_cell_taken_by(&g_cli_watch_cells, grown, "the changes a watch holds");
    if (larger == NULL)
    {
        return NULL;
    }
    if (held != 0ull)
    {
        memor.cpy(&(MemoriaCfg){.dst = larger, .src = cell, .bytes = (size_t)held});
    }
    cli_cell_given_by(&g_cli_watch_cells, cell);
    *room = grown;
    return larger;
}

// the change of `kind` at `path` put after the list's: 1, or 0 where it holds `most` or the cellblock could not meet
// the room for it
static int cli_change_put(CliChanges *changes, const char *path, unsigned int kind, unsigned int most)
{
    if (changes->count >= most)
    {
        return 0;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    char *const text =
        cli_watch_grown(changes->text, changes->used, &changes->bytes, changes->used + length, CLI_WATCH_TEXT_FIRST);
    if (text == NULL)
    {
        return 0;
    }
    changes->text = text;
    CliChange *const change =
        cli_watch_grown(changes->change, changes->count * sizeof(CliChange), &changes->change_bytes,
                        (changes->count + 1ull) * sizeof(CliChange), CLI_WATCH_CHANGES_FIRST * sizeof(CliChange));
    if (change == NULL)
    {
        return 0;
    }
    changes->change = change;
    memor.cpy(&(MemoriaCfg){.dst = text + changes->used, .src = path, .bytes = (size_t)length});
    change[changes->count] = (CliChange){changes->used, kind};
    changes->used += length;
    changes->count += 1u;
    return 1;
}

// the path of change `index` of a list
static const char *cli_change_path(const CliChanges *changes, unsigned int index)
{
    return changes->text + changes->change[index].at;
}

// a list's cells given back, the list holding none
static void cli_changes_given(CliChanges *changes)
{
    cli_cell_given_by(&g_cli_watch_cells, changes->change);
    cli_cell_given_by(&g_cli_watch_cells, changes->text);
    *changes = (CliChanges){NULL, 0u, 0ull, NULL, 0ull, 0ull};
}

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
        if (!cli_change_put(&s_watching.waiting, path, kind, CLI_WATCH_QUEUE))
        {
            s_watching.overflowed = 1;
        }
        cli_wake_all(&s_watching.wake);
    }
    cli_lock_give(&s_lock);
}

// the changes waiting taken into the batch and their cells given back, the lock held
static void cli_waiting_taken(CliBatch *batch)
{
    for (unsigned int at = 0u; at < s_watching.waiting.count; at += 1u)
    {
        if (!cli_change_put(&batch->changes, cli_change_path(&s_watching.waiting, at),
                            s_watching.waiting.change[at].kind, CLI_WATCH_GATHERED))
        {
            batch->lost = 1;
        }
    }
    cli_changes_given(&s_watching.waiting);
    batch->lost = batch->lost || s_watching.overflowed;
    s_watching.overflowed = 0;
}

// the batch's changes gathered and written, less those the watching's patterns hide: 1 where any are left
static int cli_gathered(CliBatch *batch, CliLine *out)
{
    const char *const root = s_watching.root;
    cli_patterns_of(CLI_PART_WATCHING, &batch->patterns);
    // each change puts at most its own path in one of the lists, so the batch's paths and their bytes are their size
    const unsigned int count = batch->changes.count;
    const unsigned long long bytes = batch->changes.used;
    const int made = cli_files_made(&batch->folders, count, bytes) && cli_files_made(&batch->written, count, bytes);
    batch->lost = batch->lost || !made;
    int git = batch->lost;
    int moved = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = cli_change_path(&batch->changes, at);
        const unsigned int kind = batch->changes.change[at].kind;
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
    cli_changes_given(&batch->changes);
    return any;
}

// the changes gathered into batches and each handed over, until the watch stops
static void cli_batched(void *context)
{
    (void)context;
    // the batch a cell for as long as the batcher runs; with no room for it the batcher ends at once
    CliBatch *const batch = cli_cell_taken_by(&g_cli_watch_cells, sizeof(CliBatch), "the watching's batch");
    cli_lock_take(&s_lock);
    if (batch != NULL)
    {
        batch->changes = (CliChanges){NULL, 0u, 0ull, NULL, 0ull, 0ull};
    }
    while (batch != NULL)
    {
        while ((s_watching.waiting.count == 0u) && !s_watching.overflowed && !s_watching.stop)
        {
            cli_wake_wait(&s_watching.wake, &s_lock);
        }
        if (s_watching.stop)
        {
            break;
        }
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
            while ((s_watching.waiting.count == 0u) && !s_watching.overflowed && !s_watching.stop &&
                   (cli_clock_milliseconds() < until))
            {
                cli_wake_wait_for(&s_watching.wake, &s_lock, until - cli_clock_milliseconds());
            }
            if ((s_watching.waiting.count == 0u) && !s_watching.overflowed)
            {
                break;
            }
            cli_waiting_taken(batch);
        }
        cli_lock_give(&s_lock);
        CliLine out;
        (void)cli_line_growing(&out, CLI_WATCH_JSON_FIRST, "a batch of changes written");
        const int any = cli_gathered(batch, &out);
        const int stopping = cli_watch_stopping();
        if (!stopping && any && cli_line_fits(&out))
        {
            s_watching.take(s_watching.context, out.out, out.at);
        }
        cli_cell_given(out.out);
        cli_lock_take(&s_lock);
        if (stopping)
        {
            break;
        }
    }
    cli_changes_given(&s_watching.waiting);
    s_watching.ended += 1u;
    cli_wake_all(&s_watching.wake);
    cli_lock_give(&s_lock);
    cli_cell_given_by(&g_cli_watch_cells, batch);
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
    cli_changes_given(&s_watching.waiting);
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
    CliBatch *const batch = cli_cell_taken_by(&g_cli_watch_cells, sizeof(CliBatch), "a batch gathered");
    if (batch == NULL)
    {
        return 0;
    }
    batch->changes = (CliChanges){NULL, 0u, 0ull, NULL, 0ull, 0ull};
    batch->lost = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        batch->lost = batch->lost || !cli_change_put(&batch->changes, paths[at], kinds[at], CLI_WATCH_GATHERED);
    }
    const int any = cli_gathered(batch, out);
    cli_cell_given_by(&g_cli_watch_cells, batch);
    return any;
}
