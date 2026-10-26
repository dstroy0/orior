// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_seen.c: the view of the tree, git's where git can read it, every file it tracks or would track and every folder
// containing one, or else every file walked from the top; cached a few seconds and read again behind the window as it
// ages. A view is pinned in place by its readers: there are two, each with its holders counted atomically, and a view
// is read into only while no holder pins it and no other read fills it. A view's lists are cells of the cellblock
// counted to the files, made at the sizes the tree gives and given back as the view is read again; the walk's folders
// still to look in are such cells too
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

#include <stdatomic.h>

// how long one reading of the view stands before it is read again, in milliseconds; and the most folders still to look
// in on a walk
#define CLI_SEEN_FOR 4000ull

#define CLI_WALK_PENDING 4096u

// the folders a walk does not go into where git cannot determine: version control's own, and the one cargo writes the
// app's build to
static const char *const s_skipped[2] = {".git", "target"};

// The two views and what is cached of them: each view; its holders, raised under the lock as a reader pins it and
// lowered as it lets it go; whether a read fills it and whether it contains files; the tree the views were read for,
// which view is current, whether one is cached, and whether one is being read again behind it, of which tree and into
// which view
typedef struct
{
    CliSeen view[2];
    _Atomic unsigned int holders[2];
    int filling[2];
    int has[2];
    char root[CLI_PATH_MAX_LEN];
    unsigned int current;
    int is_cached;
    int reseeing;
    CliThread reseer;
    char reseen_root[CLI_PATH_MAX_LEN];
    unsigned int reseen_at;
} CliSeenCache;

static CliSeenCache s_cache;
static CliLock s_lock = CLI_LOCK_INIT;

// A walk of the tree's folders: those still to look in, each a cell containing its path, the folder looked in, and
// where each file found goes
typedef struct
{
    char **pending;
    unsigned int pending_count;
    const char *root;
    const char *folder;
    void (*each)(void *context, const char *path, unsigned long long length);
    void *context;
} CliWalk;

// `path` copied into a cell of its own, put among the folders still to look in
static void walk_pending(CliWalk *walk, const char *path)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    char *const cell = (walk->pending_count < CLI_WALK_PENDING) ? cli_files_cell_taken(length + 1ull) : NULL;
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = path, .bytes = (size_t)length + 1u});
        walk->pending[walk->pending_count] = cell;
        walk->pending_count += 1u;
    }
}

static void walk_each(void *context, const char *name, int is_folder)
{
    CliWalk *const walk = (CliWalk *)context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (is_folder)
    {
        const int skipped =
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_skipped[0], .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_skipped[1], .cap = CLI_CELLS_POOL}));
        if (!skipped)
        {
            walk_pending(walk, path);
        }
        return;
    }
    char relative[CLI_PATH_MAX_LEN];
    cli_relative(walk->root, path, relative);
    walk->each(walk->context, relative, cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
}

void cli_files_walked(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                      void *context)
{
    CliWalk walk = {cli_files_cell_taken(CLI_WALK_PENDING * sizeof(char *)), 0u, root, NULL, each, context};
    if (walk.pending == NULL)
    {
        return;
    }
    walk_pending(&walk, root);
    while (walk.pending_count > 0u)
    {
        walk.pending_count -= 1u;
        char *const folder = walk.pending[walk.pending_count];
        walk.folder = folder;
        query_folder_each(folder, walk_each, &walk);
        cli_files_cell_given(folder);
    }
    cli_files_cell_given(walk.pending);
}

// The paths a walk found counted, and their bytes
typedef struct
{
    unsigned long long count;
    unsigned long long bytes;
} CliCounted;

static void seen_counted(void *context, const char *path, unsigned long long length)
{
    (void)path;
    CliCounted *const counted = context;
    counted->count += 1ull;
    counted->bytes += length + 1ull;
}

static void seen_put(void *context, const char *path, unsigned long long length)
{
    (void)cli_files_put((CliFiles *)context, path, length);
}

