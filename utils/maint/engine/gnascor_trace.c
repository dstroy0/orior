// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gnascor_trace.c: a trace of real queries for gnascor_read.py, put to the host a cycle at a time
//
//     gnascor_trace <scenario> <trace>
//
// A scenario contains one line a cycle, a spec for each side: satisfied, not, late or unasked. Each side is a run of
// queries put through query_put (src/cu/transpiler/lstar/protocol/query/query_put.h) between two reads of the clock,
// and the trace line written for it is the kind its queries came back as and the clock's advance across the run:
//
//     satisfied  QUERY_EQUALS at a word the program owns, carrying the word it contains
//     not        QUERY_EQUALS at that word, carrying a word it does not contain
//     late       QUERY_ADVANCES at the clock itself: it is satisfied, and each query waits out a step of the clock
//     unasked    nothing is put, and the side is written as -
//
// The clock is the first word of the page every Windows process shares that the protocol finds advancing. The bound
// is derived and never written in, and the count of runs it is read from is steered by the part, as a walk. Runs of
// satisfied queries are put unbound, and the bound is the largest of every run put, plus their spread, plus one step of
// the clock, the least the clock can distinguish. Each step of the walk puts as many runs again, each read against the
// bound the record so far gives, and reads the bound again over the doubled record. The walk ends on the first step
// whose runs all land under the bound, and only where that step is as long as the reach the trace leans on it for:
// every side still to be queried. A short step that lands under the bound shows nothing about a longer reach, and a run
// the part stalls lands in the record before the bound is taken. There is no count the walk stops at.
//
// The trace is put in sections, one a cycle. Before each cycle a satisfied run is put for every side the cycle queries,
// each read against the bound. Where every one lands under it, the bound still applies to the part and the cycle is
// put. Where one lands past it, the part has moved since the bound was read, and the walk takes up the same record
// again for the reach still ahead before the cycle is put.
//
// How far the bound swings is recorded, section by section and in sum. The bound only grows, since a run can raise the
// largest or lower the least and never the reverse. Its whole swing is the steer cost, how hard the part pulled on
// the clock while the trace was put, and a section that swings where the sections before it did not marks work the
// part did and did not name: an interrupt, a stall, another process on the core. The least satisfied run is the base
// cost, and the base cost plus the steer cost is the cost indicator. Every figure is in the clock word's own counts,
// and every section's swing is written into the trace as a line gnascor_read.py passes over.
//
// A run that is satisfied and costs past the bound is written PAST_BOUND. The four baseline cycles gnascor_read.py
// reads its edge from are written at the head of the trace, from the first eight runs.
#include "../../../src/cu/transpiler/lstar/protocol/query/query_put.h"

#include <stdio.h>
#include <string.h>

// queries in one side's run: enough for a satisfied run to span several steps of the clock
#define TRACE_RUN 1000000ull
// satisfied runs put unbound before the bound is first read: the eight the four baseline cycles are written from
#define TRACE_BASELINE 8u

#if defined(_WIN32)
static volatile unsigned int s_owned = 0x5a5au;
// The bound a late run has to pass, 0 until the bound is derived. The clock's step is not one length on every part,
// and a run of so many steps can land short of a bound read off runs of so many queries: a late run is put until its
// cost passes the bound, and late means that
static unsigned long long s_late_past = 0ull;

// the record the bound is read from: the largest and least satisfied run, how many runs, the clock's step and the bound
typedef struct
{
    unsigned long long most;
    unsigned long long least;
    unsigned long long runs;
    unsigned long long step;
    unsigned long long bound;
} TraceRecord;

// one side's run: the kind its queries came back as, and the clock's advance across it
static unsigned int trace_run(const char *spec, unsigned long long clock, unsigned long long *cost)
{
    Query query = {0};
    unsigned long long count = TRACE_RUN;
    const int late = (strcmp(spec, "late") == 0);
    if (late)
    {
        query.address = clock;
        query.qualifier = QUERY_ADVANCES;
        query.turns = 1ull << 30;
        count = 0ull;
    }
    else
    {
        // a pointer widened to the 64-bit integer an query carries, which host_entry.h narrows back to the same pointer
        query.address = (unsigned long long)&s_owned;
        query.qualifier = QUERY_EQUALS;
        query.word = (strcmp(spec, "satisfied") == 0) ? 0x5a5au : 0xa5a5u;
    }
    const unsigned int start = host_read(clock);
    for (unsigned long long turn = 0ull; turn < count; turn += 1ull)
    {
        query_put(&query);
    }
    // a late run waits out steps of the clock until its cost is past the bound
    while (late && ((unsigned long long)(host_read(clock) - start) <= s_late_past))
    {
        query_put(&query);
    }
    const unsigned int end = host_read(clock);
    *cost = (unsigned long long)(end - start);
    return query.kind;
}

// a satisfied run put and added to the record; 1 where it lands under the bound the record had before it
static unsigned int trace_satisfied(TraceRecord *record, unsigned long long clock, unsigned long long *cost)
{
    trace_run("satisfied", clock, cost);
    record->most = (*cost > record->most) ? *cost : record->most;
    record->least = (*cost < record->least) ? *cost : record->least;
    record->runs += 1ull;
    return (*cost <= record->bound) ? 1u : 0u;
}

