// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.h: the explorer in the side pane beside the editor: the group's title at its head and, for the explorer's
// group, the pane of the tree's files, its head the tree's name and its rows the folders and files, a folder at a
// time as each is opened, as the controller publishes them. The wheel scrolls the rows and the pointer marks the row it
// stands on; a row is a line down from each folder above it, the folder's arrow, the folder's icon or the file's mark,
// and the name (explorer.c)
#ifndef GUI_PARTS_EXPLORER_H
#define GUI_PARTS_EXPLORER_H
#include "../../controller/controller_parts.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiExplorerFrameXYCfg GuiExplorerFrameXYCfg;
typedef struct GuiExplorerFrameXCfg GuiExplorerFrameXCfg;
typedef struct GuiExplorerCfg GuiExplorerCfg;
typedef struct GuiExplorerRowFrameCfg GuiExplorerRowFrameCfg;
typedef struct GuiExplorerFrameCfg GuiExplorerFrameCfg;

// no row; and the sizes in the page's pixels:
// the explorer's head, the line over the pane and the pane's head, a row, a row's padding at the left, a guide's
// width, the arrow's box, the icon's box and the gap after it, the head's text size, a row's and a mark's, and the
// arrow's side
#define GUI_EXPLORER_NONE (-1)
#define GUI_EXPLORER_HEAD 30
#define GUI_EXPLORER_PANE_LINE 1
#define GUI_EXPLORER_PANE_HEAD 22
#define GUI_EXPLORER_ROW 22
#define GUI_EXPLORER_PAD_LEFT 8
#define GUI_EXPLORER_GUIDE 12
#define GUI_EXPLORER_TWISTY 16
#define GUI_EXPLORER_ICON 16
#define GUI_EXPLORER_ICON_GAP 8
#define GUI_EXPLORER_HEAD_SIZE 12u
#define GUI_EXPLORER_ROW_SIZE 13u
#define GUI_EXPLORER_MARK_SIZE 11u
#define GUI_EXPLORER_ARROW 5

// The explorer: the rows as last published, NULL for none, the controller's and read alone; the row the pointer stands
// on, GUI_EXPLORER_NONE for none; and how many rows are scrolled off the pane's top
typedef struct
{
    const CliExplorerRows *rows;
    int pointed;
    unsigned int scroll;
} GuiExplorer;

// an explorer holding no rows
void gui_explorer_init(const GuiExplorerCfg *args);
// how many rows there are
unsigned int gui_explorer_rows(const GuiExplorerFrameXYCfg *args);
// the rectangle of the row `row` in the frame's side as the rows are scrolled; and the row at x, y: 1 with its number
// into `row`, else 0
GuiRect gui_explorer_row_rect(const GuiExplorerRowFrameCfg *args);
int gui_explorer_hit(const GuiExplorerFrameXYCfg *args);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_explorer_move(const GuiExplorerFrameXCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_explorer_wheel(const GuiExplorerFrameCfg *args);
// the side drawn: its ground and its edge, `title` at its head, and where `folders` is 1 the files' pane headed by
// `tree`, the tree's name, with the rows, the one pointed on the pointed ground and the row of the file the editor
// shows on the selection's
void gui_explorer_draw(const GuiExplorerFrameXYCfg *args);
struct GuiExplorerFrameXYCfg
{
    const GuiExplorer *explorer_read;
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *row_pointer;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    GuiIcons *icons;
    const GuiPalette *palette;
    const char *title;
    int folders;
    const char *tree;
};

struct GuiExplorerFrameXCfg
{
    GuiExplorer *explorer;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiExplorerCfg
{
    GuiExplorer *explorer;
};

struct GuiExplorerRowFrameCfg
{
    const GuiExplorer *explorer_read;
    unsigned int row;
    const GuiFrame *frame;
};

struct GuiExplorerFrameCfg
{
    GuiExplorer *explorer;
    const GuiFrame *frame;
    int rows;
};

typedef struct
{
    void (*init)(const GuiExplorerCfg *args);
} CliExplorerNs;
EMBED_TABLE_LAYOUT(CliExplorerNs, init);
EMBED_TABLE_STORAGE CliExplorerNs cli_explorer EMBED_UNUSED = {
    .init = gui_explorer_init,
};

typedef struct
{
    unsigned int (*rows)(const GuiExplorerFrameXYCfg *args);
    int (*hit)(const GuiExplorerFrameXYCfg *args);
    void (*draw)(const GuiExplorerFrameXYCfg *args);
} GuiExplorerFrameXYNs;
EMBED_TABLE_LAYOUT(GuiExplorerFrameXYNs, rows, hit, draw);
EMBED_TABLE_STORAGE GuiExplorerFrameXYNs gui_explorer_frame_xy EMBED_UNUSED = {
    .rows = gui_explorer_rows,
    .hit = gui_explorer_hit,
    .draw = gui_explorer_draw,
};

typedef struct
{
    GuiRect (*row_rect)(const GuiExplorerRowFrameCfg *args);
} GuiExplorerRowFrameNs;
EMBED_TABLE_LAYOUT(GuiExplorerRowFrameNs, row_rect);
EMBED_TABLE_STORAGE GuiExplorerRowFrameNs gui_explorer_row_frame EMBED_UNUSED = {
    .row_rect = gui_explorer_row_rect,
};

typedef struct
{
    int (*move)(const GuiExplorerFrameXCfg *args);
} GuiExplorerFrameXNs;
EMBED_TABLE_LAYOUT(GuiExplorerFrameXNs, move);
EMBED_TABLE_STORAGE GuiExplorerFrameXNs gui_explorer_frame_x EMBED_UNUSED = {
    .move = gui_explorer_move,
};

typedef struct
{
    int (*wheel)(const GuiExplorerFrameCfg *args);
} GuiExplorerFrameNs;
EMBED_TABLE_LAYOUT(GuiExplorerFrameNs, wheel);
EMBED_TABLE_STORAGE GuiExplorerFrameNs gui_explorer_frame EMBED_UNUSED = {
    .wheel = gui_explorer_wheel,
};

#endif
