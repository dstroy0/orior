// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer_read.c: what the part answered of an ask, read as P1 reads it
#include "answer_read.h"

#include <stddef.h>

AnswerRead answer_read(const AnswerBracket *host, unsigned int outcome, unsigned long long answered)
{
    if (outcome == (unsigned int)RUN_ILLEGAL)
    {
        return (host->bracketed != 0) ? ANSWER_REFUSED : ANSWER_GRAY;
    }
    if (outcome == (unsigned int)RUN_NOTHING)
    {
        return (host->bracketed != 0) ? ANSWER_NOTHING : ANSWER_GRAY;
    }
    // an ask the gate held off the part, or that nothing carried, has no answer of the part to read
    if ((outcome != (unsigned int)RUN_ANSWERED) || (host->bracketed == 0))
    {
        return ANSWER_GRAY;
    }
    return ((answered == host->down) || (answered == host->up)) ? ANSWER_ALIKE : ANSWER_APART;
}

int answer_truthy(AnswerRead read)
{
    return (read == ANSWER_ALIKE) ? 1 : 0;
}

AnswerAsk answer_ask_read(const AnswerBracket *host, unsigned int cases, unsigned int outcome,
                          const unsigned long long *answered, AnswerRead *reads)
{
    AnswerAsk asked = {ANSWER_GRAY, 0u, cases};
    unsigned int bracketed = 0u;
    for (unsigned int place = 0u; place < cases; place += 1u)
    {
        const AnswerRead read =
            answer_read(&host[place], outcome, (outcome == (unsigned int)RUN_ANSWERED) ? answered[place] : 0ull);
        if (reads != NULL)
        {
            reads[place] = read;
        }
        bracketed += (host[place].bracketed != 0) ? 1u : 0u;
        asked.alike += (read == ANSWER_ALIKE) ? 1u : 0u;
        if ((read == ANSWER_APART) && (asked.first_apart == cases))
        {
            asked.first_apart = place;
        }
    }
    if ((bracketed == 0u) || ((outcome != (unsigned int)RUN_ANSWERED) && (outcome != (unsigned int)RUN_ILLEGAL) &&
                              (outcome != (unsigned int)RUN_NOTHING)))
    {
        asked.read = ANSWER_GRAY;
        return asked;
    }
    if (outcome == (unsigned int)RUN_ILLEGAL)
    {
        asked.read = ANSWER_REFUSED;
        return asked;
    }
    if (outcome == (unsigned int)RUN_NOTHING)
    {
        asked.read = ANSWER_NOTHING;
        return asked;
    }
    asked.read = (asked.first_apart != cases) ? ANSWER_APART : ANSWER_ALIKE;
    return asked;
}
