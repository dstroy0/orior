// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// print.h: orior's own printing, with no dialog of the system's: a listing laid out in pages (page.c), written as a PDF
// (pdf.c) or drawn as pixels by impero itself (font.c, font_outline.c, raster.c, raster_write.c), and sent from
// orior's own queue to a printer on the network by IPP (ipp.c, ipp_jobs.c) or its raw port, to a printer of the
// system's through its driver or CUPS, or to a PDF file.
//
// A listing prints as its lines, each a run of colored text, laid on pages of a paper size, upright or on its side, in
// columns of a monospace face 9.5 points tall, each line with its number and a long line going on under itself. The
// PDF and the raster both draw the pages laid out here, the one as text and the other as pixels.
//
// Every call that lays a listing out works on the one layout page.c stores, and is made with the print lock acquired
// (cli_print_alloc, cli_print_release)
#ifndef PRINT_H
#define PRINT_H
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliPrintLayoutCfg CliPrintLayoutCfg;
typedef struct CliPrintLayoutListingCfg CliPrintLayoutListingCfg;
typedef struct CliPrintTextCfg CliPrintTextCfg;
typedef struct CliPrintLayoutListingOutCfg CliPrintLayoutListingOutCfg;
typedef struct CliPrintJsonCfg CliPrintJsonCfg;
typedef struct CliPrintLayoutLowCfg CliPrintLayoutLowCfg;
typedef struct CliPrintJsonNodeCfg CliPrintJsonNodeCfg;
typedef struct CliPrintLayoutWidthCfg CliPrintLayoutWidthCfg;
typedef struct CliPrintCfg CliPrintCfg;
typedef struct CliPrintPageCfg CliPrintPageCfg;

// the text's size, the height of a row, the width of a column and the margin, in points
#define CLI_PRINT_SIZE 9.5f

#define CLI_PRINT_LEADING (CLI_PRINT_SIZE * 1.45f)

#define CLI_PRINT_ADVANCE (CLI_PRINT_SIZE * 0.6f)

#define CLI_PRINT_MARGIN (14.0f * 72.0f / 25.4f)

// the most lines, runs and bytes of text a listing contains; the most rows and pieces a layout contains, and the bytes
// of their text; the most pages a layout contains
#define CLI_PRINT_LINES (1u << 18u)

#define CLI_PRINT_RUNS (1u << 19u)

#define CLI_PRINT_TEXT (1u << 23u)
#define CLI_PRINT_LETTERS (CLI_PRINT_TEXT / 4u)

#define CLI_PRINT_ROWS (1u << 19u)

#define CLI_PRINT_PIECES (1u << 20u)

#define CLI_PRINT_PAGES (1u << 16u)

// A run of a line's text in one color and face: its text in the listing's, and its color as #rrggbb
typedef struct
{
    unsigned int text;
    unsigned int length;
    unsigned int color;
    unsigned int color_length;
    int bold;
    int italic;
} CliPrintRun;

// A line of the listing: its number, where it shows one, and its runs, `runs` of them from `first`
typedef struct
{
    int numbered;
    unsigned long long number;
    unsigned int first;
    unsigned int runs;
} CliPrintLine;

// What prints: its title, its lines and their runs, a tab as wide as `tab` spaces, and the texts they occupy, in the
// capacity's cell, up to CLI_PRINT_LINES lines, CLI_PRINT_RUNS runs and CLI_PRINT_TEXT bytes of text
typedef struct
{
    char title[CLI_PATH_MAX_LEN];
    CliPrintLine *line;
    unsigned int lines;
    unsigned int lines_capacity;
    CliPrintRun *run;
    unsigned int runs;
    unsigned int runs_capacity;
    char *text;
    unsigned int text_used;
    unsigned int text_capacity;
    unsigned long long tab;
} CliPrintListing;

// How a page prints: a4 or letter, on its side, in gray, the pages printed as 1-3,5 (every page where none are named),
// and the copies
typedef struct
{
    char paper[16];
    int landscape;
    int gray;
    char pages[CLI_NAME_MAX_LEN];
    int has_pages;
    unsigned int copies;
} CliPrintLayout;

// A piece of a row: the column it starts at, its text in the layout's, its color and face
typedef struct
{
    unsigned long long column;
    unsigned int text;
    unsigned int length;
    unsigned char color[3];
    int bold;
    int italic;
} CliPrintPiece;

// A row of a page: its line's number, on the first row of a line, and its pieces, `pieces` of them from `first`
typedef struct
{
    int numbered;
    unsigned long long number;
    unsigned int first;
    unsigned int pieces;
} CliPrintRow;

