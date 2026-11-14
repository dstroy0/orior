// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_internal.h: what structure.c, which reads tokens and finds the shortest edit, and structure_compare.c,
// which compares two texts by them, share
#ifndef STRUCTURE_INTERNAL_H
#define STRUCTURE_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "structure.h"

typedef struct CliStructureACtx CliStructureACtx;
typedef struct CliStructureTextCtx CliStructureTextCtx;
typedef struct CliStructureCompareRenamesCtx CliStructureCompareRenamesCtx;
typedef struct CliStructureOneCtx CliStructureOneCtx;
typedef struct CliStructureOldCtx CliStructureOldCtx;
typedef struct CliStructureSideCtx CliStructureSideCtx;
typedef struct CliStructureCtx CliStructureCtx;

// the max tokens and lines a side contains, the max steps an edit takes, and the max texts the names number; a text
// past them reads as too far apart
#define CLI_STRUCTURE_TOKENS (1u << 16u)

#define CLI_STRUCTURE_LINES (1u << 16u)

#define CLI_STRUCTURE_STEPS (2u * CLI_STRUCTURE_TOKENS)

#define CLI_STRUCTURE_NAMES (1u << 18u)

// the max edits either comparison makes before the texts read as too far apart, and the fewest tokens a line contains
// for it to read as moved
#define CLI_MAX_EDITS 1500u

#define CLI_MOVED_TOKENS 2u

// no place: an index not found
#define CLI_STRUCTURE_NONE 0xffffffffu

// A token: its line, where it starts and ends in the line in UTF-16 units as the window counts, and where it is in
// the text in bytes
typedef struct
{
    unsigned int line;
    unsigned int from;
    unsigned int to;
    unsigned int start;
    unsigned int end;
} CliToken;

// One side: its text, its tokens, the first token of each line, each line's tokens running to the next line's first,
// and each token's number and each line's, the same for the same text in either side
typedef struct
{
    const char *text;
    CliToken token[CLI_STRUCTURE_TOKENS];
    unsigned int tokens;
    unsigned int line_first[CLI_STRUCTURE_LINES + 1u];
    unsigned int lines;
    unsigned int token_name[CLI_STRUCTURE_TOKENS];
    unsigned int line_name[CLI_STRUCTURE_LINES];
} CliSide;

// A step of an edit from one sequence to another: one unchanged, one taken out of the first, or one put in from the
// second
enum
{
    CLI_STEP_EQUAL,
    CLI_STEP_OUT,
    CLI_STEP_IN
};

typedef struct
{
    unsigned int type;
    unsigned int x;
    unsigned int y;
} CliStep;

// the tokens of `size` bytes of `text`, line by line, into the side: 1, or 0 where it contains more than a side stores
int cli_structure_lex(const CliStructureTextCtx *args);
// the shortest edit from `a` to `b`, the ends they share set aside first, into `steps`, sized for `capacity`: their
// count, or CLI_STRUCTURE_NONE past CLI_MAX_EDITS or past the capacity
unsigned int cli_structure_shortest(const CliStructureACtx *args);
// each token and each line of both sides numbered, the same text the same number: 1, or 0 past the names' capacity
int cli_structure_by_name(const CliStructureOldCtx *args);
// whether a token's text starts a name
int cli_structure_starts_name(const CliStructureSideCtx *args);

// One change: the lines taken out, `out_count` of them in the list of lines out from `out_first`, and those put in
typedef struct
{
    unsigned int out_first;
    unsigned int out_count;
    unsigned int in_first;
    unsigned int in_count;
} CliHunk;

// What one compare counts as it goes
typedef struct
{
    unsigned int rows;
    unsigned int starts;
    unsigned int hunks;
    unsigned int outs;
    unsigned int ins;
    unsigned int left_marks;
    unsigned int right_marks;
    unsigned int pairs;
    unsigned int touched;
    unsigned int runs_out;
    unsigned int runs_in;
    unsigned int moved;
    unsigned int reshaped;
} CliCompare;

// A bucket of the names of the texts of tokens and of the lines of tokens: a side, a place in it, and a number, 0
// where the bucket is empty
typedef struct
{
    const CliSide *side;
    unsigned int at;
    unsigned int name;
} CliNameBucket;

// Everything one compare stores, guarded by the compare's lock while it runs (structure_compare.c): with the band of
// Myers' frontier for each edit, and the names' buckets
typedef struct
{
    int frontier[(2u * CLI_MAX_EDITS) + 3u];
    int trace[(CLI_MAX_EDITS + 2u) * (CLI_MAX_EDITS + 2u)];
    CliNameBucket token_names[CLI_STRUCTURE_NAMES];
    CliNameBucket line_names[CLI_STRUCTURE_NAMES];
    CliSide old;
    CliSide new;
    CliStep line_steps[CLI_STRUCTURE_STEPS];
    CliStep token_steps[CLI_STRUCTURE_STEPS];
    unsigned int row_left[CLI_STRUCTURE_STEPS];
    unsigned int row_right[CLI_STRUCTURE_STEPS];
    unsigned int starts[CLI_STRUCTURE_STEPS];
    CliHunk hunks[CLI_STRUCTURE_STEPS];
    CliHunk touched[CLI_STRUCTURE_STEPS];
    unsigned int outs[CLI_STRUCTURE_LINES];
    unsigned int ins[CLI_STRUCTURE_LINES];
    unsigned char out_used[CLI_STRUCTURE_LINES];
    unsigned char left[CLI_STRUCTURE_LINES];
    unsigned char right[CLI_STRUCTURE_LINES];
    unsigned char left_types[CLI_STRUCTURE_LINES];
    unsigned char right_types[CLI_STRUCTURE_LINES];
    unsigned int left_marks[CLI_STRUCTURE_TOKENS][4];
    unsigned int right_marks[CLI_STRUCTURE_TOKENS][4];
    unsigned int out_tokens[CLI_STRUCTURE_TOKENS];
    unsigned int in_tokens[CLI_STRUCTURE_TOKENS];
    unsigned int a[CLI_STRUCTURE_TOKENS];
    unsigned int b[CLI_STRUCTURE_TOKENS];
    unsigned int run_out[CLI_STRUCTURE_TOKENS];
    unsigned int run_in[CLI_STRUCTURE_TOKENS];
    unsigned int pair_x[CLI_STRUCTURE_TOKENS];
    unsigned int pair_y[CLI_STRUCTURE_TOKENS];
    unsigned int became[(CLI_STRUCTURE_NAMES / 2u) + 1u];
    unsigned int became_count[(CLI_STRUCTURE_NAMES / 2u) + 1u];
    unsigned int became_from[(CLI_STRUCTURE_NAMES / 2u) + 1u];
    unsigned int became_to[(CLI_STRUCTURE_NAMES / 2u) + 1u];
    unsigned int renames[CLI_STRUCTURE_TOKENS];
} CliStructureState;

