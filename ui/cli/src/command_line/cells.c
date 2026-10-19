// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.c: the cells every module holds its work in: one cellblock over one pool, its persistent end taken from and
// given back to cell by cell, every call of it under one lock. Each cell keeps its bytes ahead of its start, so a
// module that counts what it holds takes them off as it gives the cell back; a module's mark is raised as its
// cellblock's are, the larger of the mark and what is held. The cellblock reports its own peaks as they rise a step,
// and each module its own mark
#include "cells.h"

#include "cli_internal.h"
#include "locus_carcerum/locus_carcerum.h"

#include <stdint.h>

_Static_assert(MMGR_CARCER_ALIGN <= sizeof(size_t), "CLI_CELL_COST rounds a request to a word no wider than a size_t");
_Static_assert((CLI_CELL_ALIGN & (CLI_CELL_ALIGN - 1u)) == 0u, "CLI_CELL_ALIGN must be a power of two");
_Static_assert((2u * CLI_CELL_ALIGN) <= 255u, "how far a cell's start moved must fit the byte ahead of it");
_Static_assert(CLI_CELL_ALIGN >= 16u, "a cell's bytes, a word, sit 16 bytes ahead of its start, past the byte there");

ParsMemoriaeInternae(cli_cells_pool, CLI_CELLS_POOL);
LocusCarcerum(cli_cells_site, MMGR_MINIMUM_SECURITY(cli_cells_pool));

// a peak read off the cellblock's state, `member_` persistent_hw or temporary_hw, 0 where the build keeps none
#if defined(MMGR_ENABLE_HW_MEM_CAPACITY_CB) && MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CLI_CELLS_PEAK(member_) ((unsigned long long)cli_cells_site_cli_cells_pool_ctx.member_)
#else
#define CLI_CELLS_PEAK(member_) 0ull
#endif

// the rise of a mark past the one last told that is told again
#define CLI_CELLS_TOLD_STEP (1ull << 20u)

static CliLock s_lock = CLI_LOCK_INIT;

// the cellblock's two peaks together as last told
static unsigned long long s_told;

// a report, `<pool's bytes> <persistent> <temporary>`, written into `out`
static void cells_written(CliLine *out, unsigned long long persistent, unsigned long long temporary)
{
    cli_line_decimal(out, CLI_CELLS_POOL, 1u);
    cli_line_character(out, ' ');
    cli_line_decimal(out, persistent, 1u);
    cli_line_character(out, ' ');
    cli_line_decimal(out, temporary, 1u);
}

// `*hw` raised to `used` where `used` is the larger, as the cellblock raises its own marks
static void cells_hw(unsigned long long *hw, unsigned long long used)
{
    // the comparison widened to a mask the full width of the two it selects between
    const unsigned long long mask = 0ull - (unsigned long long)(used > *hw);
    *hw = (*hw & ~mask) | (used & mask);
}

// whether `mark` rose CLI_CELLS_TOLD_STEP past `*told`, the mark then kept there: 1 where it is to be told
static int cells_risen(unsigned long long *told, unsigned long long mark)
{
    if (mark < (*told + CLI_CELLS_TOLD_STEP))
    {
        return 0;
    }
    *told = mark;
    return 1;
}

void cli_cells_told(const char *name, const CliLine *report)
{
    if ((qry_member_run() == NULL) || !cli_line_fits(report))
    {
        return;
    }
    char named[QRY_NAME_LONGEST + 1u];
    CliLine name_line = {named, sizeof(named), 0ull};
    cli_line_text(&name_line, "cells/");
    cli_line_text(&name_line, name);
    if (cli_line_closed(&name_line) != 0ull)
    {
        (void)qry_member_append(named, report->out, report->at, QRY_ORDINARY);
    }
}

void cli_cells_said(CliLine *out)
{
    cli_lock_take(&s_lock);
    const unsigned long long persistent = CLI_CELLS_PEAK(persistent_hw);
    const unsigned long long temporary = CLI_CELLS_PEAK(temporary_hw);
    cli_lock_give(&s_lock);
    cells_written(out, persistent, temporary);
}

