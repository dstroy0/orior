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

void gui_menubar_init(const GuiMenubarGlyphsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->menubar, .val = 0, .bytes = sizeof(*args->menubar)});
    args->menubar->menus = CLI_JSON_NONE;
    args->menubar->open = -1;
    args->menubar->pointed = -1;
}

void gui_menubar_catalog(const GuiMenubarJsonCfg *args)
{
    const CliJson *json = args->json;
    unsigned int menus = args->menus;
    args->menubar->json = json;
    args->menubar->menus = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                      .node = menus, .kind = CFG_JSON_ARRAY)
                               ? menus
                               : CLI_JSON_NONE;
    args->menubar->titles = 0u;
    gui_menubar_json.close(args);
}

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *name;
    unsigned long long *length;
    unsigned int menu;
    const GuiMenubar *menubar;
    unsigned int at;
    const GuiTitle *title;
    unsigned int item;
    unsigned long long command_length;
    unsigned long long length_value;
    unsigned int *mods;
    unsigned long long *rest;
    unsigned int items;
    GuiChoice *choice;
    GuiMenubar *menubar_out;
    GuiGlyphs *glyphs;
    unsigned int size;
    int scale;
    const GuiFrame *frame;
    int x;
    int y;
    const GuiRow *row;
    unsigned int level;
    unsigned int index;
    const GuiRect *beside;
    int from;
    int by;
    int marked;
    int *alt;
    int *modded;
    const char *wanted;
    GuiRaster *raster;
    GuiRect rect;
    int thick;
    GuiColor color;
} MenubarCtx;
// the string member `name` of `node`, its length into `length`, "" where none
EMBED_INLINE const char *menubar_string(const MenubarCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    unsigned long long *length = args->length;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = node, .name = name),
                   .length_pointer = length);
    *length = (text != NULL) ? *length : 0ull;
    return (text != NULL) ? text : "";
}

// whether the catalog's menu `menu` has a title on the bar: items of its own, and neither the strip's nor a popup
EMBED_INLINE int menubar_show(const MenubarCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->menu, .name = "items");
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = items,
                      .kind = CFG_JSON_ARRAY) &&
           (json->node[items].count != 0u) &&
           (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->menu,
                       .name = "strip") == CLI_JSON_NONE) &&
           !EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->menu,
                       .name = "popup");
}

// the menus' count, and the node of the `at`th
EMBED_INLINE unsigned int menubar_menu_count(const MenubarCtx *args)
{
    return (args->menubar->menus != CLI_JSON_NONE) ? args->menubar->json->node[args->menubar->menus].count : 0u;
}

EMBED_INLINE unsigned int menubar_menu(const MenubarCtx *args)
{
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->menubar->json,
                      .array = args->menubar->menus, .index = args->at);
}

// a title's label: the menu's title, or the folded mark
EMBED_INLINE const char *menubar_title_text(const MenubarCtx *args)
{
    unsigned long long *length = args->length;
    if (args->title->folded)
    {
        *length = 3ull;
        return "\xe2\x80\xa6";
    }
    return EMBED_CALL(menubar_string, MenubarCtx, .json = args->menubar->json,
                      .node = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar, .at = args->title->menu),
                      .name = "title", .length = length);
}

// whether a row is an item of `items` the menu shows: not the string of a line, not hidden, not filled in as the
// menu opens
EMBED_INLINE int menubar_item_show(const MenubarCtx *args)
{
    const CliJson *json = args->json;
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->item,
                      .kind = CFG_JSON_OBJECT) &&
           !EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->item,
                       .name = "hidden") &&
           (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->item,
                       .name = "fill") == CLI_JSON_NONE);
}

EMBED_INLINE int menubar_item_line(const MenubarCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = args->item, .length_pointer = &length);
    return (text != NULL) && (length == 1ull) && (text[0] == '-');
}

// whether the item cannot be chosen: it needs a file open while none is, it is never chosen, or it has no command,
// `command_length` its command's length
EMBED_INLINE int menubar_item_disable(const MenubarCtx *args)
{
    const CliJson *const json = args->menubar->json;
    unsigned long long needs_length = 0ull;
    const char *const needs = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = args->item, .name = "needs",
                                         .length = &needs_length);
    const int needs_editor =
        ((needs_length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "editor", .bytes = 6u}) == 0)) ||
        ((needs_length == 3ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "tab", .bytes = 3u}) == 0));
    const int never =
        (needs_length == 5ull) && (memor.cmp(&(MemoriaCfg){.src = needs, .other = "never", .bytes = 5u}) == 0);
    return never || (needs_editor && !args->menubar->has_editor) || (args->command_length == 0ull);
}

unsigned int gui_menubar_rows(const GuiMenubarLevelCfg *args)
{
    if (args->level >= args->menubar_read->levels)
    {
        return 0u;
    }
    const GuiMenu *const menu = &args->menubar_read->menu[args->level];
    if (menu->folded)
    {
        unsigned int count = 0u;
        for (unsigned int at = args->menubar_read->folded_from;
             at < EMBED_CALL(menubar_menu_count, MenubarCtx, .menubar = args->menubar_read); at += 1u)
        {
            count += EMBED_CALL(menubar_show, MenubarCtx, .json = args->menubar_read->json,
                                .menu = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar_read, .at = at))
                         ? 1u
                         : 0u;
        }
        return count;
    }
    const CliJson *const json = args->menubar_read->json;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < json->node[menu->items].count; at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = menu->items, .index = at);
        count += (EMBED_CALL(menubar_item_line, MenubarCtx, .json = json, .item = item) ||
                  EMBED_CALL(menubar_item_show, MenubarCtx, .json = json, .item = item))
                     ? 1u
                     : 0u;
    }
    return count;
}

