// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.c: the claim's timers and slides. A timer is a deadline, 0 while disarmed, run once the clock passes it. A
// slide is sent to a number from wherever it stands, as a style transition is, and a inset's slide waits one slide
// before it runs where its edge's part is coming in, so the work takes the part's inset once the part spans its edge
#include "claim.h"

#include "../command_line/cli_internal.h"

#include <math.h>

// the milliseconds between frames while a slide runs
#define CLAIM_FRAME_MS 16ULL

// the ease curve's table, made once from the curve itself
static int s_ease[GUI_EASE_STEPS + 1u];
static int s_ease_created;

typedef struct
{
    double x1;
    double y1;
    double x2;
    double y2;
    double required;
    unsigned int one;
    unsigned int other;
    GuiAutohide *claim;
    GuiSlide *slide;
    int to;
    unsigned long long time_ms;
    unsigned long long delay_ms;
    unsigned int zone;
    int x;
    int y;
    unsigned long long end_ms;
    const GuiSlide *slide_read;
    unsigned long long first_ms;
    unsigned long long second_ms;
    int autohide_i32;
    int reveal;
    int *last;
    int grown;
} ClaimCtx;
// a curve's y at the x `required`, 0 to 1: the curve's x and y at the parameter t, t found for x by halving
EMBED_INLINE double autohide_curve(const ClaimCtx *args)
{
    double low = 0.0;
    double high = 1.0;
    double at = args->required;
    for (unsigned int step = 0u; step < 24u; step += 1u)
    {
        at = (low + high) / 2.0;
        const double rest = 1.0 - at;
        const double x = (3.0 * rest * rest * at * args->x1) + (3.0 * rest * at * at * args->x2) + (at * at * at);
        if (x < args->required)
        {
            low = at;
        }
        else
        {
            high = at;
        }
    }
    const double rest = 1.0 - at;
    return (3.0 * rest * rest * at * args->y1) + (3.0 * rest * at * at * args->y2) + (at * at * at);
}

void gui_ease_table_init(const GuiAutohideTableCfg *args)
{
    for (unsigned int step = 1u; step < GUI_EASE_STEPS; step += 1u)
    {
        // a number from 0 to 1 by GUI_FIXED_ONE, rounded, is a word
        args->table[step] = (int)((EMBED_CALL(autohide_curve, ClaimCtx, .x1 = args->x1, .y1 = args->y1, .x2 = args->x2,
                                              .y2 = args->y2, .required = (double)step / (double)GUI_EASE_STEPS) *
                                   (double)GUI_FIXED_ONE) +
                                  0.5);
    }
    args->table[0] = 0;
    args->table[GUI_EASE_STEPS] = GUI_FIXED_ONE;
}

int gui_ease_table_read(const GuiAutohideShareCfg *args)
{
    int required;
    if (args->share < 0)
    {
        required = 0;
    }
    else if (args->share > GUI_FIXED_ONE)
    {
        required = GUI_FIXED_ONE;
    }
    else
    {
        required = args->share;
    }
    // a step of the table is GUI_FIXED_ONE over the steps, 1024; the way between two entries is laid straight
    const int step = required / (GUI_FIXED_ONE / (int)GUI_EASE_STEPS);
    const int within = required % (GUI_FIXED_ONE / (int)GUI_EASE_STEPS);
    if (step >= (int)GUI_EASE_STEPS)
    {
        return GUI_FIXED_ONE;
    }
    return args->table_read[step] +
           (((args->table_read[step + 1] - args->table_read[step]) * within) / (GUI_FIXED_ONE / (int)GUI_EASE_STEPS));
}

int gui_ease(const GuiAutohideShareCfg *args)
{
    if (!s_ease_created)
    {
        EMBED_CALL(gui_autohide_table.ease_table_init, GuiAutohideTableCfg, .table = s_ease, .x1 = 0.25, .y1 = 0.1,
                   .x2 = 0.25, .y2 = 1.0);
        s_ease_created = 1;
    }
    return EMBED_CALL(gui_autohide_share.ease_table_read, GuiAutohideShareCfg, .table_read = s_ease,
                      .share = args->share);
}

