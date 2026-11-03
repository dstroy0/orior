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
#include "embed_dispatch_layout.h"

typedef struct CliNetAddressCfg CliNetAddressCfg;
typedef struct CliNetAddressHostCfg CliNetAddressHostCfg;
typedef struct CliNetMaxCfg CliNetMaxCfg;
typedef struct CliNetCfg CliNetCfg;
typedef struct CliNetHostCfg CliNetHostCfg;

// orior_cli blocked from the network, or let on, as the window's setting specifies; and whether it is blocked: as the
// window set it, or as settings.json specifies where it has not
void cli_net_disable(const CliNetCfg *args);
int cli_net_is_disable(void);
// the host of `address` into `host`, sized for CLI_PATH_MAX_LEN: a URL's, an ssh address's after its user, or the
// address itself, without a port
void cli_net_host_get(const CliNetAddressHostCfg *args);
// whether `host` is this machine's own
int cli_net_local(const CliNetHostCfg *args);
// whether `address` is a folder or a file of this machine's, as a repository's remote may be
int cli_net_path(const CliNetAddressCfg *args);
// `address` requested to be reached for `purpose`: 1 where it may be, logged where orior_cli is let on; 0 with why
// given where orior_cli is blocked. This machine's own addresses and paths are let on and not logged
int cli_net_reach(const CliNetAddressCfg *args);
struct CliNetAddressCfg
{
    const char *address;
    const char *purpose;
    VerbaLinea *err_msg;
};

struct CliNetAddressHostCfg
{
    const char *address;
    char *host;
};

struct CliNetMaxCfg
{
    unsigned int most;
    VerbaLinea *out;
};

struct CliNetCfg
{
    int off;
};

struct CliNetHostCfg
{
    const char *host_read;
};

typedef struct
{
    void (*disable)(const CliNetCfg *args);
} CliNetNs;
EMBED_TABLE_LAYOUT(CliNetNs, disable);
EMBED_TABLE_STORAGE CliNetNs cli_net EMBED_UNUSED = {
    .disable = cli_net_disable,
};

typedef struct
{
    void (*host_get)(const CliNetAddressHostCfg *args);
} CliNetAddressHostNs;
EMBED_TABLE_LAYOUT(CliNetAddressHostNs, host_get);
EMBED_TABLE_STORAGE CliNetAddressHostNs cli_net_address_host EMBED_UNUSED = {
    .host_get = cli_net_host_get,
};

typedef struct
{
    int (*local)(const CliNetHostCfg *args);
} CliNetHostNs;
EMBED_TABLE_LAYOUT(CliNetHostNs, local);
EMBED_TABLE_STORAGE CliNetHostNs cli_net_host EMBED_UNUSED = {
    .local = cli_net_local,
};

typedef struct
{
    int (*path)(const CliNetAddressCfg *args);
    int (*reach)(const CliNetAddressCfg *args);
} CliNetAddressNs;
EMBED_TABLE_LAYOUT(CliNetAddressNs, path, reach);
EMBED_TABLE_STORAGE CliNetAddressNs cli_net_address EMBED_UNUSED = {
    .path = cli_net_path,
    .reach = cli_net_reach,
};

#endif
