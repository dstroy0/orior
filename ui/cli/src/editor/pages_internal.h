// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_internal.h: what the pages sources share: the paged texts, each with its index, its pieces and the lines
// written over it, and the lines a reading of them gives, under the pages' lock
#ifndef PAGES_INTERNAL_H
#define PAGES_INTERNAL_H
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"
#include "pages.h"

typedef struct CliPagesLinesCtx CliPagesLinesCtx;
typedef struct CliPagesLinesErrMsgCtx CliPagesLinesErrMsgCtx;
typedef struct CliPagesErrMsgCtx CliPagesErrMsgCtx;
typedef struct CliPagesLinesLinesFirstCtx CliPagesLinesLinesFirstCtx;
typedef struct CliPagesPathCtx CliPagesPathCtx;
typedef struct CliPagesLinesBytesCtx CliPagesLinesBytesCtx;
typedef struct CliPagesLinesLinesMoreCtx CliPagesLinesLinesMoreCtx;
typedef struct CliPagesPieceCtx CliPagesPieceCtx;
typedef struct CliPagesCtx CliPagesCtx;
typedef struct CliPagesOneCtx CliPagesOneCtx;

// how many lines apart the index records the byte a line starts at, and how many bytes apart at max, so a reading
// of a file of long lines starts near the lines it reads; the max starts it records; how many lines find and a save
// read at a time; the max pieces of a text; the max lines and bytes one reading gives; and the max bytes of text
// find searches at a time, its units and its search each taking cells of several times that
#define CLI_PAGE_STEP 1024ull
#define CLI_PAGE_SPAN (64ull * 1024ull)
#define CLI_PAGE_STARTS (1u << 18u)
#define CLI_PAGE_BLOCK 16384ull
#define CLI_PAGE_PIECES 16384u
#define CLI_PAGE_READ_LINES (1u << 15u)
#define CLI_PAGE_READ_TEXT (1u << 24u)
#define CLI_PAGE_FIND_TEXT (1u << 22u)

// the end of the shadow's name, after a dot and the file's name, beside the file
#define CLI_PAGE_SHADOW ".~"

// A view's placing (cli_pages_on_view): the lines about a moving view's rows it is answered, and one more for each
// SPREAD lines of the step it moved, as far as its next step can differ from the last, no more than the lines a step
// reads; the lines about a resting view's rows and the minimap's it saves; the bytes of whole lines a step toward those
// reads, 16 lines at the least; the max lines it saves; and the milliseconds within which a report follows the last
// for the view to be moving at the step between them
#define CLI_PAGE_EDGE 8ull
#define CLI_PAGE_SPREAD 4ull
#define CLI_PAGE_ADJACENT 100ull
#define CLI_PAGE_REMAINDER_BYTES (128ull * 1024ull)
#define CLI_PAGE_REMAINDER_MIN 16ull
#define CLI_PAGE_SAVED 2000ull
#define CLI_PAGE_STILL 100ull