int gui_menubar_row(const GuiMenubarLevelCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->row, .val = 0, .bytes = sizeof(*args->row)});
    args->row->label = "";
    args->row->keys = "";
    args->row->command = "";
    args->row->items = CLI_JSON_NONE;
    if (args->level >= args->menubar_read->levels)
    {
        return 0;
    }
    const CliJson *const json = args->menubar_read->json;
    const GuiMenu *const menu = &args->menubar_read->menu[args->level];
    unsigned int seen = 0u;
    if (menu->folded)
    {
        for (unsigned int at = args->menubar_read->folded_from;
             at < EMBED_CALL(menubar_menu_count, MenubarCtx, .menubar = args->menubar_read); at += 1u)
        {
            const unsigned int one = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar_read, .at = at);
            if (!EMBED_CALL(menubar_show, MenubarCtx, .json = json, .menu = one))
            {
                continue;
            }
            if (seen == args->index)
            {
                args->row->kind = GUI_ROW_MORE;
                args->row->label = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = one, .name = "title",
                                              .length = &args->row->label_length);
                args->row->items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = one, .name = "items");
                return 1;
            }
            seen += 1u;
        }
        return 0;
    }
    for (unsigned int at = 0u; at < json->node[menu->items].count; at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = menu->items, .index = at);
        const int line = EMBED_CALL(menubar_item_line, MenubarCtx, .json = json, .item = item);
        if (!line && !EMBED_CALL(menubar_item_show, MenubarCtx, .json = json, .item = item))
        {
            continue;
        }
        if (seen != args->index)
        {
            seen += 1u;
            continue;
        }
        if (line)
        {
            args->row->kind = GUI_ROW_LINE;
            return 1;
        }
        const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = item, .name = "items");
        args->row->label = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = item, .name = "label",
                                      .length = &args->row->label_length);
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = items,
                       .kind = CFG_JSON_ARRAY))
        {
            args->row->kind = GUI_ROW_MORE;
            args->row->items = items;
            return 1;
        }
        args->row->kind = GUI_ROW_ITEM;
        args->row->keys = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = item, .name = "keys",
                                     .length = &args->row->keys_length);
        args->row->command = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = item, .name = "command",
                                        .length = &args->row->command_length);
        args->row->disabled = EMBED_CALL(menubar_item_disable, MenubarCtx, .menubar = args->menubar_read, .item = item,
                                         .command_length = args->row->command_length);
        return 1;
    }
    return 0;
}

// the modifiers of a key's name read off its front, Ctrl and Mod the same bit, and the key's own name after them
#define MENUBAR_MOD 1u
#define MENUBAR_ALT 2u
#define MENUBAR_SHIFT 4u
EMBED_INLINE const char *menubar_mods_read(const MenubarCtx *args)
{
    const char *name = args->name;
    unsigned long long length = args->length_value;
    unsigned int *mods = args->mods;
    unsigned long long *rest = args->rest;
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

int gui_menubar_keys_name(const GuiMenubarNameCfg *args)
{
    unsigned int item_mods = 0u;
    unsigned long long item_rest = 0ull;
    const char *const item_key = EMBED_CALL(menubar_mods_read, MenubarCtx, .name = args->keys,
                                            .length_value = args->keys_length, .mods = &item_mods, .rest = &item_rest);
    unsigned int mods = 0u;
    unsigned long long rest = 0ull;
    const char *const key = EMBED_CALL(menubar_mods_read, MenubarCtx, .name = args->name,
                                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = 64ull}),
                                       .mods = &mods, .rest = &rest);
    return (item_rest != 0ull) && (item_rest == rest) && (item_mods == mods) &&
           (memor.cmp(&(MemoriaCfg){.src = item_key, .other = key, .bytes = rest}) == 0);
}

// the item among `items` and the items under them whose keys name the key `name` and that can be chosen, into
// `choice`: 1 where one is
static int menubar_items_key(const MenubarCtx *args)
{
    const char *name = args->name;
    const CliJson *const json = args->menubar->json;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->items,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < json->node[args->items].count; at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = args->items, .index = at);
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = item,
                        .kind = CFG_JSON_OBJECT))
        {
            continue;
        }
        if (EMBED_CALL(menubar_items_key, MenubarCtx, .menubar = args->menubar,
                       .items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = item, .name = "items"),
                       .name = name, .choice = args->choice))
        {
            return 1;
        }
        unsigned long long keys_length = 0ull;
        const char *const keys =
            EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = item, .name = "keys", .length = &keys_length);
        unsigned long long command_length = 0ull;
        const char *const command = EMBED_CALL(menubar_string, MenubarCtx, .json = json, .node = item,
                                               .name = "command", .length = &command_length);
        if (!EMBED_CALL(gui_menubar_name.keys_name, GuiMenubarNameCfg, .keys = keys, .keys_length = keys_length,
                        .name = name) ||
            EMBED_CALL(menubar_item_disable, MenubarCtx, .menubar = args->menubar, .item = item,
                       .command_length = command_length))
        {
            continue;
        }
        VerbaLinea command_line = {args->choice->command, sizeof(args->choice->command), 0ull};
        mmgr_line_put_n(&command_line, command, command_length);
        (void)mmgr_line_finish(&command_line);
        VerbaLinea keys_line = {args->choice->keys, sizeof(args->choice->keys), 0ull};
        mmgr_line_put_n(&keys_line, keys, keys_length);
        (void)mmgr_line_finish(&keys_line);
        return 1;
    }
    return 0;
}

