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
typedef struct GuiMenubarJsonCfg GuiMenubarJsonCfg;
typedef struct GuiMenubarGlyphsFrameXCfg GuiMenubarGlyphsFrameXCfg;
typedef struct GuiMenubarGlyphsCfg GuiMenubarGlyphsCfg;
typedef struct GuiMenubarGlyphsFrameCfg GuiMenubarGlyphsFrameCfg;
typedef struct GuiMenubarFrameCfg GuiMenubarFrameCfg;
typedef struct GuiMenubarLevelCfg GuiMenubarLevelCfg;
typedef struct GuiMenubarXCfg GuiMenubarXCfg;
typedef struct GuiMenubarChoiceCfg GuiMenubarChoiceCfg;
typedef struct GuiMenubarNameCfg GuiMenubarNameCfg;
typedef struct GuiMenubarCfg GuiMenubarCfg;
#include "../../../cli/src/text/json_tree.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the titles the bar holds at the most and the menus open at once at the most, one under another; and the sizes in
// the page's pixels: a title's padding each side and its margins across and along, its corner radius, how far the
// titles start after the bar's left edge, the bar's padding of 16 less the eye's negative margin of 4.4, the room the
// eye takes before them while the claim does not hold, its width and the gap of .7rem after it, the room saved at the
// bar's right for the tree's name, a menu's least width,
// its padding and corner radius, an item's padding across, its height, the least gap between a label and its keys, a
// line's height with its margins, how far a menu opens below its title, how far a menu of items opens beside and
// above its item, how far a menu saves from the window's edges, and the keys' size
#define GUI_MENUBAR_TITLES 40u
#define GUI_MENU_LEVELS 4u
#define GUI_TITLE_PAD 12
#define GUI_TITLE_MARGIN_X 2
#define GUI_TITLE_MARGIN_Y 7
#define GUI_TITLE_RADIUS 5
#define GUI_MENUBAR_START 12
#define GUI_MENUBAR_EYE_ROOM 40
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
    unsigned long long submenu_deadline_ms;
    unsigned int submenu_level;
    int submenu_index;
} GuiMenubar;

// the delay before the items under the row the pointer is on open beside it, and before the items open beside a
// sibling close
#define GUI_SUBMENU_DELAY_MS 180ull

// a menu bar holding no catalog; and the catalog `json` given it, `menus` the node of its menus' array
void gui_menubar_init(const GuiMenubarGlyphsCfg *args);
void gui_menubar_catalog(const GuiMenubarJsonCfg *args);
// the titles laid across the frame's bar, as many as the window's width shows, the rest folded into the last; any
// menu open closed
void gui_menubar_layout(const GuiMenubarJsonCfg *args);
// the titles laid across the frame's bar as it moves, any menu open staying open and following its title; and the
// titles as a stage of the claim's movement laid them, `count` of them, the menus from `folded_from` on folded, any
// menu open following the same way
void gui_menubar_titles_layout(const GuiMenubarJsonCfg *args);
void gui_menubar_titles_take(const GuiMenubarFrameCfg *args);
// whether the pixel at x, y stands on a title or an open menu
int gui_menubar_holds(const GuiMenubarXCfg *args);
// a press at x, y taken: a title's menu opened or closed, an item's items opened, or an item chosen into `choice`,
// with its command empty where none was: 1 where the press was the bar's or a menu's, else 0, every menu closed where
// one was open
int gui_menubar_press(const GuiMenubarJsonCfg *args);
// the pointer moved to x, y: the title it stands on marked, another's menu opened where one is open, the
// row it stands on marked and a rest begun where its items are to open or the items beside a sibling to close: 1
// where anything changed
int gui_menubar_move(const GuiMenubarGlyphsFrameXCfg *args);
// the submenu deadline, 0 for none; and where it has passed, the row's items opened or closed: 1 where a menu opened or
// closed
unsigned long long gui_menubar_next_deadline_ms(const GuiMenubarCfg *args);
int gui_menubar_tick(const GuiMenubarGlyphsFrameXCfg *args);
// a key named as the page's editor names it taken: 1 where it was the bar's, an item chosen into `choice` where one
// was, its command empty where none
int gui_menubar_key(const GuiMenubarGlyphsCfg *args);
// the item of any menu whose keys are the key `name`, `Ctrl+` read as `Mod+` and the modifiers in any order, that
// can be chosen: 1 with its command and keys into `choice`, else 0 with the choice empty; and whether `keys_length`
// bytes of a catalog's keys name the key `name` so
int gui_menubar_keys_command(const GuiMenubarChoiceCfg *args);
int gui_menubar_keys_name(const GuiMenubarNameCfg *args);
// every menu closed
void gui_menubar_close(const GuiMenubarJsonCfg *args);
// the rows of the open menu at `level`: how many, and the row `index` of them read into `row`: 1, or 0 past them
unsigned int gui_menubar_rows(const GuiMenubarLevelCfg *args);
int gui_menubar_row(const GuiMenubarLevelCfg *args);
// the titles drawn on the bar, the one pointed or open on its ground; and the menus open drawn, each over what is
// under it
void gui_menubar_titles_draw(const GuiMenubarGlyphsFrameCfg *args);
void gui_menubar_menus_draw(const GuiMenubarGlyphsFrameCfg *args);
struct GuiMenubarJsonCfg
{
    GuiMenubar *menubar;
    const CliJson *json;
    unsigned int menus;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    int x;
    int y;
    GuiChoice *choice;
};

struct GuiMenubarGlyphsFrameXCfg
{
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiMenubarGlyphsCfg
{
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    GuiChoice *choice;
    const char *given;
};

struct GuiMenubarGlyphsFrameCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    const GuiMenubar *menubar_read;
    GuiRaster *raster;
    const GuiPalette *palette;
};

