// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_ledger_core_heap.h: events, sums, the heap, headroom and history (tessera_ledger_core.h includes the parts in
// order)
#ifndef TESSERA_LEDGER_CORE_HEAP_H
#define TESSERA_LEDGER_CORE_HEAP_H

// the ledger decisions of tessera, as one source that the host and the device both compile. There are two tesseras.
// The daemon of the host (tessera_daemon_*.c) runs them through tessera_ledger.c over arrays that it grows, with no
// CUDA context of its own. The tessera of the device (tessera.cu) runs them in one thread, over arrays that it
// arranged in the memory of the device.
//
// The core never grows an array. Each decision gives an error where an array that it needs is full, as that of the host
// did where an array did not grow. Where the ledger has no room for the max that a call could add, tessera_core_call
// gives an error before it changes anything. Then the caller grows the arrays and makes the call again, which decides
// the same

#include "tessera_ledger.h"

#include <stddef.h>

#if defined(__CUDACC__)
#define TESSERA_CORE __host__ __device__ static inline
#else
#define TESSERA_CORE static inline
#endif

#define TESSERA_FOREVER 0xFFFFFFFFFFFFFFFFULL

// the max a call can add to each of a ledger's arrays: its jobs, its tracked peaks and its deadlines
typedef struct
{
    unsigned long long jobs;
    unsigned long long history;
    unsigned long long heap;
} TesseraCapacity;

TESSERA_CORE TesseraEvent tessera_core_event(TesseraEventType type, unsigned long long identity,
                                             unsigned long long bytes, unsigned long long measured)
{
    const TesseraEvent event = {type, identity, bytes, measured};
    return event;
}

TESSERA_CORE unsigned long long tessera_core_sum(unsigned long long left, unsigned long long right)
{
    return (right > (TESSERA_FOREVER - left)) ? TESSERA_FOREVER : (left + right);
}

// 1 where two signums are the same byte for byte
TESSERA_CORE int tessera_core_signum_same(const EngineSignum *left, const EngineSignum *right)
{
    int same = 1;
    for (unsigned int at = 0u; at < ENGINE_SIGNUM_BYTES; at += 1u)
    {
        same = same && (left->bytes[at] == right->bytes[at]);
    }
    return same;
}

// the arrays that a call of `type` on a ledger can take:
//
// - a job and a deadline for a submit
// - a tracked peak and a deadline for a release
// - a deadline for an idle
// - a tracked peak for a remember
// - a deadline for each job that an admit could start
TESSERA_CORE TesseraCapacity tessera_core_capacity(const TesseraLedger *ledger, TesseraCallType type)
{
    TesseraCapacity needed = {ledger->job_count, ledger->history_count, ledger->heap_count};
    needed.jobs += (type == TESSERA_CALL_SUBMIT) ? 1ULL : 0ULL;
    needed.history += ((type == TESSERA_CALL_RELEASE) || (type == TESSERA_CALL_REMEMBER)) ? 1ULL : 0ULL;
    needed.heap += ((type == TESSERA_CALL_SUBMIT) || (type == TESSERA_CALL_RELEASE) || (type == TESSERA_CALL_IDLE))
                       ? 1ULL
                       : ((type == TESSERA_CALL_ADMIT) ? ledger->job_count : 0ULL);
    return needed;
}

// 1 where the ledger's arrays have room for `needed`
TESSERA_CORE int tessera_core_fits(const TesseraLedger *ledger, const TesseraCapacity *needed)
{
    return (needed->jobs <= ledger->job_capacity) && (needed->history <= ledger->history_capacity) &&
           (needed->heap <= ledger->heap_capacity);
}

TESSERA_CORE int tessera_core_heap_push(TesseraLedger *ledger, unsigned long long when, unsigned long long identity,
                                        TesseraEventType type)
{
    if (ledger->heap_count >= ledger->heap_capacity)
    {
        return 0;
    }
    unsigned long long at = ledger->heap_count;
    ledger->heap_count += 1ULL;
    while (at != 0ULL)
    {
        const unsigned long long parent = (at - 1ULL) / 2ULL;
        if (ledger->heap[parent].when <= when)
        {
            break;
        }
        ledger->heap[at] = ledger->heap[parent];
        at = parent;
    }
    ledger->heap[at].when = when;
    ledger->heap[at].identity = identity;
    ledger->heap[at].type = type;
    return 1;
}

