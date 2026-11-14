// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.c: the CLI's memory: MMgr's prison site, one cellblock over the pool declared here at CLI_POOL_BYTES, emptied
// for a program's request while no cell is allocated; and the program's owner table, laid here from owners.h as data,
// each module's owner a const pointer into it. Every take and release goes through the cellblock's guard and carries
// the owner. MMgr's report of a release carrying the wrong owner is logged. The reports read the cellblock's peaks and
// the owner table off their atomic words, and are written at a command's end
#include "pool.h"
#include "rings.h"

#include "cli_internal.h"

// The pool: bytes of the program's own image, declared at CLI_POOL_BYTES, the cellblock's record beside them, both
// settled by the linker; emptied for a program's request while no cell is allocated. Minimum security: a cell is left
// as it is when released and handed out unzeroed, so that a release costs no bytes it did not use and no page it did
// not touch; a capacity writes what it reads
ParsMemoriaeInternae(cli_pool, CLI_POOL_BYTES);
LocusCarcerum(cli_pool_site, MMGR_MINIMUM_SECURITY(cli_pool));

// the program's owners, the table and each owner's pointer into it
MMGR_OWNERS(CLI_OWNERS)

const MinimumSecurityGuard *const g_cli_guard = &cli_pool_site.cli_pool;

// One owner as the table holds it, its counts read off their atomic words and written after, since a line written to
// may take a cell of the same cellblock
typedef struct
{
    const char *name;
    unsigned long long in_use;
    unsigned long long hw;
} PoolOwnerRead;

typedef struct
{
    const char *type;
    const VerbaLinea *line;
    PoolOwnerRead *owners;
    VerbaLinea *out;
    unsigned long long persistent;
    unsigned long long temporary;
} PoolCtx;

// A line of the pool's log: written to the run this process is a member of as the record `cells/<type>`, and to the
// process's error stream, where the harness saves the window's
EMBED_INLINE void pool_log(const PoolCtx *args)
{
    cli_pool_reported(args->type, args->line);
    if (mmgr_line_ok(args->line))
    {
        char text[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_put(&line, "cells ");
        mmgr_line_put(&line, args->type);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put_n(&line, args->line->out, args->line->at);
        mmgr_line_ch(&line, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
                   .length = mmgr_line_finish(&line));
    }
}

embed_u64 embed_clock_ns(void)
{
    // the one clock: the microsecond clock in nanoseconds, which stamps a turnstile's badge and a cell's take alike
    return (embed_u64)cli_clock_microseconds() * 1000u;
}

embed_u64 mmgr_carcer_hw_clock_ns(void)
{
    return embed_clock_ns();
}

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, embed_word bytes)
{
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, named->name);
    mmgr_line_put(&line, " named for a cell of ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no owner of the table");
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)bytes, 1u);
    mmgr_line_put(&line, " bytes: refused");
    EMBED_CALL(pool_log, PoolCtx, .type = "misnamed", .line = &line);
}

// the turnstiles the option's storage has capacity for, which the machine's processors are bounded to, and the rows of
// touches per thread it saves
#define POOL_WIDTH_MOST 256u
#define POOL_TOUCH_ROWS 64u

// What MMgr's movable option is given: storage whose address the linker settles, and the thread its mover runs on,
// saved at idle between moves and raised across each, since a reader waits on the copy
ParsMemoriaeInternae(cli_pool_movable, MMGR_MOVABLE_STORAGE_BYTES(CLI_POOL_MOVABLE, POOL_WIDTH_MOST, POOL_TOUCH_ROWS));
static CliThread s_mover;
static CliWake s_mover_wake = CLI_WAKE_INIT;

// The mover as MMgr hands it to the thread: what it runs and on what, and whether the pool asks it to end, which its
// next wait answers, and whether it has ended, which the pool waits on before it empties the cellblock again
static void (*s_mover_run)(void *context);
static void *s_mover_context;
static embed_atomic_u32 s_mover_ending;
static embed_atomic_u32 s_mover_ended;

static void pool_mover_run(void *context)
{
    (void)context;
    s_mover_run(s_mover_context);
    EMBED_ATOMIC_STORE(&s_mover_ended, 1u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_mover_wake);
}