int gui_slide_at(const GuiAutohideSlideCfg *args)
{
    const unsigned long long start = args->slide->start_ms;
    if (args->time_ms <= start)
    {
        return args->slide->from;
    }
    if (args->time_ms >= start + GUI_AUTOHIDE_ANIMATION_MS)
    {
        return args->slide->to;
    }
    // the way along the slide, a share of its length, below GUI_FIXED_ONE; the slide's span by the curve's number is
    // within a long long and comes back to a number of GUI_FIXED_ONE
    const int share = (int)(((args->time_ms - start) * (unsigned long long)GUI_FIXED_ONE) / GUI_AUTOHIDE_ANIMATION_MS);
    return args->slide->from +
           (int)(((long long)(args->slide->to - args->slide->from) *
                  (long long)EMBED_CALL(gui_autohide_share.ease, GuiAutohideShareCfg, .share = share)) /
                 GUI_FIXED_ONE);
}

void gui_slide_to(const GuiAutohideTargetCfg *args)
{
    if (args->slide_out->to == args->to)
    {
        return;
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    args->slide_out->from =
        EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = args->slide_out, .time_ms = now_ms);
    args->slide_out->to = args->to;
    args->slide_out->start_ms = now_ms + args->delay_ms;
}

int gui_slide_running(const GuiAutohideSlideCfg *args)
{
    return (args->slide->from != args->slide->to) &&
           (args->time_ms < args->slide->start_ms + GUI_AUTOHIDE_ANIMATION_MS);
}

void gui_autohide_init(const GuiAutohideStillCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->claim, .val = 0, .bytes = sizeof(*args->claim)});
    args->claim->controls_alpha.from = GUI_FIXED_ONE;
    args->claim->controls_alpha.to = GUI_FIXED_ONE;
    args->claim->grown_top = GUI_FIXED_ONE;
    args->claim->grown_left = GUI_FIXED_ONE;
    args->claim->grown_bottom = GUI_FIXED_ONE;
    args->claim->pulled_top = -1;
    args->claim->pulled_left = -1;
    args->claim->pulled_bottom = -1;
}

// whether a zone is the head's or the controls', which stand together: the one comes as the other goes
EMBED_INLINE int autohide_together(const ClaimCtx *args)
{
    return ((args->one == GUI_AUTOHIDE_TOP) || (args->one == GUI_AUTOHIDE_CONTROLS)) &&
           ((args->other == GUI_AUTOHIDE_TOP) || (args->other == GUI_AUTOHIDE_CONTROLS));
}

// a slide of the claim's sent to `to`: counted where that is somewhere new, and standing there at once with less motion
// asked for
EMBED_INLINE void autohide_send(const ClaimCtx *args)
{
    if (args->slide->to == args->to)
    {
        return;
    }
    EMBED_CALL(gui_autohide_target.slide_to, GuiAutohideTargetCfg, .slide_out = args->slide, .to = args->to,
               .delay_ms = args->delay_ms);
    args->slide->from = args->claim->still ? args->to : args->slide->from;
    args->claim->sent += 1u;
}

// every slide sent where the state says
EMBED_INLINE void autohide_targets(const ClaimCtx *args)
{
    const int on = args->claim->autohidden;
    const unsigned int displayed = on ? args->claim->displayed : GUI_AUTOHIDE_NONE;
    const int head = (displayed == GUI_AUTOHIDE_TOP) || (displayed == GUI_AUTOHIDE_CONTROLS);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->claim,
               .to = on ? GUI_FIXED_ONE : 0, .delay_ms = 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->reveal_top,
               .to = (displayed == GUI_AUTOHIDE_TOP) ? GUI_FIXED_ONE : 0, .delay_ms = 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->reveal_bottom,
               .to = (displayed == GUI_AUTOHIDE_BOTTOM) ? GUI_FIXED_ONE : 0, .delay_ms = 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->reveal_left,
               .to = (displayed == GUI_AUTOHIDE_LEFT) ? GUI_FIXED_ONE : 0, .delay_ms = 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->reveal_right,
               .to = (displayed == GUI_AUTOHIDE_RIGHT) ? GUI_FIXED_ONE : 0, .delay_ms = 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->reveal_down,
               .to = head ? GUI_FIXED_ONE : ((on && args->claim->adjacent) ? GUI_AUTOHIDE_GHOST_DOWN : 0),
               .delay_ms = 0ULL);
    // a inset comes a slide after its reveal and goes with it
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->inset_top,
               .to = (displayed == GUI_AUTOHIDE_TOP) ? GUI_FIXED_ONE : 0,
               .delay_ms = (displayed == GUI_AUTOHIDE_TOP) ? GUI_AUTOHIDE_ANIMATION_MS : 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->inset_left,
               .to = (displayed == GUI_AUTOHIDE_LEFT) ? GUI_FIXED_ONE : 0,
               .delay_ms = (displayed == GUI_AUTOHIDE_LEFT) ? GUI_AUTOHIDE_ANIMATION_MS : 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->inset_bottom,
               .to = (displayed == GUI_AUTOHIDE_BOTTOM) ? GUI_FIXED_ONE : 0,
               .delay_ms = (displayed == GUI_AUTOHIDE_BOTTOM) ? GUI_AUTOHIDE_ANIMATION_MS : 0ULL);
    EMBED_CALL(autohide_send, ClaimCtx, .claim = args->claim, .slide = &args->claim->controls_alpha,
               .to = (on && args->claim->adjacent && !head) ? GUI_AUTOHIDE_GHOST_ALPHA : GUI_FIXED_ONE,
               .delay_ms = 0ULL);
}

