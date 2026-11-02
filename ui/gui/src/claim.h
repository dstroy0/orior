// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.h: the claim: while the reader edits, the work takes the bar's row, the strip's column and the status bar's
// row and saves the frame's gap on every side, and inside the work the editor's text takes the tabs row's room and
// the minimap's. A pointer resting within three gaps of an edge brings that edge's part back: the status bar at the
// foot, the bar and the tabs row at the head, the strip at the left, the minimap at the right, and in the controls'
// corner the three controls alone as a tab, a ghost of them showing as soon as the pointer slows in the corner. A
// part goes back a linger after the pointer leaves it, and one edge's part comes only once another's has gone. A
// pointer crossing a gap faster than the criterion is passing through, not resting. Every number the parts move on
// runs from 0 to 1 and back on one curve over one slide, as the page's claim does; the claim saves the numbers and
// the timers and says when it next wants the clock, and the frame lays the parts out for the numbers (claim.c)
#ifndef GUI_CLAIM_H
#define GUI_CLAIM_H
typedef struct GuiClaimPointerCfg GuiClaimPointerCfg;
typedef struct GuiClaimWidthCfg GuiClaimWidthCfg;
typedef struct GuiClaimTableCfg GuiClaimTableCfg;
typedef struct GuiClaimTargetCfg GuiClaimTargetCfg;
typedef struct GuiClaimStateCfg GuiClaimStateCfg;
typedef struct GuiClaimOnCfg GuiClaimOnCfg;
typedef struct GuiClaimCfg GuiClaimCfg;
typedef struct GuiClaimStillCfg GuiClaimStillCfg;
typedef struct GuiClaimShareCfg GuiClaimShareCfg;
typedef struct GuiClaimSlideCfg GuiClaimSlideCfg;
#include "embed_dispatch_layout.h"
#include "frame.h"

// the edges whose parts a rest brings back, and none
#define GUI_CLAIM_NONE 0u
#define GUI_CLAIM_TOP 1u
#define GUI_CLAIM_BOTTOM 2u
#define GUI_CLAIM_LEFT 3u
#define GUI_CLAIM_RIGHT 4u
#define GUI_CLAIM_CONTROLS 5u
// the delay before a part shows, the delay before it hides after the pointer leaves it, and a slide's duration; the
// window of the pointer's path its speed is measured over, and the speed, in pixels a millisecond, past which it is
// passing through;
// how many gaps in from an edge its band reaches; the controls' ghost's way down and the controls' opacity as the
// ghost, each a number of GUI_WHOLE; and the points of the pointer's path saved
#define GUI_CLAIM_SHOW_DELAY_MS 300ull
#define GUI_CLAIM_HIDE_DELAY_MS 450ull
#define GUI_CLAIM_ANIMATION_MS 300ull
#define GUI_CLAIM_TRAIL_WINDOW_MS 50ull
#define GUI_CLAIM_SPEED_THRESHOLD 0.06f
#define GUI_CLAIM_NEAR 3
#define GUI_CLAIM_GHOST_DOWN 29491
#define GUI_CLAIM_GHOST_ALPHA 22938
#define GUI_CLAIM_TRAIL 16u

// A slide: a number of GUI_WHOLE running from `from` to `to` over GUI_CLAIM_ANIMATION_MS on the page's ease curve, from
// `start_ms`
typedef struct
{
    int from;
    int to;
    unsigned long long start_ms;
} GuiSlide;

// A sample of the pointer's path: its position and time
typedef struct
{
    int x;
    int y;
    unsigned long long time_ms;
} GuiTrailPoint;

