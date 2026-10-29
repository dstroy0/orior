// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar.c: the menu bar. The catalog's tree is read as the bar lays out and as a menu opens; nothing of it is copied.
// An item hidden or filled in as the menu opens is left out; an item that needs an open file is drawn but cannot be
// chosen until one is
#include "menubar.h"

#include "../answer.h"
#include "chrome.h"

#include <math.h>

// the most titles the bar shows at a window width in the page's pixels, narrowest last, as the page folds them
typedef struct
{
    int width;
    unsigned int titles;
} GuiTitlesAt;

static const GuiTitlesAt s_titles_at[] = {{1600, GUI_MENUBAR_TITLES}, {1440, 13u}, {1280, 11u}, {1120, 9u}, {0, 7u}};

void gui_menubar_made(GuiMenubar *menubar)
{
    memor.set(&(MemoriaCfg){.dst = menubar, .val = 0, .bytes = sizeof(*menubar)});
    menubar->menus = CLI_JSON_NONE;
    menubar->open = -1;
    menubar->pointed = -1;
}

void gui_menubar_cataloged(GuiMenubar *menubar, const CliJson *json, unsigned int menus)
{
    menubar->json = json;
    menubar->menus = cli_json_is(json, menus, CFG_JSON_ARRAY) ? menus : CLI_JSON_NONE;
    menubar->titles = 0u;
    gui_menubar_closed(menubar);
}

// the string member `name` of `node`, its length into `length`, "" where none
static const char *menubar_string(const CliJson *json, unsigned int node, const char *name, unsigned long long *length)
{
    const char *const text = cli_json_string(json, cli_json_member(json, node, name), length);
    *length = (text != NULL) ? *length : 0ull;
    return (text != NULL) ? text : "";
}

// whether the catalog's menu `menu` has a title on the bar: items of its own, and neither the strip's nor a popup
static int menubar_shown(const CliJson *json, unsigned int menu)
{
    const unsigned int items = cli_json_member(json, menu, "items");
    return cli_json_is(json, items, CFG_JSON_ARRAY) && (json->node[items].count != 0u) &&
           (cli_json_member(json, menu, "strip") == CLI_JSON_NONE) && !gui_answer_member_true(json, menu, "popup");
}

// the menus' count, and the node of the `at`th
static unsigned int menubar_menu_count(const GuiMenubar *menubar)
{
    return (menubar->menus != CLI_JSON_NONE) ? menubar->json->node[menubar->menus].count : 0u;
}

static unsigned int menubar_menu(const GuiMenubar *menubar, unsigned int at)
{
    return cli_json_item(menubar->json, menubar->menus, at);
}

// a title's label: the menu's title, or the folded mark
static const char *menubar_title_text(const GuiMenubar *menubar, const GuiTitle *title, unsigned long long *length)
{
    if (title->folded)
    {
        *length = 3ull;
        return "\xe2\x80\xa6";
    }
    return menubar_string(menubar->json, menubar_menu(menubar, title->menu), "title", length);
}

// whether a row is an item of `items` the menu shows: not the string of a line, not hidden, not filled in as the
// menu opens
static int menubar_item_shown(const CliJson *json, unsigned int item)
{
    return cli_json_is(json, item, CFG_JSON_OBJECT) && !gui_answer_member_true(json, item, "hidden") &&
           (cli_json_member(json, item, "fill") == CLI_JSON_NONE);
}

static int menubar_item_line(const CliJson *json, unsigned int item)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, item, &length);
    return (text != NULL) && (length == 1ull) && (text[0] == '-');
}

// whether the item cannot be chosen: it needs a file open while none is, it is never chosen, or it has no command,
// `command_length` its command's length
static int menubar_item_disabled(const GuiMenubar *menubar, unsigned int item, unsigned long long command_length)
{
    const CliJson *const json = menubar->json;
    unsigned long long needs_length = 0ull;
    const char *const needs = menubar_string(json, item, "needs", &needs_length);
    const int needs_editor = ((needs_length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "editor", .bytes = 6u}) == 0)) ||
                             ((needs_length == 3ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "tab", .bytes = 3u}) == 0));
    const int never = (needs_length == 5ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "never", .bytes = 5u}) == 0);
    return never || (needs_editor && !menubar->has_editor) || (command_length == 0ull);
}