// the files of the tree at `root` walked into the view: counted on a first walk, their list made at that size, put on
// a second, and sorted: 1 where the view contains files
static int seen_walked(const char *root, CliSeen *view)
{
    CliCounted counted = {0ull, 0ull};
    cli_files_walked(root, seen_counted, &counted);
    if ((counted.count == 0ull) || !cli_files_made(&view->files, counted.count, counted.bytes))
    {
        return 0;
    }
    cli_files_walked(root, seen_put, &view->files);
    return cli_files_sorted(&view->files) && (view->files.count != 0u);
}

// git's view of the tree at `root` read into the view, its lists made at the sizes git's paths give: 1 where git read
// the tree and lists files. A tree in a folder its repository ignores is one git lists nothing of
static int seen_git_read(const char *root, CliSeen *view)
{
    const char *words[8] = {"git"};
    unsigned int count = 1u + cli_trust_git_words(root, words + 1);
    static const char *const s_args[4] = {"ls-files", "--cached", "--others", "--exclude-standard"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        words[count] = s_args[at];
        count += 1u;
    }
    words[count] = "-z";
    words[count + 1u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(words, root, NULL, 0u, ran, &unstarted) || (ran->code != 0ll))
    {
        cli_ran_given(ran);
        return 0;
    }
    // the paths and their bytes, and every folder each runs through and theirs, counted before the lists are made
    CliCounted files = {0ull, 0ull};
    CliCounted dirs = {0ull, 0ull};
    const char *const end = ran->out + ran->out_size;
    for (const char *path = ran->out; path < end;)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        files.count += (length != 0ull) ? 1ull : 0ull;
        files.bytes += (length != 0ull) ? (length + 1ull) : 0ull;
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            dirs.count += (path[at] == '/') ? 1ull : 0ull;
            dirs.bytes += (path[at] == '/') ? (at + 1ull) : 0ull;
        }
        path += length + 1ull;
    }
    const int made = (files.count != 0ull) && cli_files_made(&view->files, files.count, files.bytes) &&
                     cli_files_made(&view->dirs, dirs.count, dirs.bytes);
    for (const char *path = ran->out; made && (path < end);)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if (length != 0ull)
        {
            cli_files_put(&view->files, path, length);
            for (unsigned long long at = 0ull; at < length; at += 1ull)
            {
                if (path[at] == '/')
                {
                    cli_files_put(&view->dirs, path, at);
                }
            }
        }
        path += length + 1ull;
    }
    cli_ran_given(ran);
    return made && cli_files_sorted(&view->files) && cli_files_sorted(&view->dirs) && (view->files.count != 0u);
}

// where the name of the file at `path`, `length` bytes, starts in it, and the bytes of its ending from its last dot
// into `*ending`, 0 where no dot stands past the name's first letter
static unsigned long long seen_name(const char *path, unsigned long long length, unsigned long long *ending)
{
    unsigned long long name = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        name = ((path[at] == '/') || (path[at] == '\\')) ? (at + 1ull) : name;
    }
    *ending = 0ull;
    for (unsigned long long at = name + 1ull; at < length; at += 1ull)
    {
        *ending = (path[at] == '.') ? (length - at) : *ending;
    }
    return name;
}

