// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// folder_watch.c: the folders the program keeps as its own files, each one open from the program's start to its end.
// A change under a folder calls its `changed` on the watch's own thread once the changes rest
// CLI_FOLDER_WATCH_QUIET_MS. How the changes come is the system's (folder_watch.h)
#include "folder_watch.h"

// the max folders the program keeps open
#define CLI_FOLDER_WATCHES 4u

static CliFolderWatch s_watches[CLI_FOLDER_WATCHES];

// A walk of a folder's tree. It holds the watch, the path of the entry the walk is at with its length, and the depth.
// It also holds the fold of each file's size and time
typedef struct
{
    CliFolderWatch *watch;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long length;
    unsigned int depth;
    unsigned long long stamp;
} FolderWatchWalk;

// the file at the walk's path folded into the walk's stamp, its size and its time with the length of its path
static void folder_watch_file_put(FolderWatchWalk *walk)
{
    unsigned long long size = 0ULL;
    unsigned long long changed = 0ULL;
    (void)EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = walk->path, .size_pointer = &size,
                     .changed = &changed);
    walk->stamp = (walk->stamp ^ changed ^ (size << 20u) ^ walk->length) * 1099511628211ULL;
}

// one entry of a folder put after the walk's path: a folder put under the watch and walked into, a file folded in
static void folder_watch_entry(const CliEachNameIsFolderCfg *args)
{
    FolderWatchWalk *const walk = (FolderWatchWalk *)args->context;
    const unsigned long long at = walk->length;
    VerbaLinea line = {walk->path, sizeof(walk->path), at};
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    const unsigned long long length = mmgr_line_finish(&line);
    if ((length != 0ULL) && args->is_folder && (walk->depth < CLI_FOLDER_WATCH_DEPTH))
    {
        folder_watch_folder_put(walk->watch, walk->path);
        walk->length = length;
        walk->depth += 1u;
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = walk->path,
                         .each = folder_watch_entry, .context_bytess = walk);
        walk->depth -= 1u;
    }
    else if ((length != 0ULL) && !args->is_folder)
    {
        folder_watch_file_put(walk);
    }
    walk->length = at;
    walk->path[at] = '\0';
}

unsigned long long folder_watch_walked(CliFolderWatch *watch)
{
    FolderWatchWalk walk = {.watch = watch, .depth = 0u, .stamp = 1469598103934665603ULL};
    VerbaLinea line = {walk.path, sizeof(walk.path), 0ULL};
    mmgr_line_put(&line, watch->folder);
    walk.length = mmgr_line_finish(&line);
    folder_watch_folder_put(watch, walk.path);
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = walk.path,
                     .each = folder_watch_entry, .context_bytess = &walk);
    return walk.stamp;
}

void folder_watch_ended(CliFolderWatch *watch)
{
    watch->ended = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &watch->wake);
}

int cli_folder_watch_start(const CliFolderWatchCtx *args)
{
    unsigned int at = 0u;
    while ((at < CLI_FOLDER_WATCHES) && s_watches[at].running)
    {
        at += 1u;
    }
    if (at == CLI_FOLDER_WATCHES)
    {
        return 0;
    }
    CliFolderWatch *const watch = &s_watches[at];
    VerbaLinea line = {watch->folder, sizeof(watch->folder), 0ULL};
    mmgr_line_put(&line, args->folder);
    if ((mmgr_line_finish(&line) == 0ULL) || !folder_watch_opened(watch))
    {
        return 0;
    }
    watch->changed = args->changed;
    watch->stop = 0;
    watch->ended = 0;
    watch->thread = (CliThread){folder_watch_run, watch, PLATFORM_PRIORITY_WORK};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &watch->thread))
    {
        // the thread's loop runs here with the stop sent first: it ends at once and closes the system's parts
        watch->stop = 1;
        folder_watch_stop_sent(watch);
        folder_watch_run(watch);
        folder_watch_closed(watch);
        return 0;
    }
    watch->running = 1;
    return 1;
}

void cli_folder_watch_stop(void)
{
    for (unsigned int at = 0u; at < CLI_FOLDER_WATCHES; at += 1u)
    {
        CliFolderWatch *const watch = &s_watches[at];
        if (!watch->running)
        {
            continue;
        }
        watch->stop = 1;
        folder_watch_stop_sent(watch);
        unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &watch->wake);
        while (!watch->ended)
        {
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &watch->wake, .seen = seen);
            seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &watch->wake);
        }
        folder_watch_closed(watch);
        watch->running = 0;
    }
}
