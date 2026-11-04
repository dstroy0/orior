// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// docks.c: a panel's arrival as a slide of the way it has left, from all of it to none over GUI_DOCKS_ARRIVING_MS on
// the ease-out
#include "docks.h"

void gui_docks_init(const GuiDocksCfg *args)
{
    args->docks->arrive = (GuiSlide){0, 0, 0ULL};
}

void gui_docks_show(const GuiDocksCfg *args)
{
    args->docks->arrive = args->still ? (GuiSlide){0, 0, 0ULL} : (GuiSlide){GUI_FIXED_ONE, 0, args->time_ms};
}

int gui_docks_away(const GuiDocksCfg *args)
{
    return EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &args->docks_read->arrive,
                      .time_ms = args->time_ms, .lasts_ms = GUI_DOCKS_ARRIVING_MS, .curve = GUI_CURVE_EASE_OUT);
}

unsigned long long gui_docks_arrive_end_ms(const GuiDocksCfg *args)
{
    return EMBED_CALL(gui_autohide_slide.slide_end_ms, GuiAutohideSlideCfg, .slide = &args->docks_read->arrive,
                      .time_ms = args->time_ms, .lasts_ms = GUI_DOCKS_ARRIVING_MS);
}