// The walk over `record` for `reach` sides still to be queried: steps of as many runs again, until a step at least as
// long as the reach lands under the bound. The bound's swing across the walk is returned
static unsigned long long trace_walk(TraceRecord *record, unsigned long long clock, unsigned long long reach)
{
    const unsigned long long from = record->bound;
    unsigned long long length = 0ull;
    unsigned int all_satisfied = 0u;
    while ((all_satisfied == 0u) || (length < reach))
    {
        length = record->runs;
        all_satisfied = 1u;
        for (unsigned long long run = 0ull; run < length; run += 1ull)
        {
            unsigned long long cost = 0ull;
            all_satisfied = (trace_satisfied(record, clock, &cost) != 0u) ? all_satisfied : 0u;
        }
        const unsigned long long before = record->bound;
        record->bound = record->most + (record->most - record->least) + record->step;
        printf("  a step of %llu satisfied runs: the bound swings %llu, to %llu counts\n", length,
               record->bound - before, record->bound);
    }
    s_late_past = record->bound;
    return record->bound - from;
}

static const char *trace_kind(unsigned int kind, unsigned long long cost, unsigned long long bound)
{
    if (kind != QUERY_SATISFIED)
    {
        return "UNSATISFIED";
    }
    return (cost > bound) ? "PAST_BOUND" : "SATISFIED";
}

static void trace_side(FILE *out, const char *spec, unsigned long long clock, unsigned long long bound)
{
    if (strcmp(spec, "unasked") == 0)
    {
        fprintf(out, "- - ");
        return;
    }
    unsigned long long cost = 0ull;
    const unsigned int kind = trace_run(spec, clock, &cost);
    fprintf(out, "%s %llu ", trace_kind(kind, cost, bound), cost);
}
#endif

int main(int count_of_words, char **words)
{
    if (count_of_words != 3)
    {
        printf("  gnascor_trace <scenario> <trace>\n");
        return 2;
    }
#if defined(_WIN32)
    const unsigned long long page = 0x7ffe0000ull;
    unsigned long long clock = 0ull;
    for (unsigned long long word = 0ull; (word < 16ull) && (clock == 0ull); word += 1ull)
    {
        Query counts = {0};
        counts.address = page + (word * 4ull);
        counts.qualifier = QUERY_ADVANCES;
        counts.turns = 1ull << 26;
        if (query_put(&counts) == 1u)
        {
            clock = counts.address;
        }
    }
    if (clock == 0ull)
    {
        printf("  no word of the shared page advanced: there is no clock to trace on\n");
        return 1;
    }
    TraceRecord record = {0};
    record.least = ~0ull;
    // one step of the clock: the advance a single late query waits out
    {
        Query once = {0};
        once.address = clock;
        once.qualifier = QUERY_ADVANCES;
        once.turns = 1ull << 30;
        const unsigned int before = host_read(clock);
        query_put(&once);
        const unsigned int after = host_read(clock);
        record.step = (unsigned long long)(after - before);
    }

    // the reach the bound is leaned on for: every side the scenario queries
    FILE *scenario = fopen(words[1], "r");
    FILE *out = fopen(words[2], "w");
    if ((scenario == NULL) || (out == NULL))
    {
        printf("  could not open the scenario or the trace\n");
        return 1;
    }
    unsigned long long ahead = 0ull;
    char spec[32];
    while (fscanf(scenario, "%31s", spec) == 1)
    {
        ahead += (strcmp(spec, "unasked") != 0) ? 1ull : 0ull;
    }
    rewind(scenario);

    unsigned long long baseline[TRACE_BASELINE];
    for (unsigned int run = 0u; run < TRACE_BASELINE; run += 1u)
    {
        trace_satisfied(&record, clock, &baseline[run]);
    }
    record.bound = record.most + (record.most - record.least) + record.step;
    const unsigned long long first_bound = record.bound;
    trace_walk(&record, clock, ahead);

    fprintf(out, "# clock 0x%llx, step %llu, bound %llu from %llu unbound satisfied runs (%llu to %llu)\n", clock,
            record.step, record.bound, record.runs, record.least, record.most);
    for (unsigned int run = 0u; run < 4u; run += 1u)
    {
        fprintf(out, "SATISFIED %llu SATISFIED %llu %llu\n", baseline[2u * run], baseline[(2u * run) + 1u],
                record.bound);
    }
    char left[32];
    char right[32];
    unsigned int cycles = 0u;
    while (fscanf(scenario, "%31s %31s", left, right) == 2)
    {
        // the section's check: a satisfied run for every side the cycle queries, each read against the bound
        const unsigned long long reach =
            ((strcmp(left, "unasked") != 0) ? 1ull : 0ull) + ((strcmp(right, "unasked") != 0) ? 1ull : 0ull);
        unsigned int all_satisfied = 1u;
        for (unsigned long long run = 0ull; run < reach; run += 1ull)
        {
            unsigned long long cost = 0ull;
            all_satisfied = (trace_satisfied(&record, clock, &cost) != 0u) ? all_satisfied : 0u;
        }
        unsigned long long swing = 0ull;
        if (all_satisfied == 0u)
        {
            swing = trace_walk(&record, clock, ahead);
        }
        printf("  cycle %u: the bound swings %llu, to %llu counts\n", cycles + 1u, swing, record.bound);
        fprintf(out, "# cycle %u swing %llu bound %llu\n", cycles + 1u, swing, record.bound);
        trace_side(out, left, clock, record.bound);
        trace_side(out, right, clock, record.bound);
        fprintf(out, "%llu\n", record.bound);
        ahead -= reach;
        cycles += 1u;
    }
    const unsigned long long steer = record.bound - first_bound;
    fprintf(out, "# base cost %llu, steer cost %llu, cost indicator %llu\n", record.least, steer, record.least + steer);
    fclose(scenario);
    fclose(out);
    printf("  %u cycles traced on the clock at 0x%llx, bound %llu counts from %llu satisfied runs\n", cycles, clock,
           record.bound, record.runs);
    printf("  base cost %llu, steer cost %llu, cost indicator %llu counts\n", record.least, steer,
           record.least + steer);
    return 0;
#else
    printf("  no shared page named to this program on this platform: nothing is traced\n");
    return 0;
#endif
}
