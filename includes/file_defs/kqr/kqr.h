// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr.h: the query record of a member, its .kqr (kqr.oracle.tsv), a k-file of records (krep_records.h). It holds:
//   - each query put to the member, and what came back.
//   - each timed query, as a sample of its own.
//   - the cycles, seeds and rounds that put them.
//   - then the paths read off those queries, each a record of numbers.
//
// It is R, the record (Q9, Q10, P13). Each program that queries the member stores one record of it through this. No
// program puts again a query that R stores.
//
// The record stores a query by its identity, seven numbers. That is the hash of its code, and the registers, threads,
// blocks and launches that it goes with. Then its count of cases and the hash of its cases.
//
// What came back is one of five codes. KQR_ANSWERS comes with the word of each case. The others come with the refusal,
// the one text that a query keeps.
//
// The program queries again a query that timed out, until an answer resolves it, and the answer takes its place. The
// same holds for a query that the gate censored, where the gate admits it later, because the part never saw it. The
// program puts a timed query each time, and stores it as a sample of its own.
//
// Each record of the file stays as it was in the file, with a pair after each other record. What a run adds comes after
// the old records and before the pairs. Each run of them comes after a who record, the who of the code that put them
// (Who).
//
// The writer writes a cycle, a seed and a round before the first new query or sample that follows them. Where none
// follows, it writes them nowhere. A cycle that the record answers whole then adds no record to it.
//
// Where a file holds a record of the text form of the .kqr, the read finds it. The writer writes it back as records,
// its lines in their order.
#ifndef KQR_H
#define KQR_H

#include "../../parsers/who.h"
// the .kqr's type, the four bytes after the k-file head
#define KQR_KIND "KQR\0"

// the name of the run's table whose rows a record a run stores is a view of
#define KQR_TABLE "queries"

// the codes of the .kqr's records, as its oracle gives them
#define KQR_MEMBER_RECORD 0x10u
#define KQR_CYCLE_RECORD 0x11u
#define KQR_SEED_RECORD 0x12u
#define KQR_ROUND_RECORD 0x13u
#define KQR_QUERY_RECORD 0x14u
#define KQR_SAMPLE_RECORD 0x15u
#define KQR_PAIR_RECORD 0x16u
#define KQR_COMMENT_RECORD 0x17u

// what came back of a query
#define KQR_ANSWERS 0u
#define KQR_ILLEGAL 1u
#define KQR_NOTHING 2u
#define KQR_TIMED_OUT 3u
#define KQR_CENSORED 4u
#define KQR_ANSWER_COUNT 5u

// the most characters of a refusal, of the name of a member, of a mode, of the form of a pair and of its verdict. Then
// the most words of an answer
#define KQR_REFUSAL 128u
#define KQR_MEMBER 64u
#define KQR_MODE 64u
#define KQR_FORM 128u
#define KQR_VERDICT 512u
#define KQR_WORDS 4096u

// the max marks a cycle retains pending before its first new query or sample
#define KQR_PENDING 8u

// the identity of a query: the hash of its code, and the registers, threads, blocks and launches that it goes with.
// Then its count of cases and the hash of its cases
typedef struct
{
    unsigned long long code;
    unsigned int registers;
    unsigned int threads;
    unsigned int blocks;
    unsigned int launches;
    unsigned int cases;
    unsigned long long cases_hash;
} KqrIdentity;

// one untimed query and what came back: its identity; its answer, one of the five codes. And the word of every case
// where it answered, or the refusal where it did not
typedef struct
{
    KqrIdentity identity;
    unsigned int answer;
    unsigned long long word[KQR_WORDS];
    unsigned int words;
    char refusal[KQR_REFUSAL];
} KqrQuery;

// one timed query, a sample of its own. It holds its identity, as a query holds it, and what came back. Then the
// nanoseconds that its launches took together where it answered, or the refusal where it did not.
// Where the sample gives them, it holds the least and the max time of one launch, and its block latency (P6), with
// `launched` 1
typedef struct
{
    KqrIdentity identity;
    unsigned int answer;
    unsigned long long nanoseconds;
    int launched;
    unsigned long long least;
    unsigned long long max;
    char refusal[KQR_REFUSAL];
} KqrSample;

// a path read off the queries. It holds a pair of the bridge, and the hash of what its put reads where `read` is 1.
// Then its verdict as the bridge writes it beneath the pair
typedef struct
{
    char first[KQR_FORM];
    char second[KQR_FORM];
    int read;
    unsigned long long reads;
    char verdict[KQR_VERDICT];
} KqrPath;

#endif
