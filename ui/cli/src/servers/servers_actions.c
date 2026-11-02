// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_actions.c: the quick fixes and refactorings for a span of a file: the inspections' and the checkers' fixes
// of the findings that touch it first, then what its server offers, then what each plugin server that has open it
// offers, each given the diagnostics it gave that touch it, a fix offered for a diagnostic and again for its note
// listed once, those that can be carried out first and the preferred fixes before the rest. An action of a plugin's
// server carries the plugin's id, so it is carried out by that server (servers_act.c). A server given the file as a
// slice is asked about the slice's lines, with the diagnostics on them
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReadState s_read_state = {0};

typedef struct
{
    CliKeyed *keyed;
    const char *key;
    unsigned int array;
    CliTextPosition from;
    CliTextPosition to;
    int *written;
    VerbaLinea *out;
    unsigned int all;
    const char *path;
    const char *uri;
    VerbaLinea *params;
    const CliJson *json;
    unsigned int one;
    const char *plugin;
    CliLsp *server;
    VerbaLinea *err_msg;
} ServersActionsCtx;
