// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check.c: the tree's check: whether it is on, the tree's files, the files waiting, and how many of the files
// it was given are done. Each file of a language with a server is queued for it, a server that checks its projects as
// a whole told to check them in place of that, unless the tree's own code is held back, as such a check builds and
// runs it. CHECKING threads, started once, take the files waiting in turn, and the check is told how far it has gone
// as each is done
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// the most files waiting, the bytes their paths take, and the most of the tree's files and the bytes they take
#define CLI_SERVERS_WAITING (1u << 16u)
#define CLI_SERVERS_WAITING_ROOM (32ull << 20u)
#define CLI_SERVERS_TREE_FILES (1u << 18u)
#define CLI_SERVERS_TREE_ROOM (32ull << 20u)
// the most languages one batch of files is of
#define CLI_SERVERS_BATCH_LANGUAGES 64u

// A file waiting: its path in the paths' room and its size, and its server and language
typedef struct
{
    unsigned long long path;
    unsigned int size;
    CliServersWaiting waiting;
} CliServersQueued;

// The room a check holds while it is on, a cell taken as it starts and given back as it is let go: the tree's files,
// the files waiting, and the bytes their paths take
typedef struct
{
    char files[CLI_SERVERS_TREE_ROOM];
    unsigned long long file_at[CLI_SERVERS_TREE_FILES];
    CliServersQueued waiting[CLI_SERVERS_WAITING];
    char room[CLI_SERVERS_WAITING_ROOM];
} CliServersQueueRoom;

// The tree's check: whether it is on, its tree, where what it says goes, its room, the tree's files, the files
// waiting in order from `head`, how many it was given and how many are done, and whether its threads started
typedef struct
{
    int on;
    char root[CLI_PATH_LONGEST];
    CliServersEmit emit;
    int has_emit;
    CliServersQueueRoom *held;
    char *files;
    unsigned long long files_used;
    unsigned long long *file_at;
    unsigned int file_count;
    CliServersQueued *waiting;
    unsigned int head;
    unsigned int count;
    char *room;
    unsigned long long used;
    unsigned long long total;
    unsigned long long done;
    int started;
} CliServersQueue;

// the check, under its lock, and the wake its threads wait on for a file
static CliServersQueue s_queue;
static CliLock s_queue_lock = CLI_LOCK_INIT;
static CliWake s_queue_ready = CLI_WAKE_INIT;
static unsigned int s_order[CLI_SERVERS_WAITING];
static CliThread s_checker[CLI_SERVERS_CHECKING];
static unsigned int s_checker_index[CLI_SERVERS_CHECKING];

// the check's room taken where it holds none: 1, or 0 where the cells cannot meet it; and given back, where it holds
// one. With the queue's lock held
static int check_room_held(void)
{
    if (s_queue.held == NULL)
    {
        s_queue.held = cli_cell_taken(sizeof(*s_queue.held), "the tree's check");
        if (s_queue.held == NULL)
        {
            return 0;
        }
        s_queue.files = s_queue.held->files;
        s_queue.file_at = s_queue.held->file_at;
        s_queue.waiting = s_queue.held->waiting;
        s_queue.room = s_queue.held->room;
    }
    return 1;
}

static void check_room_given(void)
{
    cli_cell_given(s_queue.held);
    s_queue.held = NULL;
    s_queue.files = NULL;
    s_queue.file_at = NULL;
    s_queue.waiting = NULL;
    s_queue.room = NULL;
}

// the place of the `at`th file waiting
static CliServersQueued *check_queued(unsigned int at)
{
    return &s_queue.waiting[(s_queue.head + at) % CLI_SERVERS_WAITING];
}

