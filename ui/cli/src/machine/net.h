// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net.h: each address orior_cli reaches over the network, asked for here first (net.c). orior_cli reaches one only for what the
// reader asked of it: a request sent, a database connected, a repository pulled or pushed, a machine linked or deployed
// to, settings carried, and an error report the reader let file on its own. The setting `network`, `off`, holds orior
// off the network entirely: each reach is refused, saying what it was for. Each address reached is kept in
// `network.jsonl` in orior's own folder with when and what for, the last 1000 of them, and Network lists them. This
// machine's own addresses are no part of the network and are neither held nor kept
#ifndef NET_H
#define NET_H

#include "../command_line/cli_internal.h"

// orior_cli held off the network, or let on, as the window's setting says; and whether it is held off: as the window said,
// or as settings.json says where it has not
void cli_net_hold(int off);
int cli_net_held(void);
// the host of `address` into `host`, which holds CLI_PATH_LONGEST: a URL's, an ssh address's after its user, or the
// address itself, without a port
void cli_net_host_of(const char *address, char *host);
// whether `host` is this machine's own
int cli_net_local(const char *host);
// whether `address` is a folder or a file of this machine's, as a repository's remote may be
int cli_net_path(const char *address);
// `address` asked to be reached for `purpose`: 1 where it may be, kept where orior_cli is let on; 0 with why said where
// orior_cli is held off. This machine's own addresses and paths are let on and not kept
int cli_net_reach(const char *address, const char *purpose, CliLine *said);
// the addresses reached, the last first, `most` of them at most, written as `[{"at", "address", "for"}...]`
void cli_net_reached(unsigned int most, CliLine *out);

#endif
