// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer_read_check.c: the read of a query checked against P11's four answers and gray, on cases whose reads are known
//
//   alike      a word the bracket contains, at either end of a bracket of two
//   apart      a word the bracket does not contain, and the first case apart named
//   refused    a run the part ended, on a case the host brackets
//   nothing    a run nothing came back from, on a case the host brackets
//   gray       a case the host gives no bracket, whatever the part did there, and a query the gate blocked
//   truthy     alike alone
#include "../../../../../../../src/cu/transpiler/lstar/protocol/query/answer_read.h"

#include <stdio.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int gated, const char *what)
{
    s_checks += 1u;
    if (gated == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

int main(void)
{
    const AnswerBracket word = {1, 2ull, 2ull};
    const AnswerBracket ends = {1, 0x3f800000ull, 0x3f800001ull};
    const AnswerBracket none = {0, 0ull, 0ull};

    check_that(answer_read(&word, RUN_ANSWERED, 2ull) == ANSWER_ALIKE, "a word the bracket of one contains is alike");
    check_that(answer_read(&word, RUN_ANSWERED, 3ull) == ANSWER_APART, "another word is apart");
    check_that(answer_read(&ends, RUN_ANSWERED, 0x3f800000ull) == ANSWER_ALIKE, "the down end is alike");
    check_that(answer_read(&ends, RUN_ANSWERED, 0x3f800001ull) == ANSWER_ALIKE, "the up end is alike");
    check_that(answer_read(&ends, RUN_ANSWERED, 0x3f800002ull) == ANSWER_APART, "a word past both ends is apart");
    check_that(answer_read(&word, RUN_ILLEGAL, 0ull) == ANSWER_REFUSED, "a run the part ended is refused");
    check_that(answer_read(&word, RUN_NOTHING, 0ull) == ANSWER_NOTHING, "a run nothing came back from is nothing");
    check_that(answer_read(&none, RUN_ANSWERED, 2ull) == ANSWER_GRAY, "a case with no bracket is gray");
    check_that(answer_read(&none, RUN_ILLEGAL, 0ull) == ANSWER_GRAY, "a refusal on a case with no bracket is gray");
    check_that(answer_read(&word, RUN_GATED, 0ull) == ANSWER_GRAY, "a query the gate blocked is gray");
    check_that(answer_read(&word, RUN_NO_CHANNEL, 0ull) == ANSWER_GRAY, "a query nothing carried is gray");

    check_that(answer_truthy(ANSWER_ALIKE) == 1, "alike is truthy");
    check_that((answer_truthy(ANSWER_APART) == 0) && (answer_truthy(ANSWER_REFUSED) == 0) &&
                   (answer_truthy(ANSWER_NOTHING) == 0),
               "the other three are falsy");
    check_that(answer_truthy(ANSWER_GRAY) == 0, "gray is not truthy");

    const AnswerBracket cases[4] = {{1, 2ull, 2ull}, {0, 0ull, 0ull}, {1, 5ull, 5ull}, {1, 7ull, 7ull}};
    const unsigned long long alike_words[4] = {2ull, 99ull, 5ull, 7ull};
    AnswerRead reads[4];
    const AnswerTally alike = answer_tally_read(cases, 4u, RUN_ANSWERED, alike_words, reads);
    check_that(alike.read == ANSWER_ALIKE, "a query alike on every bracketed case is alike");
    check_that(alike.alike == 3u, "its reading is the count of cases alike");
    check_that(alike.first_apart == 4u, "and it names no case apart");
    check_that(reads[1] == ANSWER_GRAY, "the case with no bracket reads gray inside it");

    const unsigned long long apart_words[4] = {2ull, 0ull, 6ull, 8ull};
    const AnswerTally apart = answer_tally_read(cases, 4u, RUN_ANSWERED, apart_words, NULL);
    check_that(apart.read == ANSWER_APART, "a query apart on one case is apart");
    check_that(apart.first_apart == 2u, "the first case apart is named");

    const AnswerTally refused = answer_tally_read(cases, 4u, RUN_ILLEGAL, alike_words, NULL);
    check_that(refused.read == ANSWER_REFUSED, "a query the part ended is refused");
    const AnswerTally nothing = answer_tally_read(cases, 4u, RUN_NOTHING, alike_words, NULL);
    check_that(nothing.read == ANSWER_NOTHING, "a query nothing came back from is nothing");
    const AnswerTally gated = answer_tally_read(cases, 4u, RUN_GATED, alike_words, NULL);
    check_that(gated.read == ANSWER_GRAY, "a query the gate blocked is gray");

    const AnswerBracket unbracketed[2] = {{0, 0ull, 0ull}, {0, 0ull, 0ull}};
    const unsigned long long words[2] = {1ull, 2ull};
    check_that(answer_tally_read(unbracketed, 2u, RUN_ANSWERED, words, NULL).read == ANSWER_GRAY,
               "a query no case of which is bracketed is gray");
    check_that(answer_tally_read(unbracketed, 2u, RUN_ILLEGAL, words, NULL).read == ANSWER_GRAY,
               "and stays gray where the part ended it");

    printf("answer_read_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
