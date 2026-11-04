// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// docks.c: a panel's arrival, the moment it began and the share of its way left at any moment after, read on a table
// of the page's ease-out as the claim's slides are read on the page's ease
#include "docks.h"

#include "claim.h"

// the ease-out's table, cubic-bezier(0, 0, 0.58, 1), laid from the curve itself the first time it is read
static int s_ease_out[GUI_EASE_STEPS + 1u];
static int s_ease_out_created;

typedef struct
{
    int share;
} DocksCtx;

// the page's ease-out at `share` of the way, 0 to GUI_FIXED_ONE
EMBED_INLINE int docks_ease_out(const DocksCtx *args)
{
    if (!s_ease_out_created)
    {
        EMBED_CALL(gui_autohide_table.ease_table_init, GuiAutohideTableCfg, .table = s_ease_out, .x1 = 0.0, .y1 = 0.0,
                   .x2 = 0.58, .y2 = 1.0);
        s_ease_out_created = 1;
    }
    return EMBED_CALL(gui_autohide_share.ease_table_read, GuiAutohideShareCfg, .table_read = s_ease_out,
                      .share = args->share);
}

void gui_docks_init(const GuiDocksCfg *args)
{
    args->docks->arrive_ms = 0ULL;
}

void gui_docks_show(const GuiDocksCfg *args)
{
    args->docks->arrive_ms = args->still ? 0ULL : args->time_ms;
}

int gui_docks_away(const GuiDocksCfg *args)
{
    const unsigned long long arrive_ms = args->docks_read->arrive_ms;
    if ((arrive_ms == 0ULL) || (args->time_ms >= arrive_ms + GUI_DOCKS_ARRIVING_MS))
    {
        return 0;
    }
    const unsigned long long elapsed_ms = (args->time_ms > arrive_ms) ? args->time_ms - arrive_ms : 0ULL;
    // a share of a time under a second is a word
    const int way = (int)((elapsed_ms * (unsigned long long)GUI_FIXED_ONE) / GUI_DOCKS_ARRIVING_MS);
    return GUI_FIXED_ONE - EMBED_CALL(docks_ease_out, DocksCtx, .share = way);
}

unsigned long long gui_docks_arrive_end_ms(const GuiDocksCfg *args)
{
    const unsigned long long arrive_ms = args->docks_read->arrive_ms;
    return ((arrive_ms != 0ULL) && (args->time_ms < arrive_ms + GUI_DOCKS_ARRIVING_MS))
               ? arrive_ms + GUI_DOCKS_ARRIVING_MS
               : 0ULL;
}
