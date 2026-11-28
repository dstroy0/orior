// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// work.h: the one host of the views that are over the editor in its place, a compare, a merge, a review: one table
// of hooks a view, one open at a time, drawn in the frame's editor rectangle in place of the editor, its map and the
// problems' underline, and taking the presses, the wheel, the keys and the text there while it is open; Escape closes
// it. The host's own state is the open table and the rows scrolled off its top (work.c)
#ifndef GUI_WINDOW_WORK_H
#define GUI_WINDOW_WORK_H
#include "frame.h"
#include "glyphs.h"
#include "icons.h"
#include "palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiWorkCfg GuiWorkCfg;
typedef struct GuiWorkTable GuiWorkTable;

// a pixel off the open table's rectangle, and one on it that is on no control or row of it
#define GUI_WORK_OUTSIDE 0xFFFFFFFFu
#define GUI_WORK_NOTHING 0xFFFFFFFEu

// what a hook answers of an input: not taken, taken, and taken with the view to close
#define GUI_WORK_PASSED 0u
#define GUI_WORK_TAKEN 1u
#define GUI_WORK_CLOSES 2u

// The state of the host: the open table, NULL for none, and how many rows the view scrolled off its top
typedef struct
{
    const GuiWorkTable *open;
    unsigned int scroll;
} GuiWork;

// The table of a view:
// - its name;
// - the view drawn in `rect`;
// - the control or row at x, y in `rect`, with its index in `index_out`: 1, or 0 where the point is on none;
// - the control or row `index` pressed;
// - the wheel turned `rows` rows. A turn of none brings the first change of the view into sight;
// - the key `name` pressed, and `length` bytes of `text` typed. Each of these four gives one of the answers above;
// - the menu at `index`, built as the own rows of the menus: 1 where it has any, NULL for none;
// - the view closed, with its data let go.
struct GuiWorkTable
{
    const char *name;
    void (*draw)(const GuiWorkCfg *args);
    int (*hit)(const GuiWorkCfg *args);
    unsigned int (*press)(const GuiWorkCfg *args);
    unsigned int (*wheel)(const GuiWorkCfg *args);
    unsigned int (*key)(const GuiWorkCfg *args);
    unsigned int (*text)(const GuiWorkCfg *args);
    int (*menu)(const GuiWorkCfg *args);
    void (*close)(const GuiWorkCfg *args);
};

// The calls of the host:
// - the host begun with no table open;
// - `table` opened, after the call closes the one open before. The wheel turns none, and the view brings its first
//   change into sight;
// - the open view closed;
// - whether a view is open;
// - the open view drawn in the editor rectangle of the frame;
// - what the pixel at x, y is on. GUI_WORK_OUTSIDE is off the editor and the map, or with no view open.
//   GUI_WORK_NOTHING is on neither a control nor a row;
// - the control or row `index` pressed;
// - the wheel turned `rows` over the editor or the map;
// - the key `name`, where Escape closes the view;
// - `length` bytes of `text`;
// - the menu at `index`.
//
// The last five give 1 where the open view took the input, and 0 with no view open or where the view passed it on
void gui_work_init(const GuiWorkCfg *args);
void gui_work_open(const GuiWorkCfg *args);
void gui_work_close(const GuiWorkCfg *args);
int gui_work_is_open(const GuiWorkCfg *args);
void gui_work_draw(const GuiWorkCfg *args);
unsigned int gui_work_hit(const GuiWorkCfg *args);
int gui_work_press(const GuiWorkCfg *args);
int gui_work_wheel(const GuiWorkCfg *args);
int gui_work_key(const GuiWorkCfg *args);
int gui_work_text(const GuiWorkCfg *args);
int gui_work_menu(const GuiWorkCfg *args);
struct GuiWorkCfg
{
    GuiWork *work;
    const GuiWork *work_read;
    const GuiWorkTable *table;
    const GuiFrame *frame;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    GuiIcons *icons;
    const GuiPalette *palette;
    GuiRect rect;
    int x;
    int y;
    unsigned int index;
    unsigned int *index_out;
    int rows;
    const char *name;
    const char *text;
    unsigned long long length;
    const char *root;
};

typedef struct
{
    void (*init)(const GuiWorkCfg *args);
    void (*open)(const GuiWorkCfg *args);
    void (*close)(const GuiWorkCfg *args);
    int (*is_open)(const GuiWorkCfg *args);
    void (*draw)(const GuiWorkCfg *args);
    unsigned int (*hit)(const GuiWorkCfg *args);
    int (*press)(const GuiWorkCfg *args);
    int (*wheel)(const GuiWorkCfg *args);
    int (*key)(const GuiWorkCfg *args);
    int (*text)(const GuiWorkCfg *args);
    int (*menu)(const GuiWorkCfg *args);
} GuiWorkNs;
EMBED_TABLE_LAYOUT(GuiWorkNs, init, open, close, is_open, draw, hit, press, wheel, key, text, menu);
EMBED_TABLE_STORAGE GuiWorkNs gui_work EMBED_UNUSED = {
    .init = gui_work_init,
    .open = gui_work_open,
    .close = gui_work_close,
    .is_open = gui_work_is_open,
    .draw = gui_work_draw,
    .hit = gui_work_hit,
    .press = gui_work_press,
    .wheel = gui_work_wheel,
    .key = gui_work_key,
    .text = gui_work_text,
    .menu = gui_work_menu,
};

#endif
