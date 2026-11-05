// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick.h: the quick open's box along the top of the window, as the page lays it: under the bar, centered, 40rem wide
// or 86% of the window where that is less; a field with what is typed and the caret after it, the placeholder quieter
// while nothing is typed; under it the rows the quick open lists (editor/files.h), a row its label with what matched
// marked in signal, its detail quieter after it and its keys at the right, the chosen row on the selection's ground, a
// row that only informs quieter; the rows scrolled so the chosen one shows, the list no taller than 26rem or 60% of the
// window (quick.c)
#ifndef GUI_PARTS_QUICK_H
#define GUI_PARTS_QUICK_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiQuickRasterCfg GuiQuickRasterCfg;
typedef struct GuiQuickXCfg GuiQuickXCfg;
typedef struct GuiQuickCfg GuiQuickCfg;

// the sizes in the page's pixels: the box's width, 40rem, and its share of the window where the window is narrower,
// 86%; the box's padding, 0.35rem; the field's height, its padding across, 0.5rem, its corners' radius and its text
// size, 13.5px; the gap between the field and the rows, 0.3rem; the list's most height, 26rem, and its share of the
// window, 60%; a row's height, its padding across, 0.5rem, its corners' radius and the gap between its parts, 0.6rem;
// the label's share of the row, 60%; the text sizes of the label, 13px, the detail, 12px, and the keys, 11.5px; and the
// caret's width
#define GUI_QUICK_WIDTH 640
#define GUI_QUICK_WIDTH_PERCENT 86
#define GUI_QUICK_PAD 6
#define GUI_QUICK_FIELD 30
#define GUI_QUICK_FIELD_PAD 8
#define GUI_QUICK_FIELD_RADIUS 4
#define GUI_QUICK_FIELD_SIZE 14u
#define GUI_QUICK_LIST_GAP 5
#define GUI_QUICK_LIST_MAX 416
#define GUI_QUICK_LIST_PERCENT 60
#define GUI_QUICK_ROW 26
#define GUI_QUICK_ROW_PAD 8
#define GUI_QUICK_ROW_RADIUS 4
#define GUI_QUICK_GAP 10
#define GUI_QUICK_LABEL_PERCENT 60
#define GUI_QUICK_LABEL_SIZE 13u
#define GUI_QUICK_DETAIL_SIZE 12u
#define GUI_QUICK_KEYS_SIZE 12u
#define GUI_QUICK_CARET 2

// The quick open as drawn last: its box, its field and its list, each empty while it is closed; the rows scrolled off
// the list's top; and how many rows the list shows
typedef struct
{
    GuiRect rect;
    GuiRect field;
    GuiRect list;
    unsigned int first;
    unsigned int shown;
} GuiQuick;

// a quick open drawn nowhere, scrolled to its top
void gui_quick_init(const GuiQuickCfg *args);
// the wheel turned `rows` rows over the list, down where positive: the rows scrolled, no further than the last fills
// the list: 1 where the scroll changed
int gui_quick_wheel(const GuiQuickCfg *args);
// the box drawn in `frame` from what the quick open lists, the rows scrolled so the chosen one shows; the rectangles
// recorded, emptied while it is closed
void gui_quick_draw(const GuiQuickRasterCfg *args);
// whether the pixel at x, y stands in the box; and the row at x, y into `at`: 1, or 0 where the pixel stands on none
int gui_quick_holds(const GuiQuickXCfg *args);
int gui_quick_row_at(const GuiQuickXCfg *args);
struct GuiQuickRasterCfg
{
    GuiQuick *quick;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    const GuiFrame *frame;
};

struct GuiQuickXCfg
{
    const GuiQuick *quick_read;
    int x;
    int y;
    unsigned int *at;
};

struct GuiQuickCfg
{
    GuiQuick *quick;
    int rows;
};

typedef struct
{
    void (*init)(const GuiQuickCfg *args);
    int (*wheel)(const GuiQuickCfg *args);
} GuiQuickNs;
EMBED_TABLE_LAYOUT(GuiQuickNs, init, wheel);
EMBED_TABLE_STORAGE GuiQuickNs gui_quick EMBED_UNUSED = {
    .init = gui_quick_init,
    .wheel = gui_quick_wheel,
};

typedef struct
{
    void (*draw)(const GuiQuickRasterCfg *args);
} GuiQuickRasterNs;
EMBED_TABLE_LAYOUT(GuiQuickRasterNs, draw);
EMBED_TABLE_STORAGE GuiQuickRasterNs gui_quick_raster EMBED_UNUSED = {
    .draw = gui_quick_draw,
};

typedef struct
{
    int (*holds)(const GuiQuickXCfg *args);
    int (*row_at)(const GuiQuickXCfg *args);
} GuiQuickXNs;
EMBED_TABLE_LAYOUT(GuiQuickXNs, holds, row_at);
EMBED_TABLE_STORAGE GuiQuickXNs gui_quick_x EMBED_UNUSED = {
    .holds = gui_quick_holds,
    .row_at = gui_quick_row_at,
};

#endif
