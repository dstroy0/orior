// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_internal.h: what the pages sources share: the paged texts, each with its index, its pieces and the lines
// written over it, and the lines a reading of them gives, under the pages' lock
#ifndef PAGES_INTERNAL_H
#define PAGES_INTERNAL_H

#include "pages.h"

// how many lines apart the index records the byte a line starts at, and the most it records; how many lines find and a
// save read at a time; the most pieces of a text; the most lines and bytes one reading gives; and the most bytes of
// text find searches at a time, its units and its search each taking cells of several times that
#define CLI_PAGE_STEP 1024ull
#define CLI_PAGE_STARTS (1u << 20u)
#define CLI_PAGE_BLOCK 16384ull
#define CLI_PAGE_PIECES 65536u
#define CLI_PAGE_READ_LINES (1u << 17u)
#define CLI_PAGE_READ_TEXT (1u << 26u)
#define CLI_PAGE_FIND_TEXT (1u << 23u)

// the end of the shadow's name, after a dot and the file's name, beside the file
#define CLI_PAGE_SHADOW ".~"

// The index of a file's lines as far as it has been read: the byte each CLI_PAGE_STEP-th line starts at, a cell of
// `starts_most` of them sized to the file and no more than CLI_PAGE_STARTS, how many lines there are, whether it is
// whole, whether its reading is going on or is to stop, and the file it reads
typedef struct
{
    unsigned long long *start;
    unsigned long long starts_most;
    unsigned long long starts;
    unsigned long long lines;
    int done;
    int reading;
    int cancel;
    char path[CLI_PATH_MAX_LEN];
    CliThread reader;
} CliPageIndex;

// A run of lines: of the file, from a line to before another, `open` for to the end of the file however many lines
// the index finds; or `lines` lines written in the shadow, from its byte `from` to before its byte `to`, each ended
// by "\n"
typedef struct
{
    int written;
    unsigned long long from;
    unsigned long long to;
    int open;
    unsigned long long lines;
} CliPagePiece;

// A paged text, a cell of its own, taken as it is opened and given back once it is let go and its index has stopped,
// as many stored as the cells meet: the next stored, its key, its file and the shadow beside it the lines written over
// it are put after, the shadow's bytes, the file's size and the end of line its first line ends with, its index, and
// its pieces, a cell of CLI_PAGE_PIECES
typedef struct CliPaged
{
    struct CliPaged *next;
    char key[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char shadow[CLI_PATH_MAX_LEN];
    unsigned long long shadow_used;
    unsigned long long size;
    const char *eol;
    CliPageIndex index;
    CliPagePiece *piece;
    unsigned int pieces;
} CliPaged;

// Lines a reading of a text gives, each where it starts in the text and its bytes: one cell containing the starts, the
// lengths and the text, sized to the lines requested and the bytes read and moved into one of twice the room as either
// fills, up to CLI_PAGE_READ_LINES lines and CLI_PAGE_READ_TEXT bytes, past which the reading is full
typedef struct
{
    unsigned char *cell;
    unsigned long long *start;
    unsigned long long *length;
    unsigned long long lines_most;
    unsigned long long count;
    char *text;
    unsigned long long text_most;
    unsigned long long used;
    int full;
} CliPageLines;

// the lines stored in a cell of `lines_first` lines and `text_first` bytes to begin with: 1, or 0 with why given; the
// lines emptied, their cell retained; room for `lines_more` lines and `text_more` bytes, the cell moved into one of
// twice the room where they do not fit: 1, or 0 with the lines full; a line of `size` bytes put after the lines, the
// lines full where it does not fit; and the cell given back (pages_lines.c)
int cli_page_lines_reserved(CliPageLines *lines, unsigned long long lines_first, unsigned long long text_first,
                            VerbaLinea *err_msg);
void cli_page_lines_emptied(CliPageLines *lines);
int cli_page_lines_room(CliPageLines *lines, unsigned long long lines_more, unsigned long long text_more);
void cli_page_lines_put(CliPageLines *lines, const char *bytes, unsigned long long size);
void cli_page_lines_given(CliPageLines *lines);
// the lines of `one` from `from`, up to `*block_most` of them and none past `total`, read whole into `lines`,
// `*block_most` halved while a reading of that many is full, the line after the last read into `to`: 1, or 0 with why
// given, a single line a reading cannot contain among the whys (pages_lines.c)
int cli_page_block_read(CliPaged *one, unsigned long long from, unsigned long long total,
                        unsigned long long *block_most, unsigned long long *to, CliPageLines *lines,
                        VerbaLinea *err_msg);

// the first of the paged texts stored and the lock they and their pieces are read and written under; and the lock every
// index is read and written under, and its wake
extern CliPaged *g_cli_paged;
extern CliLock g_cli_pages_lock;
extern CliLock g_cli_index_lock;
extern CliWake g_cli_index_wake;

// the paged text stored under `key`, or NULL with why given; how many lines the index has read; how many lines a piece
// contains, as far as the index has read; the text's lines from `from` to before `to`, as far as it contains them, into
// `lines`: 1, or 0 with why given; the text's shape written; and the file at `path` opened as the text `key`, its index
// read behind where `behind` is set and whole before it answers where not: 1, or 0 with why given. The pages' lock is
// acquired around each (pages.c)
CliPaged *cli_paged_of(const char *key, VerbaLinea *err_msg);
unsigned long long cli_page_indexed(CliPaged *one);
unsigned long long cli_page_length(const CliPagePiece *piece, unsigned long long indexed);
int cli_page_lines_of(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines,
                      VerbaLinea *err_msg);
void cli_page_shape_written(CliPaged *one, VerbaLinea *out);
int cli_page_open_reading(const char *key, const char *path, int behind, VerbaLinea *out, VerbaLinea *err_msg);

// the path beside `path` named a dot, its file's name and `ending`; and the byte of the shadow line `line` of a written
// piece of `one` starts at, its end past its last line (pages_shadow.c)
void cli_page_beside(const char *path, const char *ending, char *beside);
unsigned long long cli_page_written_at(const CliPaged *one, const CliPagePiece *piece, unsigned long long line);

#endif
