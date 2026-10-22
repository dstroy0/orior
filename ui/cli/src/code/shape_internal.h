// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_internal.h: what the structural search's sources share: the tokens of a text, each its kind and its bytes; the
// placeholders of a pattern; and the match of the pattern's parts against the code's tokens
#ifndef SHAPE_INTERNAL_H
#define SHAPE_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/text.h"
#include "shape.h"

// the kinds of a token: a name, a number, a string, an operator or a bracket, and in Python the end of a statement at a
// line's end
#define CLI_SHAPE_NAME 0u
#define CLI_SHAPE_NUMBER 1u
#define CLI_SHAPE_STR 2u
#define CLI_SHAPE_PUNCT 3u
#define CLI_SHAPE_BREAK 4u

// what a placeholder stands for: one name, an expression, or statements
#define CLI_SHAPE_HOLE_NAME 0u
#define CLI_SHAPE_HOLE_EXPR 1u
#define CLI_SHAPE_HOLE_STMT 2u

// the most tokens of the code, of a pattern, and placeholders of a pattern
#define CLI_SHAPE_TOKENS (1u << 22u)
#define CLI_SHAPE_PATTERN 4096u
#define CLI_SHAPE_HOLES 256u

// The tokens of a text, in the storage its holder gives: each its kind and where it starts and ends; 1 in `full` where
// one did not fit
typedef struct
{
    unsigned char *kind;
    unsigned int *start;
    unsigned int *end;
    unsigned int count;
    unsigned int most;
    int full;
} CliShapeTokens;

// A placeholder of a pattern: the token it stands as, its name, and what it stands for
typedef struct
{
    unsigned int token;
    const char *name;
    unsigned long long name_size;
    unsigned int hole;
} CliShapeHole;

// The holes of a pattern
typedef struct
{
    CliShapeHole hole[CLI_SHAPE_HOLES];
    unsigned int count;
} CliShapeHoles;

// the tokens of `size` bytes of `src` in `language` into `tokens`, and, where `holes` is not NULL, each placeholder as
// a name token whose place is kept there (shape.c)
void cli_shape_tokens(const char *language, const char *src, unsigned long long size, CliShapeTokens *tokens,
                      CliShapeHoles *holes);

// The match of a pattern against the code: the code and its tokens; the pattern and its tokens, each part a word to
// match as written or the placeholder at `hole_of`, CLI_SHAPE_HOLES for none; the steps taken; the placeholders
// bound so far, each its name and the tokens it took; and the stack of ends tried, `ends_top` of them in a cell of
// `ends_room`, grown as ends are put and given back with cli_shape_ends_given
typedef struct
{
    const char *src;
    const CliShapeTokens *tokens;
    const char *pattern;
    const CliShapeTokens *parts;
    const unsigned int *hole_of;
    const CliShapeHoles *holes;
    unsigned int part_count;
    unsigned int steps;
    unsigned int bound[CLI_SHAPE_HOLES];
    unsigned int bound_from[CLI_SHAPE_HOLES];
    unsigned int bound_to[CLI_SHAPE_HOLES];
    unsigned int bound_count;
    unsigned int *ends;
    unsigned int ends_top;
    unsigned int ends_room;
} CliShapeMatcher;

// the cells a search holds while it runs, its room, its ends tried and a match's template written (shape_search.c)
extern CliCellsTally g_cli_shape_cells;

// the matcher's stack of ends given back (shape_match.c)
void cli_shape_ends_given(CliShapeMatcher *matcher);

// the end of a match of the parts from `part` on at the token `at`, the placeholders bound so far kept bound: 1 with it
// into `end`, or 0 where none matches (shape_match.c)
int cli_shape_from(CliShapeMatcher *matcher, unsigned int part, unsigned int at, unsigned int *end);

// the bound placeholder named `size` bytes of `name`: its place among the bound, CLI_SHAPE_HOLES where none is
// (shape_match.c)
unsigned int cli_shape_bound(const CliShapeMatcher *matcher, const char *name, unsigned long long size);

#endif
