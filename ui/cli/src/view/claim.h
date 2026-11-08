// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.h: the claim: while the reader edits, the work takes the bar's row, the strip's column and the status bar's
// row and saves the frame's gap on every side, and inside the work the editor's text takes the tabs row's inset and
// the minimap's. A pointer resting within three gaps of an edge brings that edge's part back: the status bar at the
// foot, the bar and the tabs row at the head, the strip at the left, the minimap at the right, and in the controls'
// corner the three controls alone as a tab, a ghost of them showing as soon as the pointer slows in the corner. A
// part goes back a linger after the pointer leaves it, and one edge's part comes only once another's has gone. A
// pointer crossing a gap faster than the criterion is passing through, not resting. Every number the parts move on
// runs from 0 to 1 and back on one curve over one slide, as the page's claim does; the claim saves the numbers and
// the timers and says when it next wants the clock, and the frame lays the parts out for the numbers (claim.c)
#ifndef GUI_AUTOHIDE_H
#define GUI_AUTOHIDE_H
#include "embed_dispatch_layout.h"
#include "frame.h"

typedef struct GuiAutohidePointerCfg GuiAutohidePointerCfg;
typedef struct GuiAutohideWidthCfg GuiAutohideWidthCfg;
typedef struct GuiAutohideTableCfg GuiAutohideTableCfg;
typedef struct GuiAutohideTargetCfg GuiAutohideTargetCfg;
typedef struct GuiAutohideStateCfg GuiAutohideStateCfg;
typedef struct GuiAutohideOnCfg GuiAutohideOnCfg;
typedef struct GuiAutohideCfg GuiAutohideCfg;
typedef struct GuiAutohideStillCfg GuiAutohideStillCfg;
typedef struct GuiAutohideShareCfg GuiAutohideShareCfg;
typedef struct GuiAutohideSlideCfg GuiAutohideSlideCfg;

// the edges whose parts a rest brings back, and none
#define GUI_AUTOHIDE_NONE 0u
#define GUI_AUTOHIDE_TOP 1u
#define GUI_AUTOHIDE_BOTTOM 2u
#define GUI_AUTOHIDE_LEFT 3u
#define GUI_AUTOHIDE_RIGHT 4u
#define GUI_AUTOHIDE_CONTROLS 5u
// the delay before a part shows, the delay before it hides after the pointer leaves it, and a slide's duration; the
// window of the pointer's path its speed is measured over, and the speed, in pixels a millisecond, past which it is
// passing through;
// how many gaps in from an edge its band reaches; the controls' ghost's way down and the controls' opacity as the
// ghost, each a number of GUI_FIXED_ONE; and the points of the pointer's path saved
#define GUI_AUTOHIDE_SHOW_DELAY_MS 300ull
#define GUI_AUTOHIDE_HIDE_DELAY_MS 450ull
#define GUI_AUTOHIDE_ANIMATION_MS 300ull
#define GUI_AUTOHIDE_TRAIL_WINDOW_MS 50ull
#define GUI_AUTOHIDE_SPEED_THRESHOLD 0.06f
#define GUI_AUTOHIDE_ADJACENT 3
#define GUI_AUTOHIDE_GHOST_DOWN 29491
#define GUI_AUTOHIDE_GHOST_ALPHA 22938
#define GUI_AUTOHIDE_TRAIL 16u

// A slide: a number of GUI_FIXED_ONE running from `from` to `to` from `start_ms` over its length on its curve, the
// claim's own GUI_AUTOHIDE_ANIMATION_MS on the page's ease; a slide whose ends are one stands at them. A bead's slide
// also keeps the speed it starts with, in GUI_FIXED_ONE a slide's length, so a slide sent on from mid-way keeps its
// speed; `curve` is the slide's own curve, which a caller's curve other than the ease replaces
typedef struct
{
    int from;
    int to;
    unsigned long long start_ms;
    int velocity;
    unsigned int curve;
} GuiSlide;

