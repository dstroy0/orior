// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bar.c: the bar across the top: its tint, the tree's name in the interface face at its right before the controls,
// as the page's bar ends with it, and the controls at its right end; the menu bar's titles are drawn over it by the
// menu bar part
#include "chrome.h"

#include <math.h>

// the gap in the page's pixels between the tree's name and the controls, 0.7rem, and the name's most width, 16rem
#define GUI_BAR_GAP 11
#define GUI_BAR_NAME_WIDE 256

void gui_bar_drawn(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const GuiFrame *frame,
                   const char *title)
{
    gui_raster_filled(raster, frame->bar, palette->glass);
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_HEAD_SIZE, frame->scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD, size);
    const int baseline = frame->bar.y + (int)((((float)frame->bar.height - ascent - descent) / 2.0f) + ascent + 0.5f);
    unsigned long long length = 0ull;
    while (title[length] != '\0')
    {
        length += 1ull;
    }
    const int most = gui_scaled(GUI_BAR_NAME_WIDE, frame->scale);
    const float measured = gui_text_width(glyphs, GUI_FACE_HEAD, size, title, length);
    const int wide = (measured > (float)most) ? most : (int)roundf(measured);
    const int right = frame->controls.x - gui_scaled(GUI_BAR_GAP, frame->scale);
    const GuiRect clip = {right - wide, frame->bar.y, wide, frame->bar.height};
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(right - wide), baseline, title, length,
                         palette->bar_text, gui_rect_met(clip, frame->bar));
    gui_controls_drawn(raster, palette, frame);
}