int gui_menubar_keys_command(const GuiMenubarChoiceCfg *args)
{
    args->choice->command[0] = '\0';
    args->choice->keys[0] = '\0';
    for (unsigned int at = 0u; at < EMBED_CALL(menubar_menu_count, MenubarCtx, .menubar = args->menubar_read); at += 1u)
    {
        if (EMBED_CALL(menubar_items_key, MenubarCtx, .menubar = args->menubar_read,
                       .items = EMBED_CALL(
                           cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                           .json_read = args->menubar_read->json,
                           .object = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar_read, .at = at),
                           .name = "items"),
                       .name = args->name, .choice = args->choice))
        {
            return 1;
        }
    }
    return 0;
}

// the letter of each title Alt takes it by: the first of its letters a to z no title before it has
EMBED_INLINE void menubar_letter(const MenubarCtx *args)
{
    int taken[26] = {0};
    for (unsigned int at = 0u; at < args->menubar_out->titles; at += 1u)
    {
        GuiTitle *const title = &args->menubar_out->title[at];
        title->letter = GUI_NO_LETTER;
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar_out, .title = title, .length = &length);
        for (unsigned long long byte = 0ull; (title->letter == GUI_NO_LETTER) && (byte < length); byte += 1ull)
        {
            const char letter = text[byte];
            const int lower = ((letter >= 'a') && (letter <= 'z'))   ? (letter - 'a')
                              : ((letter >= 'A') && (letter <= 'Z')) ? (letter - 'A')
                                                                     : -1;
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
EMBED_INLINE int menubar_title_wide(const MenubarCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int size = args->size;
    int scale = args->scale;
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar, .title = args->title, .length = &length);
    return (int)roundf(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                  .face = GUI_FACE_HEAD, .size = size, .text = text, .length = length)) +
           (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_PAD, .scale = scale));
}

// the menus open moved together to follow their title as the bar moves: the first under its title, turned in from the
// window's edges as it was when it opened, and the ones beside it by as much
EMBED_INLINE void menubar_menus_follow(const MenubarCtx *args)
{
    GuiMenubar *menubar = args->menubar_out;
    if ((menubar->levels == 0u) || (menubar->open < 0))
    {
        return;
    }
    // the open title is not negative past the test; a menu whose title the bar no longer shows closes
    if ((unsigned int)menubar->open >= menubar->titles)
    {
        EMBED_CALL(gui_menubar_json.close, GuiMenubarJsonCfg, .menubar = menubar);
        return;
    }
    const GuiRect first = menubar->menu[0].rect;
    const int edge =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_EDGE, .scale = args->frame->scale);
    // the window's sides are far inside what an int represents
    const int most_left = (int)args->frame->width - first.width - edge;
    const int wanted_left = menubar->title[menubar->open].rect.x;
    const int left_inside = (wanted_left > most_left) ? most_left : wanted_left;
    const int left = (left_inside < edge) ? edge : left_inside;
    const int wanted_top =
        args->frame->bar.y + args->frame->bar.height +
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_BELOW, .scale = args->frame->scale);
    const int top = (wanted_top < edge) ? edge : wanted_top;
    for (unsigned int level = 0u; level < menubar->levels; level += 1u)
    {
        menubar->menu[level].rect.x += left - first.x;
        menubar->menu[level].rect.y += top - first.y;
    }
}

void gui_menubar_titles_take(const GuiMenubarFrameCfg *args)
{
    unsigned int folded_from = args->folded_from;
    memor.cpy(&(MemoriaCfg){
        .dst = args->menubar->title, .src = args->title, .bytes = (unsigned long long)args->count * sizeof(GuiTitle)});
    args->menubar->titles = args->count;
    args->menubar->folded_from = folded_from;
    EMBED_CALL(menubar_menus_follow, MenubarCtx, .menubar_out = args->menubar, .frame = args->frame);
}

void gui_menubar_layout(const GuiMenubarJsonCfg *args)
{
    gui_menubar_json.close(args);
    gui_menubar_json.titles_layout(args);
}

