// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_job_ring.h
 * @brief The jobs of a bench on the scheduler of embedded_types: a ticket ring of jobs behind a set of turnstiles,
 *        and the host threads that pop them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The grantor is the thread that calls bench_jobs_run. It takes a ticket for each job from its count, and grants
 *       and pushes the job inside a pass. Then host_threads_run starts the workers.
 * @note A worker takes the next ticket with one fetch-add, and pops that ticket inside a pass at its own address. A pop
 *       that lands gives the job, and the worker runs it. A worker that takes a ticket past the last job ends.
 * @note After every worker ends, the grantor drains the turnstiles and releases each slot. The next run then grants on
 *       the released words. The ticket count goes on across the runs, so no two live stamps share a ticket.
 * @note A run is one at a time. The ring, its turnstiles and its jobs are in one static record, zeros at rest.
 * @warning A run that the ring refuses writes its reason to the error stream and ends the program. A refused grant, a
 *          refused pop, a refused release and a thread that the system did not start each stop it.
 */

#ifndef BENCH_JOB_RING_H
#define BENCH_JOB_RING_H

#include "embed_ticket_ring.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "host_threads.h"
#ifdef __cplusplus
}
#endif

/** @brief The slots of the ring as a shift: 2^8 slots, and so 256 jobs in a run at max. */
#define BENCH_JOBS_SHIFT 8u

/** @brief The jobs of a run at max, one slot to each job. */
#define BENCH_JOBS_MAX (1u << BENCH_JOBS_SHIFT)

/** @brief The share of one job: the context of the run, and the index of the job from 0. */
typedef void (*BenchShare)(void *context, unsigned job);

/** @brief One job: the share, its context, and the index that the share gets. */
typedef struct
{
    BenchShare share; /**< The share that the job runs. */
    void *context;    /**< The context of the share [BORROWS]. */
    unsigned index;   /**< The index of the job in its run. */
} BenchJob;

/** @brief The job ring: its slots, the ring, its turnstiles, the jobs, and the tickets of the run. */
typedef struct
{
    EmbedTicketRingSlot slots[BENCH_JOBS_MAX]; /**< The slots of the ring. */
    EmbedTicketRing ring;                      /**< The ring over the slots. */
    EmbedTurnstiles turnstiles;                /**< The turnstiles of each grant, push and pop. */
    BenchJob jobs[BENCH_JOBS_MAX];             /**< The job of each slot. */
    embed_word epoch;                          /**< The epoch of each stamp of the run. */
    embed_word first_ticket;                   /**< The ticket of the first job of the run. */
    embed_word next_ticket;                    /**< The ticket of the first job of the next run. */
    unsigned job_count;                        /**< The jobs of the run. */
    embed_atomic_u32 taken;                    /**< The tickets that the workers took. */
    embed_atomic_u32 refused;                  /**< The pops that the ring refused. */
} BenchJobRing;

/** @brief The one job ring of the bench. */
static BenchJobRing s_bench_job_ring;

/**
 * @brief The stamp of a ticket of the run.
 *
 * @param[in] ticket The ticket.
 * @return           The stamp, with the epoch of the run and the flag bits clear.
 */
static inline embed_word bench_job_stamp(embed_word ticket)
{
    return EMBED_CALL(embed_ticket_stamp, EmbedTicketStampCfg, s_bench_job_ring.epoch, ticket);
}

/** @brief The work of one worker: each ticket that it takes popped inside a pass, and the job of the pop run. */
static inline void bench_jobs_worked(void)
{
    BenchJobRing *const run = &s_bench_job_ring;
    embed_word found = 0u;
    // Explicit cast carries the address of a local of this worker as the place that its passes enter at
    const embed_atomic_uptr_value worker = (embed_atomic_uptr_value)(uintptr_t)&found;

    for (;;)
    {
        const unsigned taken = EMBED_CALL(embed_atomic_u32_fetch_add, EmbedAtomicU32FetchAddCfg, &run->taken, 1u);

        if (taken >= run->job_count)
        {
            return;
        }
        void *job = NULL;

        if (EMBED_CALL(embed_ticket_ring_pass_pop, EmbedTicketRingPassPopCfg, &run->turnstiles, worker, &run->ring,
                       bench_job_stamp(run->first_ticket + taken), &job, &found))
        {
            const BenchJob *const held = (const BenchJob *)job;

            held->share(held->context, held->index);
        }
        else
        {
            (void)EMBED_CALL(embed_atomic_u32_fetch_add, EmbedAtomicU32FetchAddCfg, &run->refused, 1u);
        }
    }
}

