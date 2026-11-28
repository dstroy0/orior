// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_ledger_core_decide.h: release, idle, start, shadow, admission and calls (tessera_ledger_core.h includes the
// parts in order)
#ifndef TESSERA_LEDGER_CORE_DECIDE_H
#define TESSERA_LEDGER_CORE_DECIDE_H

#include "tessera_ledger_core_heap.h"

TESSERA_CORE int tessera_core_history_save(TesseraLedger *ledger, const TesseraJob *job, unsigned long long now)
{
    TesseraHistory history;
    history.signum = job->request.signum;
    history.peak = job->peak;
    history.duration = now - job->started;
    return tessera_core_remember(ledger, &history);
}

TESSERA_CORE int tessera_core_release(TesseraLedger *ledger, unsigned long long identity, unsigned long long now,
                                      int finished)
{
    TesseraJob *const job = tessera_core_find(ledger, identity);
    if (job == NULL)
    {
        return 0;
    }
    const int history = (finished == 0) || (job->state != TESSERA_JOB_RUNNING) || (job->measures == 0ULL) ||
                        tessera_core_history_save(ledger, job, now);
    ledger->idle_microseconds = job->request.idle_microseconds;
    tessera_core_remove(ledger, job);
    if (ledger->job_count == 0ULL)
    {
        ledger->idle_since = now;
        return tessera_core_heap_push(ledger, tessera_core_sum(now, ledger->idle_microseconds), 0ULL,
                                      TESSERA_EVENT_IDLE) &&
               history;
    }
    return history;
}

TESSERA_CORE int tessera_core_idle(TesseraLedger *ledger, unsigned long long now, unsigned long long idle_microseconds)
{
    if (ledger->job_count != 0ULL)
    {
        return 0;
    }
    ledger->idle_since = now;
    ledger->idle_microseconds = idle_microseconds;
    return tessera_core_heap_push(ledger, tessera_core_sum(now, idle_microseconds), 0ULL, TESSERA_EVENT_IDLE);
}

TESSERA_CORE int tessera_core_start(TesseraLedger *ledger, TesseraJob *job, unsigned long long now, TesseraEvent *event)
{
    const TesseraHistory *const previous = tessera_core_history(ledger, &job->request.signum);
    const unsigned long long next_sweep = tessera_core_sum(now, job->request.sweep_microseconds);
    if (!tessera_core_heap_push(ledger, next_sweep, job->identity, TESSERA_EVENT_SWEEP))
    {
        return 0;
    }
    job->state = TESSERA_JOB_RUNNING;
    job->reservation = tessera_core_wants(ledger, job);
    // Until its first sweep the job uses what its process allocated as it requested. The device's measure already
    // counts those bytes, and the headroom owes the device only the bytes that the sweep does not find yet
    job->used = job->request.located;
    job->peak = 0ULL;
    job->measures = 0ULL;
    job->started = now;
    job->expected_end = (previous != NULL) ? tessera_core_sum(now, previous->duration) : TESSERA_FOREVER;
    job->next_sweep = next_sweep;
    *event = tessera_core_event(TESSERA_EVENT_ADMITTED, job->identity, job->reservation,
                                (previous != NULL) ? previous->peak : 0ULL);
    return 1;
}

// The time at which the running jobs of `device` that end free `wanted` past `headroom`, into `shadow`, and what stays
// over then, into `spare`. 0 where the jobs that run there never free it
TESSERA_CORE int tessera_core_shadow(const TesseraLedger *ledger, unsigned int device, unsigned long long wanted,
                                     long long headroom, unsigned long long *shadow, unsigned long long *spare)
{
    *shadow = 0ULL;
    *spare = 0ULL;
    long long available = headroom;
    unsigned long long after = 0ULL;
    for (;;)
    {
        unsigned long long soonest = TESSERA_FOREVER;
        for (unsigned long long at = 0ULL; at < ledger->job_count; at += 1ULL)
        {
            const TesseraJob *const job = &ledger->jobs[at];
            if ((job->state == TESSERA_JOB_RUNNING) && (job->request.device == device) && (job->expected_end > after) &&
                (job->expected_end < soonest))
            {
                soonest = job->expected_end;
            }
        }
        if (soonest == TESSERA_FOREVER)
        {
            return 0;
        }
        for (unsigned long long at = 0ULL; at < ledger->job_count; at += 1ULL)
        {
            const TesseraJob *const job = &ledger->jobs[at];
            if ((job->state == TESSERA_JOB_RUNNING) && (job->request.device == device) && (job->expected_end == soonest))
            {
                // a job's blocked bytes are a device's bytes, below two to the sixty-third
                available += (long long)tessera_core_blocked(job);
            }
        }
        after = soonest;
        // the wanted bytes are a device's bytes, below two to the sixty-third
        if (available >= (long long)wanted)
        {
            *shadow = soonest;
            // the capacity is at least the wanted bytes here, and the difference is not negative
            *spare = (unsigned long long)(available - (long long)wanted);
            return 1;
        }
    }
}

