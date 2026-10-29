// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.c: the claim's timers and slides. A timer is a time it is due, 0 for none; each is run as the clock passes
// it. A slide is sent to a number from wherever it stands, as a style transition is, and a room's slide waits one
// slide before it runs where its edge's part is coming in, so the work takes the part's room once the part spans its
// edge
#include "claim.h"

#include "../../cli/src/command_line/cli_internal.h"

#include <math.h>

// the milliseconds between frames while a slide runs
#define CLAIM_FRAME 16ull

float gui_ease(float share)
{
    // the curve's x and y at the parameter t, its control points (0.25, 0.1) and (0.25, 1); t found for x by halving
    const float wanted = (share < 0.0f) ? 0.0f : ((share > 1.0f) ? 1.0f : share);
    float low = 0.0f;
    float high = 1.0f;
    float at = wanted;
    for (unsigned int step = 0u; step < 20u; step += 1u)
    {
        at = (low + high) / 2.0f;
        const float rest = 1.0f - at;
        const float x = (3.0f * rest * rest * at * 0.25f) + (3.0f * rest * at * at * 0.25f) + (at * at * at);
        if (x < wanted)
        {
            low = at;
        }
        else
        {
            high = at;
        }
    }
    const float rest = 1.0f - at;
    return (3.0f * rest * rest * at * 0.1f) + (3.0f * rest * at * at * 1.0f) + (at * at * at);
}

float gui_slide_at(const GuiSlide *slide, unsigned long long now)
{
    const unsigned long long start = slide->began + slide->delay;
    if (now <= start)
    {
        return slide->from;
    }
    if (now >= start + GUI_CLAIM_SLIDE)
    {
        return slide->to;
    }
    // the way along the slide, a share of its length
    const float share = (float)(now - start) / (float)GUI_CLAIM_SLIDE;
    return slide->from + ((slide->to - slide->from) * gui_ease(share));
}

void gui_slide_to(GuiSlide *slide, float to, unsigned long long now, unsigned long long delay)
{
    if (slide->to == to)
    {
        return;
    }
    slide->from = gui_slide_at(slide, now);
    slide->to = to;
    slide->began = now;
    slide->delay = delay;
}

int gui_slide_running(const GuiSlide *slide, unsigned long long now)
{
    return (slide->from != slide->to) && (now < slide->began + slide->delay + GUI_CLAIM_SLIDE);
}

void gui_claim_made(GuiClaim *claim)
{
    memor.set(&(MemoriaCfg){.dst = claim, .val = 0, .bytes = sizeof(*claim)});
    claim->controls_alpha.from = 1.0f;
    claim->controls_alpha.to = 1.0f;
    claim->grown_top = 1.0f;
    claim->grown_left = 1.0f;
    claim->grown_bottom = 1.0f;
    claim->pulled_top = -1;
    claim->pulled_left = -1;
    claim->pulled_bottom = -1;
}

// whether a zone is the head's or the controls', which stand together: the one comes as the other goes
static int claim_together(unsigned int one, unsigned int other)
{
    return ((one == GUI_CLAIM_TOP) || (one == GUI_CLAIM_CONTROLS)) && ((other == GUI_CLAIM_TOP) || (other == GUI_CLAIM_CONTROLS));
}

