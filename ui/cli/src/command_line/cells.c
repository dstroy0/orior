// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.c: the cells every module stores its work in: one cellblock over one pool, a block taken from the system for
// the bytes requested as a program starts and made anew for another request while no cell is allocated. Every take and
// give goes through the cellblock's custodia and counts the cell to the owner named, each owner listed in
// cells_owners.h and registered in MMgr's owner queue as the pool is made. MMgr's clock is the CLI's, and its report
// of a give naming the wrong owner is logged. The reports
// read the cellblock's peaks and the owner queue off their atomic words, and are written at a command's end
#include "cells.h"

#include "cli_internal.h"

#include <stdio.h>

// The pool: MMgr's host site, one block taken from the system for the bytes requested, zero as the system hands it out,
// the cellblock's record at its start; made anew for another request, the block before given back whole, while no cell
// is allocated. Minimum security: a cell is left as it is when given back and handed out unzeroed, so that a give costs
// no bytes it did not use and no page it did not touch; a capacity writes what it reads
LocusCarcerumHospes(cli_cells_site, MMGR_MINIMUM_SECURITY(cli_cells_pool));

#define CLI_CELL_OWNER(name_, text_) CellOwner name_ = MMGR_CELL_OWNER(text_);
#include "cells_owners.h"

// One owner as the cellblock's list gave it, its counts read off their atomic words and written after, since a line
// written to may take a cell of the same cellblock
typedef struct
{
    const char *name;
    unsigned long long in_use;
    unsigned long long hw;
} CliCellsOwnerRead;
#undef CLI_CELL_OWNER

typedef struct
{
    const char *kind;
    const VerbaLinea *line;
    CellOwner *owner;
    CliCellsOwnerRead *owners;
    VerbaLinea *out;
    unsigned long long persistent;
    unsigned long long temporary;
} CellsCtx;
// A line of the cells' log: written to the run this process is a member of as the record `cells/<kind>`, and to the
// process's error stream, where the harness saves the window's
EMBED_INLINE void cells_log(const CellsCtx *args)
{
    EMBED_CALL(cli_cells_table.reported, CliCellsCfg, .name = args->kind, .report = args->line);
    if (mmgr_line_ok(args->line))
    {
        (void)fprintf(stderr, "cells %s %.*s\n", args->kind, (int)args->line->at, args->line->out);
    }
}

// An owner registered in MMgr's queue, where it does not stand there already. A refusal that leaves it unregistered, an
// owner of a name already registered or a queue past its capacity, is logged: every take for that owner is refused
EMBED_INLINE void cells_owner_ready(const CellsCtx *args)
{
    if (mmgr_owner_registered(args->owner) || mmgr_owner_register(args->owner))
    {
        return;
    }
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, args->owner->name);
    mmgr_line_put(&line, " not registered: its name is registered already, or the queue holds ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_owner_count(), 1u);
    mmgr_line_put(&line, " owners");
    EMBED_CALL(cells_log, CellsCtx, .kind = "refused", .line = &line);
}

uint32_t mmgr_carcer_hw_clock_ms(void)
{
    // the clock read as MMgr reads it, a count of milliseconds that wraps at 2^32
    return (uint32_t)cli_clock_milliseconds();
}

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, named->name);
    mmgr_line_put(&line, " named for a cell of ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no registered owner");
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)bytes, 1u);
    mmgr_line_put(&line, " bytes: refused");
    EMBED_CALL(cells_log, CellsCtx, .kind = "misnamed", .line = &line);
}

const MinimumSecurityGuard *const g_cli_cells = &cli_cells_site.cli_cells_pool;

// the turnstiles the option's storage has capacity for, which the machine's processors are bounded to, and the rows of
// touches per thread it saves
#define CELLS_WIDTH_MOST 256u
#define CELLS_TOUCH_ROWS 64u

// What MMgr's movable option is given: storage whose address the linker settles, and the thread its mover runs on,
// saved at idle between moves and raised across each, since a reader waits on the copy
ParsMemoriaeInternae(cli_cells_movable,
                     MMGR_MOVABLE_STORAGE_BYTES(CLI_CELLS_MOVABLE, CELLS_WIDTH_MOST, CELLS_TOUCH_ROWS));
static CliThread s_mover;
static CliWake s_mover_wake = CLI_WAKE_INIT;

static embed_bool cells_mover_started(void (*run)(void *context), void *context)
{
    s_mover.run = run;
    s_mover.context = context;
    s_mover.priority = CLI_PRIORITY_IDLE;
    return (embed_bool)(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_mover) != 0);
}

static embed_bool cells_mover_waited(uint32_t milliseconds)
{
    (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake,
                     .milliseconds_u64 = milliseconds);
    return EMBED_TRUE;
}

static void cells_mover_moving(embed_bool moving)
{
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx,
               .priority = moving ? CLI_PRIORITY_WORK : CLI_PRIORITY_IDLE);
}

void cli_cells_movable_ready(void)
{
    const unsigned int processors = cli_processors();
    // a refusal is the option assigned already, or the thread not started, which leaves the cells working and still
    (void)g_cli_cells->movable_init(
        &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_cli_cells_movable,
                                .storage_bytes = cli_cells_movable_bytes,
                                .cells = CLI_CELLS_MOVABLE,
                                .width = (processors < CELLS_WIDTH_MOST) ? processors : CELLS_WIDTH_MOST,
                                .rows = CELLS_TOUCH_ROWS,
                                .thread_started = cells_mover_started,
                                .thread_id = cli_thread_id,
                                .thread_wait = cells_mover_waited,
                                .thread_yield = cli_thread_yield,
                                .thread_moving = cells_mover_moving});
}