// two files waiting in the order their paths stand in the room
static int check_room_order(const void *one, const void *other)
{
    const unsigned long long left = check_queued(*(const unsigned int *)one)->path;
    const unsigned long long right = check_queued(*(const unsigned int *)other)->path;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

// the room's paths moved down together, its free bytes all after the last
static void check_room_compacted(void)
{
    for (unsigned int at = 0u; at < s_queue.count; at += 1u)
    {
        s_order[at] = at;
    }
    qsort(s_order, s_queue.count, sizeof(s_order[0]), check_room_order);
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < s_queue.count; at += 1u)
    {
        CliServersQueued *const one = check_queued(s_order[at]);
        memor.move_down(
            &(MemoriaCfg){.dst = s_queue.room + used, .src = s_queue.room + one->path, .bytes = one->size + 1u});
        one->path = used;
        used += one->size + 1u;
    }
    s_queue.used = used;
}

// a file put in the queue, at its front where `front`: 1, or 0 where it has no room
static int check_queue_put(const char *path, const CliServersWaiting *waiting, int front)
{
    const unsigned int size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    if (s_queue.held == NULL)
    {
        return 0;
    }
    if (s_queue.used + size + 1u > CLI_SERVERS_WAITING_ROOM)
    {
        check_room_compacted();
    }
    if ((s_queue.count == CLI_SERVERS_WAITING) || (s_queue.used + size + 1u > CLI_SERVERS_WAITING_ROOM))
    {
        return 0;
    }
    if (front)
    {
        s_queue.head = (s_queue.head + CLI_SERVERS_WAITING - 1u) % CLI_SERVERS_WAITING;
    }
    CliServersQueued *const one = check_queued(front ? 0u : s_queue.count);
    s_queue.count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = s_queue.room + s_queue.used, .src = path, .bytes = size + 1u});
    *one = (CliServersQueued){s_queue.used, size, *waiting};
    s_queue.used += size + 1u;
    return 1;
}

// every file waiting let go, its server's hold given back; with the queue's lock held
static void check_queue_emptied(void)
{
    for (unsigned int at = 0u; at < s_queue.count; at += 1u)
    {
        cli_lsp_given(check_queued(at)->waiting.server);
    }
    s_queue.head = 0u;
    s_queue.count = 0u;
    s_queue.used = 0ull;
}

// how far the check has gone told where it goes
static void check_told(const CliServersEmit *emit, unsigned long long done, unsigned long long total)
{
    if (emit->told == NULL)
    {
        return;
    }
    char said[128];
    CliLine line = {said, sizeof(said), 0ull};
    cli_line_text(&line, "{\"done\":");
    cli_line_decimal(&line, done, 1u);
    cli_line_text(&line, ",\"total\":");
    cli_line_decimal(&line, total, 1u);
    cli_line_character(&line, '}');
    emit->told(emit->context, "tree-check", said, line.at);
}

// a checker thread: each file waiting, once one waits and the check has somewhere to say it, checked and counted done
static void check_checker(void *context)
{
    const unsigned int checker = *(const unsigned int *)context;
    static char s_path[CLI_SERVERS_CHECKING][CLI_PATH_LONGEST];
    static CliServersEmit s_emit[CLI_SERVERS_CHECKING];
    for (;;)
    {
        cli_lock_take(&s_queue_lock);
        while ((s_queue.count == 0u) || !s_queue.has_emit)
        {
            cli_wake_wait(&s_queue_ready, &s_queue_lock);
        }
        const CliServersQueued one = *check_queued(0u);
        memor.cpy(&(MemoriaCfg){.dst = s_path[checker], .src = s_queue.room + one.path, .bytes = one.size + 1u});
        s_queue.head = (s_queue.head + 1u) % CLI_SERVERS_WAITING;
        s_queue.count -= 1u;
        s_queue.used = (s_queue.count == 0u) ? 0ull : s_queue.used;
        s_emit[checker] = s_queue.emit;
        cli_lock_give(&s_queue_lock);
        cli_servers_check_one(checker, s_path[checker], &one.waiting, &s_emit[checker]);
        cli_lsp_given(one.waiting.server);
        cli_lock_take(&s_queue_lock);
        s_queue.done += 1ull;
        const unsigned long long done = s_queue.done;
        const unsigned long long total = s_queue.total;
        const int has_emit = s_queue.has_emit;
        s_emit[checker] = s_queue.emit;
        cli_lock_give(&s_queue_lock);
        if (has_emit)
        {
            check_told(&s_emit[checker], done, total);
        }
    }
}