static embed_bool pool_mover_started(void (*run)(void *context), void *context)
{
    s_mover_run = run;
    s_mover_context = context;
    EMBED_ATOMIC_STORE(&s_mover_ending, 0u);
    EMBED_ATOMIC_STORE(&s_mover_ended, 0u);
    s_mover.run = pool_mover_run;
    s_mover.context = NULL;
    s_mover.priority = CLI_PRIORITY_IDLE;
    const int started = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_mover) != 0;
    // a thread that did not start leaves no mover to end
    s_mover_run = started ? run : NULL;
    return (embed_bool)started;
}

static embed_bool pool_mover_waited(embed_u32 milliseconds)
{
    if (EMBED_ATOMIC_LOAD(&s_mover_ending) != 0u)
    {
        return EMBED_FALSE;
    }
    (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake,
                     .milliseconds_u64 = milliseconds);
    return (embed_bool)(EMBED_ATOMIC_LOAD(&s_mover_ending) == 0u);
}

// the mover ended where it runs: asked to end, woken, and waited for
static void pool_mover_ended(void)
{
    if (s_mover_run == NULL)
    {
        return;
    }
    EMBED_ATOMIC_STORE(&s_mover_ending, 1u);
    while (EMBED_ATOMIC_LOAD(&s_mover_ended) == 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_mover_wake);
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake, .milliseconds_u64 = 1ULL);
    }
    s_mover_run = NULL;
}

// the sorts the mover has started, read by the telemetry
static embed_atomic_u32 s_mover_sorts;

static void pool_mover_moving(embed_bool moving)
{
    if (moving)
    {
        (void)EMBED_ATOMIC_ADD_32(&s_mover_sorts, 1u);
    }
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx,
               .priority = moving ? CLI_PRIORITY_WORK : CLI_PRIORITY_IDLE);
}

void cli_pool_movable_ready(void)
{
    const unsigned int processors = cli_processors();
    // a refusal is the option assigned already, or the thread not started, which leaves the pool working and still
    (void)g_cli_guard->movable_init(
        &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_cli_pool_movable,
                                .storage_bytes = cli_pool_movable_bytes,
                                .cells = CLI_POOL_MOVABLE,
                                .width = (processors < POOL_WIDTH_MOST) ? processors : POOL_WIDTH_MOST,
                                .rows = POOL_TOUCH_ROWS,
                                .thread_started = pool_mover_started,
                                .thread_id = cli_thread_id,
                                .thread_wait = pool_mover_waited,
                                .thread_yield = cli_thread_yield,
                                .thread_moving = pool_mover_moving});
}

const SeriesCellularum *cli_pool_series_cellularum(void)
{
    return &cli_pool_site_cli_pool_ctx;
}

// the pool's bytes
static unsigned long long pool_bytes(void)
{
    return (unsigned long long)cli_pool_site_cli_pool_ctx.size;
}

// the owners of the table copied into `owners`, sized for MMGR_CARCER_OWNERS: their count
EMBED_INLINE unsigned int pool_owners_read(const PoolCtx *args)
{
    unsigned int count = 0u;
    for (embed_word at = 0u; at < mmgr_owner_table_count; at += 1u)
    {
        const CellOwner *const owner = &mmgr_owner_table[at];
        args->owners[count] = (PoolOwnerRead){owner->name, (unsigned long long)mmgr_owner_in_use(owner),
                                              (unsigned long long)mmgr_owner_hw(owner)};
        count += 1u;
    }
    return count;
}

// a report, `<pool's bytes> <persistent> <temporary>`, written into `out`
EMBED_INLINE void pool_write(const PoolCtx *args)
{
    mmgr_line_decimal(args->out, pool_bytes(), 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->persistent, 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->temporary, 1u);
}

void cli_pool_reported(const char *name, const VerbaLinea *report)
{
    if ((qry_member_run() == NULL) || !mmgr_line_ok(report))
    {
        return;
    }
    char named[QRY_NAME_MAX_LEN + 1u];
    VerbaLinea name_line = {named, sizeof(named), 0u, NULL, NULL, 0u};
    mmgr_line_put(&name_line, "cells/");
    mmgr_line_put(&name_line, name);
    if (mmgr_line_finish(&name_line) != 0u)
    {
        (void)qry_member_append(named, report->out, report->at, QRY_ORDINARY);
    }
}

