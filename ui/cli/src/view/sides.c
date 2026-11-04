// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides.c: the side pane's waits. Each begins as the pointer comes onto a place or leaves it, and a tick runs those
// it reaches: the pane collapses only where, at that moment, the pointer stands on neither it nor the strip and no menu
// is open, and is looked at again a rest later
#include "sides.h"

typedef struct
{
    const GuiStrip *strip_read;
    const GuiFrame *frame;
} SidesCtx;

void gui_sides_init(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    sides->over_pane = 0;
    sides->over_strip = 0;
    sides->over_edge = 0;
    sides->collapse_ms = 0ULL;
    sides->edge_ms = 0ULL;
    sides->edge_off_ms = 0ULL;
    sides->icon = GUI_STRIP_NONE;
    sides->icon_ms = 0ULL;
    sides->previewing = 0;
    sides->previewed = GUI_STRIP_NONE;
    sides->before_displayed = GUI_STRIP_NONE;
    sides->before_collapsed = 0;
}

void gui_sides_settle(const GuiSidesCfg *args)
{
    args->sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
}

// whether the strip's tool shown has a side pane
EMBED_INLINE int sides_has_pane(const SidesCtx *args)
{
    const int at = args->strip_read->displayed;
    // the tool shown is among the tools
    return (at >= 0) && g_cli_tools[at].side;
}

// the edge a collapsed pane comes back from: the view's left, GUI_SIDES_EDGE pixels wide, from the inside's head to
// the terminal's panel, or to the inside's foot where none is open
EMBED_INLINE GuiRect sides_edge(const SidesCtx *args)
{
    const GuiFrame *const frame = args->frame;
    const int foot = EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->terminal)
                         ? frame->inside.y + frame->inside.height
                         : frame->terminal.y;
    return (GuiRect){frame->inside.x, frame->inside.y,
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDES_EDGE,
                                .scale = frame->scale),
                     foot - frame->inside.y};
}

void gui_sides_toggle(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    GuiStrip *const strip = args->strip;
    sides->previewing = 0;
    // the tools are fewer than an int counts
    if ((strip->displayed == (int)args->at) && !strip->collapsed)
    {
        strip->collapsed = 1;
        sides->collapse_ms = 0ULL;
        return;
    }
    strip->displayed = (int)args->at;
    strip->collapsed = 0;
    sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
}

int gui_sides_move(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    GuiStrip *const strip = args->strip;
    const GuiFrame *const frame = args->frame;
    const int has_pane = EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = strip);
    const int over_pane = has_pane && !strip->collapsed &&
                          EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->side, .x = args->x,
                                     .y = args->y);
    const int over_strip =
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->strip, .x = args->x, .y = args->y);
    const int over_edge = has_pane && strip->collapsed &&
                          EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                     .rect = EMBED_CALL(sides_edge, SidesCtx, .frame = frame), .x = args->x,
                                     .y = args->y);
    // the pane: no wait while the pointer stands on it, and the wait begun as it leaves
    if (over_pane && !sides->over_pane)
    {
        sides->collapse_ms = 0ULL;
    }
    else if (!over_pane && sides->over_pane)
    {
        sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
    }
    // the edge: a rest begun as the pointer comes onto it, one back within the grace resting still, and the grace
    // begun as it leaves
    if (over_edge && !sides->over_edge)
    {
        if ((sides->edge_off_ms == 0ULL) || (args->time_ms >= sides->edge_off_ms))
        {
            sides->edge_ms = args->time_ms + GUI_SIDES_EDGE_REST_MS;
        }
        sides->edge_off_ms = 0ULL;
    }
    else if (!over_edge && sides->over_edge)
    {
        sides->edge_off_ms = args->time_ms + GUI_SIDES_EDGE_GRACE_MS;
    }
    // the icon of a side tool the pointer stands on: its rest begun as the pointer comes onto it and ended as it leaves
    const int icon = (over_strip && (strip->pointed >= 0) && g_cli_tools[strip->pointed].side) ? strip->pointed
                                                                                               : GUI_STRIP_NONE;
    if (icon != sides->icon)
    {
        sides->icon = icon;
        sides->icon_ms = (icon != GUI_STRIP_NONE) ? args->time_ms + GUI_SIDES_ICON_REST_MS : 0ULL;
    }
    sides->over_pane = over_pane;
    sides->over_strip = over_strip;
    sides->over_edge = over_edge;
    // a preview the pointer has left for neither the strip nor the pane: what showed before it shown again
    if (sides->previewing && !over_strip && !over_pane)
    {
        sides->previewing = 0;
        strip->displayed = sides->before_displayed;
        strip->collapsed = sides->before_collapsed;
        sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
        return 1;
    }
    return 0;
}

