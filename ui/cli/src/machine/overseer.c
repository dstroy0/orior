// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer.c: the overseer's records in one movable cell, the states that the events give, and the tick that reads each
// process and holds each record to its limits (overseer.h)
#include "overseer.h"
#include "../command_line/command_line.h"

// the records that the cell grows by at once
#define OVERSEER_GROWTH 8u

// One record: its name, its `role` and its state, and the process that it watches, 0 for none. Its limits follow, with
// the call that ends its process. Then the start of the ask under way, 0 for none. Then the reading of the last tick:
// the processor time and its time, and the working set. Last, the share of a processor and the ticks past its limit
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    unsigned int role;
    unsigned int state;
    unsigned long long pid;
    unsigned long long memory_bytes;
    unsigned int cpu_percent;
    unsigned long long ask_ms;
    void (*end)(void *context);
    void *context;
    unsigned long long asked_ms;
    unsigned long long processor_us;
    unsigned long long read_ms;
    unsigned long long working;
    unsigned int cpu_now;
    unsigned int cpu_over;
} OverseerRecord;

typedef struct
{
    const char *name;
    unsigned int at;
    OverseerRecord *record;
    unsigned long long now_ms;
    VerbaLinea *line;
} OverseerCtx;

// The cell of the records, their count and the count the cell holds, under the gate. The gate also guards whether a
// state changed since the last tick. The time of the next tick is the main thread's own
static const CarcerMovableEntry *s_records;
static unsigned int s_count;
static unsigned int s_capacity;
static int s_changed;
static CliGate s_gate = CLI_GATE_INIT;
static unsigned long long s_next_tick_ms;

// the record at `at` read into `record`, with the gate shut
EMBED_INLINE void overseer_read(const OverseerCtx *args)
{
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_records,
                            .offset = (unsigned long long)args->at * sizeof(OverseerRecord),
                            .dst = args->record,
                            .bytes = sizeof(OverseerRecord)});
}

// the record `record` written at `at`, with the gate shut
EMBED_INLINE void overseer_write(const OverseerCtx *args)
{
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_records,
                            .offset = (unsigned long long)args->at * sizeof(OverseerRecord),
                            .src = args->record,
                            .bytes = sizeof(OverseerRecord)});
}

// the place of the record `name`, with the gate shut, or the count where no record has that name
EMBED_INLINE unsigned int overseer_find(const OverseerCtx *args)
{
    const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN}) + 1ULL;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        if (g_cli_guard->movable_buf_compare(
                &(CarcerMovableCfg){.cell = s_records,
                                    .offset = (unsigned long long)at * sizeof(OverseerRecord),
                                    .src = args->name,
                                    .bytes = bytes}) == 0)
        {
            return at;
        }
    }
    return s_count;
}

// room for one more record, with the gate shut: 1, or 0 where the pool refuses the larger cell
EMBED_INLINE int overseer_room(void)
{
    if (s_count < s_capacity)
    {
        return 1;
    }
    const unsigned int capacity = s_capacity + OVERSEER_GROWTH;
    const CarcerMovableEntry *const grown =
        g_cli_guard->movable_buf_alloc((unsigned long long)capacity * sizeof(OverseerRecord), g_cli_overseer_owner);
    if (grown == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        OverseerRecord record;
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = grown, .offset = (unsigned long long)at * sizeof(record), .src = &record, .bytes = sizeof(record)});
    }
    if (s_records != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_records, g_cli_overseer_owner);
    }
    s_records = grown;
    s_capacity = capacity;
    return 1;
}

// the state that the event `event` gives a record in the state `state`; ASKED and ANSWERED leave it as it is
EMBED_INLINE unsigned int overseer_state_after(unsigned int state, unsigned int event)
{
    static const unsigned int s_states[CLI_OVERSEER_ANSWERED + 1u] = {
        CLI_OVERSEER_STARTING, CLI_OVERSEER_STARTING, CLI_OVERSEER_UP,       CLI_OVERSEER_STOPPING,
        CLI_OVERSEER_STOPPING, CLI_OVERSEER_ERROR,    CLI_OVERSEER_CRITICAL, CLI_OVERSEER_CRITICAL,
        CLI_OVERSEER_NONE,     CLI_OVERSEER_NONE,     CLI_OVERSEER_NONE,
    };
    const int keeps = (event == CLI_OVERSEER_ASKED) || (event == CLI_OVERSEER_ANSWERED);
    return keeps ? state : s_states[event];
}

