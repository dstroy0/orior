// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.h: the cells every module stores its work in: one cellblock of MMgr's host site over a block taken from the
// system for the bytes requested, taken from and given back to through its custodia alone, each take and give one swap
// on an atomic word, whose owner list records each module that took a cell; and the reports of the cellblock and of
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

// The lanes: a cellblock of its own for each context that runs a command, the window's rooms, the terminal's one
// command and a check's process, each over a block of CLI_CELLS_LANE_POOL bytes taken from the system at the lane's
// first entry. A command's own cells, those it gives back before it ends, are taken from its lane's temporary tier,
// marked as the command begins and rewound as it ends, zeroed; what outlives a command is taken from the shared
// cellblock's persistent tier
#define CLI_CELLS_LANES 8u
#define CLI_CELLS_LANE_POOL (128ull << 20u)

    // The shared cellblock's custodia, where what outlives a command lives. A cell is taken with
    // persistent_buf_alloc(bytes, owner) and given back with persistent_buf_release(cell, owner), owner the module's
    // CellOwner or NULL to count it to none. A take the cellblock cannot meet returns NULL, which the caller reports
    // where and as it chooses. Read once the pool is made
    extern const MaximumSecurityGuard *const g_cli_cells;

    // this thread put on lane `lane`, below CLI_CELLS_LANES, the lane's cellblock made at its first entry, and the
    // lane's mark taken: the mark to rewind to, or 0 with the thread on no lane where the lane's block cannot be taken
    // from the system
    size_t cli_lane_entered(unsigned int lane);
    // this thread taken off its lane, the lane's cells kept until its rewind
    void cli_lane_left(void);
    // lane `lane` rewound to `mark` as cli_lane_entered gave it, every cell taken since dropped and zeroed, and this
    // thread taken off the lane where it is on it
    void cli_lane_rewound(unsigned int lane, size_t mark);
    // a command's own cell of `size` bytes for `owner`: from this thread's lane's temporary tier, or from the shared
    // cellblock's persistent tier where the thread is on no lane; NULL where refused
    void *cli_lane_alloc(size_t size, CellOwner *owner);
    // `cell` given back, zeroed, to the cellblock it was taken from, the shared one or a lane's; NULL gives nothing
    void cli_cell_release(void *cell, CellOwner *owner);
    // `cell` grown to `size` bytes where it stands, in the cellblock it was taken from, where it is the newest cell of
    // its tier: the cell after the growth, at a lower address where a lane's cell moved down, its bytes kept and the
    // added bytes zero; NULL with nothing changed where a cell was taken above it since or the gap cannot meet the
    // growth, and the caller takes a larger cell, copies and gives the first back
    void *cli_cell_grown(void *cell, size_t size, CellOwner *owner);

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