// the name and the ending of each of the view's files into its kinds, each once: put into a list made for every file's
// two, then copied into one made at the size they come to. 1, or 0 where the cellblock cannot meet them, none stored
static int seen_kinds_read(CliSeen *view)
{
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < view->files.count; at += 1u)
    {
        const char *const path = cli_files_at(&view->files, at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ull;
        const unsigned long long name = seen_name(path, length, &ending);
        count += (ending != 0ull) ? 2ull : 1ull;
        bytes += (length - name) + 1ull + ((ending != 0ull) ? (ending + 1ull) : 0ull);
    }
    // two for each of at most CLI_FILES_MOST files, which an unsigned int counts
    CliFiles every = {cli_files_cell_taken(count * sizeof(unsigned int)),
                      0u,
                      (unsigned int)count,
                      cli_files_cell_taken(bytes),
                      0ull,
                      bytes};
    for (unsigned int at = 0u; (every.start != NULL) && (every.text != NULL) && (at < view->files.count); at += 1u)
    {
        const char *const path = cli_files_at(&view->files, at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ull;
        const unsigned long long name = seen_name(path, length, &ending);
        (void)cli_files_put(&every, path + name, length - name);
        if (ending != 0ull)
        {
            (void)cli_files_put(&every, path + length - ending, ending);
        }
    }
    const int read = (every.start != NULL) && (every.text != NULL) && cli_files_sorted(&every) &&
                     cli_files_made(&view->kinds, every.count, every.used);
    for (unsigned int at = 0u; read && (at < every.count); at += 1u)
    {
        const char *const kind = cli_files_at(&every, at);
        (void)cli_files_put(&view->kinds, kind, cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
    }
    cli_files_emptied(&every);
    return read;
}

// the view of the tree at `root` read into `view`, its lists from before given back first: git's, or the files walked
// where git cannot read the tree or lists nothing of it, and the names and endings of its files. 1 where the view
// contains files. The view is the read's alone: no holder pins it and its filling is marked
static int seen_read(const char *root, CliSeen *view)
{
    cli_files_emptied(&view->files);
    cli_files_emptied(&view->dirs);
    cli_files_emptied(&view->kinds);
    view->at = cli_clock_milliseconds();
    view->git = seen_git_read(root, view);
    if (!view->git)
    {
        cli_files_emptied(&view->files);
        cli_files_emptied(&view->dirs);
    }
    // a view whose kinds the cellblock cannot meet still contains its files, and uses no tool for them
    const int has = view->git || seen_walked(root, view);
    if (has)
    {
        (void)seen_kinds_read(view);
    }
    return has;
}

// whether a view is free to be read into: no holder pins it and no read fills it. Under the lock
static int seen_free(unsigned int at)
{
    // Acquire, so the reads of a holder that let the view go are done before it is filled again
    return !s_cache.filling[at] && (atomic_load_explicit(&s_cache.holders[at], memory_order_acquire) == 0u);
}

// a view pinned: its holders raised. Under the lock, which is what prevents a view from being chosen to be filled
// between the choice of it here and the raise
static const CliSeen *seen_acquire(unsigned int at)
{
    // Acquire, so the holder reads the view as the read that filled it left it
    (void)atomic_fetch_add_explicit(&s_cache.holders[at], 1u, memory_order_acquire);
    return &s_cache.view[at];
}

// every view no holder pins and no read fills given back its lists. Under the lock
static void seen_views_emptied(void)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (seen_free(at))
        {
            cli_files_emptied(&s_cache.view[at].files);
            cli_files_emptied(&s_cache.view[at].dirs);
            cli_files_emptied(&s_cache.view[at].kinds);
            s_cache.has[at] = 0;
        }
    }
}

