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
#include "../../../cli/src/text/json_tree.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"

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
// kept in and how much of it is used; the row the pointer stands on, GUI_EXPLORER_NONE for none; how many rows are
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
int gui_explorer_made(GuiExplorer *explorer);
void gui_explorer_given(GuiExplorer *explorer);
void gui_explorer_emptied(GuiExplorer *explorer);
// the folder at `dir`, the top folder where empty, marked as asked for under the call's `ticket`; and a listing
// answered under `ticket` taken, `root` the array of entries, each `{"name", "path", "dir", "ignored"}`, put under
// the folder asked: 1, or 0 where no folder was asked under the ticket
void gui_explorer_listing(GuiExplorer *explorer, const char *dir, unsigned long long ticket);
int gui_explorer_listed(GuiExplorer *explorer, unsigned long long ticket, const CliJson *json, unsigned int root);
// whether the top folder's listing was asked for or came
int gui_explorer_root_asked(const GuiExplorer *explorer);
// the rows, the nodes whose folders above are all open: how many, and the node of the row `row` into `node`: 1, or 0
// past them; and a node's path, its length into `length`
unsigned int gui_explorer_rows(const GuiExplorer *explorer);
int gui_explorer_row(const GuiExplorer *explorer, unsigned int row, unsigned int *node);
const char *gui_explorer_path(const GuiExplorer *explorer, unsigned int node, unsigned long long *length);
// the rectangle of the row `row` in the frame's side as the rows are scrolled; and the row at x, y: 1 with its number
// into `row`, else 0
GuiRect gui_explorer_row_rect(const GuiFrame *frame, const GuiExplorer *explorer, unsigned int row);
int gui_explorer_hit(const GuiExplorer *explorer, const GuiFrame *frame, int x, int y, unsigned int *row);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_explorer_moved(GuiExplorer *explorer, const GuiFrame *frame, int x, int y);
// a press at x, y: a folder opened or closed, or a file picked, into `pick`: 1 where the press was the side's
int gui_explorer_pressed(GuiExplorer *explorer, const GuiFrame *frame, int x, int y, GuiPick *pick);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_explorer_wheeled(GuiExplorer *explorer, const GuiFrame *frame, int rows);
// the side drawn: its ground and its edge, `title` at its head, and where `folders` is 1 the files' pane headed by
// `tree`, the tree's name, with the rows, the one pointed on the pointed ground and the one at `shown`, the file the
// editor shows, on the selection's
void gui_explorer_drawn(const GuiExplorer *explorer, GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons,
                        const GuiPalette *palette, const GuiFrame *frame, const char *title, int folders,
                        const char *tree, const char *shown);
#endif