/**
 * @brief The end of the program on a run that the ring refused.
 *
 * @param[in] reason What the ring refused.
 */
static inline void bench_jobs_refused(const char *reason)
{
    fprintf(stderr, "bench_jobs_run: %s\n", reason);
    exit(1);
}

/**
 * @brief Runs share over jobs jobs on threads workers, and waits for every worker to end.
 *
 * @param[in] share   The share of one job.
 * @param[in] context The context of the share [BORROWS].
 * @param[in] jobs    The jobs, from 1 to BENCH_JOBS_MAX. The share of each gets its index from 0.
 * @param[in] threads The workers to start, at max HOST_THREADS_MAX. More workers than that start HOST_THREADS_MAX.
 * @note The jobs, and not the workers, split the work. So the result of a run does not change with the workers.
 */
static inline void bench_jobs_run(BenchShare share, void *context, unsigned jobs, unsigned threads)
{
    BenchJobRing *const run = &s_bench_job_ring;
    embed_word found = 0u;
    // Explicit cast carries the address of a local of the grantor as the place that its passes enter at
    const embed_atomic_uptr_value grantor = (embed_atomic_uptr_value)(uintptr_t)&found;

    if ((jobs == 0u) || (jobs > BENCH_JOBS_MAX))
    {
        bench_jobs_refused("the jobs of a run are from 1 to BENCH_JOBS_MAX");
    }
    EMBED_CALL(embed_ticket_ring_begin, EmbedTicketRingBeginCfg, &run->ring, run->slots, BENCH_JOBS_SHIFT);
    // The epoch moves every 2^(EMBED_WORD_BITS / 2) ns, far past the life of a stamp of one run
    run->epoch = embed_clock_ns() >> (EMBED_WORD_BITS / 2u);
    run->first_ticket = run->next_ticket;
    run->next_ticket += jobs;
    run->job_count = jobs;
    EMBED_CALL(embed_atomic_u32_store, EmbedAtomicU32StoreCfg, &run->taken, 0u);
    EMBED_CALL(embed_atomic_u32_store, EmbedAtomicU32StoreCfg, &run->refused, 0u);
    for (unsigned index = 0u; index < jobs; index += 1u)
    {
        const embed_word stamp = bench_job_stamp(run->first_ticket + index);

        run->jobs[index].share = share;
        run->jobs[index].context = context;
        run->jobs[index].index = index;
        if (!EMBED_CALL(embed_ticket_ring_pass_grant, EmbedTicketRingPassGrantCfg, &run->turnstiles, grantor,
                        &run->ring, stamp, &found) ||
            !EMBED_CALL(embed_ticket_ring_pass_push, EmbedTicketRingPassPushCfg, &run->turnstiles, grantor, &run->ring,
                        stamp, &run->jobs[index], &found))
        {
            bench_jobs_refused("the ring refused the grant or the push of a job");
        }
    }
    if (!host_threads_run(bench_jobs_worked, (threads > HOST_THREADS_MAX) ? HOST_THREADS_MAX : threads))
    {
        bench_jobs_refused("the system did not start a worker");
    }
    if (EMBED_CALL(embed_atomic_u32_load, EmbedAtomicU32LoadCfg, &run->refused) != 0u)
    {
        bench_jobs_refused("the ring refused the pop of a job");
    }
    for (unsigned index = 0u; index < jobs; index += 1u)
    {
        if (!EMBED_CALL(embed_ticket_ring_drain_release, EmbedTicketRingDrainReleaseCfg, &run->turnstiles, &run->ring,
                        bench_job_stamp(run->first_ticket + index), &found))
        {
            bench_jobs_refused("the ring refused the release of a job");
        }
    }
}

#endif