// What a job that waits may draw on the room of its parent, at max `needs`. 0 where it names no parent, or its parent
// runs on no device of its own
TESSERA_CORE unsigned long long tessera_core_lendable(const TesseraLedger *ledger, const TesseraJob *job,
                                                      unsigned long long needs)
{
    const TesseraJob *const parent =
        (job->request.parent != 0ULL) ? tessera_core_find(ledger, job->request.parent) : NULL;
    if ((parent == NULL) || (parent->state != TESSERA_JOB_RUNNING) || (parent->request.device != job->request.device))
    {
        return 0ULL;
    }
    const unsigned long long room = tessera_core_room(ledger, parent);
    return (room < needs) ? room : needs;
}

// the place of a device in one pass of admission
#define TESSERA_CORE_OPEN 0u
#define TESSERA_CORE_HELD 1u
#define TESSERA_CORE_SHUT 2u

// One pass over the system's one queue, in order. A job draws on its parent's room first, and the rest of what it needs
// is what must fit the device's headroom. On an open device each job that waits and fits starts. The first job that
// does not fit holds its device.
//
// Behind that job, only a job with a tracked run starts. It must end before the first job's room frees, or fit in what
// is spare then. A device whose running jobs never free that room shuts for the pass. A job that holds one device holds
// back no job of another device
TESSERA_CORE unsigned long long tessera_core_admit(TesseraLedger *ledger, unsigned long long now, TesseraEvent *events,
                                                   unsigned long long start_limit)
{
    unsigned int place[TESSERA_LEDGER_DEVICES];
    unsigned long long shadow[TESSERA_LEDGER_DEVICES];
    unsigned long long spare[TESSERA_LEDGER_DEVICES];
    for (unsigned int device = 0u; device < TESSERA_LEDGER_DEVICES; device += 1u)
    {
        place[device] = TESSERA_CORE_OPEN;
        shadow[device] = 0ULL;
        spare[device] = 0ULL;
    }
    unsigned long long made = 0ULL;
    for (unsigned long long at = 0ULL; (made < start_limit) && (at < ledger->job_count); at += 1ULL)
    {
        TesseraJob *const job = &ledger->jobs[at];
        const unsigned int device = job->request.device;
        if ((job->state != TESSERA_JOB_WAITING) || (device >= TESSERA_LEDGER_DEVICES) ||
            (place[device] == TESSERA_CORE_SHUT))
        {
            continue;
        }
        const unsigned long long needs = tessera_core_needs(ledger, job);
        const unsigned long long lent = tessera_core_lendable(ledger, job, needs);
        const unsigned long long rest = needs - lent;
        const long long headroom = tessera_core_headroom(ledger, device);
        // the needed bytes are a device's bytes, below two to the sixty-third
        const int fits = (long long)rest <= headroom;
        if (place[device] == TESSERA_CORE_OPEN)
        {
            if (fits)
            {
                if (!tessera_core_start(ledger, job, now, &events[made]))
                {
                    return made;
                }
                job->lent = lent;
                made += 1ULL;
                continue;
            }
            place[device] = tessera_core_shadow(ledger, device, rest, headroom, &shadow[device], &spare[device])
                                ? TESSERA_CORE_HELD
                                : TESSERA_CORE_SHUT;
            continue;
        }
        const TesseraHistory *const previous = tessera_core_history(ledger, &job->request.signum);
        if ((previous == NULL) || !fits)
        {
            continue;
        }
        const int before_shadow = tessera_core_sum(now, previous->duration) <= shadow[device];
        const int in_spare = rest <= spare[device];
        if (!before_shadow && !in_spare)
        {
            continue;
        }
        spare[device] -= (!before_shadow) ? rest : 0ULL;
        if (!tessera_core_start(ledger, job, now, &events[made]))
        {
            return made;
        }
        job->lent = lent;
        made += 1ULL;
    }
    return made;
}

TESSERA_CORE int tessera_core_next(const TesseraLedger *ledger, TesseraDeadline *root)
{
    if (ledger->heap_count == 0ULL)
    {
        return 0;
    }
    *root = ledger->heap[0];
    return 1;
}

