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
// is allocated. Maximum security: a cell is wiped to zero as it is given back, and every cell handed out is zero, the
// host block's bytes being zero from the start, and no room reads the bytes of the one that occupied its cell before it
LocusCarcerumHospes(cli_cells_site, MMGR_MAXIMUM_SECURITY(cli_cells_pool));

#define CLI_CELL_OWNER(name_, text_) CellOwner name_ = MMGR_CELL_OWNER(text_);
#include "cells_owners.h"
#undef CLI_CELL_OWNER

// A line of the cells' log: written to the run this process is a member of as the record `cells/<kind>`, and to the
// process's error stream, where the harness keeps the window's
static void cells_logged(const char *kind, const VerbaLinea *line)
{
    cli_cells_reported(kind, line);
    if (mmgr_line_ok(line))
    {
        (void)fprintf(stderr, "cells %s %.*s\n", kind, (int)line->at, line->out);
    }
}

// An owner registered in MMgr's queue, where it does not stand there already. A refusal that leaves it unregistered, an
// owner of a name already registered or a queue past its room, is logged: every take for that owner is refused
static void cells_owner_ready(CellOwner *owner)
{
    if (mmgr_owner_registered(owner) || mmgr_owner_register(owner))
    {
        return;
    }
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, owner->name);
    mmgr_line_put(&line, " not registered: its name is registered already, or the queue holds ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_owner_count(), 1u);
    mmgr_line_put(&line, " owners");
    cells_logged("refused", &line);
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
    cells_logged("misnamed", &line);
}

const MaximumSecurityGuard *const g_cli_cells = &cli_cells_site.cli_cells_pool;

// the turnstiles the option's storage has room for, which the machine's processors are bounded to, and the rows of
// touches per thread it keeps
#define CELLS_WIDTH_MOST 256u
#define CELLS_TOUCH_ROWS 64u

// What MMgr's movable option is given: storage whose address the linker settles, and the thread its mover runs on, kept
// at idle between moves and raised across each, since a reader waits on the copy
ParsMemoriaeInternae(cli_cells_movable,
                     MMGR_MOVABLE_STORAGE_BYTES(CLI_CELLS_MOVABLE, CELLS_WIDTH_MOST, CELLS_TOUCH_ROWS));
static CliThread s_mover;
static CliLock s_mover_lock = CLI_LOCK_INIT;
static CliWake s_mover_wake = CLI_WAKE_INIT;

static embed_bool cells_mover_started(void (*run)(void *context), void *context)
{
    s_mover.run = run;
    s_mover.context = context;
    s_mover.priority = CLI_PRIORITY_IDLE;
    return (embed_bool)(cli_thread_started(&s_mover) != 0);
}

static embed_bool cells_mover_waited(uint32_t milliseconds)
{
    cli_lock_take(&s_mover_lock);
    (void)cli_wake_wait_for(&s_mover_wake, &s_mover_lock, milliseconds);
    cli_lock_give(&s_mover_lock);
    return EMBED_TRUE;
}

static void cells_mover_moving(embed_bool moving)
{
    cli_priority_own(moving ? CLI_PRIORITY_WORK : CLI_PRIORITY_IDLE);
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

// One owner as the cellblock's list gave it, its counts read off their atomic words and written after, since a line
// written to may take a cell of the same cellblock
typedef struct
{
    const char *name;
    unsigned long long in_use;
    unsigned long long hw;
} CliCellsOwnerRead;

// the pool's bytes, 0 while no block is allocated
static unsigned long long cells_pool_bytes(void)
{
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    return (record != NULL) ? (unsigned long long)record->size : 0ull;
}

CellOwner *cli_cell_owner(const void *cell)
{
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    return ((cell != NULL) && (record != NULL)) ? mmgr_owner_of_buf(record, cell) : NULL;
}

// the owners in MMgr's owner queue copied into `owners`, sized for MMGR_CARCER_OWNERS: their count. An entry the queue
// counts and has not written yet is passed over
static unsigned int cells_owners_read(CliCellsOwnerRead *owners)
{
    const size_t listed = mmgr_owner_count();
    unsigned int count = 0u;
    for (size_t at = 0u; at < listed; at += 1u)
    {
        const CellOwner *const owner = mmgr_owner_at(at);
        if (owner != NULL)
        {
            owners[count] = (CliCellsOwnerRead){owner->name, (unsigned long long)mmgr_owner_in_use(owner),
                                                (unsigned long long)mmgr_owner_hw(owner)};
            count += 1u;
        }
    }
    return count;
}

// a report, `<pool's bytes> <persistent> <temporary>`, written into `out`
static void cells_written(VerbaLinea *out, unsigned long long persistent, unsigned long long temporary)
{
    mmgr_line_decimal(out, cells_pool_bytes(), 1u);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, persistent, 1u);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, temporary, 1u);
}

void cli_cells_reported(const char *name, const VerbaLinea *report)
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

int cli_cells_made(unsigned long long bytes)
{
    // every owner registered ahead of the first take, and a pool made anew finds them registered
    static CellOwner *const s_owners[] = {
#define CLI_CELL_OWNER(name_, text_) &name_,
#include "cells_owners.h"
#undef CLI_CELL_OWNER
    };
    for (size_t at = 0u; at < (sizeof(s_owners) / sizeof(s_owners[0])); at += 1u)
    {
        cells_owner_ready(s_owners[at]);
    }
    // a program makes its pool before any thread takes a cell, and makes it anew only while no cell is allocated, and
    // nothing takes between the test and the making
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    const int idle = (record == NULL) || mmgr_cellblock_idle(record);
    // the block's bytes are what the system can hand out at once, which a size_t counts
    return idle && cli_cells_site_made((size_t)bytes);
}

void cli_cells_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    const CarcerCellBlock *const record = cli_cells_site_cellblock();
    *pool = cells_pool_bytes();
    *persistent = (record != NULL) ? (unsigned long long)mmgr_cellblock_persistent_hw(record) : 0ull;
    *temporary = (record != NULL) ? (unsigned long long)mmgr_cellblock_temporary_hw(record) : 0ull;
}

int cli_cells_zero(void)
{
    return mmgr_host_cellblock_zero(cli_cells_site_cellblock());
}

void cli_cells_usage_written(VerbaLinea *out)
{
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    cli_cells_report(&pool, &persistent, &temporary);
    cells_written(out, persistent, temporary);
}

void cli_cells_owner_usage_written(const CellOwner *owner, VerbaLinea *out)
{
    cells_written(out, (unsigned long long)mmgr_owner_hw(owner), 0ull);
}

void cli_cells_owners_written(VerbaLinea *out)
{
    CliCellsOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = cells_owners_read(owners);
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at != 0u) ? ",{\"name\":\"" : "{\"name\":\"");
        mmgr_line_put(out, owners[at].name);
        mmgr_line_put(out, "\",\"in_use\":");
        mmgr_line_decimal(out, owners[at].in_use, 1u);
        mmgr_line_put(out, ",\"hw\":");
        mmgr_line_decimal(out, owners[at].hw, 1u);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_cells_peaks_reported(void)
{
    if (qry_member_run() == NULL)
    {
        return;
    }
    char report[96];
    VerbaLinea line = {report, sizeof(report), 0u, NULL, NULL, 0u};
    cli_cells_usage_written(&line);
    cli_cells_reported("cli_cells", &line);
    CliCellsOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = cells_owners_read(owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line.at = 0u;
        cells_written(&line, owners[at].hw, 0ull);
        cli_cells_reported(owners[at].name, &line);
    }
}
