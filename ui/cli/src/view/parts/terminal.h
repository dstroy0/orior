// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal.h: the terminal's panel at the work's foot under both views, as the controller publishes it: its top line;
// its strip of tabs, each its number and folder and a × that ends it, the tab shown on the ground with the signal
// under it and an ended shell's quieter, then New Terminal and Split, Split in the signal while the panel is split; and
// under the strip the panes shown, one or two side by side with a line between, the one the keys go to marked at its
// top while there are two, each its view's rows of its text with the selection under them and the xterm's rows drawn
// over the text's from the line they start on, each cell in its colors and weight, the cursor solid where the panel
// takes the keys and an outline where it does not (terminal.c)
#ifndef GUI_PARTS_TERMINAL_H
#define GUI_PARTS_TERMINAL_H
#include "../../controller/controller_parts.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiTerminalCfg GuiTerminalCfg;

// sizes in the page's pixels: the strip's padding at its top and its left, the gap between its items, a tab's height,
// its padding at the left and the right, the gap before its × and the ×'s own padding, a tool's width and height and
// its icon's side, a tab's corner and the line under the tab shown; the views' padding at the top, the sides and the
// foot, the text's size and a row's height, 13px at 1.3; the tabs' text size; and the line marking the screen the keys
// go to
#define GUI_TERMINAL_STRIP_PAD_TOP 2
#define GUI_TERMINAL_STRIP_PAD_LEFT 6
#define GUI_TERMINAL_STRIP_GAP 2
#define GUI_TERMINAL_TAB_HEIGHT 20
#define GUI_TERMINAL_TAB_PAD_LEFT 9
#define GUI_TERMINAL_TAB_PAD_RIGHT 4
#define GUI_TERMINAL_TAB_GAP 6
#define GUI_TERMINAL_CLOSE_PAD 3
#define GUI_TERMINAL_TOOL_WIDTH 24
#define GUI_TERMINAL_TOOL_HEIGHT 22
#define GUI_TERMINAL_ICON 14
#define GUI_TERMINAL_RADIUS 6
#define GUI_TERMINAL_SHOWN_LINE 2
#define GUI_TERMINAL_PAD_TOP 3
#define GUI_TERMINAL_PAD_SIDE 10
#define GUI_TERMINAL_PAD_BOTTOM 6
#define GUI_TERMINAL_TEXT_SIZE 13u
#define GUI_TERMINAL_ROW 17
#define GUI_TERMINAL_TAB_SIZE 12u
#define GUI_TERMINAL_CHOSEN_LINE 2

// The terminal as last published, NULL before any, the controller's and read alone
typedef struct
{
    const CliTerminalPanel *panel;
} GuiTerminal;

// the columns and rows the pane `view` shows, 0 the one shown and 1 the one beside it, into `columns` and `rows`, two
// of each at the least: 1, or 0 where the panel is closed or the view not shown
int gui_terminal_size(const GuiTerminalCfg *args);
// what the pixel at x, y stands on in the panel: CLI_HIT_TERMINAL with the pane into `index` and the row and the
// column the pixel stands nearest the left edge of into `row` and `column`, CLI_HIT_TERMINAL_TAB or
// CLI_HIT_TERMINAL_TAB_CLOSE with the tab, CLI_HIT_TERMINAL_NEW or CLI_HIT_TERMINAL_SPLIT; CLI_HIT_NONE elsewhere
unsigned int gui_terminal_hit(const GuiTerminalCfg *args);
// the row and the column of the pane `view` the pixel at x, y stands nearest, into `row` and `column`, a pixel outside
// the pane taking the nearest of its rows and columns: 1, or 0 where the pane is not shown
int gui_terminal_cell_at(const GuiTerminalCfg *args);
// the panel drawn in the frame's terminal, nothing while it is closed
void gui_terminal_draw(const GuiTerminalCfg *args);
struct GuiTerminalCfg
{
    const GuiTerminal *terminal_read;
    const GuiFrame *frame;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
    unsigned int view;
    unsigned int *columns;
    unsigned int *rows;
    int x;
    int y;
    unsigned int *index;
    unsigned long long *row;
    unsigned long long *column;
};

typedef struct
{
    int (*size)(const GuiTerminalCfg *args);
    unsigned int (*hit)(const GuiTerminalCfg *args);
    int (*cell_at)(const GuiTerminalCfg *args);
    void (*draw)(const GuiTerminalCfg *args);
} GuiTerminalNs;
EMBED_TABLE_LAYOUT(GuiTerminalNs, size, hit, cell_at, draw);
EMBED_TABLE_STORAGE GuiTerminalNs gui_terminal EMBED_UNUSED = {
    .size = gui_terminal_size,
    .hit = gui_terminal_hit,
    .cell_at = gui_terminal_cell_at,
    .draw = gui_terminal_draw,
};

#endif
