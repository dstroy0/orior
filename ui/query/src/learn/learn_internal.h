// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_internal.h: what learn.c and learn_steps.c share: what a learn's words ask for, the bounds of the clock asked
// of the bus, and the passes' own calls
#ifndef LEARN_INTERNAL_H
#define LEARN_INTERNAL_H

#include "../query_loops.h"

// the span of the BAR0 registers the clock is asked in where none is given: the first byte offset and the count of
// words, in hex. The part's answer is which word of it advances; the span is only where the asking starts
#define QUERY_CLOCK_FROM "9000"

#define QUERY_CLOCK_WORDS "400"

// the BAR0 driver's service, which maps the part's BAR0 registers read-only (the scaffolding's README)
#define QUERY_LISTENER "GpuBar0"

// What a learn's words ask for: discover, turns, registers or costs, a base source where one is given; a discover
// pass seeded from the listing named; the forms widened for a count of rounds, through the airlock; every question put
// dry for a count of rounds, each held to the gate with the part taken out; one slot's questions emitted for the cross
// to read; the clock asked of the bus, the ADVANCES qualifier at each word of a span of the BAR0 registers, the span's
// first byte offset and its count of words, in hex
typedef enum
{
    QUERY_LEARN_PASS,
    QUERY_LEARN_SEED,
    QUERY_LEARN_ROUNDS,
    QUERY_LEARN_DRY,
    QUERY_LEARN_ENUMERATE,
    QUERY_LEARN_CLOCK
} QueryLearnKind;

// a step read: its kind; the pass's name, the listing, the slot or the clock's first offset in `word`; the base source
// or the clock's count of words in `base`; the count of rounds
typedef struct
{
    QueryLearnKind kind;
    const char *word;
    const char *base;
    unsigned int rounds;
} QueryLearnStep;

int query_learn_step_read(const char *const *words, unsigned int count, QueryLearnStep *step, CliLine *said);
// the lines of the latest blob of `name` that hold more than spaces, counted
unsigned int query_learn_lines(const QueryRun *run, const char *name);
// the forms lists and the faults carried in, and the lessons where `lessons`
int query_learn_carried(QueryContext *context, QueryRun *run, int lessons, CliLine *said);
// the teacher's first words: the part, its machine file, the vendor's tables and the vendor's writer
void query_teacher_words(QueryWords *words, const char *machine, const QueryFiles *files, const char *writer);
// the gate's verdicts counted off the dry record, a line a verdict
void query_verdicts_said(QueryContext *context, const QueryRun *run);
// 1 where the BAR0 driver's service runs, read from the service controller
int query_bar0_driver_runs(void);

#endif
