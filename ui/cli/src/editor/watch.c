// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.c: the changes the system reports, or a reading of the tree finds, gathered in batches and handed to whoever
// takes them, less those the watching's patterns hide
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/patterns.h"
#include "../text/text.h"
#include "files_internal.h"
#include "watch_internal.h"

// A change: its path's place in the list's texts, and its type
typedef struct
{
    unsigned long long at;
    unsigned int type;
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
// One bundle as it is gathered: its changes, and the folders and files it reports, lists of cells counted to the files
// made at the bundle's own size and given back once it is taken; a cell the batcher owns while it runs
typedef struct
{
    CliChanges changes;
    int lost;
    int git;
    CliPatterns patterns;
    CliFiles folders;
    CliFiles written;
} CliBatch;
// how long a bundle waits for another change before it goes, the longest it gathers changes for, in milliseconds; the
// most changes waiting and gathered in one bundle; and the bytes of their paths a list of them is taken with
#define CLI_WATCH_QUIET 150ULL
#define CLI_WATCH_BATCH_MAX 1000ULL
#define CLI_WATCH_QUEUE 4096u
#define CLI_WATCH_GATHERED 16384u
#define CLI_WATCH_TEXT_CAPACITY (256ULL << 10u)

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
    CliWatchTake take;
    void *context;
    CliThread batcher;
    CliThread poller;
    CliWake wake;
} CliWatching;

static CliWatching s_watching = {.wake = CLI_WAKE_INIT};

typedef struct
{
    CliChanges *changes;
    const char *path;
    unsigned int type;
    unsigned int most;
    const CliChanges *changes_read;
    unsigned int index;
    CliBatch *batch;
} WatchCtx;
// The lists of changes stand fixed outside the pool, where they punch no hole in the persistent tier: the changes
// waiting and their paths, and the batcher's bundle with its changes and their paths
static CliChange s_waiting_change[CLI_WATCH_QUEUE];
static char s_waiting_text[CLI_WATCH_TEXT_CAPACITY];
static CliBatch s_batch;
static CliChange s_batch_change[CLI_WATCH_GATHERED];
static char s_batch_text[CLI_WATCH_TEXT_CAPACITY];

// the change of `type` at `path` put after the list's: 1, or 0 where it contains `most` or its cells cannot hold it
EMBED_INLINE int cli_change_append(const WatchCtx *args)
{
    if (args->changes->count >= args->most)
    {
        return 0;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1ULL;
    if ((args->changes->change == NULL) || ((args->changes->used + length) > args->changes->bytes))
    {
        return 0;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->changes->text + args->changes->used, .src = args->path, .bytes = (size_t)length});
    args->changes->change[args->changes->count] = (CliChange){args->changes->used, args->type};
    args->changes->used += length;
    args->changes->count += 1u;
    return 1;
}

// the path of change `index` of a list
EMBED_INLINE const char *cli_change_path(const WatchCtx *args)
{
    return args->changes_read->text + args->changes_read->change[args->index].at;
}

// a list emptied, its cells standing for the next round
EMBED_INLINE void cli_changes_clear(const WatchCtx *args)
{
    args->changes->count = 0u;
    args->changes->used = 0ULL;
}

int cli_watch_stopping(void)
{
    const int stop = s_watching.stop;
    return stop;
}

void cli_watch_change(const CliEachPathTypeCfg *args)
{
    const char *path = args->path;
    unsigned int type = args->type;
    if (!s_watching.stop)
    {
        if (!EMBED_CALL(cli_change_append, WatchCtx, .changes = &s_watching.waiting, .path = path, .type = type,
                        .most = CLI_WATCH_QUEUE))
        {
            s_watching.overflowed = 1;
        }
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_watching.wake);
    }
}