// The claim: whether it holds; the edge whose part is shown, the one waiting for it to hide and the deadline that shows
// it; the pointer's edge, the edge the show deadline is armed for and that deadline, the hide deadline and the
// controls' preview deadline, each 0 while disarmed; whether
// the controls' ghost shows; whether the pointer stands over the shown part, as its last move left it; where the
// pointer stands; its path over the last span; the slides: the claim, each edge's reveal, the controls' way down,
// each edge's room and the controls' opacity; each slat's grown share and its reveal as last read; the point of each
// edge its slat is pulled from, -1 where none has been; whether less motion is asked for; and how many times a slide
// has been sent somewhere new, which says when the stages made from them stand out of date
typedef struct
{
    int still;
    unsigned int sent;
    int claimed;
    unsigned int shown;
    unsigned int waiting;
    unsigned long long pending_show_deadline_ms;
    unsigned int zone_now;
    unsigned int show_zone;
    unsigned long long show_deadline_ms;
    unsigned long long hide_deadline_ms;
    unsigned long long preview_deadline_ms;
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
int gui_ease(const GuiClaimShareCfg *args);
// a CSS cubic-bezier(x1, y1, x2, y2) made into a table of its number at each of GUI_EASE_STEPS steps of the way, its
// ends 0 and GUI_WHOLE; and such a table read at `share` of the way, 0 to GUI_WHOLE, the step between two entries laid
// straight
#define GUI_EASE_STEPS 64u
void gui_ease_table_init(const GuiClaimTableCfg *args);
int gui_ease_table_read(const GuiClaimShareCfg *args);
// a slide's number at `time_ms`; and the slide sent to `to` from where it stands now, starting after `delay_ms`, a
// slide already bound for `to` left running
int gui_slide_at(const GuiClaimSlideCfg *args);
void gui_slide_to(const GuiClaimTargetCfg *args);
// whether a slide still runs at `time_ms`
int gui_slide_running(const GuiClaimSlideCfg *args);

// a claim not holding, every number at 0 and the controls whole
void gui_claim_init(const GuiClaimStillCfg *args);
// less motion asked for or no longer: asked for, every slide stands where it is bound, a slide running ending there,
// and a part waiting on the shown one to go comes as soon as it is asked for
void gui_claim_still(const GuiClaimStillCfg *args);
// the claim made or let go: let go, every part goes back
void gui_claim_set(const GuiClaimOnCfg *args);
// the edge whose band the pointer at x, y stands in, for a window `width` by `height` at `scale` whose controls are
// `controls_wide` wide: the controls' corner, the head, the foot, the left, the right, or none
unsigned int gui_claim_zone(const GuiClaimWidthCfg *args);
// the pointer moved to x, y, standing in `zone`, `over_parts` 1 where it stands over the part shown: the show deadline
// armed or armed again, the preview shown or hidden, the hide deadline armed or disarmed
void gui_claim_move(const GuiClaimPointerCfg *args);
// the pointer gone out of the window: the show and preview deadlines disarmed
void gui_claim_leave(const GuiClaimStillCfg *args);
// every expired deadline run: the show deadline reveals its edge, the hide deadline hides the part shown unless the
// pointer stands over it, the pending deadline shows the edge waiting, the preview shows; 1 where anything changed
int gui_claim_step(const GuiClaimStateCfg *args);
// the claim's earliest deadline, 0 for none; the next frame's while a slide runs
unsigned long long gui_claim_next_deadline_ms(const GuiClaimCfg *args);
// whether any slide runs at `time_ms`; and when the last of them ends, `time_ms` where none runs
int gui_claim_moving(const GuiClaimCfg *args);
unsigned long long gui_claim_animation_end_ms(const GuiClaimCfg *args);
// the numbers at `time_ms` for the frame: each slide's number, each slat's grown share, coming in as its reveal, going
// as it grew, whole outside the claim, and the points the slats are pulled from
void gui_claim_numbers(const GuiClaimStateCfg *args);
struct GuiClaimPointerCfg
{
    GuiClaim *claim;
    int x;
    int y;
    unsigned int zone;
    int over_parts;
};

struct GuiClaimWidthCfg
{
    unsigned int width;
    unsigned int height;
    int scale;
    int controls_wide;
    int x;
    int y;
};

struct GuiClaimTableCfg
{
    int *table;
    double x1;
    double y1;
    double x2;
    double y2;
};

struct GuiClaimTargetCfg
{
    GuiSlide *slide_out;
    int to;
    unsigned long long delay_ms;
};

struct GuiClaimStateCfg
{
    unsigned long long time_ms;
    GuiClaim *claim;
    GuiClaimNumbers *numbers;
};

struct GuiClaimOnCfg
{
    GuiClaim *claim;
    int on;
};

struct GuiClaimCfg
{
    unsigned long long time_ms;
    const GuiClaim *claim_read;
};

struct GuiClaimStillCfg
{
    GuiClaim *claim;
    int still;
};

struct GuiClaimShareCfg
{
    int share;
    const int *table_read;
};

struct GuiClaimSlideCfg
{
    const GuiSlide *slide;
    unsigned long long time_ms;
};

typedef struct
{
    int (*ease)(const GuiClaimShareCfg *args);
    int (*ease_table_read)(const GuiClaimShareCfg *args);
} GuiClaimShareNs;
EMBED_TABLE_LAYOUT(GuiClaimShareNs, ease, ease_table_read);
EMBED_TABLE_STORAGE GuiClaimShareNs gui_claim_share EMBED_UNUSED = {
    .ease = gui_ease,
    .ease_table_read = gui_ease_table_read,
};

typedef struct
{
    void (*ease_table_init)(const GuiClaimTableCfg *args);
} GuiClaimTableNs;
EMBED_TABLE_LAYOUT(GuiClaimTableNs, ease_table_init);
EMBED_TABLE_STORAGE GuiClaimTableNs gui_claim_table EMBED_UNUSED = {
    .ease_table_init = gui_ease_table_init,
};

typedef struct
{
    int (*slide_at)(const GuiClaimSlideCfg *args);
    int (*slide_running)(const GuiClaimSlideCfg *args);
} GuiClaimSlideNs;
EMBED_TABLE_LAYOUT(GuiClaimSlideNs, slide_at, slide_running);
EMBED_TABLE_STORAGE GuiClaimSlideNs gui_claim_slide EMBED_UNUSED = {
    .slide_at = gui_slide_at,
    .slide_running = gui_slide_running,
};

typedef struct
{
    void (*slide_to)(const GuiClaimTargetCfg *args);
} GuiClaimTargetNs;
EMBED_TABLE_LAYOUT(GuiClaimTargetNs, slide_to);
EMBED_TABLE_STORAGE GuiClaimTargetNs gui_claim_target EMBED_UNUSED = {
    .slide_to = gui_slide_to,
};

typedef struct
{
    void (*init)(const GuiClaimStillCfg *args);
    void (*still)(const GuiClaimStillCfg *args);
    void (*leave)(const GuiClaimStillCfg *args);
} CliClaimStillNs;
EMBED_TABLE_LAYOUT(CliClaimStillNs, init, still, leave);
EMBED_TABLE_STORAGE CliClaimStillNs cli_claim_still EMBED_UNUSED = {
    .init = gui_claim_init,
    .still = gui_claim_still,
    .leave = gui_claim_leave,
};

typedef struct
{
    void (*set)(const GuiClaimOnCfg *args);
} GuiClaimOnNs;
EMBED_TABLE_LAYOUT(GuiClaimOnNs, set);
EMBED_TABLE_STORAGE GuiClaimOnNs gui_claim_on EMBED_UNUSED = {
    .set = gui_claim_set,
};

typedef struct
{
    unsigned int (*zone)(const GuiClaimWidthCfg *args);
} GuiClaimWidthNs;
EMBED_TABLE_LAYOUT(GuiClaimWidthNs, zone);
EMBED_TABLE_STORAGE GuiClaimWidthNs gui_claim_width EMBED_UNUSED = {
    .zone = gui_claim_zone,
};

typedef struct
{
    void (*move)(const GuiClaimPointerCfg *args);
} GuiClaimPointerNs;
EMBED_TABLE_LAYOUT(GuiClaimPointerNs, move);
EMBED_TABLE_STORAGE GuiClaimPointerNs gui_claim_pointer EMBED_UNUSED = {
    .move = gui_claim_move,
};

typedef struct
{
    int (*step)(const GuiClaimStateCfg *args);
    void (*numbers)(const GuiClaimStateCfg *args);
} GuiClaimStateNs;
EMBED_TABLE_LAYOUT(GuiClaimStateNs, step, numbers);
EMBED_TABLE_STORAGE GuiClaimStateNs gui_claim_state EMBED_UNUSED = {
    .step = gui_claim_step,
    .numbers = gui_claim_numbers,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiClaimCfg *args);
    int (*moving)(const GuiClaimCfg *args);
    unsigned long long (*animation_end_ms)(const GuiClaimCfg *args);
} GuiClaimNs;
EMBED_TABLE_LAYOUT(GuiClaimNs, next_deadline_ms, moving, animation_end_ms);
EMBED_TABLE_STORAGE GuiClaimNs gui_claim EMBED_UNUSED = {
    .next_deadline_ms = gui_claim_next_deadline_ms,
    .moving = gui_claim_moving,
    .animation_end_ms = gui_claim_animation_end_ms,
};

#endif
