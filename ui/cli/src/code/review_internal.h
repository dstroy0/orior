// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_internal.h: what the review sources share: the files a change touched with their texts, and the pieces and
// the files named apart the comparison of them gives, held under the review's lock
#ifndef REVIEW_INTERNAL_H
#define REVIEW_INTERNAL_H

#include "review.h"

// the most files a change reviewed touches and the bytes of their texts; the most pieces and the bytes of their
// texts; and the most calls the pieces make in all
#define CLI_REVIEW_FILES 1024u
#define CLI_REVIEW_TEXT (1u << 26u)
#define CLI_REVIEW_PIECES 16384u
#define CLI_REVIEW_KEPT (1u << 26u)
#define CLI_REVIEW_CALLS (1u << 20u)

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
// pieces' texts are cells of CLI_REVIEW_TEXT and CLI_REVIEW_KEPT, and the comparison's room a cell of
// cli_review_compared_bytes(), each taken for one review under its lock and given back before the lock is
typedef struct
{
    void *compared;
    CliReviewFile file[CLI_REVIEW_FILES];
    unsigned int files;
    char *text;
    unsigned int used;
    CliReviewPiece piece[CLI_REVIEW_PIECES];
    unsigned int pieces;
    char *kept;
    unsigned int kept_used;
    unsigned int call[CLI_REVIEW_CALLS];
    unsigned int calls;
    unsigned int other[CLI_REVIEW_FILES];
    unsigned int others;
} CliReview;

extern CliReview g_cli_review;

// `length` bytes put in the pieces' texts, a 0 after them: their place, 0 where they do not fit, an empty text there
// (review_compare.c)
unsigned int cli_review_kept(const char *text, unsigned long long length);

// the files compared: the pieces, sorted by path and line, each with the pieces it calls, and the files named apart,
// sorted, each once, in the comparison's room; and the bytes that room takes (review_compare.c)
void cli_review_compare(void);
unsigned long long cli_review_compared_bytes(void);

// `length` bytes of `text` with the whitespace at either end left off; the lines two texts hold alike, each line's
// whitespace at its ends left off, into `same`, and the lines of the longer into `longest`, two texts of no lines
// holding all alike; and whether `text` calls `name`: the name, not part of a longer one, then an opening
// parenthesis (review_text.c)
void cli_review_trimmed(const char **text, unsigned long long *length);
void cli_review_share(const char *one, const char *other, unsigned long long *same, unsigned long long *longest);
int cli_review_calls(const char *text, const char *name);

#endif
