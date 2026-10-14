// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_check.c: the frame's asks held to what reads them (The frame asked, Open 6.3)
//
//   walk       the halving walk reads the greatest number a counted relation holds at, asking fewer times than the
//              span, and 0 where nothing past 0 holds
//
//     frame_check <folder>
//
// It runs in a query run (qry_run.sh), under the top of the tree QRY_TOP names, the folder within it.
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table/identity/frame_walk.h"

#include <stdio.h>

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

// a relation that holds at every number up to its bound, counting how often it is asked
typedef struct
{
    unsigned long long bound;
    unsigned int asked;
} Counted;

static int counted_holds(unsigned long long number, void *context)
{
    Counted *const counted = (Counted *)context;
    counted->asked += 1u;
    return number <= counted->bound;
}

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("frame_check <folder>\n");
        return 2;
    }
    (void)words;
    Counted five = {5ull, 0u};
    check_that(frame_greatest(counted_holds, &five, 0ull, 32ull) == 5ull,
               "the walk reads the greatest number a relation holds at");
    check_that(five.asked <= 6u, "and asks no more times than the span's halvings");
    Counted none = {0ull, 0u};
    check_that(frame_greatest(counted_holds, &none, 0ull, 32ull) == 0ull, "and 0 where nothing past 0 holds");
    Counted all = {31ull, 0u};
    check_that(frame_greatest(counted_holds, &all, 0ull, 32ull) == 31ull,
               "and the number before the falsy end where every one before it holds");
    printf("frame_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
