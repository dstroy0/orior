// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// panel.h: what the panels over the editor share: the box each stands on, the widget's ground inside a line with the
// corners rounded; Markdown laid out in a width and drawn, paragraphs in the text face wrapped at blanks, fenced code
// in the code face line by line, inline code in the code face in the type's color, a rule as a line; and a box placed
// against the cell it answers for, below it where the room allows, else above, kept inside its bounds (panel.c)
#ifndef GUI_PARTS_PANEL_H
#define GUI_PARTS_PANEL_H
#include "../glyphs.h"
#include "../palette.h"

// the sizes in the page's pixels: a panel's corner radius, --radius; the text face's size and its line's height in a
// hover, 13.5px over 1.5; the code face's size and its line's height, 12.5px over 1.45; a paragraph's margin; a rule's
// margin; and how far a box stands off the cell it answers for
#define GUI_PANEL_RADIUS 6
#define GUI_PANEL_TEXT_SIZE 13u
#define GUI_PANEL_TEXT_LINE 20
#define GUI_PANEL_CODE_SIZE 12u
#define GUI_PANEL_CODE_LINE 18
#define GUI_PANEL_PARAGRAPH 5
#define GUI_PANEL_RULE 7
#define GUI_PANEL_OFF 2
// the most bytes a run of a line takes as it is measured
#define GUI_PANEL_RUN 1024u

// the box drawn: `rect` filled with the widget's ground inside a one pixel line of the widget's line color, the
// corners rounded
void gui_panel_box_drawn(GuiRaster *raster, const GuiPalette *palette, GuiRect rect, float scale);
// `length` bytes of Markdown `text` laid out `wide` pixels wide at `scale` from the top left x, y and drawn on `raster`,
// no further than `clip`, or measured alone where `raster` is NULL: the height the text takes in pixels. A line of
// three backticks opens and closes a fenced block, each line of which is drawn in the code face in the editor's text
// color, broken by letters where it is wider than the width; a line of three dashes alone is a rule; a blank line ends
// a paragraph; every other run of lines is one paragraph, its words laid in the text face and wrapped at blanks, the
// text between single backticks in the code face in the type's color
int gui_panel_markdown(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const char *text,
                       unsigned long long length, int x, int y, int wide, float scale, GuiRect clip);
// a box `wide` by `tall` placed for the cell `anchor`: below the cell's row, off by GUI_PANEL_OFF, where `bounds` has
// the room, else above it; its left at the anchor's less `back`, kept inside the bounds; `above` 1 prefers above
GuiRect gui_panel_placed(GuiRect anchor, int wide, int tall, int back, GuiRect bounds, int above, float scale);
#endif
