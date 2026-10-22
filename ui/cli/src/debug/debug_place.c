// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_place.c: the cells a session's place holds: its lists of breakpoints grown as they come, and they and the tree
// it starts with given back as the place is freed
#include "debug_internal.h"

CliCellsTally g_cli_debug_cells = CLI_CELLS_TALLY("cli_debug");

void *cli_debug_grown(void *list, unsigned int count, unsigned int *room, unsigned long long size, unsigned int most)
{
    if ((count < *room) || (*room >= most))
    {
        return list;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_DEBUG_LIST_FIRST : (*room * 2u);
    const unsigned int grown = (doubled < most) ? doubled : most;
    void *const cell = cli_cell_taken_by(&g_cli_debug_cells, grown * size, "the breakpoints a session keeps");
    if (cell == NULL)
    {
        return list;
    }
    if (count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = list, .bytes = count * size});
    }
    cli_cell_given_by(&g_cli_debug_cells, list);
    *room = grown;
    return cell;
}

void cli_debug_place_freed(CliDebugSession *place)
{
    cli_cell_given_by(&g_cli_debug_cells, place->data);
    cli_cell_given_by(&g_cli_debug_cells, place->placed);
    place->data = NULL;
    place->data_count = 0u;
    place->data_room = 0u;
    place->placed = NULL;
    place->placed_count = 0u;
    place->placed_room = 0u;
    cli_json_given(&place->start);
    place->taken = 0;
}
