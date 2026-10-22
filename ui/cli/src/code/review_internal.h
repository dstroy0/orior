// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_internal.h: what the review sources share: the files a change touched with their texts, and the pieces and
// the files named apart the comparison of them gives, held under the review's lock
#ifndef REVIEW_INTERNAL_H
#define REVIEW_INTERNAL_H

#include "review.h"

// the most files a change reviewed touches and the bytes of their texts; the most pieces and the bytes of their
// texts; the bytes either texts' room is begun with, doubling as they are put; and the most calls the pieces make in
// all, and the calls their room is begun with, doubling as they are found
#define CLI_REVIEW_FILES 1024u
#define CLI_REVIEW_TEXT (1u << 26u)
#define CLI_REVIEW_PIECES 16384u
#define CLI_REVIEW_KEPT (1u << 26u)
#define CLI_REVIEW_TEXT_FIRST 65536u
#define CLI_REVIEW_CALLS (1u << 20u)
#define CLI_REVIEW_CALLS_FIRST 256u

// the most lines of a text the review reads
#define CLI_REVIEW_LINES (1u << 20u)

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

// A piece of the review: a declaration's file and name and kind, how the change touched it, the file it moved from
// where that is another, its line, its text as it was and as it is, as places in the pieces' texts, and the pieces it
// calls, `count` of them from `first` in the calls
typedef struct
{
    unsigned int path;
    unsigned int name;
    unsigned int kind;
    const char *how;
    int has_from;
    unsigned int from;
    unsigned int line;
    unsigned int was;
    unsigned int now;
    unsigned int first;
    unsigned int count;
} CliReviewPiece;

// A review: the files, their texts, the pieces, their texts and calls, and the files named apart; the files' and the
// pieces' texts are cells of `text_room` and `kept_room` bytes that double as texts are put, to CLI_REVIEW_TEXT and
// CLI_REVIEW_KEPT at the most, the comparison's room a cell of cli_review_compared_bytes(), and the calls a cell of
// `call_room` that doubles as they are found, each taken for one review under its lock and given back before the lock
// is
typedef struct
{
    void *compared;
    CliReviewFile file[CLI_REVIEW_FILES];
    unsigned int files;
    char *text;
    unsigned int used;
    unsigned long long text_room;
    CliReviewPiece piece[CLI_REVIEW_PIECES];
    unsigned int pieces;
    char *kept;
    unsigned int kept_used;
    unsigned long long kept_room;
    unsigned int *call;
    unsigned int calls;
    unsigned int call_room;
    unsigned int other[CLI_REVIEW_FILES];
    unsigned int others;
} CliReview;

// the review read, and the cells the review holds (review.c)
extern CliReview g_cli_review;
extern CliCellsTally g_cli_review_cells;

// `length` bytes put in the pieces' texts, a 0 after them: their place, 0 where they do not fit, an empty text there
// (review_compare.c)
unsigned int cli_review_kept(const char *text, unsigned long long length);
// the room `*room` of `*size` bytes, `used` of them taken, made to hold `more` bytes past them, moved into a cell of
// twice its size, or as many times twice as they need, where they do not fit, no more than `most`: 1, or 0 where they
// cannot be met;
// `length` bytes put in the files' texts, a 0 after them: their place, 0 where they do not fit; a text written into a
// growing line put after the files' texts where it was written whole, and the line given back: 1, or 0; and a growing
// line a text the review reads is written into, given back by cli_review_text_kept (review_texts.c)
int cli_review_room(char **room, unsigned long long *size, unsigned long long used, unsigned long long more,
                    unsigned long long most, const char *what);
unsigned int cli_review_text_put(const char *text, unsigned long long length);
int cli_review_text_kept(CliLine *line, int written, unsigned int *at, unsigned long long *size);
CliLine cli_review_text_line(void);

// the files compared: the pieces, sorted by path and line, each with the pieces it calls, and the files named apart,
// sorted, each once, in the comparison's room; and the bytes that room takes (review_compare.c)
void cli_review_compare(void);
unsigned long long cli_review_compared_bytes(void);

// `length` bytes of `text` with the whitespace at either end left off; the lines two texts hold alike, each line's
// whitespace at its ends left off, into `same`, and the lines of the longer into `longest`, two texts of no lines
// holding all alike: 1, or 0 where the cellblock could not meet the cell their lines are read into; and whether
// `text` calls `name`: the name, not part of a longer one, then an opening parenthesis (review_text.c)
void cli_review_trimmed(const char **text, unsigned long long *length);
int cli_review_share(const char *one, const char *other, unsigned long long *same, unsigned long long *longest);
int cli_review_calls(const char *text, const char *name);

#endif
