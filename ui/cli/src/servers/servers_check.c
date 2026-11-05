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
    s_queue.used = 0ULL;
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
    if (args->sink->check_moved != NULL)
    {
        EMBED_CALL(args->sink->check_moved, CliServerCheckCfg, .context = args->sink->context, .done = args->done,
                   .total = args->total);
    }
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
        while ((s_queue.count == 0u) || !s_queue.has_sink)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_queue_ready);
        }
        const CliServersQueued one = *EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                                 .queue_read = &s_queue, .at = 0u);
        memor.cpy(&(MemoriaCfg){.dst = s_path[checker], .src = s_queue.capacity + one.path, .bytes = one.size + 1u});
        CliServersWaiting waiting = {one.server, ""};
        memor.cpy(&(MemoriaCfg){.dst = waiting.id,
                                .src = s_queue.capacity + one.path + one.size + 1u,
                                .bytes = one.integer - (one.size + 1u)});
        s_queue.head = (s_queue.head + 1u) % s_queue.waiting_max;
        s_queue.count -= 1u;
        s_queue.used = (s_queue.count == 0u) ? 0ULL : s_queue.used;
        s_emit[checker] = s_queue.sink;
        EMBED_CALL(cli_servers_path_sink_internal.check_one, CliServersPathSinkCtx, .checker = checker,
                   .path = s_path[checker], .one_servers_waiting_pointer = &waiting, .sink = &s_emit[checker]);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = one.server);
        s_queue.done += 1ULL;
        const unsigned long long done = s_queue.done;
        const unsigned long long total = s_queue.total;
        const int has_sink = s_queue.has_sink;
        s_emit[checker] = s_queue.sink;
        if (has_sink)
        {
            EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = &s_emit[checker], .done = done, .total = total);
        }
    }
}

// the threads that check the tree's files started, once
static void check_checkers_start(void)
{
    const int started = s_queue.started;
    s_queue.started = 1;
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
    if (!EMBED_CALL(cli_servers_queue_files_internal.check_work_state_init, CliServersQueueFilesCtx, .queue = &s_queue,
                    .files = args->files, .count = args->count))
    {
        return 0u;
    }
    s_queue.on = 1;
    VerbaLinea line = {s_queue.root, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
    s_queue.sink = *sink;
    s_queue.has_sink = 1;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ULL;
    EMBED_CALL(cli_servers_queue_files_internal.check_files_append, CliServersQueueFilesCtx, .queue = &s_queue,
               .files = args->files, .count = args->count);
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
    check_queue_clear();
    s_queue.total = s_queue.done;
    const int has_sink = s_queue.has_sink;
    static CliServersSink s_emit;
    s_emit = s_queue.sink;
    const unsigned long long done = s_queue.done;
    if (has_sink)
    {
        EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = &s_emit, .done = done, .total = done);
    }
}

int cli_servers_check_on(void)
{
    const int on = s_queue.on;
    return on;
}

int cli_servers_checking(void)
{
    const int checking = s_queue.on && (s_queue.done < s_queue.total);
    return checking;
}

void cli_servers_check_set(const CliServersSinkCtx *args)
{
    int on = args->on;
    const CliServersSink *sink = args->sink;
    unsigned long long done = args->done;
    unsigned long long total = args->total;
    s_queue.on = on;
    s_queue.has_sink = sink != NULL;
    if (sink != NULL)
    {
        s_queue.sink = *sink;
    }
    s_queue.done = done;
    s_queue.total = total;
}

void cli_servers_check_count(const CliServersOnCtx *args)
{
    int *on = args->on_pointer;
    unsigned int *waiting = args->waiting_u32s;
    unsigned long long *done = args->done_pointer;
    unsigned long long *total = args->total_pointer;
    *on = s_queue.on;
    *waiting = s_queue.count;
    *done = s_queue.done;
    *total = s_queue.total;
}

