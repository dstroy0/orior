// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides.c: the side pane's waits. Each begins as the pointer comes onto a place or leaves it, and a tick runs those
// it reaches: the pane collapses only where, at that moment, the pointer stands on neither it nor the strip and no menu
// is open, and is looked at again a rest later. Each change of the pane's collapse begins its slide from where it
// stands, read on the claim's table of the page's ease
#include "sides.h"

#include "claim.h"

typedef struct
{
    GuiSides *sides;
    const GuiSides *sides_read;
    GuiStrip *strip;
    const GuiStrip *strip_read;
    const GuiFrame *frame;
    int collapsed;
    int still;
    unsigned long long time_ms;
} SidesCtx;

// the share of its way the pane has gone at `time_ms` on its slide
EMBED_INLINE int sides_away_at(const SidesCtx *args)
{
    return EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &args->sides_read->slide,
                      .time_ms = args->time_ms, .lasts_ms = GUI_SIDES_SLIDE_MS);
}

// the pane's collapse set to `collapsed` at `time_ms`, its slide begun from where it stands toward the way it goes, or
// the pane standing there at once where motion is still
EMBED_INLINE void sides_collapsed_set(const SidesCtx *args)
{
    GuiSides *const sides = args->sides;
    const int to = args->collapsed ? GUI_FIXED_ONE : 0;
    if (args->still)
    {
        sides->slide = (GuiSlide){.from = to, .to = to, .curve = GUI_CURVE_BEAD};
    }
    else
    {
        // the pane's slide is sent on from where it stands, a bead's speed kept with it
        sides->slide.curve = GUI_CURVE_BEAD;
        EMBED_CALL(gui_autohide_target.slide_to, GuiAutohideTargetCfg, .slide_out = &sides->slide, .to = to,
                   .time_ms = args->time_ms, .lasts_ms = GUI_SIDES_SLIDE_MS);
    }
    args->strip->collapsed = args->collapsed;
}

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
    sides->slide = (GuiSlide){.curve = GUI_CURVE_BEAD};
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
    return (GuiRect){
        frame->inside.x, frame->inside.y,
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDES_EDGE, .scale = frame->scale),
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
        EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip, .collapsed = 1, .still = args->still,
                   .time_ms = args->time_ms);
        sides->collapse_ms = 0ULL;
        return;
    }
    // a pane where none showed comes in from where a collapsed one stands
    if (!EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = strip))
    {
        sides->slide = (GuiSlide){.from = GUI_FIXED_ONE, .to = GUI_FIXED_ONE, .curve = GUI_CURVE_BEAD};
    }
    strip->displayed = (int)args->at;
    EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip, .collapsed = 0, .still = args->still,
               .time_ms = args->time_ms);
    sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
}

int gui_sides_move(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    GuiStrip *const strip = args->strip;
    const GuiFrame *const frame = args->frame;
    const int has_pane = EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = strip);
    const int over_pane =
        has_pane && !strip->collapsed &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->side, .x = args->x, .y = args->y);
    const int over_strip =
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->strip, .x = args->x, .y = args->y);
    const int over_edge =
        has_pane && strip->collapsed &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(sides_edge, SidesCtx, .frame = frame), .x = args->x, .y = args->y);
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
    const int icon =
        (over_strip && (strip->pointed >= 0) && g_cli_tools[strip->pointed].side) ? strip->pointed : GUI_STRIP_NONE;
    if (icon != sides->icon)
    {
        sides->icon = icon;
        sides->icon_ms = (icon != GUI_STRIP_NONE) ? args->time_ms + GUI_SIDES_ICON_REST_MS : 0ULL;
    }
    // a preview the pointer leaves for neither the strip nor the pane stands through a rest, as a pane shown by a
    // press does: the rest begins as it leaves, and the wait to collapse takes the preview back where the pointer is
    // still off both when the rest ends
    const int left_preview =
        sides->previewing && (sides->over_strip || sides->over_pane) && !over_strip && !over_pane;
    sides->over_pane = over_pane;
    sides->over_strip = over_strip;
    sides->over_edge = over_edge;
    if (left_preview)
    {
        sides->collapse_ms = args->time_ms + GUI_SIDES_REST_MS;
    }
    return 0;
}

void gui_sides_leave(const GuiSidesCfg *args)
{
    GuiSides *const sides = args->sides;
    // the pane left, or a preview left from the strip: its rest begins
    if (sides->over_pane || (sides->previewing && sides->over_strip))
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
            EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip, .collapsed = 0,
                       .still = args->still, .time_ms = now_ms);
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
        // a pane where none showed comes in from where a collapsed one stands
        if (!has_pane)
        {
            sides->slide = (GuiSlide){.from = GUI_FIXED_ONE, .to = GUI_FIXED_ONE, .curve = GUI_CURVE_BEAD};
        }
        strip->displayed = sides->icon;
        EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip, .collapsed = 0, .still = args->still,
                   .time_ms = now_ms);
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
            else if (sides->previewing)
            {
                // a preview the pointer left: what showed before it shown again
                sides->previewing = 0;
                strip->displayed = sides->before_displayed;
                EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip,
                           .collapsed = sides->before_collapsed, .still = args->still, .time_ms = now_ms);
                sides->collapse_ms = now_ms + GUI_SIDES_REST_MS;
                changed = 1;
            }
            else
            {
                EMBED_CALL(sides_collapsed_set, SidesCtx, .sides = sides, .strip = strip, .collapsed = 1,
                           .still = args->still, .time_ms = now_ms);
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

int gui_sides_away(const GuiSidesCfg *args)
{
    return EMBED_CALL(sides_away_at, SidesCtx, .sides_read = args->sides_read, .time_ms = args->time_ms);
}

unsigned long long gui_sides_slide_end_ms(const GuiSidesCfg *args)
{
    return EMBED_CALL(gui_autohide_slide.slide_end_ms, GuiAutohideSlideCfg, .slide = &args->sides_read->slide,
                      .time_ms = args->time_ms, .lasts_ms = GUI_SIDES_SLIDE_MS);
}

int gui_sides_stands(const GuiSidesCfg *args)
{
    return EMBED_CALL(sides_has_pane, SidesCtx, .strip_read = args->strip_read) &&
           (!args->strip_read->collapsed ||
            (EMBED_CALL(gui_sides.slide_end_ms, GuiSidesCfg, .sides_read = args->sides_read,
                        .time_ms = args->time_ms) != 0ULL));
}
