// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar.h: the menu bar's titles across the bar and the menus open under them, as the controller publishes them:
// those titles the window's width has capacity for, the rest folded into a last title, which the window tells the
// controller. Where each stands and which pixel stands on what is worked out here; every press and key means what the
// controller says it means (menubar.c)
#ifndef GUI_PARTS_MENUBAR_H
#define GUI_PARTS_MENUBAR_H
#include "../../controller/controller.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiMenubarBarCfg GuiMenubarBarCfg;
typedef struct GuiMenubarGlyphsFrameXCfg GuiMenubarGlyphsFrameXCfg;
typedef struct GuiMenubarGlyphsFrameCfg GuiMenubarGlyphsFrameCfg;
typedef struct GuiMenubarFrameCfg GuiMenubarFrameCfg;
typedef struct GuiMenubarXCfg GuiMenubarXCfg;
typedef struct GuiMenubarCfg GuiMenubarCfg;

// the titles the bar holds at the most and the menus open at once at the most, one under another; and the sizes in
// the page's pixels: a title's padding each side and its margins across and along, its corner radius, how far the
// titles start after the bar's left edge, the bar's padding of 16 less the eye's negative margin of 4.4, the capacity
// the eye takes before them while the claim does not hold, its width and the gap of .7rem after it, the capacity saved
// at the bar's right for the tree's name, a menu's least width, its padding and corner radius, an item's padding
// across, its padding at its left in a menu with rows on or off and where the mark of one that is on stands from its
// left, its height, the least gap between a label and its keys, a line's height with its margins, how far a menu opens
// below its title, how far a menu of items opens beside and above its item, how far a menu saves from the window's
// edges, and the keys' size
#define GUI_MENUBAR_TITLES 40u
#define GUI_MENU_LEVELS 4u
#define GUI_TITLE_PAD 12
#define GUI_TITLE_MARGIN_X 2
#define GUI_TITLE_MARGIN_Y 7
#define GUI_TITLE_RADIUS 5
#define GUI_MENUBAR_START 12
#define GUI_MENUBAR_LOGO_CAPACITY 40
#define GUI_MENUBAR_NAME_CAPACITY 278
#define GUI_MENU_WIDTH_MIN 208
#define GUI_MENU_PAD 5
#define GUI_MENU_RADIUS 7
#define GUI_ITEM_PAD_X 11
#define GUI_ITEM_CHECKS_PAD_X 28
#define GUI_ITEM_CHECK_X 10
#define GUI_ITEM_HEIGHT 27
#define GUI_ITEM_GAP 26
#define GUI_MENU_LINE_HEIGHT 11
#define GUI_MENU_BELOW 2
#define GUI_MENU_ADJACENT 2
#define GUI_MENU_ABOVE 5
#define GUI_MENU_EDGE 4
#define GUI_KEYS_SIZE 12u

// A title on the bar: its place among the menus' titles, or the folded menus where `folded` is 1; and its rectangle
typedef struct
{
    unsigned int menu;
    int folded;
    GuiRect rect;
} GuiTitle;

// The menu bar: the menus as last published, NULL for none, the controller's and read alone; the titles laid, how many,
// and the place among the titles from which they are folded into the last; the title the pointer stands on, -1 for
// none; and where each menu open stands
typedef struct
{
    const CliMenus *menus;
    GuiTitle title[GUI_MENUBAR_TITLES];
    unsigned int titles;
    unsigned int folded_from;
    int pointed;
    GuiRect menu_rect[GUI_MENU_LEVELS];
} GuiMenubar;

