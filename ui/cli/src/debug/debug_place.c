// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_position.c: the cells a session's place occupies: its lists of breakpoints in one cell taken with the place,
// and they and the tree it starts with given back as the place is freed
#include "debug_internal.h"

void cli_debug_position_free(const CliDebugCtx *args)
{
    g_cli_cells->persistent_buf_release(args->place->positioned, &g_cli_debug_cells);
    args->place->data = NULL;
    args->place->data_count = 0u;
    args->place->data_capacity = 0u;
    args->place->positioned = NULL;
    args->place->positioned_count = 0u;
    args->place->positioned_capacity = 0u;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->place->start);
    args->place->reserved = 0;
}
