// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef QUERY_ORDER_H
#define QUERY_ORDER_H

// The known order of queries put through the query protocol: links that are queries, costs read off a clock that is an
// address, and every link's cost solved from the order's answers.
//
// A link is one query, an address and a qualifier. Query r of the known order puts every link it covers, `repeat` times
// over, between two reads of the clock, and its cost is the clock's advance across them. A clock that steps far less
// often than a query takes reads a single query as a step or nothing, and `repeat` makes one query of the order
// last many steps. The order's queries are put in turn, `passes` times over: a drift in the part's speed falls on
// every query alike. Every pass's answers are retained, hadamard_order_solve gives every link's cost pass by pass, and
// the spread across passes is the noise floor, read off the answers. hadamard_links_read determines whether the links
// add or contend. Nothing here stores a floating point value.

#include "../query/query_put.h"
#include "hadamard_order.h"

#include "../../../../../../includes/exact_types/integers/exact_integer.h"

// A clock is anything that turns over, noisily, now and then: its turns frame a run, and the run is read finely by
// counting reads of the clock between turns. A run's cost is the exact rational numerator / denominator in the clock
// word's own count, and nothing divides it
typedef struct
{
    OriorExactInteger numerator;
    OriorExactInteger denominator;
} QueryCost;

// a known order of queries over links that are queries
typedef struct
{
    // the links, `links` of them, a count a known order supports; each is put as it is, its bound left unread
    const Query *link;
    unsigned int links;
    // the address of a counter found by QUERY_ADVANCES, read before and after every query of the order
    unsigned long long clock;
    // the max reads one wait for the clock's turn may take, as QUERY_ADVANCES takes its `turns`: a clock that does
    // not turn inside them refuses the order
    unsigned long long turns;
    // how many times each covered link is put inside one query of the order
    unsigned long long repeat;
    // how many times the whole order is put
    unsigned int passes;
} QueryOrder;

// The order put, every pass's cost of every query into `cost`, pass by pass: cost[pass * links + row], which has room
// for `order->passes` times `order->links` of them. 1, or 0 where the link count has no known order, no clock was
// given, or the clock did not turn inside `order->turns` reads
int query_order_put(const QueryOrder *order, QueryCost *cost);

// Every link's cost from one pass of the order's costs, exactly: link c costs numerator[c] / denominator, the
// denominator one for every link of the pass and (links + 1) times every query's denominator multiplied together.
// Two links of one pass compare by their numerators alone. 1, or 0 where the link count has no known order or the
// exact width could not represent a term
int query_order_solve(unsigned int links, const QueryCost *cost, OriorExactInteger *numerator,
                      OriorExactInteger *denominator);

// The sweep queries from `seed` put once each, `sweeps` of them, their costs into `sweep_cost`. 1, or 0 as above
int query_order_sweep(const QueryOrder *order, unsigned int seed, unsigned int sweeps, QueryCost *sweep_cost);

#endif
