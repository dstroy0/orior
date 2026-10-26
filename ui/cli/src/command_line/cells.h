// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.h: the cells every module stores its work in: one cellblock of MMgr's host site over a block taken from the
// system for the bytes requested, taken from and given back to through its custodia alone, whose lock every take and
// give runs under and whose owner list records each module that took a cell; and the reports of the cellblock and of
// each owner to the telemetry (cells.c)
#ifndef CELLS_H
#define CELLS_H

#include "file_writing.h"
#include "locus_carcerum/locus_carcerum.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the pool every program makes as it starts: the most bytes its two ends occupy at once, as the cellblock's own peaks
// give them, taken up to the next 32 MiB
#define CLI_CELLS_POOL ((256ull << 20u) + (32ull << 20u))

    // The cellblock's custodia. A cell is taken with persistent_buf_alloc(bytes, owner) and given back with
    // persistent_buf_release(cell, owner), owner the module's CellOwner or NULL to count it to none. A take the
    // cellblock cannot meet returns NULL, which the caller reports where and as it chooses. Read once the pool is made
    extern const MaximumSecurityGuard *const g_cli_cells;

    // the pool made over a block of `bytes` taken from the system, in the place of the one before, which is given back
    // whole: 1, or 0 where a cell is still allocated or the system gives no block; every program makes one before its
    // first take
    int cli_cells_made(unsigned long long bytes);
    // whether the pool stands as the system handed it out, no cell taken and every byte zero: a check's question
    int cli_cells_zero(void);
    // the pool's bytes and the cellblock's two peaks, read off its state, each 0 where the build tracks none
    void cli_cells_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary);

    // the cellblock's own report, `<pool's bytes> <persistent peak> <temporary peak>`, read off its state, written
    // into `out`, each peak 0 where the build tracks none (MMGR_ENABLE_HW_MEM_CAPACITY_CB); and an owner's,
    // `<pool's bytes> <its peak> 0`
    void cli_cells_usage_written(VerbaLinea *out);
    void cli_cells_owner_usage_written(const CellOwner *owner, VerbaLinea *out);
    // every owner on the cellblock's list, as JSON, `[{"name", "in_use", "hw"}...]`, in the order of their first
    // takes: the audit of who owns what in the cellblock
    void cli_cells_owners_written(VerbaLinea *out);

    // a report appended as the record `cells/<name>` to the run this process is a member of, and nothing where it is a
    // member of none
    void cli_cells_reported(const char *name, const VerbaLinea *report);
    // at a command's end: the cellblock's peaks recorded as `cells/cli_cells` and each listed owner's as
    // `cells/<name>`, to the run this process is a member of
    void cli_cells_peaks_reported(void);

#ifdef __cplusplus
}
#endif

#endif
