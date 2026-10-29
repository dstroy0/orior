// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.h: the parts and their rectangles for the window's size and the system's scale, as the page lays them out:
// the bar across the top with the three controls at its right end, the status bar across the foot, the strip down the
// left and the frame's gap down the right, and the work between them, its inside the work less its padding, a side
// pane at the inside's left where one is shown, its tabs row at its head past the side, the minimap's column at its
// right under the tabs row and the editor filling the rest; every size the page's pixels times the scale (frame.c)
#ifndef GUI_FRAME_H
#define GUI_FRAME_H
#include "raster.h"

// the parts' sizes in the page's pixels: the bar's height, --bar-height; the status bar's; the strip's width,
// --strip-width; the frame's gap at the right, --frame-right; the side pane's width, --side; the tabs row's height,
// --tabs-tall; the minimap's width, .ed-mini; the work's padding, top, right, bottom and left; the work's corner
// radius; a window control's width, the gap between the controls and how far their row ends from the bar's right end
#define GUI_BAR_TALL 41
#define GUI_STATUS_TALL 27
#define GUI_STRIP_WIDE 44
#define GUI_FRAME_GAP 12
#define GUI_SIDE_WIDE 304
#define GUI_TABS_TALL 31
#define GUI_MAP_WIDE 84
#define GUI_WORK_PAD_TOP 2
#define GUI_WORK_PAD_RIGHT 2
#define GUI_WORK_PAD_BOTTOM 1
#define GUI_WORK_PAD_LEFT 1
#define GUI_WORK_RADIUS 7
#define GUI_CONTROL_WIDE 34
#define GUI_CONTROL_GAP 2
#define GUI_CONTROLS_END 5
#define GUI_CONTROLS 3

// The frame: the window's size and scale it was laid out for, and each part's rectangle, the side's of no width
// where none is shown, the map's at the editor's right
typedef struct
{
    unsigned int width;
    unsigned int height;
    float scale;
    GuiRect bar;
    GuiRect controls;
    GuiRect status;
    GuiRect strip;
    GuiRect gap;
    GuiRect work;
    GuiRect inside;
    GuiRect side;
    GuiRect tabs;
    GuiRect editor;
    GuiRect map;
    unsigned int radius;
} GuiFrame;

// a size in the page's pixels at `scale`, rounded to a whole pixel
int gui_scaled(int pixels, float scale);
// the parts laid out for a window `width` by `height` at `scale`, with no side shown
void gui_frame_laid(GuiFrame *frame, unsigned int width, unsigned int height, float scale);
// the work's inside laid again with a side shown at its left where `shown` is 1, the tabs row and the editor past it,
// or with no side where it is 0
void gui_frame_sided(GuiFrame *frame, int shown);
#endif
