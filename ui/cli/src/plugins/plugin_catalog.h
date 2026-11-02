// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog.h: plugins others publish, listed by a catalog at an address the reader names and installed into the
// reader's plugins folder from their packages (plugin_catalog.c)
#ifndef PLUGIN_CATALOG_H
#define PLUGIN_CATALOG_H
typedef struct CliPluginCatalogAddressCfg CliPluginCatalogAddressCfg;
typedef struct CliPluginCatalogErrMsgCfg CliPluginCatalogErrMsgCfg;
typedef struct CliPluginCatalogCfg CliPluginCatalogCfg;
typedef struct CliPluginCatalogJsonCfg CliPluginCatalogJsonCfg;

#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

// what a catalog declares it is, and what a package declares it is
#define CLI_PLUGIN_CATALOG "plugin catalog"
#define CLI_PLUGIN_PACKAGE "plugin"

// the plugins the catalog at `address` lists, those whose id is its own slug and that name a package, written into
// `out` as a JSON array of entries `{id, name, version, about, package, sha256, reads, writes, reaches}`: 1, or 0 with
// why given
int cli_plugin_catalog_read(const CliPluginCatalogAddressCfg *args);

// the plugin the entry of `size` bytes of JSON at `entry` names, of the catalog at `address`, installed into the
// reader's plugins folder, or into the folder `plugins`, in place of the one of its id that is there: its package
// fetched, its hash checked where the entry gives one, and its plugin.json checked against the entry's id and version.
// The folder it is installed in written into `out`: 1, or 0 with why given
int cli_plugin_catalog_install(const CliPluginCatalogAddressCfg *args);
int cli_plugin_catalog_install_into(const CliPluginCatalogAddressCfg *args);

// `address` read from where `base` is, into `out`: as it is where it is whole, and beside `base` where it is not
void cli_plugin_catalog_beside(const CliPluginCatalogCfg *args);

// the entry at `entry` of `json` checked as serde reads an entry, each field it gives of its kind, `where` named
// where one is not: 1, or 0 with why given; and a text field of an entry, empty where it gives none
int cli_plugin_catalog_entry_read(const CliPluginCatalogErrMsgCfg *args);
const char *cli_plugin_catalog_field(const CliPluginCatalogJsonCfg *args);

struct CliPluginCatalogAddressCfg
{
    const char *address;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *entry;
    unsigned long long size;
    const char *plugins;
};

struct CliPluginCatalogErrMsgCfg
{
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int entry_u32;
    const char *where;
};

struct CliPluginCatalogCfg
{
    const char *address;
    VerbaLinea *out;
    const char *base;
};

struct CliPluginCatalogJsonCfg
{
    const CliJson *json;
    unsigned int entry_u32;
    const char *name;
};

typedef struct
{
    int (*read)(const CliPluginCatalogAddressCfg *args);
    int (*install)(const CliPluginCatalogAddressCfg *args);
    int (*install_into)(const CliPluginCatalogAddressCfg *args);
} CliPluginCatalogAddressNs;
EMBED_TABLE_LAYOUT(CliPluginCatalogAddressNs, read, install, install_into);
EMBED_TABLE_STORAGE CliPluginCatalogAddressNs cli_plugin_catalog_address EMBED_UNUSED = {
    .read = cli_plugin_catalog_read,
    .install = cli_plugin_catalog_install,
    .install_into = cli_plugin_catalog_install_into,
};

typedef struct
{
    void (*beside)(const CliPluginCatalogCfg *args);
} CliPluginCatalogNs;
EMBED_TABLE_LAYOUT(CliPluginCatalogNs, beside);
EMBED_TABLE_STORAGE CliPluginCatalogNs cli_plugin_catalog EMBED_UNUSED = {
    .beside = cli_plugin_catalog_beside,
};

typedef struct
{
    int (*entry_read)(const CliPluginCatalogErrMsgCfg *args);
} CliPluginCatalogErrMsgNs;
EMBED_TABLE_LAYOUT(CliPluginCatalogErrMsgNs, entry_read);
EMBED_TABLE_STORAGE CliPluginCatalogErrMsgNs cli_plugin_catalog_err_msg EMBED_UNUSED = {
    .entry_read = cli_plugin_catalog_entry_read,
};

typedef struct
{
    const char *(*field)(const CliPluginCatalogJsonCfg *args);
} CliPluginCatalogJsonNs;
EMBED_TABLE_LAYOUT(CliPluginCatalogJsonNs, field);
EMBED_TABLE_STORAGE CliPluginCatalogJsonNs cli_plugin_catalog_json EMBED_UNUSED = {
    .field = cli_plugin_catalog_field,
};

#endif