void cli_overseer_report(const CliOverseerCfg *args)
{
    if ((args->name == NULL) || (args->event > CLI_OVERSEER_ANSWERED))
    {
        return;
    }
    cli_gate_shut(&s_gate);
    unsigned int at = EMBED_CALL(overseer_find, OverseerCtx, .name = args->name);
    if (args->event == CLI_OVERSEER_STOPPED)
    {
        if (at < s_count)
        {
            // the last record takes the place of the removed one
            OverseerRecord last;
            EMBED_CALL(overseer_read, OverseerCtx, .at = s_count - 1u, .record = &last);
            EMBED_CALL(overseer_write, OverseerCtx, .at = at, .record = &last);
            s_count -= 1u;
            s_changed = 1;
        }
        cli_gate_opened(&s_gate);
        return;
    }
    OverseerRecord record;
    if (at == s_count)
    {
        if (!overseer_room())
        {
            cli_gate_opened(&s_gate);
            return;
        }
        memor.set(&(MemoriaCfg){.dst = &record, .val = 0, .bytes = sizeof(record)});
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = record.name, .src = args->name, .cap = sizeof(record.name)});
        record.role = args->role;
        s_count += 1u;
    }
    else
    {
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
    }
    const unsigned int state = overseer_state_after(record.state, args->event);
    s_changed = s_changed || (state != record.state);
    record.state = state;
    if (args->event == CLI_OVERSEER_SPAWNED)
    {
        record.pid = args->pid;
        record.read_ms = 0ULL;
        record.cpu_over = 0u;
    }
    record.pid = ((args->event == CLI_OVERSEER_ENDED) || (args->event == CLI_OVERSEER_FAILED) ||
                  (args->event == CLI_OVERSEER_LINK_LOST))
                     ? 0ULL
                     : record.pid;
    record.asked_ms = (args->event == CLI_OVERSEER_ASKED) ? cli_clock_milliseconds() : record.asked_ms;
    record.asked_ms = (args->event == CLI_OVERSEER_ANSWERED) ? 0ULL : record.asked_ms;
    EMBED_CALL(overseer_write, OverseerCtx, .at = at, .record = &record);
    cli_gate_opened(&s_gate);
}

void cli_overseer_limits(const CliOverseerCfg *args)
{
    cli_gate_shut(&s_gate);
    const unsigned int at = EMBED_CALL(overseer_find, OverseerCtx, .name = args->name);
    if (at < s_count)
    {
        OverseerRecord record;
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        record.memory_bytes = args->memory_bytes;
        record.cpu_percent = args->cpu_percent;
        record.ask_ms = args->ask_ms;
        record.end = args->end;
        record.context = args->context;
        EMBED_CALL(overseer_write, OverseerCtx, .at = at, .record = &record);
    }
    cli_gate_opened(&s_gate);
}

// the reading of the process of `record` at `now_ms`: its working set, and its share of a processor since the last
// reading. A process that the system no longer lists reads as 0. 1 where the system gave a reading, else 0
EMBED_INLINE int overseer_measure(const OverseerCtx *args)
{
    OverseerRecord *const record = args->record;
    CliProcessMemory measured;
    memor.set(&(MemoriaCfg){.dst = &measured, .val = 0, .bytes = sizeof(measured)});
    // a pid the system gives is below what an unsigned int counts
    const int read = (record->pid != 0ULL) && EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx,
                                                         .pid = (unsigned int)record->pid, .memory = &measured);
    record->working = read ? measured.working : 0ULL;
    const unsigned long long elapsed_us = (args->now_ms - record->read_ms) * 1000ULL;
    const int timed =
        read && (record->read_ms != 0ULL) && (elapsed_us != 0ULL) && (measured.processor_us >= record->processor_us);
    // the share of one processor is below what an unsigned int counts
    record->cpu_now =
        timed ? (unsigned int)(((measured.processor_us - record->processor_us) * 100ULL) / elapsed_us) : 0u;
    record->processor_us = read ? measured.processor_us : 0ULL;
    record->read_ms = read ? args->now_ms : 0ULL;
    return read;
}

