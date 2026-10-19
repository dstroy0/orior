// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_internal.h: what the catalog's sources share: the room a catalog or a package is read into, the tree
// it is read into, and their lock (plugin_catalog.c, plugin_catalog_install.c)
#ifndef PLUGIN_CATALOG_INTERNAL_H
#define PLUGIN_CATALOG_INTERNAL_H

#include "../command_line/cells.h"
#include "plugin_catalog.h"

// the most bytes a catalog or a package read takes, and the nodes and texts of the tree they are read into
#define CLI_CATALOG_BYTES (32u << 20u)
#define CLI_CATALOG_NODES (1u << 18u)

// A catalog's room, a cell taken with the lock and given back with it: the bytes read, and the tree they are read
// into, its nodes, tokens and texts
typedef struct
{
    unsigned char bytes[CLI_CATALOG_BYTES];
    CliJsonNode nodes[CLI_CATALOG_NODES];
    CfgJsonToken tokens[CLI_CATALOG_NODES];
    char text[CLI_CATALOG_BYTES];
    CliJson json;
} CliCatalogRoom;

extern CliCatalogRoom *g_cli_catalog;

// the catalog's lock taken and its room a cell under it: 1, or 0 with why said and the lock given back; and the room
// given back with the lock (plugin_catalog.c)
int cli_plugin_catalog_held(CliLine *said);
void cli_plugin_catalog_released(void);

// the bytes at `address`, a web address or a file of this machine, asked for `purpose`, read into
// the room's bytes, their count into `size`, the lock held: 1, or 0 with why said; and that a value is not the kind
// wanted said after `where`, as serde says it (plugin_catalog.c)
int cli_plugin_catalog_fetch(const char *address, const char *purpose, unsigned long long *size, CliLine *said);
void cli_plugin_catalog_invalid(const char *where, const CliJson *json, unsigned int node, const char *wanted,
                                CliLine *said);

#endif