int cli_servers_check_allocs(const CliServersPathLspCtx *args)
{
    CliLsp *lsp = args->lsp;
    int consumed = s_queue.on;
    if (consumed)
    {
        CliServersWaiting waiting = {lsp, ""};
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
        consumed = EMBED_CALL(cli_servers_queue_internal.check_queue_append, CliServersQueueCtx, .queue = &s_queue,
                              .path = args->path, .waiting = &waiting, .front = 1);
        if (consumed)
        {
            char uri[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = args->path, .uri_out = uri);
            EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 1);
            s_queue.total += 1ULL;
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_ready);
        }
        else
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
        }
    }
    return consumed;
}

void cli_servers_check_let_go(void)
{
    check_queue_clear();
    s_queue.on = 0;
    s_queue.root[0] = '\0';
    s_queue.has_sink = 0;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ULL;
    s_queue.total = 0ULL;
    s_queue.done = 0ULL;
    g_cli_cells->persistent_buf_release(s_queue.files, &g_cli_servers_check_work_state_cells);
    g_cli_cells->persistent_buf_release(s_queue.file_at, &g_cli_servers_check_work_state_cells);
    g_cli_cells->persistent_buf_release(s_queue.waiting, &g_cli_servers_check_work_state_cells);
    g_cli_cells->persistent_buf_release(s_queue.capacity, &g_cli_servers_check_work_state_cells);
    s_queue.files = NULL;
    s_queue.file_at = NULL;
    s_queue.waiting = NULL;
    s_queue.capacity = NULL;
    cli_servers_check_open_clear();
}

char *cli_servers_check_tree_files(const CliServersRootSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    const int on = s_queue.on && (s_queue.files != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = args->root, .cap = CLI_CELLS_POOL}));
    // the queue's bytes hold every path it lists, each ended by a 0
    char *const files = on ? g_cli_cells->temporary_buf_alloc(s_queue.files_used + 1ULL, &g_cli_servers_cells) : NULL;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (files != NULL) && (at < s_queue.file_count); at += 1u)
    {
        const char *const file = s_queue.files + s_queue.file_at[at];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = files + used, .src = file, .bytes = (size_t)length});
        used += length;
    }
    *size = used;
    return files;
}

int cli_servers_check_files_change(const CliServersFilesCountCtx *args)
{
    CliServersSink *sink = args->sink_out;
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
                EMBED_CALL(cli_servers_queue_files_internal.check_files_append, CliServersQueueFilesCtx,
                           .queue = &s_queue, .files = &args->files[at], .count = 1u);
            }
        }
        unsigned int remaining = 0u;
        unsigned long long used = 0ULL;
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
    return on;
}

unsigned int cli_servers_check_queue(const CliServersCountCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        int queued = 0;
        for (unsigned int one = 0u; !queued && (one < s_queue.count); one += 1u)
        {
            queued = (cellul.eq(&(CatenaFinitaCfg){
                .src = s_queue.capacity + EMBED_CALL(cli_servers_queue_at_internal.check_queued_at,
                                                     CliServersQueueAtCtx, .queue_read = &s_queue, .at = one)
                                              ->path,
                .other = args->paths[at],
                .cap = CLI_CELLS_POOL}));
        }
        if (!queued && EMBED_CALL(cli_servers_queue_internal.check_queue_append, CliServersQueueCtx, .queue = &s_queue,
                                  .path = args->paths[at], .waiting = &args->waiting[at], .front = 0))
        {
            s_queue.total += 1ULL;
            continue;
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = args->waiting[at].server);
    }
    const int idle = (s_queue.count == 0u) && (s_queue.done >= s_queue.total);
    const unsigned long long done = s_queue.done;
    const unsigned long long total = s_queue.total;
    if (!idle)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_ready);
    }
    if (idle)
    {
        EMBED_CALL(check_progress_emit, ServersCheckCtx, .sink = args->sink, .done = done, .total = total);
    }
    return args->count;
}
