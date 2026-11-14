// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check.c: the tree's check: whether it is on, the tree's files, the files waiting, and how many of the files
// it was given are done. Each file of a language with a server is queued for it, a server that checks its projects as
// a whole instructed to check them in place of that, unless the tree's own code is disabled, as such a check builds and
// runs it. CHECKING threads, started once, take the files waiting in turn, and the check is notified how far it has
// gone as each is done
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

// the most languages one batch of files is of
#define CLI_SERVERS_BATCH_LANGUAGES 64u

// the check, under its lock, and the wake its threads wait on for a file
static CliServersQueue s_queue;
static CliWake s_queue_ready = CLI_WAKE_INIT;
static CliThread s_checker[CLI_SERVERS_CHECKING];
static unsigned int s_checker_index[CLI_SERVERS_CHECKING];
// the first checker serves the ring of the files a change puts first, the others the tree's files
EMBED_STATIC_ASSERT(CLI_SERVERS_CHECKING >= 2u, "the check needs a checker for each of its two rings");

// the place of `path` among the tree's files, CLI_SERVERS_NO_FILE where it is not one; with the queue's lock acquired
static unsigned int check_file_find(const char *path)
{
    // the path with its 0 against each file's text in the cell, the start of each read from the cell's starts
    const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u;
    for (unsigned int at = 0u; (s_queue.files_cell != NULL) && (at < s_queue.file_count); at += 1u)
    {
        unsigned long long start = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_queue.files_cell, .offset = CLI_SERVERS_FILE_AT(at), .dst = &start, .bytes = sizeof(start)});
        if (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = s_queue.files_cell,
                                                                 .offset = CLI_SERVERS_FILES_TEXT_AT(&s_queue, start),
                                                                 .src = path,
                                                                 .bytes = size}) == 0)
        {
            return at;
        }
    }
    return CLI_SERVERS_NO_FILE;
}

// every file waiting let go: the generation moved on, so a checker drops each as it pops it and gives its server's
// reference back, the checkers woken to do so; with the queue's lock acquired
static void check_queue_clear(void)
{
    embed_atomic_u64_store(&s_queue.generation, embed_atomic_u64_load(&s_queue.generation) + 1ULL);
    EMBED_CALL(cli_servers_queue_files_internal.check_queued_cleared, CliServersQueueFilesCtx, .queue = &s_queue);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_ready);
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

// a checker thread: a ticket of its ring taken, the first checker's the ring of the files a change puts first and the
// others' the ring of the tree's files; the file pushed under it, once the check has somewhere to report, popped,
// checked where its generation is the check's and counted done, and its cell released
static void check_checker(void *context)
{
    const unsigned int checker = *(const unsigned int *)context;
    static char s_path[CLI_SERVERS_CHECKING][CLI_PATH_MAX_LEN];
    static CliServersSink s_emit[CLI_SERVERS_CHECKING];
    CliServersCheckRing *const ring = (checker == 0u) ? &s_queue.first : &s_queue.waiting;
    // Explicit cast carries the checker's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_checker_index[checker];
    for (;;)
    {
        const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&ring->taken, 1u);
        embed_u64 stamp = 0u;
        for (;;)
        {
            const unsigned int ready = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_queue_ready);
            if (cli_ticket_pushed(&ring->ring, ticket, &stamp) && s_queue.has_sink)
            {
                break;
            }
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_queue_ready, .seen = ready);
        }
        void *popped = NULL;
        embed_u64 seen = 0u;
        (void)embed_ticket_ring_popped(&ring->turnstiles, address, &ring->ring, stamp, &popped, &seen);
        const CarcerMovableEntry *const cell = popped;
        CliServersQueued one = {NULL, 0ULL, CLI_SERVERS_NO_FILE, 0u};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .dst = &one, .bytes = sizeof(one)});
        // a stale file's marks went with its check's cells, so only a fresh file's mark is cleared
        const int stale = one.generation != embed_atomic_u64_load(&s_queue.generation);
        if (!stale)
        {
            if ((one.file != CLI_SERVERS_NO_FILE) && (one.file < s_queue.file_at_max))
            {
                static const unsigned char s_idle = 0u;
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = s_queue.files_cell,
                                        .offset = CLI_SERVERS_QUEUED_AT(&s_queue, one.file),
                                        .src = &s_idle,
                                        .bytes = 1u});
            }
            // the path and the name read out of the cell where they lie, the name's length its bytes before its 0
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = cell, .offset = sizeof(one), .dst = s_path[checker], .bytes = one.size + 1u});
            CliServersWaiting waiting = {one.server, ""};
            const embed_word name_at = sizeof(one) + one.size + 1u;
            const embed_word name_size = g_cli_guard->movable_buf_measure(
                &(CarcerMovableCfg){.cell = cell, .offset = name_at, .bytes = CLI_NAME_MAX_LEN - 1u});
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = cell, .offset = name_at, .dst = waiting.id, .bytes = name_size + 1u});
            s_emit[checker] = s_queue.sink;
            EMBED_CALL(cli_servers_path_sink_internal.check_one, CliServersPathSinkCtx, .checker = checker,
                       .path = s_path[checker], .one_servers_waiting_pointer = &waiting, .sink = &s_emit[checker]);
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = one.server);
        (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_check_work_state_owner);
        (void)embed_ticket_ring_released(&ring->turnstiles, &ring->ring, stamp, &seen);
        if (stale)
        {
            continue;
        }
        const unsigned long long done = EMBED_ATOMIC_ADD_64(&s_queue.done, 1ULL) + 1ULL;
        const unsigned long long total = embed_atomic_u64_load(&s_queue.total);
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
    embed_atomic_u64_store(&s_queue.total, embed_atomic_u64_load(&s_queue.done));
    const int has_sink = s_queue.has_sink;
    static CliServersSink s_emit;
    s_emit = s_queue.sink;
    const unsigned long long done = embed_atomic_u64_load(&s_queue.done);
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
    const int checking = s_queue.on && (embed_atomic_u64_load(&s_queue.done) < embed_atomic_u64_load(&s_queue.total));
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
    embed_atomic_u64_store(&s_queue.done, done);
    embed_atomic_u64_store(&s_queue.total, total);
}

