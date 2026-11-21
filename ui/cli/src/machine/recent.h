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
// CLI_RECENT_MAX let go, and the list stored; an entry empty or with a line end in it changes nothing.
//
// The list `list`, at max `max` of its entries, newest first, goes to `each` with `context` as one set of rows. The
// rows are in a temporary cell of the recent module. They stay valid for the call to `each` alone, and the module
// gives the cell back before it returns. A list with no entries goes to `each` as 0 rows, and a list past
// CLI_RECENT_LISTS does not go to `each`. Returns the row count
void cli_recent_push(const CliRecentCfg *args);
unsigned int cli_recent_each(const CliRecentCfg *args);
struct CliRecentCfg
{
    unsigned int list;
    const char *entry;
    unsigned int max;
    void (*each)(const CliEachRowsCountCfg *args);
    void *context;
};

typedef struct
{
    void (*push)(const CliRecentCfg *args);
    unsigned int (*each)(const CliRecentCfg *args);
} CliRecentNs;
EMBED_TABLE_LAYOUT(CliRecentNs, push, each);
EMBED_TABLE_STORAGE CliRecentNs cli_recent EMBED_UNUSED = {
    .push = cli_recent_push,
    .each = cli_recent_each,
};

#endif
