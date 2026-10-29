// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.h: the tabs row above the editor: a tab for each file open, its name in the code face with a close mark after
// it, the one shown on its own ground with a line along its top and down its left; a press on a tab's name shows its
// file and one on its close mark closes it (tabs.c)
#ifndef GUI_PARTS_TABS_H
#define GUI_PARTS_TABS_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"

// the most tabs the array holds; and the sizes in the page's pixels: a tab's text size, its padding at its left and its
// right, the gap between its name and its close mark, its corner radius, the line along the shown tab's top and left,
// and the close mark's padding each side
#define GUI_TABS_MOST 32u
#define GUI_TAB_SIZE 12u
#define GUI_TAB_PAD_LEFT 14
#define GUI_TAB_PAD_RIGHT 6
#define GUI_TAB_GAP 7
#define GUI_TAB_RADIUS 6
#define GUI_TAB_LINE 2
#define GUI_TAB_CLOSE_PAD 5

// A tab: the file's path from the tree's top, the key its branch is stored under, whether the editor's view is open
// on it, whether its lines end in a carriage return and a newline, the ticket of its open in flight, 0 for none, and
// the names its grammar gives its tokens, `names_size` bytes of the array's JSON in a cell, NULL where none came
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    int viewed;
    int crlf;
    unsigned long long asked_open;
    char *names;
    unsigned long long names_size;
} GuiTab;

// the name a tab shows, the last part of its path, into `name`, its length
const char *gui_tab_name(const GuiTab *tab, unsigned long long *length);
// the rectangle of the tab `at` of the `count` tabs in the frame's tabs row, and its close mark's
GuiRect gui_tab_rect(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count, unsigned int at);
GuiRect gui_tab_close_rect(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count,
                           unsigned int at);
// the tab at x, y: 1 with its place into `at` and whether the pixel stands on its close mark into `closing`, else 0
int gui_tabs_hit(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count, int x, int y,
                 unsigned int *at, int *closing);
// the row drawn in the frame's tabs rectangle: its ground, the line along its foot, and each tab, the one `active`
// on its own ground with its lines, -1 for none
void gui_tabs_drawn(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const GuiFrame *frame,
                    const GuiTab *tab, unsigned int count, int active);
#endif
