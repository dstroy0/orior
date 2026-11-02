// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_internal.h: what the catalog's sources share: the work state a catalog or a package is read into, the
// tree it is read into, and their lock (plugin_catalog.c, plugin_catalog_install.c)
#ifndef PLUGIN_CATALOG_INTERNAL_H
#define PLUGIN_CATALOG_INTERNAL_H
typedef struct CliPluginCatalogWhereCtx CliPluginCatalogWhereCtx;
typedef struct CliPluginCatalogAddressCtx CliPluginCatalogAddressCtx;
typedef struct CliPluginCatalogCtx CliPluginCatalogCtx;

#include "../command_line/cells.h"
#include "embed_dispatch_layout.h"
#include "plugin_catalog.h"

// the most bytes a catalog or a package read takes
#define CLI_CATALOG_BYTES (32u << 20u)

// A catalog's work state, taken with the lock and emptied with it: the bytes read, in a cell of their size, and the
// tree they are read into, taken to their tokens
typedef struct
{
    unsigned char *bytes;
    CliJson json;
} CliCatalogWorkState;

extern CliCatalogWorkState *g_cli_catalog;

// the catalog's lock taken and its work state left empty under it: 1; and the work state's cells given back with the
// lock (plugin_catalog.c)
int cli_plugin_catalog_lock(const CliPluginCatalogCtx *args);
void cli_plugin_catalog_release(void);

// the bytes at `address`, a web address or a file of this machine, requested for `purpose`, read into a cell of their
// size as the work state's bytes, the bytes it contained given back first, their count into `size`, the lock acquired:
// 1, or 0 with why given; `size` bytes at `text` read into the work state's tree, the tree it contained given back
// first: its root, or CLI_JSON_NONE with why given; and that a value is not the kind wanted reported after `where`, as
// serde reports it (plugin_catalog.c)
unsigned int cli_plugin_catalog_tree(const CliPluginCatalogCtx *args);
void cli_plugin_catalog_invalid(const CliPluginCatalogWhereCtx *args);

struct CliPluginCatalogWhereCtx
{
    VerbaLinea *err_msg;
    const char *where;
    const CliJson *json;
    unsigned int node;
    const char *required;
};

struct CliPluginCatalogAddressCtx
{
    VerbaLinea *err_msg;
    const char *address;
    const char *purpose;
    unsigned long long *size;
};

struct CliPluginCatalogCtx
{
    VerbaLinea *err_msg;
    const char *text;
    unsigned long long size_value;
};

typedef struct
{
    int (*lock)(const CliPluginCatalogCtx *args);
    unsigned int (*tree)(const CliPluginCatalogCtx *args);
} CliPluginCatalogNsTable;
EMBED_TABLE_LAYOUT(CliPluginCatalogNsTable, lock, tree);
EMBED_TABLE_STORAGE CliPluginCatalogNsTable cli_plugin_catalog_internal EMBED_UNUSED = {
    .lock = cli_plugin_catalog_lock,

    .tree = cli_plugin_catalog_tree,
};

typedef struct
{
    void (*invalid)(const CliPluginCatalogWhereCtx *args);
} CliPluginCatalogWhereNs;
EMBED_TABLE_LAYOUT(CliPluginCatalogWhereNs, invalid);
EMBED_TABLE_STORAGE CliPluginCatalogWhereNs cli_plugin_catalog_where_internal EMBED_UNUSED = {
    .invalid = cli_plugin_catalog_invalid,
};

#endif
