// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_view.c: the comms layer as the view lists it: every part with its grants and counts, and every channel that has
// carried or refused a message with its counts, its listeners and its last messages, each list by name
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "comms_internal.h"

#include "../text/text.h"

typedef struct
{
    const char (*names)[CLI_NAME_MAX_LEN];
    unsigned int count;
    VerbaLinea *out;
    unsigned int *order;
    const char *(*name_of)(const CliEachPositionCfg *args);
    const char *name;
} CommsViewCtx;

static const char *view_part_name(const CliEachPositionCfg *args)
{
    return g_cli_comms.parts[args->place].name;
}

static const char *view_channel_name(const CliEachPositionCfg *args)
{
    return g_cli_comms.traffic[args->place].name;
}
