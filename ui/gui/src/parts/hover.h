// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.h: the hover panel: the pointer resting on a word of the editor's text asks the CLI what the word is, and the
// answer's parts, the tree's declarations and the server's Markdown, are shown in a box under the word's row, a rule
// between the parts; the box stays while the pointer is on the word, a cell either side of it, or in the box, and goes
// a linger after the pointer leaves them; the pointer resting on another word asks again (hover.c)
#ifndef GUI_PARTS_HOVER_H
#define GUI_PARTS_HOVER_H
typedef struct GuiHoverRasterCfg GuiHoverRasterCfg;
typedef struct GuiHoverInCfg GuiHoverInCfg;
typedef struct GuiHoverStateCfg GuiHoverStateCfg;
typedef struct GuiHoverXCfg GuiHoverXCfg;
typedef struct GuiHoverCfg GuiHoverCfg;
#include "../../../cli/src/text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "panel.h"

// the delay before the word is asked about and the delay before the box hides after the pointer leaves; the box's
// most width and height in the page's pixels, 36rem by 18rem, and its padding down and across; the cells either side of
// the word the pointer may stand on and save the box; and the bytes the parts are joined in
#define GUI_HOVER_SHOW_DELAY_MS 380ull
#define GUI_HOVER_HIDE_DELAY_MS 300ull
#define GUI_HOVER_WIDE 576
#define GUI_HOVER_TALL 288
#define GUI_HOVER_PAD_Y 7
#define GUI_HOVER_PAD_X 11
#define GUI_HOVER_NEAR 1
#define GUI_HOVER_FIRST 4096u

// The hover: whether the box is shown; the parts joined in a cell, `length` bytes, a rule between them; the word's
// line and its first and last units, as the answer places it; the box's rectangle and the word's, in pixels, where
// they were drawn last; the pointer, whether it stands in the editor's text, its row and cell there and its pixels;
// when it last moved in the text, 0 for never, whether the rest at its place was asked about, and the row and cell
// asked; and when it left the word and the box, 0 while it stands on them
typedef struct
{
    int shown;
    char *text;
    unsigned long long length;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
    GuiRect rect;
    GuiRect word;
    int in_text;
    unsigned long long row;
    unsigned long long column;
    int x;
    int y;
    unsigned long long show_deadline_ms;
    int asked;
    unsigned long long asked_row;
    unsigned long long asked_column;
    unsigned long long hide_deadline_ms;
} GuiHover;

// a hover showing nothing; its cell given back; and the box hidden, its parts given back
void gui_hover_init(const GuiHoverStateCfg *args);
void gui_hover_free(const GuiHoverStateCfg *args);
void gui_hover_hide(const GuiHoverStateCfg *args);
// the pointer moved to x, y: `in_text` 1 where it stands in the editor's text at `row` and `column`; a new cell arms
// the show deadline, and a pointer off the word and the box arms the hide deadline
void gui_hover_move(const GuiHoverInCfg *args);
// the earlier of the show and hide deadlines, 0 for none
unsigned long long gui_hover_next_deadline_ms(const GuiHoverCfg *args);
// expired deadlines run: the box hidden where the hide deadline passed, and `ask` set 1 where the show deadline passed
// and the cell the pointer stands on is to be asked about, its row and cell into the hover's asked place: 1 where a
// frame is owed
int gui_hover_tick(const GuiHoverStateCfg *args);
// the answer `root` of `json` taken: `{"from", "to", "parts"}` shown as its parts joined, or null, which hides the box
// where the pointer stands off it: 1, or 0 where a cell could not be taken
int gui_hover_put(const GuiHoverStateCfg *args);
// whether the pixel at x, y stands in the box shown
int gui_hover_holds(const GuiHoverXCfg *args);
// the box drawn for the word whose first cell is `anchor` and last cell `last`, placed under the word's row inside
// `bounds` at `scale`, its parts as Markdown; the word's rectangle and the box's recorded
void gui_hover_draw(const GuiHoverRasterCfg *args);
struct GuiHoverRasterCfg
{
    GuiHover *hover;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect last;
    GuiRect bounds;
    int scale;
};

struct GuiHoverInCfg
{
    GuiHover *hover;
    int in_text;
    unsigned long long row;
    unsigned long long column;
    int x;
    int y;
};

struct GuiHoverStateCfg
{
    GuiHover *hover;
    int *ask;
    const CliJson *json;
    unsigned int root;
};

struct GuiHoverXCfg
{
    int x;
    int y;
    const GuiHover *hover_read;
};

struct GuiHoverCfg
{
    const GuiHover *hover_read;
};

typedef struct
{
    void (*init)(const GuiHoverStateCfg *args);
    void (*free)(const GuiHoverStateCfg *args);
    void (*hide)(const GuiHoverStateCfg *args);
    int (*tick)(const GuiHoverStateCfg *args);
    int (*put)(const GuiHoverStateCfg *args);
} GuiHoverStateNs;
EMBED_TABLE_LAYOUT(GuiHoverStateNs, init, free, hide, tick, put);
EMBED_TABLE_STORAGE GuiHoverStateNs gui_hover_state EMBED_UNUSED = {
    .init = gui_hover_init,
    .free = gui_hover_free,
    .hide = gui_hover_hide,
    .tick = gui_hover_tick,
    .put = gui_hover_put,
};

typedef struct
{
    void (*move)(const GuiHoverInCfg *args);
} GuiHoverInNs;
EMBED_TABLE_LAYOUT(GuiHoverInNs, move);
EMBED_TABLE_STORAGE GuiHoverInNs gui_hover_in EMBED_UNUSED = {
    .move = gui_hover_move,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiHoverCfg *args);
} GuiHoverNs;
EMBED_TABLE_LAYOUT(GuiHoverNs, next_deadline_ms);
EMBED_TABLE_STORAGE GuiHoverNs gui_hover EMBED_UNUSED = {
    .next_deadline_ms = gui_hover_next_deadline_ms,
};

typedef struct
{
    int (*holds)(const GuiHoverXCfg *args);
} GuiHoverXNs;
EMBED_TABLE_LAYOUT(GuiHoverXNs, holds);
EMBED_TABLE_STORAGE GuiHoverXNs gui_hover_x EMBED_UNUSED = {
    .holds = gui_hover_holds,
};

typedef struct
{
    void (*draw)(const GuiHoverRasterCfg *args);
} GuiHoverRasterNs;
EMBED_TABLE_LAYOUT(GuiHoverRasterNs, draw);
EMBED_TABLE_STORAGE GuiHoverRasterNs gui_hover_raster EMBED_UNUSED = {
    .draw = gui_hover_draw,
};

#endif