// the view read again behind the window into the view the pin chose, made current where the same tree is cached still
static void seen_reseen(void *context)
{
    (void)context;
    cli_lock_take(&s_lock);
    const unsigned int at = s_cache.reseen_at;
    char root[CLI_PATH_MAX_LEN];
    memor.cpy(
        &(MemoriaCfg){.dst = root,
                      .src = s_cache.reseen_root,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_cache.reseen_root, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_lock);
    // the view is this read's alone while it reads: its filling is marked and no holder pinned it as the read began
    const int has = seen_read(root, &s_cache.view[at]);
    cli_lock_take(&s_lock);
    s_cache.filling[at] = 0;
    s_cache.has[at] = has;
    if (s_cache.is_cached && (cellul.eq(&(CatenaFinitaCfg){.src = s_cache.root, .other = root, .cap = CLI_CELLS_POOL})))
    {
        s_cache.current = at;
    }
    s_cache.reseeing = 0;
    if (!s_cache.is_cached)
    {
        seen_views_emptied();
    }
    cli_lock_give(&s_lock);
}

const CliSeen *cli_seen_acquire(const char *root)
{
    cli_lock_take(&s_lock);
    const unsigned int current = s_cache.current;
    const int same =
        s_cache.is_cached && (cellul.eq(&(CatenaFinitaCfg){.src = s_cache.root, .other = root, .cap = CLI_CELLS_POOL}));
    if (same && ((cli_clock_milliseconds() - s_cache.view[current].at) < CLI_SEEN_FOR))
    {
        const CliSeen *const seen = s_cache.has[current] ? seen_acquire(current) : NULL;
        cli_lock_give(&s_lock);
        return seen;
    }
    if (same && s_cache.has[current])
    {
        const unsigned int spare = 1u - current;
        if (!s_cache.reseeing && seen_free(spare))
        {
            s_cache.reseeing = 1;
            s_cache.filling[spare] = 1;
            s_cache.reseen_at = spare;
            memor.cpy(&(MemoriaCfg){.dst = s_cache.reseen_root,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
            s_cache.reseer = (CliThread){seen_reseen, NULL, CLI_PRIORITY_IDLE};
            if (!cli_thread_started(&s_cache.reseer))
            {
                s_cache.reseeing = 0;
                s_cache.filling[spare] = 0;
            }
        }
        const CliSeen *const seen = seen_acquire(current);
        cli_lock_give(&s_lock);
        return seen;
    }
    // read now, into the current view where it is free, else the other; neither free, none is read
    const unsigned int at = seen_free(current) ? current : (seen_free(1u - current) ? (1u - current) : 2u);
    if (at == 2u)
    {
        cli_lock_give(&s_lock);
        return NULL;
    }
    s_cache.filling[at] = 1;
    s_cache.has[at] = seen_read(root, &s_cache.view[at]);
    s_cache.filling[at] = 0;
    memor.cpy(&(MemoriaCfg){.dst = s_cache.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_cache.current = at;
    s_cache.is_cached = 1;
    const CliSeen *const seen = s_cache.has[at] ? seen_acquire(at) : NULL;
    cli_lock_give(&s_lock);
    return seen;
}

void cli_seen_let(const CliSeen *seen)
{
    if (seen == NULL)
    {
        return;
    }
    const unsigned int at = (seen == &s_cache.view[1]) ? 1u : 0u;
    // Release, so whatever the holder read is done before the view is chosen to be filled again
    (void)atomic_fetch_sub_explicit(&s_cache.holders[at], 1u, memory_order_release);
}

const CliFiles *cli_files_view(const char *root)
{
    const CliSeen *const seen = cli_seen_acquire(root);
    return (seen != NULL) ? &seen->files : NULL;
}

void cli_files_let(const CliFiles *files)
{
    if (files != NULL)
    {
        const int second = (files == &s_cache.view[1].files) || (files == &s_cache.view[1].kinds);
        cli_seen_let(second ? &s_cache.view[1] : &s_cache.view[0]);
    }
}

const CliFiles *cli_files_kinds_view(const char *root)
{
    const CliSeen *const seen = cli_seen_acquire(root);
    return (seen != NULL) ? &seen->kinds : NULL;
}

// the view read behind the window, as a tree opens, for the first list or search
static void seen_warmed(void *context)
{
    cli_seen_let(cli_seen_acquire((const char *)context));
}

void cli_files_warm(const char *root)
{
    static char s_root[CLI_PATH_MAX_LEN];
    static CliThread s_warmer;
    static CliLock s_warm_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_warm_lock);
    memor.cpy(&(MemoriaCfg){
        .dst = s_root, .src = root, .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_warmer = (CliThread){seen_warmed, s_root, CLI_PRIORITY_IDLE};
    cli_thread_started(&s_warmer);
    cli_lock_give(&s_warm_lock);
}

void cli_files_forget_seen(void)
{
    cli_lock_take(&s_lock);
    s_cache.is_cached = 0;
    seen_views_emptied();
    cli_lock_give(&s_lock);
}
