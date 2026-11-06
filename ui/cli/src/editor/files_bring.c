// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_fetch.c: files and folders copied into the tree as a paste copies them, each file reported as it is copied, and
// copied into a tree on another machine through calls of the tree's side there, a part at a time
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "files_internal.h"

// What a bring over a link copies: the call of the tree's side, who is notified how far it is, and a call's arguments
// and its answer, each a line given back once the bring ends
typedef struct
{
    CliCall call;
    void *call_context;
    CliProgressFn progress;
    void *context;
    VerbaLinea arguments;
    VerbaLinea answer;
} CliOver;
// how often a file brought into the tree reports how far along its copy is, in milliseconds; and how much of a file
// goes into one write here, or over the link in one part
#define CLI_FETCH_PROGRESS_EVERY 100ull
#define CLI_PART_BYTES (1ull << 20u)

// the bytes a call's arguments and an answer of the tree's side are written in
#define CLI_OVER_ARGUMENTS_CAPACITY (64ull << 20u)
#define CLI_OVER_ANSWER_CAPACITY (64ull << 20u)

// What a bring copies: the tree, the path it was requested, which a failure names, and who is notified how far it is
typedef struct
{
    const char *root;
    const char *source;
    CliProgressFn progress;
    void *context;
} CliFetching;

typedef struct
{
    CliOver *over;
    const char *name;
    VerbaLinea *err_msg;
    const char *into;
    char *to;
    const char *const *from;
    unsigned int count;
    VerbaLinea *out;
} FilesBringCtx;
