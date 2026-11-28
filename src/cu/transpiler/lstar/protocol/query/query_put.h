// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef QUERY_PUT_H
#define QUERY_PUT_H

// One query of the query protocol: an address, a qualifier put to it, the cost of the answer, and a bit.
//
//     [ ADDRESS ] -> ( QUALIFIER ) -> [ MEASURED COST ] -> BINARY RESULT (1 or 0)
//
// The program learns all things this way, and through nothing else. A query has the two relations that host_entry.h
// defines, a word put at an address and a word read at one. Nothing is between them and the part: the query calls no
// library and asks no system. The qualifier asks for a state and never for a payload, and the answer to it is a bit.
//
// The query reads the cost from a clock, and a clock is an address too. A word that advances at each read is a
// counter, and the protocol finds one with a query of the ADVANCES qualifier of an address. Nothing here assumes where
// one is. A query with no clock reads its answer, and reports its cost as not read.
//
// The program puts a query twice. With no bound, it returns the cost and no verdict. With a bound, it returns 1 where
// the qualifier is true and the cost came in under the bound, and 0 in all other cases. A qualifier that is not true
// and a cost past the bound both read 0, and the type shows which.
//
// An address that refuses a read ends the reader. The program puts a query to an address that nothing declared safe
// from inside a cell. There, an ending is the answer, and the asker goes on.

#include "host_entry.h"

// the record in which a walk program passes the run the line of each answered address, at its answer. A line is in the
// shared memory of the run before the next query can end the process
#define QUERY_WALK_RECORD "walk"

// what a query requires of its address
typedef enum
{
    // given two words that share no bits, the address gives each back: it keeps what it gets
    QUERY_RETAINS,
    // the word read at the address is the word the query carries
    QUERY_EQUALS,
    // a later read of the address, inside `turns` reads, gives a word past the first, read as unsigned: it counts
    QUERY_ADVANCES
} QueryQualifier;

// what came of a query
typedef enum
{
    // the qualifier was true, and inside the bound where the query has one
    QUERY_SATISFIED,
    // the qualifier was not true
    QUERY_UNSATISFIED,
    // the qualifier was true, and the cost came in past the bound
    QUERY_PAST_BOUND,
    // the process that put the query ended before it answered, and the ending is the answer. `fault` reports how
    QUERY_ENDED
} QueryType;

// one query, and what came back
typedef struct
{
    // the address that the query goes to
    unsigned long long address;
    // what the query asks there, a QueryQualifier
    unsigned int qualifier;
    // the word QUERY_EQUALS compares against; unread by the other qualifiers
    unsigned int word;
    // the max reads that QUERY_ADVANCES puts after its first, before the address reads as one that does not advance.
    // 0 or 1 is for one. The other qualifiers do not read it
    unsigned long long turns;
    // the address of a counter found by QUERY_ADVANCES, or 0 for none: the cost is then unread
    unsigned long long clock;
    // 0 puts the query unbound and returns its cost; any other value is the max the answer may cost
    unsigned long long bound;

    // the bit: 1 where the qualifier was true inside the bound, 0 otherwise
    unsigned int bit;
    // what came of it, a QueryType
    unsigned int type;
    // the advance of the clock across the query, in the own counts of the clock, or 0 where the query has no clock
    unsigned long long cost;
    // 1 where the query has a clock and read it
    unsigned int cost_read;
    // the rule that the ending named where QUERY_ENDED is the type, an InterfaceFault, and 0 in all other cases
    unsigned int fault;
} Query;

// `query` put to its address. The return is the bit, which the call also writes into `query`
unsigned int query_put(Query *query);

// 1 where a word read `then` after `first` advanced. The difference, in the own width of the word, is a forward step
// where it sits in the lower half of the word. A counter that wraps between the reads still reads forward
unsigned int query_advanced(unsigned int first, unsigned int then);

#endif
