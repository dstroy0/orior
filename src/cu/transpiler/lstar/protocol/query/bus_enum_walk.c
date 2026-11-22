// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bus_enum_walk.c: the classify walk run over a span in probes the interface can lose, the types collected and the
// enumeration region read from them. An address that ends its probe answered nothing, and the walk goes on from the
// next address in a fresh probe.
#include "bus_enum_walk.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"


// the types classify_walk wrote, read into `answers`, the types, from `next` on. How many it answered in order; a
// line out of order, past the addresses it was given, or not three numbers ends the read where it is
static unsigned long long bus_enum_walk_read(const QueryWalk *walk, const char *lines, unsigned long long next,
                                             unsigned long long given, void *answers)
{
    unsigned int *const types = (unsigned int *)answers;
    unsigned long long read = 0ULL;
    const char *at = lines;
    while (read < given)
    {
        const char *end = NULL;
        const unsigned long long address = mmgr_text_unsigned_in(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        const unsigned long type = mmgr_text_unsigned_in(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        // the read-back word, read past
        mmgr_text_unsigned_in(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        if (address != (walk->from + ((next + read) * walk->stride)))
        {
            break;
        }
        // a type is one of the few HostAddressType values, which an unsigned int represents
        types[next + read] = (unsigned int)type;
        read += 1ULL;
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

int bus_enum_walk(const QueryWalk *walk, unsigned int *types, unsigned long long *base, unsigned long long *stride_out,
                  unsigned long long *records, EngineError *error)
{
    const QueryInterfaceDriver driver = {0, bus_enum_walk_read, bus_enum_walk_ended, types};
    unsigned long long probes = 0ULL;
    if (query_interface_run(walk, &driver, &probes, error) == 0)
    {
        return -1;
    }
    return bus_enum_find(types, walk->count, base, stride_out, records);
}
