// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_watch.c: the tree's changes as Linux reports them, through inotify: a watch on each folder of the tree git does
// not ignore, each folder known by its closest parent and its own name alone, on a thread of the watch's own that waits
// on the system's answer and on its stop together. Git's own folder is watched for its index, HEAD, refs and rules and
// no further. A folder come in is walked by itself and each file found in it reported. Where the system refuses, and
// on every other system, the tree is read every two seconds instead (watch.c)
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../editor/files.h"
#include "../text/text.h"

#if defined(__linux__)
#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

// the most watches one inotify gives out before it is opened anew, as its numbers only rise; the bytes of their names;
// the bytes of the system's answer; and what is watched
#define CLI_INOTIFY_FOLDERS (1u << 16u)
#define CLI_INOTIFY_NAMES (1u << 20u)
#define CLI_INOTIFY_ANSWER (64u * 1024u)
#define CLI_INOTIFY_MASK                                                                                               \
    (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY | IN_CLOSE_WRITE | IN_ONLYDIR | IN_DONT_FOLLOW)

// One folder watched, under the number the system gave its watch: its closest parent's number, 0 at the tree's top, and
// where its name starts among the names
typedef struct
{
    unsigned int parent;
    unsigned int name;
    int used;
} CliInotifyFolder;

// The watch: the tree's folder, the system's inotify and the stop, the folders under their numbers and their names, who
// is notified, the thread, and whether it runs and has ended
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int inotify;
    int stopped;
    CliInotifyFolder *folder;
    unsigned int folder_most;
    char *names;
    unsigned int names_used;
    int refused;
    CliChangeFn emit;
    void *context;
    CliThread thread;
    int running;
    int ended;
    CliLock lock;
    CliWake wake;
} CliInotify;

static CliInotify s_watch = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};

typedef struct
{
#if (defined(__linux__))
    unsigned int at;
    char *path_out;
    unsigned int parent;
    const char *name;
    const char *path;
    unsigned int first;
    int reported;
    const struct inotify_event *record;
#endif
} PosixWatchCtx;
// the path from the tree's top of the folder watched under `at` into `path`, sized for CLI_PATH_MAX_LEN, the top's
// empty
EMBED_INLINE void inotify_path(const PosixWatchCtx *args)
{
    unsigned int chain[CLI_PATH_MAX_LEN / 2u];
    unsigned int depth = 0u;
    for (unsigned int up = args->at;
         (up != 0u) && (s_watch.folder[up].parent != 0u) && (depth < (CLI_PATH_MAX_LEN / 2u));
         up = s_watch.folder[up].parent)
    {
        chain[depth] = up;
        depth += 1u;
    }
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ull};
    while (depth > 0u)
    {
        depth -= 1u;
        mmgr_line_put(&line, s_watch.names + s_watch.folder[chain[depth]].name);
        mmgr_line_put(&line, (depth != 0u) ? "/" : "");
    }
    (void)mmgr_line_finish(&line);
}

// the folder `name` under the folder `parent`, `path` from the tree's top, watched: its number, or 0 where it is gone
// or the system refuses; `refused` raised where this inotify's numbers or names are spent, or the system's watches are
EMBED_INLINE unsigned int inotify_watch(const PosixWatchCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    if (args->path[0] != '\0')
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_watch.root, .relative = args->path, .joined = full);
    }
    else
    {
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = full, .src = s_watch.root, .cap = CLI_PATH_MAX_LEN});
    }
    const int number = inotify_add_watch(s_watch.inotify, full, CLI_INOTIFY_MASK);
    if (number < 0)
    {
        s_watch.refused = s_watch.refused || (errno == ENOSPC);
        return 0u;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN}) + 1ull;
    // a watch's number is not negative, which the test above holds
    const unsigned int at = (unsigned int)number;
    if ((at >= s_watch.folder_most) || ((s_watch.names_used + length) > CLI_INOTIFY_NAMES))
    {
        s_watch.refused = 1;
        return 0u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_watch.names + s_watch.names_used, .src = args->name, .bytes = (size_t)length});
    s_watch.folder[at] = (CliInotifyFolder){args->parent, s_watch.names_used, 1};
    // the names' bytes are bounded by CLI_INOTIFY_NAMES, which an unsigned int counts
    s_watch.names_used += (unsigned int)length;
    return at;
}