void gui_menubar_titles_layout(const GuiMenubarJsonCfg *args)
{
    args->menubar->titles = 0u;
    // the bar moves under a pointer standing still, which marks no title until it moves, while a menu open saves its
    // own
    if (args->menubar->levels == 0u)
    {
        args->menubar->pointed = -1;
    }
    if ((args->menubar->json == NULL) || (args->menubar->menus == CLI_JSON_NONE))
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const unsigned int count = EMBED_CALL(menubar_menu_count, MenubarCtx, .menubar = args->menubar);
    unsigned int most = GUI_MENUBAR_TITLES;
    for (unsigned int at = 0u; at < sizeof(s_titles_at) / sizeof(s_titles_at[0]); at += 1u)
    {
        if ((int)args->frame->width >= EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                  .pixels = s_titles_at[at].width, .scale = args->frame->scale))
        {
            most = s_titles_at[at].titles;
            break;
        }
    }
    // the room the titles have: from the bar's start to where the tree's name saves its room
    // and the eye's room before it, given up as the claim runs
    const long long eye_room = ((long long)(GUI_WHOLE - args->frame->claim.claim) *
                                (long long)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                      .pixels = GUI_MENUBAR_EYE_ROOM, .scale = args->frame->scale)) /
                               GUI_WHOLE;
    // the room is less than the window's width: it is far inside what an int represents
    const int start = args->frame->bar.x +
                      EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENUBAR_START,
                                 .scale = args->frame->scale) +
                      (int)eye_room;
    const int room_end =
        args->frame->controls.x - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENUBAR_NAME_ROOM,
                                             .scale = args->frame->scale);
    const int margin_x = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_MARGIN_X,
                                    .scale = args->frame->scale);
    const int margin_y = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_MARGIN_Y,
                                    .scale = args->frame->scale);
    const int tall = args->frame->bar.height - (2 * margin_y);
    GuiTitle folded = {0u, 1, {0, 0, 0, 0}, GUI_NO_LETTER};
    const int folded_wide = EMBED_CALL(menubar_title_wide, MenubarCtx, .menubar = args->menubar, .title = &folded,
                                       .glyphs = args->glyphs, .size = size, .scale = args->frame->scale);
    int x = start;
    args->menubar->folded_from = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!EMBED_CALL(menubar_show, MenubarCtx, .json = args->menubar->json,
                        .menu = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar, .at = at)))
        {
            continue;
        }
        GuiTitle title = {at, 0, {0, 0, 0, 0}, GUI_NO_LETTER};
        const int wide = EMBED_CALL(menubar_title_wide, MenubarCtx, .menubar = args->menubar, .title = &title,
                                    .glyphs = args->glyphs, .size = size, .scale = args->frame->scale);
        // the title fits where it and the folded title after it would, or it is the last and fits alone
        unsigned int left = 0u;
        for (unsigned int after = at + 1u; after < count; after += 1u)
        {
            left += EMBED_CALL(menubar_show, MenubarCtx, .json = args->menubar->json,
                               .menu = EMBED_CALL(menubar_menu, MenubarCtx, .menubar = args->menubar, .at = after))
                        ? 1u
                        : 0u;
        }
        const int reach = x + margin_x + wide + margin_x + ((left != 0u) ? (margin_x + folded_wide + margin_x) : 0);
        const int fits =
            (args->menubar->titles < most) && (reach <= room_end) && (args->menubar->titles + 1u < GUI_MENUBAR_TITLES);
        if (!fits)
        {
            args->menubar->folded_from = at;
            break;
        }
        title.rect = (GuiRect){x + margin_x, args->frame->bar.y + margin_y, wide, tall};
        args->menubar->title[args->menubar->titles] = title;
        args->menubar->titles += 1u;
        x += margin_x + wide + margin_x;
    }
    if (args->menubar->folded_from < count)
    {
        folded.rect = (GuiRect){x + margin_x, args->frame->bar.y + margin_y, folded_wide, tall};
        args->menubar->title[args->menubar->titles] = folded;
        args->menubar->titles += 1u;
    }
    EMBED_CALL(menubar_letter, MenubarCtx, .menubar_out = args->menubar);
    EMBED_CALL(menubar_menus_follow, MenubarCtx, .menubar_out = args->menubar, .frame = args->frame);
}

// the title at x, y, -1 for none; and the row of the open menu at `level` at x, y, -1 for none
EMBED_INLINE int menubar_title_at(const MenubarCtx *args)
{
    int x = args->x;
    int y = args->y;
    for (unsigned int at = 0u; at < args->menubar->titles; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->menubar->title[at].rect, .x = x,
                       .y = y))
        {
            // the titles are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// a row's height: a line's or an item's
EMBED_INLINE int menubar_row_tall(const MenubarCtx *args)
{
    int scale = args->scale;
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = (args->row->kind == GUI_ROW_LINE) ? GUI_MENU_LINE_TALL : GUI_ITEM_TALL, .scale = scale);
}

// the rectangle of the row `index` of the open menu at `level`
EMBED_INLINE GuiRect menubar_row_rect(const MenubarCtx *args)
{
    const GuiMenubar *menubar = args->menubar;
    unsigned int level = args->level;
    unsigned int index = args->index;
    int scale = args->scale;
    const GuiMenu *const menu = &menubar->menu[level];
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_PAD, .scale = scale);
    int y = menu->rect.y + pad;
    for (unsigned int at = 0u; at < index; at += 1u)
    {
        GuiRow row;
        (void)EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level,
                         .index = at, .row = &row);
        y += EMBED_CALL(menubar_row_tall, MenubarCtx, .row = &row, .scale = scale);
    }
    GuiRow own;
    (void)EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level, .index = index,
                     .row = &own);
    return (GuiRect){menu->rect.x + pad, y, menu->rect.width - (2 * pad),
                     EMBED_CALL(menubar_row_tall, MenubarCtx, .row = &own, .scale = scale)};
}

EMBED_INLINE int menubar_row_at(const MenubarCtx *args)
{
    const GuiMenubar *menubar = args->menubar;
    unsigned int level = args->level;
    int x = args->x;
    int y = args->y;
    const unsigned int rows =
        EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = menubar, .level = level, .index = at,
                                          .scale = args->scale),
                       .x = x, .y = y))
        {
            // the rows are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

int gui_menubar_holds(const GuiMenubarXCfg *args)
{
    if (EMBED_CALL(menubar_title_at, MenubarCtx, .menubar = args->menubar_read, .x = args->x, .y = args->y) >= 0)
    {
        return 1;
    }
    for (unsigned int level = 0u; level < args->menubar_read->levels; level += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->menubar_read->menu[level].rect,
                       .x = args->x, .y = args->y))
        {
            return 1;
        }
    }
    return 0;
}

