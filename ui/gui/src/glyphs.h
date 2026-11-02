// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.h: the window's text: its faces read from the system's fonts through the tree's own TrueType reader, the code
// face in its four styles, the interface's sans and the reading serif; a glyph drawn once at a pixel size and kept in
// a cache of coverage masks, keyed by face, glyph and size; a text's width measured and the text drawn on a raster;
// and the code face's cell, its glyphs' advance (glyphs.c)
#ifndef GUI_GLYPHS_H
#define GUI_GLYPHS_H
#include "../../cli/src/print/font.h"
#include "raster.h"

// the faces: the code face regular, bold, italic and bold italic, the interface face, the reading face and the
// interface face bold, the large name's
#define GUI_FACE_CODE 0u
#define GUI_FACE_CODE_BOLD 1u
#define GUI_FACE_CODE_ITALIC 2u
#define GUI_FACE_CODE_BOLD_ITALIC 3u
#define GUI_FACE_HEAD 4u
#define GUI_FACE_TEXT 5u
#define GUI_FACE_HEAD_BOLD 6u
#define GUI_FACES 7u
// the glyphs kept drawn at once; past three quarters of them every one is let go and drawn again as it is asked for
#define GUI_GLYPH_SLOTS 4096u
// the stem darkening: how far a stroke's side is pushed out, most at the small sizes and least at the large, in pixels;
// and the gamma the coverage is lifted through as a glyph is laid for the blend
#define GUI_DARKEN_MOST 0.2f
#define GUI_DARKEN_LEAST 0.06f
#define GUI_DARKEN_SMALL 12u
#define GUI_DARKEN_LARGE 32u
#define GUI_TEXT_GAMMA 1.4f

// A cache entry: whether it is in use, its key (the face, the glyph and the pixel size), the glyph's advance in
// pixels, and the coverage mask, allocated in a cell of its own, NULL where the glyph has no pixels
typedef struct
{
    int used;
    unsigned int face;
    unsigned int glyph;
    unsigned int size;
    float advance;
    GuiMask mask;
} GuiCachedGlyph;

// the glyph cells kept at once, a glyph in one color each; past three quarters of them every one is let go and made
// again as it is asked for
#define GUI_CELL_SLOTS 4096u

// A glyph cell: whether it is in use, its key (the face, the letter, the pixel size and the color), the glyph's
// advance in pixels, its box, the size and where its top left stands from the pen on the baseline, and its lanes,
// for each pixel of the box the ink's red and blue lanes by the pixel's share of the color with half of 255 in,
// its green lane the same, and the rest of 255 the ground keeps, each a word, row after row, the three in one cell
// of the cellblock, NULL where the glyph has no pixels
typedef struct
{
    int used;
    unsigned int face;
    unsigned int letter;
    unsigned int size;
    GuiColor color;
    float advance;
    unsigned int width;
    unsigned int height;
    int left;
    int top;
    unsigned int *red_blue;
    unsigned int *green;
    unsigned int *rest;
} GuiGlyphCell;

// the printable ASCII letters a table holds, from the space on, and the tables kept at once, one a face, size and color
#define GUI_ASCII_FIRST 32u
#define GUI_ASCII_COUNT 95u
#define GUI_ASCII_TABLES 32u

// An ASCII table: whether it is in use, its key, and for each printable letter its glyph cell, NULL until the letter is
// first asked for in the key, and its advance; a letter of the table is one index, its cell laid straight from there
typedef struct
{
    int used;
    unsigned int face;
    unsigned int size;
    GuiColor color;
    const GuiGlyphCell *cell[GUI_ASCII_COUNT];
    float advance[GUI_ASCII_COUNT];
} GuiAsciiTable;

// The glyphs: each face as read and whether it was, the file each was read from, the outline a glyph is drawn through,
// the cache's entries, one rasterized glyph each, with how many of them are in use, and the cells, one glyph in one
// color each, with how many of them are in use; and the ASCII tables, the one last found and the next to be taken
typedef struct
{
    CliFont face[GUI_FACES];
    int has[GUI_FACES];
    const unsigned char *data[GUI_FACES];
    unsigned long long data_size[GUI_FACES];
    CliContours *contours;
    GuiCachedGlyph *slot;
    unsigned int cached;
    GuiGlyphCell *cell;
    unsigned int celled;
    GuiAsciiTable ascii[GUI_ASCII_TABLES];
    unsigned int ascii_last;
    unsigned int ascii_next;
} GuiGlyphs;

// the faces read from the system's fonts, the code face the monospace font orior_cli draws with: 1, or 0 with why
// given, the code face being the one the window cannot do without; and every cell of theirs given back
int gui_glyphs_made(GuiGlyphs *glyphs, VerbaLinea *err_msg);
void gui_glyphs_given(GuiGlyphs *glyphs);
// the face `face` is drawn with: itself where it was read, the interface face in place of its bold where that was
// read, else the code face
unsigned int gui_face_had(const GuiGlyphs *glyphs, unsigned int face);
// the interface face's bold read for as long as it is drawn from, it being read at no other time: 1 where it was read
// now, 0 where it already was or the system has none; and given back with its letters' masks
int gui_glyphs_head_bold_read(GuiGlyphs *glyphs);
void gui_glyphs_head_bold_given(GuiGlyphs *glyphs);
// the glyph of `letter` in `face` at `size` pixels an em, drawn and kept: its mask, with no pixels where the letter
// draws none, or NULL where it could not be drawn; its advance in pixels into `advance`
const GuiMask *gui_glyph(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, float *advance);
// the cell of `letter` in `face` at `size` pixels an em in `color`, made from the glyph's mask and kept: its lanes
// laid for the blend over any ground, with no pixels where the letter draws none, or NULL where the glyph could not
// be drawn or the cell's lanes not taken; its advance in pixels into `advance`; and a cell laid on `raster` with the
// pen at x on the baseline y, no further than `clip`, each pixel of its box in one blend, straight along
const GuiGlyphCell *gui_glyph_cell(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, GuiColor color,
                                   float *advance);
void gui_glyph_cell_laid(GuiRaster *raster, const GuiGlyphCell *cell, int x, int y, GuiRect clip);
// a face's ascent above the baseline and its descent below it, in pixels at `size`; and the code face's cell, the
// advance of its glyphs at `size`
float gui_glyphs_ascent(const GuiGlyphs *glyphs, unsigned int face, unsigned int size);
float gui_glyphs_descent(const GuiGlyphs *glyphs, unsigned int face, unsigned int size);
float gui_glyphs_cell(GuiGlyphs *glyphs, unsigned int size);
// `length` bytes of the UTF-8 `text` drawn in `face` at `size` from `x` on the baseline `y` in `color`, no further than
// `clip`: the x after it; and the width the text takes
float gui_text_drawn(GuiRaster *raster, GuiGlyphs *glyphs, unsigned int face, unsigned int size, float x, int y,
                     const char *text, unsigned long long length, GuiColor color, GuiRect clip);
float gui_text_width(GuiGlyphs *glyphs, unsigned int face, unsigned int size, const char *text,
                     unsigned long long length);
#endif