TESSERA_CORE void tessera_core_heap_pop(TesseraLedger *ledger)
{
    ledger->heap_count -= 1ULL;
    const TesseraDeadline last = ledger->heap[ledger->heap_count];
    unsigned long long at = 0ULL;
    for (;;)
    {
        const unsigned long long left = (2ULL * at) + 1ULL;
        if (left >= ledger->heap_count)
        {
            break;
        }
        const unsigned long long right = left + 1ULL;
        const unsigned long long least =
            ((right < ledger->heap_count) && (ledger->heap[right].when < ledger->heap[left].when)) ? right : left;
        if (last.when <= ledger->heap[least].when)
        {
            break;
        }
        ledger->heap[at] = ledger->heap[least];
        at = least;
    }
    if (ledger->heap_count != 0ULL)
    {
        ledger->heap[at] = last;
    }
}

TESSERA_CORE TesseraJob *tessera_core_find(const TesseraLedger *ledger, unsigned long long identity)
{
    for (unsigned long long at = 0ULL; at < ledger->job_count; at += 1ULL)
    {
        if (ledger->jobs[at].identity == identity)
        {
            return &ledger->jobs[at];
        }
    }
    return NULL;
}

TESSERA_CORE void tessera_core_remove(TesseraLedger *ledger, const TesseraJob *job)
{
    // the job points into the array, and its distance from the start is its index
    const unsigned long long at = (unsigned long long)(job - ledger->jobs);
    for (unsigned long long next = at + 1ULL; next < ledger->job_count; next += 1ULL)
    {
        ledger->jobs[next - 1ULL] = ledger->jobs[next];
    }
    ledger->job_count -= 1ULL;
}

TESSERA_CORE unsigned long long tessera_core_blocked(const TesseraJob *job)
{
    return (job->reservation > job->used) ? job->reservation : job->used;
}

TESSERA_CORE unsigned long long tessera_core_unallocated(const TesseraJob *job)
{
    return (job->reservation > job->used) ? (job->reservation - job->used) : 0ULL;
}

TESSERA_CORE void tessera_core_device(TesseraLedger *ledger, unsigned long long capacity, unsigned long long in_use)
{
    ledger->capacity = capacity;
    ledger->in_use = in_use;
}

TESSERA_CORE long long tessera_core_headroom(const TesseraLedger *ledger)
{
    unsigned long long owed = 0ULL;
    for (unsigned long long at = 0ULL; at < ledger->job_count; at += 1ULL)
    {
        if (ledger->jobs[at].state == TESSERA_JOB_RUNNING)
        {
            owed = tessera_core_sum(owed, tessera_core_unallocated(&ledger->jobs[at]));
        }
    }
    const unsigned long long taken = tessera_core_sum(ledger->in_use, owed);
    // a device's bytes and every sum here stay below two to the sixty-third, and each difference is exact signed
    return (taken <= ledger->capacity) ? (long long)(ledger->capacity - taken) : -(long long)(taken - ledger->capacity);
}

TESSERA_CORE const TesseraHistory *tessera_core_history(const TesseraLedger *ledger, const EngineSignum *signum)
{
    for (unsigned long long at = 0ULL; at < ledger->history_count; at += 1ULL)
    {
        if (tessera_core_signum_same(&ledger->history[at].signum, signum))
        {
            return &ledger->history[at];
        }
    }
    return NULL;
}

// the ledger reserves for a job its whole declaration. That is the bytes that its process allocated as it requested,
// and the bytes that it declares on top. Or it reserves the tracked peak of its signum, when that is more. The ledger
// reserves the capacity that the job will take from its start, and not only after a sweep sees it grow
TESSERA_CORE unsigned long long tessera_core_wants(const TesseraLedger *ledger, const TesseraJob *job)
{
    const TesseraHistory *const previous = tessera_core_history(ledger, &job->request.signum);
    const unsigned long long total = tessera_core_sum(job->request.located, job->request.declared);
    return ((previous != NULL) && (previous->peak > total)) ? previous->peak : total;
}

