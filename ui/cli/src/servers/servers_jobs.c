// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_jobs.c: the servers' work done off the thread that requests it: a server stopped, a file checked by a
// checker or by orior's own reading, the tree checked whole, and the tree read. The jobs wait on one ticket ring of
// embedded_types': a dispatcher writes the job's bytes into a movable cell of their size, takes a ticket, waits for
// its slot's room, grants the slot and pushes the cell's token under the ticket; the workers take tickets in turn, and
// each waits for its ticket's push, pops the token, reads the job into a cell of its own call, runs it, gives both
// cells back and releases the slot. So the jobs
// run in the order the dispatchers took their tickets, the ring's proofs hold each slot to one pop, and no take
// contends with another dispatcher's claim. The workers start as the first jobs come; a dispatcher whose slot still
// holds the ticket a lap back waits for its release
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

#include "../command_line/rings.h"

// the threads that take the jobs
#define CLI_SERVERS_WORKERS 8u

// A job's head as its movable cell holds it, the job's bytes after it: what runs it and its bytes' size
typedef struct
{
    CliServersJob run;
    unsigned long long size;
} JobsCell;

// the ring over its slots, the jobs' part of the rings block, positioned as the process starts; its turnstiles, zeros at
// rest; the next ticket a dispatcher takes and the next a worker takes; how many workers have started; the wake the
// waiters sleep on; and the threads with their places
static EmbedTicketRing s_ring = {.mask = (1u << CLI_SERVERS_JOBS_RING_SHIFT) - 1u};
static EmbedTurnstiles s_turnstiles;
static embed_atomic_u32 s_ticket;
static embed_atomic_u32 s_taken;
static embed_atomic_u32 s_started;
static CliWake s_wake = CLI_WAKE_INIT;
static CliThread s_thread[CLI_SERVERS_WORKERS];
static unsigned int s_worker_index[CLI_SERVERS_WORKERS];

// a thread of the servers': a ticket taken, its push waited for, its cell's token popped, the job read into a cell of
// this call and run, both cells given back and the slot released, then the next ticket
static void jobs_worker(void *context)
{
    const unsigned int worker = *(const unsigned int *)context;
    // Explicit cast carries the worker's place as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_worker_index[worker];
    for (;;)
    {
        const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_taken, 1u);
        embed_u64 stamp = 0u;
        for (;;)
        {
            const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_wake);
            if (cli_ticket_pushed(&s_ring, ticket, &stamp))
            {
                break;
            }
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_wake, .seen = seen);
        }
        void *popped = NULL;
        embed_u64 seen_word = 0u;
        (void)embed_ticket_ring_popped(&s_turnstiles, address, &s_ring, stamp, &popped, &seen_word);
        const CarcerMovableEntry *const cell = popped;
        JobsCell job = {NULL, 0ULL};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .dst = &job, .bytes = sizeof(job)});
        char *const bytes = g_cli_guard->temporary_buf_alloc((job.size != 0ULL) ? job.size : 1ULL, g_cli_servers_owner);
        if ((bytes != NULL) && (job.size != 0ULL))
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = cell, .offset = sizeof(job), .dst = bytes, .bytes = (embed_word)job.size});
        }
        if (bytes != NULL)
        {
            EMBED_CALL(job.run, CliServersJobCfg, .job = bytes, .size = job.size);
        }
        g_cli_guard->persistent_buf_release(bytes, g_cli_servers_owner);
        (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_owner);
        (void)embed_ticket_ring_released(&s_turnstiles, &s_ring, stamp, &seen_word);
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    }
}

// a worker started where one of the eight has not: its place claimed by one compare-and-swap on the count, so each
// place starts once; a thread that could not start leaves its place empty, and the others serve
static void jobs_worker_started(void)
{
    for (;;)
    {
        embed_u32 count = EMBED_ATOMIC_LOAD(&s_started);
        if (count >= CLI_SERVERS_WORKERS)
        {
            return;
        }
        if (EMBED_ATOMIC_CAS_32(&s_started, &count, count + 1u))
        {
            s_worker_index[count] = count;
            s_thread[count] = (CliThread){jobs_worker, &s_worker_index[count], CLI_PRIORITY_WORK};
            (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread[count]);
            return;
        }
    }
}

int cli_servers_later(const CliServersRunCtx *args)
{
    const CarcerMovableEntry *const cell =
        g_cli_guard->movable_buf_alloc(sizeof(JobsCell) + args->size, g_cli_servers_owner);
    if (cell == NULL)
    {
        return 0;
    }
    const JobsCell head = {args->run, args->size};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
    if (args->size != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = cell, .offset = sizeof(head), .src = args->job, .bytes = (embed_word)args->size});
    }
    return EMBED_CALL(cli_servers_run_size_internal.later_dispatch, CliServersRunSizeCtx, .run = args->run,
                      .cell = cell, .size = args->size);
}

int cli_servers_later_dispatch(const CliServersRunSizeCtx *args)
{
    const CarcerMovableEntry *const cell = args->cell;
    if (cell == NULL)
    {
        return 0;
    }
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_ticket, 1u);
    // the slot of the ticket a lap back released first, which retains every live ticket within one lap of the ring, as
    // the ring's proofs need; a worker's release wakes the wait
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_wake);
        if (cli_ticket_room(&s_ring, ticket))
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_wake, .seen = seen);
    }
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the slot holds
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)cell;
    embed_u64 seen_word = 0u;
    (void)embed_ticket_ring_granted(&s_turnstiles, address, &s_ring, stamp, &seen_word);
    (void)embed_ticket_ring_pushed(&s_turnstiles, address, &s_ring, stamp, (void *)(embed_word)cell, &seen_word);
    jobs_worker_started();
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    return 1;
}

void cli_servers_jobs_positioned(void)
{
    s_ring.slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_SERVERS_JOBS);
}