// every slide sent where the state says, at `now`
static void claim_targets(GuiClaim *claim, unsigned long long now)
{
    const int on = claim->claimed;
    const unsigned int shown = on ? claim->shown : GUI_CLAIM_NONE;
    const int head = (shown == GUI_CLAIM_TOP) || (shown == GUI_CLAIM_CONTROLS);
    gui_slide_to(&claim->claim, on ? 1.0f : 0.0f, now, 0ull);
    gui_slide_to(&claim->reveal_top, (shown == GUI_CLAIM_TOP) ? 1.0f : 0.0f, now, 0ull);
    gui_slide_to(&claim->reveal_bottom, (shown == GUI_CLAIM_BOTTOM) ? 1.0f : 0.0f, now, 0ull);
    gui_slide_to(&claim->reveal_left, (shown == GUI_CLAIM_LEFT) ? 1.0f : 0.0f, now, 0ull);
    gui_slide_to(&claim->reveal_right, (shown == GUI_CLAIM_RIGHT) ? 1.0f : 0.0f, now, 0ull);
    gui_slide_to(&claim->reveal_down, head ? 1.0f : ((on && claim->near) ? GUI_CLAIM_GHOST_DOWN : 0.0f), now, 0ull);
    // a room comes a slide after its reveal and goes with it
    gui_slide_to(&claim->room_top, (shown == GUI_CLAIM_TOP) ? 1.0f : 0.0f, now, (shown == GUI_CLAIM_TOP) ? GUI_CLAIM_SLIDE : 0ull);
    gui_slide_to(&claim->room_left, (shown == GUI_CLAIM_LEFT) ? 1.0f : 0.0f, now, (shown == GUI_CLAIM_LEFT) ? GUI_CLAIM_SLIDE : 0ull);
    gui_slide_to(&claim->room_bottom, (shown == GUI_CLAIM_BOTTOM) ? 1.0f : 0.0f, now, (shown == GUI_CLAIM_BOTTOM) ? GUI_CLAIM_SLIDE : 0ull);
    gui_slide_to(&claim->controls_alpha, (on && claim->near && !head) ? GUI_CLAIM_GHOST_ALPHA : 1.0f, now, 0ull);
}

// `zone`'s part shown, its slat pulled from where the pointer stands, or none
static void claim_shown(GuiClaim *claim, unsigned int zone, unsigned long long now)
{
    if (zone == GUI_CLAIM_TOP)
    {
        claim->pulled_top = claim->pointer_x;
    }
    else if (zone == GUI_CLAIM_LEFT)
    {
        claim->pulled_left = claim->pointer_y;
    }
    else if (zone == GUI_CLAIM_BOTTOM)
    {
        claim->pulled_bottom = claim->pointer_x;
    }
    claim->shown = zone;
    // the ghost goes as a part comes
    claim->near = claim->near && (zone == GUI_CLAIM_NONE);
    claim_targets(claim, now);
}

// `zone`'s part shown, or none: another edge's part shown goes first, over its slide, and `zone`'s comes once it has
// gone
static void claim_revealed(GuiClaim *claim, unsigned int zone, unsigned long long now)
{
    claim->waiting = GUI_CLAIM_NONE;
    claim->switch_due = 0ull;
    const unsigned int shown = claim->shown;
    if ((zone != GUI_CLAIM_NONE) && (shown != GUI_CLAIM_NONE) && (zone != shown) && !claim_together(zone, shown))
    {
        claim_shown(claim, GUI_CLAIM_NONE, now);
        claim->waiting = zone;
        claim->switch_due = now + GUI_CLAIM_SLIDE;
        return;
    }
    claim_shown(claim, zone, now);
}

void gui_claim_set(GuiClaim *claim, int on, unsigned long long now)
{
    if (on == claim->claimed)
    {
        return;
    }
    claim->claimed = on;
    if (!on)
    {
        claim->rest_due = 0ull;
        claim->linger_due = 0ull;
        claim->near_due = 0ull;
        claim->zone_now = GUI_CLAIM_NONE;
        claim->near = 0;
        claim_revealed(claim, GUI_CLAIM_NONE, now);
        return;
    }
    claim_targets(claim, now);
}

unsigned int gui_claim_zone(unsigned int width, unsigned int height, float scale, int controls_wide, int x, int y)
{
    const int band = gui_scaled(GUI_FRAME_GAP * GUI_CLAIM_NEAR, scale);
    // the window's sides are far inside what an int represents
    const int wide = (int)width;
    const int tall = (int)height;
    if ((y < band) && (x > wide - controls_wide - band))
    {
        return GUI_CLAIM_CONTROLS;
    }
    if (y < band)
    {
        return GUI_CLAIM_TOP;
    }
    if (y > tall - band)
    {
        return GUI_CLAIM_BOTTOM;
    }
    if (x < band)
    {
        return GUI_CLAIM_LEFT;
    }
    if (x > wide - band)
    {
        return GUI_CLAIM_RIGHT;
    }
    return GUI_CLAIM_NONE;
}

