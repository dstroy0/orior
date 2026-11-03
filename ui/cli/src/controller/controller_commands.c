// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_commands.c: the commands and the menus, whoever pressed what. The catalog is read once; a menu's rows are
// read as it opens, an item hidden or filled in as the menu opens left out, an item that needs an open file not chosen
// until one is. A title pressed opens its menu, the pointer moving onto another title while one is open opens that one,
// the pointer on a row marks it and opens the items under it once it has rested there, the pointer resting on a sibling
// closing them again, a press on a row chooses it or opens its items at once; Up and Down step the mark, Left and Right
// step between the menus or out of and into the items under one, Enter chooses, Escape closes, Tab closes them all, a
// letter marks the next row it begins, and Alt with a title's letter opens its menu. A part's menu asked for at a place
// closes those open and opens the part's there, under no title, its first row that can be chosen marked; Left and Right
// at its first level step to no title. Where each menu stands on the window is the window's; it hands this the title or
// the row it hit
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "../editor/view.h"
#include "../text/text.h"

// the menus open at once at the most; a pointer's rest on a row before its items open or a sibling's close; and the
// bytes of the rows read as a menu opens
#define CONTROLLER_MENU_LEVELS 4u
#define CONTROLLER_MENU_REST_MS 180ULL
#define CONTROLLER_ROWS_BYTES ((unsigned long long)CLI_ITEMS_MAX * sizeof(CliItem))

// An open menu: its rows in a cell sized to them, the row marked and the row whose items are open under it, each -1 for
// none, and the serial it took as it opened
typedef struct
{
    CliItem *row;
    unsigned int rows;
    int pointed;
    int opened;
    unsigned int serial;
} ControllerMenu;

// The menus: each menu's title and how many, those with a place on the bar and the menu each stands for; the rows with
// keys; how many titles the window's width shows and whether the rest fold into a last one; the title open, -1 for
// none; the menus open, one under another, the place the first stands at where it was asked for, and the serial the
// last menu opened took; and the rest the pointer is in: when it ends, 0 for none, the level of the row it rests on,
// and the row whose items are to open, -1 where the rest closes the items open beside a sibling
typedef struct
{
    unsigned int menus;
    char (*title)[CLI_LABEL_MAX_LEN];
    unsigned int *title_menu;
    unsigned int titles;
    CliItem *keyed;
    unsigned int keyed_count;
    unsigned int shown;
    int folded;
    int open;
    ControllerMenu menu[CONTROLLER_MENU_LEVELS];
    unsigned int levels;
    unsigned int place;
    int x;
    int y;
    unsigned int serials;
    unsigned long long rest_due_ms;
    unsigned int rest_level;
    int rest_index;
} ControllerMenus;

static ControllerMenus s_menus;

// the commands the window has no keys for that the view runs by name
static const char *const s_view_commands[] = {"sort-ascending", "sort-descending", "unique-lines",
                                              "upper-case",     "lower-case",      "title-case"};
// the commands that run here whatever else their keys might mean, the terminal's among them
static const char *const s_own_commands[] = {
    "exit",           "close-editor", "close-all", "last-editor", "reduce-motion", "preferences",   "terminal-view",
    "toggle",         "new",          "split",     "clear",       "kill",          "terminal-copy", "terminal-paste",
    "terminal-close", "save",         "save-all"};

typedef struct
{
    const char *name;
    const char *required;
    const char *const *names;
    unsigned int count;
    const CliItem *item;
    unsigned int level;
    unsigned int at;
    unsigned int list;
    int marked;
    int from;
    int by;
    unsigned long long length;
    unsigned int *mods;
    unsigned long long *rest;
    const char *keys;
    unsigned long long keys_length;
    int *alt;
    int *modded;
    unsigned long long *number;
    unsigned long long *colon;
    unsigned int place;
    int x;
    int y;
} ControllerCommandsCtx;

// whether `name` is among `count` names
EMBED_INLINE int commands_name_among(const ControllerCommandsCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->names[at], .cap = CLI_LABEL_MAX_LEN}))
        {
            return 1;
        }
    }
    return 0;
}

// whether `name` is `required`
EMBED_INLINE int commands_by_name(const ControllerCommandsCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->required, .cap = 32ULL});
}

// whether the item cannot be chosen: it needs a file open while none is, text chosen in the terminal while none is, the
// tab shown changed while it is not or any tab changed while none is, it is never chosen, or it has no command
EMBED_INLINE int commands_item_disabled(const ControllerCommandsCtx *args)
{
    const char *const needs = args->item->needs;
    const int needs_editor = cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "editor", .cap = CLI_LABEL_MAX_LEN}) ||
                             cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "tab", .cap = CLI_LABEL_MAX_LEN});
    const int needs_chosen =
        cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "terminal-chosen", .cap = CLI_LABEL_MAX_LEN});
    const int needs_changed =
        cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "changed-active", .cap = CLI_LABEL_MAX_LEN}) ||
        cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "changed", .cap = CLI_LABEL_MAX_LEN});
    const int never = cellul.eq(&(CatenaFinitaCfg){.src = needs, .other = "never", .cap = CLI_LABEL_MAX_LEN});
    return never || (needs_editor && (g_cli_model.tabs == 0u)) ||
           (needs_chosen && !EMBED_CALL(cli_model_terminal.checked, CliModelCtx, .path = needs)) ||
           (needs_changed && !EMBED_CALL(cli_model_save.needs, CliModelCtx, .path = needs)) ||
           (args->item->command[0] == '\0');
}

