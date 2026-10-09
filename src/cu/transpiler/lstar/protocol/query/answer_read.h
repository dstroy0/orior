// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef ANSWER_READ_H
#define ANSWER_READ_H

// What the part answered of an ask, read as P1 reads it (P11, the ask).
//
// A form f is put on a case k beside the answer of the host, h(k): `f(k) = h(k) ?`. The part gives one of four
// answers. Alike, where f(k) = h(k). Apart, where it gives another word. Refused, where it ends the run: an illegal
// instruction, an illegal address or a launch out of resources. Nothing, where no answer comes back. Alike is truthy
// and the other three are falsy.
//
// The host's answer to a case is a bracket at the result's width, [down, up] (Collapse 1). An integer case is a
// bracket of one word, down = up; a floating case a bracket of two ends, and the part is alike where it answers
// either end. A case on which the host traps or leaves the result undefined carries no bracket: the part's answer
// there is kept by whoever asked, and it gates nothing.
//
// Gray is the uncertainty: no answer of the part decides the case. It is read where the host gives no bracket, and
// where no answer of the part came to be read at all, an ask the gate held off the part or that nothing carried. Gray
// is never 0 and never 1 (P1, P10).

#include "../teacher/run_channel.h"

// what one case of an ask reads
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

// What an ask over many cases reads: the read of the whole, the count of cases alike, the reading an ask
// alike on every case carries (P10), and the place of the first case apart, `cases` where none is
typedef struct
{
    AnswerRead read;
    unsigned int alike;
    unsigned int first_apart;
} AnswerAsk;

// One case read: the host's bracket `host`, and the part's word `answered` where the run's outcome `outcome`
// (RunOutcome) is RUN_ANSWERED
AnswerRead answer_read(const AnswerBracket *host, unsigned int outcome, unsigned long long answered);

// 1 where `read` is truthy: alike, 0 for every other reading
int answer_truthy(AnswerRead read);

// An ask over `cases` cases read: the host's bracket of each in `host`, the run's outcome `outcome`, and the part's
// word on each in `answered`. Refused or nothing where the run came to that, apart where a bracketed case is apart,
// alike where every bracketed case is alike and one is bracketed, and gray where no case is bracketed or no answer of
// the part came to be read. Each case's read into `reads` where it is not NULL
AnswerAsk answer_ask_read(const AnswerBracket *host, unsigned int cases, unsigned int outcome,
                          const unsigned long long *answered, AnswerRead *reads);

#endif
