// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_find.c: the environments a tree contains found, at any depth to CLI_ENVS_DEPTH, nearest the top first, each
// with the packages the files beside it require
#include "../command_line/callback_cfg.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

// A folder to look in, and how deep it is below the top
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int depth;
} CliEnvFolder;
// An environment found: its kind, its place in the tree, and where its requirements are read
typedef struct
{
    const char *kind;
    char place[CLI_PATH_MAX_LEN];
    char requires_from[CLI_PATH_MAX_LEN];
} CliFoundEnv;
// The search: the folders to look in, the folder whose folders are being read and its depth, what it found, the order
// it is written in, and the names an environment's folder requires
typedef struct
{
    CliEnvFolder *pending;
    unsigned int pending_count;
    unsigned int pending_capacity;
    const char *folder;
    unsigned int folder_depth;
    CliFoundEnv *found;
    unsigned int count;
    unsigned int found_capacity;
    unsigned int *order;
    CliRequired required;
} CliEnvSearch;
// how many folders below the top a search for environments goes, the most folders it queues to look in, and the most
// environments found; and the folders and the environments their capacities are taken with
#define CLI_ENVS_DEPTH 5u
#define CLI_ENVS_PENDING 4096u
#define CLI_ENVS_MAX 1024u

typedef struct
{
    CliEnvSearch *search;
    const char *kind;
    const char *place;
    const char *requires_from;
    const CliFoundEnv *one;
    const CliFoundEnv *other;
    const char *folder;
    const char *name;
    const char *root;
} EnvsFoundCtx;
