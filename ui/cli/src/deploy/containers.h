// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers.h: the machine's containers, as docker and kubectl report: Docker's containers, each with the Compose
// project and service it belongs to where it belongs to one, started, stopped and restarted, its logs read and a
// command run in it; and a Kubernetes cluster's pods, each with its containers, their logs read. A list that cannot be
// read reports why in place of its items, as docker's engine not running or kubectl naming no cluster.
//
// ORIOR_DOCKER and ORIOR_KUBECTL name other programs, with words of their own before docker's and kubectl's
// (containers.c)
#ifndef CONTAINERS_H
#define CONTAINERS_H
typedef struct CliContainersOutCfg CliContainersOutCfg;
typedef struct CliContainersOutLineCfg CliContainersOutLineCfg;
typedef struct CliContainersCfg CliContainersCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// how many lines of a container's or a pod's log are read, its last
#define CLI_LOG_LINES 500u

// every container docker knows, running or not, written as `{"items": [{"id", "name", "image", "state", "status",
// "project", "service", "ports"}], "problem"}`, by project, service and name; the container `name` started, stopped or
// restarted, as `act` specifies; the last CLI_LOG_LINES lines the container `name` wrote, its output and its errors as
// they came, written as a JSON string; and `line` run in the container `name` by its shell, what it wrote, its errors
// after and its exit code where it is not 0, written as a JSON string. Each of the last three 1, or 0 with why given
int cli_containers_act(const CliContainersCfg *args);
int cli_containers_logs(const CliContainersOutLineCfg *args);
int cli_containers_run_in(const CliContainersOutLineCfg *args);

struct CliContainersOutCfg
{
    VerbaLinea *out;
    const char *name;
    VerbaLinea *err_msg;
    const char *space;
    const char *container;
};

struct CliContainersOutLineCfg
{
    VerbaLinea *out;
    const char *name;
    VerbaLinea *err_msg;
    const char *line;
};

struct CliContainersCfg
{
    const char *name;
    const char *act;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*logs)(const CliContainersOutLineCfg *args);
    int (*run_in)(const CliContainersOutLineCfg *args);
} CliContainersOutLineNs;
EMBED_TABLE_LAYOUT(CliContainersOutLineNs, logs, run_in);
EMBED_TABLE_STORAGE CliContainersOutLineNs cli_containers_out_line EMBED_UNUSED = {
    .logs = cli_containers_logs,
    .run_in = cli_containers_run_in,
};

typedef struct
{
    int (*act)(const CliContainersCfg *args);
} CliContainersNs;
EMBED_TABLE_LAYOUT(CliContainersNs, act);
EMBED_TABLE_STORAGE CliContainersNs cli_containers EMBED_UNUSED = {
    .act = cli_containers_act,
};

#endif
