// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bus_enum_walk.c: the classify walk run over a span in probes the interface can lose, the kinds collected and the
// enumeration region read from them. An address that ends its probe answered nothing, and the walk goes on from the
// next address in a fresh probe.
#include "bus_enum_walk.h"

#include <stdlib.h>

// the kinds classify_walk wrote, read into `answers`, the kinds, from `next` on. How many it answered in order; a
// line out of order, past the addresses it was given, or not three numbers ends the read where it stands
static unsigned long long bus_enum_walk_read(const QueryWalk *walk, const char *lines, unsigned long long next,
                                             unsigned long long given, void *answers)
{
    unsigned int *const kinds = (unsigned int *)answers;
    unsigned long long read = 0ull;
    const char *at = lines;
    while (read < given)
    {
        char *end = NULL;
        const unsigned long long address = strtoull(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        const unsigned long kind = strtoul(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        // the read-back word, read past
        strtoul(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        if (address != (walk->from + ((next + read) * walk->stride)))
        {
            break;
        }
        // a kind is one of the few HostKind values, which an unsigned int holds
        kinds[next + read] = (unsigned int)kind;
        read += 1ull;
    }
    return read;
}

// the address that ended its probe is answered by the ending: nothing on the part drove it
static void bus_enum_walk_ended(const QueryWalk *walk, unsigned long long next, InterfaceFault fault, void *answers)
{
    (void)walk;
    (void)fault;
    ((unsigned int *)answers)[next] = HOST_NOTHING;
}

int bus_enum_walk(const QueryWalk *walk, unsigned int *kinds, unsigned long long *base, unsigned long long *stride_out,
                  unsigned long long *records, EngineError *error)
{
    const QueryInterfaceDriver driver = {0, bus_enum_walk_read, bus_enum_walk_ended, kinds};
    unsigned long long probes = 0ull;
    if (query_interface_run(walk, &driver, &probes, error) == 0)
    {
        return -1;
    }
    return bus_enum_find(kinds, walk->count, base, stride_out, records);
}