// The index of a file's lines as far as it has been read: the byte a line starts at, recorded CLI_PAGE_STEP lines or
// CLI_PAGE_SPAN bytes after the one recorded before, whichever comes first, and the line each is, in order, each a
// movable cell of `starts_max` sized to the file and no more than CLI_PAGE_STARTS; how many lines there are, whether it
// is whole,
// whether its reading is going on or is to stop, and the file it reads
typedef struct
{
    const CarcerMovableEntry *start;
    const CarcerMovableEntry *line;
    unsigned long long starts_max;
    unsigned long long starts;
    unsigned long long lines;
    int done;
    int read_state;
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
// it are put after, the shadow's bytes, the file's size and the end of line its first line ends with, its index, its
// pieces in a movable cell sized to them, no more than CLI_PAGE_PIECES, and the view the page reported last: whether
// it has reported one, the line at its top and the millisecond it was reported
// the max paged texts open at once
#define CLI_PAGED_MAX 64u

typedef struct CliPaged
{
    int reserved;
    struct CliPaged *next;
    char key[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char shadow[CLI_PATH_MAX_LEN];
    unsigned long long shadow_used;
    unsigned long long size;
    const char *eol;
    CliPageIndex index;
    const CarcerMovableEntry *piece;
    unsigned int pieces;
    int viewed;
    unsigned long long view_top;
    unsigned long long view_at;
} CliPaged;

// Lines a reading of a text gives, each where it starts in the text and its bytes: one cell containing the starts, the
// lengths and the text, sized to the lines requested and the bytes they take, up to CLI_PAGE_READ_LINES lines and
// CLI_PAGE_READ_TEXT bytes, past which the reading is full
typedef struct
{
    unsigned char *cell;
    unsigned long long *start;
    unsigned long long *length;
    unsigned long long lines_max;
    unsigned long long count;
    char *text;
    unsigned long long text_max;
    unsigned long long used;
    int full;
} CliPageLines;

// the lines stored in a cell of `lines_first` lines and `text_first` bytes: 1, or 0 with why given; the lines emptied,
// their cell retained; whether `lines_more` lines and `text_more` bytes fit: 1, or 0 with the lines full; a line of
// `size` bytes put after the lines, the lines full where it does not fit; and the cell given back (pages_lines.c)
int cli_page_lines_reserve(const CliPagesLinesLinesFirstCtx *args);
void cli_page_lines_clear(const CliPagesLinesLinesFirstCtx *args);
int cli_page_lines_capacity(const CliPagesLinesLinesMoreCtx *args);
void cli_page_lines_append(const CliPagesLinesBytesCtx *args);
void cli_page_lines_free(const CliPagesLinesLinesFirstCtx *args);
// the lines of `one` from `from`, up to `*block_max` of them and none past `total`, read whole into `lines`,
// `*block_max` halved while a reading of that many is full, the line after the last read into `to`: 1, or 0 with why
// given, a single line a reading cannot contain among the whys (pages_lines.c)
int cli_page_block_read(const CliPagesLinesCtx *args);

// the first of the paged texts stored and the lock they and their pieces are read and written under; and the lock every
// index is read and written under, and its wake
extern CliPaged *g_cli_paged;
extern CliWake g_cli_index_wake;

// the paged text stored under `key`, or NULL with why given; how many lines the index has read; how many lines a piece
// contains, as far as the index has read; the text's lines from `from` to before `to`, as far as it contains them, into
// `lines`: 1, or 0 with why given; the text's shape written; and the file at `path` opened as the text `key`, its index
// read behind where `behind` is set and whole before it answers where not: 1, or 0 with why given. The pages' lock is
// acquired around each (pages.c)
CliPaged *cli_paged_get(const CliPagesErrMsgCtx *args);
unsigned long long cli_page_index(const CliPagesOneCtx *args);
unsigned long long cli_page_length(const CliPagesCtx *args);
int cli_page_lines_get(const CliPagesLinesErrMsgCtx *args);
void cli_page_shape_write(const CliPagesOneCtx *args);
int cli_page_open_read_state(const CliPagesErrMsgCtx *args);

// the path beside `path` named a dot, its file's name and `ending`; and the byte of the shadow line `line` of a written
// piece of `one` starts at, its end past its last line (pages_shadow.c)
void cli_page_adjacent(const CliPagesPathCtx *args);
unsigned long long cli_page_write_at(const CliPagesPieceCtx *args);

struct CliPagesLinesCtx
{
    CliPageLines *lines;
    VerbaLinea *err_msg;
    CliPaged *one;
    unsigned long long from;
    unsigned long long total;
    unsigned long long *block_max;
    unsigned long long *to;
};

struct CliPagesLinesErrMsgCtx
{
    CliPageLines *lines;
    VerbaLinea *err_msg;
    CliPaged *one;
    unsigned long long from;
    unsigned long long to_value;
};

struct CliPagesErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *key;
    VerbaLinea *out;
    const char *path;
    int behind;
};

struct CliPagesLinesLinesFirstCtx
{
    CliPageLines *lines;
    unsigned long long lines_first;
    unsigned long long text_first;
    VerbaLinea *err_msg;
};

struct CliPagesPathCtx
{
    const char *path;
    const char *ending;
    char *adjacent;
};

struct CliPagesLinesBytesCtx
{
    CliPageLines *lines;
    const char *bytes;
    unsigned long long size;
};

struct CliPagesLinesLinesMoreCtx
{
    CliPageLines *lines;
    unsigned long long lines_more;
    unsigned long long text_more;
};

struct CliPagesPieceCtx
{
    const CliPagePiece *piece;
    const CliPaged *one_read;
    unsigned long long line;
};

struct CliPagesCtx
{
    const CliPagePiece *piece;
    unsigned long long indexed;
};

struct CliPagesOneCtx
{
    CliPaged *one;
    VerbaLinea *out;
};

typedef struct
{
    int (*page_lines_reserve)(const CliPagesLinesLinesFirstCtx *args);
    void (*page_lines_clear)(const CliPagesLinesLinesFirstCtx *args);
    void (*page_lines_free)(const CliPagesLinesLinesFirstCtx *args);
} CliPagesLinesLinesFirstNs;
EMBED_TABLE_LAYOUT(CliPagesLinesLinesFirstNs, page_lines_reserve, page_lines_clear, page_lines_free);
EMBED_TABLE_STORAGE CliPagesLinesLinesFirstNs cli_pages_lines_lines_first_internal EMBED_UNUSED = {
    .page_lines_reserve = cli_page_lines_reserve,
    .page_lines_clear = cli_page_lines_clear,
    .page_lines_free = cli_page_lines_free,
};

typedef struct
{
    int (*page_lines_capacity)(const CliPagesLinesLinesMoreCtx *args);
} CliPagesLinesLinesMoreNs;
EMBED_TABLE_LAYOUT(CliPagesLinesLinesMoreNs, page_lines_capacity);
EMBED_TABLE_STORAGE CliPagesLinesLinesMoreNs cli_pages_lines_lines_more_internal EMBED_UNUSED = {
    .page_lines_capacity = cli_page_lines_capacity,
};

typedef struct
{
    void (*page_lines_append)(const CliPagesLinesBytesCtx *args);
} CliPagesLinesBytesNs;
EMBED_TABLE_LAYOUT(CliPagesLinesBytesNs, page_lines_append);
EMBED_TABLE_STORAGE CliPagesLinesBytesNs cli_pages_lines_bytes_internal EMBED_UNUSED = {
    .page_lines_append = cli_page_lines_append,
};

typedef struct
{
    int (*page_block_read)(const CliPagesLinesCtx *args);
} CliPagesLinesNsTable;
EMBED_TABLE_LAYOUT(CliPagesLinesNsTable, page_block_read);
EMBED_TABLE_STORAGE CliPagesLinesNsTable cli_pages_lines_internal EMBED_UNUSED = {
    .page_block_read = cli_page_block_read,
};

typedef struct
{
    CliPaged *(*paged_get)(const CliPagesErrMsgCtx *args);
    int (*page_open_read_state)(const CliPagesErrMsgCtx *args);
} CliPagesErrMsgNs;
EMBED_TABLE_LAYOUT(CliPagesErrMsgNs, paged_get, page_open_read_state);
EMBED_TABLE_STORAGE CliPagesErrMsgNs cli_pages_err_msg_internal EMBED_UNUSED = {
    .paged_get = cli_paged_get,
    .page_open_read_state = cli_page_open_read_state,
};

typedef struct
{
    unsigned long long (*page_index)(const CliPagesOneCtx *args);
    void (*page_shape_write)(const CliPagesOneCtx *args);
} CliPagesOneNs;
EMBED_TABLE_LAYOUT(CliPagesOneNs, page_index, page_shape_write);
EMBED_TABLE_STORAGE CliPagesOneNs cli_pages_one_internal EMBED_UNUSED = {
    .page_index = cli_page_index,
    .page_shape_write = cli_page_shape_write,
};

typedef struct
{
    unsigned long long (*page_length)(const CliPagesCtx *args);
} CliPagesNsTable;
EMBED_TABLE_LAYOUT(CliPagesNsTable, page_length);
EMBED_TABLE_STORAGE CliPagesNsTable cli_pages_internal EMBED_UNUSED = {
    .page_length = cli_page_length,
};

typedef struct
{
    int (*page_lines_get)(const CliPagesLinesErrMsgCtx *args);
} CliPagesLinesErrMsgNs;
EMBED_TABLE_LAYOUT(CliPagesLinesErrMsgNs, page_lines_get);
EMBED_TABLE_STORAGE CliPagesLinesErrMsgNs cli_pages_lines_err_msg_internal EMBED_UNUSED = {
    .page_lines_get = cli_page_lines_get,
};

typedef struct
{
    void (*page_adjacent)(const CliPagesPathCtx *args);
} CliPagesPathNsTable;
EMBED_TABLE_LAYOUT(CliPagesPathNsTable, page_adjacent);
EMBED_TABLE_STORAGE CliPagesPathNsTable cli_pages_path_internal EMBED_UNUSED = {
    .page_adjacent = cli_page_adjacent,
};

typedef struct
{
    unsigned long long (*page_write_at)(const CliPagesPieceCtx *args);
} CliPagesPieceNs;
EMBED_TABLE_LAYOUT(CliPagesPieceNs, page_write_at);
EMBED_TABLE_STORAGE CliPagesPieceNs cli_pages_piece_internal EMBED_UNUSED = {
    .page_write_at = cli_page_write_at,
};

#endif