// the menu at `level` sized for its rows and placed with its top left at x, y, turned in from the window's edges;
// where it opens beside an item, `beside` that item's rectangle, it goes to the item's other side where the right
// has no room
static void menubar_menu_place(const MenubarCtx *args)
{
    GuiMenubar *menubar = args->menubar_out;
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int level = args->level;
    const int scale = args->frame->scale;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = scale);
    const unsigned int keys_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_KEYS_SIZE, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_PAD, .scale = scale);
    const int item_pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_PAD_X, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_GAP, .scale = scale);
    int wide = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_WIDE_LEAST, .scale = scale);
    int tall = 2 * pad;
    const unsigned int rows =
        EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        GuiRow row;
        (void)EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level,
                         .index = at, .row = &row);
        tall += EMBED_CALL(menubar_row_tall, MenubarCtx, .row = &row, .scale = scale);
        if (row.kind == GUI_ROW_LINE)
        {
            continue;
        }
        const float label =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                       .size = size, .text = row.label, .length = row.label_length);
        const float right =
            (row.kind == GUI_ROW_MORE)
                ? (float)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 8, .scale = scale)
                : EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = keys_size, .text = row.keys, .length = row.keys_length);
        const int needed = (int)roundf(label + (float)gap + right) + (2 * item_pad) + (2 * pad);
        wide = (needed > wide) ? needed : wide;
    }
    const int edge = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_EDGE, .scale = scale);
    // the window's sides are far inside what an int represents
    const int window_wide = (int)args->frame->width;
    const int window_tall = (int)args->frame->height;
    int left = args->x;
    if ((args->beside != NULL) && (left + wide > window_wide - edge))
    {
        left = args->beside->x - wide +
               EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_BESIDE, .scale = scale);
    }
    left = (left > window_wide - wide - edge) ? window_wide - wide - edge : left;
    left = (left < edge) ? edge : left;
    tall = (tall > window_tall - (2 * edge)) ? window_tall - (2 * edge) : tall;
    int top = args->y;
    if (top + tall > window_tall - edge)
    {
        top = (args->beside != NULL) ? window_tall - edge - tall : args->y - tall;
    }
    top = (top < edge) ? edge : top;
    menubar->menu[level].rect = (GuiRect){left, top, wide, tall};
}

// the first row of the open menu at `level` from `from` on, stepping `by`, that can be chosen, around the ends: its
// index, or -1 where none can
EMBED_INLINE int menubar_row_step(const MenubarCtx *args)
{
    const GuiMenubar *menubar = args->menubar;
    unsigned int level = args->level;
    // the rows are fewer than an int counts
    const int rows =
        (int)EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level);
    if (rows == 0)
    {
        return -1;
    }
    int at = args->from;
    for (int step = 0; step < rows; step += 1)
    {
        at = ((at + args->by) % rows + rows) % rows;
        GuiRow row;
        // the index stepped is among the rows
        if (EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level,
                       .index = (unsigned int)at, .row = &row) &&
            (row.kind != GUI_ROW_LINE) && !row.disabled)
        {
            return at;
        }
    }
    return -1;
}

// the menu of the title `at` opened, every other closed, its first row marked where `marked` is 1
EMBED_INLINE void menubar_title_open(const MenubarCtx *args)
{
    const GuiTitle *const title = &args->menubar_out->title[args->at];
    args->menubar_out->levels = 1u;
    args->menubar_out->submenu_deadline_ms = 0ull;
    // the titles are fewer than an int counts
    args->menubar_out->open = (int)args->at;
    GuiMenu *const menu = &args->menubar_out->menu[0];
    menu->folded = title->folded;
    menu->items = title->folded ? CLI_JSON_NONE
                                : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = args->menubar_out->json,
                                             .object = EMBED_CALL(menubar_menu, MenubarCtx,
                                                                  .menubar = args->menubar_out, .at = title->menu),
                                             .name = "items");
    menu->pointed = -1;
    menu->opened = -1;
    EMBED_CALL(menubar_menu_place, MenubarCtx, .menubar_out = args->menubar_out, .glyphs = args->glyphs,
               .frame = args->frame, .level = 0u, .x = title->rect.x,
               .y = args->frame->bar.y + args->frame->bar.height +
                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_BELOW,
                               .scale = args->frame->scale),
               .beside = NULL);
    menu->pointed = args->marked ? EMBED_CALL(menubar_row_step, MenubarCtx, .menubar = args->menubar_out, .level = 0u,
                                              .from = -1, .by = 1)
                                 : -1;
}

// the items of the row `index` of the menu at `level` opened beside it as the next menu, its first row marked where
// `marked` is 1: 1, or 0 where the row has no items or no level is left
EMBED_INLINE int menubar_row_open(const MenubarCtx *args)
{
    GuiMenubar *menubar = args->menubar_out;
    unsigned int level = args->level;
    unsigned int index = args->index;
    GuiRow row;
    if (!EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level, .index = index,
                    .row = &row) ||
        (row.kind != GUI_ROW_MORE) || (level + 1u >= GUI_MENU_LEVELS))
    {
        return 0;
    }
    menubar->levels = level + 2u;
    menubar->submenu_deadline_ms = 0ull;
    // the index is among the rows, which an int counts
    menubar->menu[level].opened = (int)index;
    GuiMenu *const under = &menubar->menu[level + 1u];
    under->folded = 0;
    under->items = row.items;
    under->pointed = -1;
    under->opened = -1;
    const GuiRect beside = EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = menubar, .level = level, .index = index,
                                      .scale = args->frame->scale);
    EMBED_CALL(menubar_menu_place, MenubarCtx, .menubar_out = menubar, .glyphs = args->glyphs, .frame = args->frame,
               .level = level + 1u,
               .x = beside.x + beside.width -
                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_BESIDE,
                               .scale = args->frame->scale),
               .y = beside.y - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_ABOVE,
                                          .scale = args->frame->scale),
               .beside = &beside);
    under->pointed = args->marked ? EMBED_CALL(menubar_row_step, MenubarCtx, .menubar = menubar, .level = level + 1u,
                                               .from = -1, .by = 1)
                                  : -1;
    return 1;
}

