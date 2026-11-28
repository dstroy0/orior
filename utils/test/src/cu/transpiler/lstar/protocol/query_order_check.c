// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_order_check.c: the known order of queries put to the host through the query protocol, and its solve checked
// against the work each link does
//
// Seven links, each a query at a word the test owns and nothing writes: QUERY_ADVANCES, which reads the word until it
// moves or its turns run out. A still word never moves, and link k reads it 1 + 64·k times: the work each link
// does is known and the links are far apart in it. One access against five is not far apart: a query's own overhead is
// larger than four reads, and the order put over QUERY_RETAINS against QUERY_EQUALS does not distinguish them.
//
// The clock is not named to the order: the test queries every word of the page every Windows process shares whether it
// advances, and takes the first that does. The clock turns over now and then, and a run is read on it finely by
// counting reads of the clock between turns, its cost the exact rational that gives. The known order is then put, every
// pass is solved on its own in exact integers, and every link has to come out dearer than the link before it, past
// twice the spread. That is the claim checked here: the solve, run on the part's own clock with nothing rounded and
// nothing summed away, orders links by the work they do.
//
// The run size and the count of passes are steered by the part, as query_descent.h states. The claim is read on the
// record of the size chosen: every pair has to lean dearer on one same pass, over every pass put at that size.
#include "query_descent.h"
#include "../../../../../../../includes/platform/platform.h"

#include <stdio.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int descent, const char *what)
{
    s_checks += 1u;
    if (descent == 0)
    {
        s_failed += 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  FAILED: ");
            mmgr_line_put(output_line, what);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if defined(_WIN32)
    // the clock, found by probing: the first word of the shared page's head that advances inside `turns` reads, and
    // the order waits on its turns as long
    const unsigned long long page = 0x7ffe0000ull;
    const unsigned long long turns = 1ull << 26;
    unsigned long long clock = 0ull;
    for (unsigned long long word = 0ull; (word < 16ull) && (clock == 0ull); word += 1ull)
    {
        Query counts = {0};
        counts.address = page + (word * 4ull);
        counts.qualifier = QUERY_ADVANCES;
        counts.turns = turns;
        clock = (query_put(&counts) == 1u) ? counts.address : 0ull;
    }
    check_that(clock != 0ull, "a word of the shared page advances, and it is the clock");
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the clock, found by probing: 0x");
        mmgr_line_hex(output_line, clock, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    // a word for each link, a cache line apart
    static volatile unsigned int s_owned[QUERY_DESCENT_LINKS * 16u];
    Query link[QUERY_DESCENT_LINKS] = {{0}};
    for (unsigned int number = 0u; number < QUERY_DESCENT_LINKS; number += 1u)
    {
        // a pointer widened to the 64-bit integer a query carries, which host_entry.h narrows back to the same pointer
        link[number].address = (unsigned long long)&s_owned[number * 16u];
        link[number].qualifier = QUERY_ADVANCES;
        link[number].turns = 64ull * number;
    }

    QueryOrder order = {0};
    order.link = link;
    order.links = QUERY_DESCENT_LINKS;
    order.clock = clock;
    order.turns = turns;

    // Checked at the size the descent chose. Every pass solved on its own, exactly, and every pair of neighboring links
    // compared in it: a bit a pass, 1 where the link that reads 64 more times costs more. Nothing is summed across
    // passes and nothing is cut to a least
    QueryDescent descent;
    unsigned int solved = 0u;
    query_descent_size(&order, &descent, &solved);
    check_that(solved == 1u, "every pass solves exactly inside the exact width");
    unsigned int ordered = 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  at ");
        mmgr_line_decimal(output_line, order.repeat, 1u);
        mmgr_line_put(output_line, " puts a run, each pass solved exactly, ");
        mmgr_line_decimal(output_line, descent.passes, 1u);
        mmgr_line_put(output_line, " passes put, the link reading 64 more times:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int number = 1u; number < QUERY_DESCENT_LINKS; number += 1u)
    {
        ordered += (descent.decided[number] == QUERY_DESCENT_DEARER) ? 1u : 0u;
        const char *leans = (descent.decided[number] == QUERY_DESCENT_DEARER)    ? ""
                            : (descent.decided[number] == QUERY_DESCENT_CHEAPER) ? ", leaning cheaper"
                                                                                 : ", not past twice the spread";
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    link ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, " over link ");
            mmgr_line_decimal(output_line, number - 1u, 1u);
            mmgr_line_put(output_line, ": dearer in ");
            mmgr_line_decimal(output_line, descent.dearer[number], 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, descent.passes, 1u);
            mmgr_line_put(output_line, " passes");
            mmgr_line_put(output_line, leans);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    check_that(ordered == (QUERY_DESCENT_LINKS - 1u),
               "every link is decided dearer than the one before it past the spread");
#else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  no shared page named to this test on this platform: the order is not put");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
#endif

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  query order: ");
        mmgr_line_decimal(output_line, s_checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, s_failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (s_failed == 0u) ? 0 : 1;
}
