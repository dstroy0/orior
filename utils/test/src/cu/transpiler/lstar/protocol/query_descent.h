// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_descent.h: the run size and the count of passes of a known order, steered by the part, for the checks that put
// the order to the host
//
// NO COUNT OF PASSES AND NO RUN SIZE IS SET HERE. Both are steered, the way the engine's descent steers its probes.
// Each pass is a level: its bit for a pair of neighboring links is 1 where the later link reads dearer. A pair is
// while its count over every pass put so far leans past twice its own spread neither way, and the descent ends the
// first pass nothing is, or at QUERY_DESCENT_PASSES_MAX, the bound it is limited to before it starts. A pair is
// read on its whole record at every pass and is never closed early: the reads carry noise, and a count that leans at a
// few passes can stop leaning at more. The run size is the candidate the descent is steered over. Every size is read,
// and the one that leaves the fewest pairs located, then spends the fewest puts, is chosen, a tie going to the
// shorter. A size stops once it has spent the puts the best so far needed with a pair still located, since past that
// it cannot do better.
#ifndef QUERY_DESCENT_H
#define QUERY_DESCENT_H

#include "../../../../../../../src/cu/transpiler/lstar/protocol/order/query_order.h"

#include <stdio.h>

#if defined(_WIN32)

#define QUERY_DESCENT_LINKS 7u
// the max passes a descent puts: the bound it is limited to, fixed before it starts
#define QUERY_DESCENT_PASSES_MAX 64u
// the run sizes the descent may reach, as powers of two puts a run
#define QUERY_DESCENT_SIZE_LEAST 10u
#define QUERY_DESCENT_SIZE_MAX 16u

// a pair still located, a pair whose later link leans dearer, and a pair whose later link leans cheaper
#define QUERY_DESCENT_STANDING 0u
#define QUERY_DESCENT_DEARER 1u
#define QUERY_DESCENT_CHEAPER 2u

// one descent: for every pair, the passes it read dearer in and which way its count leans at the end
typedef struct
{
    unsigned int dearer[QUERY_DESCENT_LINKS];
    unsigned int decided[QUERY_DESCENT_LINKS];
    unsigned int passes;
    unsigned int located;
    unsigned int solved;
} QueryDescent;

// `count`, a count, as an exact integer
static void query_descent_exact(OriorExactInteger *value, unsigned int count)
{
    orior_exact_zero(value);
    value->limb[0] = count;
    value->sign = (count != 0u) ? 1 : 0;
}

// Whether `toward` of `of` leans past twice its own spread toward it: 2 * toward - of is positive and its square is
// past 4 * of. Every term an exact integer
static int query_descent_leans(unsigned int toward, unsigned int of)
{
    OriorExactInteger twice;
    OriorExactInteger whole;
    OriorExactInteger lean;
    OriorExactInteger square;
    OriorExactInteger four;
    query_descent_exact(&twice, 2u * toward);
    query_descent_exact(&whole, of);
    query_descent_exact(&four, 4u * of);
    if ((orior_exact_subtract(&twice, &whole, &lean) != ORIOR_EXACT_OK) || (lean.sign <= 0) ||
        (orior_exact_multiply(&lean, &lean, &square) != ORIOR_EXACT_OK))
    {
        return 0;
    }
    return (orior_exact_compare(&square, &four) > 0) ? 1 : 0;
}

