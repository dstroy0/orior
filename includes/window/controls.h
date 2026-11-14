// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.h: the three window controls at the top menu's right end, minimize, maximize and close: each control a
// bead of water over its glyph on the table alone, no pane and no shadow under them, and which control the pointer
// is on (controls.c)
#ifndef GUI_WINDOW_CONTROLS_H
#define GUI_WINDOW_CONTROLS_H
#include "frame.h"
#include "palette.h"
#include "button.h"
#include "embed_dispatch_layout.h"

typedef struct GuiControlsRasterCfg GuiControlsRasterCfg;
typedef struct GuiControlsCfg GuiControlsCfg;

// the controls, left to right, and none; and a control's glyph's box in the page's pixels
#define GUI_CONTROL_MINIMIZE 0u
#define GUI_CONTROL_MAXIMIZE 1u
#define GUI_CONTROL_CLOSE 2u
#define GUI_CONTROL_NONE 3u
#define GUI_CONTROL_GLYPH 14

// The controls drawn at `time_ms`: the three controls in the frame's controls rectangle, each its glyph under its
// bead, nothing positioned under the beads, the beads' atlas arranged at the controls' rest places and each control's
// frames baked the first time it is drawn
void gui_controls_draw(const GuiControlsRasterCfg *args);
// the control the pointer at x, y is on, GUI_CONTROL_NONE off them all; and the rectangle of the control `which`
unsigned int gui_controls_hit(const GuiControlsCfg *args);
GuiRect gui_control_rect(const GuiControlsCfg *args);

struct GuiControlsRasterCfg
{
    GuiRaster *raster;
    const GuiPalette *palette;
    const GuiFrame *frame;
    GuiButtons *buttons;
    unsigned long long time_ms;
};

struct GuiControlsCfg
{
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int which;
};

typedef struct
{
    void (*draw)(const GuiControlsRasterCfg *args);
} GuiControlsRasterNs;
EMBED_TABLE_LAYOUT(GuiControlsRasterNs, draw);
EMBED_TABLE_STORAGE GuiControlsRasterNs gui_controls_raster EMBED_UNUSED = {
    .draw = gui_controls_draw,
};

typedef struct
{
    unsigned int (*hit)(const GuiControlsCfg *args);
    GuiRect (*rect)(const GuiControlsCfg *args);
} GuiControlsNs;
EMBED_TABLE_LAYOUT(GuiControlsNs, hit, rect);
EMBED_TABLE_STORAGE GuiControlsNs gui_controls EMBED_UNUSED = {
    .hit = gui_controls_hit,
    .rect = gui_control_rect,
};

#endif