// the changes waiting taken into the bundle and their cells given back, the lock acquired
EMBED_INLINE void cli_waiting_alloc(const WatchCtx *args)
{
    for (unsigned int at = 0u; at < s_watching.waiting.count; at += 1u)
    {
        if (!EMBED_CALL(cli_change_append, WatchCtx, .changes = &args->batch->changes,
                        .path = EMBED_CALL(cli_change_path, WatchCtx, .changes_read = &s_watching.waiting, .index = at),
                        .type = s_watching.waiting.change[at].type, .most = CLI_WATCH_GATHERED))
        {
            args->batch->lost = 1;
        }
    }
    EMBED_CALL(cli_changes_clear, WatchCtx, .changes = &s_watching.waiting);
    args->batch->lost = args->batch->lost || s_watching.overflowed;
    s_watching.overflowed = 0;
}

// the bundle's changes gathered, less those the watching's patterns hide: 1 where any are left
static int cli_gather(const WatchCtx *args)
{
    const char *const root = s_watching.root;
    EMBED_CALL(cli_patterns_part.get, CliPatternsPartCfg, .part = CLI_PART_WATCHING,
               .patterns_out = &args->batch->patterns);
    // each change puts at most its own path in one of the lists, and the bundle's paths and their bytes are their size
    const unsigned int count = args->batch->changes.count;
    const unsigned long long bytes = args->batch->changes.used;
    const int made = EMBED_CALL(cli_files_bytes_internal.work_init, CliFilesBytesCtx, .files = &args->batch->folders,
                                .count = count, .bytes = bytes) &&
                     EMBED_CALL(cli_files_bytes_internal.work_init, CliFilesBytesCtx, .files = &args->batch->written,
                                .count = count, .bytes = bytes);
    args->batch->lost = args->batch->lost || !made;
    int git = args->batch->lost;
    int relist = args->batch->lost;
    // the paths that came and went, to be mirrored to their closest parents as the round ends
    const char **const moved =
        g_cli_guard->temporary_buf_alloc((unsigned long long)count * sizeof(const char *), g_cli_watch_owner);
    unsigned int moves = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path =
            EMBED_CALL(cli_change_path, WatchCtx, .changes_read = &args->batch->changes, .index = at);
        const unsigned int type = args->batch->changes.change[at].type;
        if (type == CLI_CHANGE_LOST)
        {
            args->batch->lost = 1;
            git = 1;
            continue;
        }
        if ((cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git", .cap = CLI_POOL_BYTES})) ||
            (cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ".git/", .cap = CLI_POOL_BYTES})))
        {
            git = git || (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/index", .cap = CLI_POOL_BYTES})) ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/HEAD", .cap = CLI_POOL_BYTES})) ||
                  (cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ".git/refs/", .cap = CLI_POOL_BYTES}));
            relist = relist ||
                     (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/index", .cap = CLI_POOL_BYTES})) ||
                     (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/info/exclude", .cap = CLI_POOL_BYTES}));
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES});
        relist = relist || ((length >= 10ULL) &&
                            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = path + length - 10ULL,
                                       .other_pointer = ".gitignore", .size = 10ULL) &&
                            ((length == 10ULL) || (path[length - 11ULL] == '/')));
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        const int dir = EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = full);
        const int tracked =
            (path[0] != '\0') && EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = root, .path = path);
        if (tracked && (moved != NULL) && ((type == CLI_CHANGE_ADDED) || (type == CLI_CHANGE_REMOVED)))
        {
            moved[moves] = path;
            moves += 1u;
        }
        if ((path[0] == '\0') || EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg,
                                            .patterns = &args->batch->patterns, .path = path, .dir = dir))
        {
            continue;
        }
        git = git || tracked;
        if ((type == CLI_CHANGE_ADDED) || (type == CLI_CHANGE_REMOVED))
        {
            const char *const slash = EMBED_CALL(
                cli_text_byte.last, CliTextByteCfg, .text = path,
                .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}), .byte = '/');
            // the slash lies in the path, at or past its start
            EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &args->batch->folders,
                       .path = path, .length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ULL);
            if ((type == CLI_CHANGE_ADDED) && !dir)
            {
                EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &args->batch->written,
                           .path = path, .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
            }
        }
        else if (dir)
        {
            EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &args->batch->folders,
                       .path = path, .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
        }
        else
        {
            EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &args->batch->written,
                       .path = path, .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
        }
    }
    // a change joins or leaves the one branch it names; the tree is listed again only where the rules of the listing
    // changed, or changes were lost
    if (relist)
    {
        EMBED_CALL(cli_symbols_context.tree_relist, CliSymbolsContextCfg, .root = root, .emit = NULL, .context = NULL);
    }
    else
    {
        EMBED_CALL(cli_symbols_root.tree_mirror, CliSymbolsRootCfg, .root = root, .paths = moved, .count = moves);
    }
    g_cli_guard->persistent_buf_release((void *)moved, g_cli_watch_owner);
    args->batch->lost = args->batch->lost ||
                        !EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = &args->batch->folders) ||
                        !EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = &args->batch->written);
    args->batch->git = git;
    return (args->batch->folders.count != 0u) || (args->batch->written.count != 0u) || git || args->batch->lost;
}

