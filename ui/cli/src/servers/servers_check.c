// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check.c: the tree's check: whether it is on, the tree's files, the files waiting, and how many of the files
// it was given are done. Each file of a language with a server is queued for it, a server that checks its projects as
// a whole instructed to check them in place of that, unless the tree's own code is disabled, as such a check builds and
// runs it. CHECKING threads, started once, take the files waiting in turn, and the check is notified how far it has
// gone as each is done
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "servers_internal.h"

// the most languages one batch of files is of
#define CLI_SERVERS_BATCH_LANGUAGES 64u

// the check, under its lock, and the wake its threads wait on for a file
static CliServersQueue s_queue;
static CliLock s_queue_lock = CLI_LOCK_INIT;
static CliWake s_queue_ready = CLI_WAKE_INIT;
static CliThread s_checker[CLI_SERVERS_CHECKING];
static unsigned int s_checker_index[CLI_SERVERS_CHECKING];

// every file waiting let go, its server's reference given back; with the queue's lock acquired
static void check_queue_clear(void)
{
    for (unsigned int at = 0u; at < s_queue.count; at += 1u)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg,
                   .lsp = EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                     .queue_read = &s_queue, .at = at)
                              ->server);
    }
    s_queue.head = 0u;
    s_queue.count = 0u;
    s_queue.used = 0ull;
}

typedef struct
{
    const CliServersSink *sink;
    unsigned long long done;
    unsigned long long total;
} ServersCheckCtx;
// how far the check has gone reported where it goes
EMBED_INLINE void check_progress_emit(const ServersCheckCtx *args)
{
    if (args->sink->emit == NULL)
    {
        return;
    }
    char progress_buf[128];
    VerbaLinea line = {progress_buf, sizeof(progress_buf), 0ull};
    mmgr_line_put(&line, "{\"done\":");
    mmgr_line_decimal(&line, args->done, 1u);
    mmgr_line_put(&line, ",\"total\":");
    mmgr_line_decimal(&line, args->total, 1u);
    mmgr_line_ch(&line, '}');
    EMBED_CALL(args->sink->emit, CliEachEventJsonSizeCfg, .context = args->sink->context, .event = "tree-check",
               .json = progress_buf, .size = line.at);
}

// a checker thread: each file waiting, once one waits and the check has somewhere to report it, checked and counted
// done
static void check_checker(void *context)
{
    const unsigned int checker = *(const unsigned int *)context;
    static char s_path[CLI_SERVERS_CHECKING][CLI_PATH_MAX_LEN];
    static CliServersSink s_emit[CLI_SERVERS_CHECKING];
    for (;;)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
        while ((s_queue.count == 0u) || !s_queue.has_sink)
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_queue_ready, .lock = &s_queue_lock);
        }
        const CliServersQueued one = *EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                                 .queue_read = &s_queue, .at = 0u);
        memor.cpy(&(MemoriaCfg){.dst = s_path[checker], .src = s_queue.room + one.path, .bytes = one.size + 1u});
        CliServersWaiting waiting = {one.server, ""};
        memor.cpy(&(MemoriaCfg){
            .dst = waiting.id, .src = s_queue.room + one.path + one.size + 1u, .bytes = one.whole - (one.size + 1u)});
        s_queue.head = (s_queue.head + 1u) % s_queue.waiting_most;
        s_queue.count -= 1u;
        s_queue.used = (s_queue.count == 0u) ? 0ull : s_queue.used;
        s_emit[checker] = s_queue.sink;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
        EMBED_CALL(cli_servers_path_sink_internal.check_one, CliServersPathSinkCtx, .checker = checker,
                   .path = s_path[checker], .one_servers_waiting_pointer = &waiting, .sink = &s_emit[checker]);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = one.server);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
        s_queue.done += 1ull;
        const unsigned long long done = s_queue.done;
        const unsigned long long total = s_queue.total;
        const int has_sink = s_queue.has_sink;
        s_emit[checker] = s_queue.sink;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
        if (has_sink)
        {
            EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = &s_emit[checker], .done = done, .total = total);
        }
    }
}

