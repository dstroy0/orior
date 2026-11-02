// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.c: the changes the system reports, or a reading of the tree finds, gathered in batches and handed to the
// window, less those the watching's patterns hide
#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/patterns.h"
#include "../text/text.h"
#include "files_internal.h"
#include "watch_internal.h"

// how long a bundle waits for another change before it goes, the longest it gathers changes for, in milliseconds; the
// most changes waiting and gathered in one bundle; the changes and the bytes of their paths a list of them is taken
// with; and the bytes a bundle is written in
#define CLI_WATCH_QUIET 150ull
#define CLI_WATCH_BATCH_MOST 1000ull
#define CLI_WATCH_QUEUE 4096u
#define CLI_WATCH_GATHERED 16384u
#define CLI_WATCH_TEXT_ROOM (256ull << 10u)

// A change: its path's place in the list's texts, and its kind
typedef struct
{
    unsigned long long at;
    unsigned int kind;
} CliChange;

// A list of changes: `count` of them in a cell of `change_bytes`, and their paths, `used` bytes in a cell of `bytes`;
// each cell at their most
typedef struct
{
    CliChange *change;
    unsigned int count;
    unsigned long long change_bytes;
    char *text;
    unsigned long long used;
    unsigned long long bytes;
} CliChanges;

// The watch: the changes waiting, and whether more came than they contain; whether it is stopping, whether it reads the
// tree itself, how many of its threads have ended; the tree, who takes the bundles, and the threads
typedef struct
{
    CliChanges waiting;
    int overflowed;
    int stop;
    int running;
    int polling;
    unsigned int ended;
    char root[CLI_PATH_MAX_LEN];
    CliTake take;
    void *context;
    CliThread batcher;
    CliThread poller;
    CliWake wake;
} CliWatching;

// One bundle as it is gathered: its changes, and the folders and files it reports, lists of cells counted to the files
// made at the bundle's own size and given back as it is written; a cell the batcher owns while it runs
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

// the change of `kind` at `path` put after the list's: 1, or 0 where it contains `most` or the cellblock could not meet
// the room for it
static int cli_change_put(CliChanges *changes, const char *path, unsigned int kind, unsigned int most)
{
    if (changes->count >= most)
    {
        return 0;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    if (changes->text == NULL)
    {
        changes->text = g_cli_cells->persistent_buf_alloc(CLI_WATCH_TEXT_ROOM, &g_cli_watch_cells);
        changes->change =
            g_cli_cells->persistent_buf_alloc((unsigned long long)most * sizeof(CliChange), &g_cli_watch_cells);
        changes->bytes = (changes->text != NULL) ? CLI_WATCH_TEXT_ROOM : 0ull;
        changes->change_bytes = (changes->change != NULL) ? ((unsigned long long)most * sizeof(CliChange)) : 0ull;
    }
    if ((changes->change == NULL) || ((changes->used + length) > changes->bytes))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = changes->text + changes->used, .src = path, .bytes = (size_t)length});
    changes->change[changes->count] = (CliChange){changes->used, kind};
    changes->used += length;
    changes->count += 1u;
    return 1;
}

// the path of change `index` of a list
static const char *cli_change_path(const CliChanges *changes, unsigned int index)
{
    return changes->text + changes->change[index].at;
}

// a list's cells given back, the list containing none
static void cli_changes_given(CliChanges *changes)
{
    g_cli_cells->persistent_buf_release(changes->change, &g_cli_watch_cells);
    g_cli_cells->persistent_buf_release(changes->text, &g_cli_watch_cells);
    *changes = (CliChanges){NULL, 0u, 0ull, NULL, 0ull, 0ull};
}

// a list emptied, its cells standing for the next round
static void cli_changes_cleared(CliChanges *changes)
{
    changes->count = 0u;
    changes->used = 0ull;
}

int cli_watch_stopping(void)
{
    cli_lock_take(&s_lock);
    const int stop = s_watching.stop;
    cli_lock_give(&s_lock);
    return stop;
}

void cli_watch_changed(void *context, const char *path, unsigned int kind)
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

// the changes waiting taken into the bundle and their cells given back, the lock acquired
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
    cli_changes_cleared(&s_watching.waiting);
    batch->lost = batch->lost || s_watching.overflowed;
    s_watching.overflowed = 0;
}

