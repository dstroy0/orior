// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_calls.c: the call hierarchy: the function at a place as its top, as its server finds it where the server
// answers for calls, as the tree's leaves give it where the file's grammar names its functions and regions
// (symbols_calls.c), and as the inspections' tree finds it elsewhere; and the functions that call a function, or that
// it calls, each with the places of its calls, from where the hierarchy's top came from. A function is written with its
// name and what the server reports of it, its file and the span of its name, the file its calls stand in and where
// each starts, and the server's or the tree's own name for it, which is given back to read the level below it
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// What a level of the tree's hierarchy is written with: the tree and the tree's root, the file the hierarchy is
// at, whether its calls come in, where it is written and whether one is
typedef struct
{
    const char *tree;
    const char *root;
    const char *file;
    int incoming;
    VerbaLinea *out;
    int written;
} CliServersCalls;

// what the server answers, under the reading's lock
static CliServersReadState s_read_state = {0};
static CliServersReadState s_item = {0};

typedef struct
{
    const char *tree;
    const char *path;
    VerbaLinea *out;
    const CliTextPosition *sites;
    unsigned int count;
    const CliJson *json;
    unsigned int item;
    const char *site;
    const CliServersCalls *calls;
    const char *file;
    const CliFacts *facts;
    unsigned int function;
    const CliInspectSpan *sites_inspect_span_pointer;
    char *root;
    char *file_out;
    VerbaLinea *err_msg;
} ServersCallsCtx;
// The tree's call hierarchy being written: the tree's top, the tree's file the calls stand in where they are the
// function's asked about, NULL where each stands in its own, where it is written, and whether one is
typedef struct
{
    const char *tree;
    const char *site;
    VerbaLinea *out;
    int written;
} CliServersTreeCalls;