int cli_pool_made(unsigned long long bytes)
{
    // a program empties its pool before any thread takes a cell, and empties it again only while no cell is allocated;
    // the mover is ended first, so that it does not read the cellblock as it empties, and started again with it.
    // Nothing takes between the test and the emptying. A request past the pool declared is refused
    SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    const int idle = mmgr_series_cellularum_idle(record);
    const int fits = bytes <= CLI_POOL_BYTES;
    if (idle && fits)
    {
        pool_mover_ended();
        mmgr_series_cellularum_emptied(record);
        // the mover's thread starts with the pool, so the cellblock is sorted in every quiet past the fragmentation
        // threshold, whether or not a module stores movable cells
        cli_pool_movable_ready();
        // the rings' records laid over their parts of the rings block, with the pool, before any ticket is taken
        cli_rings.laid();
    }
    return idle && fits;
}

// The line's flush as its cell fills: a cell of twice the capacity taken, the text so far copied into it, the cell
// given back, and the line's `out` set past the copied text. 1, or 0 where the cells cannot meet the next cell or it
// would pass `most`, which fills the line
static int grown_flush(void *context, const char *bytes, size_t size)
{
    CliGrownLine *const grown = context;
    (void)bytes;
    const unsigned long long held = grown->line.flushed + size;
    const unsigned long long doubled = grown->capacity * 2ULL;
    const unsigned long long capacity = (doubled < grown->most) ? doubled : grown->most;
    if (capacity <= grown->capacity)
    {
        return 0;
    }
    char *const cell = g_cli_guard->temporary_buf_alloc(capacity, grown->owner);
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = grown->cell, .bytes = held});
    g_cli_guard->persistent_buf_release(grown->cell, grown->owner);
    grown->cell = cell;
    grown->capacity = capacity;
    grown->line.out = cell + held;
    grown->line.capacity = capacity - held;
    return 1;
}

void cli_grown_begin(CliGrownLine *grown, unsigned long long most, const CellOwner *owner)
{
    // the line's cell is taken once at its most
    const unsigned long long first = most;
    grown->cell = g_cli_guard->temporary_buf_alloc(first, owner);
    grown->capacity = (grown->cell != NULL) ? first : 0ULL;
    grown->most = most;
    grown->owner = owner;
    grown->line = mmgr_line_over(grown->cell, first);
    grown->line.flush = grown_flush;
    grown->line.context = grown;
}

unsigned long long cli_grown_size(const CliGrownLine *grown)
{
    // a full line holds its capacity less its terminator
    const unsigned long long at =
        (grown->line.at < grown->line.capacity) ? grown->line.at : (grown->line.capacity - 1ULL);
    return (grown->line.capacity != 0ULL) ? (grown->line.flushed + at) : 0ULL;
}

void cli_grown_reset(CliGrownLine *grown)
{
    grown->line = mmgr_line_over(grown->cell, grown->capacity);
    grown->line.flush = grown_flush;
    grown->line.context = grown;
}

void cli_grown_end(CliGrownLine *grown)
{
    g_cli_guard->persistent_buf_release(grown->cell, grown->owner);
    grown->cell = NULL;
    grown->capacity = 0ULL;
    grown->line = mmgr_line_over(NULL, 0u);
}

void cli_pool_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    const SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    *pool = pool_bytes();
    *persistent = (unsigned long long)mmgr_series_cellularum_persistent_hw(record);
    *temporary = (unsigned long long)mmgr_series_cellularum_temporary_hw(record);
}

void cli_pool_usage_write(VerbaLinea *out)
{
    unsigned long long pool = 0ULL;
    unsigned long long persistent = 0ULL;
    unsigned long long temporary = 0ULL;
    cli_pool_report(&pool, &persistent, &temporary);
    EMBED_CALL(pool_write, PoolCtx, .out = out, .persistent = persistent, .temporary = temporary);
}

void cli_pool_owner_usage_write(const CellOwner *owner, VerbaLinea *out)
{
    EMBED_CALL(pool_write, PoolCtx, .out = out, .persistent = (unsigned long long)mmgr_owner_hw(owner),
               .temporary = 0ULL);
}