unsigned int gui_menubar_rows(const GuiMenubar *menubar, unsigned int level)
{
    if (level >= menubar->levels)
    {
        return 0u;
    }
    const GuiMenu *const menu = &menubar->menu[level];
    if (menu->folded)
    {
        unsigned int count = 0u;
        for (unsigned int at = menubar->folded_from; at < menubar_menu_count(menubar); at += 1u)
        {
            count += menubar_shown(menubar->json, menubar_menu(menubar, at)) ? 1u : 0u;
        }
        return count;
    }
    const CliJson *const json = menubar->json;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < json->node[menu->items].count; at += 1u)
    {
        const unsigned int item = cli_json_item(json, menu->items, at);
        count += (menubar_item_line(json, item) || menubar_item_shown(json, item)) ? 1u : 0u;
    }
    return count;
}

int gui_menubar_row(const GuiMenubar *menubar, unsigned int level, unsigned int index, GuiRow *row)
{
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    row->label = "";
    row->keys = "";
    row->command = "";
    row->items = CLI_JSON_NONE;
    if (level >= menubar->levels)
    {
        return 0;
    }
    const CliJson *const json = menubar->json;
    const GuiMenu *const menu = &menubar->menu[level];
    unsigned int seen = 0u;
    if (menu->folded)
    {
        for (unsigned int at = menubar->folded_from; at < menubar_menu_count(menubar); at += 1u)
        {
            const unsigned int one = menubar_menu(menubar, at);
            if (!menubar_shown(json, one))
            {
                continue;
            }
            if (seen == index)
            {
                row->kind = GUI_ROW_MORE;
                row->label = menubar_string(json, one, "title", &row->label_length);
                row->items = cli_json_member(json, one, "items");
                return 1;
            }
            seen += 1u;
        }
        return 0;
    }
    for (unsigned int at = 0u; at < json->node[menu->items].count; at += 1u)
    {
        const unsigned int item = cli_json_item(json, menu->items, at);
        const int line = menubar_item_line(json, item);
        if (!line && !menubar_item_shown(json, item))
        {
            continue;
        }
        if (seen != index)
        {
            seen += 1u;
            continue;
        }
        if (line)
        {
            row->kind = GUI_ROW_LINE;
            return 1;
        }
        const unsigned int items = cli_json_member(json, item, "items");
        row->label = menubar_string(json, item, "label", &row->label_length);
        if (cli_json_is(json, items, CFG_JSON_ARRAY))
        {
            row->kind = GUI_ROW_MORE;
            row->items = items;
            return 1;
        }
        row->kind = GUI_ROW_ITEM;
        row->keys = menubar_string(json, item, "keys", &row->keys_length);
        row->command = menubar_string(json, item, "command", &row->command_length);
        row->disabled = menubar_item_disabled(menubar, item, row->command_length);
        return 1;
    }
    return 0;
}