// The curves a slide runs on: the page's ease, cubic-bezier(0.25, 0.1, 0.25, 1); its ease-out,
// cubic-bezier(0, 0, 0.58, 1); and the bead, a damped spring that a bead of water on glass moves as. The bead starts
// from rest, goes past its end by about 5 percent, and settles at its end as the slide ends. A bead's number can
// pass GUI_FIXED_ONE, so a slide that sets an opacity does not take it
#define GUI_CURVE_EASE 0u
#define GUI_CURVE_EASE_OUT 1u
#define GUI_CURVE_BEAD 2u

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
// each edge's inset and the controls' opacity; each slat's grown share and its reveal as last read; the point of each
// edge its slat is pulled from, -1 where none has been; whether less motion is asked for; and how many times a slide
// has been sent somewhere new, which says when the stages made from them stand out of date
typedef struct
{
    int still;
    unsigned int sent;
    int autohidden;
    unsigned int displayed;
    unsigned int waiting;
    unsigned long long pending_show_deadline_ms;
    unsigned int zone_now;
    unsigned int show_zone;
    unsigned long long show_deadline_ms;
    unsigned long long hide_deadline_ms;
    unsigned long long preview_deadline_ms;
    int adjacent;
    int over_parts;
    int pointer_x;
    int pointer_y;
    GuiTrailPoint trail[GUI_AUTOHIDE_TRAIL];
    unsigned int trail_count;
    GuiSlide claim;
    GuiSlide reveal_top;
    GuiSlide reveal_bottom;
    GuiSlide reveal_left;
    GuiSlide reveal_right;
    GuiSlide reveal_down;
    GuiSlide inset_top;
    GuiSlide inset_left;
    GuiSlide inset_bottom;
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
} GuiAutohide;

// the page's ease curve, cubic-bezier(0.25, 0.1, 0.25, 1), and its ease-out, cubic-bezier(0, 0, 0.58, 1), at `share`
// of the way, 0 to GUI_FIXED_ONE: its number, 0 to GUI_FIXED_ONE, read from a table of the curve at sixty-four steps,
// the step between two entries laid straight
int gui_ease(const GuiAutohideShareCfg *args);
int gui_ease_out(const GuiAutohideShareCfg *args);
// a CSS cubic-bezier(x1, y1, x2, y2) made into a table of its number at each of GUI_EASE_STEPS steps of the way, its
// ends 0 and GUI_FIXED_ONE; and such a table read at `share` of the way, 0 to GUI_FIXED_ONE, the step between two
// entries laid straight
#define GUI_EASE_STEPS 64u
void gui_ease_table_init(const GuiAutohideTableCfg *args);
int gui_ease_table_read(const GuiAutohideShareCfg *args);
// a slide's number at `time_ms`, the slide `lasts_ms` long, GUI_AUTOHIDE_ANIMATION_MS where 0, on `curve`, the page's
// ease where 0; and the claim's slide sent to `to` from where it stands now, starting after `delay_ms`, a slide already
// bound for `to` left running
int gui_slide_at(const GuiAutohideSlideCfg *args);
void gui_slide_to(const GuiAutohideTargetCfg *args);
// whether a slide still runs at `time_ms`; and when it ends where it runs, 0 where it does not
int gui_slide_running(const GuiAutohideSlideCfg *args);
unsigned long long gui_slide_end_ms(const GuiAutohideSlideCfg *args);

