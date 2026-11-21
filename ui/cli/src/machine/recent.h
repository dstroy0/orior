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

// `entry` put first in the list `list`, and taken out of its old place where the list holds it already. The oldest
// past CLI_RECENT_MAX goes, and the list goes into orior's folder. An empty entry, or one with a line end, changes
// nothing.
//
// The list's entries are in a movable cell of the recent module. A caller reads them into a buffer of its own call.
// The first call gives the bytes of the list `list`'s entries and their ends, 0 where it has none or is no list.
// The second reads at max `max` entries, newest first, into `text_out`, `capacity` bytes. It points `rows_out`,
// sized for `max`, at each entry there. It returns how many, 0 where the entries do not fit the capacity
void cli_recent_push(const CliRecentCfg *args);
unsigned long long cli_recent_bytes(const CliRecentCfg *args);
unsigned int cli_recent_read(const CliRecentCfg *args);
struct CliRecentCfg
{
    unsigned int list;
    const char *entry;
    unsigned int max;
    char *text_out;
    unsigned long long capacity;
    const char **rows_out;
};

typedef struct
{
    void (*push)(const CliRecentCfg *args);
    unsigned long long (*bytes)(const CliRecentCfg *args);
    unsigned int (*read)(const CliRecentCfg *args);
} CliRecentNs;
EMBED_TABLE_LAYOUT(CliRecentNs, push, bytes, read);
EMBED_TABLE_STORAGE CliRecentNs cli_recent EMBED_UNUSED = {
    .push = cli_recent_push,
    .bytes = cli_recent_bytes,
    .read = cli_recent_read,
};

#endif
