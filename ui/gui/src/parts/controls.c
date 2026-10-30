// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.c: the three window controls at the bar's right end, each its glyph in a box of GUI_CONTROL_GLYPH pixels
// centred in its button: minimize a line across the middle, maximize a square, close two diagonals, as the page's
// icons draw them
#include "chrome.h"

#include "glass.h"

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

// `color` at `share`, a number of GUI_WHOLE, of its alpha
static GuiColor controls_faded(GuiColor color, int share)
{
    const int within = (share < 0) ? 0 : ((share > GUI_WHOLE) ? GUI_WHOLE : share);
    // an alpha by a share of GUI_WHOLE, rounded, is a byte
    return (color & 0x00ffffffu) | ((GuiColor)((((int)GUI_ALPHA_OF(color) * within) + (GUI_WHOLE / 2)) / GUI_WHOLE) << 24u);
}

void gui_controls_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame, GuiUnder table)
{
    const int box = gui_scaled(GUI_CONTROL_GLYPH, frame->scale);
    // the controls' own pane, with its lip, while they stand off the bar as their tab or its ghost: a pane on the
    // table whose tint thickens from the sheet's to a pane's by the share, whole while it comes down, and its
    // corners alone once it stands, the base laying the rest of it
    const GuiColor pane = gui_glass_pane_tint(palette->glass);
    if (frame->controls_pane_share >= GUI_WHOLE)
    {
        gui_raster_paned(raster, table, frame->controls_pane, frame->radius, pane, 1);
    }
    else if (frame->controls_pane_share > 0)
    {
        const int sheet = (int)GUI_ALPHA_OF(table.over);
        const int share = frame->controls_pane_share;
        // an alpha between two bytes by a share of GUI_WHOLE, rounded, is a byte
        const GuiColor alpha = (GuiColor)(sheet + (((((int)GUI_ALPHA_OF(pane) - sheet) * share) + (GUI_WHOLE / 2)) / GUI_WHOLE));
        gui_raster_paned(raster, table, frame->controls_pane, frame->radius, (pane & 0x00ffffffu) | (alpha << 24u), 0);
    }
    const GuiColor ink = controls_faded(palette->status_text, frame->controls_alpha);
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
            gui_raster_line(raster, low, middle, high, middle, ink);
        }
        else if (which == GUI_CONTROL_MAXIMIZE)
        {
            gui_raster_line(raster, low, above, high, above, ink);
            gui_raster_line(raster, low, below, high, below, ink);
            gui_raster_line(raster, low, above, low, below, ink);
            gui_raster_line(raster, high, above, high, below, ink);
        }
        else
        {
            gui_raster_line(raster, low, above, high, below, ink);
            gui_raster_line(raster, high, above, low, below, ink);
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