// a claim not holding, every number at 0 and the controls whole
void gui_autohide_init(const GuiAutohideStillCfg *args);
// less motion asked for or no longer: asked for, every slide stands where it is bound, a slide running ending there,
// and a part waiting on the shown one to go comes as soon as it is asked for
void gui_autohide_still(const GuiAutohideStillCfg *args);
// the claim made or let go: let go, every part goes back
void gui_autohide_set(const GuiAutohideOnCfg *args);
// the edge whose band the pointer at x, y stands in, for a window `width` by `height` at `scale` whose controls are
// `controls_width` wide: the controls' corner, the head, the foot, the left, the right, or none
unsigned int gui_autohide_zone(const GuiAutohideWidthCfg *args);
// the pointer moved to x, y, standing in `zone`, `over_parts` 1 where it stands over the part shown: the show deadline
// armed or armed again, the preview shown or hidden, the hide deadline armed or disarmed
void gui_autohide_move(const GuiAutohidePointerCfg *args);
// the pointer gone out of the window: the show and preview deadlines disarmed
void gui_autohide_leave(const GuiAutohideStillCfg *args);
// every expired deadline run: the show deadline reveals its edge, the hide deadline hides the part shown unless the
// pointer stands over it, the pending deadline shows the edge waiting, the preview shows; 1 where anything changed
int gui_autohide_step(const GuiAutohideStateCfg *args);
// the claim's earliest deadline, 0 for none; the next frame's while a slide runs
unsigned long long gui_autohide_next_deadline_ms(const GuiAutohideCfg *args);
// whether any slide runs at `time_ms`; and when the last of them ends, `time_ms` where none runs
int gui_autohide_moving(const GuiAutohideCfg *args);
unsigned long long gui_autohide_animation_end_ms(const GuiAutohideCfg *args);
// the numbers at `time_ms` for the frame: each slide's number, each slat's grown share, coming in as its reveal, going
// as it grew, whole outside the claim, and the points the slats are pulled from
void gui_autohide_numbers(const GuiAutohideStateCfg *args);
struct GuiAutohidePointerCfg
{
    GuiAutohide *claim;
    int x;
    int y;
    unsigned int zone;
    int over_parts;
};

struct GuiAutohideWidthCfg
{
    unsigned int width;
    unsigned int height;
    int scale;
    int controls_width;
    int x;
    int y;
};

struct GuiAutohideTableCfg
{
    int *table;
    double x1;
    double y1;
    double x2;
    double y2;
};

// A slide sent to `to`: after `delay_ms`, at `time_ms` or the clock's now where 0, over `lasts_ms` or the claim's own
// GUI_AUTOHIDE_ANIMATION_MS where 0
struct GuiAutohideTargetCfg
{
    GuiSlide *slide_out;
    int to;
    unsigned long long delay_ms;
    unsigned long long time_ms;
    unsigned long long lasts_ms;
};

struct GuiAutohideStateCfg
{
    unsigned long long time_ms;
    GuiAutohide *claim;
    GuiAutohideNumbers *numbers;
};

struct GuiAutohideOnCfg
{
    GuiAutohide *claim;
    int on;
};

struct GuiAutohideCfg
{
    unsigned long long time_ms;
    const GuiAutohide *autohide_read;
};

struct GuiAutohideStillCfg
{
    GuiAutohide *claim;
    int still;
};

struct GuiAutohideShareCfg
{
    int share;
    const int *table_read;
};

struct GuiAutohideSlideCfg
{
    const GuiSlide *slide;
    unsigned long long time_ms;
    unsigned long long lasts_ms;
    unsigned int curve;
};

typedef struct
{
    int (*ease)(const GuiAutohideShareCfg *args);
    int (*ease_out)(const GuiAutohideShareCfg *args);
    int (*ease_table_read)(const GuiAutohideShareCfg *args);
} GuiAutohideShareNs;
EMBED_TABLE_LAYOUT(GuiAutohideShareNs, ease, ease_out, ease_table_read);
EMBED_TABLE_STORAGE GuiAutohideShareNs gui_autohide_share EMBED_UNUSED = {
    .ease = gui_ease,
    .ease_out = gui_ease_out,
    .ease_table_read = gui_ease_table_read,
};

typedef struct
{
    void (*ease_table_init)(const GuiAutohideTableCfg *args);
} GuiAutohideTableNs;
EMBED_TABLE_LAYOUT(GuiAutohideTableNs, ease_table_init);
EMBED_TABLE_STORAGE GuiAutohideTableNs gui_autohide_table EMBED_UNUSED = {
    .ease_table_init = gui_ease_table_init,
};

