// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TESSERA_LEDGER_H
#define TESSERA_LEDGER_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        TESSERA_JOB_WAITING = 0,
        TESSERA_JOB_OVER_BUDGET = 1,
        TESSERA_JOB_RUNNING = 2
    } TesseraJobState;

    typedef enum
    {
        TESSERA_EVENT_NONE = 0,
        TESSERA_EVENT_ADMITTED = 1,
        TESSERA_EVENT_OVER_BUDGET = 2,
        TESSERA_EVENT_GREW = 3,
        TESSERA_EVENT_LOST = 4,
        TESSERA_EVENT_SWEEP = 5,
        TESSERA_EVENT_IDLE = 6
    } TesseraEventType;

// the max devices that one ledger gates: the host's processors, and every GPU of the system
#define TESSERA_LEDGER_DEVICES 17u

    // A job as it requests. `device` is the ledger's index of the device that it runs on. `declared` is the bytes the
    // job states it will take there. `located` is the bytes its process already allocated on the device as it
    // requests, as the daemon measures them. They hold its CUDA context and what it kept from an earlier job.
    //
    // The job's whole declaration is the two together, and the device's measured use already counts the located
    // bytes. `parent` is the running job of an ancestor process on the same device, 0 for none. The job draws on that
    // job's room first, since its work is a part of the parent's
    typedef struct
    {
        EngineSignum signum;
        unsigned long long declared;
        unsigned long long grace_microseconds;
        unsigned long long sweep_microseconds;
        unsigned long long idle_microseconds;
        unsigned int override_budget;
        unsigned int device;
        unsigned long long located;
        unsigned long long parent;
    } TesseraJobRequest;

    typedef struct
    {
        unsigned long long identity;
        TesseraJobRequest request;
        TesseraJobState state;
        unsigned long long reservation;
        unsigned long long used;
        unsigned long long peak;
        unsigned long long measures;
        unsigned long long submitted;
        unsigned long long started;
        unsigned long long expected_end;
        unsigned long long grace_until;
        unsigned long long next_sweep;
        // the part of its reservation that it drew on its parent's room, counted once while both run
        unsigned long long lent;
    } TesseraJob;

    typedef struct
    {
        EngineSignum signum;
        unsigned long long peak;
        unsigned long long duration;
    } TesseraHistory;

    typedef struct
    {
        unsigned long long when;
        unsigned long long identity;
        TesseraEventType type;
    } TesseraDeadline;

    typedef struct
    {
        TesseraEventType type;
        unsigned long long identity;
        unsigned long long bytes;
        unsigned long long measured;
    } TesseraEvent;

    // The system's one ledger. It holds each device's capacity and measured use by its index, and every job of every
    // device in one queue. Then it holds the tracked peaks and the deadlines
    typedef struct
    {
        unsigned long long capacity[TESSERA_LEDGER_DEVICES];
        unsigned long long in_use[TESSERA_LEDGER_DEVICES];
        TesseraJob *jobs;
        unsigned long long job_count;
        unsigned long long job_capacity;
        TesseraHistory *history;
        unsigned long long history_count;
        unsigned long long history_capacity;
        TesseraDeadline *heap;
        unsigned long long heap_count;
        unsigned long long heap_capacity;
        unsigned long long next_identity;
        unsigned long long idle_since;
        unsigned long long idle_microseconds;
    } TesseraLedger;

// the max events of the answer to one call: an admit starts at max this many jobs in one call
#define TESSERA_CALL_EVENTS 16u

    // the ledger's decisions, each one call: the host daemon's ledger and the device's tessera (tessera_device.h) make
    // the same calls through one source (tessera_ledger_core.h)
    typedef enum
    {
        TESSERA_CALL_DEVICE = 0,
        TESSERA_CALL_SUBMIT = 1,
        TESSERA_CALL_OVERRIDE = 2,
        TESSERA_CALL_MEASURE = 3,
        TESSERA_CALL_RELEASE = 4,
        TESSERA_CALL_ADMIT = 5,
        TESSERA_CALL_FIRE = 6,
        TESSERA_CALL_IDLE = 7,
        TESSERA_CALL_REMEMBER = 8,
        TESSERA_CALL_NEXT = 9
    } TesseraCallType;

    // one call and what it reads:
    //
    // - the time, and the job that it names
    // - the device by its index, with its capacity and use (DEVICE), and the answer gives its headroom
    // - the bytes that the daemon measured a job to use (MEASURE)
    // - 1 where a job finished (RELEASE)
    // - the events that an admit can make, at max TESSERA_CALL_EVENTS (ADMIT)
    // - the idle time (IDLE), the job requested (SUBMIT), and the peak tracked (REMEMBER)
    typedef struct
    {
        TesseraCallType type;
        unsigned long long now;
        unsigned long long identity;
        unsigned int device;
        unsigned long long capacity;
        unsigned long long in_use;
        unsigned long long used;
        int finished;
        unsigned long long start_limit;
        unsigned long long idle_microseconds;
        TesseraJobRequest request;
        TesseraHistory history;
    } TesseraCall;

    // the answer of a call:
    //
    // - what the ledger function returned (for ADMIT, the jobs that it started)
    // - 1 where the ledger had no room for the max that the call could add, and nothing changed
    // - the identity that a submit gave, and the headroom after the call
    // - the deadline due first (NEXT), and the events that the call made
    typedef struct
    {
        unsigned long long result;
        int full;
        unsigned long long identity;
        long long headroom;
        TesseraDeadline root;
        TesseraEvent events[TESSERA_CALL_EVENTS];
    } TesseraAnswer;

    int tessera_ledger_open(TesseraLedger *ledger);

    void tessera_ledger_close(TesseraLedger *ledger);

    // the capacity and the measured use of the device at `device`
    void tessera_ledger_device(TesseraLedger *ledger, unsigned int device, unsigned long long capacity,
                               unsigned long long in_use);

    // what stays for the device at `device` past its use and the unallocated reservations of its running jobs
    long long tessera_ledger_headroom(const TesseraLedger *ledger, unsigned int device);

    int tessera_ledger_submit(TesseraLedger *ledger, const TesseraJobRequest *request, unsigned long long now,
                              unsigned long long *identity, TesseraEvent *event);

    int tessera_ledger_override(TesseraLedger *ledger, unsigned long long identity, unsigned long long now);

    int tessera_ledger_measure(TesseraLedger *ledger, unsigned long long identity, unsigned long long used,
                               TesseraEvent *event);

    int tessera_ledger_release(TesseraLedger *ledger, unsigned long long identity, unsigned long long now,
                               int finished);

    unsigned long long tessera_ledger_admit(TesseraLedger *ledger, unsigned long long now, TesseraEvent *events,
                                            unsigned long long start_limit);

    int tessera_ledger_next(const TesseraLedger *ledger, TesseraDeadline *root);

    int tessera_ledger_fire(TesseraLedger *ledger, unsigned long long now, TesseraEvent *event);

    const TesseraJob *tessera_ledger_job(const TesseraLedger *ledger, unsigned long long identity);

    const TesseraHistory *tessera_ledger_history(const TesseraLedger *ledger, const EngineSignum *signum);

    unsigned long long tessera_ledger_wants(const TesseraLedger *ledger, const TesseraJob *job);

    int tessera_ledger_remember(TesseraLedger *ledger, const TesseraHistory *history);

    int tessera_ledger_idle(TesseraLedger *ledger, unsigned long long now, unsigned long long idle_microseconds);

    void tessera_ledger_call(TesseraLedger *ledger, const TesseraCall *call, TesseraAnswer *answer);

#ifdef __cplusplus
}
#endif

#endif