// The pages laid out: the rows, a page `rows_per_page` of them, the pieces and their text, and the columns the numbers
// take, in the capacity's cell, up to CLI_PRINT_ROWS rows, CLI_PRINT_PIECES pieces and CLI_PRINT_TEXT bytes of text
typedef struct
{
    CliPrintRow *row;
    unsigned int rows;
    unsigned int rows_capacity;
    unsigned int rows_per_page;
    unsigned int pages;
    CliPrintPiece *piece;
    unsigned int pieces;
    unsigned int pieces_capacity;
    char *text;
    unsigned int text_used;
    unsigned int text_capacity;
    unsigned long long number_columns;
    int full;
} CliPrintLaid;

// A page's pixels: its width and height, one byte a pixel for gray or three for color
typedef struct
{
    unsigned long long width;
    unsigned long long height;
    unsigned long long channels;
    unsigned char *data;
} CliPixels;

// the print lock taken with the cell the listing and its layout are stored in, and both given back; where the cells
// cannot meet it, a listing read with the lock acquired is refused (page.c)
void cli_print_alloc(void);
void cli_print_release(void);
// the listing the window hands as JSON, `{title, lines: [{number, runs: [{text, color, bold, italic}]}], tab}`, read
// into the one listing; and a layout's JSON, `{paper, landscape, gray, pages, copies}`, each left out its default:
// 1, or 0 with why given
int cli_print_listing_read(const CliPrintJsonCfg *args);
void cli_print_layout_read(const CliPrintJsonNodeCfg *args);
// the paper's width and height in points, as it lies; the paper's name as IPP and PWG write it; whether the page
// numbered `page`, counted from 1, is one to print; and the page ranges as IPP takes them into `low` and `high`,
// each sized for `capacity`: their count
void cli_print_size(const CliPrintLayoutWidthCfg *args);
const char *cli_print_media(const CliPrintLayoutCfg *args);
int cli_print_prints(const CliPrintCfg *args);
unsigned int cli_print_ranges(const CliPrintLayoutLowCfg *args);
// a color #rrggbb, `length` bytes, as its three bytes, made gray by its lightness where `gray` is set
void cli_print_color(const CliPrintTextCfg *args);
// the listing laid out in pages as `layout` specifies, into the one layout
const CliPrintLaid *cli_print_lay_out(const CliPrintLayoutListingOutCfg *args);

// the listing as a PDF, as the layout specifies, into `out`, the count of pages written into `pages` (pdf.c)
void cli_print_pdf(const CliPrintLayoutListingOutCfg *args);

// the system's monospace faces read: 1, or 0 with why given; and each page the layout prints drawn at `dpi`, handed to
// `each` with its place among those printed and their count (raster.c)
int cli_print_faces(const CliPrintJsonCfg *args);
typedef struct
{
    void *context;
    const CliPixels *page;
    unsigned int index;
    unsigned int count;
} CliEachPageIndexCountCfg;

void cli_print_render(const CliPrintLayoutListingCfg *args);
// a page turned a quarter to the left into `out`, whose data contains the page's bytes: a page on its side made upright
void cli_print_turn(const CliPrintPageCfg *args);
// the head of a PWG raster, and one page of it, `count` pages in all; the head of Apple's raster, and one page of it
// (raster_write.c)
void cli_print_pwg_begin(const CliPrintLayoutListingOutCfg *args);
void cli_print_pwg_page(const CliPrintLayoutCfg *args);
void cli_print_urf_begin(const CliPrintLayoutCfg *args);
void cli_print_urf_page(const CliPrintLayoutCfg *args);
// the lines of a page in the compression PWG and Apple raster share
void cli_print_compress(const CliPrintLayoutCfg *args);

struct CliPrintLayoutCfg
{
    const CliPrintLayout *layout;
    VerbaLinea *out;
    const CliPixels *page;
    unsigned int dpi;
    unsigned int count;
};

struct CliPrintLayoutListingCfg
{
    const CliPrintLayout *layout;
    const CliPrintListing *listing;
    float dpi_f32;
    void (*each)(const CliEachPageIndexCountCfg *args);
    void *context;
};

struct CliPrintTextCfg
{
    const char *text;
    unsigned long long length;
    int gray;
    unsigned char *rgb;
};

struct CliPrintLayoutListingOutCfg
{
    const CliPrintLayout *layout;
    const CliPrintListing *listing;
    VerbaLinea *out;
    unsigned long long *pages;
};

struct CliPrintJsonCfg
{
    const CliJson *json;
    unsigned int node;
    CliPrintListing **listing_pointer;
    VerbaLinea *err_msg;
};

struct CliPrintLayoutLowCfg
{
    const CliPrintLayout *layout;
    int *low;
    int *high;
    unsigned int capacity;
};

struct CliPrintJsonNodeCfg
{
    const CliJson *json;
    unsigned int node;
    CliPrintLayout *layout_out;
};

struct CliPrintLayoutWidthCfg
{
    const CliPrintLayout *layout;
    float *width;
    float *height;
};