// a menu bar holding no menus
void gui_menubar_init(const GuiMenubarCfg *args);
// the titles laid across the frame's bar, as many as the window's width shows, the rest folded into the last, the menus
// open following them; the titles as a stage of the claim's movement laid them, `count` of them, the titles from
// `folded_from` on folded, the menus open following the same way; and the menus open placed under their titles and
// beside their items
void gui_menubar_titles_layout(const GuiMenubarBarCfg *args);
void gui_menubar_titles_alloc(const GuiMenubarFrameCfg *args);
void gui_menubar_menus_layout(const GuiMenubarBarCfg *args);
// whether the pixel at x, y stands on a title or an open menu; and what it stands on, a title's place into `index`, or
// a menu's level into `level` and its row into `index`, the rows' count where it stands on none of them: CLI_HIT_NONE,
// CLI_HIT_MENU_TITLE or CLI_HIT_MENU_ROW
int gui_menubar_holds(const GuiMenubarXCfg *args);
unsigned int gui_menubar_hit(const GuiMenubarXCfg *args);
// the pointer moved to x, y: the title it stands on marked: 1 where that changed
int gui_menubar_move(const GuiMenubarGlyphsFrameXCfg *args);
// the titles drawn on the bar, the one pointed or open on its ground; and the menus open drawn, each over what is under
// it
void gui_menubar_titles_draw(const GuiMenubarGlyphsFrameCfg *args);
void gui_menubar_menus_draw(const GuiMenubarGlyphsFrameCfg *args);
struct GuiMenubarBarCfg
{
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
};

struct GuiMenubarGlyphsFrameXCfg
{
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    int x;
    int y;
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
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    const GuiTitle *title;
    unsigned int count;
    unsigned int folded_from;
};

struct GuiMenubarXCfg
{
    const GuiMenubar *menubar_read;
    int scale;
    int x;
    int y;
    unsigned int *level;
    unsigned int *index;
};

struct GuiMenubarCfg
{
    GuiMenubar *menubar;
};

typedef struct
{
    void (*init)(const GuiMenubarCfg *args);
} GuiMenubarNs;
EMBED_TABLE_LAYOUT(GuiMenubarNs, init);
EMBED_TABLE_STORAGE GuiMenubarNs gui_menubar EMBED_UNUSED = {
    .init = gui_menubar_init,
};

typedef struct
{
    void (*titles_layout)(const GuiMenubarBarCfg *args);
    void (*menus_layout)(const GuiMenubarBarCfg *args);
} GuiMenubarBarNs;
EMBED_TABLE_LAYOUT(GuiMenubarBarNs, titles_layout, menus_layout);
EMBED_TABLE_STORAGE GuiMenubarBarNs gui_menubar_bar EMBED_UNUSED = {
    .titles_layout = gui_menubar_titles_layout,
    .menus_layout = gui_menubar_menus_layout,
};

typedef struct
{
    void (*titles_alloc)(const GuiMenubarFrameCfg *args);
} GuiMenubarFrameNs;
EMBED_TABLE_LAYOUT(GuiMenubarFrameNs, titles_alloc);
EMBED_TABLE_STORAGE GuiMenubarFrameNs gui_menubar_frame EMBED_UNUSED = {
    .titles_alloc = gui_menubar_titles_alloc,
};

typedef struct
{
    int (*holds)(const GuiMenubarXCfg *args);
    unsigned int (*hit)(const GuiMenubarXCfg *args);
} GuiMenubarXNs;
EMBED_TABLE_LAYOUT(GuiMenubarXNs, holds, hit);
EMBED_TABLE_STORAGE GuiMenubarXNs gui_menubar_x EMBED_UNUSED = {
    .holds = gui_menubar_holds,
    .hit = gui_menubar_hit,
};

typedef struct
{
    int (*move)(const GuiMenubarGlyphsFrameXCfg *args);
} GuiMenubarGlyphsFrameXNs;
EMBED_TABLE_LAYOUT(GuiMenubarGlyphsFrameXNs, move);
EMBED_TABLE_STORAGE GuiMenubarGlyphsFrameXNs gui_menubar_glyphs_frame_x EMBED_UNUSED = {
    .move = gui_menubar_move,
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