// the threads that check the tree's files started, once
static void check_checkers_start(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    const int started = s_queue.started;
    s_queue.started = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    for (unsigned int at = 0u; !started && (at < CLI_SERVERS_CHECKING); at += 1u)
    {
        s_checker_index[at] = at;
        s_checker[at] = (CliThread){check_checker, &s_checker_index[at], CLI_PRIORITY_GUEST};
        (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_checker[at]);
    }
}

unsigned int cli_servers_check_tree(const CliServersRootSinkCountCfg *args)
{
    const CliServersSink *sink = args->sink;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    if (!EMBED_CALL(cli_servers_queue_files_internal.check_room_init, CliServersQueueFilesCtx, .queue = &s_queue,
                    .files = args->files, .count = args->count))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
        return 0u;
    }
    s_queue.on = 1;
    VerbaLinea line = {s_queue.root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
    s_queue.sink = *sink;
    s_queue.has_sink = 1;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    EMBED_CALL(cli_servers_queue_files_internal.check_files_put, CliServersQueueFilesCtx, .queue = &s_queue,
               .files = args->files, .count = args->count);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    check_checkers_start();
    EMBED_CALL(cli_servers_files_internal.check_tree_with_checkers, CliServersFilesCtx, .root = args->root,
               .sink = sink);
    EMBED_CALL(cli_servers_files_internal.check_tree_own, CliServersFilesCtx, .root = args->root, .files = args->files,
               .count = args->count, .sink = sink);
    return EMBED_CALL(cli_servers_files_internal.check_files, CliServersFilesCtx, .root = args->root,
                      .files = args->files, .count = args->count, .sink = sink);
}

void cli_servers_stop_check(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    check_queue_clear();
    s_queue.total = s_queue.done;
    const int has_sink = s_queue.has_sink;
    static CliServersSink s_emit;
    s_emit = s_queue.sink;
    const unsigned long long done = s_queue.done;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    if (has_sink)
    {
        EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = &s_emit, .done = done, .total = done);
    }
}

int cli_servers_check_on(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    const int on = s_queue.on;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    return on;
}

int cli_servers_checking(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    const int checking = s_queue.on && (s_queue.done < s_queue.total);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    return checking;
}

void cli_servers_check_set(const CliServersSinkCtx *args)
{
    int on = args->on;
    const CliServersSink *sink = args->sink;
    unsigned long long done = args->done;
    unsigned long long total = args->total;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    s_queue.on = on;
    s_queue.has_sink = sink != NULL;
    if (sink != NULL)
    {
        s_queue.sink = *sink;
    }
    s_queue.done = done;
    s_queue.total = total;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
}

void cli_servers_check_count(const CliServersOnCtx *args)
{
    int *on = args->on_pointer;
    unsigned int *waiting = args->waiting_u32s;
    unsigned long long *done = args->done_pointer;
    unsigned long long *total = args->total_pointer;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    *on = s_queue.on;
    *waiting = s_queue.count;
    *done = s_queue.done;
    *total = s_queue.total;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
}

int cli_servers_check_takes(const CliServersPathLspCtx *args)
{
    CliLsp *lsp = args->lsp;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    int took = s_queue.on;
    if (took)
    {
        CliServersWaiting waiting = {lsp, ""};
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
        took = EMBED_CALL(cli_servers_queue_internal.check_queue_put, CliServersQueueCtx, .queue = &s_queue,
                          .path = args->path, .waiting = &waiting, .front = 1);
        if (took)
        {
            char uri[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = args->path, .uri_out = uri);
            EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 1);
            s_queue.total += 1ull;
            EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_queue_ready);
        }
        else
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    return took;
}

