// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.h: the claim: while the reader edits, the work takes the bar's row, the strip's column and the status bar's
// row and keeps the frame's gap on every side, and inside the work the editor's text takes the tabs row's room and
// the minimap's. A pointer resting within three gaps of an edge brings that edge's part back: the status bar at the
// foot, the bar and the tabs row at the head, the strip at the left, the minimap at the right, and in the controls'
// corner the three controls alone as a tab, a ghost of them showing as soon as the pointer slows in the corner. A
// part goes back a linger after the pointer leaves it, and one edge's part comes only once another's has gone. A
// pointer crossing a gap faster than the criterion is passing through, not resting. Every number the parts move on
// runs from 0 to 1 and back on one curve over one slide, as the page's claim does; the claim keeps the numbers and
// the timers and says when it next wants the clock, and the frame lays the parts out for the numbers (claim.c)
#ifndef GUI_CLAIM_H
#define GUI_CLAIM_H
#include "frame.h"

// the edges whose parts a rest brings back, and none
#define GUI_CLAIM_NONE 0u
#define GUI_CLAIM_TOP 1u
#define GUI_CLAIM_BOTTOM 2u
#define GUI_CLAIM_LEFT 3u
#define GUI_CLAIM_RIGHT 4u
#define GUI_CLAIM_CONTROLS 5u
// the milliseconds a rest takes, a part lingers after the pointer leaves it, and a slide runs; the milliseconds of
// the pointer's path its pace is read over, and the pace, in pixels a millisecond, past which it is passing through;
// how many gaps in from an edge its band reaches; the controls' ghost's way down and the controls' opacity as the
// ghost, each a number of GUI_WHOLE; and the points of the pointer's path kept
#define GUI_CLAIM_RESTS 300ull
#define GUI_CLAIM_LINGER 450ull
#define GUI_CLAIM_SLIDE 300ull
#define GUI_CLAIM_SPAN 50ull
#define GUI_CLAIM_CRITERION 0.06f
#define GUI_CLAIM_NEAR 3
#define GUI_CLAIM_GHOST_DOWN 29491
#define GUI_CLAIM_GHOST_ALPHA 22938
#define GUI_CLAIM_TRAIL 16u

// A slide: a number of GUI_WHOLE, running from `from` to `to` over one slide on
// the page's ease curve, begun at `began` after
// `delay` milliseconds
typedef struct
{
    int from;
    int to;
    unsigned long long began;
    unsigned long long delay;
} GuiSlide;

// A point of the pointer's path: where and when
typedef struct
{
    int x;
    int y;
    unsigned long long at;
} GuiTrailPoint;

// The claim: whether it holds; the edge whose part is shown, the one waiting for it to go and when it comes; the
// pointer's edge, the edge whose rest runs and when it is due, when the linger is due, when the ghost is due; whether
// the controls' ghost shows; whether the pointer stands over the shown part, as its last move left it; where the
// pointer stands; its path over the last span; the slides: the claim, each edge's reveal, the controls' way down,
// each edge's room and the controls' opacity; each slat's grown share and its reveal as last read; and the point of
// each edge its slat is pulled from, -1 where none has been
typedef struct
{
    int claimed;
    unsigned int shown;
    unsigned int waiting;
    unsigned long long switch_due;
    unsigned int zone_now;
    unsigned int rest_zone;
    unsigned long long rest_due;
    unsigned long long linger_due;
    unsigned long long near_due;
    int near;
    int over_parts;
    int pointer_x;
    int pointer_y;
    GuiTrailPoint trail[GUI_CLAIM_TRAIL];
    unsigned int trail_count;
    GuiSlide claim;
    GuiSlide reveal_top;
    GuiSlide reveal_bottom;
    GuiSlide reveal_left;
    GuiSlide reveal_right;
    GuiSlide reveal_down;
    GuiSlide room_top;
    GuiSlide room_left;
    GuiSlide room_bottom;
    GuiSlide controls_alpha;
    int grown_top;
    int grown_left;
    int grown_bottom;
    int last_top;
    int last_left;
    int last_bottom;
    int pulled_top;
    int pulled_left;
    int pulled_bottom;
} GuiClaim;

// the page's ease curve, cubic-bezier(0.25, 0.1, 0.25, 1), at `share` of the way, 0 to GUI_WHOLE: its number, 0 to
// GUI_WHOLE, read from a table of the curve at sixty-four steps, the step between two entries laid straight
int gui_ease(int share);
// a slide's number at `now`; and the slide sent to `to` from where it stands at `now`, after `delay`, a slide already
// bound for `to` left running
int gui_slide_at(const GuiSlide *slide, unsigned long long now);
void gui_slide_to(GuiSlide *slide, int to, unsigned long long now, unsigned long long delay);
// whether a slide still runs at `now`
int gui_slide_running(const GuiSlide *slide, unsigned long long now);

// a claim not holding, every number at 0 and the controls whole
void gui_claim_made(GuiClaim *claim);
// the claim made or let go at `now`: let go, every part goes back
void gui_claim_set(GuiClaim *claim, int on, unsigned long long now);
// the edge whose band the pointer at x, y stands in, for a window `width` by `height` at `scale` whose controls are
// `controls_wide` wide: the controls' corner, the head, the foot, the left, the right, or none
unsigned int gui_claim_zone(unsigned int width, unsigned int height, float scale, int controls_wide, int x, int y);
// the pointer moved to x, y at `now`, standing in `zone`, `over_parts` 1 where it stands over the part shown: a rest
// begun or begun again, the ghost shown or hidden, the linger begun or ended
void gui_claim_moved(GuiClaim *claim, int x, int y, unsigned int zone, int over_parts, unsigned long long now);
// the pointer gone out of the window: nothing waiting on its rest comes
void gui_claim_left(GuiClaim *claim);
// every timer due at `now` run: the rest reveals its edge, the linger hides the part shown unless the pointer stands
// over it, the switch shows the edge waiting, the ghost shows; 1 where anything changed
int gui_claim_stepped(GuiClaim *claim, unsigned long long now);
// when the claim next wants the clock, in milliseconds from `now`: 0 for never, the time to the nearest timer, or a
// frame's time while a slide runs
unsigned long long gui_claim_due(const GuiClaim *claim, unsigned long long now);
// whether any slide runs at `now`
int gui_claim_moving(const GuiClaim *claim, unsigned long long now);
// the numbers at `now` for the frame: each slide's number, each slat's grown share, coming in as its reveal, going as
// it grew, whole outside the claim, and the points the slats are pulled from
void gui_claim_numbers(GuiClaim *claim, unsigned long long now, GuiClaimNumbers *numbers);
#endif
