// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog.h: plugins others publish, listed by a catalog at an address the reader names and installed into the
// reader's plugins folder from their packages (plugin_catalog.c)
#ifndef PLUGIN_CATALOG_H
#define PLUGIN_CATALOG_H

#include "../text/json_tree.h"

// what a catalog says it is, and what a package says it is
#define CLI_PLUGIN_CATALOG "plugin catalog"
#define CLI_PLUGIN_PACKAGE "plugin"

// the plugins the catalog at `address` lists, those whose id is its own slug and that name a package, written into
// `out` as a JSON array of entries `{id, name, version, about, package, sha256, reads, writes, reaches}`: 1, or 0 with
// why said
int cli_plugin_catalog_read(const char *address, CliLine *out, CliLine *said);

// the plugin the entry of `size` bytes of JSON at `entry` names, of the catalog at `address`, installed into the
// reader's plugins folder, or into the folder `plugins`, in place of the one of its id that is there: its package
// fetched, its hash checked where the entry gives one, and its plugin.json held to the entry's id and version. The
// folder it is installed in written into `out`: 1, or 0 with why said
int cli_plugin_catalog_install(const char *address, const char *entry, unsigned long long size, CliLine *out,
                               CliLine *said);
int cli_plugin_catalog_install_into(const char *plugins, const char *address, const char *entry,
                                    unsigned long long size, CliLine *out, CliLine *said);

// `address` read from where `base` is, into `out`: as it is where it is whole, and beside `base` where it is not
void cli_plugin_catalog_beside(const char *base, const char *address, CliLine *out);

// the entry at `entry` of `json` checked as serde reads an entry, each field it gives of its kind, `where` named
// where one is not: 1, or 0 with why said; and a text field of an entry, empty where it gives none
int cli_plugin_catalog_entry_read(const CliJson *json, unsigned int entry, const char *where, CliLine *said);
const char *cli_plugin_catalog_field(const CliJson *json, unsigned int entry, const char *name);

#endif
