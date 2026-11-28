// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef ANSWER_READ_H
#define ANSWER_READ_H

// What the part answered of a query, read as P1 reads it (P11, the query).
//
// The program puts a form f on a case k, beside the answer of the host, h(k): `f(k) = h(k) ?`. The part gives one of
// four answers:
//
// - Alike, where `f(k) = h(k)`.
// - Apart, where it gives another word.
// - Refused, where it ends the run: an illegal instruction, an illegal address, or a launch out of resources.
// - Nothing, where no answer comes back.
//
// Alike is truthy, and the other three are falsy.
//
// The answer of the host to a case is a bracket at the width of the result, `[down, up]` (Collapse 1). An integer case
// is a bracket of one word, `down = up`. A floating case is a bracket of two ends, and the part is alike where it
// answers either end. A case on which the host traps, or leaves the result undefined, carries no bracket. The asker
// keeps the answer of the part there, and it gates nothing.
//
// Gray is the uncertainty: no answer of the part decides the case. The reader reads it where the host gives no bracket.
// It also reads it where no answer of the part came to read at all. That is a query that the gate blocked, or that
// nothing carried. Gray is never 0 and never 1 (P1, P10).

#include "../teacher/run_channel.h"

// what one case of a query reads
typedef enum
{
    ANSWER_ALIKE,
    ANSWER_APART,
    ANSWER_REFUSED,
    ANSWER_NOTHING,
    ANSWER_GRAY
} AnswerRead;

// the host's answer to one case: `bracketed` 0 where the host gives none, and otherwise the two ends, down = up for
// a case of one word
typedef struct
{
    int bracketed;
    unsigned long long down;
    unsigned long long up;
} AnswerBracket;

// What a query over many cases reads: the read of the whole, and the count of cases alike. Then the reading that a
// query alike on every case carries (P10), and the place of the first case apart, `cases` where none is
typedef struct
{
    AnswerRead read;
    unsigned int alike;
    unsigned int first_apart;
} AnswerTally;

// One case read: the bracket `host` of the host, and the word `answered` of the part where RUN_ANSWERED is the
// outcome `outcome` (RunOutcome) of the run
AnswerRead answer_read(const AnswerBracket *host, unsigned int outcome, unsigned long long answered);

// 1 where `read` is truthy: alike, 0 for every other reading
int answer_truthy(AnswerRead read);

// A query over `cases` cases, read. It has the bracket of the host for each in `host`, the outcome `outcome` of the
// run, and the word of the part on each in `answered`. The read is:
//
// - refused or nothing, where the run came to that
// - apart, where a bracketed case is apart
// - alike, where every bracketed case is alike and one has a bracket
// - gray, where no case has a bracket, or no answer of the part came to read
//
// The read of each case goes into `reads` where `reads` is not NULL
AnswerTally answer_tally_read(const AnswerBracket *host, unsigned int cases, unsigned int outcome,
                              const unsigned long long *answered, AnswerRead *reads);

#endif
