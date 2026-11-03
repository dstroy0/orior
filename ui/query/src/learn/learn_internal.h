// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_internal.h: what learn.c and learn_steps.c share: what a learn's words request, the bounds of the clock queried
// of the bus, and the passes' own calls
#ifndef LEARN_INTERNAL_H
#define LEARN_INTERNAL_H
#include "../query_loops.h"
#include "embed_dispatch_layout.h"

typedef struct QueryLearnErrMsgCtx QueryLearnErrMsgCtx;
typedef struct QueryLearnWordsCtx QueryLearnWordsCtx;
typedef struct QueryLearnWordsWordsCtx QueryLearnWordsWordsCtx;
typedef struct QueryLearnCtx QueryLearnCtx;
typedef struct QueryLearnRunCtx QueryLearnRunCtx;

// the span of the BAR0 registers the clock is queried in where none is given: the first byte offset and the count of
// words, in hex. The part's answer is which word of it advances; the span is only where the querying starts
#define QUERY_CLOCK_FROM "9000"

#define QUERY_CLOCK_WORDS "400"

// the BAR0 driver's service, which maps the part's BAR0 registers read-only (the scaffolding's README)
#define QUERY_LISTENER "GpuBar0"

// What a learn's words request: discover, turns, registers or costs, a base source where one is given; a discover
// pass seeded from the listing named; the forms widened for a count of rounds, through the airlock; every question put
// dry for a count of rounds, each checked at the gate with the part taken out; one place's questions emitted for the
// cross to read; the clock queried from the bus, the ADVANCES qualifier at each word of a span of the BAR0 registers,
// the span's first byte offset and its count of words, in hex
typedef enum
{
    QUERY_LEARN_PASS,
    QUERY_LEARN_SEED,
    QUERY_LEARN_ROUNDS,
    QUERY_LEARN_DRY,
    QUERY_LEARN_ENUMERATE,
    QUERY_LEARN_CLOCK
} QueryLearnKind;

// a step read: its kind; the pass's name, the listing, the place or the clock's first offset in `word`; the base source
// or the clock's count of words in `base`; the count of rounds
typedef struct
{
    QueryLearnKind kind;
    const char *word_text;
    const char *base;
    unsigned int rounds;
} QueryLearnStep;

int query_learn_step_read(const QueryLearnWordsCtx *args);
// the lines of the latest blob of `name` that contain more than spaces, counted
unsigned int query_learn_lines(const QueryLearnCtx *args);
// the forms lists and the faults carried in, and the lessons where `lessons`
int query_learn_machine_transfer(const QueryLearnErrMsgCtx *args);
// the teacher's first words: the part, its machine file, the vendor's tables and the vendor's writer
void query_teacher_words(const QueryLearnWordsWordsCtx *args);
// the gate's verdicts counted off the dry record, a line a verdict
void query_verdicts_print(const QueryLearnRunCtx *args);
// 1 where the BAR0 driver's service runs, read from the service controller
int query_bar0_driver_runs(void);

struct QueryLearnErrMsgCtx
{
    VerbaLinea *err_msg;
    QueryContext *context;
    QueryRun *run_out;
    int lessons;
};

struct QueryLearnWordsCtx
{
    const char *const *words;
    unsigned int count;
    QueryLearnStep *step;
    VerbaLinea *err_msg;
};

struct QueryLearnWordsWordsCtx
{
    QueryWords *words_words_pointer;
    const char *machine;
    const QueryFiles *files;
    const char *writer;
};

struct QueryLearnCtx
{
    const QueryRun *run;
    const char *name;
};

struct QueryLearnRunCtx
{
    const QueryRun *run;
    QueryContext *context;
};

typedef struct
{
    int (*step_read)(const QueryLearnWordsCtx *args);
} QueryLearnWordsNs;
EMBED_TABLE_LAYOUT(QueryLearnWordsNs, step_read);
EMBED_TABLE_STORAGE QueryLearnWordsNs query_learn_words_internal EMBED_UNUSED = {
    .step_read = query_learn_step_read,
};

typedef struct
{
    unsigned int (*lines)(const QueryLearnCtx *args);
} QueryLearnNs;
EMBED_TABLE_LAYOUT(QueryLearnNs, lines);
EMBED_TABLE_STORAGE QueryLearnNs query_learn_internal EMBED_UNUSED = {
    .lines = query_learn_lines,
};

typedef struct
{
    int (*carry)(const QueryLearnErrMsgCtx *args);
} QueryLearnErrMsgNs;
EMBED_TABLE_LAYOUT(QueryLearnErrMsgNs, carry);
EMBED_TABLE_STORAGE QueryLearnErrMsgNs query_learn_err_msg_internal EMBED_UNUSED = {
    .carry = query_learn_machine_transfer,
};

typedef struct
{
    void (*teacher_words)(const QueryLearnWordsWordsCtx *args);
} QueryLearnWordsWordsNs;
EMBED_TABLE_LAYOUT(QueryLearnWordsWordsNs, teacher_words);
EMBED_TABLE_STORAGE QueryLearnWordsWordsNs query_learn_words_words_internal EMBED_UNUSED = {
    .teacher_words = query_teacher_words,
};

typedef struct
{
    void (*verdicts_print)(const QueryLearnRunCtx *args);
} QueryLearnRunNs;
EMBED_TABLE_LAYOUT(QueryLearnRunNs, verdicts_print);
EMBED_TABLE_STORAGE QueryLearnRunNs query_learn_run_internal EMBED_UNUSED = {
    .verdicts_print = query_verdicts_print,
};

#endif