typedef struct
{
    int (*slide_at)(const GuiAutohideSlideCfg *args);
    int (*slide_running)(const GuiAutohideSlideCfg *args);
    unsigned long long (*slide_end_ms)(const GuiAutohideSlideCfg *args);
} GuiAutohideSlideNs;
EMBED_TABLE_LAYOUT(GuiAutohideSlideNs, slide_at, slide_running, slide_end_ms);
EMBED_TABLE_STORAGE GuiAutohideSlideNs gui_autohide_slide EMBED_UNUSED = {
    .slide_at = gui_slide_at,
    .slide_running = gui_slide_running,
    .slide_end_ms = gui_slide_end_ms,
};

typedef struct
{
    void (*slide_to)(const GuiAutohideTargetCfg *args);
} GuiAutohideTargetNs;
EMBED_TABLE_LAYOUT(GuiAutohideTargetNs, slide_to);
EMBED_TABLE_STORAGE GuiAutohideTargetNs gui_autohide_target EMBED_UNUSED = {
    .slide_to = gui_slide_to,
};

typedef struct
{
    void (*init)(const GuiAutohideStillCfg *args);
    void (*still)(const GuiAutohideStillCfg *args);
    void (*leave)(const GuiAutohideStillCfg *args);
} CliAutohideStillNs;
EMBED_TABLE_LAYOUT(CliAutohideStillNs, init, still, leave);
EMBED_TABLE_STORAGE CliAutohideStillNs cli_autohide_still EMBED_UNUSED = {
    .init = gui_autohide_init,
    .still = gui_autohide_still,
    .leave = gui_autohide_leave,
};

typedef struct
{
    void (*set)(const GuiAutohideOnCfg *args);
} GuiAutohideOnNs;
EMBED_TABLE_LAYOUT(GuiAutohideOnNs, set);
EMBED_TABLE_STORAGE GuiAutohideOnNs gui_autohide_on EMBED_UNUSED = {
    .set = gui_autohide_set,
};

typedef struct
{
    unsigned int (*zone)(const GuiAutohideWidthCfg *args);
} GuiAutohideWidthNs;
EMBED_TABLE_LAYOUT(GuiAutohideWidthNs, zone);
EMBED_TABLE_STORAGE GuiAutohideWidthNs gui_autohide_width EMBED_UNUSED = {
    .zone = gui_autohide_zone,
};

typedef struct
{
    void (*move)(const GuiAutohidePointerCfg *args);
} GuiAutohidePointerNs;
EMBED_TABLE_LAYOUT(GuiAutohidePointerNs, move);
EMBED_TABLE_STORAGE GuiAutohidePointerNs gui_autohide_pointer EMBED_UNUSED = {
    .move = gui_autohide_move,
};

typedef struct
{
    int (*step)(const GuiAutohideStateCfg *args);
    void (*numbers)(const GuiAutohideStateCfg *args);
} GuiAutohideStateNs;
EMBED_TABLE_LAYOUT(GuiAutohideStateNs, step, numbers);
EMBED_TABLE_STORAGE GuiAutohideStateNs gui_autohide_state EMBED_UNUSED = {
    .step = gui_autohide_step,
    .numbers = gui_autohide_numbers,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiAutohideCfg *args);
    int (*moving)(const GuiAutohideCfg *args);
    unsigned long long (*animation_end_ms)(const GuiAutohideCfg *args);
} GuiAutohideNs;
EMBED_TABLE_LAYOUT(GuiAutohideNs, next_deadline_ms, moving, animation_end_ms);
EMBED_TABLE_STORAGE GuiAutohideNs gui_autohide EMBED_UNUSED = {
    .next_deadline_ms = gui_autohide_next_deadline_ms,
    .moving = gui_autohide_moving,
    .animation_end_ms = gui_autohide_animation_end_ms,
};

#endif
