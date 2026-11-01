// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_found.c: the programs found put in order, and their cells given back
#include "../text/text.h"
#include "executables_internal.h"

void cli_executables_ordered(CliExecutables *found)
{
    g_cli_cells->persistent_buf_release(found->order, &g_cli_executables_cells);
    found->order = (found->count != 0u) ? g_cli_cells->persistent_buf_alloc(found->count * sizeof(*found->order),
                                                                            &g_cli_executables_cells)
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
    g_cli_cells->persistent_buf_release(found->item, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(found->order, &g_cli_executables_cells);
    *found = (CliExecutables){NULL, 0u, 0u, NULL};
}
