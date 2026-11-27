// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.h: the window's text: its faces read through the tree's own TrueType reader from the files the caller's
// system holds, the code face in its four styles, the interface's sans and the reading serif; a glyph drawn once at a pixel size and saved in
// a cache of coverage masks, keyed by face, glyph and size; a text's width measured and the text drawn on a raster;
// and the code face's cell, its glyphs' advance (glyphs.c)
#ifndef GUI_GLYPHS_H
#define GUI_GLYPHS_H
#include "../compositor/compositor.h"
#include "../font/font.h"
#include "embed_dispatch_layout.h"
#include "../raster/raster.h"
#include "verba_scribo/verba_linea.h"

typedef struct GuiGlyphsFaceSizeCfg GuiGlyphsFaceSizeCfg;
typedef struct GuiGlyphsFaceLetterCfg GuiGlyphsFaceLetterCfg;
typedef struct GuiGlyphsRasterCfg GuiGlyphsRasterCfg;
typedef struct GuiGlyphsFaceCfg GuiGlyphsFaceCfg;
typedef struct GuiGlyphsCfg GuiGlyphsCfg;
typedef struct GuiGlyphsBakeCfg GuiGlyphsBakeCfg;

// the faces. They are the code face regular, bold, italic and bold italic, and the interface face. Then come the reading
// face, the bold interface face and the face of the large name. The last is the symbol face, which draws a character that its own
// face does not hold
#define GUI_FACE_CODE 0u
#define GUI_FACE_CODE_BOLD 1u
#define GUI_FACE_CODE_ITALIC 2u
#define GUI_FACE_CODE_BOLD_ITALIC 3u
#define GUI_FACE_HEAD 4u
#define GUI_FACE_TEXT 5u
#define GUI_FACE_HEAD_BOLD 6u
#define GUI_FACE_SYMBOL 7u
#define GUI_FACES 8u
// the drawn glyphs that the cache keeps at once. Past three quarters of them, the cache lets each go and draws it again
// when a caller asks for it
#define GUI_GLYPH_CACHE_ENTRIES 4096u
// the stem dark: how far the code pushes out the side of a stroke, in pixels. It is max at the small sizes and least at
// the large sizes. The gamma lifts the coverage when the code positions a glyph for the blend
#define GUI_DARKEN_MAX 0.2f
#define GUI_DARKEN_MIN 0.06f
#define GUI_DARKEN_SMALL 12u
#define GUI_DARKEN_LARGE 32u
#define GUI_TEXT_GAMMA 1.4f

// A cache entry. It holds whether it is in use, and its key: the face, the glyph and the pixel size. It holds the
// advance of the glyph in pixels. It holds the coverage mask in a cell of its own, NULL where the glyph has no pixels
typedef struct
{
    int used;
    unsigned int face;
    unsigned int glyph;
    unsigned int size;
    float advance;
    GuiMask mask_bits;
} GuiCachedGlyph;

// the glyph cells that the cache keeps at once, a glyph in one color each. Past three quarters of them, the cache lets
// each go and makes it again when a caller asks for it
#define GUI_CELL_CACHE_ENTRIES 4096u

