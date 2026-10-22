// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_grants.c: what a grant names: whether it grants a channel, and whether one grant lies within another
#include "comms_internal.h"

#include "../text/text.h"

int cli_comms_grants(const char *grant, const char *channel)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = grant, .other = "*", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = grant, .other = channel, .cap = CLI_CELLS_POOL})))
    {
        return 1;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = grant, .cap = CLI_CELLS_POOL});
    // a grant ending in /* names every channel under the name before it
    if ((length < 2u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = grant + length - 2u, .other = "/*", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const size_t under = length - 2u;
    return cli_text_same(channel, grant, under) && (channel[under] == '/');
}

int cli_comms_within(const char *inner, const char *outer)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = outer, .other = "*", .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = inner, .other = "*", .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    char inner_under[CLI_NAME_LONGEST];
    char outer_under[CLI_NAME_LONGEST];
    const size_t inner_length = cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_CELLS_POOL});
    const size_t outer_length = cellul.len(&(CatenaFinitaCfg){.src = outer, .cap = CLI_CELLS_POOL});
    const int inner_all =
        (inner_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = inner + inner_length - 2u, .other = "/*", .cap = CLI_CELLS_POOL}));
    const int outer_all =
        (outer_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = outer + outer_length - 2u, .other = "/*", .cap = CLI_CELLS_POOL}));
    if (inner_all && !outer_all)
    {
        return 0;
    }
    if (!inner_all)
    {
        return cli_comms_grants(outer, inner);
    }
    CliLine inner_line = {inner_under, sizeof(inner_under), 0ull};
    cli_line_counted(&inner_line, inner, inner_length - 2u);
    cli_line_closed(&inner_line);
    CliLine outer_line = {outer_under, sizeof(outer_under), 0ull};
    cli_line_counted(&outer_line, outer, outer_length - 2u);
    cli_line_closed(&outer_line);
    return (cellul.eq(&(CatenaFinitaCfg){.src = inner_under, .other = outer_under, .cap = CLI_CELLS_POOL})) ||
           cli_comms_grants(outer, inner_under);
}
