// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gutter.h: the change marks in the editor's gutter, a bar a row just before the text: lines put in in the second
// mark's color, lines changed in the third's, and lines taken out after a row as a short bar in the first's across the
// row's foot; the mark of a row is the one the store's diff answers for its line (editor/document_changes.h), and a
// press on the bar's column shows that line's change (gutter.c)
#ifndef GUI_WINDOW_GUTTER_H
#define GUI_WINDOW_GUTTER_H
#include "../../../includes/raster/raster.h"
#include "../../../includes/window/palette.h"
#include "embed_dispatch_layout.h"
typedef struct GuiGutterCfg GuiGutterCfg;
// the sizes in the page's pixels: how far the bar's right stands before the text, its width, the reach of a press
// about it, and the height of a mark of lines taken out
#define GUI_GUTTER_MARK_RIGHT 4
#define GUI_GUTTER_MARK_WIDTH 3
#define GUI_GUTTER_MARK_REACH 4
#define GUI_GUTTER_MARK_FOOT 4
// the mark `mark` drawn on the row `rect`, the row's text starting at `text_x`, at `scale`, cut to `clip`: nothing
// where the mark is none; and whether the pixel column `x` stands on the marks' column before `text_x`
void gui_gutter_mark_draw(const GuiGutterCfg *args);
int gui_gutter_mark_hit(const GuiGutterCfg *args);
struct GuiGutterCfg
{
    GuiRaster *raster;
    const GuiPalette *palette;
    unsigned int mark;
    GuiRect rect;
    int text_x;
    int x;
    int scale;
    GuiRect clip;
};
typedef struct
{
    void (*mark_draw)(const GuiGutterCfg *args);
    int (*mark_hit)(const GuiGutterCfg *args);
} GuiGutterNs;
EMBED_TABLE_LAYOUT(GuiGutterNs, mark_draw, mark_hit);
EMBED_TABLE_STORAGE GuiGutterNs gui_gutter EMBED_UNUSED = {
    .mark_draw = gui_gutter_mark_draw,
    .mark_hit = gui_gutter_mark_hit,
};
#endif
