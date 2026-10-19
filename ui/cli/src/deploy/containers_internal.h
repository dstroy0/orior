// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_internal.h: what the containers sources share: docker and kubectl run, a list's problem written, and
// the room a list is read into, under its lock
#ifndef CONTAINERS_INTERNAL_H
#define CONTAINERS_INTERNAL_H

#include "../text/json_tree.h"
#include "containers.h"

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

// The containers or pods listed, `most` of them, the order they are written in, and the texts kept of them, `text_most`
// bytes; one cell, sized to what docker or kubectl wrote, taken with the lock and given back with it
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
int cli_containers_held(CliLine *out, unsigned int most, unsigned long long text_most);
void cli_containers_released(void);

// docker or kubectl run with `args`, what it wrote into `ran`: 1, or 0 with what it said where it failed, its first
// line where it said many
int cli_containers_tool(const char *variable, const char *program, const char *const *args, unsigned int count,
                        CliRan *ran, CliLine *said);
// a list's problem written, and the list closed
void cli_containers_problem_written(CliLine *out, int listed, const char *problem, unsigned long long length);
// `size` bytes of what docker listed, a container a line, and of what kubectl listed, its pods, written as
// cli_containers_list and cli_containers_pods write them, with `problem_size` bytes of `problem` where it is not
// `listed` (containers.c, containers_pods.c)
void cli_containers_listed(const char *text, unsigned long long size, int listed, const char *problem,
                           unsigned long long problem_size, CliLine *out);
void cli_containers_pods_listed(const char *text, unsigned long long size, int listed, const char *problem,
                                unsigned long long problem_size, CliLine *out);

#endif
