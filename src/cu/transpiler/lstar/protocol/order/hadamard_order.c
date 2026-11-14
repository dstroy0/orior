// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hadamard_order.c: the known order of queries, its solve, and the read of whether the links contend
#include "hadamard_order.h"
#include "../counterexample/ladder.h"

#include "../../../../../../includes/exact_types/integers/exact_integer.h"

#include <string.h>

// the count of ones in `word`
static unsigned int bits_set(unsigned int word)
{
    unsigned int count = 0u;
    while (word != 0u)
    {
        word &= word - 1u;
        count += 1u;
    }
    return count;
}

int hadamard_order_fits(unsigned int links)
{
    return ((links >= 3u) && (links <= HADAMARD_ORDER_LINKS_MOST) && (((links + 1u) & links) == 0u)) ? 1 : 0;
}

int hadamard_order_covers(unsigned int links, unsigned int row, unsigned int link)
{
    if ((hadamard_order_fits(links) == 0) || (row >= links) || (link >= links))
    {
        return 0;
    }
    return ((bits_set((row + 1u) & (link + 1u)) & 1u) != 0u) ? 1 : 0;
}

int hadamard_order_solve(unsigned int links, const unsigned long long *cost, long long *scaled)
{
    if (hadamard_order_fits(links) == 0)
    {
        return 0;
    }
    // a summed cost is under 2^55 by the header's bound, which a signed 64 bit word represents
    long long whole = 0;
    for (unsigned int row = 0u; row < links; row += 1u)
    {
        whole += (long long)cost[row];
    }
    for (unsigned int link = 0u; link < links; link += 1u)
    {
        long long covered = 0;
        for (unsigned int row = 0u; row < links; row += 1u)
        {
            covered += (hadamard_order_covers(links, row, link) != 0) ? (long long)cost[row] : 0;
        }
        scaled[link] = (4 * covered) - (2 * whole);
    }
    return 1;
}

unsigned int hadamard_sweep_covers(unsigned int links, unsigned int seed, unsigned int row, unsigned char *covered)
{
    if ((links == 0u) || (links > HADAMARD_ORDER_LINKS_MOST))
    {
        return 0u;
    }
    unsigned char order[HADAMARD_ORDER_LINKS_MOST];
    for (unsigned int at = 0u; at < links; at += 1u)
    {
        // a link index is under HADAMARD_ORDER_LINKS_MOST, which a byte represents
        order[at] = (unsigned char)at;
    }
    unsigned int state = seed ^ ((row + 1u) * 0x9e3779b9u);
    state = (state != 0u) ? state : 0x9e3779b9u;
    for (unsigned int at = links; at > 1u; at -= 1u)
    {
        // a zero state never leaves zero, and the state is maintained off zero above
        const unsigned int with = ladder_swept(&state) % at;
        const unsigned char swapped = order[at - 1u];
        order[at - 1u] = order[with];
        order[with] = swapped;
    }
    // one link, half plus one, and every link, in turn: a square read against a line has the least spread with its
    // queries at the two ends and the middle
    const unsigned int counts[3] = {1u, (links + 1u) / 2u, links};
    const unsigned int count = counts[row % 3u];
    memset(covered, 0, links);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        covered[order[at]] = 1u;
    }
    return count;
}

// `given` as an exact integer
static void hadamard_exact(OriorExactInteger *value, long long given)
{
    orior_exact_zero(value);
    // the magnitude of the most negative word is one past the most positive, which the unsigned word represents
    const unsigned long long magnitude = (given < 0) ? (0ull - (unsigned long long)given) : (unsigned long long)given;
    value->limb[0] = (uint32_t)(magnitude & 0xffffffffull);
    value->limb[1] = (uint32_t)(magnitude >> 32);
    value->sign = (given > 0) ? 1 : ((given < 0) ? -1 : 0);
}

// `left` times `right` added into `sum`; 1, or 0 where the exact width could not represent it
static int hadamard_exact_add_product(OriorExactInteger *sum, const OriorExactInteger *left,
                                      const OriorExactInteger *right)
{
    OriorExactInteger product;
    if (orior_exact_multiply(left, right, &product) != ORIOR_EXACT_OK)
    {
        return 0;
    }
    return (orior_exact_add(sum, &product, sum) == ORIOR_EXACT_OK) ? 1 : 0;
}

// `first` times `second` less `third` times `fourth`, into `out`; 1, or 0 where the exact width could not represent it
static int hadamard_exact_difference(OriorExactInteger *out, const OriorExactInteger *first,
                                     const OriorExactInteger *second, const OriorExactInteger *third,
                                     const OriorExactInteger *fourth)
{
    OriorExactInteger product;
    OriorExactInteger taken;
    if ((orior_exact_multiply(first, second, &product) != ORIOR_EXACT_OK) ||
        (orior_exact_multiply(third, fourth, &taken) != ORIOR_EXACT_OK))
    {
        return 0;
    }
    return (orior_exact_subtract(&product, &taken, out) == ORIOR_EXACT_OK) ? 1 : 0;
}

// the three things a sweep query's leftover is read against, and the leftover, summed alone and in pairs
enum
{
    REGRESSION_COUNT,
    REGRESSION_SQUARE,
    REGRESSION_LEFT,
    REGRESSION_TERMS
};