struct GuiMenubarFrameCfg
{
    GuiMenubar *menubar;
    const GuiFrame *frame;
    const GuiTitle *title;
    unsigned int count;
    unsigned int folded_from;
};

struct GuiMenubarLevelCfg
{
    const GuiMenubar *menubar_read;
    unsigned int level;
    unsigned int index;
    GuiRow *row;
};

struct GuiMenubarXCfg
{
    const GuiMenubar *menubar_read;
    int x;
    int y;
};

struct GuiMenubarChoiceCfg
{
    const GuiMenubar *menubar_read;
    GuiChoice *choice;
    const char *name;
};

struct GuiMenubarNameCfg
{
    const char *name;
    const char *keys;
    unsigned long long keys_length;
};

struct GuiMenubarCfg
{
    const GuiMenubar *menubar_read;
};

typedef struct
{
    void (*init)(const GuiMenubarGlyphsCfg *args);
    int (*key)(const GuiMenubarGlyphsCfg *args);
} GuiMenubarGlyphsNs;
EMBED_TABLE_LAYOUT(GuiMenubarGlyphsNs, init, key);
EMBED_TABLE_STORAGE GuiMenubarGlyphsNs gui_menubar_glyphs EMBED_UNUSED = {
    .init = gui_menubar_init,
    .key = gui_menubar_key,
};

typedef struct
{
    void (*catalog)(const GuiMenubarJsonCfg *args);
    void (*layout)(const GuiMenubarJsonCfg *args);
    void (*titles_layout)(const GuiMenubarJsonCfg *args);
    int (*press)(const GuiMenubarJsonCfg *args);
    void (*close)(const GuiMenubarJsonCfg *args);
} GuiMenubarJsonNs;
EMBED_TABLE_LAYOUT(GuiMenubarJsonNs, catalog, layout, titles_layout, press, close);
EMBED_TABLE_STORAGE GuiMenubarJsonNs gui_menubar_json EMBED_UNUSED = {
    .catalog = gui_menubar_catalog,
    .layout = gui_menubar_layout,
    .titles_layout = gui_menubar_titles_layout,
    .press = gui_menubar_press,
    .close = gui_menubar_close,
};

typedef struct
{
    void (*titles_take)(const GuiMenubarFrameCfg *args);
} GuiMenubarFrameNs;
EMBED_TABLE_LAYOUT(GuiMenubarFrameNs, titles_take);
EMBED_TABLE_STORAGE GuiMenubarFrameNs gui_menubar_frame EMBED_UNUSED = {
    .titles_take = gui_menubar_titles_take,
};

typedef struct
{
    int (*holds)(const GuiMenubarXCfg *args);
} GuiMenubarXNs;
EMBED_TABLE_LAYOUT(GuiMenubarXNs, holds);
EMBED_TABLE_STORAGE GuiMenubarXNs gui_menubar_x EMBED_UNUSED = {
    .holds = gui_menubar_holds,
};

typedef struct
{
    int (*move)(const GuiMenubarGlyphsFrameXCfg *args);
    int (*tick)(const GuiMenubarGlyphsFrameXCfg *args);
} GuiMenubarGlyphsFrameXNs;
EMBED_TABLE_LAYOUT(GuiMenubarGlyphsFrameXNs, move, tick);
EMBED_TABLE_STORAGE GuiMenubarGlyphsFrameXNs gui_menubar_glyphs_frame_x EMBED_UNUSED = {
    .move = gui_menubar_move,
    .tick = gui_menubar_tick,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiMenubarCfg *args);
} GuiMenubarNs;
EMBED_TABLE_LAYOUT(GuiMenubarNs, next_deadline_ms);
EMBED_TABLE_STORAGE GuiMenubarNs gui_menubar EMBED_UNUSED = {
    .next_deadline_ms = gui_menubar_next_deadline_ms,
};

typedef struct
{
    int (*keys_command)(const GuiMenubarChoiceCfg *args);
} GuiMenubarChoiceNs;
EMBED_TABLE_LAYOUT(GuiMenubarChoiceNs, keys_command);
EMBED_TABLE_STORAGE GuiMenubarChoiceNs gui_menubar_choice EMBED_UNUSED = {
    .keys_command = gui_menubar_keys_command,
};

typedef struct
{
    int (*keys_name)(const GuiMenubarNameCfg *args);
} GuiMenubarNameNs;
EMBED_TABLE_LAYOUT(GuiMenubarNameNs, keys_name);
EMBED_TABLE_STORAGE GuiMenubarNameNs gui_menubar_name EMBED_UNUSED = {
    .keys_name = gui_menubar_keys_name,
};

typedef struct
{
    unsigned int (*rows)(const GuiMenubarLevelCfg *args);
    int (*row)(const GuiMenubarLevelCfg *args);
} GuiMenubarLevelNs;
EMBED_TABLE_LAYOUT(GuiMenubarLevelNs, rows, row);
EMBED_TABLE_STORAGE GuiMenubarLevelNs gui_menubar_level EMBED_UNUSED = {
    .rows = gui_menubar_rows,
    .row = gui_menubar_row,
};

typedef struct
{
    void (*titles_draw)(const GuiMenubarGlyphsFrameCfg *args);
    void (*menus_draw)(const GuiMenubarGlyphsFrameCfg *args);
} GuiMenubarGlyphsFrameNs;
EMBED_TABLE_LAYOUT(GuiMenubarGlyphsFrameNs, titles_draw, menus_draw);
EMBED_TABLE_STORAGE GuiMenubarGlyphsFrameNs gui_menubar_glyphs_frame EMBED_UNUSED = {
    .titles_draw = gui_menubar_titles_draw,
    .menus_draw = gui_menubar_menus_draw,
};

#endif
