// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.h: the CLI's memory as MMgr's prison site: one cellblock over the pool declared at CLI_POOL_BYTES, reached
// through its guard alone, every take and release carrying the owner the cell counts to (owners.h); the pool's
// emptying, its movable option, and the reports of the cellblock's peaks and of each owner's counts to the telemetry
// (pool.c)
#ifndef POOL_H
#define POOL_H
#include "locus_carcerum/locus_carcerum.h"
#include "owners.h"
#include "verba_scribo/verba_linea.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the pool every program is on: the max bytes its two ends occupy at once, as the cellblock's own peaks read
// them, rounded up to the next 32 MiB
#define CLI_POOL_BYTES ((256ull << 20u) + (32ull << 20u))

// the max movable cells the pool names at once
#define CLI_POOL_MOVABLE (131072u + 1u)

    // The cellblock's guard, MMgr's: a cell is taken with persistent_buf_alloc(bytes, owner) and given back with
    // persistent_buf_release(cell, owner), owner the module's own of owners.h. A take the cellblock cannot meet
    // returns NULL, which the caller reports where and as it chooses. Read once the pool is made
    extern const MinimumSecurityGuard *const g_cli_guard;

    // the pool emptied for a request of `bytes`, which the pool declared has to cover, the mover ended first and
    // started again with it: 1, or 0 where a cell is still allocated or the request is past it; every program empties
    // its pool before its first take
    int cli_pool_made(unsigned long long bytes);
    // the cellblock's record: what mmgr_owner_of_buf and the peak readers take
    const SeriesCellularum *cli_pool_series_cellularum(void);
    // MMgr's movable option assigned to the pool, once, and given the thread its mover runs on. cli_pool_made calls
    // this as the pool is made; a module that stores movable cells may call it again before its first take, to no
    // effect past the first
    void cli_pool_movable_ready(void);

    // A line written into one cell of the temporary tier taken once at `max` bytes: its text is whole in `cell`,
    // and past `max` the line fills as a line with no flush fills. `line.out` is where the next
    // write goes, inside `cell`; the text is read at `cell` for cli_grown_size bytes, terminated. Written through the
    // line's own entries, as any line; begun before its first write and ended after its last read
    typedef struct
    {
        char *cell;
        unsigned long long capacity;
        unsigned long long max;
        const CellOwner *owner;
        VerbaLinea line;
    } CliGrownLine;

    void cli_grown_begin(CliGrownLine *grown, unsigned long long max, const CellOwner *owner);
    // the bytes the text holds, what fit where a write did not; mmgr_line_ok over `line` says whether every write fit
    unsigned long long cli_grown_size(const CliGrownLine *grown);
    // the line begun over again at its cell's start, the cell retained at the size it grew to
    void cli_grown_reset(CliGrownLine *grown);
    void cli_grown_end(CliGrownLine *grown);

    // the pool's bytes and the cellblock's two peaks, read off its state, each 0 where the build tracks none
    void cli_pool_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary);
    // the cellblock's own report, `<pool's bytes> <persistent peak> <temporary peak>`, read off its state, written
    // into `out`, each peak 0 where the build tracks none (MMGR_ENABLE_HW_MEM_CAPACITY_CB); and an owner's,
    // `<pool's bytes> <its peak> 0`
    void cli_pool_usage_write(VerbaLinea *out);
    void cli_pool_owner_usage_write(const CellOwner *owner, VerbaLinea *out);
    // every owner of the table, a line each, its name, the bytes it has in use and the max it has had, a tab between
    // them, in the table's order: the audit of who owns what in the cellblock
    void cli_pool_owners_write(VerbaLinea *out);
    // a report appended as the record `cells/<name>` to the run this process is a member of, and nothing where it is a
    // member of none
    void cli_pool_reported(const char *name, const VerbaLinea *report);
    // at a command's end: the cellblock's peaks recorded as `cells/cli_cells` and each owner's as `cells/<name>`, to
    // the run this process is a member of
    void cli_pool_peaks_report(void);
    // MMgr's telemetry to the error stream, from the window's main thread on its memory tick. The line is
    // `cells telemetry <pool> <in use> <persistent peak> <temporary peak> <queue> <lane passes>`. Each owner whose
    // in-use count moved since the last line gets `cells owner <name> <in use> <peak>`, and each owner whose passes
    // through the turnstiles moved gets `cells touches <name> <passes>`
    void cli_pool_telemetry_log(void);

#ifdef __cplusplus
}
#endif

#endif
