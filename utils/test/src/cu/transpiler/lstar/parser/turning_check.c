// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// turning_check.c: the turning case of a ladder form held: the first case of its relation whose three readings in the
// register pass stand apart, so that no run is placed by a coincidence
//
//   apart    for every relation that has one, the case's words differ, the answer differs from both, and the relation
//            over each word twice differs from the second word and from the answer
//   up       the shift up's case is (3, 4): (1, 2) is passed over, 1 shifted by 1 being the second word, which would
//            read the amount's run as the destination; read swapped, (2, 1) stands
//   none     a relation with no such case gives none
//
//     turning_check
#include "../../../../../../../src/cu/transpiler/lstar/parser/khw_write/khw_write_internal.h"

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

int main(void)
{
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        unsigned int word[2] = {0u, 0u};
        unsigned int expected = 0u;
        if (!khw_turning_words(anchor, 0, word, &expected))
        {
            continue;
        }
        const unsigned int first_twice[2] = {word[0], word[0]};
        const unsigned int second_twice[2] = {word[1], word[1]};
        unsigned int first_answer = 0u;
        unsigned int second_answer = 0u;
        const int read = ladder_answer(anchor, first_twice, 2u, &first_answer) &&
                         ladder_answer(anchor, second_twice, 2u, &second_answer);
        char what[160];
        snprintf(what, sizeof(what), "relation %u: the words (%u, %u) and the answer %u stand apart", anchor, word[0],
                 word[1], expected);
        check_that((word[0] != word[1]) && (expected != word[0]) && (expected != word[1]), what);
        snprintf(what, sizeof(what), "relation %u: each word twice answers %u and %u, apart from the second word and the answer",
                 anchor, first_answer, second_answer);
        check_that(read && (first_answer != word[1]) && (second_answer != word[1]) && (first_answer != expected) &&
                       (second_answer != expected),
                   what);
    }
    unsigned int up[2] = {0u, 0u};
    unsigned int shifted = 0u;
    check_that(khw_turning_words((unsigned int)LADDER_UP, 0, up, &shifted) && (up[0] == 3u) && (up[1] == 4u) &&
                   (shifted == 48u),
               "the shift up's turning case is (3, 4) with the answer 48");
    unsigned int swapped[2] = {0u, 0u};
    check_that(khw_turning_words((unsigned int)LADDER_UP, 1, swapped, &shifted) && (swapped[0] == 2u) &&
                   (swapped[1] == 1u) && (shifted == 4u),
               "the shift up's turning case read swapped is (2, 1): 2 shifted by 2 and 1 by 1 are apart from 1 and 4");
    printf("turning_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