// the pointer's pace over the last span of its path, with the point x, y at `now` its latest, in pixels a
// millisecond
static float claim_pace(GuiClaim *claim, int x, int y, unsigned long long now)
{
    if (claim->trail_count == GUI_CLAIM_TRAIL)
    {
        for (unsigned int at = 1u; at < GUI_CLAIM_TRAIL; at += 1u)
        {
            claim->trail[at - 1u] = claim->trail[at];
        }
        claim->trail_count -= 1u;
    }
    claim->trail[claim->trail_count] = (GuiTrailPoint){x, y, now};
    claim->trail_count += 1u;
    unsigned int first = 0u;
    while ((claim->trail_count - first > 2u) && (now - claim->trail[first].at > GUI_CLAIM_SPAN))
    {
        first += 1u;
    }
    if (first > 0u)
    {
        for (unsigned int at = first; at < claim->trail_count; at += 1u)
        {
            claim->trail[at - first] = claim->trail[at];
        }
        claim->trail_count -= first;
    }
    const GuiTrailPoint *const oldest = &claim->trail[0];
    const unsigned long long lapse = now - oldest->at;
    if (lapse == 0ull)
    {
        return 0.0f;
    }
    // the distance between two points of the window's pixels is far inside a float's reach
    return hypotf((float)(x - oldest->x), (float)(y - oldest->y)) / (float)lapse;
}

void gui_claim_moved(GuiClaim *claim, int x, int y, unsigned int zone, int over_parts, unsigned long long now)
{
    if (!claim->claimed)
    {
        return;
    }
    claim->pointer_x = x;
    claim->pointer_y = y;
    const int passing = claim_pace(claim, x, y, now) > GUI_CLAIM_CRITERION;
    // the controls' ghost: shown as the pointer slows in their corner, kept while it stays there, and gone as it
    // leaves the corner or as a part comes
    const int near = (claim->shown == GUI_CLAIM_NONE) && (zone == GUI_CLAIM_CONTROLS);
    claim->near_due = 0ull;
    if (!near)
    {
        claim->near = 0;
    }
    else if (!passing)
    {
        claim->near = 1;
    }
    else if (!claim->near)
    {
        // a pointer come into the corner fast shows the ghost once it has settled
        claim->near_due = now + (2ull * GUI_CLAIM_SPAN);
    }
    // a pointer passing through starts its rest again with each move, so the rest begins once it slows
    if ((zone != claim->zone_now) || passing)
    {
        claim->zone_now = zone;
        claim->rest_due = 0ull;
        if ((zone != GUI_CLAIM_NONE) && (zone != claim->shown))
        {
            claim->rest_due = now + GUI_CLAIM_RESTS;
            claim->rest_zone = zone;
        }
    }
    claim->over_parts = over_parts;
    if ((claim->shown != GUI_CLAIM_NONE) && (zone == GUI_CLAIM_NONE) && !over_parts)
    {
        claim->linger_due = now + GUI_CLAIM_LINGER;
    }
    else
    {
        claim->linger_due = 0ull;
    }
    claim_targets(claim, now);
}

void gui_claim_left(GuiClaim *claim)
{
    claim->rest_due = 0ull;
    claim->near_due = 0ull;
    claim->zone_now = GUI_CLAIM_NONE;
    claim->trail_count = 0u;
    claim->near = 0;
}

int gui_claim_stepped(GuiClaim *claim, unsigned long long now)
{
    int changed = 0;
    if ((claim->switch_due != 0ull) && (now >= claim->switch_due))
    {
        const unsigned int zone = claim->waiting;
        claim->switch_due = 0ull;
        claim->waiting = GUI_CLAIM_NONE;
        claim_shown(claim, zone, now);
        changed = 1;
    }
    if ((claim->rest_due != 0ull) && (now >= claim->rest_due))
    {
        claim->rest_due = 0ull;
        claim_revealed(claim, claim->rest_zone, now);
        changed = 1;
    }
    if ((claim->linger_due != 0ull) && (now >= claim->linger_due))
    {
        if (claim->over_parts)
        {
            claim->linger_due = now + GUI_CLAIM_LINGER;
        }
        else
        {
            claim->linger_due = 0ull;
            claim_revealed(claim, GUI_CLAIM_NONE, now);
            changed = 1;
        }
    }
    if ((claim->near_due != 0ull) && (now >= claim->near_due))
    {
        claim->near_due = 0ull;
        claim->near = (claim->shown == GUI_CLAIM_NONE) && (claim->zone_now == GUI_CLAIM_CONTROLS);
        claim_targets(claim, now);
        changed = 1;
    }
    return changed;
}

