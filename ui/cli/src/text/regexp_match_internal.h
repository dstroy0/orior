// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_match_internal.h: what matching shares across regexp_match.c and regexp_walk.c: a match's run, the frames
// that hand a place on, and the steps of matching one calls in another
#ifndef REGEXP_MATCH_INTERNAL_H
#define REGEXP_MATCH_INTERNAL_H

#include "regexp.h"

#include <stdint.h>

// the most passes' ends the one-way repeats standing at once keep, and a pass that took nothing
#define CLI_REGEXP_ENDS_MOST (1u << 22u)

#define CLI_REGEXP_NO_PASS 0xffffffffffffffffull

// Where a match stands: the text and its length, each group's span, the steps taken, where the match's end was
// handed, and the address its stack stood at as it began
typedef struct
{
    const unsigned int *text;
    unsigned long long length;
    CliRegexpSpan *groups;
    unsigned long long steps;
    unsigned long long end;
    uintptr_t base;
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
// character node's holding of one letter, -1 for a node that takes no one letter; and a letter a first set allows
int cli_regexp_word(unsigned int letter);
int cli_regexp_space(unsigned int letter);
int cli_regexp_ends_line(unsigned int letter);
int cli_regexp_same(const CliRegexp *regexp, unsigned int a, unsigned int b);
int cli_regexp_one(const CliRegexp *regexp, unsigned int index, unsigned int letter);
int cli_regexp_allows(const CliRegexpFirst *first, unsigned int letter);
// the steps (regexp_match.c): what comes after handed a place; the rest of a sequence; a repetition at a pass; a repeat
// over one character, and one whose pass takes the text one way or none
int cli_regexp_then(const CliRegexp *regexp, const CliRegexpThen *then, unsigned long long at, CliRegexpRun *run);
int cli_regexp_chain(const CliRegexp *regexp, unsigned int node, unsigned int index, unsigned long long at,
                     CliRegexpRun *run, const CliRegexpThen *then);
int cli_regexp_repeat(const CliRegexp *regexp, unsigned int node, unsigned int count, unsigned long long at,
                      CliRegexpRun *run, const CliRegexpThen *then);
int cli_regexp_counted(const CliRegexp *regexp, const CliRegexpNode *repeat, unsigned long long at, CliRegexpRun *run,
                       const CliRegexpThen *then);
int cli_regexp_passed(const CliRegexp *regexp, const CliRegexpNode *repeat, unsigned long long at, CliRegexpRun *run,
                      const CliRegexpThen *then);
// a node walked from `at`, the place it stops handed to `then` (regexp_walk.c); where a pass of a one-way node that
// takes the text at `at` ends, or CLI_REGEXP_NO_PASS
int cli_regexp_walk(const CliRegexp *regexp, unsigned int node, unsigned long long at, CliRegexpRun *run,
                    const CliRegexpThen *then);
unsigned long long cli_regexp_pass(const CliRegexp *regexp, unsigned int node, const CliRegexpRun *run,
                                   unsigned long long at);

#endif
