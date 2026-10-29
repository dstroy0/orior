// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar.h: the menu bar's titles across the bar and the menus they open, from the catalog's menus as commands_read
// hands them: each menu with items that is neither the strip's nor a popup a title, those the window's width has room
// for, the rest folded into a last title. A title pressed opens its menu under it, the pointer moving onto another
// title while one is open opens that one instead, the pointer on an item marks it and opens the items under it beside
// it once it has rested there, the pointer resting on a sibling closing them again, a press on an item chooses it or
// opens its items at once, Up and Down step the mark, Left and Right step between the menus or out of and
// into the items under one, Enter chooses, Escape closes, a letter marks the next item it begins, and Alt with a
// title's letter opens its menu (menubar.c)
#ifndef GUI_PARTS_MENUBAR_H
#define GUI_PARTS_MENUBAR_H
#include "../../../cli/src/text/json_tree.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"

// the titles the bar holds at the most and the menus open at once at the most, one under another; and the sizes in
// the page's pixels: a title's padding each side and its margins across and along, its corner radius, how far the
// titles start after the bar's left edge, the room kept at the bar's right for the tree's name, a menu's least width,
// its padding and corner radius, an item's padding across, its height, the least gap between a label and its keys, a
// line's height with its margins, how far a menu opens below its title, how far a menu of items opens beside and
// above its item, how far a menu keeps from the window's edges, and the keys' size
#define GUI_MENUBAR_TITLES 40u
#define GUI_MENU_LEVELS 4u
#define GUI_TITLE_PAD 12
#define GUI_TITLE_MARGIN_X 2
#define GUI_TITLE_MARGIN_Y 7
#define GUI_TITLE_RADIUS 5
#define GUI_MENUBAR_START 13
#define GUI_MENUBAR_NAME_ROOM 278
#define GUI_MENU_WIDE_LEAST 208
#define GUI_MENU_PAD 5
#define GUI_MENU_RADIUS 7
#define GUI_ITEM_PAD_X 11
#define GUI_ITEM_TALL 27
#define GUI_ITEM_GAP 26
#define GUI_MENU_LINE_TALL 11
#define GUI_MENU_BELOW 2
#define GUI_MENU_BESIDE 2
#define GUI_MENU_ABOVE 5
#define GUI_MENU_EDGE 4
#define GUI_KEYS_SIZE 12u
// a title with no letter for Alt
#define GUI_NO_LETTER (~0u)

// A title on the bar: the catalog's menu it opens, by its place among the menus, or the folded menus where `folded`
// is 1; its rectangle; and the byte of its label Alt takes it by, GUI_NO_LETTER for none
typedef struct
{
    unsigned int menu;
    int folded;
    GuiRect rect;
    unsigned int letter;
} GuiTitle;

// A row of an open menu as it is read: its kind, a line, an item, or an item with items of its own; its label and
// its keys, each with its length; the array node of its items where it has them; its command with its length; and
// whether it can be chosen
#define GUI_ROW_LINE 0u
#define GUI_ROW_ITEM 1u
#define GUI_ROW_MORE 2u
typedef struct
{
    unsigned int kind;
    const char *label;
    unsigned long long label_length;
    const char *keys;
    unsigned long long keys_length;
    const char *command;
    unsigned long long command_length;
    unsigned int items;
    int disabled;
} GuiRow;

// An open menu: the array node of its items, or the folded menus where `folded` is 1; its rectangle; the row marked,
// -1 for none; and the row whose items are open under it as the next menu, -1 for none
typedef struct
{
    unsigned int items;
    int folded;
    GuiRect rect;
    int pointed;
    int opened;
} GuiMenu;

// What choosing an item hands the window: its command and its keys, each as the catalog gives them, empty where none
typedef struct
{
    char command[256];
    char keys[256];
} GuiChoice;

