// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar.c: the status bar across the foot, tinted with the ink
#include "chrome.h"

void gui_statusbar_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame)
{
    gui_raster_filled(raster, frame->status, palette->glass);
}
