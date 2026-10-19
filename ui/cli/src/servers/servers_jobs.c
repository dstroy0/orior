// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_jobs.c: the servers' work done off the thread that asks for it: a server stopped, a file checked by a
// checker or by orior's own reading, the tree checked whole, and the tree's index read. Each job takes a place of its
// own as it is handed over, its bytes a cell sized to them, copied there or handed over whole by the caller who filled
// it, and is taken by the first of the servers' threads free, which gives the cell back once the job is done; the
// threads started as the first jobs come; a job handed over while every place is taken waits for one
#include "../command_line/cells.h"
#include "servers_internal.h"

// the threads that take the jobs, and the places jobs wait in
#define CLI_SERVERS_WORKERS 8u
#define CLI_SERVERS_PLACES 16u

// A job waiting: whether its place is taken, the function that does it, and its bytes, a cell of `size`
typedef struct
{
    int used;
    CliServersJob run;
    unsigned long long size;
    char *bytes;
} CliServersPlace;

// the places, the order they were taken in, and the threads, under the jobs' lock
static CliServersPlace s_place[CLI_SERVERS_PLACES];
static unsigned int s_order[CLI_SERVERS_PLACES];
static unsigned int s_waiting;
static CliLock s_lock = CLI_LOCK_INIT;
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
        cli_lock_take(&s_lock);
        s_idle += 1u;
        while (s_waiting == 0u)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
        s_idle -= 1u;
        CliServersPlace *const place = &s_place[s_order[0]];
        s_waiting -= 1u;
        memor.move_down(&(MemoriaCfg){.dst = s_order, .src = s_order + 1u, .bytes = s_waiting * sizeof(s_order[0])});
        const CliServersJob run = place->run;
        const unsigned long long size = place->size;
        char *const bytes = place->bytes;
        place->bytes = NULL;
        place->used = 0;
        cli_wake_all(&s_wake);
        cli_lock_give(&s_lock);
        run(bytes, size);
        cli_cell_given(bytes);
    }
}

int cli_servers_later(CliServersJob run, const void *job, unsigned long long size)
{
    char *const bytes = cli_cell_taken(size, "a job handed to the servers");
    if (bytes != NULL)
    {
        // a size the cell holds fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = job, .bytes = (size_t)size});
    }
    return cli_servers_later_handed(run, bytes, size);
}

int cli_servers_later_handed(CliServersJob run, char *bytes, unsigned long long size)
{
    if (bytes == NULL)
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    for (;;)
    {
        unsigned int free = 0u;
        while ((free < CLI_SERVERS_PLACES) && s_place[free].used)
        {
            free += 1u;
        }
        if (free < CLI_SERVERS_PLACES)
        {
            CliServersPlace *const place = &s_place[free];
            place->used = 1;
            place->run = run;
            place->size = size;
            place->bytes = bytes;
            s_order[s_waiting] = free;
            s_waiting += 1u;
            break;
        }
        cli_wake_wait(&s_wake, &s_lock);
    }
    if ((s_idle < s_waiting) && (s_started < CLI_SERVERS_WORKERS))
    {
        s_index[s_started] = s_started;
        s_thread[s_started] = (CliThread){jobs_worker, &s_index[s_started]};
        s_started += cli_thread_started(&s_thread[s_started]) ? 1u : 0u;
    }
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
    return 1;
}
