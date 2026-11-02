// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_start.h: a session's start as it is planned and then carried out: what it debugs and by which adapter, and the
// request it is started with (debug_start.c, debug_launch.c)
#ifndef DEBUG_START_H
#define DEBUG_START_H
typedef struct CliDebugStartCfg CliDebugStartCfg;

#include "debug_internal.h"
#include "embed_dispatch_layout.h"

// A session's plan: the kind of its start, the test it runs, its language, whether it is a recorded run, the program a
// CMake or Meson build makes from its file, the adapter's spec in the session's start capacity, the toolchain's name
// and id, and the adapter's program; the request, launch or attach, its arguments in the start capacity, the session's
// name, the session that started it; and the address of a debugger already listening, and whether ending the session
// ends the program there
typedef struct
{
    char kind[16];
    char id[CLI_PATH_MAX_LEN];
    char language[CLI_NAME_MAX_LEN];
    int recording;
    CliProgram made;
    int has_created;
    unsigned int spec;
    char tool[CLI_NAME_MAX_LEN];
    char tool_id[CLI_NAME_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    const char *request;
    unsigned int arguments;
    char name[CLI_PATH_MAX_LEN];
    int has_parent;
    unsigned long long parent;
    int listening;
    char host[CLI_NAME_MAX_LEN];
    unsigned int port;
    int ends_program;
    int has_pid;
    unsigned int pid;
} CliDebugPlan;

// the start `start` of `json` planned in the session's place, with `options`, into `plan`: 1, or 0 with why given
// (debug_start.c)
int cli_debug_plan(const CliDebugStartCfg *args);

struct CliDebugStartCfg
{
    CliDebugSession *place;
    const char *root;
    const CliJson *json;
    unsigned int start;
    unsigned int options;
    CliDebugPlan *plan;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*debug_plan)(const CliDebugStartCfg *args);
} CliDebugStartNs;
EMBED_TABLE_LAYOUT(CliDebugStartNs, debug_plan);
EMBED_TABLE_STORAGE CliDebugStartNs cli_debug_start_table EMBED_UNUSED = {
    .debug_plan = cli_debug_plan,
};

#endif
