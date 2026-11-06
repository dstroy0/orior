// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.c: the CLI's memory: MMgr's prison site, one cellblock over the pool declared here at CLI_POOL_BYTES, emptied
// for a program's request while no cell is allocated; and the program's owner table, laid here from owners.h as data,
// each module's owner a const pointer into it. Every take and release goes through the cellblock's guard and carries
// the owner. MMgr's report of a release carrying the wrong owner is logged. The reports read the cellblock's peaks and
// the owner table off their atomic words, and are written at a command's end
#include "pool.h"

#include "cli_internal.h"

#include <stdio.h>

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
    const char *kind;
    const VerbaLinea *line;
    PoolOwnerRead *owners;
    VerbaLinea *out;
    unsigned long long persistent;
    unsigned long long temporary;
} PoolCtx;

// A line of the pool's log: written to the run this process is a member of as the record `cells/<kind>`, and to the
// process's error stream, where the harness saves the window's
EMBED_INLINE void pool_log(const PoolCtx *args)
{
    cli_pool_reported(args->kind, args->line);
    if (mmgr_line_ok(args->line))
    {
        (void)fprintf(stderr, "cells %s %.*s\n", args->kind, (int)args->line->at, args->line->out);
    }
}

uint64_t mmgr_carcer_hw_clock_ns(void)
{
    // the clock read as MMgr stamps a take with it: the microsecond clock in nanoseconds, rolling in the word's bits
    return (uint64_t)cli_clock_microseconds() * 1000u;
}

embed_u64 embed_clock_ns(void)
{
    // the clock a turnstile badge is stamped with, the same one
    return (embed_u64)cli_clock_microseconds() * 1000u;
}

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, named->name);
    mmgr_line_put(&line, " named for a cell of ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no owner of the table");
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)bytes, 1u);
    mmgr_line_put(&line, " bytes: refused");
    EMBED_CALL(pool_log, PoolCtx, .kind = "misnamed", .line = &line);
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

static embed_bool pool_mover_started(void (*run)(void *context), void *context)
{
    s_mover.run = run;
    s_mover.context = context;
    s_mover.priority = CLI_PRIORITY_IDLE;
    return (embed_bool)(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_mover) != 0);
}

static embed_bool pool_mover_waited(uint32_t milliseconds)
{
    (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake,
                     .milliseconds_u64 = milliseconds);
    return EMBED_TRUE;
}

static void pool_mover_moving(embed_bool moving)
{
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
    for (size_t at = 0u; at < mmgr_owner_table_count; at += 1u)
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
    // a program empties its pool before any thread takes a cell, and empties it again only while no cell is allocated
    // and the mover does not read it; nothing takes between the test and the emptying. A request past the pool declared
    // is refused
    SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    const int idle = mmgr_series_cellularum_idle(record) && (embed_atomic_uptr_load(&record->movable) == 0u);
    const int fits = bytes <= CLI_POOL_BYTES;
    if (idle && fits)
    {
        mmgr_series_cellularum_emptied(record);
    }
    return idle && fits;
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
