// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.h: the hover panel: the pointer resting on a word of the editor's text asks the CLI what the word is, and the
// answer's parts, the tree's declarations and the server's Markdown, are shown in a box under the word's row, a rule
// between the parts; the box stays while the pointer is on the word, a cell either side of it, or in the box, and goes
// a linger after the pointer leaves them; the pointer resting on another word asks again (hover.c)
#ifndef GUI_PARTS_HOVER_H
#define GUI_PARTS_HOVER_H
#include "../../../cli/src/text/json_tree.h"
#include "panel.h"

// the milliseconds a rest takes before the word is asked about and the box lingers after the pointer leaves; the box's
// most width and height in the page's pixels, 36rem by 18rem, and its padding down and across; the cells either side of
// the word the pointer may stand on and keep the box; and the bytes the parts are joined in
#define GUI_HOVER_RESTS 380ull
#define GUI_HOVER_LINGER 300ull
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
    unsigned long long moved_ms;
    int asked;
    unsigned long long asked_row;
    unsigned long long asked_column;
    unsigned long long left_ms;
} GuiHover;

// a hover showing nothing; its cell given back; and the box hidden, its parts given back
void gui_hover_made(GuiHover *hover);
void gui_hover_given(GuiHover *hover);
void gui_hover_hidden(GuiHover *hover);
// the pointer moved to x, y at `now`: `in_text` 1 where it stands in the editor's text at `row` and `column`; a new
// cell begins a rest, and a pointer off the word and the box begins the linger
void gui_hover_moved(GuiHover *hover, int in_text, unsigned long long row, unsigned long long column, int x, int y,
                     unsigned long long now);
// the milliseconds until the hover next wants the clock, 0 for never
unsigned long long gui_hover_due(const GuiHover *hover, unsigned long long now);
// the clock come at `now`: the box hidden where the linger is up, and `ask` set 1 where the rest is up and the cell
// the pointer stands on is to be asked about, its row and cell into the hover's asked place: 1 where a frame is owed
int gui_hover_ticked(GuiHover *hover, unsigned long long now, int *ask);
// the answer `root` of `json` taken: `{"from", "to", "parts"}` shown as its parts joined, or null, which hides the box
// where the pointer stands off it: 1, or 0 where a cell could not be taken
int gui_hover_put(GuiHover *hover, const CliJson *json, unsigned int root);
// whether the pixel at x, y stands in the box shown
int gui_hover_holds(const GuiHover *hover, int x, int y);
// the box drawn for the word whose first cell is `anchor` and last cell `last`, placed under the word's row inside
// `bounds` at `scale`, its parts as Markdown; the word's rectangle and the box's recorded
void gui_hover_drawn(GuiHover *hover, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, GuiRect anchor,
                     GuiRect last, GuiRect bounds, float scale);
#endif