void cli_pool_owners_write(VerbaLinea *out)
{
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, owners[at].name);
        mmgr_line_ch(out, '\t');
        mmgr_line_decimal(out, owners[at].in_use, 1u);
        mmgr_line_ch(out, '\t');
        mmgr_line_decimal(out, owners[at].hw, 1u);
        mmgr_line_ch(out, '\n');
    }
}

// the in-use count of each owner at the last telemetry line, which only the window's main thread reads and writes
static unsigned long long s_pool_telemetry_in_use[MMGR_CARCER_OWNERS];

void cli_pool_telemetry_log(void)
{
    // The reads come first and the writes after them, because a line can take a cell of the same cellblock
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    unsigned long long pool = 0ULL;
    unsigned long long persistent = 0ULL;
    unsigned long long temporary = 0ULL;
    cli_pool_report(&pool, &persistent, &temporary);
    unsigned long long in_use = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        in_use += owners[at].in_use;
    }
    const SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    unsigned long long touches = 0ULL;
    for (embed_word row = 0u; row <= POOL_TOUCH_ROWS; row += 1u)
    {
        embed_word id = 0u;
        embed_word passes = 0u;
        if (mmgr_movable_touches(record, row, &id, &passes))
        {
            touches += (unsigned long long)passes;
        }
    }
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_decimal(&line, pool, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, in_use, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, persistent, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, temporary, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)mmgr_movable_queue_depth(record), 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, touches, 1u);
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(pool_log, PoolCtx, .type = "telemetry", .line = &line);
    }
    // the sorts the mover has run, with the persistent end and the fragmentation as they stand
    line.at = 0ULL;
    mmgr_line_decimal(&line, EMBED_ATOMIC_LOAD(&s_mover_sorts), 1u);
    mmgr_line_put(&line, " end ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_series_cellularum_persistent_end(record), 1u);
    mmgr_line_put(&line, " fragmentation ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_series_cellularum_fragmentation(record), 1u);
    EMBED_CALL(pool_log, PoolCtx, .type = "sorts", .line = &line);
    // an owner gets a line only where its in-use count moved since the last telemetry line
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (owners[at].in_use != s_pool_telemetry_in_use[at])
        {
            s_pool_telemetry_in_use[at] = owners[at].in_use;
            line.at = 0ULL;
            mmgr_line_put(&line, owners[at].name);
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, owners[at].in_use, 1u);
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, owners[at].hw, 1u);
            EMBED_CALL(pool_log, PoolCtx, .type = "owner", .line = &line);
        }
    }
    // A lane's passes are an owner's takes and releases, its id the owner's place from 1. A row gets a line only where
    // its passes moved since the last telemetry line; the last row counts every lane past the rest
    static unsigned long long s_pool_telemetry_passes[POOL_TOUCH_ROWS + 1u];
    for (embed_word row = 0u; row <= POOL_TOUCH_ROWS; row += 1u)
    {
        embed_word id = 0u;
        embed_word passes = 0u;
        if (mmgr_movable_touches(record, row, &id, &passes) &&
            ((unsigned long long)passes != s_pool_telemetry_passes[row]))
        {
            s_pool_telemetry_passes[row] = (unsigned long long)passes;
            const int named = (id >= 1u) && (id <= (embed_word)count);
            line.at = 0ULL;
            mmgr_line_put(&line, named ? owners[id - 1u].name : "other");
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, (unsigned long long)passes, 1u);
            EMBED_CALL(pool_log, PoolCtx, .type = "touches", .line = &line);
        }
    }
}

void cli_pool_peaks_report(void)
{
    if (qry_member_run() == NULL)
    {
        return;
    }
    char report[96];
    VerbaLinea line = {report, sizeof(report), 0u, NULL, NULL, 0u};
    cli_pool_usage_write(&line);
    cli_pool_reported("cli_cells", &line);
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line.at = 0u;
        EMBED_CALL(pool_write, PoolCtx, .out = &line, .persistent = owners[at].hw, .temporary = 0ULL);
        cli_pool_reported(owners[at].name, &line);
    }
}
