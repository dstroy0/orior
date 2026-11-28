// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.h: the CLI's memory, the one pool of the platform (platform_pool.h). Every take and release goes through the
// platform's guard and carries the owner that the cell counts to (platform_places.h). The CLI opens and empties the
// pool for each of its requests, and writes the reports of the cellblock's peaks and of each owner's counts to the
// telemetry (pool.c)
#ifndef POOL_H
#define POOL_H
#include "../../../../includes/platform/owners.h"
#include "../../../../includes/platform/platform_pool.h"
#include "locus_carcerum/locus_carcerum.h"
#include "verba_scribo/verba_linea.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the pool every program is on: the max bytes its two ends occupy at once, as the cellblock's own peaks read
// them, rounded up to the next 32 MiB; the system gives the bytes as the pool is first made
#define CLI_POOL_BYTES ((256ULL << 20u) + (32ULL << 20u))

    // the pool opened at the first call and emptied for a request of `bytes`, which the pool has to cover: 1, or 0
    // where a cell is still out or the request is past it. Every program empties its pool before its first take
    int cli_pool_made(unsigned long long bytes);

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
    // the pages of the cellblock no cell holds given back to the system, the gap between the tiers and each region's
    // room above its depth, cut to whole pages and only where a span holds 64 KiB or more: the working set then reads
    // the live cells, not every page a take touched. The window's main thread calls it on its memory tick
    void cli_pool_idle_released(void);

#ifdef __cplusplus
}
#endif

#endif