// The limit that `record` goes past at `now_ms`, written into `line`: 1, or 0 where it keeps to its limits. The
// processor share must stay past its limit for CLI_OVERSEER_CPU_TICKS ticks
EMBED_INLINE int overseer_past(const OverseerCtx *args)
{
    OverseerRecord *const record = args->record;
    VerbaLinea *const line = args->line;
    record->cpu_over =
        ((record->cpu_percent != 0u) && (record->cpu_now > record->cpu_percent)) ? record->cpu_over + 1u : 0u;
    if ((record->memory_bytes != 0ULL) && (record->working > record->memory_bytes))
    {
        mmgr_line_put(line, "memory ");
        mmgr_line_decimal(line, record->working >> 20u, 1u);
        mmgr_line_put(line, " MiB, past its limit of ");
        mmgr_line_decimal(line, record->memory_bytes >> 20u, 1u);
        mmgr_line_put(line, " MiB");
        return 1;
    }
    if (record->cpu_over >= CLI_OVERSEER_CPU_TICKS)
    {
        mmgr_line_put(line, "processor ");
        mmgr_line_decimal(line, record->cpu_now, 1u);
        mmgr_line_put(line, "%, past its limit of ");
        mmgr_line_decimal(line, record->cpu_percent, 1u);
        mmgr_line_put(line, "%");
        return 1;
    }
    if ((record->ask_ms != 0ULL) && (record->asked_ms != 0ULL) && (args->now_ms > record->asked_ms + record->ask_ms))
    {
        mmgr_line_put(line, "an ask of ");
        mmgr_line_decimal(line, args->now_ms - record->asked_ms, 1u);
        mmgr_line_put(line, " ms, past its limit of ");
        mmgr_line_decimal(line, record->ask_ms, 1u);
        mmgr_line_put(line, " ms");
        return 1;
    }
    return 0;
}

// the telemetry line of `record`: `overseer <name> <state> <working bytes> <processor percent>`
EMBED_INLINE void overseer_telemetry(const OverseerCtx *args)
{
    static const char *const s_names[CLI_OVERSEER_CRITICAL + 1u] = {"none",     "starting", "up",
                                                                    "stopping", "error",    "critical"};
    char text[CLI_NAME_MAX_LEN + 96u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "overseer ");
    mmgr_line_put(&line, args->record->name);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, s_names[(args->record->state <= CLI_OVERSEER_CRITICAL) ? args->record->state : 0u]);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, args->record->working, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, args->record->cpu_now, 1u);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_finish(&line));
}

int cli_overseer_tick(const CliOverseerCfg *args)
{
    if (args->now_ms < s_next_tick_ms)
    {
        return 0;
    }
    s_next_tick_ms = args->now_ms + CLI_OVERSEER_TICK_MS;
    cli_gate_shut(&s_gate);
    const unsigned int count = s_count;
    cli_gate_opened(&s_gate);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        OverseerRecord record;
        cli_gate_shut(&s_gate);
        const int present = at < s_count;
        if (present)
        {
            EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        }
        cli_gate_opened(&s_gate);
        if (!present)
        {
            break;
        }
        // the process reads with the gate open, because a system call can take a while
        const int read = EMBED_CALL(overseer_measure, OverseerCtx, .record = &record, .now_ms = args->now_ms);
        // the far end of a remote link that the system no longer lists lost the link
        if ((record.role == CLI_OVERSEER_REMOTE) && (record.pid != 0ULL) && !read)
        {
            cli_overseer_report(&(CliOverseerCfg){.name = record.name, .event = CLI_OVERSEER_LINK_LOST});
        }
        char why[CLI_NAME_MAX_LEN + 96u];
        VerbaLinea line = {why, sizeof(why), 0ULL};
        const int past =
            (record.end != NULL) && (record.pid != 0ULL) &&
            EMBED_CALL(overseer_past, OverseerCtx, .record = &record, .now_ms = args->now_ms, .line = &line);
        if (args->telemetry)
        {
            EMBED_CALL(overseer_telemetry, OverseerCtx, .record = &record);
        }
        cli_gate_shut(&s_gate);
        // a report between the read and this write moves the record; the write goes to its place by name
        const unsigned int place = EMBED_CALL(overseer_find, OverseerCtx, .name = record.name);
        if (place < s_count)
        {
            OverseerRecord now;
            EMBED_CALL(overseer_read, OverseerCtx, .at = place, .record = &now);
            now.working = record.working;
            now.cpu_now = record.cpu_now;
            now.cpu_over = record.cpu_over;
            now.processor_us = record.processor_us;
            now.read_ms = record.read_ms;
            EMBED_CALL(overseer_write, OverseerCtx, .at = place, .record = &now);
        }
        cli_gate_opened(&s_gate);
        if (past)
        {
            // the end runs with the gate open, because it can report the process's end back here
            record.end(record.context);
            cli_overseer_report(&(CliOverseerCfg){.name = record.name, .event = CLI_OVERSEER_ENDED});
            char text[CLI_NAME_MAX_LEN * 2u + 128u];
            VerbaLinea notice = {text, sizeof(text), 0ULL};
            mmgr_line_put(&notice, record.name);
            mmgr_line_put(&notice, " ended: ");
            mmgr_line_put_n(&notice, why, mmgr_line_ok(&line) ? line.at : 0ULL);
            (void)mmgr_line_finish(&notice);
            cli_notice_post(&(CliNoticesCfg){.text = text, .failed = 1, .name = record.name});
        }
    }
    cli_gate_shut(&s_gate);
    const int changed = s_changed;
    s_changed = 0;
    cli_gate_opened(&s_gate);
    return changed;
}

