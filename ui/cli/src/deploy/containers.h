// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers.h: the machine's containers, as docker and kubectl say: Docker's containers, each with the Compose
// project and service it belongs to where it belongs to one, started, stopped and restarted, its logs read and a
// command run in it; and a Kubernetes cluster's pods, each with its containers, their logs read. A list that cannot be
// read says why in place of its items, as docker's engine not running or kubectl naming no cluster.
//
// ORIOR_DOCKER and ORIOR_KUBECTL name other programs, with words of their own before docker's and kubectl's
// (containers.c)
#ifndef CONTAINERS_H
#define CONTAINERS_H

#include "../command_line/cli_internal.h"

// how many lines of a container's or a pod's log are read, its last
#define CLI_LOG_LINES 500u

// every container docker knows, running or not, written as `{"items": [{"id", "name", "image", "state", "status",
// "project", "service", "ports"}], "problem"}`, by project, service and name; the container `name` started, stopped or
// restarted, as `act` says; the last CLI_LOG_LINES lines the container `name` wrote, its output and its errors as they
// came, written as a JSON string; and `line` run in the container `name` by its shell, what it wrote, its errors after
// and its exit code where it is not 0, written as a JSON string. Each of the last three 1, or 0 with why said
void cli_containers_list(CliLine *out);
int cli_containers_act(const char *name, const char *act, CliLine *said);
int cli_containers_logs(const char *name, CliLine *out, CliLine *said);
int cli_containers_run_in(const char *name, const char *line, CliLine *out, CliLine *said);

// every pod of the cluster kubectl names, in every namespace, written as `{"items": [{"namespace", "name", "phase",
// "ready", "containers", "node"}], "problem"}`, by namespace and name; and the last CLI_LOG_LINES lines the container
// `container` of the pod `name` in `namespace` wrote, or its only container's where `container` is NULL or empty,
// written as a JSON string: 1, or 0 with why said
void cli_containers_pods(CliLine *out);
int cli_containers_pod_logs(const char *space, const char *name, const char *container, CliLine *out, CliLine *said);

#endif