// whether the watch goes into the folder at `path` from the tree's top: git's own for its index, HEAD, refs and rules
// alone; otherwise one a walk goes into that git does not ignore
EMBED_INLINE int inotify_goes_into(const PosixWatchCtx *args)
{
    const char *path = args->path;
    if (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git", .cap = CLI_PATH_MAX_LEN}) ||
        cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/info", .cap = CLI_PATH_MAX_LEN}) ||
        cellul.eq(&(CatenaFinitaCfg){.src = path, .other = ".git/refs", .cap = CLI_PATH_MAX_LEN}) ||
        cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ".git/refs/", .cap = CLI_PATH_MAX_LEN}))
    {
        return 1;
    }
    const int git = cli_tree_git();
    if (EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .path = path, .git = git))
    {
        return 0;
    }
    if (!git || EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = path))
    {
        return 1;
    }
    // a folder the tree does not contain: an empty one, one made since it was listed, or one git ignores
    char inside[CLI_PATH_MAX_LEN];
    VerbaLinea line = {inside, sizeof(inside), 0ull};
    mmgr_line_put(&line, path);
    mmgr_line_put(&line, "/.");
    return (mmgr_line_finish(&line) != 0ull) &&
           EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_watch.root, .path = inside);
}

// A walk of folders: those still to look in, each by its number, the folder looked in, and whether each file found is
// reported, as it is of a folder come in after the tree was listed
typedef struct
{
    unsigned int *pending;
    unsigned int pending_count;
    unsigned int at;
    const char *path;
    int reported;
} CliInotifyWalk;

static void inotify_walk_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliInotifyWalk *const walk = (CliInotifyWalk *)args->context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, walk->path);
    mmgr_line_put(&line, (walk->path[0] != '\0') ? "/" : "");
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (args->is_folder && EMBED_CALL(inotify_goes_into, PosixWatchCtx, .path = path))
    {
        const unsigned int at =
            EMBED_CALL(inotify_watch, PosixWatchCtx, .parent = walk->at, .name = name, .path = path);
        if ((at != 0u) && (walk->pending_count < CLI_INOTIFY_FOLDERS))
        {
            walk->pending[walk->pending_count] = at;
            walk->pending_count += 1u;
        }
    }
    if (walk->reported)
    {
        EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = path,
                   .kind = CLI_CHANGE_ADDED);
    }
}

// the folder watched under `first` walked, each folder under it watched, each file reported where `reported` is set
EMBED_INLINE void inotify_walk(const PosixWatchCtx *args)
{
    CliInotifyWalk walk = {
        g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_INOTIFY_FOLDERS * sizeof(unsigned int),
                                         &g_cli_watch_folders_cells),
        0u, 0u, NULL, args->reported};
    if (walk.pending == NULL)
    {
        return;
    }
    walk.pending[0] = args->first;
    walk.pending_count = 1u;
    while ((walk.pending_count > 0u) && !s_watch.refused)
    {
        walk.pending_count -= 1u;
        walk.at = walk.pending[walk.pending_count];
        char path[CLI_PATH_MAX_LEN];
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(inotify_path, PosixWatchCtx, .at = walk.at, .path_out = path);
        if (path[0] != '\0')
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_watch.root, .relative = path, .joined = full);
        }
        else
        {
            (void)cellul.copy(&(CatenaFinitaCfg){.dst = full, .src = s_watch.root, .cap = CLI_PATH_MAX_LEN});
        }
        walk.path = path;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = full,
                   .each = inotify_walk_each, .context_bytess = &walk);
    }
    g_cli_cells->persistent_buf_release(walk.pending, &g_cli_watch_folders_cells);
}

