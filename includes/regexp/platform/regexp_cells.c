// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_cells.c: the cells of the regular expressions in a program of the platform, from the platform's pool on the
// temporary tier, under the program's owner (regexp_base.h). The CLI defines its own, and its build leaves this out
#include "../../../src/platform/platform.h"
#include "../regexp_base.h"

void *regexp_cell_taken(unsigned long long bytes)
{
    return platform_cell_taken(bytes, platform_place_g_program_owner, 1);
}

void regexp_cell_released(void *cell)
{
    platform_cell_released(cell, platform_place_g_program_owner);
}
