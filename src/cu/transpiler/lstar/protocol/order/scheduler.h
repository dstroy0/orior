// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SCHEDULER_H
#define SCHEDULER_H

// The scheduler (P13). The gray entries of every set are its queue. At each turn, the scheduler chooses the query that
// settles the most of them, and puts it through the asker of its answerer. It writes its answer to R and to every entry
// that it settles.
//
// The scheduler puts the queries in rounds. It writes each question that waits on no answer but that of the round
// before it, before it puts any. A round is one tier: the carrier carries its questions together, through the many of
// the run channel (run_channel.h). No question of a round waits on the answer of another. Within a round, the question
// that settles the most entries goes first, in the order passed where two settle as many. A round goes at max
// SCHEDULER_ROUND_MAX questions to a process, for the memory that each uses while the scheduler puts it.

#include "../teacher/run_channel.h"

// the max questions that one round carries to a process. The airlock reads a round in waves of the readings of the
// vendor, each a whole start of the vendor, whatever it holds. A larger round spreads those waves over more questions
#define SCHEDULER_ROUND_MAX 8192u

// The `count` questions of `question`, each of which settles `settles` entries, carried as one round. The one that
// settles the most goes first, stable where two settle as many, SCHEDULER_ROUND_MAX to a process. Each question gets
// its own outcome and answers. The return is the count of those answered
unsigned int scheduler_round(RunQuestion *const *question, const unsigned int *settles, unsigned int count);

#endif
