// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_match_internal.h: what matching shares across regexp_match.c and regexp_walk.c: a match's run, the frames
// that hand a place on, and the steps of matching one calls in another
#ifndef REGEXP_MATCH_INTERNAL_H
#define REGEXP_MATCH_INTERNAL_H
typedef struct CliRegexpMatchIndexCtx CliRegexpMatchIndexCtx;
typedef struct CliRegexpMatchThenCtx CliRegexpMatchThenCtx;
typedef struct CliRegexpMatchIndexAtCtx CliRegexpMatchIndexAtCtx;
typedef struct CliRegexpMatchCtx CliRegexpMatchCtx;
typedef struct CliRegexpMatchACtx CliRegexpMatchACtx;

#include "embed_dispatch_layout.h"
#include "regexp.h"

#include <stdint.h>

// the most passes' ends the one-way repeats standing at once store, the ends a match stores on its own stack before
// they go to a cell, and a pass that took nothing
#define CLI_REGEXP_ENDS_MOST (1u << 22u)
#define CLI_REGEXP_ENDS_STACKED 256u

#define CLI_REGEXP_NO_PASS 0xffffffffffffffffull

// Where a match stands: the text and its length, each group's span, the steps taken, where the match's end was handed,
// and the address its stack stood at as it began; and the passes' ends of the one-way repeats standing on the stack,
// each repeat's from where it began, how many stand and the room they have, stored on the match's own stack until they
// outgrow it and then in a cell at their most, `ends_reserved` 1 while they are in one
typedef struct
{
    const unsigned int *text;
    unsigned long long length;
    CliRegexpSpan *groups;
    unsigned long long steps;
    unsigned long long end;
    uintptr_t base;
    unsigned int *ends;
    unsigned long long ends_top;
    unsigned long long ends_room;
    int ends_reserved;
} CliRegexpRun;

// What comes after a node, handed the place it stops at: the match's end; the rest of the sequence `node` from its
// part `index`; the group `index`'s end, it begun at `from`; a lookahead's true; the repetition `node`'s next pass,
// after `count` passes, this one begun at `from`. Each hands on to `next`
enum
{
    CLI_THEN_END,
    CLI_THEN_CHAIN,
    CLI_THEN_GROUP,
    CLI_THEN_TRUE,
    CLI_THEN_REPEAT
};

typedef struct CliRegexpThen
{
    unsigned int kind;
    unsigned int node;
    unsigned int index;
    unsigned int count;
    unsigned long long from;
    const struct CliRegexpThen *next;
} CliRegexpThen;

// a word's character, a space, a line's end; two letters the same, under `i` without case; a class node's or a
// character node's matching of one letter, -1 for a node that takes no one letter
int cli_regexp_word(const CliRegexpMatchCtx *args);
int cli_regexp_space(const CliRegexpMatchCtx *args);
int cli_regexp_ends_line(const CliRegexpMatchCtx *args);
int cli_regexp_same(const CliRegexpMatchACtx *args);
int cli_regexp_one(const CliRegexpMatchCtx *args);
// the steps (regexp_match.c): what comes after handed a place; the rest of a sequence; a repetition at a pass; a repeat
// over one character, and one whose pass takes the text one way or none
int cli_regexp_then(const CliRegexpMatchIndexCtx *args);
int cli_regexp_chain(const CliRegexpMatchIndexCtx *args);
int cli_regexp_repeat(const CliRegexpMatchIndexCtx *args);
int cli_regexp_count(const CliRegexpMatchThenCtx *args);
int cli_regexp_on_pass(const CliRegexpMatchThenCtx *args);
// a node walked from `at`, the place it stops handed to `then` (regexp_walk.c); where a pass of a one-way node that
// takes the text at `at` ends, or CLI_REGEXP_NO_PASS
int cli_regexp_walk(const CliRegexpMatchIndexCtx *args);
unsigned long long cli_regexp_pass(const CliRegexpMatchIndexAtCtx *args);