void cli_servers_check_count(const CliServersOnCtx *args)
{
    int *on = args->on_pointer;
    unsigned int *waiting = args->waiting_u32s;
    unsigned long long *done = args->done_pointer;
    unsigned long long *total = args->total_pointer;
    *on = s_queue.on;
    *waiting = cli_servers_check_waiting(&s_queue);
    *done = embed_atomic_u64_load(&s_queue.done);
    *total = embed_atomic_u64_load(&s_queue.total);
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
                              .path = args->path, .waiting = &waiting, .file = check_file_find(args->path), .front = 1);
        if (consumed)
        {
            char uri[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = args->path, .uri_out = uri);
            EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 1);
            (void)EMBED_ATOMIC_ADD_64(&s_queue.total, 1ULL);
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
    embed_atomic_u64_store(&s_queue.total, 0ULL);
    embed_atomic_u64_store(&s_queue.done, 0ULL);
    if (s_queue.files_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_queue.files_cell, g_cli_servers_check_work_state_owner);
    }
    s_queue.files_cell = NULL;
    s_queue.files_max = 0ULL;
    s_queue.file_at_max = 0u;
    cli_servers_check_open_clear();
}

char *cli_servers_check_tree_files(const CliServersRootSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    const int on = s_queue.on && (s_queue.files_cell != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = args->root, .cap = CLI_POOL_BYTES}));
    // the cell's texts hold every path it lists, each ended by a 0, read whole into the caller's cell
    char *const files = on ? g_cli_guard->temporary_buf_alloc(s_queue.files_used + 1ULL, g_cli_servers_owner) : NULL;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (files != NULL) && (at < s_queue.file_count); at += 1u)
    {
        unsigned long long start = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_queue.files_cell, .offset = CLI_SERVERS_FILE_AT(at), .dst = &start, .bytes = sizeof(start)});
        const unsigned long long text = CLI_SERVERS_FILES_TEXT_AT(&s_queue, start);
        const unsigned long long length =
            g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = s_queue.files_cell, .offset = text}) + 1ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_queue.files_cell, .offset = text, .dst = files + used, .bytes = (embed_word)length});
        used += length;
    }
    *size = used;
    return files;
}

int cli_servers_check_files_change(const CliServersFilesCountCtx *args)
{
    CliServersSink *sink = args->sink_out;
    const int on = s_queue.on && s_queue.has_sink && (s_queue.files_cell != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = args->root, .cap = CLI_POOL_BYTES}));
    if (on)
    {
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            if (check_file_find(args->files[at]) == CLI_SERVERS_NO_FILE)
            {
                EMBED_CALL(cli_servers_queue_files_internal.check_files_append, CliServersQueueFilesCtx,
                           .queue = &s_queue, .files = &args->files[at], .count = 1u);
            }
        }
        // the files still on the disk kept, each read out of the cell and written back at the first free place
        unsigned int remaining = 0u;
        unsigned long long used = 0ULL;
        for (unsigned int at = 0u; at < s_queue.file_count; at += 1u)
        {
            char relative[CLI_PATH_MAX_LEN];
            char path[CLI_PATH_MAX_LEN];
            unsigned long long start = 0ULL;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_queue.files_cell, .offset = CLI_SERVERS_FILE_AT(at), .dst = &start, .bytes = sizeof(start)});
            const unsigned long long text = CLI_SERVERS_FILES_TEXT_AT(&s_queue, start);
            const embed_word size =
                g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = s_queue.files_cell, .offset = text}) + 1u;
            if (size > sizeof(relative))
            {
                continue;
            }
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_queue.files_cell, .offset = text, .dst = relative, .bytes = size});
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = relative, .joined = path);
            if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = s_queue.files_cell,
                                        .offset = CLI_SERVERS_FILES_TEXT_AT(&s_queue, used),
                                        .src = relative,
                                        .bytes = size});
                (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_queue.files_cell,
                                                                         .offset = CLI_SERVERS_FILE_AT(remaining),
                                                                         .src = &used,
                                                                         .bytes = sizeof(used)});
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
        // a tree's file already waiting is not queued again
        const unsigned int file = check_file_find(args->paths[at]);
        unsigned char waiting = 0u;
        if (file != CLI_SERVERS_NO_FILE)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_queue.files_cell,
                                                                    .offset = CLI_SERVERS_QUEUED_AT(&s_queue, file),
                                                                    .dst = &waiting,
                                                                    .bytes = 1u});
        }
        const int queued = waiting != 0u;
        if (!queued && EMBED_CALL(cli_servers_queue_internal.check_queue_append, CliServersQueueCtx, .queue = &s_queue,
                                  .path = args->paths[at], .waiting = &args->waiting[at], .file = file, .front = 0))
        {
            (void)EMBED_ATOMIC_ADD_64(&s_queue.total, 1ULL);
            continue;
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = args->waiting[at].server);
    }
    const int idle = (cli_servers_check_waiting(&s_queue) == 0u) &&
                     (embed_atomic_u64_load(&s_queue.done) >= embed_atomic_u64_load(&s_queue.total));
    const unsigned long long done = embed_atomic_u64_load(&s_queue.done);
    const unsigned long long total = embed_atomic_u64_load(&s_queue.total);
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
