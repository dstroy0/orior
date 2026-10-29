// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar.h: the status bar across the foot, tinted with the ink: at its left the crumbs of the file shown, its
// folders then it; at its right what orior holds in RAM, then the editor's line while a file is shown, the cursor as
// line and column, what is selected, the line ends, the encoding, the indentation and the lock (statusbar.c)
#ifndef GUI_PARTS_STATUSBAR_H
#define GUI_PARTS_STATUSBAR_H
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"

// the sizes in the page's pixels: the bar's text size, its padding at each end past the strip, the gap between its
// items, the gap about a crumb's mark, the lock's side and how far it ends before the frame's gap, the memory
// reading's least width in zeros of the text; and the most bytes an item's text takes
#define GUI_STATUS_SIZE 12u
#define GUI_STATUS_PAD 16
#define GUI_STATUS_GAP 18
#define GUI_STATUS_CRUMB_GAP 5
#define GUI_STATUS_LOCK 14
#define GUI_STATUS_LOCK_END 4
#define GUI_STATUS_MEMORY_ZEROS 7
#define GUI_STATUS_TEXT 64u

// What the bar shows: the shown file's path from the tree's top, empty for none; whether the editor has a cursor,
// its line and column from 0 and the anchor the selection runs from; whether the file's lines end in a carriage
// return and a newline; the indentation, tabs 1, spaces 0 or -1 where nothing set it, and its step; and the bytes
// orior holds in RAM, 0 where no reading came
typedef struct
{
    const char *path;
    int has_cursor;
    unsigned long long line;
    unsigned long long col;
    unsigned long long anchor_line;
    unsigned long long anchor_col;
    int crlf;
    int indent_tabs;
    unsigned int indent_size;
    unsigned long long memory;
} GuiStatus;

// the items' texts, each into `text` of `size` bytes: the memory reading in megabytes, to a tenth under a hundred;
// the cursor as line:column from one; the selection as its lines or its letters, empty where none; and the
// indentation as a tab with its step or the step's spaces
void gui_status_memory_text(unsigned long long bytes, char *text, unsigned long long size);
void gui_status_cursor_text(const GuiStatus *status, char *text, unsigned long long size);
void gui_status_selection_text(const GuiStatus *status, char *text, unsigned long long size);
void gui_status_indent_text(const GuiStatus *status, char *text, unsigned long long size);
// the bar drawn in the frame's status rectangle: its tint, the crumbs, and the items from the right
void gui_statusbar_drawn(GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette,
                         const GuiFrame *frame, const GuiStatus *status);
#endif
