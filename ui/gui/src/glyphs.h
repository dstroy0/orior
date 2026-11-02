// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.h: the window's text: its faces read from the system's fonts through the tree's own TrueType reader, the code
// face in its four styles, the interface's sans and the reading serif; a glyph drawn once at a pixel size and saved in
// a cache of coverage masks, keyed by face, glyph and size; a text's width measured and the text drawn on a raster;
// and the code face's cell, its glyphs' advance (glyphs.c)
#ifndef GUI_GLYPHS_H
#define GUI_GLYPHS_H
typedef struct GuiGlyphsFaceSizeCfg GuiGlyphsFaceSizeCfg;
typedef struct GuiGlyphsFaceLetterCfg GuiGlyphsFaceLetterCfg;
typedef struct GuiGlyphsRasterCfg GuiGlyphsRasterCfg;
typedef struct GuiGlyphsFaceCfg GuiGlyphsFaceCfg;
typedef struct GuiGlyphsCfg GuiGlyphsCfg;
#include "../../cli/src/print/font.h"
#include "embed_dispatch_layout.h"
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
// the glyphs saved drawn at once; past three quarters of them every one is let go and drawn again as it is asked for
#define GUI_GLYPH_CACHE_ENTRIES 4096u
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

// the glyph cells saved at once, a glyph in one color each; past three quarters of them every one is let go and made
// again as it is asked for
#define GUI_CELL_CACHE_ENTRIES 4096u

// A glyph cell: whether it is in use, its key (the face, the letter, the pixel size and the color), the glyph's
// advance in pixels, its box, the size and where its top left stands from the pen on the baseline, and its lanes,
// for each pixel of the box the ink's red and blue lanes by the pixel's share of the color with half of 255 in,
// its green lane the same, and the rest of 255 the ground saves, each a word, row after row, the three in one cell
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

// the printable ASCII letters a table holds, from the space on, and the tables saved at once, one a face, size and
// color
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
    GuiCachedGlyph *entry;
    unsigned int cached;
    GuiGlyphCell *cell;
    unsigned int celled;
    GuiAsciiTable ascii[GUI_ASCII_TABLES];
    unsigned int ascii_last;
    unsigned int ascii_next;
} GuiGlyphs;

// the faces read from the system's fonts, the code face the monospace font orior_cli draws with: 1, or 0 with why
// given, the code face being the one the window cannot do without; and every cell of theirs given back
int gui_glyphs_init(const GuiGlyphsCfg *args);
void gui_glyphs_free(const GuiGlyphsCfg *args);
// the face `face` is drawn with: itself where it was read, the interface face in place of its bold where that was
// read, else the code face
unsigned int gui_face_had(const GuiGlyphsFaceCfg *args);
// the interface face's bold read for as long as it is drawn from, it being read at no other time: 1 where it was read
// now, 0 where it already was or the system has none; and given back with its letters' masks
int gui_glyphs_head_bold_read(const GuiGlyphsCfg *args);
void gui_glyphs_head_bold_free(const GuiGlyphsCfg *args);
// the glyph of `letter` in `face` at `size` pixels an em, drawn and saved: its mask, with no pixels where the letter
// draws none, or NULL where it could not be drawn; its advance in pixels into `advance`
const GuiMask *gui_glyph(const GuiGlyphsFaceLetterCfg *args);
// the cell of `letter` in `face` at `size` pixels an em in `color`, made from the glyph's mask and saved: its lanes
// laid for the blend over any ground, with no pixels where the letter draws none, or NULL where the glyph could not
// be drawn or the cell's lanes not taken; its advance in pixels into `advance`; and a cell laid on `raster` with the
// pen at x on the baseline y, no further than `clip`, each pixel of its box in one blend, straight along
const GuiGlyphCell *gui_glyph_cell(const GuiGlyphsFaceLetterCfg *args);
void gui_glyph_cell_layout(const GuiGlyphsRasterCfg *args);
// a face's ascent above the baseline and its descent below it, in pixels at `size`; and the code face's cell, the
// advance of its glyphs at `size`
float gui_glyphs_ascent(const GuiGlyphsFaceCfg *args);
float gui_glyphs_descent(const GuiGlyphsFaceCfg *args);
float gui_glyphs_cell(const GuiGlyphsFaceLetterCfg *args);
// `length` bytes of the UTF-8 `text` drawn in `face` at `size` from `x` on the baseline `y` in `color`, no further than
// `clip`: the x after it; and the width the text takes
float gui_text_draw(const GuiGlyphsFaceSizeCfg *args);
float gui_text_width(const GuiGlyphsFaceSizeCfg *args);
struct GuiGlyphsFaceSizeCfg
{
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    GuiColor color;
    GuiRaster *raster;
    int y;
    GuiRect clip;
    float x_f32;
    const char *text;
    unsigned long long length;
};

