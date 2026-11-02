// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container.h: orior's container read: whether a process stands under another in the system's tree, the processes
// under one, and how often a supervised process may die and be started again (container.c); and the calls the
// system gives for the container itself (cli_internal.h, the platform files)
#ifndef CONTAINER_H
#define CONTAINER_H
typedef struct CliContainerRootCfg CliContainerRootCfg;
typedef struct CliContainerRootIdCfg CliContainerRootIdCfg;
typedef struct CliContainerRestartsCfg CliContainerRestartsCfg;
typedef struct CliContainerCfg CliContainerCfg;
typedef struct CliContainerAtCfg CliContainerAtCfg;
typedef struct CliContainerRestartsAtCfg CliContainerRestartsAtCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the most deaths a supervisor counts
#define CLI_RESTARTS_MAX 64u

// How often a supervised process may die and be started again: at most `most` deaths in any span of `within`
// milliseconds, the deaths' moments stored oldest first. One past that is stopped and reported, as a supervisor of
// Erlang's stops a child that fails faster than starting it again can help
typedef struct
{
    unsigned int most;
    unsigned long long within;
    unsigned long long death[CLI_RESTARTS_MAX];
    unsigned int deaths;
} CliRestarts;

// the processes the system lists, a cell counted to `owner`, their count into `count`, listed again in a cell twice as
// large while a list fills its cell, up to CLI_PROCESSES_MAX: the cell, or NULL with none counted where the cellblock
// could not meet it
CliProcess *cli_processes_alloc(const CliContainerCfg *args);
// 1 where the process `id` is `root` or stands under it in the tree `parents` gives, `count` of them
int cli_under(const CliContainerRootIdCfg *args);
// the processes under `root` in the tree `parents` gives, by child and parent, each once, children after their
// parents, into `found`: their count
unsigned int cli_descendants(const CliContainerRootCfg *args);
void cli_restarts_init(const CliContainerRestartsCfg *args);
// a death counted at the moment `at`, in milliseconds: 1 where the process may be started again
int cli_restarts_dy(const CliContainerRestartsAtCfg *args);
// the deaths counted within the span ending at `at`
unsigned int cli_restarts_count(const CliContainerAtCfg *args);

struct CliContainerRootCfg
{
    unsigned int root;
    const CliProcess *parents;
    unsigned int count;
    unsigned int *found;
    unsigned int capacity;
};

struct CliContainerRootIdCfg
{
    unsigned int root;
    unsigned int id;
    const CliProcess *parents;
    unsigned int count;
};

struct CliContainerRestartsCfg
{
    CliRestarts *restarts;
    unsigned int most;
    unsigned long long within;
};

struct CliContainerCfg
{
    CellOwner *owner;
    unsigned int *count_pointer;
};

struct CliContainerAtCfg
{
    unsigned long long at;
    const CliRestarts *restarts_read;
};

struct CliContainerRestartsAtCfg
{
    CliRestarts *restarts;
    unsigned long long at;
};

typedef struct
{
    CliProcess *(*processes_alloc)(const CliContainerCfg *args);
} CliContainerNs;
EMBED_TABLE_LAYOUT(CliContainerNs, processes_alloc);
EMBED_TABLE_STORAGE CliContainerNs cli_container EMBED_UNUSED = {
    .processes_alloc = cli_processes_alloc,
};

typedef struct
{
    int (*under)(const CliContainerRootIdCfg *args);
} CliContainerRootIdNs;
EMBED_TABLE_LAYOUT(CliContainerRootIdNs, under);
EMBED_TABLE_STORAGE CliContainerRootIdNs cli_container_root_id EMBED_UNUSED = {
    .under = cli_under,
};

typedef struct
{
    unsigned int (*descendants)(const CliContainerRootCfg *args);
} CliContainerRootNs;
EMBED_TABLE_LAYOUT(CliContainerRootNs, descendants);
EMBED_TABLE_STORAGE CliContainerRootNs cli_container_root EMBED_UNUSED = {
    .descendants = cli_descendants,
};

typedef struct
{
    void (*restarts_init)(const CliContainerRestartsCfg *args);
} CliContainerRestartsNs;
EMBED_TABLE_LAYOUT(CliContainerRestartsNs, restarts_init);
EMBED_TABLE_STORAGE CliContainerRestartsNs cli_container_restarts EMBED_UNUSED = {
    .restarts_init = cli_restarts_init,
};

typedef struct
{
    int (*restarts_dy)(const CliContainerRestartsAtCfg *args);
} CliContainerRestartsAtNs;
EMBED_TABLE_LAYOUT(CliContainerRestartsAtNs, restarts_dy);
EMBED_TABLE_STORAGE CliContainerRestartsAtNs cli_container_restarts_at EMBED_UNUSED = {
    .restarts_dy = cli_restarts_dy,
};

typedef struct
{
    unsigned int (*restarts_count)(const CliContainerAtCfg *args);
} CliContainerAtNs;
EMBED_TABLE_LAYOUT(CliContainerAtNs, restarts_count);
EMBED_TABLE_STORAGE CliContainerAtNs cli_container_at EMBED_UNUSED = {
    .restarts_count = cli_restarts_count,
};

#endif