// the modifiers of a key's name read off its front, Ctrl and Mod the same bit, and the key's own name after them
#define MENUBAR_MOD 1u
#define MENUBAR_ALT 2u
#define MENUBAR_SHIFT 4u
static const char *menubar_mods_read(const char *name, unsigned long long length, unsigned int *mods, unsigned long long *rest)
{
    *mods = 0u;
    for (;;)
    {
        if ((length >= 5ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Ctrl+", .bytes = 5u}) == 0))
        {
            *mods |= MENUBAR_MOD;
            name += 5;
            length -= 5ull;
        }
        else if ((length >= 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Mod+", .bytes = 4u}) == 0))
        {
            *mods |= MENUBAR_MOD;
            name += 4;
            length -= 4ull;
        }
        else if ((length >= 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Alt+", .bytes = 4u}) == 0))
        {
            *mods |= MENUBAR_ALT;
            name += 4;
            length -= 4ull;
        }
        else if ((length >= 6ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Shift+", .bytes = 6u}) == 0))
        {
            *mods |= MENUBAR_SHIFT;
            name += 6;
            length -= 6ull;
        }
        else
        {
            *rest = length;
            return name;
        }
    }
}

int gui_menubar_keys_name(const char *keys, unsigned long long keys_length, const char *name)
{
    unsigned int item_mods = 0u;
    unsigned long long item_rest = 0ull;
    const char *const item_key = menubar_mods_read(keys, keys_length, &item_mods, &item_rest);
    unsigned int mods = 0u;
    unsigned long long rest = 0ull;
    const char *const key = menubar_mods_read(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64ull}), &mods, &rest);
    return (item_rest != 0ull) && (item_rest == rest) && (item_mods == mods) &&
           (memor.cmp(&(MemoriaCfg){.src = item_key, .other = key, .bytes = rest}) == 0);
}

// the item among `items` and the items under them whose keys name the key `name` and that can be chosen, into
// `choice`: 1 where one is
static int menubar_items_keyed(const GuiMenubar *menubar, unsigned int items, const char *name, GuiChoice *choice)
{
    const CliJson *const json = menubar->json;
    if (!cli_json_is(json, items, CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < json->node[items].count; at += 1u)
    {
        const unsigned int item = cli_json_item(json, items, at);
        if (!cli_json_is(json, item, CFG_JSON_OBJECT))
        {
            continue;
        }
        if (menubar_items_keyed(menubar, cli_json_member(json, item, "items"), name, choice))
        {
            return 1;
        }
        unsigned long long keys_length = 0ull;
        const char *const keys = menubar_string(json, item, "keys", &keys_length);
        unsigned long long command_length = 0ull;
        const char *const command = menubar_string(json, item, "command", &command_length);
        if (!gui_menubar_keys_name(keys, keys_length, name) || menubar_item_disabled(menubar, item, command_length))
        {
            continue;
        }
        VerbaLinea command_line = {choice->command, sizeof(choice->command), 0ull};
        mmgr_line_put_n(&command_line, command, command_length);
        (void)mmgr_line_finish(&command_line);
        VerbaLinea keys_line = {choice->keys, sizeof(choice->keys), 0ull};
        mmgr_line_put_n(&keys_line, keys, keys_length);
        (void)mmgr_line_finish(&keys_line);
        return 1;
    }
    return 0;
}

int gui_menubar_keys_command(const GuiMenubar *menubar, const char *name, GuiChoice *choice)
{
    choice->command[0] = '\0';
    choice->keys[0] = '\0';
    for (unsigned int at = 0u; at < menubar_menu_count(menubar); at += 1u)
    {
        if (menubar_items_keyed(menubar, cli_json_member(menubar->json, menubar_menu(menubar, at), "items"), name, choice))
        {
            return 1;
        }
    }
    return 0;
}

// the letter of each title Alt takes it by: the first of its letters a to z no title before it has
static void menubar_lettered(GuiMenubar *menubar)
{
    int taken[26] = {0};
    for (unsigned int at = 0u; at < menubar->titles; at += 1u)
    {
        GuiTitle *const title = &menubar->title[at];
        title->letter = GUI_NO_LETTER;
        unsigned long long length = 0ull;
        const char *const text = menubar_title_text(menubar, title, &length);
        for (unsigned long long byte = 0ull; (title->letter == GUI_NO_LETTER) && (byte < length); byte += 1ull)
        {
            const char letter = text[byte];
            const int lower = ((letter >= 'a') && (letter <= 'z')) ? (letter - 'a') : ((letter >= 'A') && (letter <= 'Z')) ? (letter - 'A') : -1;
            if ((lower >= 0) && !taken[lower])
            {
                taken[lower] = 1;
                // the byte is among the label's, which are far fewer than an unsigned int counts
                title->letter = (unsigned int)byte;
            }
        }
    }
}

// a title's width for its text: the text and its padding each side
static int menubar_title_wide(const GuiMenubar *menubar, const GuiTitle *title, GuiGlyphs *glyphs, unsigned int size,
                              float scale)
{
    unsigned long long length = 0ull;
    const char *const text = menubar_title_text(menubar, title, &length);
    return (int)roundf(gui_text_width(glyphs, GUI_FACE_HEAD, size, text, length)) + (2 * gui_scaled(GUI_TITLE_PAD, scale));
}

void gui_menubar_laid(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame)
{
    gui_menubar_closed(menubar);
    menubar->titles = 0u;
    menubar->pointed = -1;
    if ((menubar->json == NULL) || (menubar->menus == CLI_JSON_NONE))
    {
        return;
    }
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_HEAD_SIZE, frame->scale);
    const unsigned int count = menubar_menu_count(menubar);
    unsigned int most = GUI_MENUBAR_TITLES;
    for (unsigned int at = 0u; at < sizeof(s_titles_at) / sizeof(s_titles_at[0]); at += 1u)
    {
        if ((int)frame->width >= gui_scaled(s_titles_at[at].width, frame->scale))
        {
            most = s_titles_at[at].titles;
            break;
        }
    }
    // the room the titles have: from the bar's start to where the tree's name keeps its room
    const int start = frame->bar.x + gui_scaled(GUI_MENUBAR_START, frame->scale);
    const int room_end = frame->controls.x - gui_scaled(GUI_MENUBAR_NAME_ROOM, frame->scale);
    const int margin_x = gui_scaled(GUI_TITLE_MARGIN_X, frame->scale);
    const int margin_y = gui_scaled(GUI_TITLE_MARGIN_Y, frame->scale);
    const int tall = frame->bar.height - (2 * margin_y);
    GuiTitle folded = {0u, 1, {0, 0, 0, 0}, GUI_NO_LETTER};
    const int folded_wide = menubar_title_wide(menubar, &folded, glyphs, size, frame->scale);
    int x = start;
    menubar->folded_from = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!menubar_shown(menubar->json, menubar_menu(menubar, at)))
        {
            continue;
        }
        GuiTitle title = {at, 0, {0, 0, 0, 0}, GUI_NO_LETTER};
        const int wide = menubar_title_wide(menubar, &title, glyphs, size, frame->scale);
        // the title fits where it and the folded title after it would, or it is the last and fits alone
        unsigned int left = 0u;
        for (unsigned int after = at + 1u; after < count; after += 1u)
        {
            left += menubar_shown(menubar->json, menubar_menu(menubar, after)) ? 1u : 0u;
        }
        const int reach = x + margin_x + wide + margin_x + ((left != 0u) ? (margin_x + folded_wide + margin_x) : 0);
        const int fits = (menubar->titles < most) && (reach <= room_end) && (menubar->titles + 1u < GUI_MENUBAR_TITLES);
        if (!fits)
        {
            menubar->folded_from = at;
            break;
        }
        title.rect = (GuiRect){x + margin_x, frame->bar.y + margin_y, wide, tall};
        menubar->title[menubar->titles] = title;
        menubar->titles += 1u;
        x += margin_x + wide + margin_x;
    }
    if (menubar->folded_from < count)
    {
        folded.rect = (GuiRect){x + margin_x, frame->bar.y + margin_y, folded_wide, tall};
        menubar->title[menubar->titles] = folded;
        menubar->titles += 1u;
    }
    menubar_lettered(menubar);
}

// the title at x, y, -1 for none; and the row of the open menu at `level` at x, y, -1 for none
static int menubar_title_at(const GuiMenubar *menubar, int x, int y)
{
    for (unsigned int at = 0u; at < menubar->titles; at += 1u)
    {
        if (gui_rect_holds(menubar->title[at].rect, x, y))
        {
            // the titles are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// a row's height: a line's or an item's
static int menubar_row_tall(const GuiRow *row, float scale)
{
    return gui_scaled((row->kind == GUI_ROW_LINE) ? GUI_MENU_LINE_TALL : GUI_ITEM_TALL, scale);
}

// the rectangle of the row `index` of the open menu at `level`
static GuiRect menubar_row_rect(const GuiMenubar *menubar, unsigned int level, unsigned int index, float scale)
{
    const GuiMenu *const menu = &menubar->menu[level];
    const int pad = gui_scaled(GUI_MENU_PAD, scale);
    int y = menu->rect.y + pad;
    for (unsigned int at = 0u; at < index; at += 1u)
    {
        GuiRow row;
        (void)gui_menubar_row(menubar, level, at, &row);
        y += menubar_row_tall(&row, scale);
    }
    GuiRow own;
    (void)gui_menubar_row(menubar, level, index, &own);
    return (GuiRect){menu->rect.x + pad, y, menu->rect.width - (2 * pad), menubar_row_tall(&own, scale)};
}

static int menubar_row_at(const GuiMenubar *menubar, unsigned int level, int x, int y, float scale)
{
    const unsigned int rows = gui_menubar_rows(menubar, level);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        if (gui_rect_holds(menubar_row_rect(menubar, level, at, scale), x, y))
        {
            // the rows are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

int gui_menubar_holds(const GuiMenubar *menubar, int x, int y)
{
    if (menubar_title_at(menubar, x, y) >= 0)
    {
        return 1;
    }
    for (unsigned int level = 0u; level < menubar->levels; level += 1u)
    {
        if (gui_rect_holds(menubar->menu[level].rect, x, y))
        {
            return 1;
        }
    }
    return 0;
}

// the menu at `level` sized for its rows and placed with its top left at x, y, turned in from the window's edges;
// where it opens beside an item, `beside` that item's rectangle, it goes to the item's other side where the right
// has no room
static void menubar_menu_placed(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, unsigned int level,
                                int x, int y, const GuiRect *beside)
{
    const float scale = frame->scale;
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_HEAD_SIZE, scale);
    const unsigned int keys_size = (unsigned int)gui_scaled((int)GUI_KEYS_SIZE, scale);
    const int pad = gui_scaled(GUI_MENU_PAD, scale);
    const int item_pad = gui_scaled(GUI_ITEM_PAD_X, scale);
    const int gap = gui_scaled(GUI_ITEM_GAP, scale);
    int wide = gui_scaled(GUI_MENU_WIDE_LEAST, scale);
    int tall = 2 * pad;
    const unsigned int rows = gui_menubar_rows(menubar, level);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        GuiRow row;
        (void)gui_menubar_row(menubar, level, at, &row);
        tall += menubar_row_tall(&row, scale);
        if (row.kind == GUI_ROW_LINE)
        {
            continue;
        }
        const float label = gui_text_width(glyphs, GUI_FACE_HEAD, size, row.label, row.label_length);
        const float right = (row.kind == GUI_ROW_MORE) ? (float)gui_scaled(8, scale)
                                                       : gui_text_width(glyphs, GUI_FACE_CODE, keys_size, row.keys, row.keys_length);
        const int needed = (int)roundf(label + (float)gap + right) + (2 * item_pad) + (2 * pad);
        wide = (needed > wide) ? needed : wide;
    }
    const int edge = gui_scaled(GUI_MENU_EDGE, scale);
    // the window's sides are far inside what an int represents
    const int window_wide = (int)frame->width;
    const int window_tall = (int)frame->height;
    int left = x;
    if ((beside != NULL) && (left + wide > window_wide - edge))
    {
        left = beside->x - wide + gui_scaled(GUI_MENU_BESIDE, scale);
    }
    left = (left > window_wide - wide - edge) ? window_wide - wide - edge : left;
    left = (left < edge) ? edge : left;
    tall = (tall > window_tall - (2 * edge)) ? window_tall - (2 * edge) : tall;
    int top = y;
    if (top + tall > window_tall - edge)
    {
        top = (beside != NULL) ? window_tall - edge - tall : y - tall;
    }
    top = (top < edge) ? edge : top;
    menubar->menu[level].rect = (GuiRect){left, top, wide, tall};
}

// the first row of the open menu at `level` from `from` on, stepping `by`, that can be chosen, around the ends: its
// index, or -1 where none can
static int menubar_row_stepped(const GuiMenubar *menubar, unsigned int level, int from, int by)
{
    // the rows are fewer than an int counts
    const int rows = (int)gui_menubar_rows(menubar, level);
    if (rows == 0)
    {
        return -1;
    }
    int at = from;
    for (int step = 0; step < rows; step += 1)
    {
        at = ((at + by) % rows + rows) % rows;
        GuiRow row;
        // the index stepped is among the rows
        if (gui_menubar_row(menubar, level, (unsigned int)at, &row) && (row.kind != GUI_ROW_LINE) && !row.disabled)
        {
            return at;
        }
    }
    return -1;
}

// the menu of the title `at` opened, every other closed, its first row marked where `marked` is 1
static void menubar_title_opened(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, unsigned int at, int marked)
{
    const GuiTitle *const title = &menubar->title[at];
    menubar->levels = 1u;
    // the titles are fewer than an int counts
    menubar->open = (int)at;
    GuiMenu *const menu = &menubar->menu[0];
    menu->folded = title->folded;
    menu->items = title->folded ? CLI_JSON_NONE : cli_json_member(menubar->json, menubar_menu(menubar, title->menu), "items");
    menu->pointed = -1;
    menu->opened = -1;
    menubar_menu_placed(menubar, glyphs, frame, 0u, title->rect.x, frame->bar.y + frame->bar.height + gui_scaled(GUI_MENU_BELOW, frame->scale), NULL);
    menu->pointed = marked ? menubar_row_stepped(menubar, 0u, -1, 1) : -1;
}

// the items of the row `index` of the menu at `level` opened beside it as the next menu, its first row marked where
// `marked` is 1: 1, or 0 where the row has no items or no level is left
static int menubar_row_opened(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, unsigned int level,
                              unsigned int index, int marked)
{
    GuiRow row;
    if (!gui_menubar_row(menubar, level, index, &row) || (row.kind != GUI_ROW_MORE) || (level + 1u >= GUI_MENU_LEVELS))
    {
        return 0;
    }
    menubar->levels = level + 2u;
    // the index is among the rows, which an int counts
    menubar->menu[level].opened = (int)index;
    GuiMenu *const under = &menubar->menu[level + 1u];
    under->folded = 0;
    under->items = row.items;
    under->pointed = -1;
    under->opened = -1;
    const GuiRect beside = menubar_row_rect(menubar, level, index, frame->scale);
    menubar_menu_placed(menubar, glyphs, frame, level + 1u, beside.x + beside.width - gui_scaled(GUI_MENU_BESIDE, frame->scale),
                        beside.y - gui_scaled(GUI_MENU_ABOVE, frame->scale), &beside);
    under->pointed = marked ? menubar_row_stepped(menubar, level + 1u, -1, 1) : -1;
    return 1;
}

// the menus from `level` on closed
static void menubar_closed_from(GuiMenubar *menubar, unsigned int level)
{
    if (level < menubar->levels)
    {
        menubar->levels = level;
        if (level != 0u)
        {
            menubar->menu[level - 1u].opened = -1;
        }
    }
    if (level == 0u)
    {
        menubar->open = -1;
    }
}

void gui_menubar_closed(GuiMenubar *menubar)
{
    menubar_closed_from(menubar, 0u);
}

// the row `index` of the menu at `level` chosen into `choice`
static void menubar_chosen(GuiMenubar *menubar, unsigned int level, unsigned int index, GuiChoice *choice)
{
    GuiRow row;
    (void)gui_menubar_row(menubar, level, index, &row);
    VerbaLinea command = {choice->command, sizeof(choice->command), 0ull};
    mmgr_line_put_n(&command, row.command, row.command_length);
    (void)mmgr_line_finish(&command);
    VerbaLinea keys = {choice->keys, sizeof(choice->keys), 0ull};
    mmgr_line_put_n(&keys, row.keys, row.keys_length);
    (void)mmgr_line_finish(&keys);
    gui_menubar_closed(menubar);
}

int gui_menubar_pressed(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, int x, int y, GuiChoice *choice)
{
    choice->command[0] = '\0';
    choice->keys[0] = '\0';
    const int title = menubar_title_at(menubar, x, y);
    if (title >= 0)
    {
        if (menubar->open == title)
        {
            gui_menubar_closed(menubar);
        }
        else
        {
            // the title is among the titles
            menubar_title_opened(menubar, glyphs, frame, (unsigned int)title, 1);
        }
        return 1;
    }
    for (unsigned int level = menubar->levels; level > 0u; level -= 1u)
    {
        if (!gui_rect_holds(menubar->menu[level - 1u].rect, x, y))
        {
            continue;
        }
        const int index = menubar_row_at(menubar, level - 1u, x, y, frame->scale);
        GuiRow row;
        // the index found is among the rows
        if ((index < 0) || !gui_menubar_row(menubar, level - 1u, (unsigned int)index, &row) || (row.kind == GUI_ROW_LINE) || row.disabled)
        {
            return 1;
        }
        if (row.kind == GUI_ROW_MORE)
        {
            menubar->menu[level - 1u].pointed = index;
            (void)menubar_row_opened(menubar, glyphs, frame, level - 1u, (unsigned int)index, 1);
        }
        else
        {
            menubar_chosen(menubar, level - 1u, (unsigned int)index, choice);
        }
        return 1;
    }
    if (menubar->levels != 0u)
    {
        gui_menubar_closed(menubar);
    }
    return 0;
}

int gui_menubar_moved(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, int x, int y)
{
    int changed = 0;
    const int title = menubar_title_at(menubar, x, y);
    if (title != menubar->pointed)
    {
        menubar->pointed = title;
        changed = 1;
    }
    if ((title >= 0) && (menubar->open >= 0) && (title != menubar->open))
    {
        // the title is among the titles
        menubar_title_opened(menubar, glyphs, frame, (unsigned int)title, 0);
        return 1;
    }
    for (unsigned int level = menubar->levels; level > 0u; level -= 1u)
    {
        GuiMenu *const menu = &menubar->menu[level - 1u];
        if (!gui_rect_holds(menu->rect, x, y))
        {
            continue;
        }
        const int index = menubar_row_at(menubar, level - 1u, x, y, frame->scale);
        GuiRow row;
        // the index found is among the rows
        if ((index < 0) || !gui_menubar_row(menubar, level - 1u, (unsigned int)index, &row) || (row.kind == GUI_ROW_LINE) || row.disabled)
        {
            return changed;
        }
        if (menu->pointed == index)
        {
            return changed;
        }
        menu->pointed = index;
        if (row.kind == GUI_ROW_MORE)
        {
            (void)menubar_row_opened(menubar, glyphs, frame, level - 1u, (unsigned int)index, 0);
        }
        else
        {
            menubar_closed_from(menubar, level);
        }
        return 1;
    }
    return changed;
}

// a key's name read: whether Alt is held and the key's own name, the other modifiers left on it
static const char *menubar_key_read(const char *name, int *alt, int *modded)
{
    *alt = 0;
    *modded = 0;
    for (;;)
    {
        if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Alt+", .bytes = 4u}) == 0)
        {
            *alt = 1;
            name += 4;
        }
        else if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Mod+", .bytes = 4u}) == 0)
        {
            *modded = 1;
            name += 4;
        }
        else if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Shift+", .bytes = 6u}) == 0)
        {
            *modded = 1;
            name += 6;
        }
        else
        {
            return name;
        }
    }
}

static int menubar_named(const char *name, const char *wanted)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = wanted, .cap = 32ull});
}

// the title whose letter is the single letter `name`, -1 for none
static int menubar_title_lettered(const GuiMenubar *menubar, const char *name)
{
    if ((name[0] == '\0') || (name[1] != '\0'))
    {
        return -1;
    }
    const char wanted = ((name[0] >= 'A') && (name[0] <= 'Z')) ? (char)(name[0] - 'A' + 'a') : name[0];
    for (unsigned int at = 0u; at < menubar->titles; at += 1u)
    {
        const GuiTitle *const title = &menubar->title[at];
        unsigned long long length = 0ull;
        const char *const text = menubar_title_text(menubar, title, &length);
        if (title->letter < length)
        {
            const char letter = text[title->letter];
            const char lower = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
            if (lower == wanted)
            {
                // the titles are fewer than an int counts
                return (int)at;
            }
        }
    }
    return -1;
}

int gui_menubar_keyed(GuiMenubar *menubar, GuiGlyphs *glyphs, const GuiFrame *frame, const char *given, GuiChoice *choice)
{
    choice->command[0] = '\0';
    choice->keys[0] = '\0';
    int alt = 0;
    int modded = 0;
    const char *const name = menubar_key_read(given, &alt, &modded);
    if (alt && !modded)
    {
        const int title = menubar_title_lettered(menubar, name);
        if (title >= 0)
        {
            // the title is among the titles
            menubar_title_opened(menubar, glyphs, frame, (unsigned int)title, 1);
            return 1;
        }
        return 0;
    }
    if ((menubar->levels == 0u) || alt || modded)
    {
        return 0;
    }
    const unsigned int level = menubar->levels - 1u;
    GuiMenu *const menu = &menubar->menu[level];
    if (menubar_named(name, "Escape"))
    {
        menubar_closed_from(menubar, level);
        return 1;
    }
    if (menubar_named(name, "Down") || menubar_named(name, "Up"))
    {
        menu->pointed = menubar_row_stepped(menubar, level, menu->pointed, menubar_named(name, "Down") ? 1 : -1);
        return 1;
    }
    if (menubar_named(name, "Home"))
    {
        menu->pointed = menubar_row_stepped(menubar, level, -1, 1);
        return 1;
    }
    if (menubar_named(name, "End"))
    {
        // the rows are fewer than an int counts
        menu->pointed = menubar_row_stepped(menubar, level, (int)gui_menubar_rows(menubar, level), -1);
        return 1;
    }
    GuiRow row;
    const int pointed = (menu->pointed >= 0) && gui_menubar_row(menubar, level, (unsigned int)menu->pointed, &row);
    if (menubar_named(name, "Right"))
    {
        if (pointed && (row.kind == GUI_ROW_MORE))
        {
            (void)menubar_row_opened(menubar, glyphs, frame, level, (unsigned int)menu->pointed, 1);
        }
        else if ((level == 0u) && (menubar->titles != 0u))
        {
            // the title open is among the titles
            menubar_title_opened(menubar, glyphs, frame, ((unsigned int)menubar->open + 1u) % menubar->titles, 1);
        }
        return 1;
    }
    if (menubar_named(name, "Left"))
    {
        if (level > 0u)
        {
            menubar_closed_from(menubar, level);
        }
        else if (menubar->titles != 0u)
        {
            menubar_title_opened(menubar, glyphs, frame, ((unsigned int)menubar->open + menubar->titles - 1u) % menubar->titles, 1);
        }
        return 1;
    }
    if (menubar_named(name, "Enter") || menubar_named(name, "Space"))
    {
        if (pointed && (row.kind == GUI_ROW_MORE))
        {
            (void)menubar_row_opened(menubar, glyphs, frame, level, (unsigned int)menu->pointed, 1);
        }
        else if (pointed && !row.disabled)
        {
            menubar_chosen(menubar, level, (unsigned int)menu->pointed, choice);
        }
        return 1;
    }
    if ((name[0] != '\0') && (name[1] == '\0'))
    {
        // a letter marks the next row it begins, around the end
        const char wanted = ((name[0] >= 'A') && (name[0] <= 'Z')) ? (char)(name[0] - 'A' + 'a') : name[0];
        const int rows = (int)gui_menubar_rows(menubar, level);
        for (int step = 1; step <= rows; step += 1)
        {
            const int at = ((menu->pointed + step) % rows + rows) % rows;
            GuiRow one;
            if (gui_menubar_row(menubar, level, (unsigned int)at, &one) && (one.kind != GUI_ROW_LINE) && !one.disabled && (one.label_length != 0ull))
            {
                const char first = one.label[0];
                const char lower = ((first >= 'A') && (first <= 'Z')) ? (char)(first - 'A' + 'a') : first;
                if (lower == wanted)
                {
                    menu->pointed = at;
                    break;
                }
            }
        }
        return 1;
    }
    return 1;
}

void gui_menubar_titles_drawn(const GuiMenubar *menubar, GuiRaster *raster, GuiGlyphs *glyphs,
                              const GuiPalette *palette, const GuiFrame *frame)
{
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_HEAD_SIZE, frame->scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD, size);
    for (unsigned int at = 0u; at < menubar->titles; at += 1u)
    {
        const GuiTitle *const title = &menubar->title[at];
        // the titles are fewer than an int counts
        const int lit = ((int)at == menubar->open) || ((int)at == menubar->pointed);
        if (lit)
        {
            gui_raster_rounded(raster, gui_rect_met(title->rect, frame->bar), (unsigned int)gui_scaled(GUI_TITLE_RADIUS, frame->scale),
                               palette->bar_title_hover);
        }
        unsigned long long length = 0ull;
        const char *const text = menubar_title_text(menubar, title, &length);
        const int baseline = title->rect.y + (int)((((float)title->rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(title->rect.x + gui_scaled(GUI_TITLE_PAD, frame->scale)),
                             baseline, text, length, lit ? palette->bar_bright : palette->bar_title, frame->bar);
    }
}

// a box's outline drawn, `thick` pixels wide inside it
static void menubar_box_drawn(GuiRaster *raster, GuiRect rect, int thick, GuiColor color)
{
    gui_raster_filled(raster, (GuiRect){rect.x, rect.y, rect.width, thick}, color);
    gui_raster_filled(raster, (GuiRect){rect.x, rect.y + rect.height - thick, rect.width, thick}, color);
    gui_raster_filled(raster, (GuiRect){rect.x, rect.y, thick, rect.height}, color);
    gui_raster_filled(raster, (GuiRect){rect.x + rect.width - thick, rect.y, thick, rect.height}, color);
}

void gui_menubar_menus_drawn(const GuiMenubar *menubar, GuiRaster *raster, GuiGlyphs *glyphs,
                             const GuiPalette *palette, const GuiFrame *frame)
{
    const float scale = frame->scale;
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_HEAD_SIZE, scale);
    const unsigned int keys_size = (unsigned int)gui_scaled((int)GUI_KEYS_SIZE, scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD, size);
    const int item_pad = gui_scaled(GUI_ITEM_PAD_X, scale);
    for (unsigned int level = 0u; level < menubar->levels; level += 1u)
    {
        const GuiMenu *const menu = &menubar->menu[level];
        gui_raster_rounded(raster, menu->rect, (unsigned int)gui_scaled(GUI_MENU_RADIUS, scale), palette->surface);
        menubar_box_drawn(raster, menu->rect, gui_scaled(1, scale), palette->widget_line);
        const unsigned int rows = gui_menubar_rows(menubar, level);
        for (unsigned int at = 0u; at < rows; at += 1u)
        {
            GuiRow row;
            (void)gui_menubar_row(menubar, level, at, &row);
            const GuiRect rect = menubar_row_rect(menubar, level, at, scale);
            if (row.kind == GUI_ROW_LINE)
            {
                const int inset = gui_scaled(6, scale);
                gui_raster_filled(raster, (GuiRect){rect.x + inset, rect.y + (rect.height / 2), rect.width - (2 * inset), gui_scaled(1, scale)},
                                  palette->line);
                continue;
            }
            // the rows are fewer than an int counts
            if ((int)at == menu->pointed)
            {
                gui_raster_rounded(raster, rect, (unsigned int)gui_scaled(4, scale), palette->selection);
            }
            else if ((int)at == menu->opened)
            {
                gui_raster_rounded(raster, rect, (unsigned int)gui_scaled(4, scale), palette->selection_idle);
            }
            const int baseline = rect.y + (int)((((float)rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
            const GuiColor color = row.disabled ? palette->text_3 : palette->text;
            (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(rect.x + item_pad), baseline, row.label, row.label_length,
                                 color, rect);
            if (row.kind == GUI_ROW_MORE)
            {
                // a chevron pointing to where the items open
                const int arm = gui_scaled(3, scale);
                const int tip_x = rect.x + rect.width - item_pad;
                const int middle = rect.y + (rect.height / 2);
                gui_raster_line(raster, tip_x - arm, middle - arm, tip_x, middle, color);
                gui_raster_line(raster, tip_x - arm, middle + arm, tip_x, middle, color);
            }
            else if (row.keys_length != 0ull)
            {
                const float keys_wide = gui_text_width(glyphs, GUI_FACE_CODE, keys_size, row.keys, row.keys_length);
                (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, keys_size, (float)(rect.x + rect.width - item_pad) - keys_wide,
                                     baseline, row.keys, row.keys_length, palette->text_3, rect);
            }
        }
    }
}
