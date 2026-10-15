// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ranges_check.c: the cross's ranges held: the distinct slot encodings cut over the host's processors
//
//   tiling   the ranges stand end to end from the first encoding to the last, each holding at least one
//   count    no more ranges than processors, and none where there are no encodings
//   fewer    fewer encodings than processors go one to a range
//   whole    a count the processors divide goes in equal ranges
//
//     ranges_check
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query/msq_internal.h"

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

// the ranges of `words` encodings over `processors` tile the encodings end to end, none empty, none past the last
static int tiled(size_t words, unsigned int processors, size_t *count)
{
    MsqWork ranges[64];
    *count = msq_ranges(words, processors, ranges);
    size_t next = 0u;
    int held = *count <= (size_t)processors;
    for (size_t range = 0u; held && (range < *count); range += 1u)
    {
        held = (ranges[range].kind == MSQ_WORK_RANGE) && (ranges[range].first == next) && (ranges[range].count != 0u) &&
               ((ranges[range].first + ranges[range].count) <= words);
        next = ranges[range].first + ranges[range].count;
    }
    return held && (next == words);
}

int main(void)
{
    size_t count = 0u;
    check_that(tiled(18u, 16u, &count), "18 encodings over 16 processors tile end to end");
    check_that(tiled(17u, 16u, &count), "17 encodings over 16 processors tile end to end");
    check_that(tiled(33u, 16u, &count), "33 encodings over 16 processors tile end to end");
    check_that(tiled(8192u, 16u, &count) && (count == 16u), "8192 encodings over 16 processors go in 16 equal ranges");
    check_that(tiled(5u, 16u, &count) && (count == 5u), "5 encodings over 16 processors go one to a range");
    check_that(tiled(1u, 1u, &count) && (count == 1u), "one encoding on one processor is one range");
    check_that(tiled(0u, 16u, &count) && (count == 0u), "no encodings make no range");
    check_that(tiled(7u, 3u, &count) && (count == 3u), "7 encodings over 3 processors go in 3 ranges");
    printf("ranges_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