// The order put one pass at a time at `order->repeat`, each pass solved on its own, until no pair is, the bound
// is reached, or `spend_max` puts are spent with a pair still located. 0 for `spend_max` sets no such limit
static void query_descent_put(QueryOrder *order, unsigned long long spend_max, QueryDescent *descent)
{
    static QueryCost s_cost[QUERY_DESCENT_LINKS];
    descent->passes = 0u;
    descent->solved = 1u;
    descent->located = QUERY_DESCENT_LINKS - 1u;
    for (unsigned int number = 0u; number < QUERY_DESCENT_LINKS; number += 1u)
    {
        descent->dearer[number] = 0u;
        descent->decided[number] = QUERY_DESCENT_STANDING;
    }
    order->passes = 1u;
    while ((descent->located > 0u) && (descent->passes < QUERY_DESCENT_PASSES_MAX))
    {
        if ((spend_max != 0ull) && (((unsigned long long)descent->passes * order->repeat) >= spend_max))
        {
            return;
        }
        if (query_order_put(order, s_cost) == 0)
        {
            descent->solved = 0u;
            return;
        }
        descent->passes += 1u;
        OriorExactInteger numerator[QUERY_DESCENT_LINKS];
        OriorExactInteger denominator;
        if (query_order_solve(QUERY_DESCENT_LINKS, s_cost, numerator, &denominator) == 0)
        {
            descent->solved = 0u;
            continue;
        }
        descent->located = 0u;
        for (unsigned int number = 1u; number < QUERY_DESCENT_LINKS; number += 1u)
        {
            // one pass's links share one denominator, and its sign is positive: the numerators order them
            descent->dearer[number] +=
                (orior_exact_compare(&numerator[number], &numerator[number - 1u]) > 0) ? 1u : 0u;
            descent->decided[number] = QUERY_DESCENT_STANDING;
            if (query_descent_leans(descent->dearer[number], descent->passes) != 0)
            {
                descent->decided[number] = QUERY_DESCENT_DEARER;
            }
            else if (query_descent_leans(descent->passes - descent->dearer[number], descent->passes) != 0)
            {
                descent->decided[number] = QUERY_DESCENT_CHEAPER;
            }
            descent->located += (descent->decided[number] == QUERY_DESCENT_STANDING) ? 1u : 0u;
        }
    }
}

// Whether `next` at `next_repeat` puts a run does strictly better than `best` at `best_repeat`: fewer pairs located,
// or as many for fewer puts spent. Every query of a pass puts `repeat` runs of its links: a descent spends its passes
// times its repeat
static int query_descent_better(const QueryDescent *next, unsigned long long next_repeat, const QueryDescent *best,
                                unsigned long long best_repeat)
{
    if (next->located != best->located)
    {
        return (next->located < best->located) ? 1 : 0;
    }
    return (((unsigned long long)next->passes * next_repeat) < ((unsigned long long)best->passes * best_repeat)) ? 1
                                                                                                                 : 0;
}

// The run size, steered over every size the descent may reach, each one printed. The size chosen is returned and its
// descent written to `best`; `solved` is 1 where every pass of every size solved inside the exact width. A short run
// drowns in the counting's own spread and a long one gathers whatever else the part is doing, and where the floor
// between them falls is the part's to determine
static unsigned long long query_descent_size(QueryOrder *order, QueryDescent *best, unsigned int *solved)
{
    QueryDescent next;
    unsigned long long best_repeat = 0ull;
    best->passes = 0u;
    best->located = QUERY_DESCENT_LINKS - 1u;
    *solved = 1u;
    for (unsigned int power = QUERY_DESCENT_SIZE_LEAST; power <= QUERY_DESCENT_SIZE_MAX; power += 1u)
    {
        order->repeat = 1ull << power;
        const unsigned long long spend_max =
            ((best_repeat != 0ull) && (best->located == 0u)) ? ((unsigned long long)best->passes * best_repeat) : 0ull;
        query_descent_put(order, spend_max, &next);
        *solved = (next.solved == 1u) ? *solved : 0u;
        printf("  2^%u puts a run: %u pairs located after %u passes, %llu puts spent\n", power, next.located,
               next.passes, (unsigned long long)next.passes * order->repeat);
        if ((best_repeat == 0ull) || (query_descent_better(&next, order->repeat, best, best_repeat) != 0))
        {
            *best = next;
            best_repeat = order->repeat;
        }
    }
    order->repeat = best_repeat;
    printf("  the descent chooses %llu puts a run\n", best_repeat);
    return best_repeat;
}

#endif

#endif
