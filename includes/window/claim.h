// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim.h: the claim: while the reader edits, the work takes the bar's row, the side_menu's column and the status bar's
// row and saves the frame's gap on every side, and inside the work the editor's text takes the tabs row's inset and
// the minimap's. A pointer resting within three gaps of an edge brings that edge's part back: the status bar at the
// foot, the bar at the head, the side_menu at the left, the minimap at the right, and in the controls'
// corner the three controls alone as a tab, a ghost of them showing as soon as the pointer slows in the corner. A
// part goes back a linger after the pointer leaves it, and one edge's part comes only once another's has gone. The
// tabs row is a claim of its own inside the work: a rest in the band under the head's brings it, and the head's
// rest leaves it away. A
// pointer crossing a gap faster than the criterion is passing through, not resting. Every number the parts move on
// runs from 0 to 1 and back on one curve over one slide, as the page's claim does; the claim saves the numbers and
// the timers and says when it next wants the clock, and the frame positions the parts out for the numbers (claim.c)
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

// the edges whose parts a rest brings back, and none. The tabs row is a claim of its own inside the work, brought
// back by a rest in the band under the head's
#define GUI_AUTOHIDE_NONE 0u
#define GUI_AUTOHIDE_TOP 1u
#define GUI_AUTOHIDE_BOTTOM 2u
#define GUI_AUTOHIDE_LEFT 3u
#define GUI_AUTOHIDE_RIGHT 4u
#define GUI_AUTOHIDE_CONTROLS 5u
#define GUI_AUTOHIDE_TABS 6u
// the delay before a part shows, the delay before it hides after the pointer leaves it, and the duration of a slide.
// Then the window of the path of the pointer that the speed uses. Then the speed, in pixels a millisecond, past which
// the pointer only passes through. Then how many gaps in from an edge its band reaches. Then the points of the path
// of the pointer that the claim keeps
#define GUI_AUTOHIDE_SHOW_DELAY_MS 300ULL
#define GUI_AUTOHIDE_HIDE_DELAY_MS 450ULL
#define GUI_AUTOHIDE_ANIMATION_MS 300ULL
#define GUI_AUTOHIDE_TRAIL_WINDOW_MS 50ULL
#define GUI_AUTOHIDE_SPEED_THRESHOLD 0.5f
// the pixels that a hand at rest moves the pointer. The pace does not count them as travel
#define GUI_AUTOHIDE_TREMOR_PX 4
#define GUI_AUTOHIDE_ADJACENT 3
#define GUI_AUTOHIDE_TRAIL 16u
// The lag a pane's far edge trails its front edge by. The frame reads a bead's slide a second time this many
// milliseconds behind the clock, at its start where the lag reaches before it. The pane stretches between the two
// readings. A bead's slide runs the lag past its end: the far edge catches up before the slides rest
#define GUI_AUTOHIDE_LAG_MS 40ULL

// A slide: a number of GUI_FIXED_ONE that runs from `from` to `to` from `start_ms`, over its length on its curve. The
// length is GUI_AUTOHIDE_ANIMATION_MS of the claim, on the ease of the page. A slide whose ends are one is at them.
//
// The slide of a bead also keeps the speed that it starts with, in GUI_FIXED_ONE a length of the slide. A slide that
// the claim sends on from mid-way then keeps its speed. `curve` is the curve of the slide, and a caller can replace
// the ease with another curve
typedef struct
{
    int from;
    int to;
    unsigned long long start_ms;
    int velocity;
    unsigned int curve;
} GuiSlide;

// The curves of a slide. The ease of the page is cubic-bezier(0.25, 0.1, 0.25, 1), and its ease-out is
// cubic-bezier(0, 0, 0.58, 1). The bead is a damped spring that moves as a bead of water on glass moves. The bead
// starts from rest, goes past its end by about 5 percent, and settles at its end as the slide ends. The number of a
// bead can pass GUI_FIXED_ONE. For that reason, a slide that sets an opacity does not use the bead
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

// The claim. It holds whether it holds, the edge whose part shows, the edge that waits for that part to hide, and the
// deadline that shows it. It holds the edge of the pointer and the edge of the armed show deadline, with that
// deadline. It holds the hide deadline and the preview deadline of the controls. Each deadline is 0 while disarmed.
//
// It holds whether the ghost of the controls shows, and whether the pointer is over the shown part after its last
// move. It holds where the pointer is, and its path over the last span. It holds the slides: the claim, the reveal of
// each edge and the way down of the controls. The inset of each edge and the opacity of the controls are slides too.
//
// It holds the grown share of each slat and its last read reveal. It holds the point of each edge that its slat
// pulls from, -1 where there is none. It holds whether the user asks for less motion. It holds how many times the
// claim sent a slide to a new place, which tells when the stages from them are out of date
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
    GuiSlide reveal_tabs;
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