// the changes gathered into bundles and each handed over, until the watch stops
static void cli_batch(void *context)
{
    (void)context;
    // the bundle is the batcher's own for as long as it runs, past any one call
    CliBatch *const batch = &s_batch;
    memor.set(&(MemoriaCfg){.dst = batch, .val = 0, .bytes = sizeof(*batch)});
    batch->changes = (CliChanges){s_batch_change, 0u, sizeof(s_batch_change), s_batch_text, 0ULL, sizeof(s_batch_text)};
    int going = 1;
    while (going)
    {
        while ((s_watching.waiting.count == 0u) && !s_watching.overflowed && !s_watching.stop)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_watching.wake);
        }
        if (s_watching.stop)
        {
            going = 0;
            continue;
        }
        batch->lost = 0;
        EMBED_CALL(cli_waiting_alloc, WatchCtx, .batch = batch);
        // more changes taken while they come within CLI_WATCH_QUIET of one another, for at most CLI_WATCH_BATCH_MAX
        const unsigned long long began = cli_clock_milliseconds();
        int gathering = 1;
        while (gathering)
        {
            const unsigned long long spent = cli_clock_milliseconds() - began;
            if ((spent >= CLI_WATCH_BATCH_MAX) || s_watching.stop)
            {
                gathering = 0;
                continue;
            }
            const unsigned long long left = CLI_WATCH_BATCH_MAX - spent;
            const unsigned long long quiet = (left < CLI_WATCH_QUIET) ? left : CLI_WATCH_QUIET;
            const unsigned long long until = cli_clock_milliseconds() + quiet;
            while ((s_watching.waiting.count == 0u) && !s_watching.overflowed && !s_watching.stop &&
                   (cli_clock_milliseconds() < until))
            {
                EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_watching.wake,
                           .milliseconds_u64 = until - cli_clock_milliseconds());
            }
            if ((s_watching.waiting.count == 0u) && !s_watching.overflowed)
            {
                gathering = 0;
                continue;
            }
            EMBED_CALL(cli_waiting_alloc, WatchCtx, .batch = batch);
        }
        const int any = EMBED_CALL(cli_gather, WatchCtx, .batch = batch);
        const int stopping = cli_watch_stopping();
        if (!stopping && any)
        {
            EMBED_CALL(s_watching.take, CliWatchBatchCfg, .context = s_watching.context, .folders = &batch->folders,
                       .files = &batch->written, .git = batch->git, .lost = batch->lost);
        }
        EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &batch->folders);
        EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &batch->written);
        EMBED_CALL(cli_changes_clear, WatchCtx, .changes = &batch->changes);
        if (stopping)
        {
            going = 0;
            continue;
        }
    }
    EMBED_CALL(cli_changes_clear, WatchCtx, .changes = &s_watching.waiting);
    s_watching.ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_watching.wake);
    EMBED_CALL(cli_changes_clear, WatchCtx, .changes = &batch->changes);
}

