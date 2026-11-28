// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_load_limit.h
 * @brief How much of a shared machine a bench may take.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note This machine is not a cluster node. The desktop, the browser and the operating system live
 *       on it while a bench runs. A measurement that makes the machine unusable is a bad
 *       measurement, however good its numbers are.
 * @note Cores minus one is not enough on its own. Take a bench with fifteen of sixteen threads at
 *       normal priority and a four gigabyte working set. It starves the interactive session, even
 *       with one core nominally free. The contention is for memory bandwidth and scheduler turns,
 *       and not for cores.
 * @note Thus two measures, and not one. The worker count leaves real headroom. The process runs
 *       below normal priority, and anything that the user does preempts it. Below normal costs a
 *       long run almost nothing on an idle machine. An idle machine has no work of higher priority
 *       to give the turns to.
 */

#ifndef BENCH_LOAD_LIMIT_H
#define BENCH_LOAD_LIMIT_H

#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

/** @brief The processors the system runs threads on, as the system counts them, 0 where it does not say. */
static inline unsigned bench_processors(void)
{
#ifdef _WIN32
    return (unsigned)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    const long online = sysconf(_SC_NPROCESSORS_ONLN);
    return (online > 0l) ? (unsigned)online : 0u;
#endif
}

/**
 * @brief Drops this process below normal priority so interactive work preempts it.
 *
 * @note Called once at the top of a bench. The function ignores failure deliberately. A bench that
 *       cannot lower its own priority should still run, and it will simply be ruder.
 */
static inline void bench_lower_priority(void)
{
#ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
}

/**
 * @brief How many bytes a bench may allocate, measured once at launch.
 *
 * @return Bytes that the bench may take, or zero where the function cannot find the figure.
 * @note Eighty percent of the memory free at the start of the process, and not of the memory free
 *       now. A later sample would let a bench grow into memory that another process released, and
 *       then keep it. The limit would also depend on the time of the call.
 * @note A bench that wants more than this says so, and skips the pass. An allocation past the limit
 *       lets the machine page, and that defeats the measurement. A paged run produces a number with
 *       an untrustworthy time, and a desktop that nobody can use.
 */
static inline uint64_t bench_memory_budget(void)
{
#ifdef _WIN32
    static uint64_t settled = 0u;
    if (settled == 0u)
    {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status) != 0)
        {
            settled = (uint64_t)((double)status.ullAvailPhys * 0.80);
        }
    }
    return settled;
#else
    return 0u;
#endif
}

/**
 * @brief How many worker threads a bench should start on this machine.
 *
 * @param[in] requested Zero to choose, or an explicit count from the caller.
 * @return              Worker count, at least one.
 * @note The default keeps a quarter of the machine, rounded up to at least two cores, out of the
 *       hands of the bench. On sixteen logical cores, that is twelve workers, and not fifteen. It
 *       costs a fifth of the throughput. It is the difference between a slow machine and an unusable
 *       machine.
 * @note BENCH_THREADS overrides it. Thus a run on an idle machine can still take everything, with
 *       no edit to a file.
 */
static inline unsigned bench_worker_count(unsigned requested)
{
    if (requested > 0u)
    {
        return requested;
    }

    const char *const asked = getenv("BENCH_THREADS");
    if (asked != nullptr)
    {
        const unsigned long parsed = strtoul(asked, nullptr, 10);
        if (parsed > 0ul)
        {
            return (unsigned)parsed;
        }
    }

    const unsigned present = bench_processors();
    if (present <= 2u)
    {
        return 1u;
    }

    const unsigned reserved = (present / 4u) > 2u ? (present / 4u) : 2u;
    return present - reserved;
}

#endif