// the menus from `level` on closed
EMBED_INLINE void menubar_closed_from(const MenubarCtx *args)
{
    args->menubar_out->submenu_deadline_ms = 0ull;
    if (args->level < args->menubar_out->levels)
    {
        args->menubar_out->levels = args->level;
        if (args->level != 0u)
        {
            args->menubar_out->menu[args->level - 1u].opened = -1;
        }
    }
    if (args->level == 0u)
    {
        args->menubar_out->open = -1;
    }
}

void gui_menubar_close(const GuiMenubarJsonCfg *args)
{
    EMBED_CALL(menubar_closed_from, MenubarCtx, .menubar_out = args->menubar, .level = 0u);
}

// the row `index` of the menu at `level` chosen into `choice`
EMBED_INLINE void menubar_choose(const MenubarCtx *args)
{
    GuiMenubar *menubar = args->menubar_out;
    unsigned int level = args->level;
    unsigned int index = args->index;
    GuiRow row;
    (void)EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = menubar, .level = level, .index = index,
                     .row = &row);
    VerbaLinea command = {args->choice->command, sizeof(args->choice->command), 0ull};
    mmgr_line_put_n(&command, row.command, row.command_length);
    (void)mmgr_line_finish(&command);
    VerbaLinea keys = {args->choice->keys, sizeof(args->choice->keys), 0ull};
    mmgr_line_put_n(&keys, row.keys, row.keys_length);
    (void)mmgr_line_finish(&keys);
    EMBED_CALL(gui_menubar_json.close, GuiMenubarJsonCfg, .menubar = menubar);
}

int gui_menubar_press(const GuiMenubarJsonCfg *args)
{
    args->choice->command[0] = '\0';
    args->choice->keys[0] = '\0';
    const int title = EMBED_CALL(menubar_title_at, MenubarCtx, .menubar = args->menubar, .x = args->x, .y = args->y);
    if (title >= 0)
    {
        if (args->menubar->open == title)
        {
            gui_menubar_json.close(args);
        }
        else
        {
            // the title is among the titles
            EMBED_CALL(menubar_title_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                       .frame = args->frame, .at = (unsigned int)title, .marked = 1);
        }
        return 1;
    }
    for (unsigned int level = args->menubar->levels; level > 0u; level -= 1u)
    {
        if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->menubar->menu[level - 1u].rect,
                        .x = args->x, .y = args->y))
        {
            continue;
        }
        const int index = EMBED_CALL(menubar_row_at, MenubarCtx, .menubar = args->menubar, .level = level - 1u,
                                     .x = args->x, .y = args->y, .scale = args->frame->scale);
        GuiRow row;
        // the index found is among the rows
        if ((index < 0) ||
            !EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = args->menubar, .level = level - 1u,
                        .index = (unsigned int)index, .row = &row) ||
            (row.kind == GUI_ROW_LINE) || row.disabled)
        {
            return 1;
        }
        if (row.kind == GUI_ROW_MORE)
        {
            args->menubar->menu[level - 1u].pointed = index;
            (void)EMBED_CALL(menubar_row_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                             .frame = args->frame, .level = level - 1u, .index = (unsigned int)index, .marked = 1);
        }
        else
        {
            EMBED_CALL(menubar_choose, MenubarCtx, .menubar_out = args->menubar, .level = level - 1u,
                       .index = (unsigned int)index, .choice = args->choice);
        }
        return 1;
    }
    if (args->menubar->levels != 0u)
    {
        gui_menubar_json.close(args);
    }
    return 0;
}

int gui_menubar_move(const GuiMenubarGlyphsFrameXCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int changed = 0;
    const int title = EMBED_CALL(menubar_title_at, MenubarCtx, .menubar = args->menubar, .x = args->x, .y = args->y);
    if (title != args->menubar->pointed)
    {
        args->menubar->pointed = title;
        changed = 1;
    }
    if ((title >= 0) && (args->menubar->open >= 0) && (title != args->menubar->open))
    {
        // the title is among the titles
        EMBED_CALL(menubar_title_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                   .frame = args->frame, .at = (unsigned int)title, .marked = 0);
        return 1;
    }
    for (unsigned int level = args->menubar->levels; level > 0u; level -= 1u)
    {
        GuiMenu *const menu = &args->menubar->menu[level - 1u];
        if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = menu->rect, .x = args->x, .y = args->y))
        {
            continue;
        }
        const int index = EMBED_CALL(menubar_row_at, MenubarCtx, .menubar = args->menubar, .level = level - 1u,
                                     .x = args->x, .y = args->y, .scale = args->frame->scale);
        GuiRow row;
        // the index found is among the rows
        if ((index < 0) ||
            !EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = args->menubar, .level = level - 1u,
                        .index = (unsigned int)index, .row = &row) ||
            (row.kind == GUI_ROW_LINE) || row.disabled)
        {
            return changed;
        }
        if (menu->pointed == index)
        {
            return changed;
        }
        menu->pointed = index;
        // the row's items open once the pointer has rested on it, unless they are open already; the items open beside
        // a sibling close once it has rested on a row without any; a rest begun before is let go
        const int opens = (row.kind == GUI_ROW_MORE) && (menu->opened != index);
        const int closes = (row.kind != GUI_ROW_MORE) && (args->menubar->levels > level);
        args->menubar->submenu_level = level - 1u;
        args->menubar->submenu_index = opens ? index : -1;
        args->menubar->submenu_deadline_ms = (opens || closes) ? now_ms + GUI_SUBMENU_DELAY_MS : 0ull;
        return 1;
    }
    return changed;
}

