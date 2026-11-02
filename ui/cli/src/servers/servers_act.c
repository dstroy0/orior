// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_act.c: a quick fix or refactoring carried out: orior's own edits returned to be written; a server's action,
// its edits filled in by the server first where it left them out, returned to be written, and its command run, whose
// edits the server requests on its own; and an action of a plugin's server carried out by that server, a command it
// does not run itself passed to its plugin. A fix that runs a line in the terminal is the page's to run
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReadState s_read_state = {0};
static CliServersReadState s_resolved = {0};

typedef struct
{
    const CliJson *json;
    unsigned int edit;
    const char *name;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *path;
    unsigned int edits;
    CliLsp *server;
    const char *plugin;
    unsigned int command;
    unsigned int action;
    const char *raw;
    unsigned long long size;
    unsigned int wrapped;
} ServersActCtx;
