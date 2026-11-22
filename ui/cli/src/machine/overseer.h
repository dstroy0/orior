// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer.h: the overseer, the telemetry gatherer of the window's main thread. Each process that the program starts
// for a language server or a script plugin, and each remote session, has a record here under its name. The modules that
// start and drive them report events, and the overseer holds the state that each event gives. Its tick reads each
// process's memory and processor time, and ends a process past the limits of its record with a notice.
//
// The window's dots read the states. A dot shows green and blinks while a record starts, and it shows solid green while
// the record is up. It shows yellow and blinks while the record stops or its link has errors. It shows red and blinks
// after an error, and it shows solid red where the record failed for good. No other module holds a state (overseer.c)
#ifndef OVERSEER_H
#define OVERSEER_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CliOverseerCfg CliOverseerCfg;

// the values of a record's `role`: a language server, a script plugin's host, and a remote session
#define CLI_OVERSEER_SERVER 0u
#define CLI_OVERSEER_PLUGIN 1u
#define CLI_OVERSEER_REMOTE 2u

// the states, in the order of their weight, from `CLI_OVERSEER_NONE` to `CLI_OVERSEER_CRITICAL`
#define CLI_OVERSEER_NONE 0u
#define CLI_OVERSEER_STARTING 1u
#define CLI_OVERSEER_UP 2u
#define CLI_OVERSEER_STOPPING 3u
#define CLI_OVERSEER_ERROR 4u
#define CLI_OVERSEER_CRITICAL 5u

// The events and the state that each gives. `SPAWNED` and `CONNECTING` give `CLI_OVERSEER_STARTING`, and `SPAWNED`
// also records the pid. `READY` gives `CLI_OVERSEER_UP`, and `SHUTDOWN` and `LINK_ERROR` give `CLI_OVERSEER_STOPPING`.
// `ENDED` gives `CLI_OVERSEER_ERROR`, and `FAILED` and `LINK_LOST` give `CLI_OVERSEER_CRITICAL`. `STOPPED` removes the
// record. `ASKED` and `ANSWERED` start and end the time of an ask
#define CLI_OVERSEER_SPAWNED 0u
#define CLI_OVERSEER_CONNECTING 1u
#define CLI_OVERSEER_READY 2u
#define CLI_OVERSEER_SHUTDOWN 3u
#define CLI_OVERSEER_LINK_ERROR 4u
#define CLI_OVERSEER_ENDED 5u
#define CLI_OVERSEER_FAILED 6u
#define CLI_OVERSEER_LINK_LOST 7u
#define CLI_OVERSEER_STOPPED 8u
#define CLI_OVERSEER_ASKED 9u
#define CLI_OVERSEER_ANSWERED 10u

// the milliseconds between two ticks, half a blink, and the ticks that a processor share must stay past its limit
#define CLI_OVERSEER_TICK_MS 2000ULL
#define CLI_OVERSEER_BLINK_MS 500ULL
#define CLI_OVERSEER_CPU_TICKS 3u

    // One record as the window reads it. It has its name, its `role`, its state and its working set in bytes. Then it
    // has its percent of one processor over the last tick
    typedef struct
    {
        char name[CLI_NAME_MAX_LEN];
        unsigned int role;
        unsigned int state;
        unsigned long long working;
        unsigned int cpu_percent;
    } CliOverseerSeen;

    // the event `event` of the record `name`, its `role` given, with the process `pid` for `CLI_OVERSEER_SPAWNED`. The
    // event makes a record where none has the name, and `CLI_OVERSEER_STOPPED` removes it
    void cli_overseer_report(const CliOverseerCfg *args);
    // the limits of the record `name`: `memory_bytes` of working set, `cpu_percent` of one processor, and `ask_ms` for
    // one ask, each 0 for none. `end` with `context` ends its process where it goes past one of them
    void cli_overseer_limits(const CliOverseerCfg *args);
    // the tick at `now_ms`: each process read, each limit applied, and a telemetry line for each record where
    // `telemetry` is 1. 1 where a state changed since the last tick, else 0
    int cli_overseer_tick(const CliOverseerCfg *args);
    // the records of the `role` given into `seen_out`, `max` at most, in the order of their names: their
    // count
    unsigned int cli_overseer_seen(const CliOverseerCfg *args);
    // the heaviest state among the records of the `role` given, CLI_OVERSEER_NONE where it has none
    unsigned int cli_overseer_worst(const CliOverseerCfg *args);
    // whether a dot of the state `state` shows at `now_ms`. A state that blinks shows for half of each blink
    int cli_overseer_lit(const CliOverseerCfg *args);
    // the time of the next tick or the next half blink after `now_ms`, 0 where the overseer has no record; and whether
    // a record's state blinks
    unsigned long long cli_overseer_due_ms(const CliOverseerCfg *args);
    int cli_overseer_blinking(void);
    // every record given back as the program ends
    void cli_overseer_end(void);

    struct CliOverseerCfg
    {
        const char *name;
        unsigned int role;
        unsigned int event;
        unsigned long long pid;
        unsigned long long memory_bytes;
        unsigned int cpu_percent;
        unsigned long long ask_ms;
        void (*end)(void *context);
        void *context;
        unsigned long long now_ms;
        int telemetry;
        unsigned int state;
        CliOverseerSeen *seen_out;
        unsigned int max;
    };

    typedef struct
    {
        void (*report)(const CliOverseerCfg *args);
        void (*limits)(const CliOverseerCfg *args);
        int (*tick)(const CliOverseerCfg *args);
        unsigned int (*seen)(const CliOverseerCfg *args);
        unsigned int (*worst)(const CliOverseerCfg *args);
        int (*lit)(const CliOverseerCfg *args);
        unsigned long long (*due_ms)(const CliOverseerCfg *args);
    } CliOverseerNs;
    EMBED_TABLE_LAYOUT(CliOverseerNs, report, limits, tick, seen, worst, lit, due_ms);
    EMBED_TABLE_STORAGE CliOverseerNs cli_overseer EMBED_UNUSED = {
        .report = cli_overseer_report,
        .limits = cli_overseer_limits,
        .tick = cli_overseer_tick,
        .seen = cli_overseer_seen,
        .worst = cli_overseer_worst,
        .lit = cli_overseer_lit,
        .due_ms = cli_overseer_due_ms,
    };

#ifdef __cplusplus
}
#endif

#endif
