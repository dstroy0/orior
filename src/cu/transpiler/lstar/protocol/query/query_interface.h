// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef QUERY_INTERFACE_H
#define QUERY_INTERFACE_H

// Queries put from inside the interface, at addresses that nothing declared safe.
//
// An address that refuses a read or a put ends the process that sent it. From inside one process, that end is the last
// thing that the process learns. The interface runs the queries in a probe that it can lose. A walk of addresses goes
// to `query_walk`, which answers each address in turn until one ends it, and the end is the answer of that address. The
// interface puts the walk again from the next address, in a fresh probe, until every address has an answer.
//
// The program asks a system nothing about what lies at an address. It queries the address, and the address answers,
// keeps, reads as a word, advances or ends the asker.

#include "../../interface/interface.h"
#include "query_put.h"

// the max addresses that one probe gets, which bounds the lines that it writes back
#define QUERY_INTERFACE_MAX 1024ULL

// one walk: the program that it runs as, the qualifier put at every address, and the addresses. Then how long a probe
// gets before the interface ends it
typedef struct
{
    // the built query_walk, which passes its lines to the run this process is a member of (QUERY_WALK_RECORD)
    const char *program;
    // a QueryQualifier, put at every address
    unsigned int qualifier;
    // the first address, how many, and the bytes between two
    unsigned long long from;
    unsigned long long count;
    unsigned long long stride;
    // how many reads ADVANCES puts at one address after its first, before the address reads as one that does not
    // advance
    unsigned long long turns;
    // the word QUERY_EQUALS compares against
    unsigned int word;
    // the max time that one probe gets, 0 for no limit
    unsigned long long limit_microseconds;
} QueryWalk;

// What one program that the interface walks reads and answers. `takes_qualifier` is 1 where the program takes the
// qualifier before the span, and the turns and word after it, as query_walk does. It is 0 where the program takes the
// span alone. `read` reads the lines of a probe into `answers` from address `next` on, and gives how many it answered
// in order. `ended` answers address `next`, the one that ended its probe with `fault`
typedef struct
{
    int takes_qualifier;
    unsigned long long (*read)(const QueryWalk *walk, const char *lines, unsigned long long next,
                               unsigned long long given, void *answers);
    void (*ended)(const QueryWalk *walk, unsigned long long next, InterfaceFault fault, void *answers);
    void *answers;
} QueryInterfaceDriver;

// `walk` put through the interface in probes it can lose, each probe's lines read and its ending answered by `driver`,
// until every address has an answer. The count of probes through `probes`. 1, or 0 with the error raised where the
// interface itself failed or a probe that exited clean wrote something other than its lines
int query_interface_run(const QueryWalk *walk, const QueryInterfaceDriver *driver, unsigned long long *probes,
                        EngineError *error);

// `walk`, put through the interface, with one answer for each address into `answers`, which has room for
// `walk->count` of them. Every answer carries its address, its bit and its type. An address that ended its probe reads
// QUERY_ENDED with the fault that the interface named. One whose probe ran out of time, or exited by itself, reads
// QUERY_ENDED with no fault. The return is the count of probes that the walk ran. It is 0, with the error raised,
// where the interface itself failed, or a probe wrote a thing other than its lines
unsigned long long query_interface_walk(const QueryWalk *walk, Query *answers, EngineError *error);

#endif