// `zone`'s part shown, its slat pulled from where the pointer stands, or none
EMBED_INLINE void autohide_show(const ClaimCtx *args)
{
    if (args->zone == GUI_AUTOHIDE_TOP)
    {
        args->claim->pulled_top = args->claim->pointer_x;
    }
    else if (args->zone == GUI_AUTOHIDE_LEFT)
    {
        args->claim->pulled_left = args->claim->pointer_y;
    }
    else if (args->zone == GUI_AUTOHIDE_BOTTOM)
    {
        args->claim->pulled_bottom = args->claim->pointer_x;
    }
    args->claim->displayed = args->zone;
    // the ghost goes as a part comes
    args->claim->adjacent = args->claim->adjacent && (args->zone == GUI_AUTOHIDE_NONE);
    autohide_targets(args);
}

// `zone`'s part shown, or none: another edge's part shown goes first, over its slide, and `zone`'s comes once it has
// gone
EMBED_INLINE void autohide_reveal(const ClaimCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    args->claim->waiting = GUI_AUTOHIDE_NONE;
    args->claim->pending_show_deadline_ms = 0ULL;
    const unsigned int displayed = args->claim->displayed;
    if ((args->zone != GUI_AUTOHIDE_NONE) && (displayed != GUI_AUTOHIDE_NONE) && (args->zone != displayed) &&
        !EMBED_CALL(autohide_together, ClaimCtx, .one = args->zone, .other = displayed))
    {
        EMBED_CALL(autohide_show, ClaimCtx, .claim = args->claim, .zone = GUI_AUTOHIDE_NONE);
        args->claim->waiting = args->zone;
        // the part shown goes over a slide, or at once with less motion asked for
        args->claim->pending_show_deadline_ms = now_ms + (args->claim->still ? 1ULL : GUI_AUTOHIDE_ANIMATION_MS);
        return;
    }
    autohide_show(args);
}

void gui_autohide_set(const GuiAutohideOnCfg *args)
{
    if (args->on == args->claim->autohidden)
    {
        return;
    }
    args->claim->autohidden = args->on;
    if (!args->on)
    {
        args->claim->show_deadline_ms = 0ULL;
        args->claim->hide_deadline_ms = 0ULL;
        args->claim->preview_deadline_ms = 0ULL;
        args->claim->zone_now = GUI_AUTOHIDE_NONE;
        args->claim->adjacent = 0;
        EMBED_CALL(autohide_reveal, ClaimCtx, .claim = args->claim, .zone = GUI_AUTOHIDE_NONE);
        return;
    }
    EMBED_CALL(autohide_targets, ClaimCtx, .claim = args->claim);
}

unsigned int gui_autohide_zone(const GuiAutohideWidthCfg *args)
{
    const int band = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                .pixels = GUI_FRAME_GAP * GUI_AUTOHIDE_ADJACENT, .scale = args->scale);
    // the window's sides are far inside what an int represents
    const int pixel_width = (int)args->width;
    const int pixel_height = (int)args->height;
    if ((args->y < band) && (args->x > pixel_width - args->controls_width - band))
    {
        return GUI_AUTOHIDE_CONTROLS;
    }
    if (args->y < band)
    {
        return GUI_AUTOHIDE_TOP;
    }
    if (args->y > pixel_height - band)
    {
        return GUI_AUTOHIDE_BOTTOM;
    }
    if (args->x < band)
    {
        return GUI_AUTOHIDE_LEFT;
    }
    if (args->x > pixel_width - band)
    {
        return GUI_AUTOHIDE_RIGHT;
    }
    return GUI_AUTOHIDE_NONE;
}

