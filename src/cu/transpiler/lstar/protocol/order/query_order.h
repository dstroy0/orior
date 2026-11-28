// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef QUERY_ORDER_H
#define QUERY_ORDER_H

// The known order of queries put through the query protocol: links that are queries, and costs read from a clock that
// is an address. The program solves the cost of every link from the answers of the order.
//
// A link is one query, an address and a qualifier. Query r of the known order puts every link that it covers, `repeat`
// times over, between two reads of the clock. Its cost is the advance of the clock across them. A clock that steps far
// less often than a query takes reads a single query as a step or nothing. Thus `repeat` makes one query of the order
// last many steps.
//
// The program puts the queries of the order in turn, `passes` times over. Thus a drift in the speed of the part falls
// on every query alike. It keeps the answers of every pass, and hadamard_order_solve gives the cost of every link pass
// by pass. The spread across passes is the noise floor, read from the answers. The call hadamard_links_read finds
// whether the links add or contend. Nothing here stores a floating point value.

#include "../query/query_put.h"
#include "hadamard_order.h"

#include "../../../../../../includes/exact_types/integers/exact_integer.h"

// A clock is any thing that turns over, noisily, now and then. Its turns frame a run, and the program reads the run
// finely by a count of reads of the clock between turns. The cost of a run is the exact rational
// `numerator / denominator` in the own count of the clock word, and nothing divides it
typedef struct
{
    OriorExactInteger numerator;
    OriorExactInteger denominator;
} QueryCost;

// a known order of queries over links that are queries
typedef struct
{
    // the links, `links` of them, a count that a known order supports. The program puts each as it is, and does not
    // read its bound
    const Query *link;
    unsigned int links;
    // the address of a counter found by QUERY_ADVANCES, read before and after every query of the order
    unsigned long long clock;
    // the max reads that one wait for the turn of the clock can take, as QUERY_ADVANCES takes its `turns`. A clock that
    // does not turn inside them refuses the order
    unsigned long long turns;
    // how many times the program puts each covered link inside one query of the order
    unsigned long long repeat;
    // how many times the program puts the whole order
    unsigned int passes;
} QueryOrder;

// The order, put, with the cost of every query in every pass into `cost`, pass by pass: `cost[pass * links + row]`. It
// has room for `order->passes` times `order->links` of them. The return is 1, or 0 where the link count has no known
// order, the caller gave no clock, or the clock did not turn inside `order->turns` reads
int query_order_put(const QueryOrder *order, QueryCost *cost);

// The cost of every link from one pass of the costs of the order, exactly: link c costs `numerator[c] / denominator`.
// The denominator is one for every link of the pass, and `(links + 1)` times the denominators of every query
// multiplied together. Two links of one pass compare by their numerators alone. The return is 1, or 0 where the link
// count has no known order, or the exact width could not hold a term
int query_order_solve(unsigned int links, const QueryCost *cost, OriorExactInteger *numerator,
                      OriorExactInteger *denominator);

// The sweep queries from `seed` put once each, `sweeps` of them, their costs into `sweep_cost`. 1, or 0 as above
int query_order_sweep(const QueryOrder *order, unsigned int seed, unsigned int sweeps, QueryCost *sweep_cost);

#endif
