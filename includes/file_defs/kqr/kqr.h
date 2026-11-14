// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr.h: a member's query record, its .kqr (kqr.oracle.tsv), a k-file of records (krep_records.h): every query put to
// the member and what came back, every timed query a sample of its own, the cycles, seeds and rounds that put them, and
// then the paths read off those queries, each a record of numbers. It is R, the record (Q9, Q10, P13), and every
// program that queries the member stores one record of it through this: a query R stores is not put again.
//
// A query is stored by its identity, seven numbers: the hash of its code, the registers, threads, blocks and launches
// it is put with, its count of cases and the hash of its cases. What came back is one of five codes, KQR_ANSWERS with
// the word of every case, and the others with the refusal, the one text a query retains. A query that timed out is
// queried again until an answer resolves it, and the answer takes its place; so does a query the gate censored, where
// the gate admits it later, since the part never saw it. A timed query is put every time and stored as a sample of its
// own.
//
// Every record the file contained stands as it contained it, a pair after every other record. What a run adds comes
// after the records it contained and before the pairs, each run of them after a who record, the who of the code that
// put them (Who); and a cycle, a seed and a round are each written before the first new query or sample that follows
// them and nowhere where none follows, so that a cycle the record answers whole adds no record to it. A record of the
// .kqr's text form, read where a file contains it, is written back as records, its lines in their order.
#ifndef KQR_H
#define KQR_H

#include "../../parsers/who.h"
// the .kqr's kind, the four bytes after the k-file head
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

// the most letters of a refusal, of a member's name, of a mode, of a pair's form and of its verdict, and the most
// words of an answer
#define KQR_REFUSAL 128u
#define KQR_MEMBER 64u
#define KQR_MODE 64u
#define KQR_FORM 128u
#define KQR_VERDICT 512u
#define KQR_WORDS 4096u

// the most marks a cycle retains pending before its first new query or sample
#define KQR_PENDING 8u

// a query's identity: the hash of its code, the registers, threads, blocks and launches it is put with, its count of
// cases and the hash of its cases
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

// one untimed query and what came back: its identity; its answer, one of the five codes; and the word of every case
// where it answered, or the refusal where it did not
typedef struct
{
    KqrIdentity identity;
    unsigned int answer;
    unsigned long long word[KQR_WORDS];
    unsigned int words;
    char refusal[KQR_REFUSAL];
} KqrQuery;

// one timed query, a sample of its own: its identity as a query's, what came back, and the nanoseconds its launches
// took together where it answered, or the refusal where it did not; and where the sample gives them, the least and
// the most one launch took, its block latency (P6), `launched` 1
typedef struct
{
    KqrIdentity identity;
    unsigned int answer;
    unsigned long long nanoseconds;
    int launched;
    unsigned long long least;
    unsigned long long most;
    char refusal[KQR_REFUSAL];
} KqrSample;

// a path read off the queries: a pair of the bridge, the hash of what its put reads where `read` is 1, and its
// verdict as the bridge writes it beneath the pair
typedef struct
{
    char first[KQR_FORM];
    char second[KQR_FORM];
    int read;
    unsigned long long reads;
    char verdict[KQR_VERDICT];
} KqrPath;

#endif
