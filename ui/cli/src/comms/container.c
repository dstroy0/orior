// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container.c: orior's container: the one process every process orior_cli starts stands under, the language servers,
// kernels, debuggers, shells, database links and runs, and every process those start. On Windows the process puts
// itself in a job object as it starts: every process it starts joins the job on its own, and so does every process
// those start; the job lists them, and ends them all with the container, however the container ends. On Linux and
// macOS the container reads its descendants from the system's list of processes, and ends them as it ends. What orior
// opens for the reader to retain, a page in the browser, a folder in the file manager or another window of orior's, is
// started outside the container and outlives it: on Windows it breaks away from the job; on Linux and macOS it is its
// own process group, and the container leaves it and every process under it out of its members. The calls the system
// gives stand in platform/windows.c and platform/posix.c
#include "container.h"

CliProcess *cli_processes_alloc(const CliContainerCfg *args)
{
    unsigned int *count = args->count_pointer;
    CliProcess *const processes =
        g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_PROCESSES_MAX * sizeof(CliProcess), args->owner);
    *count = (processes != NULL) ? EMBED_CALL(cli_capacity_processes_internal.processes_list, CliCapacityProcessesCtx,
                                              .processes = processes, .capacity_u32 = CLI_PROCESSES_MAX)
                                 : 0u;
    return processes;
}

int cli_under(const CliContainerRootIdCfg *args)
{
    unsigned int at = args->id;
    // a parent's id the system has given again to a later process can close a loop, and the walk ends after as many
    // steps as the tree has processes
    for (unsigned int step = 0u; step < args->count; step += 1u)
    {
        if (at == args->root)
        {
            return 1;
        }
        unsigned int found = args->count;
        for (unsigned int each = 0u; (found == args->count) && (each < args->count); each += 1u)
        {
            found = (args->parents[each].id == at) ? each : found;
        }
        if ((found == args->count) || (args->parents[found].parent == at))
        {
            return 0;
        }
        at = args->parents[found].parent;
    }
    return 0;
}

unsigned int cli_descendants(const CliContainerRootCfg *args)
{
    unsigned int founds = 0u;
    // the frontier stored at the end of `found` past what it contains: a process found is walked from in turn
    unsigned int walked = 0u;
    unsigned int frontier = args->root;
    for (;;)
    {
        for (unsigned int each = 0u; each < args->count; each += 1u)
        {
            if ((args->parents[each].parent != frontier) || (args->parents[each].id == args->root))
            {
                continue;
            }
            int known = 0;
            for (unsigned int at = 0u; !known && (at < founds); at += 1u)
            {
                known = args->found[at] == args->parents[each].id;
            }
            if (!known && (founds < args->capacity))
            {
                args->found[founds] = args->parents[each].id;
                founds += 1u;
            }
        }
        if (walked >= founds)
        {
            return founds;
        }
        frontier = args->found[walked];
        walked += 1u;
    }
}

void cli_restarts_init(const CliContainerRestartsCfg *args)
{
    unsigned int most = args->most;
    unsigned long long within = args->within;
    args->restarts->most = (most < CLI_RESTARTS_MAX) ? most : CLI_RESTARTS_MAX;
    args->restarts->within = within;
    args->restarts->deaths = 0u;
}

int cli_restarts_dy(const CliContainerRestartsAtCfg *args)
{
    // deaths older than the span let go from the front
    unsigned int first = 0u;
    while ((first < args->restarts->deaths) && ((args->at - args->restarts->death[first]) > args->restarts->within))
    {
        first += 1u;
    }
    memor.move_down(
        &(MemoriaCfg){.dst = &args->restarts->death[0],
                      .src = &args->restarts->death[first],
                      .bytes = (size_t)(args->restarts->deaths - first) * sizeof(args->restarts->death[0])});
    args->restarts->deaths -= first;
    if (args->restarts->deaths >= CLI_RESTARTS_MAX)
    {
        memor.move_down(
            &(MemoriaCfg){.dst = &args->restarts->death[0],
                          .src = &args->restarts->death[1],
                          .bytes = (size_t)(args->restarts->deaths - 1u) * sizeof(args->restarts->death[0])});
        args->restarts->deaths -= 1u;
    }
    args->restarts->death[args->restarts->deaths] = args->at;
    args->restarts->deaths += 1u;
    return args->restarts->deaths <= args->restarts->most;
}

unsigned int cli_restarts_count(const CliContainerAtCfg *args)
{
    unsigned int count = 0u;
    for (unsigned int each = 0u; each < args->restarts_read->deaths; each += 1u)
    {
        count += ((args->at - args->restarts_read->death[each]) <= args->restarts_read->within) ? 1u : 0u;
    }
    return count;
}