// the earlier of two due times, 0 standing for none
static unsigned long long claim_sooner(unsigned long long one, unsigned long long other)
{
    if (one == 0ull)
    {
        return other;
    }
    return ((other == 0ull) || (one < other)) ? one : other;
}

int gui_claim_moving(const GuiClaim *claim, unsigned long long now)
{
    return gui_slide_running(&claim->claim, now) || gui_slide_running(&claim->reveal_top, now) ||
           gui_slide_running(&claim->reveal_bottom, now) || gui_slide_running(&claim->reveal_left, now) ||
           gui_slide_running(&claim->reveal_right, now) || gui_slide_running(&claim->reveal_down, now) ||
           gui_slide_running(&claim->room_top, now) || gui_slide_running(&claim->room_left, now) ||
           gui_slide_running(&claim->room_bottom, now) || gui_slide_running(&claim->controls_alpha, now);
}

unsigned long long gui_claim_due(const GuiClaim *claim, unsigned long long now)
{
    if (gui_claim_moving(claim, now))
    {
        return CLAIM_FRAME;
    }
    const unsigned long long due = claim_sooner(claim_sooner(claim->switch_due, claim->rest_due),
                                                claim_sooner(claim->linger_due, claim->near_due));
    if (due == 0ull)
    {
        return 0ull;
    }
    return (due > now) ? due - now : 1ull;
}

// a slat's grown share: coming in, it grows with its reveal; going, it stands as long as it grew; outside the claim it
// is whole
static float claim_grown(float claim, float reveal, float *last, float grown)
{
    float now = grown;
    if (claim < 1.0f)
    {
        now = 1.0f;
    }
    else if (reveal > *last)
    {
        now = reveal;
    }
    else if (reveal == 0.0f)
    {
        now = 0.0f;
    }
    *last = reveal;
    return now;
}

void gui_claim_numbers(GuiClaim *claim, unsigned long long now, GuiClaimNumbers *numbers)
{
    numbers->claim = gui_slide_at(&claim->claim, now);
    numbers->reveal_top = gui_slide_at(&claim->reveal_top, now);
    numbers->reveal_bottom = gui_slide_at(&claim->reveal_bottom, now);
    numbers->reveal_left = gui_slide_at(&claim->reveal_left, now);
    numbers->reveal_right = gui_slide_at(&claim->reveal_right, now);
    numbers->reveal_down = gui_slide_at(&claim->reveal_down, now);
    numbers->room_top = gui_slide_at(&claim->room_top, now);
    numbers->room_left = gui_slide_at(&claim->room_left, now);
    numbers->room_bottom = gui_slide_at(&claim->room_bottom, now);
    numbers->controls_alpha = gui_slide_at(&claim->controls_alpha, now);
    claim->grown_top = claim_grown(numbers->claim, numbers->reveal_top, &claim->last_top, claim->grown_top);
    claim->grown_left = claim_grown(numbers->claim, numbers->reveal_left, &claim->last_left, claim->grown_left);
    claim->grown_bottom = claim_grown(numbers->claim, numbers->reveal_bottom, &claim->last_bottom, claim->grown_bottom);
    numbers->grown_top = claim->grown_top;
    numbers->grown_left = claim->grown_left;
    numbers->grown_bottom = claim->grown_bottom;
    numbers->pulled_top = claim->pulled_top;
    numbers->pulled_left = claim->pulled_left;
    numbers->pulled_bottom = claim->pulled_bottom;
}
