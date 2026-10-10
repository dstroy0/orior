// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SCHEDULER_H
#define SCHEDULER_H

// The scheduler (P13). The gray entries of every set are its queue, and each turn the ask that settles the most of them
// is chosen, put through the asker of its answerer, and its answer written to R and to every entry it settles.
//
// The asks are put in rounds. Each question that waits on no answer but the round's before it is written before any is
// put, and a round is one tier: its questions are carried together, through the run channel's many (run_channel.h),
// and no question of a round waits on another's answer. Within a round the question that settles the most entries is
// put first, in the order handed where two settle as many. A round is carried SCHEDULER_ROUND_MOST questions to a
// process at most, for the memory each holds while it is put.

#include "../teacher/run_channel.h"

// the most questions one round carries to a process
#define SCHEDULER_ROUND_MOST 256u

// The `count` questions of `question`, each settling `settles` entries, carried as one round: the most settling first,
// stable where two settle as many, SCHEDULER_ROUND_MOST to a process. Each question's outcome and answers its own. The
// count of those answered
unsigned int scheduler_round(RunQuestion *const *question, const unsigned int *settles, unsigned int count);

#endif
