// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// folder_watch.c: the folders the program keeps as its own files, each one open from the program's start to its end.
// A change under a folder calls its `changed` on the watch's own thread once the changes rest
// CLI_FOLDER_WATCH_QUIET_MS. Windows reports the changes through a change notice on the folder. Linux reports them
// through inotify on the folder and on each folder under it. Every other system has the watch read the folder every
// CLI_FOLDER_WATCH_READ_MS
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

// the max folders the program keeps open; how long the changes rest before the call; how often a system without a
// change notice reads the folder; the max depth of the folders under a folder that the watch goes into
#define CLI_FOLDER_WATCHES 4u
#define CLI_FOLDER_WATCH_QUIET_MS 150u
#define CLI_FOLDER_WATCH_READ_MS 2000u
#define CLI_FOLDER_WATCH_DEPTH 8u

// One folder kept open: its path, the call on a change, the thread, whether it runs, whether it must stop and whether
// its thread ended, the wake the stop and the end go through, and the system's parts of the watch
typedef struct
{
    char folder[CLI_PATH_MAX_LEN];
    void (*changed)(void);
    CliThread thread;
    int running;
    int stop;
    int ended;
    CliWake wake;
#if defined(_WIN32)
    HANDLE notified;
    HANDLE stopped;
#elif defined(__linux__)
    int inotify;
    int stopped;
#else
    unsigned long long stamp;
#endif
} CliFolderWatch;

static CliFolderWatch s_watches[CLI_FOLDER_WATCHES];

// A walk of a folder's tree: the watch, the path of the entry the walk is at with its length, the depth, and the fold
// of each file's size and time
typedef struct
{
    CliFolderWatch *watch;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long length;
    unsigned int depth;
    unsigned long long stamp;
} FolderWatchWalk;

// the folder at the walk's path put under the watch, on Linux alone
static void folder_watch_folder_put(FolderWatchWalk *walk)
{
#if defined(__linux__)
    (void)inotify_add_watch(walk->watch->inotify, walk->path,
                            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DONT_FOLLOW);
#else
    (void)walk;
#endif
}

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
        folder_watch_folder_put(walk);
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

// the folder of `watch` and all under it walked: each folder put under the watch, and the stamp of its files
static unsigned long long folder_watch_walked(CliFolderWatch *watch)
{
    FolderWatchWalk walk = {.watch = watch, .depth = 0u, .stamp = 1469598103934665603ULL};
    VerbaLinea line = {walk.path, sizeof(walk.path), 0ULL};
    mmgr_line_put(&line, watch->folder);
    walk.length = mmgr_line_finish(&line);
    folder_watch_folder_put(&walk);
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = walk.path,
                     .each = folder_watch_entry, .context_bytess = &walk);
    return walk.stamp;
}

// the thread of `watch` ended, and its stopper woken
static void folder_watch_ended(CliFolderWatch *watch)
{
    watch->ended = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &watch->wake);
}

#if defined(_WIN32)
// Each notice waited for, then each notice after it until CLI_FOLDER_WATCH_QUIET_MS pass with none, then the call.
// The stop ends the thread
static void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    const HANDLE waits[2] = {watch->notified, watch->stopped};
    while (WaitForMultipleObjects(2u, waits, FALSE, INFINITE) == WAIT_OBJECT_0)
    {
        DWORD waited = WAIT_OBJECT_0;
        while ((waited == WAIT_OBJECT_0) && FindNextChangeNotification(watch->notified))
        {
            waited = WaitForMultipleObjects(2u, waits, FALSE, CLI_FOLDER_WATCH_QUIET_MS);
        }
        if (waited != WAIT_TIMEOUT)
        {
            break;
        }
        watch->changed();
    }
    FindCloseChangeNotification(watch->notified);
    folder_watch_ended(watch);
}

