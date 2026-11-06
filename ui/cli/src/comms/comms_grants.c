// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_grants.c: what a grant names: whether it grants a channel, and whether one grant lies within another
#include "comms_internal.h"

#include "../text/text.h"

int cli_comms_grants(const CliCommsGrantCfg *args)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->grant, .other = "*", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->grant, .other = args->channel, .cap = CLI_POOL_BYTES})))
    {
        return 1;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->grant, .cap = CLI_POOL_BYTES});
    // a grant ending in a slash and a star names every channel under the name before it
    if ((length < 2u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->grant + length - 2u, .other = "/*", .cap = CLI_POOL_BYTES})))
    {
        return 0;
    }
    const size_t under = length - 2u;
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->channel, .other_pointer = args->grant,
                      .size = under) &&
           (args->channel[under] == '/');
}

int cli_comms_within(const CliCommsInnerCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->outer, .other = "*", .cap = CLI_POOL_BYTES}))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->inner, .other = "*", .cap = CLI_POOL_BYTES}))
    {
        return 0;
    }
    char inner_under[CLI_NAME_MAX_LEN];
    char outer_under[CLI_NAME_MAX_LEN];
    const size_t inner_length = cellul.len(&(CatenaFinitaCfg){.src = args->inner, .cap = CLI_POOL_BYTES});
    const size_t outer_length = cellul.len(&(CatenaFinitaCfg){.src = args->outer, .cap = CLI_POOL_BYTES});
    const int inner_all =
        (inner_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->inner + inner_length - 2u, .other = "/*", .cap = CLI_POOL_BYTES}));
    const int outer_all =
        (outer_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->outer + outer_length - 2u, .other = "/*", .cap = CLI_POOL_BYTES}));
    if (inner_all && !outer_all)
    {
        return 0;
    }
    if (!inner_all)
    {
        return EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = args->outer, .channel = args->inner);
    }
    VerbaLinea inner_line = {inner_under, sizeof(inner_under), 0ULL};
    mmgr_line_put_n(&inner_line, args->inner, inner_length - 2u);
    mmgr_line_finish(&inner_line);
    VerbaLinea outer_line = {outer_under, sizeof(outer_under), 0ULL};
    mmgr_line_put_n(&outer_line, args->outer, outer_length - 2u);
    mmgr_line_finish(&outer_line);
    return (cellul.eq(&(CatenaFinitaCfg){.src = inner_under, .other = outer_under, .cap = CLI_POOL_BYTES})) ||
           EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = args->outer, .channel = inner_under);
}
