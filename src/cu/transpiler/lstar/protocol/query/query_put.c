// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_put.c: one query of the query protocol, made of a word put and a word read
#include "query_put.h"

// whether the qualifier is true at the address
static unsigned int query_satisfied(const Query *query)
{
    switch (query->qualifier)
    {
    case QUERY_RETAINS: {
        unsigned int back = 0u;
        return (host_address_probe(query->address, &back) == HOST_RETAINS) ? 1u : 0u;
    }
    case QUERY_EQUALS:
        return (host_read(query->address) == query->word) ? 1u : 0u;
    case QUERY_ADVANCES: {
        // The first read is retained and the address read again until a word differs from it or the reads run out: a
        // counter that steps once in a while is caught at its step and not only where a step falls between two
        // reads in a row. A counter that wraps between the reads still reads forward: the difference taken in the
        // word's own width is a forward step where it sits in the lower half of the word
        const unsigned int first = host_read(query->address);
        const unsigned long long reads = (query->turns > 1ull) ? query->turns : 1ull;
        unsigned int then = first;
        for (unsigned long long turn = 0ull; (turn < reads) && (then == first); turn += 1ull)
        {
            then = host_read(query->address);
        }
        return query_advanced(first, then);
    }
    default:
        return 0u;
    }
}

unsigned int query_advanced(unsigned int first, unsigned int then)
{
    const unsigned int step = then - first;
    return ((step != 0u) && (step < 0x80000000u)) ? 1u : 0u;
}

unsigned int query_put(Query *query)
{
    const unsigned int clocked = (query->clock != 0ull) ? 1u : 0u;
    const unsigned int start = (clocked != 0u) ? host_read(query->clock) : 0u;
    const unsigned int satisfied = query_satisfied(query);
    const unsigned int end = (clocked != 0u) ? host_read(query->clock) : 0u;

    // the clock's advance in its own width, which represents one wrap of the counter
    query->cost = (clocked != 0u) ? (unsigned long long)(end - start) : 0ull;
    query->cost_read = clocked;
    query->fault = 0u;

    if (satisfied == 0u)
    {
        query->type = QUERY_UNSATISFIED;
        query->bit = 0u;
    }
    // A bound is the max the answer may cost, and a cost nobody read cannot be displayed to be inside it: a bound with
    // no clock reads 0 and is never a verdict nobody measured
    else if ((query->bound != 0ull) && ((clocked == 0u) || (query->cost > query->bound)))
    {
        query->type = QUERY_PAST_BOUND;
        query->bit = 0u;
    }
    else
    {
        // Unbound, the cost is what the query returns, and the type still reports the qualifier was true. The cost of a
        // query whose qualifier was not true is not the cost of an answer, and the unbound pass separates the two
        query->type = QUERY_SATISFIED;
        query->bit = 1u;
    }
    return query->bit;
}
