// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.h: the explorer in the side pane beside the editor: the group's title at its head and, for the explorer's
// group, the pane of the tree's files, its head the tree's name and its rows the folders and files, a folder at a
// time as each is opened. The window asks the CLI for a folder's listing and hands the answer here under the call's
// ticket; the entries stand as rows under their folder, a level deeper, in the listing's order. A press on a folder
// opens or closes it, asking for its listing the first time, a press on a file picks it to open, the wheel scrolls
// the rows, and the pointer marks the row it stands on; a row is a line down from each folder above it, the folder's
// arrow, the folder's icon or the file's mark, and the name (explorer.c)
#ifndef GUI_PARTS_EXPLORER_H
#define GUI_PARTS_EXPLORER_H
typedef struct GuiExplorerFrameXYCfg GuiExplorerFrameXYCfg;
typedef struct GuiExplorerFrameXCfg GuiExplorerFrameXCfg;
typedef struct GuiExplorerTicketCfg GuiExplorerTicketCfg;
typedef struct GuiExplorerCfg GuiExplorerCfg;
typedef struct GuiExplorerNodeCfg GuiExplorerNodeCfg;
typedef struct GuiExplorerRowCfg GuiExplorerRowCfg;
typedef struct GuiExplorerRowFrameCfg GuiExplorerRowFrameCfg;
typedef struct GuiExplorerFrameCfg GuiExplorerFrameCfg;
#include "../../../cli/src/text/json_tree.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the most nodes the cells hold and the most bytes their paths take; no row; and the sizes in the page's pixels:
// the explorer's head, the line over the pane and the pane's head, a row, a row's padding at the left, a guide's
// width, the arrow's box, the icon's box and the gap after it, the head's text size, a row's and a mark's, and the
// arrow's side
#define GUI_EXPLORER_NODES 8192u
#define GUI_EXPLORER_TEXT (1024u * 1024u)
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

// what a press picks: nothing, a folder's listing to ask for, or a file to open, each with its path from the tree's
// top
#define GUI_PICK_NONE 0u
#define GUI_PICK_LIST 1u
#define GUI_PICK_OPEN 2u

// A node, a folder or file of the tree: its path's start in the explorer's text and its length, where its name starts
// in the path, how deep it stands, whether it is a folder, whether git ignores it, whether it is open and whether its
// entries are listed, and the ticket of its listing asked for, 0 where none is in flight
typedef struct
{
    unsigned int path;
    unsigned int length;
    unsigned int name;
    unsigned int depth;
    int dir;
    int ignored;
    int open;
    int listed;
    unsigned long long ticket;
} GuiNode;

// What a press picked: its kind and the path
typedef struct
{
    unsigned int kind;
    char path[CLI_PATH_MAX_LEN];
} GuiPick;

// The explorer: the nodes in the tree's order, a folder's entries after it, and how many; the text their paths are
// saved in and how much of it is used; the row the pointer stands on, GUI_EXPLORER_NONE for none; how many rows are
// scrolled off the pane's top; and whether the top folder's listing came and the ticket it was asked under
typedef struct
{
    GuiNode *node;
    unsigned int nodes;
    char *text;
    unsigned long long used;
    int pointed;
    unsigned int scroll;
    int root_listed;
    unsigned long long root_ticket;
} GuiExplorer;

