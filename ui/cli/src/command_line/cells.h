// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.h: the cells every module stores its work in: one cellblock of MMgr's host site over a block taken from the
// system for the bytes requested, taken from and given back to through its custodia alone, each take and give naming
// the owner its cell counts to, every owner listed in cells_owners.h and registered in MMgr's owner queue as the pool
// is made; and the reports of the cellblock and of each owner to the telemetry (cells.c)
#ifndef CELLS_H
#define CELLS_H
typedef struct CliCellsCfg CliCellsCfg;

#include "embed_dispatch_layout.h"
#include "locus_carcerum/locus_carcerum.h"
#include "verba_scribo/verba_linea.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the pool every program makes as it starts: the most bytes its two ends occupy at once, as the cellblock's own peaks
// give them, taken up to the next 32 MiB
#define CLI_CELLS_POOL ((256ull << 20u) + (32ull << 20u))

// the most movable cells the pool names at once
#define CLI_CELLS_MOVABLE (131072u + 1u)

    // The shared cellblock's custodia, where what outlives a command lives. A cell is taken with
    // persistent_buf_alloc(bytes, owner) and given back with persistent_buf_release(cell, owner), owner the
    // CellOwner the cell counts to: a give naming another is refused and logged. A take the cellblock cannot meet
    // returns NULL, which the caller reports where and as it chooses. Read once the pool is made
    extern const MinimumSecurityGuard *const g_cli_cells;

    // The shared cellblock's owners, one for each module that takes from it
#define CLI_CELL_OWNER(name_, text_) extern CellOwner name_;
#include "cells_owners.h"
#undef CLI_CELL_OWNER

    // the owner `cell` counts to, read off the cell in the cellblock it was taken from: the owner a holder names that
    // gives back a cell another module took for it, NULL where `cell` is NULL or no cell is taken
    CellOwner *cli_cell_owner(const CliCellsCfg *args);

    // the pool made over a block of `bytes` taken from the system, in the place of the one before, which is given back
    // whole: 1, or 0 where a cell is still allocated or the system gives no block; every program makes one before its
    // first take
    int cli_cells_init(const CliCellsCfg *args);
    // whether the pool stands as the system handed it out, no cell taken and every byte zero: a check's question
    int cli_cells_zero(void);
    // MMgr's movable option assigned to the pool, once, and given the thread its mover runs on. A module that stores
    // movable cells calls this before its first take
    void cli_cells_movable_ready(void);
    // the pool's bytes and the cellblock's two peaks, read off its state, each 0 where the build tracks none
    void cli_cells_report(const CliCellsCfg *args);

    // the cellblock's own report, `<pool's bytes> <persistent peak> <temporary peak>`, read off its state, written
    // into `out`, each peak 0 where the build tracks none (MMGR_ENABLE_HW_MEM_CAPACITY_CB); and an owner's,
    // `<pool's bytes> <its peak> 0`
    void cli_cells_usage_write(const CliCellsCfg *args);
    void cli_cells_owner_usage_write(const CliCellsCfg *args);
    // every owner in MMgr's owner queue, as JSON, `[{"name", "in_use", "hw"}...]`, in the order they registered: the
    // audit of who owns what in the cellblock and the lanes
    void cli_cells_owners_write(const CliCellsCfg *args);

    // a report appended as the record `cells/<name>` to the run this process is a member of, and nothing where it is a
    // member of none
    void cli_cells_on_report(const CliCellsCfg *args);
    // at a command's end: the cellblock's peaks recorded as `cells/cli_cells` and each registered owner's as
    // `cells/<name>`, to the run this process is a member of
    void cli_cells_peaks_report(void);

    struct CliCellsCfg
    {
        const void *cell;
        unsigned long long bytes;
        unsigned long long *pool;
        unsigned long long *persistent;
        unsigned long long *temporary;
        VerbaLinea *out;
        const CellOwner *owner;
        const char *name;
        const VerbaLinea *report;
    };

    typedef struct
    {
        CellOwner *(*cell_owner)(const CliCellsCfg *args);
        int (*init)(const CliCellsCfg *args);
        void (*report)(const CliCellsCfg *args);
        void (*usage_write)(const CliCellsCfg *args);
        void (*owner_usage_write)(const CliCellsCfg *args);
        void (*owners_write)(const CliCellsCfg *args);
        void (*reported)(const CliCellsCfg *args);
    } CliCellsNs;
    EMBED_TABLE_LAYOUT(CliCellsNs, cell_owner, init, report, usage_write, owner_usage_write, owners_write, reported);
    EMBED_TABLE_STORAGE CliCellsNs cli_cells_table EMBED_UNUSED = {
        .cell_owner = cli_cell_owner,
        .init = cli_cells_init,
        .report = cli_cells_report,
        .usage_write = cli_cells_usage_write,
        .owner_usage_write = cli_cells_owner_usage_write,
        .owners_write = cli_cells_owners_write,
        .reported = cli_cells_on_report,
    };

#ifdef __cplusplus
}
#endif

#endif