unsigned long long gui_menubar_next_deadline_ms(const GuiMenubarCfg *args)
{
    return args->menubar_read->submenu_deadline_ms;
}

int gui_menubar_tick(const GuiMenubarGlyphsFrameXCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if ((args->menubar->submenu_deadline_ms == 0ull) || (now_ms < args->menubar->submenu_deadline_ms))
    {
        return 0;
    }
    args->menubar->submenu_deadline_ms = 0ull;
    if (args->menubar->submenu_index >= 0)
    {
        // the index saved is a row's
        return EMBED_CALL(menubar_row_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                          .frame = args->frame, .level = args->menubar->submenu_level,
                          .index = (unsigned int)args->menubar->submenu_index, .marked = 0);
    }
    EMBED_CALL(menubar_closed_from, MenubarCtx, .menubar_out = args->menubar,
               .level = args->menubar->submenu_level + 1u);
    return 1;
}

// a key's name parsed: whether Alt is down and the key's own name, the other modifiers left on it
EMBED_INLINE const char *menubar_key_read(const MenubarCtx *args)
{
    const char *name = args->name;
    int *alt = args->alt;
    int *modded = args->modded;
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

EMBED_INLINE int menubar_by_name(const MenubarCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->wanted, .cap = 32ull});
}

// the title whose letter is the single letter `name`, -1 for none
EMBED_INLINE int menubar_title_letter(const MenubarCtx *args)
{
    if ((args->name[0] == '\0') || (args->name[1] != '\0'))
    {
        return -1;
    }
    const char wanted =
        ((args->name[0] >= 'A') && (args->name[0] <= 'Z')) ? (char)(args->name[0] - 'A' + 'a') : args->name[0];
    for (unsigned int at = 0u; at < args->menubar->titles; at += 1u)
    {
        const GuiTitle *const title = &args->menubar->title[at];
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar, .title = title, .length = &length);
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

int gui_menubar_key(const GuiMenubarGlyphsCfg *args)
{
    args->choice->command[0] = '\0';
    args->choice->keys[0] = '\0';
    int alt = 0;
    int modded = 0;
    const char *const name =
        EMBED_CALL(menubar_key_read, MenubarCtx, .name = args->given, .alt = &alt, .modded = &modded);
    if (alt && !modded)
    {
        const int title = EMBED_CALL(menubar_title_letter, MenubarCtx, .menubar = args->menubar, .name = name);
        if (title >= 0)
        {
            // the title is among the titles
            EMBED_CALL(menubar_title_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                       .frame = args->frame, .at = (unsigned int)title, .marked = 1);
            return 1;
        }
        return 0;
    }
    if ((args->menubar->levels == 0u) || alt || modded)
    {
        return 0;
    }
    const unsigned int level = args->menubar->levels - 1u;
    GuiMenu *const menu = &args->menubar->menu[level];
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Escape"))
    {
        EMBED_CALL(menubar_closed_from, MenubarCtx, .menubar_out = args->menubar, .level = level);
        return 1;
    }
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Down") ||
        EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Up"))
    {
        menu->pointed =
            EMBED_CALL(menubar_row_step, MenubarCtx, .menubar = args->menubar, .level = level, .from = menu->pointed,
                       .by = EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Down") ? 1 : -1);
        return 1;
    }
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Home"))
    {
        menu->pointed =
            EMBED_CALL(menubar_row_step, MenubarCtx, .menubar = args->menubar, .level = level, .from = -1, .by = 1);
        return 1;
    }
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "End"))
    {
        // the rows are fewer than an int counts
        menu->pointed = EMBED_CALL(menubar_row_step, MenubarCtx, .menubar = args->menubar, .level = level,
                                   .from = (int)EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg,
                                                           .menubar_read = args->menubar, .level = level),
                                   .by = -1);
        return 1;
    }
    GuiRow row;
    const int pointed =
        (menu->pointed >= 0) && EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = args->menubar,
                                           .level = level, .index = (unsigned int)menu->pointed, .row = &row);
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Right"))
    {
        if (pointed && (row.kind == GUI_ROW_MORE))
        {
            (void)EMBED_CALL(menubar_row_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                             .frame = args->frame, .level = level, .index = (unsigned int)menu->pointed, .marked = 1);
        }
        else if ((level == 0u) && (args->menubar->titles != 0u))
        {
            // the title open is among the titles
            EMBED_CALL(menubar_title_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                       .frame = args->frame, .at = ((unsigned int)args->menubar->open + 1u) % args->menubar->titles,
                       .marked = 1);
        }
        return 1;
    }
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Left"))
    {
        if (level > 0u)
        {
            EMBED_CALL(menubar_closed_from, MenubarCtx, .menubar_out = args->menubar, .level = level);
        }
        else if (args->menubar->titles != 0u)
        {
            EMBED_CALL(menubar_title_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                       .frame = args->frame,
                       .at = ((unsigned int)args->menubar->open + args->menubar->titles - 1u) % args->menubar->titles,
                       .marked = 1);
        }
        return 1;
    }
    if (EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Enter") ||
        EMBED_CALL(menubar_by_name, MenubarCtx, .name = name, .wanted = "Space"))
    {
        if (pointed && (row.kind == GUI_ROW_MORE))
        {
            (void)EMBED_CALL(menubar_row_open, MenubarCtx, .menubar_out = args->menubar, .glyphs = args->glyphs,
                             .frame = args->frame, .level = level, .index = (unsigned int)menu->pointed, .marked = 1);
        }
        else if (pointed && !row.disabled)
        {
            EMBED_CALL(menubar_choose, MenubarCtx, .menubar_out = args->menubar, .level = level,
                       .index = (unsigned int)menu->pointed, .choice = args->choice);
        }
        return 1;
    }
    if ((name[0] != '\0') && (name[1] == '\0'))
    {
        // a letter marks the next row it begins, around the end
        const char wanted = ((name[0] >= 'A') && (name[0] <= 'Z')) ? (char)(name[0] - 'A' + 'a') : name[0];
        const int rows =
            (int)EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = args->menubar, .level = level);
        for (int step = 1; step <= rows; step += 1)
        {
            const int at = ((menu->pointed + step) % rows + rows) % rows;
            GuiRow one;
            if (EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = args->menubar, .level = level,
                           .index = (unsigned int)at, .row = &one) &&
                (one.kind != GUI_ROW_LINE) && !one.disabled && (one.label_length != 0ull))
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

