// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef HADAMARD_ORDER_H
#define HADAMARD_ORDER_H

// The order of queries that slices a chain, and what its answers show.
//
// The cost of a chain is one number, and names no link. The known order queries about half the links at a time, and
// any two queries overlap on a quarter. Every answer then carries every link: one query informs every link at once.
// Query r covers link c where `(r + 1) & (c + 1)` has an odd count of ones. These are the rows of a Hadamard matrix of
// order `links + 1`, with its first row and column dropped.
//
// The program draws nothing and stores nothing. The order follows from the link count, and a record of it is that
// count.
//
// A known order supports a link count one short of a power of two. Every query then covers the same count of links.
// Contention that grows with how many links a query covers moves the cost of every link by the same amount. It leaves
// their order as it was.
//
// A count between two of those has no known order here, and the program refuses it. A cover that leaves links out of a
// larger order would cover a different count in each query, and give that up. The engine chooses how to compose a
// chain, and it composes one of a length that a known order supports.
//
// The program stores no floating point value. The solve is integer adds, and the contention read is exact integers. A
// cost is a count of the own clock of the part.

// the max links one known order covers
#define HADAMARD_ORDER_LINKS_MAX 63u

// 1 where a known order covers `links`: one short of a power of two, from 3 to HADAMARD_ORDER_LINKS_MAX
int hadamard_order_fits(unsigned int links);

// 1 where query `row` of the known order over `links` links covers link `link`. The order contains `links` queries
int hadamard_order_covers(unsigned int links, unsigned int row, unsigned int link);

// The cost of every link, from one cost for each query of the known order: `cost[row]`, summed over every pass put.
// It goes into `scaled[link]` as `(links + 1)` times the summed cost: `scaled = 4 * S'cost - 2 * (sum of cost)`. A
// division by `links + 1` would be a shift, and the caller that wants ticks does it. A comparison of two links needs
// none. The return is 1, or 0 where `links` has no known order.
//
// The arithmetic is 64 bit. A summed cost past `2^55 / (4 * links)` can overflow it, and nothing checks for that
int hadamard_order_solve(unsigned int links, const unsigned long long *cost, long long *scaled);

// the links that sweep query `row` covers, from `seed`, into `covered`, one byte for each link. The return is the count
// covered. The queries cover one link, `(links + 1) / 2` links and every link, in turn. Each is a prefix of an order of
// the links drawn from `seed` and `row`. Those three counts are the two ends and the middle. There, a read of the
// square of the count against the count has the least spread
unsigned int hadamard_sweep_covers(unsigned int links, unsigned int seed, unsigned int row, unsigned char *covered);

// what the sweep's answers show about the links
typedef enum
{
    // the cost of a set of links read as the sum of its links, within what the noise allows
    HADAMARD_LINKS_ADD,
    // the cost rises with the square of how many links an query covers, past twice its own spread: the links contend
    HADAMARD_LINKS_CONTEND,
    // nothing to read: fewer than four sweep queries, or fewer than three counts covered among them
    HADAMARD_LINKS_UNREAD
} HadamardLinks;

// Whether the links contend, read from `sweeps` sweep queries from `seed` and their costs `sweep_cost`, each from one
// pass. The call reads them against `scaled`, as hadamard_order_solve gave it from `passes` passes of the known order.
//
// The call takes the links of the solve out of each sweep cost. It reads the rest against a constant, the count that
// the query covered, and the square of that count. It reads contention where the slope of the square clears twice its
// own spread.
//
// The constant takes the overhead that every query pays. The count takes the share of that overhead that the solve
// spreads evenly over every link. That share reaches a sweep query in proportion to the count that it covers. Without
// the count, the call would read it as part of the square. Every sum is an exact integer
HadamardLinks hadamard_links_read(unsigned int links, const long long *scaled, unsigned int passes, unsigned int seed,
                                  const unsigned long long *sweep_cost, unsigned int sweeps);

#endif