// the system's notice on the folder of `watch` and the stop's event made: 1, or 0 where the system refuses
static int folder_watch_opened(CliFolderWatch *watch)
{
    wchar_t wide[CLI_PATH_MAX_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, watch->folder, -1, wide, CLI_PATH_MAX_LEN) == 0)
    {
        return 0;
    }
    watch->notified = FindFirstChangeNotificationW(wide, TRUE,
                                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                                       FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (watch->notified == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    watch->stopped = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (watch->stopped == NULL)
    {
        FindCloseChangeNotification(watch->notified);
        return 0;
    }
    return 1;
}

// the stop of `watch` given to its thread
static void folder_watch_stop_sent(CliFolderWatch *watch)
{
    SetEvent(watch->stopped);
}

// the stop's event of `watch` closed after its thread ends
static void folder_watch_closed(CliFolderWatch *watch)
{
    CloseHandle(watch->stopped);
}
#elif defined(__linux__)
// a wait of `timeout_ms` on the inotify of `watch` and on its stop, -1 for no end: 1 where changes came, each read
// off the inotify; 0 where the time ran out; -1 where the stop came or the wait failed
static int folder_watch_waited(CliFolderWatch *watch, int timeout_ms)
{
    struct pollfd waits[2] = {{watch->inotify, POLLIN, 0}, {watch->stopped, POLLIN, 0}};
    const int ready = poll(waits, 2u, timeout_ms);
    if ((ready < 0) || ((waits[1].revents & POLLIN) != 0))
    {
        return -1;
    }
    if (ready == 0)
    {
        return 0;
    }
    char answer[4096];
    while (read(watch->inotify, answer, sizeof(answer)) > 0)
    {
    }
    return 1;
}

// Each change waited for, then each change after it until CLI_FOLDER_WATCH_QUIET_MS pass with none. A folder made
// since the last walk goes under the watch, and then the call. The stop ends the thread
static void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    while (folder_watch_waited(watch, -1) == 1)
    {
        int waited = 1;
        while (waited == 1)
        {
            waited = folder_watch_waited(watch, (int)CLI_FOLDER_WATCH_QUIET_MS);
        }
        if (waited != 0)
        {
            break;
        }
        (void)folder_watch_walked(watch);
        watch->changed();
    }
    close(watch->inotify);
    folder_watch_ended(watch);
}

// the inotify on the folder of `watch` and each folder under it, and the stop's event, made: 1, or 0 where the system
// refuses
static int folder_watch_opened(CliFolderWatch *watch)
{
    watch->inotify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (watch->inotify < 0)
    {
        return 0;
    }
    watch->stopped = eventfd(0u, EFD_CLOEXEC);
    if (watch->stopped < 0)
    {
        close(watch->inotify);
        return 0;
    }
    (void)folder_watch_walked(watch);
    return 1;
}

static void folder_watch_stop_sent(CliFolderWatch *watch)
{
    const unsigned long long one = 1ULL;
    (void)write(watch->stopped, &one, sizeof(one));
}

static void folder_watch_closed(CliFolderWatch *watch)
{
    close(watch->stopped);
}
#else
// The folder read every CLI_FOLDER_WATCH_READ_MS, and the call where the stamp of its files changed since the last
// reading. The stop ends the thread
static void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &watch->wake);
        if (watch->stop)
        {
            break;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &watch->wake, .seen = seen,
                         .milliseconds_u64 = CLI_FOLDER_WATCH_READ_MS);
        if (watch->stop)
        {
            break;
        }
        const unsigned long long stamp = folder_watch_walked(watch);
        if (stamp != watch->stamp)
        {
            watch->stamp = stamp;
            watch->changed();
        }
    }
    folder_watch_ended(watch);
}

// the first stamp of the folder of `watch`: 1
static int folder_watch_opened(CliFolderWatch *watch)
{
    watch->stamp = folder_watch_walked(watch);
    return 1;
}

static void folder_watch_stop_sent(CliFolderWatch *watch)
{
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &watch->wake);
}

static void folder_watch_closed(CliFolderWatch *watch)
{
    (void)watch;
}
#endif

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
    watch->thread = (CliThread){folder_watch_run, watch, CLI_PRIORITY_WORK};
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