// The menu bar: the catalog and the node of its menus' array; the titles, how many, and the place among the menus
// from which they are folded into the last title; the title whose menu is open and the one the pointer stands on,
// each -1 for none; the menus open, one under another, and how many; and whether a file is open, which the items that
// need one wait on; and the rest the pointer is in: when it ends, 0 for none, the level of the row it rests on, and the
// row's index where its items are to open, -1 where the rest closes the items open beside a sibling
typedef struct
{
    const CliJson *json;
    unsigned int menus;
    GuiTitle title[GUI_MENUBAR_TITLES];
    unsigned int titles;
    unsigned int folded_from;
    int open;
    int pointed;
    GuiMenu menu[GUI_MENU_LEVELS];
    unsigned int levels;
    int has_editor;
    unsigned long long rest_due_ms;
    unsigned int rest_level;
    int rest_index;
} GuiMenubar;

// the milliseconds the pointer rests on a row before the items under it open beside it, and before the items open
// beside a sibling close, as the page's menu.js rests
#define GUI_MENU_REST 180ull

// a menu bar holding no catalog; and the catalog `json` given it, `menus` the node of its menus' array
void gui_menubar_made(GuiMenubar *menubar);
void gui_menubar_cataloged(GuiMenubar *menubar, const CliJson *json, unsigned int menus);
// the titles laid across the frame's bar, as many as the window's width shows, the rest folded into the last; any
// menu open closed
void gui_menubar_laid(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame);
// whether the pixel at x, y stands on a title or an open menu
int gui_menubar_holds(const GuiMenubar *menubar, int x, int y);
// a press at x, y taken: a title's menu opened or closed, an item's items opened, or an item chosen into `choice`,
// with its command empty where none was: 1 where the press was the bar's or a menu's, else 0, every menu closed where
// one was open
int gui_menubar_pressed(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, int x, int y,
                        GuiChoice *choice);
// the pointer moved to x, y at `now`: the title it stands on marked, another's menu opened where one is open, the
// row it stands on marked and a rest begun where its items are to open or the items beside a sibling to close: 1
// where anything changed
int gui_menubar_moved(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, int x, int y, unsigned long long now);
// the milliseconds from `now` until the rest the pointer is in ends, 0 where it is in none; and the tick come at
// `now`: the rest's items opened or closed where it has ended: 1 where a menu opened or closed
unsigned long long gui_menubar_due(const GuiMenubar *menubar, unsigned long long now);
int gui_menubar_ticked(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, unsigned long long now);
// a key named as the page's editor names it taken: 1 where it was the bar's, an item chosen into `choice` where one
// was, its command empty where none
int gui_menubar_keyed(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, const char *name,
                      GuiChoice *choice);
// the item of any menu whose keys are the key `name`, `Ctrl+` read as `Mod+` and the modifiers in any order, that
// can be chosen: 1 with its command and keys into `choice`, else 0 with the choice empty; and whether `keys_length`
// bytes of a catalog's keys name the key `name` so
int gui_menubar_keys_command(const GuiMenubar *menubar, const char *name, GuiChoice *choice);
int gui_menubar_keys_name(const char *keys, unsigned long long keys_length, const char *name);
// every menu closed
void gui_menubar_closed(GuiMenubar *menubar);
// the rows of the open menu at `level`: how many, and the row `index` of them read into `row`: 1, or 0 past them
unsigned int gui_menubar_rows(const GuiMenubar *menubar, unsigned int level);
int gui_menubar_row(const GuiMenubar *menubar, unsigned int level, unsigned int index, GuiRow *row);
// the titles drawn on the bar, the one pointed or open on its ground; and the menus open drawn, each over what is
// under it
void gui_menubar_titles_drawn(const GuiMenubar *menubar, GuiRaster *raster, GuiGlyphs *glyphs,
                              const GuiPalette *palette, const GuiFrame *frame);
void gui_menubar_menus_drawn(const GuiMenubar *menubar, GuiRaster *raster, GuiGlyphs *glyphs,
                             const GuiPalette *palette, const GuiFrame *frame);
#endif
