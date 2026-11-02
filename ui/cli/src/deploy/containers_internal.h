// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_internal.h: what the containers sources share: docker and kubectl run, a list's problem written, and
// the room a list is read into, under its lock
#ifndef CONTAINERS_INTERNAL_H
#define CONTAINERS_INTERNAL_H
typedef struct CliContainersOutCtx CliContainersOutCtx;
typedef struct CliContainersVariableCtx CliContainersVariableCtx;
typedef struct CliContainersOutListedCtx CliContainersOutListedCtx;
typedef struct CliContainersCtx CliContainersCtx;

#include "../text/json_tree.h"
#include "containers.h"
#include "embed_dispatch_layout.h"

// A container docker lists: its texts' places, and whether it belongs to a Compose project and service
typedef struct
{
    unsigned int id;
    unsigned int name;
    unsigned int image;
    unsigned int state;
    unsigned int status;
    unsigned int project;
    unsigned int service;
    unsigned int ports;
    int has_project;
    int has_service;
} CliContainer;

// The containers or pods listed, `most` of them, the order they are written in, and the texts stored of them,
// `text_most` bytes; one cell, sized to what docker or kubectl wrote, taken with the lock and given back with it
typedef struct
{
    CliContainer *item;
    unsigned int *order;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned long long used;
    unsigned long long text_most;
} CliContainers;

extern CliContainers g_cli_containers;

// the lock taken and the list's cell taken under it, for `most` items and `text_most` bytes of texts: 1, or 0 with the
// lock given back and an empty list written into `out` with why as its problem; and the cell given back with the lock
// (containers.c)
int cli_containers_lock(const CliContainersCtx *args);
void cli_containers_release(void);

// docker or kubectl run with `args`, what it wrote into `ran`: 1, or 0 with what it printed where it failed, its first
// line where it printed many
int cli_containers_tool(const CliContainersVariableCtx *args);
// a list's problem written, and the list closed
void cli_containers_problem_write(const CliContainersOutListedCtx *args);
// `size` bytes of what docker listed, a container a line, and of what kubectl listed, its pods, written as
// cli_containers_list and cli_containers_pods write them, with `problem_size` bytes of `problem` where it is not
// `listed` (containers.c, containers_pods.c)
void cli_containers_on_list(const CliContainersOutCtx *args);
void cli_containers_pods_list(const CliContainersOutCtx *args);

struct CliContainersOutCtx
{
    VerbaLinea *out;
    int listed;
    const char *problem;
    const char *text;
    unsigned long long size;
    unsigned long long problem_size;
};

struct CliContainersVariableCtx
{
    const char *variable;
    const char *program;
    const char *const *request;
    unsigned int count;
    CliRan *ran;
    VerbaLinea *err_msg;
};

struct CliContainersOutListedCtx
{
    VerbaLinea *out;
    int listed;
    const char *problem;
    unsigned long long length;
};

struct CliContainersCtx
{
    VerbaLinea *out;
    unsigned int most;
    unsigned long long text_most;
};

typedef struct
{
    int (*lock)(const CliContainersCtx *args);
} CliContainersNsTable;
EMBED_TABLE_LAYOUT(CliContainersNsTable, lock);
EMBED_TABLE_STORAGE CliContainersNsTable cli_containers_internal EMBED_UNUSED = {
    .lock = cli_containers_lock,
};

typedef struct
{
    int (*tool)(const CliContainersVariableCtx *args);
} CliContainersVariableNs;
EMBED_TABLE_LAYOUT(CliContainersVariableNs, tool);
EMBED_TABLE_STORAGE CliContainersVariableNs cli_containers_variable_internal EMBED_UNUSED = {
    .tool = cli_containers_tool,
};

typedef struct
{
    void (*problem_write)(const CliContainersOutListedCtx *args);
} CliContainersOutListedNs;
EMBED_TABLE_LAYOUT(CliContainersOutListedNs, problem_write);
EMBED_TABLE_STORAGE CliContainersOutListedNs cli_containers_out_listed_internal EMBED_UNUSED = {
    .problem_write = cli_containers_problem_write,
};

typedef struct
{
    void (*list)(const CliContainersOutCtx *args);
    void (*pods_list)(const CliContainersOutCtx *args);
} CliContainersOutNsTable;
EMBED_TABLE_LAYOUT(CliContainersOutNsTable, list, pods_list);
EMBED_TABLE_STORAGE CliContainersOutNsTable cli_containers_out_internal EMBED_UNUSED = {
    .list = cli_containers_on_list,
    .pods_list = cli_containers_pods_list,
};

#endif
