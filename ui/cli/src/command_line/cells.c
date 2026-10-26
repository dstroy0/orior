// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.c: the cells every module stores its work in: one cellblock over one pool, a block taken from the system for
// the bytes requested as a program starts and made anew for another request while no cell is allocated. Every take and
// give goes through the cellblock's custodia, under its lock, and counts the cell to the owner named; the cellblock
// lists each owner at its first take. The reports read the cellblock's peaks and its owner list, and are written at a
// command's end
#include "cells.h"

#include "cli_internal.h"

_Static_assert(MMGR_ENABLE_LOCK, "every thread of the command line takes cells from the one cellblock");
_Static_assert(MMGR_ENABLE_OWNERS, "the command line lists every owner of a cell off the cellblock");

// The pool: MMgr's host site, one block taken from the system for the bytes requested, zero as the system hands it out,
// the cellblock's record at its start; made anew for another request, the block before given back whole, while no cell
// is allocated. Maximum security: a cell is wiped to zero as it is given back, so every cell handed out is zero, the
// host block's bytes being zero from the start, and no room reads the bytes of the one that occupied its cell before it
LocusCarcerumHospes(cli_cells_site, MMGR_MAXIMUM_SECURITY(cli_cells_pool));

const MaximumSecurityGuard *const g_cli_cells = &cli_cells_site.cli_cells_pool;

// the cellblock's record and its lock, as the site declares them
#define CLI_CELLS_RECORD cli_cells_site_cli_cells_pool_ctx
#define CLI_CELLS_LOCK cli_cells_site_cli_cells_pool_lock

// a peak read off the cellblock's state, `member_` persistent_hw or temporary_hw, 0 where the build tracks none
#if defined(MMGR_ENABLE_HW_MEM_CAPACITY_CB) && MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CLI_CELLS_PEAK(member_) ((CLI_CELLS_RECORD != NULL) ? (unsigned long long)CLI_CELLS_RECORD->member_ : 0ull)
#else
#define CLI_CELLS_PEAK(member_) 0ull
#endif

// One owner as the cellblock's list gave it, read under the lock and written after it, since a line written to may
// take a cell through the same lock
typedef struct
{
    const char *name;
    unsigned long long in_use;
    unsigned long long hw;
} CliCellsOwnerRead;

// the pool's bytes, 0 while no block is allocated
static unsigned long long cells_pool_bytes(void)
{
    return (CLI_CELLS_RECORD != NULL) ? (unsigned long long)CLI_CELLS_RECORD->size : 0ull;
}

// the owners on the cellblock's list copied into `owners`, sized for MMGR_CARCER_OWNERS: their count
static unsigned int cells_owners_read(CliCellsOwnerRead *owners)
{
    MMGR_CARCER_ACQUIRE(CLI_CELLS_LOCK);
    const unsigned int count = (CLI_CELLS_RECORD != NULL) ? (unsigned int)CLI_CELLS_RECORD->owner_count : 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CellOwner *const owner = CLI_CELLS_RECORD->owners[at];
        owners[at] = (CliCellsOwnerRead){owner->name, owner->in_use, owner->hw};
    }
    MMGR_CARCER_RELEASE(CLI_CELLS_LOCK);
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
    // the idle test and the making under one acquisition of the lock, so no take lands between them
    MMGR_CARCER_ACQUIRE(CLI_CELLS_LOCK);
    const int idle = (CLI_CELLS_RECORD == NULL) || ((CLI_CELLS_RECORD->persistent_end == 0u) &&
                                                    (CLI_CELLS_RECORD->temporary_top == CLI_CELLS_RECORD->size));
    // the block's bytes are what the system can hand out at once, which a size_t counts
    const int made = idle && mmgr_host_cellblock_made(&CLI_CELLS_RECORD, (size_t)bytes);
    MMGR_CARCER_RELEASE(CLI_CELLS_LOCK);
    return made;
}

void cli_cells_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    MMGR_CARCER_ACQUIRE(CLI_CELLS_LOCK);
    *pool = cells_pool_bytes();
    *persistent = CLI_CELLS_PEAK(persistent_hw);
    *temporary = CLI_CELLS_PEAK(temporary_hw);
    MMGR_CARCER_RELEASE(CLI_CELLS_LOCK);
}

int cli_cells_zero(void)
{
    MMGR_CARCER_ACQUIRE(CLI_CELLS_LOCK);
    const int zero = mmgr_host_cellblock_zero(CLI_CELLS_RECORD);
    MMGR_CARCER_RELEASE(CLI_CELLS_LOCK);
    return zero;
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
    MMGR_CARCER_ACQUIRE(CLI_CELLS_LOCK);
    const unsigned long long hw = owner->hw;
    MMGR_CARCER_RELEASE(CLI_CELLS_LOCK);
    cells_written(out, hw, 0ull);
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