// the state the item shows, by the name it gives it, as its part answers: 1 on and 0 off, a name no part answers for
// off, and -1 where the item shows none
EMBED_INLINE int commands_item_checked(const ControllerCommandsCtx *args)
{
    const char *const checks = args->item->checks;
    if (checks[0] == '\0')
    {
        return -1;
    }
    const int motion = cellul.eq(&(CatenaFinitaCfg){.src = checks, .other = "reduce-motion", .cap = CLI_LABEL_MAX_LEN});
    const int focus = cellul.eq(&(CatenaFinitaCfg){.src = checks, .other = "focus", .cap = CLI_LABEL_MAX_LEN});
    return (motion && (g_cli_model.motion_saved || g_cli_model.motion_system)) || (focus && g_cli_model.focus) ||
           EMBED_CALL(cli_model_terminal.checked, CliModelCtx, .path = checks);
}

// whether the row `at` of the menu at `level` can be marked
EMBED_INLINE int commands_row_selectable(const ControllerCommandsCtx *args)
{
    const ControllerMenu *const menu = &s_menus.menu[args->level];
    if (args->at >= menu->rows)
    {
        return 0;
    }
    const CliItem *const item = &menu->row[args->at];
    return !item->line &&
           ((item->list != 0u) || !EMBED_CALL(commands_item_disabled, ControllerCommandsCtx, .item = item));
}

// the row the mark steps to from `from` by `by` around the menu at `level`, past lines and rows that cannot be chosen:
// its place, or -1 where none can
EMBED_INLINE int commands_row_step(const ControllerCommandsCtx *args)
{
    // the rows are fewer than an int counts
    const int rows = (int)s_menus.menu[args->level].rows;
    int at = args->from;
    for (int step = 0; (rows != 0) && (step < rows); step += 1)
    {
        at = ((at + args->by) % rows + rows) % rows;
        if (EMBED_CALL(commands_row_selectable, ControllerCommandsCtx, .level = args->level, .at = (unsigned int)at))
        {
            return at;
        }
    }
    return -1;
}

// the cell of the menu at `level` released
EMBED_INLINE void commands_menu_release(const ControllerCommandsCtx *args)
{
    g_cli_cells->persistent_buf_release(s_menus.menu[args->level].row, &g_cli_controller_cells);
    s_menus.menu[args->level].row = NULL;
    s_menus.menu[args->level].rows = 0u;
}

// the menu at `level` given the next serial, as it opens with rows read anew
EMBED_INLINE void commands_serial_take(const ControllerCommandsCtx *args)
{
    s_menus.serials += 1u;
    s_menus.menu[args->level].serial = s_menus.serials;
}

// the rows of the list `list` a menu shows read into a cell sized to them for the menu at `level`: a line, or an item
// neither hidden nor filled in as the menu opens
EMBED_INLINE void commands_rows_read(const ControllerCommandsCtx *args)
{
    EMBED_CALL(commands_menu_release, ControllerCommandsCtx, .level = args->level);
    EMBED_CALL(commands_serial_take, ControllerCommandsCtx, .level = args->level);
    CliItem *const read = g_cli_cells->temporary_buf_alloc(CONTROLLER_ROWS_BYTES, &g_cli_controller_cells);
    if (read == NULL)
    {
        return;
    }
    const unsigned int count = cli_menu_rows(args->list, read, CLI_ITEMS_MAX);
    unsigned int saved = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (read[at].line || (!read[at].hidden && !read[at].fill))
        {
            read[saved] = read[at];
            saved += 1u;
        }
    }
    ControllerMenu *const menu = &s_menus.menu[args->level];
    menu->row =
        g_cli_cells->persistent_buf_alloc(((saved != 0u) ? saved : 1u) * sizeof(*menu->row), &g_cli_controller_cells);
    if (menu->row != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = menu->row, .src = read, .bytes = saved * sizeof(*menu->row)});
        menu->rows = saved;
    }
    g_cli_cells->persistent_buf_release(read, &g_cli_controller_cells);
}

// the titles folded into the last made the rows of the menu at the first level
EMBED_INLINE void commands_folded_read(const ControllerCommandsCtx *args)
{
    (void)args;
    EMBED_CALL(commands_menu_release, ControllerCommandsCtx, .level = 0u);
    EMBED_CALL(commands_serial_take, ControllerCommandsCtx, .level = 0u);
    const unsigned int count = (s_menus.titles > s_menus.shown) ? (s_menus.titles - s_menus.shown) : 0u;
    ControllerMenu *const menu = &s_menus.menu[0];
    menu->row =
        g_cli_cells->persistent_buf_alloc(((count != 0u) ? count : 1u) * sizeof(*menu->row), &g_cli_controller_cells);
    for (unsigned int at = 0u; (menu->row != NULL) && (at < count); at += 1u)
    {
        const unsigned int menu_at = s_menus.title_menu[s_menus.shown + at];
        CliItem *const item = &menu->row[at];
        memor.set(&(MemoriaCfg){.dst = item, .val = 0, .bytes = sizeof(*item)});
        item->label = s_menus.title[menu_at];
        item->command = "";
        item->keys = "";
        item->args = "";
        item->needs = "";
        item->checks = "";
        item->list = cli_menu_list(menu_at);
        menu->rows += 1u;
    }
}

