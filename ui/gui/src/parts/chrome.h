// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chrome.h: the parts about the work: the bar with the tree's name and the three window controls at its right end
// and the status bar across the foot, each tinted with the ink over the ground; and which control the pointer stands
// on (bar.c, controls.c, statusbar.c). The menu bar's titles on the bar are the menu bar part's (menubar.h) and the
// strip down the left is its own part (strip.h)
#ifndef GUI_PARTS_CHROME_H
#define GUI_PARTS_CHROME_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"

// the controls, left to right, and none
#define GUI_CONTROL_MINIMIZE 0u
#define GUI_CONTROL_MAXIMIZE 1u
#define GUI_CONTROL_CLOSE 2u
#define GUI_CONTROL_NONE 3u
// the interface text's size in the page's pixels, and a control's glyph's box
#define GUI_HEAD_SIZE 13u
#define GUI_CONTROL_GLYPH 14

// the bar drawn: its tint across the frame's bar, `title`, the tree's name, in the interface face at its right before
// the controls, and the controls
void gui_bar_drawn(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const GuiFrame *frame,
                   const char *title);
// the three controls drawn in the frame's controls rectangle, minimize, maximize and close; and the control the
// pointer at x, y stands on, GUI_CONTROL_NONE off them all
void gui_controls_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame);
unsigned int gui_controls_hit(const GuiFrame *frame, int x, int y);
// the status bar drawn, its tint
void gui_statusbar_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame);
#endif
