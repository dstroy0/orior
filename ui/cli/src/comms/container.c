// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container.c: orior's container: the one process every process orior starts stands under, the language servers,
// kernels, debuggers, shells, database links and runs, and every process those start. On Windows the process puts
// itself in a job object as it starts: every process it starts joins the job on its own, and so does every process
// those start; the job lists them, and ends them all with the container, however the container ends. On Linux and
// macOS the container reads its descendants from the system's list of processes, and ends them as it ends. What orior
// opens for the reader to keep, a page in the browser, a folder in the file manager or another window of orior's, is
// started outside the container and outlives it: on Windows it breaks away from the job; on Linux and macOS it is its
// own process group, and the container leaves it and every process under it out of its members. The calls the system
// gives stand in platform/windows.c and platform/posix.c
#include "container.h"

int cli_under(unsigned int root, unsigned int id, const CliProcess *parents, unsigned int count)
{
    unsigned int at = id;
    // a parent's id the system has given again to a later process can close a loop, and the walk ends after as many
    // steps as the tree has processes
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        if (at == root)
        {
            return 1;
        }
        unsigned int found = count;
        for (unsigned int each = 0u; (found == count) && (each < count); each += 1u)
        {
            found = (parents[each].id == at) ? each : found;
        }
        if ((found == count) || (parents[found].parent == at))
        {
            return 0;
        }
        at = parents[found].parent;
    }
    return 0;
}

unsigned int cli_descendants(unsigned int root, const CliProcess *parents, unsigned int count, unsigned int *found,
                             unsigned int room)
{
    unsigned int founds = 0u;
    // the frontier kept at the end of `found` past what it holds: a process found is walked from in turn
    unsigned int walked = 0u;
    unsigned int frontier = root;
    for (;;)
    {
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            if ((parents[each].parent != frontier) || (parents[each].id == root))
            {
                continue;
            }
            int known = 0;
            for (unsigned int at = 0u; !known && (at < founds); at += 1u)
            {
                known = found[at] == parents[each].id;
            }
            if (!known && (founds < room))
            {
                found[founds] = parents[each].id;
                founds += 1u;
            }
        }
        if (walked >= founds)
        {
            return founds;
        }
        frontier = found[walked];
        walked += 1u;
    }
}

void cli_restarts_made(CliRestarts *restarts, unsigned int most, unsigned long long within)
{
    restarts->most = (most < CLI_RESTARTS_MOST) ? most : CLI_RESTARTS_MOST;
    restarts->within = within;
    restarts->deaths = 0u;
}

int cli_restarts_died(CliRestarts *restarts, unsigned long long at)
{
    // deaths older than the span let go from the front
    unsigned int first = 0u;
    while ((first < restarts->deaths) && ((at - restarts->death[first]) > restarts->within))
    {
        first += 1u;
    }
    memor.move_down(&(MemoriaCfg){.dst = &restarts->death[0],
                                  .src = &restarts->death[first],
                                  .bytes = (size_t)(restarts->deaths - first) * sizeof(restarts->death[0])});
    restarts->deaths -= first;
    if (restarts->deaths >= CLI_RESTARTS_MOST)
    {
        memor.move_down(&(MemoriaCfg){.dst = &restarts->death[0],
                                      .src = &restarts->death[1],
                                      .bytes = (size_t)(restarts->deaths - 1u) * sizeof(restarts->death[0])});
        restarts->deaths -= 1u;
    }
    restarts->death[restarts->deaths] = at;
    restarts->deaths += 1u;
    return restarts->deaths <= restarts->most;
}

unsigned int cli_restarts_count(const CliRestarts *restarts, unsigned long long at)
{
    unsigned int count = 0u;
    for (unsigned int each = 0u; each < restarts->deaths; each += 1u)
    {
        count += ((at - restarts->death[each]) <= restarts->within) ? 1u : 0u;
    }
    return count;
}