// the menus from `level` on closed
EMBED_INLINE void commands_closed_from(const ControllerCommandsCtx *args)
{
    s_menus.rest_due_ms = 0ULL;
    if (args->level < s_menus.levels)
    {
        for (unsigned int at = args->level; at < s_menus.levels; at += 1u)
        {
            EMBED_CALL(commands_menu_release, ControllerCommandsCtx, .level = at);
        }
        s_menus.levels = args->level;
        if (args->level != 0u)
        {
            s_menus.menu[args->level - 1u].opened = -1;
        }
    }
    if (args->level == 0u)
    {
        s_menus.open = -1;
    }
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
}

// the menu of the title `at` on the bar opened, the last where the titles fold standing for those folded, every other
// closed, its first row marked where `marked` is 1
EMBED_INLINE void commands_title_open(const ControllerCommandsCtx *args)
{
    EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
    const int folded = s_menus.folded && (args->at >= s_menus.shown);
    if (folded)
    {
        EMBED_CALL(commands_folded_read, ControllerCommandsCtx, .at = 0u);
    }
    else if (args->at < s_menus.titles)
    {
        EMBED_CALL(commands_rows_read, ControllerCommandsCtx, .level = 0u,
                   .list = cli_menu_list(s_menus.title_menu[args->at]));
    }
    s_menus.levels = 1u;
    // the titles are fewer than an int counts
    s_menus.open = (int)args->at;
    s_menus.menu[0].pointed =
        args->marked ? EMBED_CALL(commands_row_step, ControllerCommandsCtx, .level = 0u, .from = -1, .by = 1) : -1;
    s_menus.menu[0].opened = -1;
    EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
}

// the items of the row `at` of the menu at `level` opened beside it as the next menu, its first row marked where
// `marked` is 1
EMBED_INLINE void commands_row_open(const ControllerCommandsCtx *args)
{
    if ((args->at >= s_menus.menu[args->level].rows) || (s_menus.menu[args->level].row[args->at].list == 0u) ||
        ((args->level + 1u) >= CONTROLLER_MENU_LEVELS))
    {
        return;
    }
    EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = args->level + 1u);
    EMBED_CALL(commands_rows_read, ControllerCommandsCtx, .level = args->level + 1u,
               .list = s_menus.menu[args->level].row[args->at].list);
    s_menus.levels = args->level + 2u;
    // the row is among the rows, which an int counts
    s_menus.menu[args->level].opened = (int)args->at;
    s_menus.menu[args->level + 1u].pointed = args->marked ? EMBED_CALL(commands_row_step, ControllerCommandsCtx,
                                                                       .level = args->level + 1u, .from = -1, .by = 1)
                                                          : -1;
    s_menus.menu[args->level + 1u].opened = -1;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
}

// the row `at` of the menu at `level` chosen: every menu closed and its command run
EMBED_INLINE void commands_choose(const ControllerCommandsCtx *args)
{
    // a row's texts stand as long as the program runs, past the menu it stands in
    const char *const command = s_menus.menu[args->level].row[args->at].command;
    const char *const keys = s_menus.menu[args->level].row[args->at].keys;
    EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
    if (command[0] != '\0')
    {
        EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = command, .value = keys);
    }
}

int cli_model_catalog_read(const CliModelCtx *args)
{
    (void)args;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg))
    {
        return 0;
    }
    s_menus.open = -1;
    s_menus.menus = cli_menus();
    const unsigned int menus = (s_menus.menus != 0u) ? s_menus.menus : 1u;
    s_menus.title = g_cli_cells->persistent_buf_alloc(menus * (sizeof(*s_menus.title) + sizeof(*s_menus.title_menu)),
                                                      &g_cli_controller_cells);
    CliItem *const read = g_cli_cells->temporary_buf_alloc(CONTROLLER_ROWS_BYTES, &g_cli_controller_cells);
    if ((s_menus.title == NULL) || (read == NULL))
    {
        g_cli_cells->persistent_buf_release(read, &g_cli_controller_cells);
        return 0;
    }
    s_menus.title_menu = (unsigned int *)(void *)(s_menus.title + menus);
    unsigned int keyed = 0u;
    for (unsigned int at = 0u; at < s_menus.menus; at += 1u)
    {
        EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = at, .title = s_menus.title[at]);
        if (cli_menu_shown(at))
        {
            s_menus.title_menu[s_menus.titles] = at;
            s_menus.titles += 1u;
        }
        const unsigned int from = keyed;
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = at, .items = read + from, .capacity = CLI_ITEMS_MAX - from);
        for (unsigned int each = 0u; each < items; each += 1u)
        {
            if (read[from + each].keys[0] != '\0')
            {
                read[keyed] = read[from + each];
                keyed += 1u;
            }
        }
    }
    s_menus.keyed = g_cli_cells->persistent_buf_alloc(((keyed != 0u) ? keyed : 1u) * sizeof(*s_menus.keyed),
                                                      &g_cli_controller_cells);
    if (s_menus.keyed != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_menus.keyed, .src = read, .bytes = keyed * sizeof(*s_menus.keyed)});
        s_menus.keyed_count = keyed;
    }
    g_cli_cells->persistent_buf_release(read, &g_cli_controller_cells);
    s_menus.shown = s_menus.titles;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
    return 1;
}

