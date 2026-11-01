// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check.c: the tree's check: whether it is on, the tree's files, the files waiting, and how many of the files
// it was given are done. Each file of a language with a server is queued for it, a server that checks its projects as
// a whole instructed to check them in place of that, unless the tree's own code is disabled, as such a check builds and
// runs it. CHECKING threads, started once, take the files waiting in turn, and the check is notified how far it has
// gone as each is done
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
static void check_queue_emptied(void)
{
    for (unsigned int at = 0u; at < s_queue.count; at += 1u)
    {
        cli_lsp_unref(cli_servers_check_queued_at(&s_queue, at)->server);
    }
    s_queue.head = 0u;
    s_queue.count = 0u;
    s_queue.used = 0ull;
}

// how far the check has gone reported where it goes
static void check_progress_emitted(const CliServersSink *sink, unsigned long long done, unsigned long long total)
{
    if (sink->emit == NULL)
    {
        return;
    }
    char progress_buf[128];
    VerbaLinea line = {progress_buf, sizeof(progress_buf), 0ull};
    mmgr_line_put(&line, "{\"done\":");
    mmgr_line_decimal(&line, done, 1u);
    mmgr_line_put(&line, ",\"total\":");
    mmgr_line_decimal(&line, total, 1u);
    mmgr_line_ch(&line, '}');
    sink->emit(sink->context, "tree-check", progress_buf, line.at);
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
        cli_lock_take(&s_queue_lock);
        while ((s_queue.count == 0u) || !s_queue.has_sink)
        {
            cli_wake_wait(&s_queue_ready, &s_queue_lock);
        }
        const CliServersQueued one = *cli_servers_check_queued_at(&s_queue, 0u);
        memor.cpy(&(MemoriaCfg){.dst = s_path[checker], .src = s_queue.room + one.path, .bytes = one.size + 1u});
        CliServersWaiting waiting = {one.server, ""};
        memor.cpy(&(MemoriaCfg){
            .dst = waiting.id, .src = s_queue.room + one.path + one.size + 1u, .bytes = one.whole - (one.size + 1u)});
        s_queue.head = (s_queue.head + 1u) % s_queue.waiting_most;
        s_queue.count -= 1u;
        s_queue.used = (s_queue.count == 0u) ? 0ull : s_queue.used;
        s_emit[checker] = s_queue.sink;
        cli_lock_give(&s_queue_lock);
        cli_servers_check_one(checker, s_path[checker], &waiting, &s_emit[checker]);
        cli_lsp_unref(one.server);
        cli_lock_take(&s_queue_lock);
        s_queue.done += 1ull;
        const unsigned long long done = s_queue.done;
        const unsigned long long total = s_queue.total;
        const int has_sink = s_queue.has_sink;
        s_emit[checker] = s_queue.sink;
        cli_lock_give(&s_queue_lock);
        if (has_sink)
        {
            check_progress_emitted(&s_emit[checker], done, total);
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
        s_checker[at] = (CliThread){check_checker, &s_checker_index[at], CLI_PRIORITY_GUEST};
        (void)cli_thread_started(&s_checker[at]);
    }
}