// the threads that check the tree's files started, once
static void check_checkers_started(void)
{
    cli_lock_take(&s_queue_lock);
    const int started = s_queue.started;
    s_queue.started = 1;
    cli_lock_give(&s_queue_lock);
    for (unsigned int at = 0u; !started && (at < CLI_SERVERS_CHECKING); at += 1u)
    {
        s_checker_index[at] = at;
        s_checker[at] = (CliThread){check_checker, &s_checker_index[at]};
        (void)cli_thread_started(&s_checker[at]);
    }
}

// the tree's files kept, those of `files` and those kept before
static void check_files_kept(const char *const *files, unsigned int count)
{
    for (unsigned int at = 0u; (s_queue.held != NULL) && (at < count); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_CELLS_POOL}) + 1u;
        if ((s_queue.file_count == CLI_SERVERS_TREE_FILES) || (s_queue.files_used + size > CLI_SERVERS_TREE_ROOM))
        {
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_queue.files + s_queue.files_used, .src = files[at], .bytes = size});
        s_queue.file_at[s_queue.file_count] = s_queue.files_used;
        s_queue.file_count += 1u;
        s_queue.files_used += size;
    }
}

unsigned int cli_servers_check_tree(const char *root, const char *const *files, unsigned int count,
                                    const CliServersEmit *emit)
{
    cli_lock_take(&s_queue_lock);
    if (!check_room_held())
    {
        cli_lock_give(&s_queue_lock);
        return 0u;
    }
    s_queue.on = 1;
    CliLine line = {s_queue.root, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, root);
    (void)cli_line_closed(&line);
    s_queue.emit = *emit;
    s_queue.has_emit = 1;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    check_files_kept(files, count);
    cli_lock_give(&s_queue_lock);
    check_checkers_started();
    cli_servers_check_tree_with_checkers(root, emit);
    cli_servers_check_tree_own(root, files, count, emit);
    return cli_servers_check_files(root, files, count, emit);
}

void cli_servers_stop_check(void)
{
    cli_lock_take(&s_queue_lock);
    check_queue_emptied();
    s_queue.total = s_queue.done;
    const int has_emit = s_queue.has_emit;
    static CliServersEmit s_emit;
    s_emit = s_queue.emit;
    const unsigned long long done = s_queue.done;
    cli_lock_give(&s_queue_lock);
    if (has_emit)
    {
        check_told(&s_emit, done, done);
    }
}

int cli_servers_check_on(void)
{
    cli_lock_take(&s_queue_lock);
    const int on = s_queue.on;
    cli_lock_give(&s_queue_lock);
    return on;
}

int cli_servers_checking(void)
{
    cli_lock_take(&s_queue_lock);
    const int checking = s_queue.on && (s_queue.done < s_queue.total);
    cli_lock_give(&s_queue_lock);
    return checking;
}

void cli_servers_check_set(int on, const CliServersEmit *emit, unsigned long long done, unsigned long long total)
{
    cli_lock_take(&s_queue_lock);
    s_queue.on = on;
    s_queue.has_emit = emit != NULL;
    if (emit != NULL)
    {
        s_queue.emit = *emit;
    }
    s_queue.done = done;
    s_queue.total = total;
    cli_lock_give(&s_queue_lock);
}

void cli_servers_check_count(int *on, unsigned int *waiting, unsigned long long *done, unsigned long long *total)
{
    cli_lock_take(&s_queue_lock);
    *on = s_queue.on;
    *waiting = s_queue.count;
    *done = s_queue.done;
    *total = s_queue.total;
    cli_lock_give(&s_queue_lock);
}

int cli_servers_check_takes(const char *path, CliLsp *lsp)
{
    cli_lock_take(&s_queue_lock);
    int took = s_queue.on;
    if (took)
    {
        CliServersWaiting waiting = {lsp, ""};
        cli_lsp_held(lsp);
        took = check_queue_put(path, &waiting, 1);
        if (took)
        {
            char uri[CLI_PATH_LONGEST];
            cli_servers_uri_of(path, uri);
            cli_servers_hold(uri, 1);
            s_queue.total += 1ull;
            cli_wake_all(&s_queue_ready);
        }
        else
        {
            cli_lsp_given(lsp);
        }
    }
    cli_lock_give(&s_queue_lock);
    return took;
}

