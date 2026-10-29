// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.c: the three window controls at the bar's right end, each its glyph in a box of GUI_CONTROL_GLYPH pixels
// centred in its button: minimize a line across the middle, maximize a square, close two diagonals, as the page's
// icons draw them
#include "chrome.h"

// the rectangle of the control `which` among the frame's controls
static GuiRect controls_button(const GuiFrame *frame, unsigned int which)
{
    const int wide = gui_scaled(GUI_CONTROL_WIDE, frame->scale);
    const int gap = gui_scaled(GUI_CONTROL_GAP, frame->scale);
    // a control's index is one of three
    const int at = frame->controls.x + ((int)which * (wide + gap));
    const GuiRect button = {at, frame->controls.y, wide, frame->controls.height};
    return button;
}

void gui_controls_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame)
{
    const int box = gui_scaled(GUI_CONTROL_GLYPH, frame->scale);
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        const GuiRect button = controls_button(frame, which);
        const int left = button.x + ((button.width - box) / 2);
        const int top = button.y + ((button.height - box) / 2);
        // the glyphs' strokes in the box, as the icons stand in theirs of sixteen: the line at 3.5..12.5 by 8, the
        // square at 4..12, the diagonals from 4.2 to 11.8
        const int low = left + ((box * 4) / 16);
        const int high = left + ((box * 12) / 16) - 1;
        const int middle = top + (box / 2);
        const int above = top + ((box * 4) / 16);
        const int below = top + ((box * 12) / 16) - 1;
        if (which == GUI_CONTROL_MINIMIZE)
        {
            gui_raster_line(raster, low, middle, high, middle, palette->status_text);
        }
        else if (which == GUI_CONTROL_MAXIMIZE)
        {
            gui_raster_line(raster, low, above, high, above, palette->status_text);
            gui_raster_line(raster, low, below, high, below, palette->status_text);
            gui_raster_line(raster, low, above, low, below, palette->status_text);
            gui_raster_line(raster, high, above, high, below, palette->status_text);
        }
        else
        {
            gui_raster_line(raster, low, above, high, below, palette->status_text);
            gui_raster_line(raster, high, above, low, below, palette->status_text);
        }
    }
}

unsigned int gui_controls_hit(const GuiFrame *frame, int x, int y)
{
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        if (gui_rect_holds(controls_button(frame, which), x, y))
        {
            return which;
        }
    }
    return GUI_CONTROL_NONE;
}
