// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells.h: the cells every module holds its work in: one cellblock, taken from under its lock and given back to as the
// work ends, each cell starting on a CLI_CELL_ALIGN boundary; a take the cellblock cannot meet is refused and said,
// with what it was for; each module's own mark of the most it held at once, raised as it takes; and the reports of the
// cellblock and of each module to the telemetry (cells.c)
#ifndef CELLS_H
#define CELLS_H

#include "file_writing.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the boundary every cell starts on, a vector register's width; the most bytes a cell of `bytes_` takes from the
// cellblock, its header, the size and the used flag, the two boundaries' width its start and its size are taken over
// by, and the cellblock's rounding up to a word; and the cellblock's pool, the most bytes its two ends hold at once, as
// the cellblock's own peaks give them, taken up to the next 32 MiB
#define CLI_CELL_ALIGN 32u
#define CLI_CELL_COST(bytes_) ((2u * sizeof(size_t)) + (bytes_) + (2u * CLI_CELL_ALIGN) + (sizeof(size_t) - 1u))
#define CLI_CELLS_POOL ((256ull << 20u) + (32ull << 20u))

    // A module's own count of the cells it holds: its name as its report is told, the bytes it holds now, its mark of
    // the most it held at once, and that mark as last told. Read and raised under the cellblock's lock
    typedef struct
    {
        const char *name;
        unsigned long long held;
        unsigned long long hw;
        unsigned long long told;
    } CliCellsTally;

// a module's count, holding nothing yet, its report told as `name_`
#define CLI_CELLS_TALLY(name_) {(name_), 0ull, 0ull, 0ull}

    // a cell of `bytes` for `what`, from the persistent end: its first byte, on a CLI_CELL_ALIGN boundary, or NULL,
    // said, where the cellblock cannot meet it; and a cell given back, NULL given back as nothing
    void *cli_cell_taken(unsigned long long bytes, const char *what);
    void cli_cell_given(void *cell);

    // the same for a module's count: the cell's bytes added to what the module holds, and its mark raised to that; and
    // taken off again as the cell is given back. The module's report told as its mark rises a step
    void *cli_cell_taken_by(CliCellsTally *tally, unsigned long long bytes, const char *what);
    void cli_cell_given_by(CliCellsTally *tally, void *cell);

    // the cellblock's own report, `<pool's bytes> <persistent peak> <temporary peak>`, read off its state, written
    // into `out`, each peak 0 where the build keeps none (MMGR_ENABLE_HW_MEM_CAPACITY_CB); and a module's,
    // `<pool's bytes> <its mark> 0`, read off its count
    void cli_cells_said(CliLine *out);
    void cli_cells_tally_said(const CliCellsTally *tally, CliLine *out);

    // a report appended as the record `cells/<name>` to the run this process is a member of, and nothing where it is a
    // member of none
    void cli_cells_told(const char *name, const CliLine *report);

#ifdef __cplusplus
}
#endif

#endif