// the pointer's speed over the last window of its path, with the point x, y its latest, in pixels a millisecond
EMBED_INLINE float autohide_pace(const ClaimCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->claim->trail_count == GUI_AUTOHIDE_TRAIL)
    {
        for (unsigned int at = 1u; at < GUI_AUTOHIDE_TRAIL; at += 1u)
        {
            args->claim->trail[at - 1u] = args->claim->trail[at];
        }
        args->claim->trail_count -= 1u;
    }
    args->claim->trail[args->claim->trail_count] = (GuiTrailPoint){args->x, args->y, now_ms};
    args->claim->trail_count += 1u;
    unsigned int first = 0u;
    while ((args->claim->trail_count - first > 2u) &&
           (now_ms - args->claim->trail[first].time_ms > GUI_AUTOHIDE_TRAIL_WINDOW_MS))
    {
        first += 1u;
    }
    if (first > 0u)
    {
        for (unsigned int at = first; at < args->claim->trail_count; at += 1u)
        {
            args->claim->trail[at - first] = args->claim->trail[at];
        }
        args->claim->trail_count -= first;
    }
    const GuiTrailPoint *const oldest = &args->claim->trail[0];
    const unsigned long long lapse = now_ms - oldest->time_ms;
    if (lapse == 0ULL)
    {
        return 0.0f;
    }
    // the distance between two points of the window's pixels is far inside a float's reach
    return hypotf((float)(args->x - oldest->x), (float)(args->y - oldest->y)) / (float)lapse;
}

void gui_autohide_move(const GuiAutohidePointerCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int over_parts = args->over_parts;
    if (!args->claim->autohidden)
    {
        return;
    }
    args->claim->pointer_x = args->x;
    args->claim->pointer_y = args->y;
    const int passing = EMBED_CALL(autohide_pace, ClaimCtx, .claim = args->claim, .x = args->x, .y = args->y) >
                        GUI_AUTOHIDE_SPEED_THRESHOLD;
    // the controls' ghost: shown as the pointer slows in their corner, saved while it stays there, and gone as it
    // leaves the corner or as a part comes
    const int adjacent = (args->claim->displayed == GUI_AUTOHIDE_NONE) && (args->zone == GUI_AUTOHIDE_CONTROLS);
    args->claim->preview_deadline_ms = 0ULL;
    if (!adjacent)
    {
        args->claim->adjacent = 0;
    }
    else if (!passing)
    {
        args->claim->adjacent = 1;
    }
    else if (!args->claim->adjacent)
    {
        // a pointer come into the corner fast shows the ghost once it has settled
        args->claim->preview_deadline_ms = now_ms + (2ULL * GUI_AUTOHIDE_TRAIL_WINDOW_MS);
    }
    // a pointer passing through restarts its rest timer with each move: the rest begins once it slows
    if ((args->zone != args->claim->zone_now) || passing)
    {
        args->claim->zone_now = args->zone;
        args->claim->show_deadline_ms = 0ULL;
        if ((args->zone != GUI_AUTOHIDE_NONE) && (args->zone != args->claim->displayed))
        {
            args->claim->show_deadline_ms = now_ms + GUI_AUTOHIDE_SHOW_DELAY_MS;
            args->claim->show_zone = args->zone;
        }
    }
    args->claim->over_parts = over_parts;
    if ((args->claim->displayed != GUI_AUTOHIDE_NONE) && (args->zone == GUI_AUTOHIDE_NONE) && !over_parts)
    {
        args->claim->hide_deadline_ms = now_ms + GUI_AUTOHIDE_HIDE_DELAY_MS;
    }
    else
    {
        args->claim->hide_deadline_ms = 0ULL;
    }
    EMBED_CALL(autohide_targets, ClaimCtx, .claim = args->claim);
}

