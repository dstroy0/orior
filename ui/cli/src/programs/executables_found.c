// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_found.c: the programs found, a list grown in a cell as they are put, put in order, and given back
#include "../text/text.h"
#include "executables_internal.h"

// the programs the list is begun with, doubling as they are found
#define CLI_EXECUTABLES_FIRST 16u

int cli_executables_room(CliExecutables *found)
{
    if (found->count < found->room)
    {
        return 1;
    }
    if (found->room >= CLI_EXECUTABLES_MOST)
    {
        return 0;
    }
    const unsigned int doubled = (found->room == 0u) ? CLI_EXECUTABLES_FIRST : (found->room * 2u);
    const unsigned int room = (doubled < CLI_EXECUTABLES_MOST) ? doubled : CLI_EXECUTABLES_MOST;
    CliExecutable *const grown =
        cli_cell_taken_by(&g_cli_executables_cells, room * sizeof(*grown), "the programs the build scripts make");
    if (grown == NULL)
    {
        return 0;
    }
    if (found->count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = found->item, .bytes = found->count * sizeof(*grown)});
    }
    cli_cell_given_by(&g_cli_executables_cells, found->item);
    found->item = grown;
    found->room = room;
    return 1;
}

void cli_executables_ordered(CliExecutables *found)
{
    cli_cell_given_by(&g_cli_executables_cells, found->order);
    found->order = (found->count != 0u)
                       ? cli_cell_taken_by(&g_cli_executables_cells, found->count * sizeof(*found->order),
                                           "the order of the programs found")
                       : NULL;
    if (found->order == NULL)
    {
        found->count = 0u;
        return;
    }
    // the programs by script, then by name, their places sorted and the programs left where they are
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        unsigned int place = at;
        while (place > 0u)
        {
            const CliExecutable *const before = &found->item[found->order[place - 1u]];
            const CliExecutable *const one = &found->item[at];
            const int order = cli_text_order(before->script, one->script);
            if ((order < 0) || ((order == 0) && (cli_text_order(before->name, one->name) <= 0)))
            {
                break;
            }
            found->order[place] = found->order[place - 1u];
            place -= 1u;
        }
        found->order[place] = at;
    }
}

void cli_executables_given(CliExecutables *found)
{
    cli_cell_given_by(&g_cli_executables_cells, found->item);
    cli_cell_given_by(&g_cli_executables_cells, found->order);
    *found = (CliExecutables){NULL, 0u, 0u, NULL};
}
