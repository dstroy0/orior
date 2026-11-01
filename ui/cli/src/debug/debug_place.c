// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_place.c: the cells a session's place occupies: its lists of breakpoints in one cell taken with the place, and
// they and the tree it starts with given back as the place is freed
#include "debug_internal.h"

void cli_debug_place_freed(CliDebugSession *place)
{
    g_cli_cells->persistent_buf_release(place->placed, &g_cli_debug_cells);
    place->data = NULL;
    place->data_count = 0u;
    place->data_room = 0u;
    place->placed = NULL;
    place->placed_count = 0u;
    place->placed_room = 0u;
    cli_json_given(&place->start);
    place->taken = 0;
}
