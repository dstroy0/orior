// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy.h: the busy circle at the foot of the editor, shown while an ask of the CLI stands unanswered, or work it
// reports on runs, past a wait: a fuse bent into the ring of the regular busy indicator, the fire swirling round it,
// ash ahead of the fire and the trail burnt behind it, and at its head the burn, five pixels, two lines of three
// crossing, twitching, pulsing and dithered. A swirl's stages are pictures made once as the circle comes and given back
// as it goes; a stage's step restores the pixels saved from under the circle and blits the next picture over them
// (busy.c)
#ifndef GUI_PARTS_BUSY_H
#define GUI_PARTS_BUSY_H
typedef struct GuiBusyFrameCfg GuiBusyFrameCfg;
typedef struct GuiBusyTicketCfg GuiBusyTicketCfg;
typedef struct GuiBusyRunningCfg GuiBusyRunningCfg;
typedef struct GuiBusyCfg GuiBusyCfg;
typedef struct GuiBusyStillCfg GuiBusyStillCfg;
typedef struct GuiBusySinceCfg GuiBusySinceCfg;
#include "../frame.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the tickets of asks saved at once, a power of two past every ask the link can stand unanswered; the milliseconds an
// ask stands before the circle comes, and the least the circle stands once it has come; a swirl's stages, a power of
// two, and the shift from milliseconds to a stage; and the sizes in the page's pixels: the circle's side, how far the
// ring's middle reaches from its center, and how far the circle saves from the editor's corner
#define GUI_BUSY_ASKS 8192u
#define GUI_BUSY_SHOW_DELAY_MS 200ull
#define GUI_BUSY_MIN_VISIBLE_MS 256ull
#define GUI_BUSY_STAGES 64u
#define GUI_BUSY_STAGE_SHIFT 4u
#define GUI_BUSY_STAGE_MS (1ull << GUI_BUSY_STAGE_SHIFT)
#define GUI_BUSY_SIDE 20
#define GUI_BUSY_REACH 7
#define GUI_BUSY_EDGE 8

// The circle: when each ask still unanswered was asked, one past the millisecond and 0 where it is answered, read by
// the ticket's low bits; the oldest ticket and the newest; when the work running began, one past the millisecond and 0
// where none runs; whether the circle is shown, since when, and whether it stands still at one stage for less motion;
// the one cell its pictures lie in, their side and stages, each stage's three lanes of a blit side by side pixels; the
// pixels saved from under it, the rectangle they came from, and whether any are saved
typedef struct
{
    unsigned long long request_start_ms[GUI_BUSY_ASKS];
    unsigned long long oldest;
    unsigned long long newest;
    unsigned long long work_start_ms;
    int shown;
    unsigned long long visible_start_ms;
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
    int under_saved;
    unsigned int stage_drawn;
} GuiBusy;

// a circle with nothing asked; and its cell given back
void gui_busy_init(const GuiBusyStillCfg *args);
void gui_busy_free(const GuiBusyStillCfg *args);
// an ask made under `ticket`, and the ask of `ticket` answered; whether work the CLI reports on past its
// answer runs as of `now`, standing as an ask does from when it began until it ends; and when the oldest ask unanswered
// or work running began, in `*since`: 1, or 0 where none stands
void gui_busy_request(const GuiBusyTicketCfg *args);
void gui_busy_answer(const GuiBusyTicketCfg *args);
void gui_busy_work(const GuiBusyRunningCfg *args);
int gui_busy_waiting(const GuiBusySinceCfg *args);
// the tick come: the circle come where an ask has stood past the wait, gone where none has and it has stood
// its least: 1 where it came or went; and the milliseconds from `now` until a tick is wanted, 0 where none is
int gui_busy_tick(const GuiBusyTicketCfg *args);
unsigned long long gui_busy_next_deadline_ms(const GuiBusyCfg *args);
// the stage standing, the first while it stands still; and whether it stands still, its pictures made again
// for it
unsigned int gui_busy_stage_at(const GuiBusyCfg *args);
void gui_busy_still(const GuiBusyStillCfg *args);
// the circle's rectangle in `frame`, at the editor's foot on its right
GuiRect gui_busy_rect(const GuiBusyFrameCfg *args);
// the circle laid at the end of a whole frame, its pictures made where they are wanted, the pixels under it saved
// first; and a step of its stage alone: the pixels saved restored and the stage blitted over them: the
// rectangle to show again, empty where none is saved
void gui_busy_draw(const GuiBusyFrameCfg *args);
GuiRect gui_busy_redrawn(const GuiBusyFrameCfg *args);
struct GuiBusyFrameCfg
{
    GuiBusy *busy;
    const GuiFrame *frame;
    GuiRaster *raster;
    const GuiPalette *palette;
};

