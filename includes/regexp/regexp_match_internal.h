// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_match_internal.h: what matching shares across regexp_match.c and regexp_walk.c: a match's run, the frames
// that hand a place on, and the steps of matching one calls in another
#ifndef REGEXP_MATCH_INTERNAL_H
#define REGEXP_MATCH_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "regexp.h"

typedef struct RegexpMatchIndexCtx RegexpMatchIndexCtx;
typedef struct RegexpMatchThenCtx RegexpMatchThenCtx;
typedef struct RegexpMatchIndexAtCtx RegexpMatchIndexAtCtx;
typedef struct RegexpMatchCtx RegexpMatchCtx;
typedef struct RegexpMatchACtx RegexpMatchACtx;

// The max passes' ends that the one-way repeats hold at once. The ends a match keeps on its own stack before they go
// to a cell. The mark of a pass that took nothing
#define REGEXP_ENDS_MAX (1u << 22u)
#define REGEXP_ENDS_STACKED 256u

#define REGEXP_NO_PASS 0xffffffffffffffffULL

// Where a match is: the text and its length, each group's span, the steps taken, and where the match passed its end.
// It holds the address of its stack as it began. It holds the passes' ends of the one-way repeats on the stack, each
// repeat's from where it began, with their count and their room. The ends stay on the match's own stack until they
// outgrow it, and then go into a cell at their max. `ends_reserved` is 1 while they are in a cell
typedef struct
{
    const unsigned int *text;
    unsigned long long length;
    RegexpSpan *groups;
    unsigned long long steps;
    unsigned long long end;
    embed_word base;
    unsigned int *ends;
    unsigned long long ends_top;
    unsigned long long ends_capacity;
    int ends_reserved;
} RegexpRun;

// What comes after a node, given the place it stops at. It is the match's end, or the rest of the sequence `node`
// from its part `index`. It is the end of the group `index`, which began at `from`, or a lookahead's true. It is
// the next pass of the repetition `node` after `count` passes, this one from `from`. Each passes on to `next`
enum
{
    REGEXP_THEN_END,
    REGEXP_THEN_CHAIN,
    REGEXP_THEN_GROUP,
    REGEXP_THEN_TRUE,
    REGEXP_THEN_REPEAT
};

typedef struct RegexpThen
{
    unsigned int type;
    unsigned int node;
    unsigned int index;
    unsigned int count;
    unsigned long long from;
    const struct RegexpThen *next;
} RegexpThen;

// A word's character, a space, a line's end, and two characters the same, under `i` without case. Then a class
// node's or a character node's match of one character, -1 for a node that takes no one character
int regexp_word(const RegexpMatchCtx *args);
int regexp_space(const RegexpMatchCtx *args);
int regexp_ends_line(const RegexpMatchCtx *args);
int regexp_same(const RegexpMatchACtx *args);
int regexp_one(const RegexpMatchCtx *args);
// The steps (regexp_match.c). What comes after a node at a place, the rest of a sequence, and a repetition at a pass.
// A repeat over one character, and one whose pass takes the text one way or none
int regexp_then(const RegexpMatchIndexCtx *args);
int regexp_chain(const RegexpMatchIndexCtx *args);
int regexp_repeat(const RegexpMatchIndexCtx *args);
int regexp_count(const RegexpMatchThenCtx *args);
int regexp_on_pass(const RegexpMatchThenCtx *args);
// A node walked from `at`, with the place it stops passed to `then` (regexp_walk.c). Then where a pass of a one-way
// node that takes the text at `at` ends, or REGEXP_NO_PASS
int regexp_walk(const RegexpMatchIndexCtx *args);
unsigned long long regexp_pass(const RegexpMatchIndexAtCtx *args);

struct RegexpMatchIndexCtx
{
    const Regexp *regexp;
    unsigned int index;
    const RegexpThen *then;
    unsigned long long at;
    RegexpRun *run;
    unsigned int node;
    unsigned int count;
};

struct RegexpMatchThenCtx
{
    const Regexp *regexp;
    const RegexpThen *then;
    unsigned long long at;
    RegexpRun *run;
    const RegexpNode *repeat;
};

struct RegexpMatchIndexAtCtx
{
    const Regexp *regexp;
    unsigned int index;
    unsigned long long at;
    const RegexpRun *run_read;
};

struct RegexpMatchCtx
{
    unsigned int character;
    const Regexp *regexp;
    unsigned int index;
};

struct RegexpMatchACtx
{
    const Regexp *regexp;
    unsigned int a;
    unsigned int b;
};

typedef struct
{
    int (*regexp_word)(const RegexpMatchCtx *args);
    int (*regexp_space)(const RegexpMatchCtx *args);
    int (*regexp_ends_line)(const RegexpMatchCtx *args);
    int (*regexp_one)(const RegexpMatchCtx *args);
} RegexpMatchNs;
EMBED_TABLE_LAYOUT(RegexpMatchNs, regexp_word, regexp_space, regexp_ends_line, regexp_one);
EMBED_TABLE_STORAGE RegexpMatchNs regexp_match_internal EMBED_UNUSED = {
    .regexp_word = regexp_word,
    .regexp_space = regexp_space,
    .regexp_ends_line = regexp_ends_line,
    .regexp_one = regexp_one,
};

typedef struct
{
    int (*regexp_same)(const RegexpMatchACtx *args);
} RegexpMatchANs;
EMBED_TABLE_LAYOUT(RegexpMatchANs, regexp_same);
EMBED_TABLE_STORAGE RegexpMatchANs regexp_match_a_internal EMBED_UNUSED = {
    .regexp_same = regexp_same,
};

typedef struct
{
    int (*regexp_then)(const RegexpMatchIndexCtx *args);
    int (*regexp_chain)(const RegexpMatchIndexCtx *args);
    int (*regexp_repeat)(const RegexpMatchIndexCtx *args);
    int (*regexp_walk)(const RegexpMatchIndexCtx *args);
} RegexpMatchIndexNs;
EMBED_TABLE_LAYOUT(RegexpMatchIndexNs, regexp_then, regexp_chain, regexp_repeat, regexp_walk);
EMBED_TABLE_STORAGE RegexpMatchIndexNs regexp_match_index_internal EMBED_UNUSED = {
    .regexp_then = regexp_then,
    .regexp_chain = regexp_chain,
    .regexp_repeat = regexp_repeat,
    .regexp_walk = regexp_walk,
};

typedef struct
{
    int (*regexp_count)(const RegexpMatchThenCtx *args);
    int (*regexp_pass)(const RegexpMatchThenCtx *args);
} RegexpMatchThenNs;
EMBED_TABLE_LAYOUT(RegexpMatchThenNs, regexp_count, regexp_pass);
EMBED_TABLE_STORAGE RegexpMatchThenNs regexp_match_then_internal EMBED_UNUSED = {
    .regexp_count = regexp_count,
    .regexp_pass = regexp_on_pass,
};

typedef struct
{
    unsigned long long (*regexp_pass)(const RegexpMatchIndexAtCtx *args);
} RegexpMatchIndexAtNs;
EMBED_TABLE_LAYOUT(RegexpMatchIndexAtNs, regexp_pass);
EMBED_TABLE_STORAGE RegexpMatchIndexAtNs regexp_match_index_at_internal EMBED_UNUSED = {
    .regexp_pass = regexp_pass,
};

#endif