// whether the name `name` comes after the name `other`. The compare covers the shorter name and its end byte, which
// orders a name before every longer name that starts with it
EMBED_INLINE int overseer_after(const char *name, const char *other)
{
    const unsigned long long name_bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long other_bytes = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long bytes = ((name_bytes < other_bytes) ? name_bytes : other_bytes) + 1ULL;
    return memor.cmp(&(MemoriaCfg){.src = name, .other = other, .bytes = bytes}) > 0;
}

unsigned int cli_overseer_seen(const CliOverseerCfg *args)
{
    unsigned int count = 0u;
    cli_gate_shut(&s_gate);
    for (unsigned int at = 0u; (at < s_count) && (count < args->max); at += 1u)
    {
        OverseerRecord record;
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        if (record.role != args->role)
        {
            continue;
        }
        // the names go in their order: each moves up past the names after it
        unsigned int place = count;
        while ((place > 0u) && overseer_after(args->seen_out[place - 1u].name, record.name))
        {
            args->seen_out[place] = args->seen_out[place - 1u];
            place -= 1u;
        }
        CliOverseerSeen *const seen = &args->seen_out[place];
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = seen->name, .src = record.name, .cap = sizeof(seen->name)});
        seen->role = record.role;
        seen->state = record.state;
        seen->working = record.working;
        seen->cpu_percent = record.cpu_now;
        count += 1u;
    }
    cli_gate_opened(&s_gate);
    return count;
}

unsigned int cli_overseer_worst(const CliOverseerCfg *args)
{
    unsigned int worst = CLI_OVERSEER_NONE;
    cli_gate_shut(&s_gate);
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        OverseerRecord record;
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        worst = ((record.role == args->role) && (record.state > worst)) ? record.state : worst;
    }
    cli_gate_opened(&s_gate);
    return worst;
}

// whether the state `state` blinks: `CLI_OVERSEER_STARTING`, `CLI_OVERSEER_STOPPING` and `CLI_OVERSEER_ERROR` do
EMBED_INLINE int overseer_blinks(unsigned int state)
{
    return (state == CLI_OVERSEER_STARTING) || (state == CLI_OVERSEER_STOPPING) || (state == CLI_OVERSEER_ERROR);
}

int cli_overseer_lit(const CliOverseerCfg *args)
{
    if (args->state == CLI_OVERSEER_NONE)
    {
        return 0;
    }
    return !overseer_blinks(args->state) || (((args->now_ms / CLI_OVERSEER_BLINK_MS) % 2ULL) == 0ULL);
}

int cli_overseer_blinking(void)
{
    int blinking = 0;
    cli_gate_shut(&s_gate);
    for (unsigned int at = 0u; (at < s_count) && !blinking; at += 1u)
    {
        OverseerRecord record;
        EMBED_CALL(overseer_read, OverseerCtx, .at = at, .record = &record);
        blinking = overseer_blinks(record.state);
    }
    cli_gate_opened(&s_gate);
    return blinking;
}

unsigned long long cli_overseer_due_ms(const CliOverseerCfg *args)
{
    cli_gate_shut(&s_gate);
    const unsigned int count = s_count;
    cli_gate_opened(&s_gate);
    if (count == 0u)
    {
        return 0ULL;
    }
    const int blinking = cli_overseer_blinking();
    const unsigned long long tick = (s_next_tick_ms > args->now_ms) ? s_next_tick_ms : args->now_ms + 1ULL;
    const unsigned long long blink = ((args->now_ms / CLI_OVERSEER_BLINK_MS) + 1ULL) * CLI_OVERSEER_BLINK_MS;
    return (blinking && (blink < tick)) ? blink : tick;
}

void cli_overseer_end(void)
{
    cli_gate_shut(&s_gate);
    if (s_records != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_records, g_cli_overseer_owner);
    }
    s_records = NULL;
    s_count = 0u;
    s_capacity = 0u;
    s_changed = 0;
    s_next_tick_ms = 0ULL;
    cli_gate_opened(&s_gate);
}