void cli_servers_check_let_go(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    check_queue_clear();
    s_queue.on = 0;
    s_queue.root[0] = '\0';
    s_queue.has_sink = 0;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    s_queue.total = 0ull;
    s_queue.done = 0ull;
    EMBED_CALL(cli_servers_queue_files_internal.check_room_free, CliServersQueueFilesCtx, .queue = &s_queue);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_check_open_lock);
    cli_servers_check_open_clear();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_check_open_lock);
}

char *cli_servers_check_tree_files(const CliServersRootSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    const int on = s_queue.on && (s_queue.files != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = args->root, .cap = CLI_CELLS_POOL}));
    // the queue's bytes hold every path it lists, each ended by a 0
    char *const files = on ? g_cli_cells->temporary_buf_alloc(s_queue.files_used + 1ull, &g_cli_servers_cells) : NULL;
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (files != NULL) && (at < s_queue.file_count); at += 1u)
    {
        const char *const file = s_queue.files + s_queue.file_at[at];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1ull;
        memor.cpy(&(MemoriaCfg){.dst = files + used, .src = file, .bytes = (size_t)length});
        used += length;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    *size = used;
    return files;
}

int cli_servers_check_files_change(const CliServersFilesCountCtx *args)
{
    CliServersSink *sink = args->sink_out;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    const int on = s_queue.on && s_queue.has_sink && (s_queue.files != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = args->root, .cap = CLI_CELLS_POOL}));
    if (on)
    {
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            unsigned int remaining = 0u;
            while ((remaining < s_queue.file_count) &&
                   (!cellul.eq(&(CatenaFinitaCfg){.src = s_queue.files + s_queue.file_at[remaining],
                                                  .other = args->files[at],
                                                  .cap = CLI_CELLS_POOL})))
            {
                remaining += 1u;
            }
            if (remaining == s_queue.file_count)
            {
                EMBED_CALL(cli_servers_queue_files_internal.check_files_put, CliServersQueueFilesCtx, .queue = &s_queue,
                           .files = &args->files[at], .count = 1u);
            }
        }
        unsigned int remaining = 0u;
        unsigned long long used = 0ull;
        for (unsigned int at = 0u; at < s_queue.file_count; at += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root,
                       .relative = s_queue.files + s_queue.file_at[at], .joined = path);
            if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
            {
                const size_t size =
                    cellul.len(&(CatenaFinitaCfg){.src = s_queue.files + s_queue.file_at[at], .cap = CLI_CELLS_POOL}) +
                    1u;
                memor.move_down(&(MemoriaCfg){
                    .dst = s_queue.files + used, .src = s_queue.files + s_queue.file_at[at], .bytes = size});
                s_queue.file_at[remaining] = used;
                used += size;
                remaining += 1u;
            }
        }
        s_queue.file_count = remaining;
        s_queue.files_used = used;
        *sink = s_queue.sink;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    return on;
}

unsigned int cli_servers_check_queue(const CliServersCountCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_queue_lock);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        int queued = 0;
        for (unsigned int one = 0u; !queued && (one < s_queue.count); one += 1u)
        {
            queued = (cellul.eq(&(CatenaFinitaCfg){
                .src = s_queue.room + EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                                 .queue_read = &s_queue, .at = one)
                                          ->path,
                .other = args->paths[at],
                .cap = CLI_CELLS_POOL}));
        }
        if (!queued && EMBED_CALL(cli_servers_queue_internal.check_queue_put, CliServersQueueCtx, .queue = &s_queue,
                                  .path = args->paths[at], .waiting = &args->waiting[at], .front = 0))
        {
            s_queue.total += 1ull;
            continue;
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = args->waiting[at].server);
    }
    const int idle = (s_queue.count == 0u) && (s_queue.done >= s_queue.total);
    const unsigned long long done = s_queue.done;
    const unsigned long long total = s_queue.total;
    if (!idle)
    {
        EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_queue_ready);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_queue_lock);
    if (idle)
    {
        EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = args->sink, .done = done, .total = total);
    }
    return args->count;
}