// an explorer made holding no nodes, its cells taken: 1, or 0 where they could not be; every cell given back; and
// the nodes let go, the top folder unlisted
int gui_explorer_init(const GuiExplorerCfg *args);
void gui_explorer_free(const GuiExplorerCfg *args);
void gui_explorer_clear(const GuiExplorerCfg *args);
// the folder at `dir`, the top folder where empty, marked as asked for under the call's `ticket`; and a listing
// answered under `ticket` taken, `root` the array of entries, each `{"name", "path", "dir", "ignored"}`, put under
// the folder asked: 1, or 0 where no folder was asked under the ticket
void gui_explorer_listing(const GuiExplorerCfg *args);
int gui_explorer_list(const GuiExplorerTicketCfg *args);
// whether the top folder's listing was asked for or came
int gui_explorer_root_request(const GuiExplorerNodeCfg *args);
// the rows, the nodes whose folders above are all open: how many, and the node of the row `row` into `node`: 1, or 0
// past them; and a node's path, its length into `length`
unsigned int gui_explorer_rows(const GuiExplorerFrameXYCfg *args);
int gui_explorer_row(const GuiExplorerRowCfg *args);
const char *gui_explorer_path(const GuiExplorerNodeCfg *args);
// the rectangle of the row `row` in the frame's side as the rows are scrolled; and the row at x, y: 1 with its number
// into `row`, else 0
GuiRect gui_explorer_row_rect(const GuiExplorerRowFrameCfg *args);
int gui_explorer_hit(const GuiExplorerFrameXYCfg *args);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_explorer_move(const GuiExplorerFrameXCfg *args);
// a press at x, y: a folder opened or closed, or a file picked, into `pick`: 1 where the press was the side's
int gui_explorer_press(const GuiExplorerFrameXCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_explorer_wheel(const GuiExplorerFrameCfg *args);
// the side drawn: its ground and its edge, `title` at its head, and where `folders` is 1 the files' pane headed by
// `tree`, the tree's name, with the rows, the one pointed on the pointed ground and the one at `shown`, the file the
// editor shows, on the selection's
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
    const char *shown;
};

struct GuiExplorerFrameXCfg
{
    GuiExplorer *explorer;
    const GuiFrame *frame;
    int x;
    int y;
    GuiPick *pick;
};

struct GuiExplorerTicketCfg
{
    GuiExplorer *explorer;
    unsigned long long ticket;
    const CliJson *json;
    unsigned int root;
};

struct GuiExplorerCfg
{
    GuiExplorer *explorer;
    const char *dir;
    unsigned long long ticket;
};

struct GuiExplorerNodeCfg
{
    const GuiExplorer *explorer_read;
    unsigned int node_value;
    unsigned long long *length;
};

struct GuiExplorerRowCfg
{
    const GuiExplorer *explorer_read;
    unsigned int row;
    unsigned int *node;
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
    int (*init)(const GuiExplorerCfg *args);
    void (*free)(const GuiExplorerCfg *args);
    void (*clear)(const GuiExplorerCfg *args);
    void (*listing)(const GuiExplorerCfg *args);
} CliExplorerNs;
EMBED_TABLE_LAYOUT(CliExplorerNs, init, free, clear, listing);
EMBED_TABLE_STORAGE CliExplorerNs cli_explorer EMBED_UNUSED = {
    .init = gui_explorer_init,
    .free = gui_explorer_free,
    .clear = gui_explorer_clear,
    .listing = gui_explorer_listing,
};

typedef struct
{
    int (*list)(const GuiExplorerTicketCfg *args);
} GuiExplorerTicketNs;
EMBED_TABLE_LAYOUT(GuiExplorerTicketNs, list);
EMBED_TABLE_STORAGE GuiExplorerTicketNs gui_explorer_ticket EMBED_UNUSED = {
    .list = gui_explorer_list,
};

typedef struct
{
    int (*root_request)(const GuiExplorerNodeCfg *args);
    const char *(*path)(const GuiExplorerNodeCfg *args);
} GuiExplorerNodeNs;
EMBED_TABLE_LAYOUT(GuiExplorerNodeNs, root_request, path);
EMBED_TABLE_STORAGE GuiExplorerNodeNs gui_explorer_node EMBED_UNUSED = {
    .root_request = gui_explorer_root_request,
    .path = gui_explorer_path,
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
    int (*row)(const GuiExplorerRowCfg *args);
} CliExplorerRowNs;
EMBED_TABLE_LAYOUT(CliExplorerRowNs, row);
EMBED_TABLE_STORAGE CliExplorerRowNs cli_explorer_row EMBED_UNUSED = {
    .row = gui_explorer_row,
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
    int (*press)(const GuiExplorerFrameXCfg *args);
} GuiExplorerFrameXNs;
EMBED_TABLE_LAYOUT(GuiExplorerFrameXNs, move, press);
EMBED_TABLE_STORAGE GuiExplorerFrameXNs gui_explorer_frame_x EMBED_UNUSED = {
    .move = gui_explorer_move,
    .press = gui_explorer_press,
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
