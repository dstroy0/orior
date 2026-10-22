// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host.h: orior_cli in a Flatpak's sandbox runs the machine's own programs on the machine, through a stand-in of each
// program's name put first on the PATH (host.c)
#ifndef HOST_H
#define HOST_H

#include "../command_line/cli_internal.h"

// whether orior_cli runs inside a Flatpak's sandbox
int cli_sandboxed(void);
// the stand-in for the machine's program `name`, written into `line`
void cli_stand_in(const char *name, CliLine *line);
// a stand-in written in `into` for each program of `names`, one a line, but those that keep running inside: how many,
// or -1 with why said
long long cli_stand_ins_written(const char *into, const char *names, CliLine *said);
// inside a sandbox, a stand-in made for each program on the machine's PATH and their folder put first on orior's own
// PATH, which all it starts take: how many it made, -1 with why said, or -2 outside a sandbox. Run as orior_cli starts,
// before it starts anything else
long long cli_host_prepared(CliLine *said);

#endif