// A glyph cell. It holds whether it is in use, and its key: the face, the character, the pixel size and the color.
// It holds the advance of the glyph in pixels. It holds its box: the size, and the top left from the pen on the
// baseline.
//
// It holds its lanes. Each pixel of the box has the red, blue and green lanes of the ink. Each lane is by the share of
// the color of the pixel, with half of 255 in. The rest of 255 is the part of the ground that
// stays. Each is a word, row after row. The three are in one cell of the cellblock, NULL where the glyph has no
// pixels
typedef struct
{
    int used;
    unsigned int face;
    unsigned int character;
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

// the printable ASCII characters a table holds, from the space on, and the tables saved at once, one a face, size and
// color
#define GUI_ASCII_FIRST 32u
#define GUI_ASCII_COUNT 95u
#define GUI_ASCII_TABLES 32u

// An ASCII table. It holds whether it is in use and its key. For each printable character it holds the glyph cell and
// the advance. The cell is NULL until a caller first asks for the character in the key. A character of the table is
// one index, and the code positions its cell straight from there
typedef struct
{
    int used;
    unsigned int face;
    unsigned int size;
    GuiColor color;
    const GuiGlyphCell *cell[GUI_ASCII_COUNT];
    float advance[GUI_ASCII_COUNT];
} GuiAsciiTable;

// A place of a cache's order: the key of an entry or a cell, the face first and 0 for a part the key has not, and the
// place of the entry or the cell
typedef struct
{
    unsigned int key[4];
    unsigned int place;
} GuiGlyphPlace;

// the places of the run of the newest keys of a cache, and no place
#define GUI_GLYPH_RUN 256u
#define GUI_GLYPH_NONE 0xffffffffu

// The keys of a cache: an order of them sorted, `ordered` of them, a run of the newest, `run_count` of them, and the
// room that the sort of a full run writes, each in the block's tables part
typedef struct
{
    GuiGlyphPlace *order;
    unsigned int ordered;
    GuiGlyphPlace *run;
    unsigned int run_count;
    GuiGlyphPlace *room;
} GuiGlyphOrder;

// The faces as the caller's system holds them: face `face`'s file measured, 1 with its bytes into `size`, or 0 where
// the system has no such face. And that file's `size` bytes read whole into `at`, 1 or 0
typedef struct
{
    int (*size)(unsigned int face, unsigned long long *size);
    int (*read)(unsigned int face, unsigned char *at, unsigned long long size);
} GuiGlyphsFaces;

// The glyphs. They hold the files of the faces as the system of the caller holds them. For each face, they hold the
// read face, whether the read succeeded, and the file of the read. They hold whether the code looked for the symbol
// face.
//
// They hold the outline that draws a glyph. They hold the entries of the cache, one rasterized glyph each, how many
// are in use and the order of their keys. They hold the cells, one glyph in one color each, how many are in use and
// the order of their keys. The entries and the cells are in use from the first place on. They hold the ASCII tables,
// the last table found and the next table to take. They hold how far the lanes of the cells and the masks of the
// entries took their rings of the block
typedef struct
{
    const GuiGlyphsFaces *faces;
    Font face[GUI_FACES];
    int has[GUI_FACES];
    const unsigned char *data[GUI_FACES];
    unsigned long long data_size[GUI_FACES];
    int symbol_looked;
    FontContours *contours;
    GuiCachedGlyph *entry;
    unsigned int cached;
    GuiGlyphOrder entry_order;
    GuiGlyphCell *cell;
    unsigned int celled;
    GuiGlyphOrder cell_order;
    GuiAsciiTable ascii[GUI_ASCII_TABLES];
    unsigned int ascii_last;
    unsigned int ascii_next;
    unsigned long long cells_used;
    unsigned long long masks_used;
} GuiGlyphs;

// the faces, read through `faces`, the code face first: 1, or 0 with the reason. The window cannot work without the
// code face. A second call gives back each of their cells
int gui_glyphs_init(const GuiGlyphsCfg *args);
void gui_glyphs_free(const GuiGlyphsCfg *args);
// the face that draws `face`. That is `face` itself where the read of it succeeded. For the bold interface face,
// it is the interface face where its read succeeded. Else it is the code face
unsigned int gui_face_had(const GuiGlyphsFaceCfg *args);
// the bold interface face, read for as long as the code draws from it, and at no other time. The call gives 1 where
// it reads the face now, and 0 where the code read the face before or the system has none. A second call gives it back
// with the masks of its characters
int gui_glyphs_head_bold_read(const GuiGlyphsCfg *args);
void gui_glyphs_head_bold_free(const GuiGlyphsCfg *args);
// the glyph of `character` in `face` at `size` pixels an em, drawn and kept. Where `face` does not hold the character
// and the symbol face does, the glyph comes from the symbol face. The code reads that face from the fonts of the system
// the first time a character needs it.
// The result is its mask, with no pixels where the character draws none, or NULL where the code cannot draw it. Its
// advance in pixels goes into `advance`
const GuiMask *gui_glyph(const GuiGlyphsFaceLetterCfg *args);
// the cell of `character` in `face` at `size` pixels an em in `color`, made from the mask of the glyph and kept. The
// result holds its lanes, positioned for the blend over any ground. It has no pixels where the character draws none.
// It is NULL where the code cannot draw the glyph or cannot take the lanes of the cell. Its advance in pixels goes into
// `advance`.
//
// The second call positions a cell on `raster`, with the pen at x on the baseline y, no further than `clip`. Each pixel
// of its box takes one blend, straight along
const GuiGlyphCell *gui_glyph_cell(const GuiGlyphsFaceLetterCfg *args);
void gui_glyph_cell_layout(const GuiGlyphsRasterCfg *args);
// a face's ascent above the baseline and its descent below it, in pixels at `size`; and the code face's cell, the
// advance of its glyphs at `size`
float gui_glyphs_ascent(const GuiGlyphsFaceCfg *args);
float gui_glyphs_descent(const GuiGlyphsFaceCfg *args);
// the baseline of text in `face` at `size` centered in a box `pixel_height` pixels high from `top`: the box's height
// less the face's, halved, then the ascent down, rounded
int gui_glyphs_baseline(const GuiGlyphsFaceCfg *args);
float gui_glyphs_cell(const GuiGlyphsFaceLetterCfg *args);
// `length` bytes of the UTF-8 `text` drawn in `face` at `size` from `x` on the baseline `y` in `color`, no further than
// `clip`: the x after it; and the width the text takes
float gui_text_draw(const GuiGlyphsFaceSizeCfg *args);
// The glyph of `character` in `face` at `size` pixels an em, put into the atlas of the compositor at the place `phase`
// across a pixel. The code moves its outline right by a quarter of a pixel for each place, and draws it as `gui_glyph`
// draws it.
// The atlas gets its coverage without the lift. The atlas keys it by the requested face and the character, which keeps
// the printable ASCII in one range. The result is the entry, which the atlas finds where it holds the glyph already.
// It is GUI_COMPOSITOR_NO_GLYPH where the code cannot draw the glyph or the atlas has no room
embed_u32 gui_glyph_bake(const GuiGlyphsBakeCfg *args);
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
    unsigned int character;
    unsigned int size;
    float *advance;
    GuiColor color;
};