// the modifiers of a key's name read off its front, Ctrl and Mod the same bit, and the key's own name after them
#define CONTROLLER_MOD 1u
#define CONTROLLER_ALT 2u
#define CONTROLLER_SHIFT 4u
EMBED_INLINE const char *commands_mods_read(const ControllerCommandsCtx *args)
{
    const char *name = args->name;
    unsigned long long length = args->length;
    *args->mods = 0u;
    for (;;)
    {
        if ((length >= 5ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Ctrl+", .bytes = 5u}) == 0))
        {
            *args->mods |= CONTROLLER_MOD;
            name += 5;
            length -= 5ULL;
        }
        else if ((length >= 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Mod+", .bytes = 4u}) == 0))
        {
            *args->mods |= CONTROLLER_MOD;
            name += 4;
            length -= 4ULL;
        }
        else if ((length >= 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Alt+", .bytes = 4u}) == 0))
        {
            *args->mods |= CONTROLLER_ALT;
            name += 4;
            length -= 4ULL;
        }
        else if ((length >= 6ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "Shift+", .bytes = 6u}) == 0))
        {
            *args->mods |= CONTROLLER_SHIFT;
            name += 6;
            length -= 6ULL;
        }
        else
        {
            *args->rest = length;
            return name;
        }
    }
}

// whether `keys_length` bytes of `keys` name the key `name`, `Ctrl+` read as `Mod+` and the modifiers in any order
EMBED_INLINE int commands_keys_name(const ControllerCommandsCtx *args)
{
    unsigned int item_mods = 0u;
    unsigned long long item_rest = 0ULL;
    const char *const item_key = EMBED_CALL(commands_mods_read, ControllerCommandsCtx, .name = args->keys,
                                            .length = args->keys_length, .mods = &item_mods, .rest = &item_rest);
    unsigned int mods = 0u;
    unsigned long long rest = 0ULL;
    const char *const key = EMBED_CALL(commands_mods_read, ControllerCommandsCtx, .name = args->name,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = 64ULL}),
                                       .mods = &mods, .rest = &rest);
    return (item_rest != 0ULL) && (item_rest == rest) && (item_mods == mods) &&
           (memor.cmp(&(MemoriaCfg){.src = item_key, .other = key, .bytes = rest}) == 0);
}

// a key's name parsed: whether Alt is down and the key's own name, the other modifiers left on it
EMBED_INLINE const char *commands_key_read(const ControllerCommandsCtx *args)
{
    const char *name = args->name;
    *args->alt = 0;
    *args->modded = 0;
    for (;;)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Alt+", .cap = 64ULL}))
        {
            *args->alt = 1;
            name += 4;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Mod+", .cap = 64ULL}))
        {
            *args->modded = 1;
            name += 4;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Shift+", .cap = 64ULL}))
        {
            *args->modded = 1;
            name += 6;
        }
        else
        {
            return name;
        }
    }
}

// a letter in lower case
EMBED_INLINE char commands_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
}

// the title on the bar whose letter is the single letter `name`: each title's letter the first of its letters a to z no
// title before it has; -1 for none
EMBED_INLINE int commands_title_letter(const ControllerCommandsCtx *args)
{
    if ((args->name[0] == '\0') || (args->name[1] != '\0'))
    {
        return -1;
    }
    const char required = commands_lower(args->name[0]);
    int taken[26] = {0};
    for (unsigned int at = 0u; (at < s_menus.titles) && (at < s_menus.shown); at += 1u)
    {
        const char *const text = s_menus.title[s_menus.title_menu[at]];
        char letter = '\0';
        unsigned int byte = 0u;
        while ((letter == '\0') && (text[byte] != '\0'))
        {
            const char lower = commands_lower(text[byte]);
            letter = ((lower >= 'a') && (lower <= 'z') && !taken[lower - 'a']) ? lower : '\0';
            byte += 1u;
        }
        if (letter == '\0')
        {
            continue;
        }
        taken[letter - 'a'] = 1;
        if (letter == required)
        {
            // the titles are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the key `name` taken by the menus: 1 where it was theirs
EMBED_INLINE int commands_menu_key(const ControllerCommandsCtx *args)
{
    int alt = 0;
    int modded = 0;
    const char *const name =
        EMBED_CALL(commands_key_read, ControllerCommandsCtx, .name = args->name, .alt = &alt, .modded = &modded);
    if (alt && !modded)
    {
        const int title = EMBED_CALL(commands_title_letter, ControllerCommandsCtx, .name = name);
        if (title >= 0)
        {
            // the title is among the titles
            EMBED_CALL(commands_title_open, ControllerCommandsCtx, .at = (unsigned int)title, .marked = 1);
            return 1;
        }
        return 0;
    }
    if ((s_menus.levels == 0u) || alt || modded)
    {
        return 0;
    }
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
    const unsigned int level = s_menus.levels - 1u;
    ControllerMenu *const menu = &s_menus.menu[level];
    const unsigned int slots = s_menus.shown + (s_menus.folded ? 1u : 0u);
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Escape"))
    {
        EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = level);
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Tab"))
    {
        EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Down") ||
        EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Up"))
    {
        menu->pointed = EMBED_CALL(
            commands_row_step, ControllerCommandsCtx, .level = level, .from = menu->pointed,
            .by = EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Down") ? 1 : -1);
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Home"))
    {
        menu->pointed = EMBED_CALL(commands_row_step, ControllerCommandsCtx, .level = level, .from = -1, .by = 1);
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "End"))
    {
        // the rows are fewer than an int counts
        menu->pointed =
            EMBED_CALL(commands_row_step, ControllerCommandsCtx, .level = level, .from = (int)menu->rows, .by = -1);
        return 1;
    }
    const CliItem *const row =
        ((menu->pointed >= 0) && ((unsigned int)menu->pointed < menu->rows)) ? &menu->row[menu->pointed] : NULL;
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Right"))
    {
        if ((row != NULL) && (row->list != 0u))
        {
            EMBED_CALL(commands_row_open, ControllerCommandsCtx, .level = level, .at = (unsigned int)menu->pointed,
                       .marked = 1);
        }
        else if ((level == 0u) && (slots != 0u) && (s_menus.open >= 0))
        {
            // the title open is among the titles
            EMBED_CALL(commands_title_open, ControllerCommandsCtx, .at = ((unsigned int)s_menus.open + 1u) % slots,
                       .marked = 1);
        }
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Left"))
    {
        if (level > 0u)
        {
            EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = level);
        }
        else if ((slots != 0u) && (s_menus.open >= 0))
        {
            // the title open is among the titles
            EMBED_CALL(commands_title_open, ControllerCommandsCtx,
                       .at = ((unsigned int)s_menus.open + slots - 1u) % slots, .marked = 1);
        }
        return 1;
    }
    if (EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Enter") ||
        EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "Space"))
    {
        if ((row != NULL) && (row->list != 0u))
        {
            EMBED_CALL(commands_row_open, ControllerCommandsCtx, .level = level, .at = (unsigned int)menu->pointed,
                       .marked = 1);
        }
        else if ((row != NULL) && EMBED_CALL(commands_row_selectable, ControllerCommandsCtx, .level = level,
                                             .at = (unsigned int)menu->pointed))
        {
            EMBED_CALL(commands_choose, ControllerCommandsCtx, .level = level, .at = (unsigned int)menu->pointed);
        }
        return 1;
    }
    if ((name[0] != '\0') && (name[1] == '\0'))
    {
        // a letter marks the next row it begins, around the end
        const char required = commands_lower(name[0]);
        // the rows are fewer than an int counts
        const int rows = (int)menu->rows;
        for (int step = 1; step <= rows; step += 1)
        {
            const int at = ((menu->pointed + step) % rows + rows) % rows;
            if (EMBED_CALL(commands_row_selectable, ControllerCommandsCtx, .level = level, .at = (unsigned int)at) &&
                (menu->row[at].label[0] != '\0') && (commands_lower(menu->row[at].label[0]) == required))
            {
                menu->pointed = at;
                break;
            }
        }
    }
    return 1;
}

void cli_model_command_run(const CliModelCtx *args)
{
    const char *const command = args->path;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "exit", .cap = CLI_LABEL_MAX_LEN}))
    {
        g_cli_model.closing = 1;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "preferences", .cap = CLI_LABEL_MAX_LEN}))
    {
        g_cli_model.preferences_displayed = !g_cli_model.preferences_displayed;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "close-editor", .cap = CLI_LABEL_MAX_LEN}))
    {
        if (g_cli_model.active >= 0)
        {
            // the tab shown is among the tabs
            EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = (unsigned int)g_cli_model.active);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "close-all", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(cli_model.tabs_close, CliModelCtx, .at = 0u);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "last-editor", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(cli_model.tab_last, CliModelCtx, .at = 0u);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "save", .cap = CLI_LABEL_MAX_LEN}))
    {
        if (g_cli_model.active >= 0)
        {
            // the tab shown is among the tabs
            EMBED_CALL(cli_model_save.tab_save, CliModelCtx, .at = (unsigned int)g_cli_model.active);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "save-all", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(cli_model_save.tabs_save, CliModelCtx, .at = 0u);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "focus", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(cli_model_notices.focus, CliModelCtx, .on = !g_cli_model.focus);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "notice-show-again", .cap = CLI_LABEL_MAX_LEN}))
    {
        if (args->at != 0u)
        {
            EMBED_CALL(cli_model_notices.show_again, CliModelCtx, .path = args->words[0]);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "reduce-motion", .cap = CLI_LABEL_MAX_LEN}))
    {
        // on or off by its word, else turned from what shows, the saved setting or the system's wish
        const char *const word = (args->at != 0u) ? args->words[0] : "";
        const int on = cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "on", .cap = CLI_LABEL_MAX_LEN});
        const int off = cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "off", .cap = CLI_LABEL_MAX_LEN});
        g_cli_model.motion_saved = on || (!off && !(g_cli_model.motion_saved || g_cli_model.motion_system));
        EMBED_CALL(cli_model.value_save, CliModelCtx, .path = "reduce-motion",
                   .value = g_cli_model.motion_saved ? "true" : "false");
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_SETTINGS);
    }
    else if (EMBED_CALL(cli_model_terminal.command, CliModelCtx, .path = command, .words = args->words, .at = args->at))
    {
        return;
    }
    else if ((args->value != NULL) && (args->value[0] != '\0'))
    {
        EMBED_CALL(cli_model_commands.key_take, CliModelCtx, .path = args->value);
    }
    else if (EMBED_CALL(commands_name_among, ControllerCommandsCtx, .name = command, .names = s_view_commands,
                        .count = sizeof(s_view_commands) / sizeof(s_view_commands[0])))
    {
        EMBED_CALL(cli_model_commands.key_take, CliModelCtx, .path = command);
    }
}

