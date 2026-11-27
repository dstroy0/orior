// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_regexp_cells.c: the cells of the regular expressions in the table's tools, from the platform's pool on the
// temporary tier, under the owner of the program (includes/regexp/regexp_base.h)
#include "../../../../../../includes/regexp/regexp_base.h"
#include "../../../../../../includes/platform/platform.h"

void *regexp_cell_taken(unsigned long long bytes)
{
    return platform_cell_taken(bytes, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

void regexp_cell_released(void *cell)
{
    platform_cell_released(cell, platform_place_g_program_owner);
}
