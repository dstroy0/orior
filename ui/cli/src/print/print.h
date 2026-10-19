// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// print.h: orior's own printing, with no dialog of the system's: a listing laid out in pages (page.c), written as a PDF
// (pdf.c) or drawn as pixels by orior itself (font.c, font_outline.c, raster.c, raster_write.c), and sent from orior's
// own queue to a printer on the network by IPP (ipp.c, ipp_jobs.c) or its raw port, to a printer of the system's
// through its driver or CUPS, or to a PDF file.
//
// A listing prints as its lines, each a run of colored text, laid on pages of a paper size, upright or on its side, in
// columns of a monospace face 9.5 points tall, each line with its number and a long line going on under itself. The
// PDF and the raster both draw the pages laid out here, the one as text and the other as pixels.
//
// Every call that lays a listing out works on the one layout page.c keeps, and is made with the print lock held
// (cli_print_take, cli_print_give)
#ifndef PRINT_H
#define PRINT_H

#include "../text/json_tree.h"

// the text's size, the height of a row, the width of a column and the margin, in points
#define CLI_PRINT_SIZE 9.5f

#define CLI_PRINT_LEADING (CLI_PRINT_SIZE * 1.45f)

#define CLI_PRINT_ADVANCE (CLI_PRINT_SIZE * 0.6f)

#define CLI_PRINT_MARGIN (14.0f * 72.0f / 25.4f)

// the most lines, runs and bytes of text a listing holds; the most rows and pieces a layout holds, and the bytes of
// their text; the most pages a layout holds
#define CLI_PRINT_LINES (1u << 18u)

#define CLI_PRINT_RUNS (1u << 19u)

#define CLI_PRINT_TEXT (1u << 23u)

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

// What prints: its title, its lines and their runs, a tab as wide as `tab` spaces, and the texts they keep
typedef struct
{
    char title[CLI_PATH_LONGEST];
    CliPrintLine line[CLI_PRINT_LINES];
    unsigned int lines;
    CliPrintRun run[CLI_PRINT_RUNS];
    unsigned int runs;
    char text[CLI_PRINT_TEXT];
    unsigned int text_used;
    unsigned long long tab;
} CliPrintListing;

// How a page prints: a4 or letter, on its side, in gray, the pages printed as 1-3,5 (every page where none are named),
// and the copies
typedef struct
{
    char paper[16];
    int landscape;
    int gray;
    char pages[CLI_NAME_LONGEST];
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
// take
typedef struct
{
    CliPrintRow row[CLI_PRINT_ROWS];
    unsigned int rows;
    unsigned int rows_per_page;
    unsigned int pages;
    CliPrintPiece piece[CLI_PRINT_PIECES];
    unsigned int pieces;
    char text[CLI_PRINT_TEXT];
    unsigned int text_used;
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

// the print lock taken with the cell the listing and its layout are kept in, and both given back; where the cells
// cannot meet it, a listing read with the lock held is refused (page.c)
void cli_print_take(void);
void cli_print_give(void);
// the listing the window hands as JSON, `{title, lines: [{number, runs: [{text, color, bold, italic}]}], tab}`, read
// into the one listing; and a layout's JSON, `{paper, landscape, gray, pages, copies}`, each left out its default:
// 1, or 0 with why said
int cli_print_listing_read(const CliJson *json, unsigned int node, CliPrintListing **listing, CliLine *said);
void cli_print_layout_read(const CliJson *json, unsigned int node, CliPrintLayout *layout);
// the paper's width and height in points, as it lies; the paper's name as IPP and PWG write it; whether the page
// numbered `page`, counted from 1, is one to print; and the page ranges as IPP takes them into `low` and `high`,
// each holding `room`: their count
void cli_print_size(const CliPrintLayout *layout, float *width, float *height);
const char *cli_print_media(const CliPrintLayout *layout);
int cli_print_prints(const CliPrintLayout *layout, unsigned long long page);
unsigned int cli_print_ranges(const CliPrintLayout *layout, int *low, int *high, unsigned int room);
// a color #rrggbb, `length` bytes, as its three bytes, made gray by its lightness where `gray` is set
void cli_print_color(const char *text, unsigned long long length, int gray, unsigned char *rgb);
// the listing laid out in pages as `layout` says, into the one layout
const CliPrintLaid *cli_print_lay_out(const CliPrintListing *listing, const CliPrintLayout *layout);

// the listing as a PDF, as the layout says, into `out`, the count of pages written into `pages` (pdf.c)
void cli_print_pdf(const CliPrintListing *listing, const CliPrintLayout *layout, CliLine *out,
                   unsigned long long *pages);

// the system's monospace faces read: 1, or 0 with why said; and each page the layout prints drawn at `dpi`, handed to
// `each` with its place among those printed and their count (raster.c)
int cli_print_faces(CliLine *said);
void cli_print_render(const CliPrintListing *listing, const CliPrintLayout *layout, float dpi,
                      void (*each)(void *context, const CliPixels *page, unsigned int index, unsigned int count),
                      void *context);
// a page turned a quarter to the left into `out`, whose data holds the page's bytes: a page on its side made upright
void cli_print_turned(const CliPixels *page, CliPixels *out);
// the head of a PWG raster, and one page of it, `count` pages in all; the head of Apple's raster, and one page of it
// (raster_write.c)
void cli_print_pwg_begin(CliLine *out);
void cli_print_pwg_page(CliLine *out, const CliPixels *page, const CliPrintLayout *layout, unsigned int dpi,
                        unsigned int count);
void cli_print_urf_begin(CliLine *out, unsigned int count);
void cli_print_urf_page(CliLine *out, const CliPixels *page, unsigned int dpi);
// the lines of a page in the compression PWG and Apple raster share
void cli_print_compress(const CliPixels *page, CliLine *out);

#endif