// the terminal's menu opened at the place `place` names, at x, y where that is its point, its first row that can be
// chosen marked
EMBED_INLINE void commands_terminal_menu_open(const ControllerCommandsCtx *args)
{
    ControllerMenu *const menu = &s_menus.menu[0];
    menu->row =
        g_cli_cells->persistent_buf_alloc(CLI_MODEL_TERMINAL_MENU_ROWS * sizeof(*menu->row), &g_cli_controller_cells);
    if (menu->row == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = menu->row,
                            .src = EMBED_CALL(cli_model_terminal.menu, CliModelCtx, .at = 0u),
                            .bytes = CLI_MODEL_TERMINAL_MENU_ROWS * sizeof(*menu->row)});
    menu->rows = CLI_MODEL_TERMINAL_MENU_ROWS;
    EMBED_CALL(commands_serial_take, ControllerCommandsCtx, .level = 0u);
    s_menus.levels = 1u;
    s_menus.open = -1;
    s_menus.place = args->place;
    s_menus.x = args->x;
    s_menus.y = args->y;
    menu->pointed = EMBED_CALL(commands_row_step, ControllerCommandsCtx, .level = 0u, .from = -1, .by = 1);
    menu->opened = -1;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
}

// the keyed row whose command is `name`, NULL for none
EMBED_INLINE const CliItem *commands_keyed_named(const ControllerCommandsCtx *args)
{
    for (unsigned int at = 0u; at < s_menus.keyed_count; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = s_menus.keyed[at].command, .other = args->name, .cap = CLI_LABEL_MAX_LEN}))
        {
            return &s_menus.keyed[at];
        }
    }
    return NULL;
}

