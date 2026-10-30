// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.c: the cells every module stores its work in: one cellblock over one pool, a block taken from the system for
// the bytes requested as a program starts and made anew for another request while no cell is allocated. Every take and
// give goes through the cellblock's custodia, each one swap on an atomic word, and counts the cell to the owner named,
// registered in MMgr's owner queue the first time a take, give or growth names it. MMgr's clock is the CLI's, and its
// reports of a cell given back at the end of its lifetime and of a give naming the wrong owner are logged. The reports
// read the cellblock's peaks and the owner queue off their atomic words, and are written at a command's end
#include "cells.h"

#include "cli_internal.h"

#include <stdio.h>

// The pool: MMgr's host site, one block taken from the system for the bytes requested, zero as the system hands it out,
// the cellblock's record at its start; made anew for another request, the block before given back whole, while no cell
// is allocated. Maximum security: a cell is wiped to zero as it is given back, and every cell handed out is zero, the
// host block's bytes being zero from the start, and no room reads the bytes of the one that occupied its cell before it
LocusCarcerumHospes(cli_cells_site, MMGR_MAXIMUM_SECURITY(cli_cells_pool));

// the cellblock's record, as the site declares it
#define CLI_CELLS_RECORD cli_cells_site_cli_cells_pool_ctx

CellOwner g_cli_code_cells = MMGR_CELL_OWNER("cli_code");
CellOwner g_cli_command_line_cells = MMGR_CELL_OWNER("cli_command_line");
CellOwner g_cli_deploy_cells = MMGR_CELL_OWNER("cli_deploy");
CellOwner g_cli_editor_cells = MMGR_CELL_OWNER("cli_editor");
CellOwner g_cli_inspect_cells = MMGR_CELL_OWNER("cli_inspect");
CellOwner g_cli_machine_cells = MMGR_CELL_OWNER("cli_machine");
CellOwner g_cli_platform_cells = MMGR_CELL_OWNER("cli_platform");
CellOwner g_cli_print_cells = MMGR_CELL_OWNER("cli_print");
CellOwner g_cli_programs_cells = MMGR_CELL_OWNER("cli_programs");
CellOwner g_cli_runs_cells = MMGR_CELL_OWNER("cli_runs");
CellOwner g_cli_servers_cells = MMGR_CELL_OWNER("cli_servers");
CellOwner g_cli_tests_cells = MMGR_CELL_OWNER("cli_tests");
CellOwner g_cli_text_cells = MMGR_CELL_OWNER("cli_text");
CellOwner g_cli_toolchains_cells = MMGR_CELL_OWNER("cli_toolchains");
CellOwner g_cli_window_cells = MMGR_CELL_OWNER("cli_window");
CellOwner g_cli_bundle_cells = MMGR_CELL_OWNER("cli_bundle");
CellOwner g_cli_build_cells = MMGR_CELL_OWNER("cli_builds");
CellOwner g_cli_comms_cells = MMGR_CELL_OWNER("cli_comms");
CellOwner g_cli_db_cells = MMGR_CELL_OWNER("cli_databases");
CellOwner g_cli_debug_cells = MMGR_CELL_OWNER("cli_debug");
CellOwner g_cli_notebook_cells = MMGR_CELL_OWNER("cli_notebooks");
CellOwner g_cli_plugins_cells = MMGR_CELL_OWNER("cli_plugins");
CellOwner g_cli_requests_cells = MMGR_CELL_OWNER("cli_requests");
CellOwner g_gui_cells = MMGR_CELL_OWNER("gui");
CellOwner g_checks_cells = MMGR_CELL_OWNER("checks");

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

// An owner registered in MMgr's queue the first time a take, give or growth names it. Two threads naming it at once
// both ask, and MMgr registers it once and refuses the second, whose owner then stands registered. A refusal that
// leaves the owner unregistered, an owner of a name already registered or a queue past its room, is logged: every take
// for that owner is refused
static void cells_owner_ready(CellOwner *owner)
{
    if ((owner == NULL) || (embed_atomic_usize_load(&owner->place) != 0u) || mmgr_owner_register(owner) ||
        (embed_atomic_usize_load(&owner->place) != 0u))
    {
        return;
    }
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, (owner->name != NULL) ? owner->name : "(no name)");
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

// The shared cellblock's entries, each registering the owner it is handed before the site's own entry runs
static void *cells_persistent_buf_alloc(size_t size, CellOwner *owner)
{
    cells_owner_ready(owner);
    return cli_cells_site.cli_cells_pool.persistent_buf_alloc(size, owner);
}

static void cells_persistent_buf_release(void *prisoner, CellOwner *owner)
{
    cells_owner_ready(owner);
    cli_cells_site.cli_cells_pool.persistent_buf_release(prisoner, owner);
}

