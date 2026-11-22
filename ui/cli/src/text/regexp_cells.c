// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_cells.c: the cells of the regular expressions in the CLI, from the CLI's own pool on the temporary tier, under
// the owner of the CLI's text (includes/regexp/regexp_base.h)
#include "../../../../includes/regexp/regexp_base.h"
#include "../command_line/pool.h"

void *regexp_cell_taken(unsigned long long bytes)
{
    // a read's or a match's cell is far below what a size_t counts
    return g_cli_guard->temporary_buf_alloc((size_t)bytes, g_cli_text_owner);
}

void regexp_cell_released(void *cell)
{
    if (cell != NULL)
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_text_owner);
    }
}