void gui_menubar_titles_draw(const GuiMenubarGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    // the titles stand in the bar's pane, the slat its edge shows
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->bar_slat,
                                    .other = args->frame->bar);
    for (unsigned int at = 0u; at < args->menubar_read->titles; at += 1u)
    {
        const GuiTitle *const title = &args->menubar_read->title[at];
        // the titles are fewer than an int counts
        const int lit = ((int)at == args->menubar_read->open) || ((int)at == args->menubar_read->pointed);
        if (lit)
        {
            EMBED_CALL(
                gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = title->rect, .other = pane),
                .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                   .pixels = GUI_TITLE_RADIUS, .scale = args->frame->scale),
                .color = args->palette->bar_title_hover);
        }
        unsigned long long length = 0ull;
        const char *const text = EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar_read,
                                            .title = title, .length = &length);
        const int baseline =
            title->rect.y + (int)((((float)title->rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = size,
                         .x_f32 =
                             (float)(title->rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                .pixels = GUI_TITLE_PAD, .scale = args->frame->scale)),
                         .y = baseline, .text = text, .length = length,
                         .color = lit ? args->palette->bar_bright : args->palette->bar_title, .clip = pane);
    }
}

// a box's outline drawn, `thick` pixels wide inside it
EMBED_INLINE void menubar_box_draw(const MenubarCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiRect rect = args->rect;
    GuiColor color = args->color;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y, rect.width, args->thick}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y + rect.height - args->thick, rect.width, args->thick}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y, args->thick, rect.height}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x + rect.width - args->thick, rect.y, args->thick, rect.height}, .color = color);
}

void gui_menubar_menus_draw(const GuiMenubarGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = scale);
    const unsigned int keys_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_KEYS_SIZE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const int item_pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_PAD_X, .scale = scale);
    for (unsigned int level = 0u; level < args->menubar_read->levels; level += 1u)
    {
        const GuiMenu *const menu = &args->menubar_read->menu[level];
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = menu->rect,
                   .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                      .pixels = GUI_MENU_RADIUS, .scale = scale),
                   .color = args->palette->surface);
        EMBED_CALL(menubar_box_draw, MenubarCtx, .raster = raster, .rect = menu->rect,
                   .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                   .color = args->palette->widget_line);
        const unsigned int rows =
            EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = args->menubar_read, .level = level);
        for (unsigned int at = 0u; at < rows; at += 1u)
        {
            GuiRow row;
            (void)EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = args->menubar_read,
                             .level = level, .index = at, .row = &row);
            const GuiRect rect = EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = args->menubar_read, .level = level,
                                            .index = at, .scale = scale);
            if (row.kind == GUI_ROW_LINE)
            {
                const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 6, .scale = scale);
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                           .rect = (GuiRect){rect.x + inset, rect.y + (rect.height / 2), rect.width - (2 * inset),
                                             EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                        .scale = scale)},
                           .color = args->palette->line);
                continue;
            }
            // the rows are fewer than an int counts
            if ((int)at == menu->pointed)
            {
                EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = rect,
                           .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4,
                                                              .scale = scale),
                           .color = args->palette->selection);
            }
            else if ((int)at == menu->opened)
            {
                EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = rect,
                           .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4,
                                                              .scale = scale),
                           .color = args->palette->selection_idle);
            }
            const int baseline = rect.y + (int)((((float)rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
            const GuiColor color = row.disabled ? args->palette->text_3 : args->palette->text;
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(rect.x + item_pad), .y = baseline,
                             .text = row.label, .length = row.label_length, .color = color, .clip = rect);
            if (row.kind == GUI_ROW_MORE)
            {
                // a chevron pointing to where the items open
                const int arm = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
                const int tip_x = rect.x + rect.width - item_pad;
                const int middle = rect.y + (rect.height / 2);
                EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = tip_x - arm,
                           .y0 = middle - arm, .x1 = tip_x, .y1 = middle, .color = color);
                EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = tip_x - arm,
                           .y0 = middle + arm, .x1 = tip_x, .y1 = middle, .color = color);
            }
            else if (row.keys_length != 0ull)
            {
                const float keys_wide =
                    EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                               .face = GUI_FACE_CODE, .size = keys_size, .text = row.keys, .length = row.keys_length);
                (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                 .glyphs = glyphs, .face = GUI_FACE_CODE, .size = keys_size,
                                 .x_f32 = (float)(rect.x + rect.width - item_pad) - keys_wide, .y = baseline,
                                 .text = row.keys, .length = row.keys_length, .color = args->palette->text_3,
                                 .clip = rect);
            }
        }
    }
}
