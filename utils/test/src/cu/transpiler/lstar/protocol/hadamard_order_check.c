// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hadamard_order_check.c: the known order of queries, checked against its exact claims at every size it covers
//
// The exact claims are checked over the whole of every set they are about, and the count is printed:
//
//   inverse     4 S' - 2 J times S is (links + 1) times the identity, entry by entry
//   coverage    every query covers (links + 1) / 2 links and any two share (links + 1) / 4
//   solve       costs built from integer link costs solve back to (links + 1) times each link, at any pass count
//   overhead    a fixed cost on every query moves every link by the same amount
//   contention  a cost that grows with the square of the count a query covers does the same
//   refusal     a link count no known order covers is refused, and the solve writes nothing
//   sweep       the sweep's queries cover one link, half plus one and every link, in turn
//
// The contention read is a test on noisy costs and is measured and not proved: costs drawn in integers around an
// overhead, pairs of links contending by an amount drawn per pair, and the rate the read finds contention at each
// amount, the rate at none being its false alarms.
#include "../../../../../../../src/cu/transpiler/lstar/protocol/order/hadamard_order.h"

#include <stdio.h>
#include <string.h>

static const unsigned int s_sizes[] = {3u, 7u, 15u, 31u, 63u};
#define SIZE_COUNT (sizeof(s_sizes) / sizeof(s_sizes[0]))

static unsigned long long s_checks = 0ull;
static unsigned long long s_failed = 0ull;

static void check_that(int passed, const char *what, unsigned int links)
{
    s_checks += 1ull;
    if (passed == 0)
    {
        s_failed += 1ull;
        printf("  FAILED: %s at %u links\n", what, links);
    }
}

// the test's own generator, never zero
static unsigned int check_drawn(unsigned int *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

// a draw near a bell curve in integers: the sum of twelve draws in [-500, 500], whose spread is 1000
static long long check_noise(unsigned int *state)
{
    long long sum = 0;
    for (unsigned int at = 0u; at < 12u; at += 1u)
    {
        sum += (long long)(check_drawn(state) % 1001u) - 500;
    }
    return sum;
}

static void check_inverse_and_coverage(unsigned int links)
{
    int inverse = 1;
    for (unsigned int row = 0u; row < links; row += 1u)
    {
        for (unsigned int column = 0u; column < links; column += 1u)
        {
            long long sum = 0;
            for (unsigned int design_row = 0u; design_row < links; design_row += 1u)
            {
                const long long term = 2 * ((2 * (long long)hadamard_order_covers(links, design_row, row)) - 1);
                sum += term * (long long)hadamard_order_covers(links, design_row, column);
            }
            inverse = inverse && (sum == ((row == column) ? (long long)(links + 1u) : 0));
        }
    }
    check_that(inverse, "4 S' - 2 J times S is (links + 1) times the identity", links);

    int halves = 1;
    int quarters = 1;
    for (unsigned int design_row = 0u; design_row < links; design_row += 1u)
    {
        unsigned int covered = 0u;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            covered += (unsigned int)hadamard_order_covers(links, design_row, link);
        }
        halves = halves && (covered == ((links + 1u) / 2u));
        for (unsigned int other = design_row + 1u; other < links; other += 1u)
        {
            unsigned int shared = 0u;
            for (unsigned int link = 0u; link < links; link += 1u)
            {
                shared += (unsigned int)(hadamard_order_covers(links, design_row, link) &
                                         hadamard_order_covers(links, other, link));
            }
            quarters = quarters && (shared == ((links + 1u) / 4u));
        }
    }
    check_that(halves, "every query covers (links + 1) / 2 links", links);
    check_that(quarters, "any two queries share (links + 1) / 4 links", links);
}

// the costs of the known order's queries for integer link costs `cost`, each with `fixed` added and `per_square` times
// the square of the count it covers, summed over `passes`
static void check_row_costs(unsigned int links, const unsigned long long *cost, unsigned long long fixed,
                            unsigned long long per_square, unsigned int passes, unsigned long long *row_cost)
{
    for (unsigned int row = 0u; row < links; row += 1u)
    {
        unsigned long long sum = 0ull;
        unsigned long long count = 0ull;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            const int covers = hadamard_order_covers(links, row, link);
            sum += (covers != 0) ? cost[link] : 0ull;
            count += (covers != 0) ? 1ull : 0ull;
        }
        row_cost[row] = (unsigned long long)passes * (sum + fixed + (per_square * count * count));
    }
}