void cli_servers_check_let_go(void)
{
    cli_lock_take(&s_queue_lock);
    check_queue_emptied();
    s_queue.on = 0;
    s_queue.root[0] = '\0';
    s_queue.has_emit = 0;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    s_queue.total = 0ull;
    s_queue.done = 0ull;
    check_room_given();
    cli_lock_give(&s_queue_lock);
    cli_lock_take(&g_cli_servers_held_lock);
    cli_servers_held_cleared();
    cli_lock_give(&g_cli_servers_held_lock);
}

int cli_servers_check_tree_files(const char *root, void (*each)(void *context, const char *file), void *context)
{
    cli_lock_take(&s_queue_lock);
    const int on = s_queue.on && (s_queue.held != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = root, .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; on && (at < s_queue.file_count); at += 1u)
    {
        each(context, s_queue.files + s_queue.file_at[at]);
    }
    cli_lock_give(&s_queue_lock);
    return on;
}

int cli_servers_check_files_changed(const char *root, const char *const *files, unsigned int count,
                                    CliServersEmit *emit)
{
    cli_lock_take(&s_queue_lock);
    const int on = s_queue.on && s_queue.has_emit && (s_queue.held != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = root, .cap = CLI_CELLS_POOL}));
    if (on)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            unsigned int kept = 0u;
            while ((kept < s_queue.file_count) &&
                   (!cellul.eq(&(CatenaFinitaCfg){
                       .src = s_queue.files + s_queue.file_at[kept], .other = files[at], .cap = CLI_CELLS_POOL})))
            {
                kept += 1u;
            }
            if (kept == s_queue.file_count)
            {
                check_files_kept(&files[at], 1u);
            }
        }
        unsigned int kept = 0u;
        unsigned long long used = 0ull;
        for (unsigned int at = 0u; at < s_queue.file_count; at += 1u)
        {
            char path[CLI_PATH_LONGEST];
            cli_full(root, s_queue.files + s_queue.file_at[at], path);
            if (query_is_file(path))
            {
                const size_t size =
                    cellul.len(&(CatenaFinitaCfg){.src = s_queue.files + s_queue.file_at[at], .cap = CLI_CELLS_POOL}) +
                    1u;
                memor.move_down(&(MemoriaCfg){
                    .dst = s_queue.files + used, .src = s_queue.files + s_queue.file_at[at], .bytes = size});
                s_queue.file_at[kept] = used;
                used += size;
                kept += 1u;
            }
        }
        s_queue.file_count = kept;
        s_queue.files_used = used;
        *emit = s_queue.emit;
    }
    cli_lock_give(&s_queue_lock);
    return on;
}

unsigned int cli_servers_check_queued(const char *const *paths, const CliServersWaiting *waiting, unsigned int count,
                                      const CliServersEmit *emit)
{
    cli_lock_take(&s_queue_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        int queued = 0;
        for (unsigned int one = 0u; !queued && (one < s_queue.count); one += 1u)
        {
            queued = (cellul.eq(&(CatenaFinitaCfg){
                .src = s_queue.room + check_queued(one)->path, .other = paths[at], .cap = CLI_CELLS_POOL}));
        }
        if (!queued && check_queue_put(paths[at], &waiting[at], 0))
        {
            s_queue.total += 1ull;
            continue;
        }
        cli_lsp_given(waiting[at].server);
    }
    const int idle = (s_queue.count == 0u) && (s_queue.done >= s_queue.total);
    const unsigned long long done = s_queue.done;
    const unsigned long long total = s_queue.total;
    if (!idle)
    {
        cli_wake_all(&s_queue_ready);
    }
    cli_lock_give(&s_queue_lock);
    if (idle)
    {
        check_told(emit, done, total);
    }
    return count;
}
