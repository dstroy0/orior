// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net.h: each address orior_cli reaches over the network, requested here first (net.c). orior_cli reaches one only for
// what the reader requested of it: a request sent, a database connected, a repository pulled or pushed, a machine
// linked or deployed to, settings carried, and an error report the reader let file on its own. The setting `network`,
// `off`, blocks orior from the network entirely: each reach is refused, stating what it was for. Each address reached
// is logged in `network.jsonl` in orior's own folder with when and what for, the last 1000 of them, and Network lists
// them. This machine's own addresses are no part of the network and are neither blocked nor logged
#ifndef NET_H
#define NET_H

#include "../command_line/cli_internal.h"

// orior_cli blocked from the network, or let on, as the window's setting specifies; and whether it is blocked: as the
// window set it, or as settings.json specifies where it has not
void cli_net_disable(int off);
int cli_net_is_disabled(void);
// the host of `address` into `host`, sized for CLI_PATH_MAX_LEN: a URL's, an ssh address's after its user, or the
// address itself, without a port
void cli_net_host_of(const char *address, char *host);
// whether `host` is this machine's own
int cli_net_local(const char *host);
// whether `address` is a folder or a file of this machine's, as a repository's remote may be
int cli_net_path(const char *address);
// `address` requested to be reached for `purpose`: 1 where it may be, logged where orior_cli is let on; 0 with why
// given where orior_cli is blocked. This machine's own addresses and paths are let on and not logged
int cli_net_reach(const char *address, const char *purpose, VerbaLinea *err_msg);
// the addresses reached, the last first, `most` of them at most, written as `[{"at", "address", "for"}...]`
void cli_net_reached(unsigned int most, VerbaLinea *out);

#endif