unsigned int cli_servers_check_tree(const char *root, const char *const *files, unsigned int count,
                                    const CliServersSink *sink)
{
    cli_lock_take(&s_queue_lock);
    if (!cli_servers_check_room_made(&s_queue, files, count))
    {
        cli_lock_give(&s_queue_lock);
        return 0u;
    }
    s_queue.on = 1;
    VerbaLinea line = {s_queue.root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    (void)mmgr_line_finish(&line);
    s_queue.sink = *sink;
    s_queue.has_sink = 1;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    cli_servers_check_files_put(&s_queue, files, count);
    cli_lock_give(&s_queue_lock);
    check_checkers_started();
    cli_servers_check_tree_with_checkers(root, sink);
    cli_servers_check_tree_own(root, files, count, sink);
    return cli_servers_check_files(root, files, count, sink);
}

void cli_servers_stop_check(void)
{
    cli_lock_take(&s_queue_lock);
    check_queue_emptied();
    s_queue.total = s_queue.done;
    const int has_sink = s_queue.has_sink;
    static CliServersSink s_emit;
    s_emit = s_queue.sink;
    const unsigned long long done = s_queue.done;
    cli_lock_give(&s_queue_lock);
    if (has_sink)
    {
        check_progress_emitted(&s_emit, done, done);
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

void cli_servers_check_set(int on, const CliServersSink *sink, unsigned long long done, unsigned long long total)
{
    cli_lock_take(&s_queue_lock);
    s_queue.on = on;
    s_queue.has_sink = sink != NULL;
    if (sink != NULL)
    {
        s_queue.sink = *sink;
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
        cli_lsp_ref(lsp);
        took = cli_servers_check_queue_put(&s_queue, path, &waiting, 1);
        if (took)
        {
            char uri[CLI_PATH_MAX_LEN];
            cli_servers_uri_of(path, uri);
            cli_servers_check_open_set(uri, 1);
            s_queue.total += 1ull;
            cli_wake_all(&s_queue_ready);
        }
        else
        {
            cli_lsp_unref(lsp);
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
    s_queue.has_sink = 0;
    s_queue.file_count = 0u;
    s_queue.files_used = 0ull;
    s_queue.total = 0ull;
    s_queue.done = 0ull;
    cli_servers_check_room_given(&s_queue);
    cli_lock_give(&s_queue_lock);
    cli_lock_take(&g_cli_servers_check_open_lock);
    cli_servers_check_open_cleared();
    cli_lock_give(&g_cli_servers_check_open_lock);
}

char *cli_servers_check_tree_files(const char *root, unsigned long long *size)
{
    cli_lock_take(&s_queue_lock);
    const int on = s_queue.on && (s_queue.files != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = root, .cap = CLI_CELLS_POOL}));
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
    cli_lock_give(&s_queue_lock);
    *size = used;
    return files;
}

int cli_servers_check_files_changed(const char *root, const char *const *files, unsigned int count,
                                    CliServersSink *sink)
{
    cli_lock_take(&s_queue_lock);
    const int on = s_queue.on && s_queue.has_sink && (s_queue.files != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.root, .other = root, .cap = CLI_CELLS_POOL}));
    if (on)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            unsigned int remaining = 0u;
            while ((remaining < s_queue.file_count) &&
                   (!cellul.eq(&(CatenaFinitaCfg){
                       .src = s_queue.files + s_queue.file_at[remaining], .other = files[at], .cap = CLI_CELLS_POOL})))
            {
                remaining += 1u;
            }
            if (remaining == s_queue.file_count)
            {
                cli_servers_check_files_put(&s_queue, &files[at], 1u);
            }
        }
        unsigned int remaining = 0u;
        unsigned long long used = 0ull;
        for (unsigned int at = 0u; at < s_queue.file_count; at += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            cli_full(root, s_queue.files + s_queue.file_at[at], path);
            if (query_is_file(path))
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
    cli_lock_give(&s_queue_lock);
    return on;
}

unsigned int cli_servers_check_queued(const char *const *paths, const CliServersWaiting *waiting, unsigned int count,
                                      const CliServersSink *sink)
{
    cli_lock_take(&s_queue_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        int queued = 0;
        for (unsigned int one = 0u; !queued && (one < s_queue.count); one += 1u)
        {
            queued =
                (cellul.eq(&(CatenaFinitaCfg){.src = s_queue.room + cli_servers_check_queued_at(&s_queue, one)->path,
                                              .other = paths[at],
                                              .cap = CLI_CELLS_POOL}));
        }
        if (!queued && cli_servers_check_queue_put(&s_queue, paths[at], &waiting[at], 0))
        {
            s_queue.total += 1ull;
            continue;
        }
        cli_lsp_unref(waiting[at].server);
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
        check_progress_emitted(sink, done, total);
    }
    return count;
}
