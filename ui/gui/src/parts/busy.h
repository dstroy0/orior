// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy.h: the busy circle at the foot of the editor, shown while an ask of the CLI stands unanswered, or work it reports on
// runs, past a wait: a fuse
// bent into the ring of the regular busy indicator, the fire swirling round it, ash ahead of the fire and the trail
// burnt behind it, and at its head the burn, five pixels, two lines of three crossing, twitching, pulsing and dithered.
// A swirl's stages are pictures made once as the circle comes and given back as it goes; a stage's step restores the
// pixels kept from under the circle and blits the next picture over them (busy.c)
#ifndef GUI_PARTS_BUSY_H
#define GUI_PARTS_BUSY_H
#include "../frame.h"
#include "../palette.h"

// the tickets of asks kept at once, a power of two past every ask the link can stand unanswered; the milliseconds an ask
// stands before the circle comes, and the least the circle stands once it has come; a swirl's stages, a power of two, and
// the shift from milliseconds to a stage; and the sizes in the page's pixels: the circle's side, how far the ring's middle
// reaches from its center, and how far the circle keeps from the editor's corner
#define GUI_BUSY_ASKS 2048u
#define GUI_BUSY_WAITS 200ull
#define GUI_BUSY_STANDS 256ull
#define GUI_BUSY_STAGES 64u
#define GUI_BUSY_STAGE_SHIFT 4u
#define GUI_BUSY_STAGE_MS (1ull << GUI_BUSY_STAGE_SHIFT)
#define GUI_BUSY_SIDE 20
#define GUI_BUSY_REACH 7
#define GUI_BUSY_EDGE 8

// The circle: when each ask still unanswered was asked, one past the millisecond and 0 where it is answered, read by the
// ticket's low bits; the oldest ticket and the newest; when the work running began, one past the millisecond and 0 where none
// runs; whether the circle is shown, since when, and whether it stands still
// at one stage for less motion; the one cell its pictures lie in, their side and stages, each stage's three lanes of a
// blit side by side pixels; the pixels kept from under it, the rectangle they came from, and whether any are kept
typedef struct
{
    unsigned long long asked_ms[GUI_BUSY_ASKS];
    unsigned long long oldest;
    unsigned long long newest;
    unsigned long long work_ms;
    int shown;
    unsigned long long shown_ms;
    int still;
    unsigned char *cell;
    unsigned long long cell_bytes;
    unsigned int side;
    unsigned int stages;
    unsigned int *red_blue;
    unsigned int *green;
    unsigned int *rest;
    GuiColor *under;
    GuiRect under_rect;
    GuiRect laid_rect;
    int under_kept;
    unsigned int stage_drawn;
} GuiBusy;

// a circle with nothing asked; and its cell given back
void gui_busy_made(GuiBusy *busy);
void gui_busy_given(GuiBusy *busy);
// an ask made at `now` under `ticket`, and the ask of `ticket` answered; whether work the CLI reports on past its answer runs
// as of `now`, standing as an ask does from when it began until it ends; and when the oldest ask unanswered or work running
// began, in `*since`: 1, or 0 where none stands
void gui_busy_asked(GuiBusy *busy, unsigned long long ticket, unsigned long long now);
void gui_busy_answered(GuiBusy *busy, unsigned long long ticket);
void gui_busy_work(GuiBusy *busy, int running, unsigned long long now);
int gui_busy_waiting(const GuiBusy *busy, unsigned long long *since);
// the tick come at `now`: the circle come where an ask has stood past the wait, gone where none has and it has stood its
// least: 1 where it came or went; and the milliseconds from `now` until a tick is wanted, 0 where none is
int gui_busy_ticked(GuiBusy *busy, unsigned long long now);
unsigned long long gui_busy_due(const GuiBusy *busy, unsigned long long now);
// the stage standing at `now`, the first while it stands still; and whether it stands still, its pictures made again for it
unsigned int gui_busy_stage_at(const GuiBusy *busy, unsigned long long now);
void gui_busy_still(GuiBusy *busy, int still);
// the circle's rectangle in `frame`, at the editor's foot on its right
GuiRect gui_busy_rect(const GuiFrame *frame);
// the circle laid at the end of a whole frame, its pictures made where they are wanted, the pixels under it kept first; and
// a step of its stage alone: the pixels kept restored and the stage at `now` blitted over them: the rectangle to show again,
// empty where none is kept
void gui_busy_drawn(GuiBusy *busy, GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame, unsigned long long now);
GuiRect gui_busy_redrawn(GuiBusy *busy, GuiRaster *raster, unsigned long long now);
#endif