void gui_autohide_leave(const GuiAutohideStillCfg *args)
{
    args->claim->show_deadline_ms = 0ULL;
    args->claim->preview_deadline_ms = 0ULL;
    args->claim->zone_now = GUI_AUTOHIDE_NONE;
    args->claim->trail_count = 0u;
    args->claim->adjacent = 0;
}

int gui_autohide_step(const GuiAutohideStateCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int changed = 0;
    if ((args->claim->pending_show_deadline_ms != 0ULL) && (now_ms >= args->claim->pending_show_deadline_ms))
    {
        const unsigned int zone = args->claim->waiting;
        args->claim->pending_show_deadline_ms = 0ULL;
        args->claim->waiting = GUI_AUTOHIDE_NONE;
        EMBED_CALL(autohide_show, ClaimCtx, .claim = args->claim, .zone = zone);
        changed = 1;
    }
    if ((args->claim->show_deadline_ms != 0ULL) && (now_ms >= args->claim->show_deadline_ms))
    {
        args->claim->show_deadline_ms = 0ULL;
        EMBED_CALL(autohide_reveal, ClaimCtx, .claim = args->claim, .zone = args->claim->show_zone);
        changed = 1;
    }
    if ((args->claim->hide_deadline_ms != 0ULL) && (now_ms >= args->claim->hide_deadline_ms))
    {
        if (args->claim->over_parts)
        {
            args->claim->hide_deadline_ms = now_ms + GUI_AUTOHIDE_HIDE_DELAY_MS;
        }
        else
        {
            args->claim->hide_deadline_ms = 0ULL;
            EMBED_CALL(autohide_reveal, ClaimCtx, .claim = args->claim, .zone = GUI_AUTOHIDE_NONE);
            changed = 1;
        }
    }
    if ((args->claim->preview_deadline_ms != 0ULL) && (now_ms >= args->claim->preview_deadline_ms))
    {
        args->claim->preview_deadline_ms = 0ULL;
        args->claim->adjacent =
            (args->claim->displayed == GUI_AUTOHIDE_NONE) && (args->claim->zone_now == GUI_AUTOHIDE_CONTROLS);
        EMBED_CALL(autohide_targets, ClaimCtx, .claim = args->claim);
        changed = 1;
    }
    return changed;
}

void gui_autohide_still(const GuiAutohideStillCfg *args)
{
    int still = args->still;
    args->claim->still = still;
    if (still)
    {
        GuiSlide *const slides[10] = {&args->claim->claim,         &args->claim->reveal_top,
                                      &args->claim->reveal_bottom, &args->claim->reveal_left,
                                      &args->claim->reveal_right,  &args->claim->reveal_down,
                                      &args->claim->inset_top,     &args->claim->inset_left,
                                      &args->claim->inset_bottom,  &args->claim->controls_alpha};
        for (unsigned int at = 0u; at < 10u; at += 1u)
        {
            slides[at]->from = slides[at]->to;
        }
        args->claim->sent += 1u;
    }
}

// the later of `end_ms` and the moment `slide` ends, where it runs at `time_ms`
EMBED_INLINE unsigned long long autohide_later(const ClaimCtx *args)
{
    const GuiSlide *slide = args->slide_read;
    if (!EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = slide, .time_ms = args->time_ms))
    {
        return args->end_ms;
    }
    const unsigned long long slide_end_ms = slide->start_ms + GUI_AUTOHIDE_ANIMATION_MS;
    return (slide_end_ms > args->end_ms) ? slide_end_ms : args->end_ms;
}

unsigned long long gui_autohide_animation_end_ms(const GuiAutohideCfg *args)
{
    unsigned long long end_ms = args->time_ms;
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->claim,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->reveal_top,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->reveal_bottom,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->reveal_left,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->reveal_right,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->reveal_down,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->inset_top,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->inset_left,
                        .time_ms = args->time_ms);
    end_ms = EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->inset_bottom,
                        .time_ms = args->time_ms);
    return EMBED_CALL(autohide_later, ClaimCtx, .end_ms = end_ms, .slide_read = &args->autohide_read->controls_alpha,
                      .time_ms = args->time_ms);
}

// the earlier of two deadlines, 0 while disarmed
EMBED_INLINE unsigned long long autohide_deadline_min(const ClaimCtx *args)
{
    if (args->first_ms == 0ULL)
    {
        return args->second_ms;
    }
    return ((args->second_ms == 0ULL) || (args->first_ms < args->second_ms)) ? args->first_ms : args->second_ms;
}