// the number after the last colon of the first `length` bytes of `name`, its place in `*colon`: 1, or 0 where they do
// not end in a colon and digits
EMBED_INLINE int commands_colon_number(const ControllerCommandsCtx *args)
{
    const unsigned long long after = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                .text = args->name, .at = args->length, .set = ":");
    const int digits =
        (after > 1ULL) && (after < args->length) &&
        (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->name, .at = after,
                    .size = args->length, .classes = CLI_TEXT_DIGITS, .set = "") == args->length);
    if (digits)
    {
        *args->number = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->name, .at = after,
                                   .size = args->length, .end = NULL);
        *args->colon = after - 1ULL;
    }
    return digits;
}

// the file a path names opened, the line and the column after it, each a colon and digits and counted from 1, its
// cursor's
EMBED_INLINE void commands_file_open(const ControllerCommandsCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = sizeof(path)});
    unsigned long long first = 0ULL;
    unsigned long long second = 0ULL;
    unsigned long long colon = 0ULL;
    int numbers = 0;
    if (EMBED_CALL(commands_colon_number, ControllerCommandsCtx, .name = path, .length = length, .number = &second,
                   .colon = &colon))
    {
        length = colon;
        numbers = 1;
        if (EMBED_CALL(commands_colon_number, ControllerCommandsCtx, .name = path, .length = length, .number = &first,
                       .colon = &colon))
        {
            length = colon;
            numbers = 2;
        }
    }
    path[length] = '\0';
    const unsigned long long place_line = (numbers == 2) ? first : second;
    const unsigned long long place_col = (numbers == 2) ? second : 1ULL;
    EMBED_CALL(cli_model.file_open, CliModelCtx, .path = path, .placed = numbers != 0,
               .line = (place_line != 0ULL) ? (place_line - 1ULL) : 0ULL,
               .col = (place_col != 0ULL) ? (place_col - 1ULL) : 0ULL);
}

int cli_model_command_named(const CliModelCtx *args)
{
    const char *const command = args->path;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "file", .cap = CLI_LABEL_MAX_LEN}))
    {
        if (args->at != 0u)
        {
            EMBED_CALL(commands_file_open, ControllerCommandsCtx, .name = args->words[0]);
        }
        return args->at != 0u;
    }
    const CliItem *const item = EMBED_CALL(commands_keyed_named, ControllerCommandsCtx, .name = command);
    const int known = (item != NULL) ||
                      EMBED_CALL(commands_name_among, ControllerCommandsCtx, .name = command, .names = s_own_commands,
                                 .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])) ||
                      EMBED_CALL(commands_name_among, ControllerCommandsCtx, .name = command, .names = s_view_commands,
                                 .count = sizeof(s_view_commands) / sizeof(s_view_commands[0]));
    if (known)
    {
        EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = command,
                   .value = (item != NULL) ? item->keys : NULL, .words = args->words, .at = args->at);
    }
    return known;
}

// the keyed row whose keys are the key `name` that can be chosen, NULL for none
EMBED_INLINE const CliItem *commands_keyed(const ControllerCommandsCtx *args)
{
    for (unsigned int at = 0u; at < s_menus.keyed_count; at += 1u)
    {
        const CliItem *const item = &s_menus.keyed[at];
        if (EMBED_CALL(commands_keys_name, ControllerCommandsCtx, .keys = item->keys,
                       .keys_length = cellul.len(&(CatenaFinitaCfg){.src = item->keys, .cap = CLI_LABEL_MAX_LEN}),
                       .name = args->name) &&
            !EMBED_CALL(commands_item_disabled, ControllerCommandsCtx, .item = item))
        {
            return item;
        }
    }
    return NULL;
}

