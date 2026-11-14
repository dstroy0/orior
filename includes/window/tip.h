// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tip.h: the tooltip: the label of the button the pointer rests on, and its keys where it has any, in a panel box
// under the button, the label in the interface face and the keys after it in the quieter text. The pointer come onto
// a button starts a rest; the tip shows as the rest ends and goes as the pointer leaves the button, presses or types
// (tip.c)
#ifndef GUI_WINDOW_TIP_H
#define GUI_WINDOW_TIP_H
#include "glyphs.h"
#include "palette.h"
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"

typedef struct GuiTipRasterCfg GuiTipRasterCfg;
typedef struct GuiTipPointCfg GuiTipPointCfg;
typedef struct GuiTipTimeCfg GuiTipTimeCfg;
typedef struct GuiTipReadCfg GuiTipReadCfg;
typedef struct GuiTipCfg GuiTipCfg;

// the rest on a button before its tip shows, in milliseconds; and the sizes in the page's pixels: the box's padding
// across and down, the gap between the label and the keys, and how far the box is off the button
#define GUI_TIP_REST_MS 600ULL
#define GUI_TIP_PAD_X 8
#define GUI_TIP_PAD_Y 4
#define GUI_TIP_GAP 10
#define GUI_TIP_OFF 4

// The tip: the label of the button the pointer is on, NULL off every button, and its keys, NULL or empty for
// none; the button's rectangle, and whether the box is beside the button, at its right as a side_menu tool's does,
// in place of under it; when the rest on it ends; whether the tip shows; and the box's rectangle where it was drawn
// last, empty while it does not show
typedef struct
{
    const char *label;
    const char *keys;
    GuiRect anchor;
    int beside;
    unsigned long long due_ms;
    int displayed;
    GuiRect rect;
} GuiTip;

// a tip on no button
void gui_tip_init(const GuiTipCfg *args);
// the pointer at `time_ms` on the button `anchor` labelled `label` with `keys`, its box to be `beside` the button
// or under it, or on no button where `label` is NULL: another button starts the rest again, the same one retains it: 1
// where a tip that showed went
int gui_tip_point(const GuiTipPointCfg *args);
// the clock at `time_ms`: 1 where the rest ended and the tip came to show
int gui_tip_tick(const GuiTipTimeCfg *args);
// when the tip next wants a tick, in milliseconds, 0 for never
unsigned long long gui_tip_deadline_ms(const GuiTipReadCfg *args);
// the tip drawn under its button, centred on it, above the button where below has no room, or at the button's right,
// centred on its row, where the tip is beside it, inside `bounds` at `scale`: the label in the interface face in
// the text's color and the keys after it in the quieter text; the box's rectangle recorded
void gui_tip_draw(const GuiTipRasterCfg *args);
struct GuiTipRasterCfg
{
    GuiTip *tip;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect bounds;
    int scale;
};

struct GuiTipPointCfg
{
    GuiTip *tip;
    const char *label;
    const char *keys;
    GuiRect anchor;
    int beside;
    unsigned long long time_ms;
};

struct GuiTipTimeCfg
{
    GuiTip *tip;
    unsigned long long time_ms;
};

struct GuiTipReadCfg
{
    const GuiTip *tip_read;
};

struct GuiTipCfg
{
    GuiTip *tip;
};

typedef struct
{
    void (*init)(const GuiTipCfg *args);
} GuiTipNs;
EMBED_TABLE_LAYOUT(GuiTipNs, init);
EMBED_TABLE_STORAGE GuiTipNs gui_tip EMBED_UNUSED = {
    .init = gui_tip_init,
};

typedef struct
{
    int (*point)(const GuiTipPointCfg *args);
} GuiTipPointNs;
EMBED_TABLE_LAYOUT(GuiTipPointNs, point);
EMBED_TABLE_STORAGE GuiTipPointNs gui_tip_pointer EMBED_UNUSED = {
    .point = gui_tip_point,
};

typedef struct
{
    int (*tick)(const GuiTipTimeCfg *args);
} GuiTipTimeNs;
EMBED_TABLE_LAYOUT(GuiTipTimeNs, tick);
EMBED_TABLE_STORAGE GuiTipTimeNs gui_tip_time EMBED_UNUSED = {
    .tick = gui_tip_tick,
};

typedef struct
{
    unsigned long long (*deadline_ms)(const GuiTipReadCfg *args);
} GuiTipReadNs;
EMBED_TABLE_LAYOUT(GuiTipReadNs, deadline_ms);
EMBED_TABLE_STORAGE GuiTipReadNs gui_tip_read EMBED_UNUSED = {
    .deadline_ms = gui_tip_deadline_ms,
};

typedef struct
{
    void (*draw)(const GuiTipRasterCfg *args);
} GuiTipRasterNs;
EMBED_TABLE_LAYOUT(GuiTipRasterNs, draw);
EMBED_TABLE_STORAGE GuiTipRasterNs gui_tip_raster EMBED_UNUSED = {
    .draw = gui_tip_draw,
};

#endif
