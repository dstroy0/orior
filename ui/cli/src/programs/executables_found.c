// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_found.c: the programs found put in order, and their cells given back
#include "../text/text.h"
#include "executables_internal.h"

void cli_executables_order(const CliExecutablesFoundCtx *args)
{
    g_cli_cells->persistent_buf_release(args->found->order, &g_cli_executables_cells);
    args->found->order = (args->found->count != 0u)
                             ? g_cli_cells->persistent_buf_alloc(args->found->count * sizeof(*args->found->order),
                                                                 &g_cli_executables_cells)
                             : NULL;
    if (args->found->order == NULL)
    {
        args->found->count = 0u;
        return;
    }
    // the programs by script, then by name, their places sorted and the programs left where they are
    for (unsigned int at = 0u; at < args->found->count; at += 1u)
    {
        unsigned int place = at;
        while (place > 0u)
        {
            const CliExecutable *const before = &args->found->item[args->found->order[place - 1u]];
            const CliExecutable *const one = &args->found->item[at];
            const int order = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = before->script,
                                         .other_pointer = one->script);
            if ((order < 0) || ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                            .one = before->name, .other_pointer = one->name) <= 0)))
            {
                break;
            }
            args->found->order[place] = args->found->order[place - 1u];
            place -= 1u;
        }
        args->found->order[place] = at;
    }
}

void cli_executables_free(const CliExecutablesFoundCtx *args)
{
    CliExecutables *found = args->found;
    g_cli_cells->persistent_buf_release(found->item, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(found->order, &g_cli_executables_cells);
    *found = (CliExecutables){NULL, 0u, 0u, NULL};
}