void cli_cells_tally_said(const CliCellsTally *tally, CliLine *out)
{
    cli_lock_take(&s_lock);
    const unsigned long long hw = tally->hw;
    cli_lock_give(&s_lock);
    cells_written(out, hw, 0ull);
}

// a cell of `bytes` for `what`, counted to `tally` where it is given, and the reports told whose marks rose a step
static void *cells_taken(CliCellsTally *tally, unsigned long long bytes, const char *what)
{
    unsigned char *cell = NULL;
    int rose = 0;
    int tally_rose = 0;
    unsigned long long tally_hw = 0ull;
    // a request past the pool is refused before the boundaries' width is added to it, so the sum cannot wrap, and a
    // request no larger than the pool fits a size_t
    if (bytes <= CLI_CELLS_POOL)
    {
        cli_lock_take(&s_lock);
        cell = cli_cells_site.cli_cells_pool.persistent_buf_alloc((size_t)(bytes + (2u * CLI_CELL_ALIGN)));
        rose = cells_risen(&s_told, CLI_CELLS_PEAK(persistent_hw) + CLI_CELLS_PEAK(temporary_hw));
        if ((cell != NULL) && (tally != NULL))
        {
            tally->held += bytes;
            cells_hw(&tally->hw, tally->held);
            tally_rose = cells_risen(&tally->told, tally->hw);
            tally_hw = tally->hw;
        }
        cli_lock_give(&s_lock);
    }
    char report[96];
    CliLine line = {report, sizeof(report), 0ull};
    if (rose)
    {
        cli_cells_said(&line);
        cli_cells_told("cli_cells", &line);
    }
    if (tally_rose)
    {
        line.at = 0ull;
        cells_written(&line, tally_hw, 0ull);
        cli_cells_told(tally->name, &line);
    }
    if (cell == NULL)
    {
        char told[512];
        CliLine refused = {told, sizeof(told), 0ull};
        cli_line_text(&refused, "orior: no cell of ");
        cli_line_decimal(&refused, bytes, 1u);
        cli_line_text(&refused, " bytes for ");
        cli_line_text(&refused, what);
        cli_line_text(&refused, ": the cellblock cannot meet it\n");
        query_terminal_written(1, told, cli_line_fits(&refused) ? refused.at : 0ull);
        return NULL;
    }
    // the start moves up to the second boundary past the cell's first byte, CLI_CELL_ALIGN + 1 to twice that, the
    // byte ahead of the start holding how far and the word 16 bytes ahead the cell's bytes; an address is a pointer's
    // width, which uintptr_t holds
    const uintptr_t at = (uintptr_t)cell;
    const uintptr_t aligned = (at + (2u * CLI_CELL_ALIGN)) & ~(uintptr_t)(CLI_CELL_ALIGN - 1u);
    unsigned char *const start = cell + (aligned - at);
    // the distance is CLI_CELL_ALIGN + 1 to twice that, which the assertion above holds within a byte
    start[-1] = (unsigned char)(aligned - at);
    memor.cpy(&(MemoriaCfg){.dst = start - 16, .src = &bytes, .bytes = sizeof(bytes)});
    return start;
}

// a cell given back, its bytes taken off `tally` where it is given
static void cells_given(CliCellsTally *tally, void *cell)
{
    if (cell == NULL)
    {
        return;
    }
    unsigned char *const start = cell;
    unsigned char *const first = start - start[-1];
    unsigned long long bytes = 0ull;
    memor.cpy(&(MemoriaCfg){.dst = &bytes, .src = start - 16, .bytes = sizeof(bytes)});
    cli_lock_take(&s_lock);
    cli_cells_site.cli_cells_pool.persistent_buf_release(first);
    if (tally != NULL)
    {
        tally->held -= bytes;
    }
    cli_lock_give(&s_lock);
}

void *cli_cell_taken(unsigned long long bytes, const char *what)
{
    return cells_taken(NULL, bytes, what);
}

void cli_cell_given(void *cell)
{
    cells_given(NULL, cell);
}

void *cli_cell_taken_by(CliCellsTally *tally, unsigned long long bytes, const char *what)
{
    return cells_taken(tally, bytes, what);
}

void cli_cell_given_by(CliCellsTally *tally, void *cell)
{
    cells_given(tally, cell);
}