struct GuiGlyphsFaceLetterCfg
{
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int letter;
    unsigned int size;
    float *advance;
    GuiColor color;
};

struct GuiGlyphsRasterCfg
{
    GuiRaster *raster;
    const GuiGlyphCell *cell;
    int x;
    int y;
    GuiRect clip;
};

struct GuiGlyphsFaceCfg
{
    const GuiGlyphs *glyphs_read;
    unsigned int face;
    unsigned int size;
};

struct GuiGlyphsCfg
{
    GuiGlyphs *glyphs;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*init)(const GuiGlyphsCfg *args);
    void (*free)(const GuiGlyphsCfg *args);
    int (*head_bold_read)(const GuiGlyphsCfg *args);
    void (*head_bold_free)(const GuiGlyphsCfg *args);
} CliGlyphsNs;
EMBED_TABLE_LAYOUT(CliGlyphsNs, init, free, head_bold_read, head_bold_free);
EMBED_TABLE_STORAGE CliGlyphsNs cli_glyphs EMBED_UNUSED = {
    .init = gui_glyphs_init,
    .free = gui_glyphs_free,
    .head_bold_read = gui_glyphs_head_bold_read,
    .head_bold_free = gui_glyphs_head_bold_free,
};

typedef struct
{
    unsigned int (*face_had)(const GuiGlyphsFaceCfg *args);
    float (*ascent)(const GuiGlyphsFaceCfg *args);
    float (*descent)(const GuiGlyphsFaceCfg *args);
} GuiGlyphsFaceNs;
EMBED_TABLE_LAYOUT(GuiGlyphsFaceNs, face_had, ascent, descent);
EMBED_TABLE_STORAGE GuiGlyphsFaceNs gui_glyphs_face EMBED_UNUSED = {
    .face_had = gui_face_had,
    .ascent = gui_glyphs_ascent,
    .descent = gui_glyphs_descent,
};

typedef struct
{
    const GuiMask *(*glyph)(const GuiGlyphsFaceLetterCfg *args);
    const GuiGlyphCell *(*glyph_cell)(const GuiGlyphsFaceLetterCfg *args);
    float (*cell)(const GuiGlyphsFaceLetterCfg *args);
} GuiGlyphsFaceLetterNs;
EMBED_TABLE_LAYOUT(GuiGlyphsFaceLetterNs, glyph, glyph_cell, cell);
EMBED_TABLE_STORAGE GuiGlyphsFaceLetterNs gui_glyphs_face_letter EMBED_UNUSED = {
    .glyph = gui_glyph,
    .glyph_cell = gui_glyph_cell,
    .cell = gui_glyphs_cell,
};

typedef struct
{
    void (*glyph_cell_layout)(const GuiGlyphsRasterCfg *args);
} GuiGlyphsRasterNs;
EMBED_TABLE_LAYOUT(GuiGlyphsRasterNs, glyph_cell_layout);
EMBED_TABLE_STORAGE GuiGlyphsRasterNs gui_glyphs_raster EMBED_UNUSED = {
    .glyph_cell_layout = gui_glyph_cell_layout,
};

typedef struct
{
    float (*text_draw)(const GuiGlyphsFaceSizeCfg *args);
    float (*text_width)(const GuiGlyphsFaceSizeCfg *args);
} GuiGlyphsFaceSizeNs;
EMBED_TABLE_LAYOUT(GuiGlyphsFaceSizeNs, text_draw, text_width);
EMBED_TABLE_STORAGE GuiGlyphsFaceSizeNs gui_glyphs_face_size EMBED_UNUSED = {
    .text_draw = gui_text_draw,
    .text_width = gui_text_width,
};

#endif
