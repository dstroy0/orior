// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the parts laid out for the window's size and scale; a part a window too small leaves no room for is a
// rectangle of none
#include "frame.h"

int gui_scaled(int pixels, float scale)
{
    // a size in pixels, scaled and rounded, is far inside what an int represents
    return (int)(((float)pixels * scale) + 0.5f);
}

// the work's inside laid out: the inside the work less its padding, the side `side_wide` pixels of it at the left,
// and the tabs row and the editor in the rest
static void frame_inside_laid(GuiFrame *frame, int side_wide)
{
    const float scale = frame->scale;
    const int pad_top = gui_scaled(GUI_WORK_PAD_TOP, scale);
    const int pad_right = gui_scaled(GUI_WORK_PAD_RIGHT, scale);
    const int pad_bottom = gui_scaled(GUI_WORK_PAD_BOTTOM, scale);
    const int pad_left = gui_scaled(GUI_WORK_PAD_LEFT, scale);
    const int tabs_tall = gui_scaled(GUI_TABS_TALL, scale);
    const int inner_wide = frame->work.width - pad_left - pad_right;
    const int inner_tall = frame->work.height - pad_top - pad_bottom;
    const int side = (side_wide > inner_wide) ? ((inner_wide > 0) ? inner_wide : 0) : side_wide;
    frame->inside = (GuiRect){frame->work.x + pad_left, frame->work.y + pad_top, inner_wide, inner_tall};
    frame->side = (GuiRect){frame->inside.x, frame->inside.y, side, inner_tall};
    frame->tabs = (GuiRect){frame->inside.x + side, frame->inside.y, inner_wide - side, tabs_tall};
    frame->editor = (GuiRect){frame->tabs.x, frame->tabs.y + tabs_tall, inner_wide - side, inner_tall - tabs_tall};
}

void gui_frame_sided(GuiFrame *frame, int shown)
{
    frame_inside_laid(frame, shown ? gui_scaled(GUI_SIDE_WIDE, frame->scale) : 0);
}

void gui_frame_laid(GuiFrame *frame, unsigned int width, unsigned int height, float scale)
{
    // the window's sides are far inside what an int represents
    const int wide = (int)width;
    const int tall = (int)height;
    const int bar_tall = gui_scaled(GUI_BAR_TALL, scale);
    const int status_tall = gui_scaled(GUI_STATUS_TALL, scale);
    const int strip_wide = gui_scaled(GUI_STRIP_WIDE, scale);
    const int gap_wide = gui_scaled(GUI_FRAME_GAP, scale);
    const int middle_tall = tall - bar_tall - status_tall;
    frame->width = width;
    frame->height = height;
    frame->scale = scale;
    frame->bar = (GuiRect){0, 0, wide, bar_tall};
    frame->status = (GuiRect){0, tall - status_tall, wide, status_tall};
    frame->strip = (GuiRect){0, bar_tall, strip_wide, middle_tall};
    frame->gap = (GuiRect){wide - gap_wide, bar_tall, gap_wide, middle_tall};
    frame->work = (GuiRect){strip_wide, bar_tall, wide - strip_wide - gap_wide, middle_tall};
    frame_inside_laid(frame, 0);
    const int control_wide = gui_scaled(GUI_CONTROL_WIDE, scale);
    const int control_gap = gui_scaled(GUI_CONTROL_GAP, scale);
    const int controls_wide = (GUI_CONTROLS * control_wide) + ((GUI_CONTROLS - 1) * control_gap);
    frame->controls = (GuiRect){wide - gui_scaled(GUI_CONTROLS_END, scale) - controls_wide, 0, controls_wide, bar_tall};
    // a radius in pixels is not negative
    frame->radius = (unsigned int)gui_scaled(GUI_WORK_RADIUS, scale);
}