TESSERA_CORE int tessera_core_fire(TesseraLedger *ledger, unsigned long long now, TesseraEvent *event)
{
    *event = tessera_core_event(TESSERA_EVENT_NONE, 0ULL, 0ULL, 0ULL);
    while ((ledger->heap_count != 0ULL) && (ledger->heap[0].when <= now))
    {
        const TesseraDeadline due = ledger->heap[0];
        tessera_core_heap_pop(ledger);
        if (due.type == TESSERA_EVENT_IDLE)
        {
            if ((ledger->job_count == 0ULL) &&
                (tessera_core_sum(ledger->idle_since, ledger->idle_microseconds) == due.when))
            {
                event->type = TESSERA_EVENT_IDLE;
                return 1;
            }
            continue;
        }
        TesseraJob *const job = tessera_core_find(ledger, due.identity);
        if (job == NULL)
        {
            continue;
        }
        if ((due.type == TESSERA_EVENT_SWEEP) && (job->state == TESSERA_JOB_RUNNING) && (job->next_sweep == due.when))
        {
            job->next_sweep = tessera_core_sum(due.when, job->request.sweep_microseconds);
            event->type = TESSERA_EVENT_SWEEP;
            event->identity = job->identity;
            return tessera_core_heap_push(ledger, job->next_sweep, job->identity, TESSERA_EVENT_SWEEP);
        }
        if ((due.type == TESSERA_EVENT_LOST) && (job->state == TESSERA_JOB_OVER_BUDGET) &&
            (job->grace_until == due.when))
        {
            *event = tessera_core_event(TESSERA_EVENT_LOST, job->identity, job->request.declared, 0ULL);
            tessera_core_remove(ledger, job);
            return 1;
        }
    }
    return 0;
}

// One call of the ledger and its answer. Where the ledger lacks room for the max that the call could add, the answer is
// an error with nothing changed. Otherwise it is the decision that the call names, with the headroom after it
TESSERA_CORE void tessera_core_call(TesseraLedger *ledger, const TesseraCall *call, TesseraAnswer *answer)
{
    answer->result = 0ULL;
    answer->full = 0;
    answer->identity = 0ULL;
    answer->root.when = 0ULL;
    answer->root.identity = 0ULL;
    answer->root.type = TESSERA_EVENT_NONE;
    for (unsigned int at = 0u; at < TESSERA_CALL_EVENTS; at += 1u)
    {
        answer->events[at] = tessera_core_event(TESSERA_EVENT_NONE, 0ULL, 0ULL, 0ULL);
    }
    const TesseraCapacity needed = tessera_core_capacity(ledger, call->type);
    if (!tessera_core_fits(ledger, &needed))
    {
        answer->full = 1;
        answer->headroom = tessera_core_headroom(ledger, call->device);
        return;
    }
    // the answer's word widens each decision's 1 or 0, and an admit's count of jobs started converts exactly
    switch (call->type)
    {
    case TESSERA_CALL_DEVICE:
        tessera_core_device(ledger, call->device, call->capacity, call->in_use);
        answer->result = 1ULL;
        break;
    case TESSERA_CALL_SUBMIT:
        answer->result = (unsigned long long)tessera_core_submit(ledger, &call->request, call->now, &answer->identity,
                                                                 &answer->events[0]);
        break;
    case TESSERA_CALL_OVERRIDE:
        answer->result = (unsigned long long)tessera_core_override(ledger, call->identity, call->now);
        break;
    case TESSERA_CALL_MEASURE:
        answer->result =
            (unsigned long long)tessera_core_measure(ledger, call->identity, call->used, &answer->events[0]);
        break;
    case TESSERA_CALL_RELEASE:
        answer->result = (unsigned long long)tessera_core_release(ledger, call->identity, call->now, call->finished);
        break;
    case TESSERA_CALL_ADMIT:
        answer->result =
            tessera_core_admit(ledger, call->now, answer->events,
                               (call->start_limit < TESSERA_CALL_EVENTS) ? call->start_limit : TESSERA_CALL_EVENTS);
        break;
    case TESSERA_CALL_FIRE:
        answer->result = (unsigned long long)tessera_core_fire(ledger, call->now, &answer->events[0]);
        break;
    case TESSERA_CALL_IDLE:
        answer->result = (unsigned long long)tessera_core_idle(ledger, call->now, call->idle_microseconds);
        break;
    case TESSERA_CALL_REMEMBER:
        answer->result = (unsigned long long)tessera_core_remember(ledger, &call->history);
        break;
    case TESSERA_CALL_NEXT:
        answer->result = (unsigned long long)tessera_core_next(ledger, &answer->root);
        break;
    default:
        break;
    }
    answer->headroom = tessera_core_headroom(ledger, call->device);
}

#endif