// the pool's bytes, 0 while no block is allocated
static unsigned long long cells_pool_bytes(void)
{
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    return (record != NULL) ? (unsigned long long)record->size : 0ull;
}

CellOwner *cli_cell_owner(const CliCellsCfg *args)
{
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    return ((args->cell != NULL) && (record != NULL)) ? mmgr_owner_of_buf(record, args->cell) : NULL;
}

// the owners in MMgr's owner queue copied into `owners`, sized for MMGR_CARCER_OWNERS: their count. An entry the queue
// counts and has not written yet is passed over
EMBED_INLINE unsigned int cells_owners_read(const CellsCtx *args)
{
    const size_t list_entries = mmgr_owner_count();
    unsigned int count = 0u;
    for (size_t at = 0u; at < list_entries; at += 1u)
    {
        const CellOwner *const owner = mmgr_owner_at(at);
        if (owner != NULL)
        {
            args->owners[count] = (CliCellsOwnerRead){owner->name, (unsigned long long)mmgr_owner_in_use(owner),
                                                      (unsigned long long)mmgr_owner_hw(owner)};
            count += 1u;
        }
    }
    return count;
}

// a report, `<pool's bytes> <persistent> <temporary>`, written into `out`
EMBED_INLINE void cells_write(const CellsCtx *args)
{
    mmgr_line_decimal(args->out, cells_pool_bytes(), 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->persistent, 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->temporary, 1u);
}

void cli_cells_on_report(const CliCellsCfg *args)
{
    if ((qry_member_run() == NULL) || !mmgr_line_ok(args->report))
    {
        return;
    }
    char named[QRY_NAME_MAX_LEN + 1u];
    VerbaLinea name_line = {named, sizeof(named), 0u, NULL, NULL, 0u};
    mmgr_line_put(&name_line, "cells/");
    mmgr_line_put(&name_line, args->name);
    if (mmgr_line_finish(&name_line) != 0u)
    {
        (void)qry_member_append(named, args->report->out, args->report->at, QRY_ORDINARY);
    }
}

int cli_cells_init(const CliCellsCfg *args)
{
    // every owner registered ahead of the first take, and a pool made anew finds them registered
    static CellOwner *const s_owners[] = {
#define CLI_CELL_OWNER(name_, text_) &name_,
#include "cells_owners.h"
#undef CLI_CELL_OWNER
    };
    for (size_t at = 0u; at < (sizeof(s_owners) / sizeof(s_owners[0])); at += 1u)
    {
        EMBED_CALL(cells_owner_ready, CellsCtx, .owner = s_owners[at]);
    }
    // a program makes its pool before any thread takes a cell, and makes it anew only while no cell is allocated, and
    // nothing takes between the test and the making
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    const int idle = (record == NULL) || mmgr_cellblock_idle(record);
    // the block's bytes are what the system can hand out at once, which a size_t counts
    return idle && cli_cells_site_made((size_t)args->bytes);
}

void cli_cells_report(const CliCellsCfg *args)
{
    unsigned long long *pool = args->pool;
    unsigned long long *persistent = args->persistent;
    unsigned long long *temporary = args->temporary;
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    *pool = cells_pool_bytes();
    *persistent = (record != NULL) ? (unsigned long long)mmgr_cellblock_persistent_hw(record) : 0ull;
    *temporary = (record != NULL) ? (unsigned long long)mmgr_cellblock_temporary_hw(record) : 0ull;
}

int cli_cells_zero(void)
{
    return mmgr_host_cellblock_zero(cli_cells_site_cellblock());
}

void cli_cells_usage_write(const CliCellsCfg *args)
{
    VerbaLinea *out = args->out;
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &persistent, .temporary = &temporary);
    EMBED_CALL(cells_write, CellsCtx, .out = out, .persistent = persistent, .temporary = temporary);
}

void cli_cells_owner_usage_write(const CliCellsCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cells_write, CellsCtx, .out = out, .persistent = (unsigned long long)mmgr_owner_hw(args->owner),
               .temporary = 0ull);
}

void cli_cells_owners_write(const CliCellsCfg *args)
{
    CliCellsOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(cells_owners_read, CellsCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(args->out, owners[at].name);
        mmgr_line_ch(args->out, '\t');
        mmgr_line_decimal(args->out, owners[at].in_use, 1u);
        mmgr_line_ch(args->out, '\t');
        mmgr_line_decimal(args->out, owners[at].hw, 1u);
        mmgr_line_ch(args->out, '\n');
    }
}

void cli_cells_peaks_report(void)
{
    if (qry_member_run() == NULL)
    {
        return;
    }
    char report[96];
    VerbaLinea line = {report, sizeof(report), 0u, NULL, NULL, 0u};
    EMBED_CALL(cli_cells_table.usage_write, CliCellsCfg, .out = &line);
    EMBED_CALL(cli_cells_table.reported, CliCellsCfg, .name = "cli_cells", .report = &line);
    CliCellsOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(cells_owners_read, CellsCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line.at = 0u;
        EMBED_CALL(cells_write, CellsCtx, .out = &line, .persistent = owners[at].hw, .temporary = 0ull);
        EMBED_CALL(cli_cells_table.reported, CliCellsCfg, .name = owners[at].name, .report = &line);
    }
}