struct GuiGlyphsBakeCfg
{
    GuiGlyphs *glyphs;
    GuiCompositor *compositor;
    unsigned int face;
    unsigned int character;
    unsigned int size;
    unsigned int phase;
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
    int top;
    int pixel_height;
};

struct GuiGlyphsCfg
{
    GuiGlyphs *glyphs;
    const GuiGlyphsFaces *faces;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*init)(const GuiGlyphsCfg *args);
    void (*free)(const GuiGlyphsCfg *args);
    int (*head_bold_read)(const GuiGlyphsCfg *args);
    void (*head_bold_free)(const GuiGlyphsCfg *args);
} GuiGlyphsNs;
EMBED_TABLE_LAYOUT(GuiGlyphsNs, init, free, head_bold_read, head_bold_free);
EMBED_TABLE_STORAGE GuiGlyphsNs gui_glyphs EMBED_UNUSED = {
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
    int (*baseline)(const GuiGlyphsFaceCfg *args);
} GuiGlyphsFaceNs;
EMBED_TABLE_LAYOUT(GuiGlyphsFaceNs, face_had, ascent, descent, baseline);
EMBED_TABLE_STORAGE GuiGlyphsFaceNs gui_glyphs_face EMBED_UNUSED = {
    .face_had = gui_face_had,
    .ascent = gui_glyphs_ascent,
    .descent = gui_glyphs_descent,
    .baseline = gui_glyphs_baseline,
};

typedef struct
{
    const GuiMask *(*glyph)(const GuiGlyphsFaceLetterCfg *args);
    const GuiGlyphCell *(*glyph_cell)(const GuiGlyphsFaceLetterCfg *args);
    float (*cell)(const GuiGlyphsFaceLetterCfg *args);
} GuiGlyphsFaceLetterNs;
EMBED_TABLE_LAYOUT(GuiGlyphsFaceLetterNs, glyph, glyph_cell, cell);
EMBED_TABLE_STORAGE GuiGlyphsFaceLetterNs gui_glyphs_face_character EMBED_UNUSED = {
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

typedef struct
{
    embed_u32 (*bake)(const GuiGlyphsBakeCfg *args);
} GuiGlyphsBakeNs;
EMBED_TABLE_LAYOUT(GuiGlyphsBakeNs, bake);
EMBED_TABLE_STORAGE GuiGlyphsBakeNs gui_glyphs_atlas EMBED_UNUSED = {
    .bake = gui_glyph_bake,
};

#endif
