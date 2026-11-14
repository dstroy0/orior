// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// survivors.c: the gate's conjunction over the cases, and the descent that orders them
#include "survivors.h"

unsigned int gate_survivors(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                            unsigned char *survives)
{
    unsigned int count = 0u;
    for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
    {
        unsigned char is = 1u;
        for (unsigned int at = 0u; (at < cases) && (is != 0u); at += 1u)
        {
            is = (fails[candidate][at] != 0u) ? 0u : 1u;
        }
        survives[candidate] = is;
        count += is;
    }
    return count;
}

unsigned long long gate_price(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                              const unsigned int *order)
{
    unsigned long long price = 0ull;
    for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
    {
        unsigned int place = cases;
        for (unsigned int at = 0u; (at < cases) && (place == cases); at += 1u)
        {
            place = (fails[candidate][order[at]] != 0u) ? at : cases;
        }
        price += (place == cases) ? (unsigned long long)cases : (unsigned long long)(place + 1u);
    }
    return price;
}

unsigned long long gate_descent(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                                unsigned int *order)
{
    unsigned char located[GATE_CANDIDATES];
    unsigned char placed[GATE_CASES];
    for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
    {
        located[candidate] = 1u;
    }
    for (unsigned int at = 0u; at < cases; at += 1u)
    {
        placed[at] = 0u;
    }
    for (unsigned int place = 0u; place < cases; place += 1u)
    {
        unsigned int best = cases;
        unsigned int best_failing = 0u;
        for (unsigned int at = 0u; at < cases; at += 1u)
        {
            if (placed[at] != 0u)
            {
                continue;
            }
            unsigned int failing = 0u;
            for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
            {
                failing += ((located[candidate] != 0u) && (fails[candidate][at] != 0u)) ? 1u : 0u;
            }
            if ((best == cases) || (failing > best_failing))
            {
                best = at;
                best_failing = failing;
            }
        }
        order[place] = best;
        placed[best] = 1u;
        for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
        {
            located[candidate] = (fails[candidate][best] != 0u) ? 0u : located[candidate];
        }
    }
    return gate_price(fails, candidates, cases, order);
}