// an inotify opened, every folder of the tree watched: 1, or 0 where the system refuses, none left open
static int inotify_open(void)
{
    s_watch.inotify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (s_watch.inotify < 0)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){
        .dst = s_watch.folder, .val = 0, .bytes = (size_t)s_watch.folder_most * sizeof(CliInotifyFolder)});
    s_watch.names_used = 1u;
    s_watch.names[0] = '\0';
    s_watch.refused = 0;
    const unsigned int top = EMBED_CALL(inotify_watch, PosixWatchCtx, .parent = 0u, .name = "", .path = "");
    if (top != 0u)
    {
        EMBED_CALL(inotify_walk, PosixWatchCtx, .first = top, .reported = 0);
    }
    if ((top == 0u) || s_watch.refused)
    {
        (void)close(s_watch.inotify);
        s_watch.inotify = -1;
        return 0;
    }
    return 1;
}

// the watches on the folder `name` under `parent`, moved out of the tree, and every folder under it taken off
static void inotify_take_off(const PosixWatchCtx *args)
{
    for (unsigned int at = 1u; at < s_watch.folder_most; at += 1u)
    {
        const CliInotifyFolder *const one = &s_watch.folder[at];
        if (!one->used || (one->parent != args->parent) ||
            ((args->name != NULL) &&
             !cellul.eq(
                 &(CatenaFinitaCfg){.src = s_watch.names + one->name, .other = args->name, .cap = CLI_NAME_MAX_LEN})))
        {
            continue;
        }
        EMBED_CALL(inotify_take_off, PosixWatchCtx, .parent = at, .name = NULL);
        // a number in the table came from the system, and it is an int there
        (void)inotify_rm_watch(s_watch.inotify, (int)at);
        s_watch.folder[at].used = 0;
    }
}

// one record of the system's answer reported by its path from the tree's top and what happened to it
EMBED_INLINE void inotify_record_emit(const PosixWatchCtx *args)
{
    if ((args->record->mask & IN_Q_OVERFLOW) != 0u)
    {
        EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = "", .kind = CLI_CHANGE_LOST);
        return;
    }
    // a number from the system is not negative
    const unsigned int at = (unsigned int)args->record->wd;
    if ((at == 0u) || (at >= s_watch.folder_most) || !s_watch.folder[at].used)
    {
        return;
    }
    if ((args->record->mask & IN_IGNORED) != 0u)
    {
        s_watch.folder[at].used = 0;
        return;
    }
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(inotify_path, PosixWatchCtx, .at = at, .path_out = folder);
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, (folder[0] != '\0') ? "/" : "");
    mmgr_line_put(&line, args->record->name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    const int is_folder = (args->record->mask & IN_ISDIR) != 0u;
    if ((args->record->mask & (IN_CREATE | IN_MOVED_TO)) != 0u)
    {
        EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = path,
                   .kind = CLI_CHANGE_ADDED);
        if (is_folder && EMBED_CALL(inotify_goes_into, PosixWatchCtx, .path = path))
        {
            const unsigned int come_in =
                EMBED_CALL(inotify_watch, PosixWatchCtx, .parent = at, .name = args->record->name, .path = path);
            if (come_in != 0u)
            {
                EMBED_CALL(inotify_walk, PosixWatchCtx, .first = come_in, .reported = 1);
            }
        }
    }
    else if ((args->record->mask & (IN_DELETE | IN_MOVED_FROM)) != 0u)
    {
        if (is_folder && ((args->record->mask & IN_MOVED_FROM) != 0u))
        {
            EMBED_CALL(inotify_take_off, PosixWatchCtx, .parent = at, .name = args->record->name);
        }
        EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = path,
                   .kind = CLI_CHANGE_REMOVED);
    }
    else if (!is_folder)
    {
        EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = path,
                   .kind = CLI_CHANGE_MODIFIED);
    }
}

