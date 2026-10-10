// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scheduler.c: a round of questions carried together, the one that settles the most entries first
#include "scheduler.h"

unsigned int scheduler_round(RunQuestion *const *question, const unsigned int *settles, unsigned int count)
{
    unsigned int answered = 0u;
    for (unsigned int first = 0u; first < count; first += SCHEDULER_ROUND_MOST)
    {
        const unsigned int carried = ((count - first) < SCHEDULER_ROUND_MOST) ? (count - first) : SCHEDULER_ROUND_MOST;
        // the round's order by insertion, the most settling first and the order handed kept between equals: a round
        // holds few enough questions for that
        RunQuestion *ordered[SCHEDULER_ROUND_MOST];
        unsigned int weight[SCHEDULER_ROUND_MOST];
        for (unsigned int at = 0u; at < carried; at += 1u)
        {
            unsigned int place = at;
            while ((place > 0u) && (weight[place - 1u] < settles[first + at]))
            {
                ordered[place] = ordered[place - 1u];
                weight[place] = weight[place - 1u];
                place -= 1u;
            }
            ordered[place] = question[first + at];
            weight[place] = settles[first + at];
        }
        answered += run_channel_ask_many(ordered, carried);
    }
    return answered;
}