HadamardLinks hadamard_links_read(unsigned int links, const long long *scaled, unsigned int passes, unsigned int seed,
                                  const unsigned long long *sweep_cost, unsigned int sweeps)
{
    // the constant, the count and the square are three unknowns, and the spread past them needs a fourth query
    if ((hadamard_order_fits(links) == 0) || (passes == 0u) || (sweeps < 4u))
    {
        return HADAMARD_LINKS_UNREAD;
    }
    // each sweep cost is from one pass, and the solve's costs are scaled by the passes and by links + 1
    const long long scale = (long long)passes * (long long)(links + 1u);

    OriorExactInteger sum[REGRESSION_TERMS];
    OriorExactInteger pair[REGRESSION_TERMS][REGRESSION_TERMS];
    for (unsigned int term = 0u; term < REGRESSION_TERMS; term += 1u)
    {
        orior_exact_zero(&sum[term]);
        for (unsigned int other = 0u; other < REGRESSION_TERMS; other += 1u)
        {
            orior_exact_zero(&pair[term][other]);
        }
    }

    int exact = 1;
    for (unsigned int row = 0u; row < sweeps; row += 1u)
    {
        unsigned char covered[HADAMARD_ORDER_LINKS_MOST];
        const unsigned int count = hadamard_sweep_covers(links, seed, row, covered);
        long long predicted = 0;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            predicted += (covered[link] != 0u) ? scaled[link] : 0;
        }
        // what the solve's links leave of this query's cost, at the solve's scale; a summed cost is under the
        // header's bound, which bounds the scaled cost inside a signed 64 bit word
        OriorExactInteger value[REGRESSION_TERMS];
        hadamard_exact(&value[REGRESSION_COUNT], (long long)count);
        hadamard_exact(&value[REGRESSION_SQUARE], (long long)count * (long long)count);
        hadamard_exact(&value[REGRESSION_LEFT], (scale * (long long)sweep_cost[row]) - predicted);
        for (unsigned int term = 0u; term < REGRESSION_TERMS; term += 1u)
        {
            exact = exact && (orior_exact_add(&sum[term], &value[term], &sum[term]) == ORIOR_EXACT_OK);
            for (unsigned int other = term; other < REGRESSION_TERMS; other += 1u)
            {
                exact = exact && hadamard_exact_add_product(&pair[term][other], &value[term], &value[other]);
            }
        }
    }
    if (exact == 0)
    {
        return HADAMARD_LINKS_UNREAD;
    }

    // Every sum centered and scaled by the query count N, which leaves it an integer
    OriorExactInteger sweep_count;
    OriorExactInteger centered[REGRESSION_TERMS][REGRESSION_TERMS];
    hadamard_exact(&sweep_count, (long long)sweeps);
    for (unsigned int term = 0u; term < REGRESSION_TERMS; term += 1u)
    {
        for (unsigned int other = term; other < REGRESSION_TERMS; other += 1u)
        {
            if (hadamard_exact_difference(&centered[term][other], &sweep_count, &pair[term][other], &sum[term],
                                          &sum[other]) == 0)
            {
                return HADAMARD_LINKS_UNREAD;
            }
        }
    }
    // a sweep whose queries all cover one count has nothing to read the square against
    if (centered[REGRESSION_COUNT][REGRESSION_COUNT].sign <= 0)
    {
        return HADAMARD_LINKS_UNREAD;
    }

    // The count taken out of the square and of the leftover, each scaled by the count's own spread, which leaves
    // every one an integer. The constant takes the overhead every query pays, and the count takes the share of it the
    // solve spreads over every link and therefore over every query in proportion to the count it covers
    OriorExactInteger square_square;
    OriorExactInteger square_left;
    OriorExactInteger left_left;
    const OriorExactInteger *const count_count = &centered[REGRESSION_COUNT][REGRESSION_COUNT];
    if ((hadamard_exact_difference(&square_square, count_count, &centered[REGRESSION_SQUARE][REGRESSION_SQUARE],
                                   &centered[REGRESSION_COUNT][REGRESSION_SQUARE],
                                   &centered[REGRESSION_COUNT][REGRESSION_SQUARE]) == 0) ||
        (hadamard_exact_difference(&square_left, count_count, &centered[REGRESSION_SQUARE][REGRESSION_LEFT],
                                   &centered[REGRESSION_COUNT][REGRESSION_SQUARE],
                                   &centered[REGRESSION_COUNT][REGRESSION_LEFT]) == 0) ||
        (hadamard_exact_difference(&left_left, count_count, &centered[REGRESSION_LEFT][REGRESSION_LEFT],
                                   &centered[REGRESSION_COUNT][REGRESSION_LEFT],
                                   &centered[REGRESSION_COUNT][REGRESSION_LEFT]) == 0))
    {
        return HADAMARD_LINKS_UNREAD;
    }
    // fewer than three counts covered leaves the square on the line the count draws, and nothing to read
    if (square_square.sign <= 0)
    {
        return HADAMARD_LINKS_UNREAD;
    }
    if (square_left.sign <= 0)
    {
        return HADAMARD_LINKS_ADD;
    }

    // With N - 3 left over, the square's slope clears twice its own spread exactly where
    // square_left^2 (N + 1) > 4 left_left square_square
    OriorExactInteger widened;
    OriorExactInteger four;
    OriorExactInteger cross_squared;
    OriorExactInteger cleared;
    OriorExactInteger spreads;
    OriorExactInteger bar;
    hadamard_exact(&widened, (long long)sweeps + 1);
    hadamard_exact(&four, 4);
    if ((orior_exact_multiply(&square_left, &square_left, &cross_squared) != ORIOR_EXACT_OK) ||
        (orior_exact_multiply(&cross_squared, &widened, &cleared) != ORIOR_EXACT_OK) ||
        (orior_exact_multiply(&left_left, &square_square, &spreads) != ORIOR_EXACT_OK) ||
        (orior_exact_multiply(&spreads, &four, &bar) != ORIOR_EXACT_OK))
    {
        return HADAMARD_LINKS_UNREAD;
    }
    return (orior_exact_compare(&cleared, &bar) > 0) ? HADAMARD_LINKS_CONTEND : HADAMARD_LINKS_ADD;
}