static void check_solve(unsigned int links, unsigned int *state)
{
    unsigned long long cost[HADAMARD_ORDER_LINKS_MOST];
    unsigned long long row_cost[HADAMARD_ORDER_LINKS_MOST];
    long long scaled[HADAMARD_ORDER_LINKS_MOST];
    long long shifted[HADAMARD_ORDER_LINKS_MOST];
    for (unsigned int trial = 0u; trial < 64u; trial += 1u)
    {
        const unsigned int passes = 1u + (trial % 8u);
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            cost[link] = 1ull + (check_drawn(state) % 1000000u);
        }
        check_row_costs(links, cost, 0ull, 0ull, passes, row_cost);
        int exact = hadamard_order_solve(links, row_cost, scaled);
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            exact = exact && (scaled[link] == ((long long)passes * (long long)(links + 1u) * (long long)cost[link]));
        }
        check_that(exact, "integer link costs solve back exactly", links);

        const unsigned long long fixed = check_drawn(state) % 100000u;
        check_row_costs(links, cost, fixed, 0ull, passes, row_cost);
        hadamard_order_solve(links, row_cost, shifted);
        int even = 1;
        for (unsigned int link = 1u; link < links; link += 1u)
        {
            even = even && ((shifted[link] - scaled[link]) == (shifted[0] - scaled[0]));
        }
        check_that(even, "a fixed cost on every query moves every link by one amount", links);

        const unsigned long long per_square = 1ull + (check_drawn(state) % 1000u);
        check_row_costs(links, cost, 0ull, per_square, passes, row_cost);
        hadamard_order_solve(links, row_cost, shifted);
        even = 1;
        for (unsigned int link = 1u; link < links; link += 1u)
        {
            even = even && ((shifted[link] - scaled[link]) == (shifted[0] - scaled[0]));
        }
        check_that(even, "contention on the count covered moves every link by one amount", links);
    }
}

static void check_sweep(unsigned int links)
{
    unsigned char covered[HADAMARD_ORDER_LINKS_MOST];
    const unsigned int counts[3] = {1u, (links + 1u) / 2u, links};
    int every = 1;
    for (unsigned int row = 0u; row < (3u * links); row += 1u)
    {
        const unsigned int count = hadamard_sweep_covers(links, 0x51a7u, row, covered);
        unsigned int marked = 0u;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            marked += covered[link];
        }
        every = every && (count == counts[row % 3u]) && (marked == count);
    }
    check_that(every, "the sweep covers one link, half plus one and every link, in turn", links);
}

static void check_refusals(void)
{
    unsigned long long row_cost[128];
    long long scaled[128];
    memset(row_cost, 0, sizeof(row_cost));
    for (unsigned int links = 0u; links < 128u; links += 1u)
    {
        const int covered = ((links == 3u) || (links == 7u) || (links == 15u) || (links == 31u) || (links == 63u));
        check_that(hadamard_order_fits(links) == covered, "a known order exists exactly where it should", links);
        if (covered == 0)
        {
            scaled[0] = 12345;
            check_that((hadamard_order_solve(links, row_cost, scaled) == 0) && (scaled[0] == 12345),
                       "a refused count writes nothing", links);
        }
    }
}