struct GuiBusyTicketCfg
{
    GuiBusy *busy;
    unsigned long long ticket;
};

struct GuiBusyRunningCfg
{
    GuiBusy *busy;
    int running;
};

struct GuiBusyCfg
{
    const GuiBusy *busy_read;
};

struct GuiBusyStillCfg
{
    GuiBusy *busy;
    int still;
};

struct GuiBusySinceCfg
{
    const GuiBusy *busy_read;
    unsigned long long *since;
};

typedef struct
{
    void (*init)(const GuiBusyStillCfg *args);
    void (*free)(const GuiBusyStillCfg *args);
    void (*still)(const GuiBusyStillCfg *args);
} CliBusyStillNs;
EMBED_TABLE_LAYOUT(CliBusyStillNs, init, free, still);
EMBED_TABLE_STORAGE CliBusyStillNs cli_busy_still EMBED_UNUSED = {
    .init = gui_busy_init,
    .free = gui_busy_free,
    .still = gui_busy_still,
};

typedef struct
{
    void (*request)(const GuiBusyTicketCfg *args);
    void (*answer)(const GuiBusyTicketCfg *args);
    int (*tick)(const GuiBusyTicketCfg *args);
} GuiBusyTicketNs;
EMBED_TABLE_LAYOUT(GuiBusyTicketNs, request, answer, tick);
EMBED_TABLE_STORAGE GuiBusyTicketNs gui_busy_ticket EMBED_UNUSED = {
    .request = gui_busy_request,
    .answer = gui_busy_answer,
    .tick = gui_busy_tick,
};

typedef struct
{
    void (*work)(const GuiBusyRunningCfg *args);
} GuiBusyRunningNs;
EMBED_TABLE_LAYOUT(GuiBusyRunningNs, work);
EMBED_TABLE_STORAGE GuiBusyRunningNs gui_busy_running EMBED_UNUSED = {
    .work = gui_busy_work,
};

typedef struct
{
    int (*waiting)(const GuiBusySinceCfg *args);
} GuiBusySinceNs;
EMBED_TABLE_LAYOUT(GuiBusySinceNs, waiting);
EMBED_TABLE_STORAGE GuiBusySinceNs gui_busy_since EMBED_UNUSED = {
    .waiting = gui_busy_waiting,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiBusyCfg *args);
    unsigned int (*stage_at)(const GuiBusyCfg *args);
} GuiBusyNs;
EMBED_TABLE_LAYOUT(GuiBusyNs, next_deadline_ms, stage_at);
EMBED_TABLE_STORAGE GuiBusyNs gui_busy EMBED_UNUSED = {
    .next_deadline_ms = gui_busy_next_deadline_ms,
    .stage_at = gui_busy_stage_at,
};

typedef struct
{
    GuiRect (*rect)(const GuiBusyFrameCfg *args);
    void (*draw)(const GuiBusyFrameCfg *args);
    GuiRect (*redrawn)(const GuiBusyFrameCfg *args);
} GuiBusyFrameNs;
EMBED_TABLE_LAYOUT(GuiBusyFrameNs, rect, draw, redrawn);
EMBED_TABLE_STORAGE GuiBusyFrameNs gui_busy_frame EMBED_UNUSED = {
    .rect = gui_busy_rect,
    .draw = gui_busy_draw,
    .redrawn = gui_busy_redrawn,
};

#endif
