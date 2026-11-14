// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recent.h: the files opened last and the commands run last, newest first, CLI_RECENT_MAX of each, which the quick
// open ranks first. Each list is in one cell sized to its entries, its rows offsets into it, read from orior's
// folder the first time either entry is asked and stored there under `recent.files` and `recent.commands`, a line an
// entry, as each push changes it (recent.c)
#ifndef RECENT_H
#define RECENT_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliRecentCfg CliRecentCfg;

// the lists: the files opened, and the commands run; and the max entries a list carries
#define CLI_RECENT_FILES 0u
#define CLI_RECENT_COMMANDS 1u
#define CLI_RECENT_LISTS 2u
#define CLI_RECENT_MAX 20u

// `entry` put first in the list `list`, out of the place it was where it was in it already, the oldest past
// CLI_RECENT_MAX let go, and the list stored; an entry empty or with a line end in it changes nothing. And the list
// `list`, at max `max` of its entries, newest first, into `rows_out`, sized for `max`: how many, each located in
// the temporary cell of `owner` written into `cell_out`, which the caller gives back with
// g_cli_guard->persistent_buf_release
void cli_recent_push(const CliRecentCfg *args);
unsigned int cli_recent_list(const CliRecentCfg *args);
struct CliRecentCfg
{
    unsigned int list;
    const char *entry;
    unsigned int max;
    const char **rows_out;
    char **cell_out;
    const CellOwner *owner;
};

typedef struct
{
    void (*push)(const CliRecentCfg *args);
    unsigned int (*list)(const CliRecentCfg *args);
} CliRecentNs;
EMBED_TABLE_LAYOUT(CliRecentNs, push, list);
EMBED_TABLE_STORAGE CliRecentNs cli_recent EMBED_UNUSED = {
    .push = cli_recent_push,
    .list = cli_recent_list,
};

#endif
