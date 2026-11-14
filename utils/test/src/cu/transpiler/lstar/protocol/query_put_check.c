// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_put_check.c: the query protocol's put, checked against what it specifies on addresses whose answers are known
//
// Each check puts a query to an address whose state the test set itself, or to one a reader can name as a counter,
// and compares the bit, the type and the cost against what that address has to answer:
//
//   retains    memory the test owns gives back both words it is put, and is left as it was found
//   equals     a word the test wrote reads as that word, and as no other
//   advances   a word nothing writes does not advance
//   bound      a bound with no clock reads 0 past the bound, and an unbound query carries its type
//   clock      a counter named by the platform advances under the ADVANCES qualifier, and a query clocked on it
//              returns a cost that a bound below it fails and a bound above it passes
//
// Only the counter sits at an address the test does not own. On Windows every process can read the
// interrupt time the kernel maintains at 0x7FFE0008 with a plain load and no call: the test forms its question there,
// and the protocol's answer is checked against the fact that the word counts. A platform that names no such address
// to the test skips the clock checks and reports it.
#include "../../../../../../../src/cu/transpiler/lstar/protocol/query/query_put.h"

#include <stdio.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int passed, const char *what)
{
    s_checks += 1u;
    if (passed == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// an address in this program, as the integer a query carries
static unsigned long long check_address(volatile unsigned int *word)
{
    // a pointer widened to the 64-bit integer a query carries, which host_entry.h narrows back to the same pointer
    return (unsigned long long)word;
}

int main(void)
{
    static volatile unsigned int s_owned[4] = {0x1234u, 0x5678u, 0u, 0u};

    Query query = {0};
    query.address = check_address(&s_owned[0]);
    query.qualifier = QUERY_RETAINS;
    check_that(query_put(&query) == 1u, "owned memory retains what it is given");
    check_that(query.type == QUERY_SATISFIED, "retaining reads as satisfied");
    check_that(s_owned[0] == 0x1234u, "a retaining query leaves the word as it found it");
    check_that(query.cost_read == 0u, "a query given no clock reports its cost unread");

    Query equals = {0};
    equals.address = check_address(&s_owned[1]);
    equals.qualifier = QUERY_EQUALS;
    equals.word = 0x5678u;
    check_that(query_put(&equals) == 1u, "a written word equals itself");
    equals.word = 0x5679u;
    check_that(query_put(&equals) == 0u, "a written word equals no other word");
    check_that(equals.type == QUERY_UNSATISFIED, "a qualifier that is not satisfied reads as unsatisfied");

    Query still = {0};
    still.address = check_address(&s_owned[2]);
    still.qualifier = QUERY_ADVANCES;
    check_that(query_put(&still) == 0u, "a word nothing writes does not advance");

    Query unclocked = {0};
    unclocked.address = check_address(&s_owned[1]);
    unclocked.qualifier = QUERY_EQUALS;
    unclocked.word = 0x5678u;
    unclocked.bound = 1000ull;
    check_that(query_put(&unclocked) == 0u, "a bound with no clock to read it against reads 0");
    check_that(unclocked.type == QUERY_PAST_BOUND, "and reads as past its bound, not as unsatisfied");

#if defined(_WIN32)
    const unsigned long long clock = 0x7ffe0008ull;
    Query counts = {0};
    counts.address = clock;
    counts.qualifier = QUERY_ADVANCES;
    // The counter is maintained in hundred-nanosecond counts and is stepped once per clock interrupt: it moves by a
    // whole interrupt at a time and a query is far shorter than one. The ADVANCES query reads until the counter moves,
    // and every clocked query below is put until the counter moves under it; each cap is far past the reads one
    // interrupt takes
    const unsigned int turns = 1u << 30;
    counts.turns = turns;
    check_that(query_put(&counts) == 1u, "the interrupt time advances under the ADVANCES qualifier, queried once");
    Query once = counts;
    once.turns = 0ull;
    unsigned int caught = 0u;
    for (unsigned int turn = 0u; turn < 1000u; turn += 1u)
    {
        caught += query_put(&once);
    }
    check_that(caught < 1000u, "two reads in a row see the counter still more often than not");

    // a query clocked on that counter, put until one query has a step of the counter inside it
    Query timed = {0};
    timed.address = check_address(&s_owned[1]);
    timed.qualifier = QUERY_EQUALS;
    timed.word = 0x5678u;
    timed.clock = clock;
    unsigned int put = 0u;
    for (put = 0u; (put < turns) && (timed.cost == 0ull); put += 1u)
    {
        query_put(&timed);
    }
    const unsigned long long step = timed.cost;
    check_that(timed.cost_read == 1u, "a query given a clock reads its cost");
    check_that(step > 0ull, "a query put until the counter moves under it reads a cost above nothing");
    check_that(timed.type == QUERY_SATISFIED,
               "an unbound query that was satisfied reads as satisfied, whatever it cost");

    // the bound judged against the cost the query itself read: a bound past one step of the counter passes wherever the
    // query lasts less than two steps, and a bound of one count fails on the query a step lands inside
    timed.bound = (step * 2ull) + 1ull;
    check_that(query_put(&timed) == 1u, "a bound past two steps of the counter passes");
    Query tight = timed;
    tight.bound = 1ull;
    unsigned int failed_tight = 0u;
    for (unsigned int turn = 0u; (turn < turns) && (failed_tight == 0u); turn += 1u)
    {
        failed_tight = ((query_put(&tight) == 0u) && (tight.type == QUERY_PAST_BOUND)) ? 1u : 0u;
    }
    check_that(failed_tight == 1u, "a bound of one count fails on the query a step of the counter lands inside");
    printf("  the clock at 0x%llx: one step is %llu counts, read after %u queries\n", clock, step, put);
#else
    printf("  no counter named to this test on this platform: the clock checks are skipped\n");
#endif

    printf("  query put: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