// the ease curve of the page, cubic-bezier(0.25, 0.1, 0.25, 1), and its ease-out, cubic-bezier(0, 0, 0.58, 1), at
// `share` of the way, 0 to GUI_FIXED_ONE. The result is its number, 0 to GUI_FIXED_ONE, from a table of the curve at
// sixty-four steps. A step between two entries is a straight line
int gui_ease(const GuiAutohideShareCfg *args);
int gui_ease_out(const GuiAutohideShareCfg *args);
// a CSS cubic-bezier(x1, y1, x2, y2) made into a table of its number at each of GUI_EASE_STEPS steps of the way. Its
// ends are 0 and GUI_FIXED_ONE. The second call reads such a table at `share` of the way, 0 to GUI_FIXED_ONE. A step
// between two entries is a straight line
#define GUI_EASE_STEPS 64u
void gui_ease_table_init(const GuiAutohideTableCfg *args);
int gui_ease_table_read(const GuiAutohideShareCfg *args);
// the number of a slide at `time_ms`. The slide is `lasts_ms` long, GUI_AUTOHIDE_ANIMATION_MS where 0, on `curve`, the
// ease of the page where 0. The second call sends the slide of the claim to `to` from where it is now. It starts after
// `delay_ms`. A slide that already goes to `to` keeps on
int gui_slide_at(const GuiAutohideSlideCfg *args);
void gui_slide_to(const GuiAutohideTargetCfg *args);
// whether a slide still runs at `time_ms`, and when it ends where it runs, 0 where it does not. The slide of a bead
// runs GUI_AUTOHIDE_LAG_MS past its length. That is the lag of its far edge
int gui_slide_running(const GuiAutohideSlideCfg *args);
unsigned long long gui_slide_end_ms(const GuiAutohideSlideCfg *args);
// a slide's number GUI_AUTOHIDE_LAG_MS before `time_ms`, its start's where the lag reaches before the slide
int gui_slide_lag_at(const GuiAutohideSlideCfg *args);

// a claim that does not hold, each number at 0 and the controls whole
void gui_autohide_init(const GuiAutohideStillCfg *args);
// less motion, on or off. With less motion on, each slide is at its end, and a slide that runs ends there. A part that
// waits for the shown part to go then comes as soon as a caller asks for it
void gui_autohide_still(const GuiAutohideStillCfg *args);
// the claim made or let go: let go, every part goes back
void gui_autohide_set(const GuiAutohideOnCfg *args);
// the edge whose band holds the pointer at x, y, for a window `width` by `height` at `scale`. Its controls are
// `controls_width` wide. The edge is the corner of the controls, the head, the foot, the left, the right, or none
unsigned int gui_autohide_zone(const GuiAutohideWidthCfg *args);
// the pointer moved to x, y in `zone`, with `over_parts` 1 where it is over the shown part. The call arms the show
// deadline or arms it again, shows or hides the preview, and arms or disarms the hide deadline. With `menu_open` the
// claim holds, and no part comes. The shown part then stays for a linger that starts again at each move
void gui_autohide_move(const GuiAutohidePointerCfg *args);
// `zone`'s part displayed at once for a press at x, y: no rest and no slide, every slide located at its target
void gui_autohide_show_now(const GuiAutohidePointerCfg *args);
// the pointer gone out of the window: the show and preview deadlines disarmed
void gui_autohide_leave(const GuiAutohideStillCfg *args);
// each expired deadline runs. The show deadline reveals its edge. The hide deadline hides the shown part, except where
// the pointer is over it or `menu_open`. The pending deadline shows the edge that waits, and the preview shows. The
// call gives 1 where anything changed
int gui_autohide_step(const GuiAutohideStateCfg *args);
// the claim's earliest deadline, 0 for none; the next frame's while a slide runs
unsigned long long gui_autohide_next_deadline_ms(const GuiAutohideCfg *args);
// whether any slide runs at `time_ms`; and when the last of them ends, `time_ms` where none runs
int gui_autohide_moving(const GuiAutohideCfg *args);
unsigned long long gui_autohide_animation_end_ms(const GuiAutohideCfg *args);
// the numbers at `time_ms` for the frame. They are the number of each slide and the lagged number of each reveal.
// They are the grown share of each slat, which comes in as its reveal and goes as it grew, whole outside the claim.
// They are the points that the slats pull from. With less motion on, each lagged number is the number of its reveal
void gui_autohide_numbers(const GuiAutohideStateCfg *args);
struct GuiAutohidePointerCfg
{
    unsigned long long now_ms;
    GuiAutohide *claim;
    int x;
    int y;
    unsigned int zone;
    int over_parts;
    int menu_open;
};

// the window's sides, the scale, the controls' width, the pixel asked about, and the work's inside top, under which
// the tabs row's band begins
struct GuiAutohideWidthCfg
{
    unsigned int width;
    unsigned int height;
    int scale;
    int controls_width;
    int x;
    int y;
    int work_top;
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
    int menu_open;
};

struct GuiAutohideOnCfg
{
    unsigned long long now_ms;
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
    unsigned long long now_ms;
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
    int (*lag_at)(const GuiAutohideSlideCfg *args);
} GuiAutohideSlideNs;
EMBED_TABLE_LAYOUT(GuiAutohideSlideNs, slide_at, slide_running, slide_end_ms, lag_at);
EMBED_TABLE_STORAGE GuiAutohideSlideNs gui_autohide_slide EMBED_UNUSED = {
    .slide_at = gui_slide_at,
    .slide_running = gui_slide_running,
    .slide_end_ms = gui_slide_end_ms,
    .lag_at = gui_slide_lag_at,
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
} GuiAutohideStillNs;
EMBED_TABLE_LAYOUT(GuiAutohideStillNs, init, still, leave);
EMBED_TABLE_STORAGE GuiAutohideStillNs gui_autohide_still_table EMBED_UNUSED = {
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
    void (*show_now)(const GuiAutohidePointerCfg *args);
} GuiAutohidePointerNs;
EMBED_TABLE_LAYOUT(GuiAutohidePointerNs, move, show_now);
EMBED_TABLE_STORAGE GuiAutohidePointerNs gui_autohide_pointer EMBED_UNUSED = {
    .move = gui_autohide_move,
    .show_now = gui_autohide_show_now,
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