// the one compare's state, a cell taken with the compare's lock and given back with it (structure_compare.c)
extern CliStructureState *g_cli_structure;

// the steps of a compare past its edits (structure_marks.c): the names renamed found, in order, their count; the
// tokens of each moved line marked moved; each token of a change that is not moved marked; each line of a change given
// its type; two marks in order; and the compare written as the window reads it
unsigned int cli_structure_renames_find(const CliStructureCompareRenamesCtx *args);
void cli_structure_moved_mark(const CliStructureCtx *args);
void cli_structure_touched_mark(const CliStructureCtx *args);
void cli_structure_lines_type(const CliStructureCtx *args);
int cli_structure_mark_order(const CliStructureOneCtx *args);

struct CliStructureACtx
{
    const unsigned int *a;
    unsigned int n;
    const unsigned int *b;
    unsigned int m;
    CliStep *steps;
    unsigned int capacity;
};

struct CliStructureTextCtx
{
    const char *text;
    unsigned long long size;
    CliSide *side;
};

struct CliStructureCompareRenamesCtx
{
    const CliCompare *compare_read;
    unsigned int renames;
    VerbaLinea *out;
};

struct CliStructureOneCtx
{
    const void *one;
    const void *other;
};

struct CliStructureOldCtx
{
    CliSide *old;
    CliSide *now;
};

struct CliStructureSideCtx
{
    const CliSide *side_read;
    unsigned int token;
};

struct CliStructureCtx
{
    CliCompare *compare;
};

typedef struct
{
    int (*lex)(const CliStructureTextCtx *args);
} CliStructureTextNs;
EMBED_TABLE_LAYOUT(CliStructureTextNs, lex);
EMBED_TABLE_STORAGE CliStructureTextNs cli_structure_text_internal EMBED_UNUSED = {
    .lex = cli_structure_lex,
};

typedef struct
{
    unsigned int (*shortest)(const CliStructureACtx *args);
} CliStructureANs;
EMBED_TABLE_LAYOUT(CliStructureANs, shortest);
EMBED_TABLE_STORAGE CliStructureANs cli_structure_a_internal EMBED_UNUSED = {
    .shortest = cli_structure_shortest,
};

typedef struct
{
    int (*by_name)(const CliStructureOldCtx *args);
} CliStructureOldNs;
EMBED_TABLE_LAYOUT(CliStructureOldNs, by_name);
EMBED_TABLE_STORAGE CliStructureOldNs cli_structure_old_internal EMBED_UNUSED = {
    .by_name = cli_structure_by_name,
};

typedef struct
{
    int (*starts_name)(const CliStructureSideCtx *args);
} CliStructureSideNs;
EMBED_TABLE_LAYOUT(CliStructureSideNs, starts_name);
EMBED_TABLE_STORAGE CliStructureSideNs cli_structure_side_internal EMBED_UNUSED = {
    .starts_name = cli_structure_starts_name,
};

typedef struct
{
    unsigned int (*renames_find)(const CliStructureCompareRenamesCtx *args);
} CliStructureCompareRenamesNs;
EMBED_TABLE_LAYOUT(CliStructureCompareRenamesNs, renames_find);
EMBED_TABLE_STORAGE CliStructureCompareRenamesNs cli_structure_compare_renames_internal EMBED_UNUSED = {
    .renames_find = cli_structure_renames_find,
};

typedef struct
{
    void (*moved_mark)(const CliStructureCtx *args);
    void (*touched_mark)(const CliStructureCtx *args);
    void (*lines_type)(const CliStructureCtx *args);
} CliStructureNsTable;
EMBED_TABLE_LAYOUT(CliStructureNsTable, moved_mark, touched_mark, lines_type);
EMBED_TABLE_STORAGE CliStructureNsTable cli_structure_internal EMBED_UNUSED = {
    .moved_mark = cli_structure_moved_mark,
    .touched_mark = cli_structure_touched_mark,
    .lines_type = cli_structure_lines_type,
};

typedef struct
{
    int (*mark_order)(const CliStructureOneCtx *args);
} CliStructureOneNs;
EMBED_TABLE_LAYOUT(CliStructureOneNs, mark_order);
EMBED_TABLE_STORAGE CliStructureOneNs cli_structure_one_internal EMBED_UNUSED = {
    .mark_order = cli_structure_mark_order,
};

#endif