struct CliRegexpMatchIndexCtx
{
    const CliRegexp *regexp;
    unsigned int index;
    const CliRegexpThen *then;
    unsigned long long at;
    CliRegexpRun *run;
    unsigned int node;
    unsigned int count;
};

struct CliRegexpMatchThenCtx
{
    const CliRegexp *regexp;
    const CliRegexpThen *then;
    unsigned long long at;
    CliRegexpRun *run;
    const CliRegexpNode *repeat;
};

struct CliRegexpMatchIndexAtCtx
{
    const CliRegexp *regexp;
    unsigned int index;
    unsigned long long at;
    const CliRegexpRun *run_read;
};

struct CliRegexpMatchCtx
{
    unsigned int letter;
    const CliRegexp *regexp;
    unsigned int index;
};

struct CliRegexpMatchACtx
{
    const CliRegexp *regexp;
    unsigned int a;
    unsigned int b;
};

typedef struct
{
    int (*regexp_word)(const CliRegexpMatchCtx *args);
    int (*regexp_space)(const CliRegexpMatchCtx *args);
    int (*regexp_ends_line)(const CliRegexpMatchCtx *args);
    int (*regexp_one)(const CliRegexpMatchCtx *args);
} CliRegexpMatchNs;
EMBED_TABLE_LAYOUT(CliRegexpMatchNs, regexp_word, regexp_space, regexp_ends_line, regexp_one);
EMBED_TABLE_STORAGE CliRegexpMatchNs cli_regexp_match_internal EMBED_UNUSED = {
    .regexp_word = cli_regexp_word,
    .regexp_space = cli_regexp_space,
    .regexp_ends_line = cli_regexp_ends_line,
    .regexp_one = cli_regexp_one,
};

typedef struct
{
    int (*regexp_same)(const CliRegexpMatchACtx *args);
} CliRegexpMatchANs;
EMBED_TABLE_LAYOUT(CliRegexpMatchANs, regexp_same);
EMBED_TABLE_STORAGE CliRegexpMatchANs cli_regexp_match_a_internal EMBED_UNUSED = {
    .regexp_same = cli_regexp_same,
};

typedef struct
{
    int (*regexp_then)(const CliRegexpMatchIndexCtx *args);
    int (*regexp_chain)(const CliRegexpMatchIndexCtx *args);
    int (*regexp_repeat)(const CliRegexpMatchIndexCtx *args);
    int (*regexp_walk)(const CliRegexpMatchIndexCtx *args);
} CliRegexpMatchIndexNs;
EMBED_TABLE_LAYOUT(CliRegexpMatchIndexNs, regexp_then, regexp_chain, regexp_repeat, regexp_walk);
EMBED_TABLE_STORAGE CliRegexpMatchIndexNs cli_regexp_match_index_internal EMBED_UNUSED = {
    .regexp_then = cli_regexp_then,
    .regexp_chain = cli_regexp_chain,
    .regexp_repeat = cli_regexp_repeat,
    .regexp_walk = cli_regexp_walk,
};

typedef struct
{
    int (*regexp_count)(const CliRegexpMatchThenCtx *args);
    int (*regexp_pass)(const CliRegexpMatchThenCtx *args);
} CliRegexpMatchThenNs;
EMBED_TABLE_LAYOUT(CliRegexpMatchThenNs, regexp_count, regexp_pass);
EMBED_TABLE_STORAGE CliRegexpMatchThenNs cli_regexp_match_then_internal EMBED_UNUSED = {
    .regexp_count = cli_regexp_count,
    .regexp_pass = cli_regexp_on_pass,
};

typedef struct
{
    unsigned long long (*regexp_pass)(const CliRegexpMatchIndexAtCtx *args);
} CliRegexpMatchIndexAtNs;
EMBED_TABLE_LAYOUT(CliRegexpMatchIndexAtNs, regexp_pass);
EMBED_TABLE_STORAGE CliRegexpMatchIndexAtNs cli_regexp_match_index_at_internal EMBED_UNUSED = {
    .regexp_pass = cli_regexp_pass,
};

#endif