// the tool whose keys are the key `name`, -1 for none
EMBED_INLINE int commands_tool_key(const ControllerCommandsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_tools_count; at += 1u)
    {
        const char *const keys = g_cli_tools[at].keys;
        if ((keys[0] != '\0') &&
            EMBED_CALL(commands_keys_name, ControllerCommandsCtx, .keys = keys,
                       .keys_length = cellul.len(&(CatenaFinitaCfg){.src = keys, .cap = 32ULL}), .name = args->name))
        {
            // the tools are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

void cli_model_key_take(const CliModelCtx *args)
{
    const char *const name = args->path;
    // the menu key opens the menu of what takes the keys, at its place, as a browser opens it, and does nothing on a
    // menu open
    if ((s_menus.levels == 0u) && !args->on &&
        EMBED_CALL(commands_by_name, ControllerCommandsCtx, .name = name, .required = "ContextMenu") &&
        EMBED_CALL(cli_model_terminal.checked, CliModelCtx, .path = "terminal-keys"))
    {
        EMBED_CALL(commands_terminal_menu_open, ControllerCommandsCtx, .place = CLI_MENU_PLACE_TERMINAL_KEYS);
        return;
    }
    // the terminal's panel, where it takes the keys, takes them ahead of the window while no menu is open
    if ((s_menus.levels == 0u) && !args->on && EMBED_CALL(cli_model_terminal.key, CliModelCtx, .path = name))
    {
        return;
    }
    if (EMBED_CALL(commands_menu_key, ControllerCommandsCtx, .name = name) || args->on)
    {
        return;
    }
    const CliItem *const item = EMBED_CALL(commands_keyed, ControllerCommandsCtx, .name = name);
    if ((item != NULL) &&
        EMBED_CALL(commands_name_among, ControllerCommandsCtx, .name = item->command, .names = s_own_commands,
                   .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])))
    {
        EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = item->command, .value = item->keys);
        return;
    }
    const int tool = EMBED_CALL(commands_tool_key, ControllerCommandsCtx, .name = name);
    if (tool >= 0)
    {
        // the tool is among the tools
        EMBED_CALL(cli_model.tool_toggle, CliModelCtx, .at = (unsigned int)tool);
        return;
    }
    if (EMBED_CALL(cli_model_panels.key, CliModelCtx, .path = name))
    {
        return;
    }
    const CliViewInput keyed = {.kind = CLI_VIEW_KEY, .name = name};
    EMBED_CALL(cli_model.editing, CliModelCtx, .on = 1);
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &keyed, .on = 1);
}

void cli_model_preferences_key(const CliModelCtx *args)
{
    const CliItem *const item = EMBED_CALL(commands_keyed, ControllerCommandsCtx, .name = args->path);
    if ((item != NULL) &&
        cellul.eq(&(CatenaFinitaCfg){.src = item->command, .other = "preferences", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = item->command, .value = item->keys);
    }
}

void cli_model_menu_input(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (input->kind == CLI_INPUT_POINTER_DOWN)
    {
        if (input->hit == CLI_HIT_MENU_TITLE)
        {
            // a title's place on the bar is far inside what an int represents
            if (s_menus.open == (int)input->index)
            {
                EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
            }
            else
            {
                EMBED_CALL(commands_title_open, ControllerCommandsCtx, .at = input->index, .marked = 1);
            }
        }
        else if ((input->hit == CLI_HIT_MENU_ROW) && (input->level < s_menus.levels) &&
                 EMBED_CALL(commands_row_selectable, ControllerCommandsCtx, .level = input->level, .at = input->index))
        {
            if (s_menus.menu[input->level].row[input->index].list != 0u)
            {
                // a row's place is far inside what an int represents
                s_menus.menu[input->level].pointed = (int)input->index;
                EMBED_CALL(commands_row_open, ControllerCommandsCtx, .level = input->level, .at = input->index,
                           .marked = 1);
            }
            else
            {
                EMBED_CALL(commands_choose, ControllerCommandsCtx, .level = input->level, .at = input->index);
            }
        }
        else if ((input->hit != CLI_HIT_MENU_ROW) && (s_menus.levels != 0u))
        {
            EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
        }
        return;
    }
    if (input->kind != CLI_INPUT_POINTER_MOVED)
    {
        return;
    }
    if (input->hit == CLI_HIT_MENU_TITLE)
    {
        // a title's place on the bar is far inside what an int represents
        if ((s_menus.open >= 0) && ((int)input->index != s_menus.open))
        {
            EMBED_CALL(commands_title_open, ControllerCommandsCtx, .at = input->index, .marked = 0);
        }
    }
    else if ((input->hit == CLI_HIT_MENU_ROW) && (input->level < s_menus.levels) &&
             EMBED_CALL(commands_row_selectable, ControllerCommandsCtx, .level = input->level, .at = input->index))
    {
        ControllerMenu *const menu = &s_menus.menu[input->level];
        // a row's place is far inside what an int represents
        const int index = (int)input->index;
        if (menu->pointed != index)
        {
            menu->pointed = index;
            // the row's items open once the pointer has rested on it, unless they are open already; the items open
            // beside a sibling close once it has rested on a row without any; a rest begun before is let go
            const int more = menu->row[index].list != 0u;
            const int opens = more && (menu->opened != index);
            const int closes = !more && (s_menus.levels > (input->level + 1u));
            s_menus.rest_level = input->level;
            s_menus.rest_index = opens ? index : -1;
            s_menus.rest_due_ms = (opens || closes) ? (now_ms + CONTROLLER_MENU_REST_MS) : 0ULL;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
        }
    }
}

void cli_model_menu_open(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    // a menu asked for on a menu open is none of a part's, and one asked for anywhere else closes those open first
    const int asked = input->kind == CLI_INPUT_MENU;
    if (asked && (input->hit == CLI_HIT_MENU_ROW))
    {
        return;
    }
    EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = 0u);
    // the terminal's panel answers for every place on it
    if (asked && (input->hit >= CLI_HIT_TERMINAL) && (input->hit <= CLI_HIT_TERMINAL_PANEL))
    {
        EMBED_CALL(commands_terminal_menu_open, ControllerCommandsCtx, .place = CLI_MENU_PLACE_POINT, .x = input->x,
                   .y = input->y);
    }
}

