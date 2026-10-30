// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.h: the cells every module stores its work in: one cellblock of MMgr's host site over a block taken from the
// system for the bytes requested, taken from and given back to through its custodia alone, each take and give one swap
// on an atomic word and each naming the owner its cell counts to, registered in MMgr's owner queue the first time a
// take names it; and the reports of the cellblock and of each owner to the telemetry (cells.c)
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

    // The shared cellblock's custodia, where what outlives a command lives. A cell is taken with
    // persistent_buf_alloc(bytes, owner) and given back with persistent_buf_release(cell, owner), owner the
    // CellOwner the cell counts to: a give naming another is refused and logged. An owner is registered in MMgr's
    // queue the first time a take, give or growth names it. A take the cellblock cannot meet returns NULL, which the
    // caller reports where and as it chooses. Read once the pool is made
    extern const MaximumSecurityGuard *const g_cli_cells;

    // The owners of the modules that name no owner of their own: each source folder of the command line, the window,
    // the query tool and the checks, every cell a module's source takes or gives without one counted to its module's
    extern CellOwner g_cli_code_cells;
    extern CellOwner g_cli_command_line_cells;
    extern CellOwner g_cli_deploy_cells;
    extern CellOwner g_cli_editor_cells;
    extern CellOwner g_cli_inspect_cells;
    extern CellOwner g_cli_machine_cells;
    extern CellOwner g_cli_platform_cells;
    extern CellOwner g_cli_print_cells;
    extern CellOwner g_cli_programs_cells;
    extern CellOwner g_cli_runs_cells;
    extern CellOwner g_cli_servers_cells;
    extern CellOwner g_cli_tests_cells;
    extern CellOwner g_cli_text_cells;
    extern CellOwner g_cli_toolchains_cells;
    extern CellOwner g_cli_window_cells;
    extern CellOwner g_cli_bundle_cells;
    extern CellOwner g_cli_build_cells;
    extern CellOwner g_cli_comms_cells;
    extern CellOwner g_cli_db_cells;
    extern CellOwner g_cli_debug_cells;
    extern CellOwner g_cli_notebook_cells;
    extern CellOwner g_cli_plugins_cells;
    extern CellOwner g_cli_requests_cells;
    extern CellOwner g_gui_cells;
    extern CellOwner g_query_cells;
    extern CellOwner g_checks_cells;

    // a cell of `size` bytes for `owner` from the tier `cell` lies in, the persistent one where `cell` is NULL
    void *cli_cell_beside(const void *cell, size_t size, CellOwner *owner);

    // the owner `cell` counts to, read off the cell in the cellblock it was taken from: the owner a holder names that
    // grows or gives back a cell another module took for it, NULL where `cell` is NULL or no cell is taken
    CellOwner *cli_cell_owner(const void *cell);
    // `cell`, holding `used` bytes, made `size` bytes: grown where it stands, else a cell of `size` taken from where
    // `cell` was taken, the shared cellblock for NULL, the used bytes copied into it and `cell` given back; the cell
    // after, or NULL with `cell` as it was where no cell can be taken
    void *cli_cell_regrown(void *cell, size_t used, size_t size, CellOwner *owner);

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
    // every owner in MMgr's owner queue, as JSON, `[{"name", "in_use", "hw"}...]`, in the order they registered: the
    // audit of who owns what in the cellblock and the lanes
    void cli_cells_owners_written(VerbaLinea *out);

    // a report appended as the record `cells/<name>` to the run this process is a member of, and nothing where it is a
    // member of none
    void cli_cells_reported(const char *name, const VerbaLinea *report);
    // at a command's end: the cellblock's peaks recorded as `cells/cli_cells` and each registered owner's as
    // `cells/<name>`, to the run this process is a member of
    void cli_cells_peaks_reported(void);

#ifdef __cplusplus
}
#endif

#endif
