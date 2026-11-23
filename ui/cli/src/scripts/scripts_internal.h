// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts_internal.h: what the scripts sources share. A file of the scripts module goes in a folder of orior's own
// folder: the host in plugin_host, and the state and the answers of each plugin in plugin_state (scripts.c)
#ifndef SCRIPTS_INTERNAL_H
#define SCRIPTS_INTERNAL_H
#include "scripts.h"

typedef struct
{
    const char *leaf;
    const char *name;
    char *path_out;
    VerbaLinea *err_msg;
} CliScriptsPathCtx;

typedef struct CliLsp CliLsp;

typedef struct
{
    const char *id;
    CliLsp *lsp;
    const char *params;
    unsigned long long size;
    VerbaLinea *answer;
} CliScriptsCallCtx;

// the path of `name` in the folder `leaf` of orior's own folder into `path_out`, sized for CLI_PATH_MAX_LEN. The call
// makes the folder where it is not there: 1, or 0 with why given
int cli_scripts_path(const CliScriptsPathCtx *args);
// the call `orior/call` of the host of the plugin `id`, `size` bytes of JSON `params` {name, args}, on the reading
// thread of the server `lsp`. The call's result goes into `answer`, or `{"oriorError": why}`. A call that waits on the
// reader's answer writes nothing, and its answer goes to `lsp` later (scripts_calls.c)
void cli_scripts_call(const CliScriptsCallCtx *args);
// The event `plugin-server` of a plugin's language server, `size` bytes of JSON {plugin, method, params, request}.
// A worker thread takes it to the plugin's host, in the order that the events come. Other events go nowhere
// (scripts_server.c)
void cli_scripts_server_emit(const CliEachEventJsonSizeCfg *args);

typedef struct
{
    int (*path)(const CliScriptsPathCtx *args);
    void (*call)(const CliScriptsCallCtx *args);
} CliScriptsNsTable;
EMBED_TABLE_LAYOUT(CliScriptsNsTable, path, call);
EMBED_TABLE_STORAGE CliScriptsNsTable cli_scripts_internal EMBED_UNUSED = {
    .path = cli_scripts_path,
    .call = cli_scripts_call,
};

#endif
