// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_place.c: the cells a session's place occupies: its lists of breakpoints in one cell taken with the place, and
// they and the tree it starts with given back as the place is freed
#include "debug_internal.h"

void cli_debug_place_free(const CliDebugCtx *args)
{
    g_cli_cells->persistent_buf_release(args->place->placed, &g_cli_debug_cells);
    args->place->data = NULL;
    args->place->data_count = 0u;
    args->place->data_room = 0u;
    args->place->placed = NULL;
    args->place->placed_count = 0u;
    args->place->placed_room = 0u;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->place->start);
    args->place->taken = 0;
}