static void *cells_temporary_buf_alloc(size_t size, CellOwner *owner)
{
    cells_owner_ready(owner);
    return cli_cells_site.cli_cells_pool.temporary_buf_alloc(size, owner);
}

static size_t cells_temporary_buf_mark(void)
{
    return cli_cells_site.cli_cells_pool.temporary_buf_mark();
}

static void cells_temporary_buf_release(size_t mark)
{
    cli_cells_site.cli_cells_pool.temporary_buf_release(mark);
}

static void cells_temporary_buf_reset(void)
{
    cli_cells_site.cli_cells_pool.temporary_buf_reset();
}

static embed_bool cells_who_owns_buf(const void *at)
{
    return cli_cells_site.cli_cells_pool.who_owns_buf(at);
}

static size_t cells_buf_available(void)
{
    return cli_cells_site.cli_cells_pool.buf_available();
}

static void *cells_buf_grow(void *prisoner, size_t size, CellOwner *owner)
{
    cells_owner_ready(owner);
    return cli_cells_site.cli_cells_pool.buf_grow(prisoner, size, owner);
}

static const MaximumSecurityGuard s_cells_guard = {
    cells_persistent_buf_alloc, cells_persistent_buf_release, cells_temporary_buf_alloc,
    cells_temporary_buf_mark,   cells_temporary_buf_release,  cells_temporary_buf_reset,
    cells_who_owns_buf,         cells_buf_available,          cells_buf_grow};

const MaximumSecurityGuard *const g_cli_cells = &s_cells_guard;

// a peak read off the cellblock's state, `member_` persistent_hw or temporary_hw, 0 where the build tracks none
#if defined(MMGR_ENABLE_HW_MEM_CAPACITY_CB) && MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CLI_CELLS_PEAK(member_)                                                                                        \
    ((CLI_CELLS_RECORD != NULL) ? (unsigned long long)embed_atomic_usize_load(&CLI_CELLS_RECORD->member_) : 0ull)
#else
#define CLI_CELLS_PEAK(member_) 0ull
#endif

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
    return (CLI_CELLS_RECORD != NULL) ? (unsigned long long)CLI_CELLS_RECORD->size : 0ull;
}

CellOwner *cli_cell_owner(const void *cell)
{
    return ((cell != NULL) && (CLI_CELLS_RECORD != NULL)) ? mmgr_owner_of_buf(CLI_CELLS_RECORD, cell) : NULL;
}

void *cli_cell_beside(const void *cell, size_t size, CellOwner *owner)
{
    // a persistent cell lies below the persistent end
    const int temporary = (cell != NULL) && (CLI_CELLS_RECORD != NULL) &&
                          ((size_t)((const uint8_t *)cell - CLI_CELLS_RECORD->base) >= mmgr_cellblock_persistent_end(CLI_CELLS_RECORD));
    return temporary ? g_cli_cells->temporary_buf_alloc(size, owner) : g_cli_cells->persistent_buf_alloc(size, owner);
}

void *cli_cell_regrown(void *cell, size_t used, size_t size, CellOwner *owner)
{
    void *const grown = g_cli_cells->buf_grow(cell, size, owner);
    if (grown != NULL)
    {
        return grown;
    }
    void *const larger = cli_cell_beside(cell, size, owner);
    if (larger == NULL)
    {
        return NULL;
    }
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = larger, .src = cell, .bytes = used});
        g_cli_cells->persistent_buf_release(cell, owner);
    }
    return larger;
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
            owners[count] =
                (CliCellsOwnerRead){owner->name, (unsigned long long)embed_atomic_usize_load(&owner->in_use),
                                    (unsigned long long)embed_atomic_usize_load(&owner->hw)};
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
    // a program makes its pool before any thread takes a cell, and makes it anew only while no cell is allocated: the
    // idle test reads both tiers off one word, and nothing takes between the test and the making
    const int idle =
        (CLI_CELLS_RECORD == NULL) || ((mmgr_cellblock_persistent_end(CLI_CELLS_RECORD) == 0u) &&
                                       (mmgr_cellblock_temporary_top(CLI_CELLS_RECORD) == CLI_CELLS_RECORD->size));
    // the block's bytes are what the system can hand out at once, which a size_t counts
    return idle && mmgr_host_cellblock_made(&CLI_CELLS_RECORD, (size_t)bytes);
}

void cli_cells_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    *pool = cells_pool_bytes();
    *persistent = CLI_CELLS_PEAK(persistent_hw);
    *temporary = CLI_CELLS_PEAK(temporary_hw);
}

int cli_cells_zero(void)
{
    int zero = mmgr_host_cellblock_zero(CLI_CELLS_RECORD);
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
    cells_written(out, (unsigned long long)embed_atomic_usize_load(&owner->hw), 0ull);
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
