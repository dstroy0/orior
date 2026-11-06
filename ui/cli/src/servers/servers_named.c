// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_named.c: the checkers the reader names, by name or id: stored by their ids, a checker no longer named taking
// its findings with it, each file the editor has open checked again with them, and the tree where its check is on; the
// names no checker answers to given back
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../toolchains/checkers.h"
#include "servers_internal.h"

typedef struct
{
    CliKeyed *keyed;
    unsigned long long *size;
    const char *id;
    const char *key;
    const char *uri;
    const CliServersSink *sink;
} ServersNamedCtx;
