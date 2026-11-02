// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores.h: the machine's cores orior's own work spreads over: every core the machine has less those reserved for the
// reader, never fewer than one reserved and never fewer than one to work on (cores.c). How many are reserved is the
// setting `cores-reserved`, one where nothing sets it, the reader's to raise in Preferences or in their settings file;
// a tree's file does not set it. The window gives it as it starts and as it changes, and the command line reads it from
// the reader's settings the first time it is read
#ifndef CORES_H
#define CORES_H
typedef struct CliCoresCfg CliCoresCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the setting's key, as settings.json stores it, and the fewest cores reserved for the reader
#define CLI_CORES_KEY "cores-reserved"

#define CLI_CORES_RESERVED_MIN 1u

// the cores the machine has; `reserved` cores reserved for the reader, CLI_CORES_RESERVED_MIN at the fewest; the cores
// reserved, as given or as the reader's settings set them; and the cores orior's work may take, the machine's less
// those reserved, one at the fewest
unsigned int cli_cores_machine(void);
void cli_cores_reserve(const CliCoresCfg *args);
unsigned int cli_cores_on_reserve(void);
unsigned int cli_cores_for_work(void);

struct CliCoresCfg
{
    unsigned int reserved;
};

typedef struct
{
    void (*reserve)(const CliCoresCfg *args);
} CliCoresNs;
EMBED_TABLE_LAYOUT(CliCoresNs, reserve);
EMBED_TABLE_STORAGE CliCoresNs cli_cores EMBED_UNUSED = {
    .reserve = cli_cores_reserve,
};

#endif
