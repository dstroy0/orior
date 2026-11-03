// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_internal.h: what the fill's sources share: a line of the text by its bounds and its place, the lines written
// into the edit's text, the texts whose words are filled, and words filled into lines no wider than the margin
#ifndef FILL_INTERNAL_H
#define FILL_INTERNAL_H
#include "../text/text.h"
#include "embed_dispatch_layout.h"
#include "fill.h"

typedef struct CliFillFilledCtx CliFillFilledCtx;
typedef struct CliFillTextCtx CliFillTextCtx;
typedef struct CliFillTextFilledCtx CliFillTextFilledCtx;
typedef struct CliFillTextSizeCtx CliFillTextSizeCtx;
typedef struct CliFillSizeCtx CliFillSizeCtx;
typedef struct CliFillFilledSizeCtx CliFillFilledSizeCtx;
typedef struct CliFillCtx CliFillCtx;

// the most texts whose words one fill takes
#define CLI_FILL_SLICES (1u << 20u)

// A line of the text, split at each "\n": where it starts, where it ends, and its place among the lines
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    unsigned int index;
} CliFillLine;

// The lines written into the edit's text: where they go, where the last starts, and how many there are
typedef struct
{
    VerbaLinea *out;
    unsigned long long line_start;
    unsigned int lines;
} CliFilled;

// the texts whose words are filled, each where it starts and its size, a cell taken under the fill's lock with capacity
// for `g_cli_fill_slices` of them, each a line of the text or a place after the lines (fill.c)
extern const char **g_cli_fill_slice;
extern unsigned long long *g_cli_fill_slice_size;
extern unsigned int g_cli_fill_slices;

// the line at `index` of `size` bytes of `text`: 1, or 0 where it has none; the line before `line`, and the line after
// it, each into it: 1, or 0 where it has none (fill.c)
int cli_fill_line_at(const CliFillTextSizeCtx *args);
int cli_fill_before(const CliFillTextSizeCtx *args);
int cli_fill_after(const CliFillTextSizeCtx *args);

// a line begun after the last; the text of the last; a line of `size` bytes of `bytes` put whole; and the last taken
// off (fill.c)
void cli_filled_begin(const CliFillSizeCtx *args);
const char *cli_filled_last(const CliFillFilledSizeCtx *args);
void cli_filled_line(const CliFillSizeCtx *args);
void cli_filled_drop(const CliFillTextCtx *args);

// the words of the texts `slice` to `slice` and `count`, filled into lines no wider than `width` letters, the first
// starting with `first` and each after it with `rest`, each line's whitespace at its end taken off (fill.c)
void cli_fill_words(const CliFillFilledCtx *args);

// the edit that writes the lines `first` to `last` as the lines written (fill.c)
void cli_fill_edit_get(const CliFillTextFilledCtx *args);

// the comment fill's capacity, a cell taken under the fill's lock: 1, or 0 with why given; and the capacity given back
// before the lock is (fill_comment.c)
int cli_fill_comment_lock(const CliFillCtx *args);
void cli_fill_comment_release(void);

// the paragraph of the Python docstring around `at`, up to a blank line or a line of the closing quotes alone, the
// opening quotes placed at the head of its first line and the closing quotes at the end of its last: 1 with its edit,
// or 0 where `at` stands in none (fill_docstring.c)
int cli_fill_docstring(const CliFillTextCtx *args);

struct CliFillFilledCtx
{
    CliFilled *filled;
    unsigned int slice;
    unsigned int count;
    const char *first;
    unsigned long long first_size;
    const char *rest;
    unsigned long long remainder_size;
    unsigned int width;
};

struct CliFillTextCtx
{
    const char *text;
    unsigned long long size;
    CliFilled *filled;
    unsigned int width;
    CliTextEdit *edit;
    const CliFillLine *at;
};

struct CliFillTextFilledCtx
{
    const char *text;
    const CliFilled *filled_read;
    const CliFillLine *first_fill_line_pointer;
    const CliFillLine *last;
    CliTextEdit *edit;
};

struct CliFillTextSizeCtx
{
    const char *text;
    unsigned long long size;
    unsigned int index;
    CliFillLine *line;
};

struct CliFillSizeCtx
{
    unsigned long long size;
    CliFilled *filled;
    const char *bytes;
};

struct CliFillFilledSizeCtx
{
    const CliFilled *filled_read;
    unsigned long long *size_pointer;
};

struct CliFillCtx
{
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*line_at)(const CliFillTextSizeCtx *args);
    int (*before)(const CliFillTextSizeCtx *args);
    int (*after)(const CliFillTextSizeCtx *args);
} CliFillTextSizeNs;
EMBED_TABLE_LAYOUT(CliFillTextSizeNs, line_at, before, after);
EMBED_TABLE_STORAGE CliFillTextSizeNs cli_fill_text_size_internal EMBED_UNUSED = {
    .line_at = cli_fill_line_at,
    .before = cli_fill_before,
    .after = cli_fill_after,
};

typedef struct
{
    void (*filled_begin)(const CliFillSizeCtx *args);
    void (*filled_line)(const CliFillSizeCtx *args);
} CliFillSizeNs;
EMBED_TABLE_LAYOUT(CliFillSizeNs, filled_begin, filled_line);
EMBED_TABLE_STORAGE CliFillSizeNs cli_fill_size_internal EMBED_UNUSED = {
    .filled_begin = cli_filled_begin,
    .filled_line = cli_filled_line,
};

typedef struct
{
    const char *(*filled_last)(const CliFillFilledSizeCtx *args);
} CliFillFilledSizeNs;
EMBED_TABLE_LAYOUT(CliFillFilledSizeNs, filled_last);
EMBED_TABLE_STORAGE CliFillFilledSizeNs cli_fill_filled_size_internal EMBED_UNUSED = {
    .filled_last = cli_filled_last,
};

typedef struct
{
    void (*filled_drop)(const CliFillTextCtx *args);
    int (*docstring)(const CliFillTextCtx *args);
} CliFillTextNs;
EMBED_TABLE_LAYOUT(CliFillTextNs, filled_drop, docstring);
EMBED_TABLE_STORAGE CliFillTextNs cli_fill_text_internal EMBED_UNUSED = {
    .filled_drop = cli_filled_drop,
    .docstring = cli_fill_docstring,
};

typedef struct
{
    void (*words)(const CliFillFilledCtx *args);
} CliFillFilledNs;
EMBED_TABLE_LAYOUT(CliFillFilledNs, words);
EMBED_TABLE_STORAGE CliFillFilledNs cli_fill_filled_internal EMBED_UNUSED = {
    .words = cli_fill_words,
};

typedef struct
{
    void (*edit_get)(const CliFillTextFilledCtx *args);
} CliFillTextFilledNs;
EMBED_TABLE_LAYOUT(CliFillTextFilledNs, edit_get);
EMBED_TABLE_STORAGE CliFillTextFilledNs cli_fill_text_filled_internal EMBED_UNUSED = {
    .edit_get = cli_fill_edit_get,
};

typedef struct
{
    int (*comment_lock)(const CliFillCtx *args);
} CliFillNsTable;
EMBED_TABLE_LAYOUT(CliFillNsTable, comment_lock);
EMBED_TABLE_STORAGE CliFillNsTable cli_fill_internal EMBED_UNUSED = {
    .comment_lock = cli_fill_comment_lock,
};

#endif
