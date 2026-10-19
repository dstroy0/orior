// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container.h: orior's container read: whether a process stands under another in the system's tree, the processes
// under one, and how often a supervised process may die and be started again (container.c); and the calls the
// system gives for the container itself (cli_internal.h, the platform files)
#ifndef CONTAINER_H
#define CONTAINER_H

#include "../command_line/cli_internal.h"

// the most deaths a supervisor counts
#define CLI_RESTARTS_MOST 64u

// How often a supervised process may die and be started again: at most `most` deaths in any span of `within`
// milliseconds, the deaths' moments kept oldest first. One past that is stopped and said, as a supervisor of Erlang's
// stops a child that fails faster than starting it again can help
typedef struct
{
    unsigned int most;
    unsigned long long within;
    unsigned long long death[CLI_RESTARTS_MOST];
    unsigned int deaths;
} CliRestarts;

// 1 where the process `id` is `root` or stands under it in the tree `parents` gives, `count` of them
int cli_under(unsigned int root, unsigned int id, const CliProcess *parents, unsigned int count);
// the processes under `root` in the tree `parents` gives, by child and parent, each once, children after their
// parents, into `found`: their count
unsigned int cli_descendants(unsigned int root, const CliProcess *parents, unsigned int count, unsigned int *found,
                             unsigned int room);
void cli_restarts_made(CliRestarts *restarts, unsigned int most, unsigned long long within);
// a death counted at the moment `at`, in milliseconds: 1 where the process may be started again
int cli_restarts_died(CliRestarts *restarts, unsigned long long at);
// the deaths counted within the span ending at `at`
unsigned int cli_restarts_count(const CliRestarts *restarts, unsigned long long at);

#endif