// the bundle's changes gathered and written, less those the watching's patterns hide: 1 where any are left
static int cli_gathered(CliBatch *batch, VerbaLinea *out)
{
    const char *const root = s_watching.root;
    cli_patterns_of(CLI_PART_WATCHING, &batch->patterns);
    // each change puts at most its own path in one of the lists, and the bundle's paths and their bytes are their size
    const unsigned int count = batch->changes.count;
    const unsigned long long bytes = batch->changes.used;
    const int made =
        cli_files_work_made(&batch->folders, count, bytes) && cli_files_work_made(&batch->written, count, bytes);
    batch->lost = batch->lost || !made;
    int git = batch->lost;
    int relist = batch->lost;
    // the paths that came and went, to be mirrored to their closest parents as the round ends
    const char **const moved =
        g_cli_cells->temporary_buf_alloc((unsigned long long)count * sizeof(const char *), &g_cli_watch_cells);
    unsigned int moves = 0u;
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
            relist = relist || (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/index", .cap = CLI_CELLS_POOL})) ||
                     (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/info/exclude", .cap = CLI_CELLS_POOL}));
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        relist = relist || ((length >= 10ull) && cli_text_same(path + length - 10ull, ".gitignore", 10ull) &&
                            ((length == 10ull) || (path[length - 11ull] == '/')));
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, path, full);
        const int dir = query_is_folder(full);
        const int tracked = (path[0] != '\0') && cli_files_tracked(root, path);
        if (tracked && (moved != NULL) && ((kind == CLI_CHANGE_ADDED) || (kind == CLI_CHANGE_REMOVED)))
        {
            moved[moves] = path;
            moves += 1u;
        }
        if ((path[0] == '\0') || cli_patterns_hides(&batch->patterns, path, dir))
        {
            continue;
        }
        git = git || tracked;
        if ((kind == CLI_CHANGE_ADDED) || (kind == CLI_CHANGE_REMOVED))
        {
            const char *const slash =
                cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
            // the slash lies in the path, at or past its start
            cli_files_put(&batch->folders, path, (slash != NULL) ? (unsigned long long)(slash - path) : 0ull);
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
    // a change joins or leaves the one branch it names; the tree is listed again only where the rules of the listing
    // changed, or changes were lost
    if (relist)
    {
        cli_tree_relisted(root, NULL, NULL);
    }
    else
    {
        cli_tree_mirrored(root, moved, moves);
    }
    g_cli_cells->persistent_buf_release((void *)moved, &g_cli_watch_cells);
    batch->lost = batch->lost || !cli_files_sorted(&batch->folders) || !cli_files_sorted(&batch->written);
    mmgr_line_put(out, "{\"folders\":[");
    for (unsigned int at = 0u; at < batch->folders.count; at += 1u)
    {
        const char *const folder = cli_files_at(&batch->folders, at);
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, "],\"files\":[");
    for (unsigned int at = 0u; at < batch->written.count; at += 1u)
    {
        const char *const file = cli_files_at(&batch->written, at);
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, git ? "],\"git\":true" : "],\"git\":false");
    mmgr_line_put(out, batch->lost ? ",\"lost\":true}" : ",\"lost\":false}");
    const int any = (batch->folders.count != 0u) || (batch->written.count != 0u) || git || batch->lost;
    cli_files_emptied(&batch->folders);
    cli_files_emptied(&batch->written);
    cli_changes_cleared(&batch->changes);
    return any;
}

// the changes gathered into bundles and each handed over, until the watch stops
static void cli_batched(void *context)
{
    (void)context;
    // the bundle a cell for as long as the batcher runs, past any one call; with no room for it the batcher ends at
    // once
    CliBatch *const batch = g_cli_cells->persistent_buf_alloc(sizeof(CliBatch), &g_cli_watch_cells);
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
        const unsigned long long out_room =
            (batch->changes.used * 12ull) + ((unsigned long long)batch->changes.count * 32ull) + 256ull;
        char *const out_cell = g_cli_cells->temporary_buf_alloc(out_room, &g_cli_watch_cells);
        VerbaLinea out = mmgr_line_over(out_cell, out_room);
        const int any = cli_gathered(batch, &out);
        const int stopping = cli_watch_stopping();
        if (!stopping && any && mmgr_line_ok(&out))
        {
            s_watching.take(s_watching.context, out.out, out.at);
        }
        g_cli_cells->persistent_buf_release(out.out, &g_cli_watch_cells);
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
    if (batch != NULL)
    {
        cli_changes_given(&batch->changes);
    }
    g_cli_cells->persistent_buf_release(batch, &g_cli_watch_cells);
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
    if (!cli_watch_system_start(root, cli_watch_changed, NULL))
    {
        s_watching.poller = (CliThread){cli_watch_polled, s_watching.root};
        const int polling = cli_watch_first_reading(root) && cli_thread_started(&s_watching.poller);
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
                          VerbaLinea *out)
{
    memor.cpy(&(MemoriaCfg){.dst = s_watching.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    CliBatch *const batch = g_cli_cells->temporary_buf_alloc(sizeof(CliBatch), &g_cli_watch_cells);
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
    cli_changes_given(&batch->changes);
    g_cli_cells->persistent_buf_release(batch, &g_cli_watch_cells);
    return any;
}
