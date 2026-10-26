// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef QUERY_PUT_H
#define QUERY_PUT_H

// One query of the query protocol: an address, a qualifier put to it, the cost of the answer, and a bit.
//
//     [ ADDRESS ] -> ( QUALIFIER ) -> [ MEASURED COST ] -> BINARY RESULT (1 or 0)
//
// Everything is learned this way and through nothing else. A query is made of the two relations host_entry.h defines,
// a word put at an address and a word read at one, and of nothing between them and the part: no library is called
// and no system is queried. The qualifier requests a state and never for a payload, and the answer to it is a bit.
//
// The cost is read off a clock, and a clock is an address too: a word that advances each time it is read is a
// counter, and the protocol finds one by querying the ADVANCES qualifier of an address. Nothing here assumes where one
// is. A query given no clock reads its answer and reports its cost unread.
//
// A query is put twice. Unbound, it returns the cost and no verdict. Bound, it returns 1 where the qualifier is true
// and the cost came in under the bound, and 0 otherwise; a qualifier that is not true and a cost past the bound both
// read 0, and the kind shows which.
//
// An address that refuses a read ends whatever read it. A query put to an address nothing has declared is safe is put
// from inside a cell, where an ending is the answer and the asker goes on.

#include "host_entry.h"

// the record a walk program hands the run each answered address's line in, as it is answered: a line is in the run's
// shared memory before the next query can end the process
#define QUERY_WALK_RECORD "walk"

// what a query requires of its address
typedef enum
{
    // given two words that share no bits, the address gives each back: it retains what it is given
    QUERY_RETAINS,
    // the word read at the address is the word the query carries
    QUERY_EQUALS,
    // a later read of the address, inside `turns` reads, gives a word past the first, read as unsigned: it counts
    QUERY_ADVANCES
} QueryQualifier;

// what came of a query
typedef enum
{
    // the qualifier was true, and where a bound was given, inside it
    QUERY_SATISFIED,
    // the qualifier was not true
    QUERY_UNSATISFIED,
    // the qualifier was true, and the cost came in past the bound
    QUERY_PAST_BOUND,
    // the process the query was put from ended before it answered, and the ending is the answer: `fault` reports how
    QUERY_ENDED
} QueryKind;

// one query, and what came back
typedef struct
{
    // where it is put
    unsigned long long address;
    // what is queried there, a QueryQualifier
    unsigned int qualifier;
    // the word QUERY_EQUALS compares against; unread by the other qualifiers
    unsigned int word;
    // the most reads QUERY_ADVANCES puts after its first before the address reads as not advancing, 0 or 1 for one;
    // unread by the other qualifiers
    unsigned long long turns;
    // the address of a counter found by QUERY_ADVANCES, or 0 for none: the cost is then unread
    unsigned long long clock;
    // 0 puts the query unbound and returns its cost; any other value is the most the answer may cost
    unsigned long long bound;

    // the bit: 1 where the qualifier was true inside the bound, 0 otherwise
    unsigned int bit;
    // what came of it, a QueryKind
    unsigned int kind;
    // the clock's advance across the query, in the clock's own counts; 0 where no clock was given
    unsigned long long cost;
    // 1 where a clock was given and read
    unsigned int cost_read;
    // the rule the ending named where the kind is QUERY_ENDED, a InterfaceFault; 0 otherwise
    unsigned int fault;
} Query;

// `query` put to its address. The bit, which is also written into `query`
unsigned int query_put(Query *query);

// 1 where a word read `then` after `first` has advanced: the difference taken in the word's own width is a forward
// step where it sits in the lower half of the word, and a counter that wraps between the reads still reads forward
unsigned int query_advanced(unsigned int first, unsigned int then);

#endif
