// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_internal.h: what the catalog's sources share: the room a catalog or a package is read into, the tree
// it is read into, and their lock (plugin_catalog.c, plugin_catalog_install.c)
#ifndef PLUGIN_CATALOG_INTERNAL_H
#define PLUGIN_CATALOG_INTERNAL_H

#include "../command_line/cells.h"
#include "plugin_catalog.h"

// the most bytes a catalog or a package read takes
#define CLI_CATALOG_BYTES (32u << 20u)

// A catalog's room, held with the lock and emptied with it: the bytes read, in a cell of their size, and the tree they
// are read into, taken to their tokens
typedef struct
{
    unsigned char *bytes;
    CliJson json;
} CliCatalogRoom;

extern CliCatalogRoom *g_cli_catalog;

// the catalog's lock taken and its room held empty under it: 1; and the room's cells given back with the lock
// (plugin_catalog.c)
int cli_plugin_catalog_held(CliLine *said);
void cli_plugin_catalog_released(void);

// the bytes at `address`, a web address or a file of this machine, asked for `purpose`, read into a cell of their size
// as the room's bytes, the bytes it held given back first, their count into `size`, the lock held: 1, or 0 with why
// said; `size` bytes at `text` read into the room's tree, the tree it held given back first: its root, or
// CLI_JSON_NONE with why said; and that a value is not the kind wanted said after `where`, as serde says it
// (plugin_catalog.c)
int cli_plugin_catalog_fetch(const char *address, const char *purpose, unsigned long long *size, CliLine *said);
unsigned int cli_plugin_catalog_tree(const char *text, unsigned long long size, CliLine *said);
void cli_plugin_catalog_invalid(const char *where, const CliJson *json, unsigned int node, const char *wanted,
                                CliLine *said);

#endif