void cli_watch_stop(void)
{
    if (!s_watching.running)
    {
        return;
    }
    s_watching.stop = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_watching.wake);
    cli_watch_system_stop();
    const unsigned int threads = s_watching.polling ? 2u : 1u;
    while (s_watching.ended < threads)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_watching.wake);
    }
    s_watching.running = 0;
}

void cli_watch_start(const CliWatchCfg *args)
{
    CliWatchTake take = args->take;
    void *context = args->context;
    cli_watch_stop();
    memor.cpy(&(MemoriaCfg){.dst = s_watching.root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    s_watching.take = take;
    s_watching.context = context;
    s_watching.waiting =
        (CliChanges){s_waiting_change, 0u, sizeof(s_waiting_change), s_waiting_text, 0ULL, sizeof(s_waiting_text)};
    s_watching.overflowed = 0;
    s_watching.stop = 0;
    s_watching.ended = 0u;
    s_watching.polling = 0;
    s_watching.running = 1;
    if (!EMBED_CALL(cli_root_context_internal.watch_system_start, CliRootContextCtx, .root = args->root,
                    .emit = cli_watch_change, .context = NULL))
    {
        s_watching.poller = (CliThread){cli_watch_poll, s_watching.root};
        const int polling = EMBED_CALL(cli_watch_internal.first_read_state, CliWatchCtx, .root = args->root) &&
                            EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_watching.poller);
        s_watching.polling = polling;
    }
    s_watching.batcher = (CliThread){cli_batch, NULL};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_watching.batcher))
    {
        s_watching.ended += 1u;
    }
}

void cli_watch_poll_end(void)
{
    s_watching.ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_watching.wake);
}

int cli_watch_gathered_get(const CliWatchPathsCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = s_watching.root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    // the bundle, its changes and their paths are the call's own
    CliBatch *const batch = g_cli_guard->temporary_buf_alloc(sizeof(CliBatch), g_cli_watch_owner);
    CliChange *const change =
        g_cli_guard->temporary_buf_alloc((unsigned long long)CLI_WATCH_GATHERED * sizeof(CliChange), g_cli_watch_owner);
    char *const text = g_cli_guard->temporary_buf_alloc(CLI_WATCH_TEXT_CAPACITY, g_cli_watch_owner);
    if ((batch == NULL) || (change == NULL) || (text == NULL))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_watch_owner);
        g_cli_guard->persistent_buf_release(change, g_cli_watch_owner);
        g_cli_guard->persistent_buf_release(batch, g_cli_watch_owner);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = batch, .val = 0, .bytes = sizeof(*batch)});
    batch->changes = (CliChanges){change, 0u,   (unsigned long long)CLI_WATCH_GATHERED * sizeof(CliChange),
                                  text,   0ULL, CLI_WATCH_TEXT_CAPACITY};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        batch->lost =
            batch->lost || !EMBED_CALL(cli_change_append, WatchCtx, .changes = &batch->changes, .path = args->paths[at],
                                       .type = args->types[at], .most = CLI_WATCH_GATHERED);
    }
    const int any = EMBED_CALL(cli_gather, WatchCtx, .batch = batch);
    if (any)
    {
        EMBED_CALL(args->take, CliWatchBatchCfg, .context = args->context, .folders = &batch->folders,
                   .files = &batch->written, .git = batch->git, .lost = batch->lost);
    }
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &batch->folders);
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &batch->written);
    g_cli_guard->persistent_buf_release(text, g_cli_watch_owner);
    g_cli_guard->persistent_buf_release(change, g_cli_watch_owner);
    g_cli_guard->persistent_buf_release(batch, g_cli_watch_owner);
    return any;
}
