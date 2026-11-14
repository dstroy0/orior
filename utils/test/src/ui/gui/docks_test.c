// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// docks_test.c: the docks' check: a panel displayed begins its arrival with all its way left, the ease-out brings it past
// half its way by half its time and leaves less at each moment after, and at GUI_DOCKS_ARRIVING_MS it has arrived and
// the arrival ends; the arrival's end is while it runs and not after; a panel displayed while motion is still is
// at once; and no arrival is before any panel shows
#include "../../../../../includes/window/docks.h"
#include "../cli/cli_check.h"

// the share of its way the panel has left at `time_ms`, and when its arrival ends
static int away(const GuiDocks *docks, unsigned long long time_ms)
{
    return EMBED_CALL(gui_docks.away, GuiDocksCfg, .docks_read = docks, .time_ms = time_ms);
}

static unsigned long long ends(const GuiDocks *docks, unsigned long long time_ms)
{
    return EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = docks, .time_ms = time_ms);
}

int main(void)
{
    GuiDocks docks;
    EMBED_CALL(gui_docks.init, GuiDocksCfg, .docks = &docks);
    cli_check((away(&docks, 500ULL) == 0) && (ends(&docks, 500ULL) == 0ULL),
              "before any panel shows none of the way is left and no arrival runs");

    EMBED_CALL(gui_docks.show, GuiDocksCfg, .docks = &docks, .time_ms = 1000ULL);
    cli_check(away(&docks, 1000ULL) == GUI_FIXED_ONE, "a panel displayed has all its way left as it begins");
    const int halfway = away(&docks, 1000ULL + (GUI_DOCKS_ARRIVING_MS / 2ULL));
    cli_check((halfway > 0) && (halfway < GUI_FIXED_ONE / 2),
              "by half its time the ease-out has brought the panel past half its way, with some left");
    cli_check((away(&docks, 1035ULL) > halfway) && (halfway > away(&docks, 1105ULL)),
              "and less of its way is left at each moment after");
    cli_check((ends(&docks, 1000ULL) == 1000ULL + GUI_DOCKS_ARRIVING_MS) &&
                  (ends(&docks, 1000ULL + GUI_DOCKS_ARRIVING_MS - 1ULL) == 1000ULL + GUI_DOCKS_ARRIVING_MS),
              "while it runs the arrival ends GUI_DOCKS_ARRIVING_MS from when the panel showed");
    cli_check((away(&docks, 1000ULL + GUI_DOCKS_ARRIVING_MS) == 0) &&
                  (ends(&docks, 1000ULL + GUI_DOCKS_ARRIVING_MS) == 0ULL),
              "at GUI_DOCKS_ARRIVING_MS the panel has arrived and the arrival no longer runs");

    EMBED_CALL(gui_docks.show, GuiDocksCfg, .docks = &docks, .still = 1, .time_ms = 2000ULL);
    cli_check((away(&docks, 2000ULL) == 0) && (ends(&docks, 2000ULL) == 0ULL),
              "a panel displayed while motion is still is at once");
    return cli_on_check();
}
