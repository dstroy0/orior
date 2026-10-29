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

// the faces: the code face regular, bold, italic and bold italic, the interface face and the reading face
#define GUI_FACE_CODE 0u
#define GUI_FACE_CODE_BOLD 1u
#define GUI_FACE_CODE_ITALIC 2u
#define GUI_FACE_CODE_BOLD_ITALIC 3u
#define GUI_FACE_HEAD 4u
#define GUI_FACE_TEXT 5u
#define GUI_FACES 6u
// the glyphs kept drawn at once; past three quarters of them every one is let go and drawn again as it is asked for
#define GUI_GLYPH_SLOTS 4096u

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

// The glyphs: each face as read and whether it was, the file each was read from, the outline a glyph is drawn through,
// and the cache's entries, one rasterized glyph each, with how many of them are in use
typedef struct
{
    CliFont face[GUI_FACES];
    int has[GUI_FACES];
    const unsigned char *data[GUI_FACES];
    unsigned long long data_size[GUI_FACES];
    CliContours *contours;
    GuiCachedGlyph *slot;
    unsigned int cached;
} GuiGlyphs;

// the faces read from the system's fonts, the code face the monospace font orior_cli draws with: 1, or 0 with why
// given, the code face being the one the window cannot do without; and every cell of theirs given back
int gui_glyphs_made(GuiGlyphs *glyphs, VerbaLinea *err_msg);
void gui_glyphs_given(GuiGlyphs *glyphs);
// the face `face` is drawn with: itself where it was read, else the code face
unsigned int gui_face_had(const GuiGlyphs *glyphs, unsigned int face);
// the glyph of `letter` in `face` at `size` pixels an em, drawn and kept: its mask, with no pixels where the letter
// draws none, or NULL where it could not be drawn; its advance in pixels into `advance`
const GuiMask *gui_glyph(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, float *advance);
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
