// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_internal.h: what the review sources share: the files a change touched with their texts, and the pieces and
// the files named apart the comparison of them gives, guarded by the review's lock
#ifndef REVIEW_INTERNAL_H
#define REVIEW_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "review.h"

typedef struct CliReviewOneCtx CliReviewOneCtx;
typedef struct CliReviewLineCtx CliReviewLineCtx;
typedef struct CliReviewCtx CliReviewCtx;
typedef struct CliReviewTextLengthCtx CliReviewTextLengthCtx;
typedef struct CliReviewTextLengthTypeCtx CliReviewTextLengthTypeCtx;

// the max files a change reviewed touches and the bytes of their texts; the max pieces and the bytes of their
// texts; the bytes each text is read in; and the max calls the pieces make in all
#define CLI_REVIEW_FILES 1024u
#define CLI_REVIEW_TEXT (1u << 25u)
#define CLI_REVIEW_PIECES 8192u
#define CLI_REVIEW_TEXTS_MAX (1u << 25u)
#define CLI_REVIEW_TEXT_CAPACITY (16ull << 20u)
#define CLI_REVIEW_CALLS (1u << 18u)

// the max lines of a text the review reads
#define CLI_REVIEW_LINES (1u << 18u)

// A file a change touched: its path now and its path before, and its text before and now, each where it has one, as
// places in the texts
typedef struct
{
    unsigned int path;
    unsigned int was_path;
    unsigned int was;
    unsigned long long was_size;
    int has_was;
    unsigned int now;
    unsigned long long now_size;
    int has_now;
} CliReviewFile;

// A piece of the review: a declaration's file and name and type, how the change touched it, the file it moved from
// where that is another, its line, its text as it was and as it is, as places in the pieces' texts, and the pieces it
// calls, `count` of them from `first` in the calls
typedef struct
{
    unsigned int path;
    unsigned int name;
    unsigned int type;
    const char *how;
    int has_from;
    unsigned int from;
    unsigned int line;
    unsigned int was;
    unsigned int now;
    unsigned int first;
    unsigned int count;
} CliReviewPiece;

// A review: the files, their texts, the pieces, their texts and calls, and the files named apart; the texts and the
// calls are in one cell at their max, the comparison's capacity a cell of cli_review_compared_bytes(), each taken
// for one review under its lock and given back before the lock is
typedef struct
{
    void *compared;
    CliReviewFile file[CLI_REVIEW_FILES];
    unsigned int files;
    char *text;
    unsigned int used;
    unsigned long long text_capacity;
    CliReviewPiece piece[CLI_REVIEW_PIECES];
    unsigned int pieces;
    char *texts;
    unsigned int texts_used;
    unsigned long long texts_capacity;
    unsigned int *call;
    unsigned int calls;
    unsigned int other[CLI_REVIEW_FILES];
    unsigned int others;
} CliReview;

// the review read, and the cells the review occupies (review.c)
extern CliReview g_cli_review;

// `length` bytes put in the pieces' texts, a 0 after them: their place, 0 where they do not fit, an empty text there
// (review_compare.c)
unsigned int cli_review_piece_text_append(const CliReviewTextLengthCtx *args);
// `length` bytes put in the files' texts, a 0 after them: their place, 0 where they do not fit; a text written into a
// line put after the files' texts where it was written whole, and the line given back: 1, or 0; and a line a text the
// review reads is written into, grown to it and ended by cli_review_text_add (review_texts.c)
unsigned int cli_review_text_append(const CliReviewTextLengthCtx *args);
int cli_review_text_add(const CliReviewLineCtx *args);
void cli_review_text_line(CliGrownLine *line);

// the files compared: the pieces, sorted by path and line, each with the pieces it calls, and the files named apart,
// sorted, each once, in the comparison's capacity; and the bytes that capacity takes (review_compare.c)
void cli_review_compare(void);
unsigned long long cli_review_compared_bytes(void);

// `length` bytes of `text` with the whitespace at either end left off; the lines two texts contain alike, each line's
// whitespace at its ends left off, into `same`, and the lines of the longer into `longest`, two texts of no lines
// containing all alike: 1, or 0 where the cellblock could not meet the cell their lines are read into; and whether
// `text` calls `name`: the name, not part of a longer one, then an opening parenthesis (review_text.c)
void cli_review_trim(const CliReviewTextLengthTypeCtx *args);
int cli_review_share(const CliReviewOneCtx *args);
int cli_review_calls(const CliReviewCtx *args);

struct CliReviewOneCtx
{
    const char *one;
    const char *other;
    unsigned long long *same;
    unsigned long long *longest;
};

struct CliReviewLineCtx
{
    CliGrownLine *line;
    int written;
    unsigned int *at;
    unsigned long long *size;
};

struct CliReviewCtx
{
    const char *text;
    const char *name;
};

struct CliReviewTextLengthCtx
{
    const char *text;
    unsigned long long length;
};

struct CliReviewTextLengthTypeCtx
{
    const char **text_pointer;
    unsigned long long *length_pointer;
};

typedef struct
{
    unsigned int (*piece_text_append)(const CliReviewTextLengthCtx *args);
    unsigned int (*text_append)(const CliReviewTextLengthCtx *args);
} CliReviewTextLengthNs;
EMBED_TABLE_LAYOUT(CliReviewTextLengthNs, piece_text_append, text_append);
EMBED_TABLE_STORAGE CliReviewTextLengthNs cli_review_text_length_internal EMBED_UNUSED = {
    .piece_text_append = cli_review_piece_text_append,
    .text_append = cli_review_text_append,
};

typedef struct
{
    int (*text_add)(const CliReviewLineCtx *args);
} CliReviewLineNs;
EMBED_TABLE_LAYOUT(CliReviewLineNs, text_add);
EMBED_TABLE_STORAGE CliReviewLineNs cli_review_line_internal EMBED_UNUSED = {
    .text_add = cli_review_text_add,
};

typedef struct
{
    void (*trim)(const CliReviewTextLengthTypeCtx *args);
} CliReviewTextLengthTypeNs;
EMBED_TABLE_LAYOUT(CliReviewTextLengthTypeNs, trim);
EMBED_TABLE_STORAGE CliReviewTextLengthTypeNs cli_review_text_length_type_internal EMBED_UNUSED = {
    .trim = cli_review_trim,
};

typedef struct
{
    int (*share)(const CliReviewOneCtx *args);
} CliReviewOneNs;
EMBED_TABLE_LAYOUT(CliReviewOneNs, share);
EMBED_TABLE_STORAGE CliReviewOneNs cli_review_one_internal EMBED_UNUSED = {
    .share = cli_review_share,
};

typedef struct
{
    int (*calls)(const CliReviewCtx *args);
} CliReviewNsTable;
EMBED_TABLE_LAYOUT(CliReviewNsTable, calls);
EMBED_TABLE_STORAGE CliReviewNsTable cli_review_internal EMBED_UNUSED = {
    .calls = cli_review_calls,
};

#endif