void cli_model_menu_bar_size(const CliModelCtx *args)
{
    s_menus.shown = args->at;
    s_menus.folded = args->on;
}

unsigned long long cli_model_menu_due_ms(const CliModelCtx *args)
{
    (void)args;
    return s_menus.rest_due_ms;
}

int cli_model_menu_step(const CliModelCtx *args)
{
    (void)args;
    if ((s_menus.rest_due_ms == 0ULL) || (cli_clock_milliseconds() < s_menus.rest_due_ms))
    {
        return 0;
    }
    s_menus.rest_due_ms = 0ULL;
    if (s_menus.rest_index >= 0)
    {
        // the index saved is a row's
        EMBED_CALL(commands_row_open, ControllerCommandsCtx, .level = s_menus.rest_level,
                   .at = (unsigned int)s_menus.rest_index, .marked = 0);
    }
    else
    {
        EMBED_CALL(commands_closed_from, ControllerCommandsCtx, .level = s_menus.rest_level + 1u);
    }
    return 1;
}

void *cli_model_menus_alloc(const CliModelCtx *args)
{
    (void)args;
    // the titles' pointers and labels, the levels, and each level's rows with their labels and keys, counted then laid
    unsigned long long texts = 0ULL;
    unsigned long long rows = 0ULL;
    for (unsigned int at = 0u; at < s_menus.titles; at += 1u)
    {
        texts +=
            cellul.len(&(CatenaFinitaCfg){.src = s_menus.title[s_menus.title_menu[at]], .cap = CLI_LABEL_MAX_LEN}) +
            1ULL;
    }
    for (unsigned int level = 0u; level < s_menus.levels; level += 1u)
    {
        rows += s_menus.menu[level].rows;
        for (unsigned int at = 0u; at < s_menus.menu[level].rows; at += 1u)
        {
            const CliItem *const item = &s_menus.menu[level].row[at];
            texts += cellul.len(&(CatenaFinitaCfg){.src = item->label, .cap = CLI_LABEL_MAX_LEN}) +
                     cellul.len(&(CatenaFinitaCfg){.src = item->keys, .cap = CLI_LABEL_MAX_LEN}) + 2ULL;
        }
    }
    CliMenus *const menus = g_cli_cells->persistent_buf_alloc(
        sizeof(*menus) + (s_menus.titles * sizeof(*menus->title)) + (s_menus.levels * sizeof(*menus->level)) +
            (rows * sizeof(CliMenuRow)) + texts,
        &g_cli_controller_cells);
    if (menus == NULL)
    {
        return NULL;
    }
    menus->title = (const char **)(void *)(menus + 1);
    menus->titles = s_menus.titles;
    menus->open = s_menus.open;
    menus->level = (CliMenuLevel *)(void *)(menus->title + s_menus.titles);
    menus->levels = s_menus.levels;
    menus->place = s_menus.place;
    menus->x = s_menus.x;
    menus->y = s_menus.y;
    CliMenuRow *row = (CliMenuRow *)(void *)(menus->level + s_menus.levels);
    char *text = (char *)(void *)(row + rows);
    for (unsigned int at = 0u; at < s_menus.titles; at += 1u)
    {
        const char *const title = s_menus.title[s_menus.title_menu[at]];
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_LABEL_MAX_LEN}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = title, .bytes = size});
        menus->title[at] = text;
        text += size;
    }
    for (unsigned int level = 0u; level < s_menus.levels; level += 1u)
    {
        const ControllerMenu *const menu = &s_menus.menu[level];
        menus->level[level] = (CliMenuLevel){row, menu->rows, menu->pointed, menu->opened, menu->serial};
        for (unsigned int at = 0u; at < menu->rows; at += 1u)
        {
            const CliItem *const item = &menu->row[at];
            const unsigned long long label =
                cellul.len(&(CatenaFinitaCfg){.src = item->label, .cap = CLI_LABEL_MAX_LEN}) + 1ULL;
            const unsigned long long keys =
                cellul.len(&(CatenaFinitaCfg){.src = item->keys, .cap = CLI_LABEL_MAX_LEN}) + 1ULL;
            memor.cpy(&(MemoriaCfg){.dst = text, .src = item->label, .bytes = label});
            memor.cpy(&(MemoriaCfg){.dst = text + label, .src = item->keys, .bytes = keys});
            const unsigned int listed_kind = (item->list != 0u) ? CLI_MENU_ROW_MORE : CLI_MENU_ROW_ITEM;
            row[at] = (CliMenuRow){item->line ? CLI_MENU_ROW_LINE : listed_kind, text, text + label,
                                   !item->line && (item->list == 0u) &&
                                       EMBED_CALL(commands_item_disabled, ControllerCommandsCtx, .item = item),
                                   EMBED_CALL(commands_item_checked, ControllerCommandsCtx, .item = item)};
            text += label + keys;
        }
        row += menu->rows;
    }
    return menus;
}