void gui_sides_leave(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    if (sides->over_pane)
    {
        sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
    }
    if (sides->over_edge)
    {
        sides->edge_off_ms = args->time_ms + GUI_SIDES_EDGE_GRACE_MS;
    }
    sides->over_pane = 0;
    sides->over_strip = 0;
    sides->over_edge = 0;
    sides->icon = GUI_STRIP_NONE;
    sides->icon_ms = 0ULL;
}

int gui_sides_press(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    const GuiStrip *const strip = args->strip;
    if (!sides->previewing)
    {
        return 0;
    }
    const int on_strip = EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->strip,
                                    .x = args->x, .y = args->y);
    const int on_pane = !strip->collapsed && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                        .rect = args->frame->side, .x = args->x, .y = args->y);
    if (on_strip || on_pane)
    {
        sides->previewing = 0;
    }
    return on_strip && (strip->pointed == sides->previewed);
}

int gui_sides_tick(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    GuiStrip *const strip = args->strip;
    const unsigned long long now_ms = args->time_ms;
    const int has_pane = EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = strip);
    int changed = 0;
    // the grace off the edge run out: the rest on it ended
    if ((sides->edge_off_ms != 0ULL) && (now_ms >= sides->edge_off_ms))
    {
        sides->edge_off_ms = 0ULL;
        sides->edge_ms = 0ULL;
    }
    // the rest on the edge reached: the pane shown, looked at again a rest later
    if ((sides->edge_ms != 0ULL) && (now_ms >= sides->edge_ms))
    {
        sides->edge_ms = 0ULL;
        if (has_pane && strip->collapsed)
        {
            // the pane shown stands over the edge, which is gone with the pointer's place on it
            strip->collapsed = 0;
            sides->over_edge = 0;
            sides->edge_off_ms = 0ULL;
            sides->collapse_ms = now_ms + GUI_SIDES_REST_MS;
            changed = 1;
        }
    }
    // the rest on an icon reached: its tool's pane shown as a preview, what showed before saved where no preview stands
    if ((sides->icon_ms != 0ULL) && (now_ms >= sides->icon_ms))
    {
        sides->icon_ms = 0ULL;
        if (!sides->previewing)
        {
            sides->previewing = 1;
            sides->before_displayed = strip->displayed;
            sides->before_collapsed = strip->collapsed;
        }
        sides->previewed = sides->icon;
        changed = (strip->displayed != sides->icon) || strip->collapsed;
        strip->displayed = sides->icon;
        strip->collapsed = 0;
        sides->collapse_ms = now_ms + GUI_SIDES_REST_MS;
    }
    // the wait to collapse reached: the pane collapsed where the pointer stands on neither it nor the strip and no menu
    // is open, else looked at again a rest later
    if ((sides->collapse_ms != 0ULL) && (now_ms >= sides->collapse_ms))
    {
        sides->collapse_ms = 0ULL;
        if (EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = strip) && !strip->collapsed)
        {
            if (sides->over_pane || sides->over_strip || args->menu_open)
            {
                sides->collapse_ms = now_ms + GUI_SIDES_REST_MS;
            }
            else
            {
                strip->collapsed = 1;
                changed = 1;
            }
        }
    }
    return changed;
}

unsigned long long gui_sides_next_deadline_ms(const GuiSidesCfg *args)
{
    const GuiSides *const sides = args->sides_read;
    const unsigned long long waits[4] = {sides->collapse_ms, sides->edge_ms, sides->edge_off_ms, sides->icon_ms};
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if ((waits[at] != 0ULL) && ((earliest == 0ULL) || (waits[at] < earliest)))
        {
            earliest = waits[at];
        }
    }
    return earliest;
}