// the share, in tenths of a percent, of `trials` passes the read finds contention on, links contending by up to
// `most` per pair, over `links` links, `passes` passes of the known order and `sweeps` sweep queries
static unsigned int check_contention_rate(unsigned int links, unsigned int passes, unsigned int sweeps,
                                          unsigned int most, unsigned int trials, unsigned int *state)
{
    static unsigned int contend[HADAMARD_ORDER_LINKS_MOST][HADAMARD_ORDER_LINKS_MOST];
    unsigned long long cost[HADAMARD_ORDER_LINKS_MOST];
    unsigned long long row_cost[HADAMARD_ORDER_LINKS_MOST];
    unsigned long long swept[4u * HADAMARD_ORDER_LINKS_MOST];
    long long scaled[HADAMARD_ORDER_LINKS_MOST];
    unsigned char covered[HADAMARD_ORDER_LINKS_MOST];
    const long long overhead = 20000;
    unsigned int found = 0u;
    for (unsigned int trial = 0u; trial < trials; trial += 1u)
    {
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            cost[link] = 500ull + (check_drawn(state) % 1001u);
            for (unsigned int other = link + 1u; other < links; other += 1u)
            {
                contend[link][other] = (most == 0u) ? 0u : (check_drawn(state) % (most + 1u));
            }
        }
        for (unsigned int row = 0u; row < links; row += 1u)
        {
            long long sum = 0;
            for (unsigned int link = 0u; link < links; link += 1u)
            {
                covered[link] = (unsigned char)hadamard_order_covers(links, row, link);
            }
            for (unsigned int pass = 0u; pass < passes; pass += 1u)
            {
                long long one = overhead + check_noise(state);
                for (unsigned int link = 0u; link < links; link += 1u)
                {
                    one += (covered[link] != 0u) ? (long long)cost[link] : 0;
                    for (unsigned int other = link + 1u; other < links; other += 1u)
                    {
                        one += ((covered[link] != 0u) && (covered[other] != 0u)) ? (long long)contend[link][other] : 0;
                    }
                }
                sum += one;
            }
            // the overhead lifts every pass far above the noise, and a pass is never below zero
            row_cost[row] = (unsigned long long)sum;
        }
        hadamard_order_solve(links, row_cost, scaled);
        const unsigned int seed = 0x51a7u + trial;
        for (unsigned int row = 0u; row < sweeps; row += 1u)
        {
            hadamard_sweep_covers(links, seed, row, covered);
            long long one = overhead + check_noise(state);
            for (unsigned int link = 0u; link < links; link += 1u)
            {
                one += (covered[link] != 0u) ? (long long)cost[link] : 0;
                for (unsigned int other = link + 1u; other < links; other += 1u)
                {
                    one += ((covered[link] != 0u) && (covered[other] != 0u)) ? (long long)contend[link][other] : 0;
                }
            }
            swept[row] = (unsigned long long)one;
        }
        found += (hadamard_links_read(links, scaled, passes, seed, swept, sweeps) == HADAMARD_LINKS_CONTEND) ? 1u : 0u;
    }
    return (1000u * found) / trials;
}

int main(void)
{
    unsigned int state = 0x9e3779b9u;
    for (unsigned int at = 0u; at < SIZE_COUNT; at += 1u)
    {
        check_inverse_and_coverage(s_sizes[at]);
        check_solve(s_sizes[at], &state);
        check_sweep(s_sizes[at]);
    }
    check_refusals();
    printf("  exact claims at 3, 7, 15, 31 and 63 links: %llu checks, %llu failed\n", s_checks, s_failed);

    // the noise's spread is 1000, the floor; links cost 500 to 1500, under it, and a pair contends by up to `most`
    const unsigned int links = 15u;
    const unsigned int passes = 4u;
    const unsigned int sweeps = 3u * links;
    const unsigned int trials = 300u;
    static const unsigned int s_most[] = {0u, 80u, 200u};
    unsigned int rate[3];
    printf("  contention read, %u links, %u passes of the known order, %u sweep queries, %u trials:\n", links, passes,
           sweeps, trials);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        rate[at] = check_contention_rate(links, passes, sweeps, s_most[at], trials, &state);
        printf("    up to %3u a pair: contention found on %u.%u%%\n", s_most[at], rate[at] / 10u, rate[at] % 10u);
    }
    // the read is required to have a false alarm rate under one in ten and to find the largest contention nine times in
    // ten
    const int read_stands = (rate[0] < 100u) && (rate[2] >= 900u);
    printf("  the read's false alarms stay under 10%% and it finds the largest contention on 90%% or more: %s\n",
           read_stands ? "yes" : "NO");
    return ((s_failed == 0ull) && read_stands) ? 0 : 1;
}