struct CliPrintCfg
{
    const CliPrintLayout *layout;
    unsigned long long page_u64;
};

struct CliPrintPageCfg
{
    const CliPixels *page;
    CliPixels *out_pixels_pointer;
};

typedef struct
{
    int (*listing_read)(const CliPrintJsonCfg *args);
    int (*faces)(const CliPrintJsonCfg *args);
} CliPrintJsonNs;
EMBED_TABLE_LAYOUT(CliPrintJsonNs, listing_read, faces);
EMBED_TABLE_STORAGE CliPrintJsonNs cli_print_json EMBED_UNUSED = {
    .listing_read = cli_print_listing_read,
    .faces = cli_print_faces,
};

typedef struct
{
    void (*layout_read)(const CliPrintJsonNodeCfg *args);
} CliPrintJsonNodeNs;
EMBED_TABLE_LAYOUT(CliPrintJsonNodeNs, layout_read);
EMBED_TABLE_STORAGE CliPrintJsonNodeNs cli_print_json_node EMBED_UNUSED = {
    .layout_read = cli_print_layout_read,
};

typedef struct
{
    void (*size)(const CliPrintLayoutWidthCfg *args);
} CliPrintLayoutWidthNs;
EMBED_TABLE_LAYOUT(CliPrintLayoutWidthNs, size);
EMBED_TABLE_STORAGE CliPrintLayoutWidthNs cli_print_layout_width EMBED_UNUSED = {
    .size = cli_print_size,
};

typedef struct
{
    const char *(*media)(const CliPrintLayoutCfg *args);
    void (*pwg_page)(const CliPrintLayoutCfg *args);
    void (*urf_begin)(const CliPrintLayoutCfg *args);
    void (*urf_page)(const CliPrintLayoutCfg *args);
    void (*compress)(const CliPrintLayoutCfg *args);
} CliPrintLayoutNs;
EMBED_TABLE_LAYOUT(CliPrintLayoutNs, media, pwg_page, urf_begin, urf_page, compress);
EMBED_TABLE_STORAGE CliPrintLayoutNs cli_print_layout EMBED_UNUSED = {
    .media = cli_print_media,
    .pwg_page = cli_print_pwg_page,
    .urf_begin = cli_print_urf_begin,
    .urf_page = cli_print_urf_page,
    .compress = cli_print_compress,
};

typedef struct
{
    int (*prints)(const CliPrintCfg *args);
} CliPrintNs;
EMBED_TABLE_LAYOUT(CliPrintNs, prints);
EMBED_TABLE_STORAGE CliPrintNs cli_print_table EMBED_UNUSED = {
    .prints = cli_print_prints,
};

typedef struct
{
    unsigned int (*ranges)(const CliPrintLayoutLowCfg *args);
} CliPrintLayoutLowNs;
EMBED_TABLE_LAYOUT(CliPrintLayoutLowNs, ranges);
EMBED_TABLE_STORAGE CliPrintLayoutLowNs cli_print_layout_low EMBED_UNUSED = {
    .ranges = cli_print_ranges,
};

typedef struct
{
    void (*color)(const CliPrintTextCfg *args);
} CliPrintTextNs;
EMBED_TABLE_LAYOUT(CliPrintTextNs, color);
EMBED_TABLE_STORAGE CliPrintTextNs cli_print_text EMBED_UNUSED = {
    .color = cli_print_color,
};

typedef struct
{
    const CliPrintLaid *(*lay_out)(const CliPrintLayoutListingOutCfg *args);
    void (*pdf)(const CliPrintLayoutListingOutCfg *args);
    void (*pwg_begin)(const CliPrintLayoutListingOutCfg *args);
} CliPrintLayoutListingOutNs;
EMBED_TABLE_LAYOUT(CliPrintLayoutListingOutNs, lay_out, pdf, pwg_begin);
EMBED_TABLE_STORAGE CliPrintLayoutListingOutNs cli_print_layout_listing_out EMBED_UNUSED = {
    .lay_out = cli_print_lay_out,
    .pdf = cli_print_pdf,
    .pwg_begin = cli_print_pwg_begin,
};

typedef struct
{
    void (*render)(const CliPrintLayoutListingCfg *args);
} CliPrintLayoutListingNs;
EMBED_TABLE_LAYOUT(CliPrintLayoutListingNs, render);
EMBED_TABLE_STORAGE CliPrintLayoutListingNs cli_print_layout_listing EMBED_UNUSED = {
    .render = cli_print_render,
};

typedef struct
{
    void (*turn)(const CliPrintPageCfg *args);
} CliPrintPageNs;
EMBED_TABLE_LAYOUT(CliPrintPageNs, turn);
EMBED_TABLE_STORAGE CliPrintPageNs cli_print_page EMBED_UNUSED = {
    .turn = cli_print_turn,
};

#endif