// the system's answers read and each record reported, until the stop is set; this inotify's numbers or names spent, a
// new one is opened and the changes between the two are lost
static void inotify_read(void *context)
{
    (void)context;
    static _Alignas(struct inotify_event) unsigned char s_answer[CLI_INOTIFY_ANSWER];
    int going = 1;
    while (going)
    {
        struct pollfd waits[2] = {{s_watch.inotify, POLLIN, 0}, {s_watch.stopped, POLLIN, 0}};
        going = (poll(waits, 2u, -1) >= 0) || (errno == EINTR);
        going = going && ((waits[1].revents & POLLIN) == 0);
        const long written =
            (going && ((waits[0].revents & POLLIN) != 0)) ? read(s_watch.inotify, s_answer, sizeof(s_answer)) : 0;
        for (long at = 0; at < written;)
        {
            // the system lays each record on its own alignment, which the answer's room starts on
            const struct inotify_event *const record = (const struct inotify_event *)(const void *)(s_answer + at);
            EMBED_CALL(inotify_record_emit, PosixWatchCtx, .record = record);
            at += (long)(sizeof(*record) + record->len);
        }
        if (going && s_watch.refused)
        {
            (void)close(s_watch.inotify);
            s_watch.inotify = -1;
            going = inotify_open();
            EMBED_CALL(s_watch.emit, CliEachPathKindCfg, .context = s_watch.context, .path = "",
                       .kind = CLI_CHANGE_LOST);
        }
    }
    if (s_watch.inotify >= 0)
    {
        (void)close(s_watch.inotify);
        s_watch.inotify = -1;
    }
    (void)close(s_watch.stopped);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_watch.lock);
    s_watch.ended = 1;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_watch.wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_watch.lock);
}

// the watch's cells given back
static void inotify_free(void)
{
    g_cli_cells->persistent_buf_release(s_watch.folder, &g_cli_watch_folders_cells);
    g_cli_cells->persistent_buf_release(s_watch.names, &g_cli_watch_folders_cells);
    s_watch.folder = NULL;
    s_watch.names = NULL;
}

int cli_watch_system_start(const CliRootContextCtx *args)
{
    const char *root = args->root;
    CliChangeFn emit = args->emit;
    void *context = args->context;
    cli_watch_system_stop();
    if (!cellul.copy(&(CatenaFinitaCfg){.dst = s_watch.root, .src = root, .cap = CLI_PATH_MAX_LEN}))
    {
        return 0;
    }
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    s_watch.folder_most = CLI_INOTIFY_FOLDERS;
    s_watch.folder = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)CLI_INOTIFY_FOLDERS * sizeof(CliInotifyFolder), &g_cli_watch_folders_cells);
    s_watch.names = g_cli_cells->persistent_buf_alloc(CLI_INOTIFY_NAMES, &g_cli_watch_folders_cells);
    s_watch.emit = emit;
    s_watch.context = context;
    s_watch.inotify = -1;
    s_watch.stopped = eventfd(0u, EFD_CLOEXEC);
    if ((s_watch.folder == NULL) || (s_watch.names == NULL) || (s_watch.stopped < 0) || !inotify_open())
    {
        if (s_watch.stopped >= 0)
        {
            (void)close(s_watch.stopped);
        }
        inotify_free();
        return 0;
    }
    s_watch.ended = 0;
    s_watch.thread = (CliThread){inotify_read, NULL};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_watch.thread))
    {
        (void)close(s_watch.inotify);
        (void)close(s_watch.stopped);
        inotify_free();
        return 0;
    }
    s_watch.running = 1;
    return 1;
}

void cli_watch_system_stop(void)
{
    if (!s_watch.running)
    {
        return;
    }
    const unsigned long long one = 1ull;
    (void)write(s_watch.stopped, &one, sizeof(one));
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_watch.lock);
    while (!s_watch.ended)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_watch.wake, .lock = &s_watch.lock);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_watch.lock);
    inotify_free();
    s_watch.running = 0;
}
#elif !defined(_WIN32)
int cli_watch_system_start(const CliRootContextCtx *args)
{
    (void)args;
    return 0;
}

void cli_watch_system_stop(void)
{
}
#endif