// the bytes that a job which waits must still find on the device. That is what it wants, less what its process already
// allocated there, which the measured use of the device counts
TESSERA_CORE unsigned long long tessera_core_needs(const TesseraLedger *ledger, const TesseraJob *job)
{
    const unsigned long long wants = tessera_core_wants(ledger, job);
    return (wants > job->request.located) ? (wants - job->request.located) : 0ULL;
}

TESSERA_CORE int tessera_core_submit(TesseraLedger *ledger, const TesseraJobRequest *request, unsigned long long now,
                                     unsigned long long *identity, TesseraEvent *event)
{
    *event = tessera_core_event(TESSERA_EVENT_NONE, 0ULL, 0ULL, 0ULL);
    if ((request->declared == 0ULL) || (ledger->job_count >= ledger->job_capacity))
    {
        return 0;
    }
    const TesseraHistory *const previous = tessera_core_history(ledger, &request->signum);
    const int over = (previous != NULL) && (request->declared > previous->peak) && (request->override_budget == 0u);
    TesseraJob *const job = &ledger->jobs[ledger->job_count];
    job->identity = ledger->next_identity;
    job->request = *request;
    job->state = over ? TESSERA_JOB_OVER_BUDGET : TESSERA_JOB_WAITING;
    job->reservation = 0ULL;
    job->used = 0ULL;
    job->peak = 0ULL;
    job->measures = 0ULL;
    job->submitted = now;
    job->started = 0ULL;
    job->expected_end = 0ULL;
    job->grace_until = over ? tessera_core_sum(now, request->grace_microseconds) : 0ULL;
    job->next_sweep = 0ULL;
    if (over && !tessera_core_heap_push(ledger, job->grace_until, job->identity, TESSERA_EVENT_LOST))
    {
        return 0;
    }
    ledger->job_count += 1ULL;
    ledger->next_identity += 1ULL;
    *identity = job->identity;
    *event = tessera_core_event(over ? TESSERA_EVENT_OVER_BUDGET : TESSERA_EVENT_NONE, job->identity, request->declared,
                                (previous != NULL) ? previous->peak : 0ULL);
    return 1;
}

TESSERA_CORE int tessera_core_override(TesseraLedger *ledger, unsigned long long identity, unsigned long long now)
{
    TesseraJob *const job = tessera_core_find(ledger, identity);
    if ((job == NULL) || (job->state != TESSERA_JOB_OVER_BUDGET) || (now > job->grace_until))
    {
        return 0;
    }
    job->state = TESSERA_JOB_WAITING;
    job->request.override_budget = 1u;
    return 1;
}

TESSERA_CORE int tessera_core_measure(TesseraLedger *ledger, unsigned long long identity, unsigned long long used,
                                      TesseraEvent *event)
{
    *event = tessera_core_event(TESSERA_EVENT_NONE, 0ULL, 0ULL, 0ULL);
    TesseraJob *const job = tessera_core_find(ledger, identity);
    if ((job == NULL) || (job->state != TESSERA_JOB_RUNNING))
    {
        return 0;
    }
    job->used = used;
    job->peak = (used > job->peak) ? used : job->peak;
    job->measures += 1ULL;
    if (used > job->reservation)
    {
        job->reservation = used;
        *event = tessera_core_event(TESSERA_EVENT_GREW, identity, job->request.declared, used);
    }
    return 1;
}

TESSERA_CORE int tessera_core_remember(TesseraLedger *ledger, const TesseraHistory *entry)
{
    for (unsigned long long at = 0ULL; at < ledger->history_count; at += 1ULL)
    {
        if (tessera_core_signum_same(&ledger->history[at].signum, &entry->signum))
        {
            ledger->history[at] = *entry;
            return 1;
        }
    }
    if (ledger->history_count >= ledger->history_capacity)
    {
        return 0;
    }
    ledger->history[ledger->history_count] = *entry;
    ledger->history_count += 1ULL;
    return 1;
}

#endif