int gui_autohide_moving(const GuiAutohideCfg *args)
{
    return EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->claim,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->reveal_top,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg,
                      .slide = &args->autohide_read->reveal_bottom, .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->reveal_left,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg,
                      .slide = &args->autohide_read->reveal_right, .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->reveal_down,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->inset_top,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &args->autohide_read->inset_left,
                      .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg,
                      .slide = &args->autohide_read->inset_bottom, .time_ms = args->time_ms) ||
           EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg,
                      .slide = &args->autohide_read->controls_alpha, .time_ms = args->time_ms);
}

unsigned long long gui_autohide_next_deadline_ms(const GuiAutohideCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = args->autohide_read, .time_ms = now_ms))
    {
        return now_ms + CLAIM_FRAME_MS;
    }
    return EMBED_CALL(autohide_deadline_min, ClaimCtx,
                      .first_ms = EMBED_CALL(autohide_deadline_min, ClaimCtx,
                                             .first_ms = args->autohide_read->pending_show_deadline_ms,
                                             .second_ms = args->autohide_read->show_deadline_ms),
                      .second_ms =
                          EMBED_CALL(autohide_deadline_min, ClaimCtx, .first_ms = args->autohide_read->hide_deadline_ms,
                                     .second_ms = args->autohide_read->preview_deadline_ms));
}

// a slat's grown share: coming in, it grows with its reveal; going, it stands as long as it grew; outside the claim it
// is whole
EMBED_INLINE int autohide_grow(const ClaimCtx *args)
{
    int *last = args->last;
    int grown = args->grown;
    if (args->autohide_i32 < GUI_FIXED_ONE)
    {
        grown = GUI_FIXED_ONE;
    }
    else if (args->reveal > *last)
    {
        grown = args->reveal;
    }
    else if (args->reveal == 0)
    {
        grown = 0;
    }
    *last = args->reveal;
    return grown;
}

void gui_autohide_numbers(const GuiAutohideStateCfg *args)
{
    GuiAutohide *claim = args->claim;
    args->numbers->claim =
        EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &claim->claim, .time_ms = args->time_ms);
    args->numbers->reveal_top = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                           .slide = &claim->reveal_top, .time_ms = args->time_ms);
    args->numbers->reveal_bottom = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                              .slide = &claim->reveal_bottom, .time_ms = args->time_ms);
    args->numbers->reveal_left = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                            .slide = &claim->reveal_left, .time_ms = args->time_ms);
    args->numbers->reveal_right = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                             .slide = &claim->reveal_right, .time_ms = args->time_ms);
    args->numbers->reveal_down = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                            .slide = &claim->reveal_down, .time_ms = args->time_ms);
    args->numbers->inset_top = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &claim->inset_top,
                                          .time_ms = args->time_ms);
    args->numbers->inset_left = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                           .slide = &claim->inset_left, .time_ms = args->time_ms);
    args->numbers->inset_bottom = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                             .slide = &claim->inset_bottom, .time_ms = args->time_ms);
    args->numbers->controls_alpha = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg,
                                               .slide = &claim->controls_alpha, .time_ms = args->time_ms);
    claim->grown_top =
        EMBED_CALL(autohide_grow, ClaimCtx, .autohide_i32 = args->numbers->claim, .reveal = args->numbers->reveal_top,
                   .last = &claim->last_top, .grown = claim->grown_top);
    claim->grown_left =
        EMBED_CALL(autohide_grow, ClaimCtx, .autohide_i32 = args->numbers->claim, .reveal = args->numbers->reveal_left,
                   .last = &claim->last_left, .grown = claim->grown_left);
    claim->grown_bottom =
        EMBED_CALL(autohide_grow, ClaimCtx, .autohide_i32 = args->numbers->claim,
                   .reveal = args->numbers->reveal_bottom, .last = &claim->last_bottom, .grown = claim->grown_bottom);
    args->numbers->grown_top = claim->grown_top;
    args->numbers->grown_left = claim->grown_left;
    args->numbers->grown_bottom = claim->grown_bottom;
    args->numbers->pulled_top = claim->pulled_top;
    args->numbers->pulled_left = claim->pulled_left;
    args->numbers->pulled_bottom = claim->pulled_bottom;
}
