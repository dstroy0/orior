// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_jobs.c: the servers' work done off the thread that requests it: a server stopped, a file checked by a
// checker or by orior's own reading, the tree checked whole, and the tree read. Each job takes a place of its
// own as it is handed over, its bytes a cell sized to them, copied there or handed over whole by the caller who filled
// it, and is taken by the first of the servers' threads free, which gives the cell back once the job is done; the
// threads started as the first jobs come; a job handed over while every place is taken waits for one
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

// the threads that take the jobs, and the places jobs wait in
#define CLI_SERVERS_WORKERS 8u
#define CLI_SERVERS_POSITIONS 16u

// A job waiting: whether its place is taken, the function that does it, and its bytes, a cell of `size`
typedef struct
{
    int used;
    CliServersJob run;
    unsigned long long size;
    char *bytes;
} CliServersPosition;

// the places, the order they were taken in, and the threads, under the jobs' lock
static CliServersPosition s_position[CLI_SERVERS_POSITIONS];
static unsigned int s_order[CLI_SERVERS_POSITIONS];
static unsigned int s_waiting;
static CliWake s_wake = CLI_WAKE_INIT;
static CliThread s_thread[CLI_SERVERS_WORKERS];
static unsigned int s_index[CLI_SERVERS_WORKERS];
static unsigned int s_started;
static unsigned int s_idle;

// a thread of the servers': each job taken in the order it was handed over and its place given back, then done and
// its cell given back
static void jobs_worker(void *context)
{
    (void)context;
    for (;;)
    {
        s_idle += 1u;
        while (s_waiting == 0u)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
        }
        s_idle -= 1u;
        CliServersPosition *const place = &s_position[s_order[0]];
        s_waiting -= 1u;
        memor.move_down(&(MemoriaCfg){.dst = s_order, .src = s_order + 1u, .bytes = s_waiting * sizeof(s_order[0])});
        const CliServersJob run = place->run;
        const unsigned long long size = place->size;
        char *const bytes = place->bytes;
        place->bytes = NULL;
        place->used = 0;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
        EMBED_CALL(run, CliServersJobCfg, .job = bytes, .size = size);
        g_cli_guard->persistent_buf_release(bytes, g_cli_servers_owner);
    }
}

int cli_servers_later(const CliServersRunCtx *args)
{
    char *const bytes = g_cli_guard->persistent_buf_alloc(args->size, g_cli_servers_owner);
    if (bytes != NULL)
    {
        // a size the cell stores fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = args->job, .bytes = (size_t)args->size});
    }
    return EMBED_CALL(cli_servers_run_size_internal.later_dispatch, CliServersRunSizeCtx, .run = args->run,
                      .bytes = bytes, .size = args->size);
}

int cli_servers_later_dispatch(const CliServersRunSizeCtx *args)
{
    CliServersJob run = args->run;
    char *bytes = args->bytes;
    unsigned long long size = args->size;
    if (bytes == NULL)
    {
        return 0;
    }
    for (;;)
    {
        unsigned int free = 0u;
        while ((free < CLI_SERVERS_POSITIONS) && s_position[free].used)
        {
            free += 1u;
        }
        if (free < CLI_SERVERS_POSITIONS)
        {
            CliServersPosition *const place = &s_position[free];
            place->used = 1;
            place->run = run;
            place->size = size;
            place->bytes = bytes;
            s_order[s_waiting] = free;
            s_waiting += 1u;
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    if ((s_idle < s_waiting) && (s_started < CLI_SERVERS_WORKERS))
    {
        s_index[s_started] = s_started;
        s_thread[s_started] = (CliThread){jobs_worker, &s_index[s_started]};
        s_started +=
            EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread[s_started]) ? 1u : 0u;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    return 1;
}
