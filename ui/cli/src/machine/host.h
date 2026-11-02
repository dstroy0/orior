// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host.h: orior_cli in a Flatpak's sandbox runs the machine's own programs on the machine, through a stand-in of each
// program's name put first on the PATH (host.c)
#ifndef HOST_H
#define HOST_H
typedef struct CliHostIntoCfg CliHostIntoCfg;
typedef struct CliHostCfg CliHostCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// whether orior_cli runs inside a Flatpak's sandbox
int cli_sandbox(void);
// the stand-in for the machine's program `name`, written into `line`
void cli_stand_in(const CliHostCfg *args);
// a stand-in written in `into` for each program of `names`, one a line, but those that stay running inside: how many,
// or -1 with why given
long long cli_stand_ins_write(const CliHostIntoCfg *args);
// inside a sandbox, a stand-in made for each program on the machine's PATH and their folder put first on orior's own
// PATH, which all it starts take: how many it made, -1 with why given, or -2 outside a sandbox. Run as orior_cli
// starts, before it starts anything else
long long cli_host_prepare(const CliHostIntoCfg *args);

struct CliHostIntoCfg
{
    const char *into;
    const char *names;
    VerbaLinea *err_msg;
};

struct CliHostCfg
{
    const char *name;
    VerbaLinea *line;
};

typedef struct
{
    void (*stand_in)(const CliHostCfg *args);
} CliHostNs;
EMBED_TABLE_LAYOUT(CliHostNs, stand_in);
EMBED_TABLE_STORAGE CliHostNs cli_host EMBED_UNUSED = {
    .stand_in = cli_stand_in,
};

typedef struct
{
    long long (*stand_ins_write)(const CliHostIntoCfg *args);
    long long (*prepare)(const CliHostIntoCfg *args);
} CliHostIntoNs;
EMBED_TABLE_LAYOUT(CliHostIntoNs, stand_ins_write, prepare);
EMBED_TABLE_STORAGE CliHostIntoNs cli_host_into EMBED_UNUSED = {
    .stand_ins_write = cli_stand_ins_write,
    .prepare = cli_host_prepare,
};

#endif
