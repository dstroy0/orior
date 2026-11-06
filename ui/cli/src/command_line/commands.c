// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the commands, as commands.json lists them: the window's menus, and the command line's words. A menu is a
// title and its items, and a menu of jobs names the catalog's groups it lists, split by what each job works on where
// `split` specifies so. An item is a label, the command it runs, the keys that run it in the window, the words it takes
// after its name, and whether the command line runs it itself, in the terminal, or opens the window to run it there.
// The window reads the same file for the rest of an item: what it needs before it can act, and a label that changes
// with the scheme. The list is built into the program, as the window builds in the same one, and read once
#include "commands.h"
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "pool.h"

// the list as it is built in, with a zero after it, read where it lies; its tokens, a cell of their count for as long
// as the program runs; and each string token's text, read once, in one cell for as long as the program runs
static const char *s_text;
static unsigned long long s_size;
static CfgJsonToken *s_tokens;
static unsigned int s_count;
static const char **s_string;
// the token of each menu, in the order the list contains them
static unsigned int s_menu[CLI_MENUS_MAX];
static unsigned int s_menus;

int cli_commands_read(const CliCommandsCfg *args)
{
    if (s_count != 0u)
    {
        return 1;
    }
    // the list's bytes read as the text they are
    s_text = (const char *)g_cli_commands_json;
    s_size = g_cli_commands_json_size;
    const unsigned int tokens =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = s_text, .size = s_size) + 1u;
    s_tokens = (s_tokens != NULL) ? s_tokens
                                  : g_cli_guard->persistent_buf_alloc((unsigned long long)tokens * sizeof(*s_tokens),
                                                                      g_cli_command_line_owner);
    if (s_tokens == NULL)
    {
        mmgr_line_put(args->err_msg, "commands.json: the cells cannot meet its tokens");
        return 0;
    }
    CfgJsonParse parse;
    // a list far below what a size_t counts
    if (!cfg_json_parse_metadata(s_text, (size_t)s_size, s_tokens, tokens, &parse))
    {
        mmgr_line_put(args->err_msg, "commands.json: ");
        mmgr_line_put(args->err_msg, parse.reason);
        mmgr_line_put(args->err_msg, " at line ");
        mmgr_line_decimal(args->err_msg, parse.line, 0u);
        return 0;
    }
    s_count = parse.tokens;
    s_string = g_cli_guard->persistent_buf_alloc(((unsigned long long)s_count * sizeof(*s_string)) + s_size + s_count,
                                                 g_cli_command_line_owner);
    if (s_string == NULL)
    {
        mmgr_line_put(args->err_msg, "commands.json: the cells cannot meet its texts");
        s_count = 0u;
        return 0;
    }
    char *string_text = (char *)(void *)(s_string + s_count);
    unsigned long long string_left = s_size + s_count;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        s_string[at] = "";
        if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            (void)cfg_json_string(s_text, &s_tokens[at], string_text, (size_t)string_left);
            const unsigned long long used =
                cellul.len(&(CatenaFinitaCfg){.src = string_text, .cap = string_left}) + 1ULL;
            s_string[at] = string_text;
            string_text += used;
            string_left -= used;
        }
    }
    const unsigned int menus = cfg_json_member(s_text, s_tokens, 0u, "menus");
    if ((menus == 0u) || (s_tokens[menus].kind != CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, "commands.json contains no menus");
        s_count = 0u;
        return 0;
    }
    unsigned int at = menus + 1u;
    for (unsigned int each = 0u; (each < s_tokens[menus].count) && (s_menus < CLI_MENUS_MAX); each += 1u)
    {
        s_menu[s_menus] = at;
        s_menus += 1u;
        at = s_tokens[at].next;
    }
    return 1;
}

unsigned int cli_menus(void)
{
    return s_menus;
}

typedef struct
{
    unsigned int object;
    const char *name;
    char *out;
    size_t capacity;
    unsigned int at;
    CliItem *item;
    unsigned int list;
    const char *word_text;
    CliItem *items;
    unsigned int count;
    unsigned int capacity_u32;
} CommandsCtx;
// the string member `name` of the object at token `object` into `out`, sized for `capacity`; empty where it has none
EMBED_INLINE void cli_member_text(const CommandsCtx *args)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, args->object, args->name);
    args->out[0] = '\0';
    if ((value != 0u) && (s_tokens[value].kind == CFG_JSON_STRING))
    {
        cfg_json_string(s_text, &s_tokens[value], args->out, args->capacity);
    }
}

void cli_menu_title(const CliCommandsMenuTitleCfg *args)
{
    EMBED_CALL(cli_member_text, CommandsCtx, .object = s_menu[args->menu], .name = "title", .out = args->title,
               .capacity = CLI_LABEL_MAX_LEN);
}

void cli_menu_word(const CliCommandsMenuWordCfg *args)
{
    EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = args->menu,
               .title = args->word_out);
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->word_out,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->word_out, .cap = CLI_POOL_BYTES}));
}

void cli_menu_split(const CliCommandsMenuSplitCfg *args)
{
    EMBED_CALL(cli_member_text, CommandsCtx, .object = s_menu[args->menu], .name = "split", .out = args->split,
               .capacity = CLI_LABEL_MAX_LEN);
}

unsigned int cli_menu_groups(const CliCommandsMenuGroupsCfg *args)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[args->menu], "groups");
    if ((list == 0u) || (s_tokens[list].kind != CFG_JSON_ARRAY))
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; (each < s_tokens[list].count) && (count < args->capacity); each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            cfg_json_string(s_text, &s_tokens[at], args->groups[count], CLI_LABEL_MAX_LEN);
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

// the string member `name` of the object at token `object`, standing as long as the program runs; empty where it has
// none
EMBED_INLINE const char *cli_member_string(const CommandsCtx *args)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, args->object, args->name);
    return ((value != 0u) && (s_tokens[value].kind == CFG_JSON_STRING)) ? s_string[value] : "";
}

// the item of the object at token `at` read into `item`
EMBED_INLINE void cli_item_read(const CommandsCtx *args)
{
    args->item->label = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "label");
    args->item->command = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "command");
    args->item->keys = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "keys");
    args->item->args = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "args");
    args->item->needs = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "needs");
    args->item->checks = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "checks");
    const unsigned int console = cfg_json_member(s_text, s_tokens, args->at, "console");
    args->item->console = (console != 0u) && (s_tokens[console].kind == CFG_JSON_TRUE);
    const unsigned int hidden = cfg_json_member(s_text, s_tokens, args->at, "hidden");
    args->item->hidden = (hidden != 0u) && (s_tokens[hidden].kind == CFG_JSON_TRUE);
    args->item->fill = cfg_json_member(s_text, s_tokens, args->at, "fill") != 0u;
    const unsigned int nested = cfg_json_member(s_text, s_tokens, args->at, "items");
    args->item->list = ((nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY)) ? nested : 0u;
    args->item->line = 0;
}

// the item among the entries of the array at token `list`, those of its groups among them, whose command is `word`,
// read into `item`: 1, or 0 where none is
static int cli_flat_find(const CommandsCtx *args)
{
    unsigned int at = args->list + 1u;
    for (unsigned int each = 0u; each < s_tokens[args->list].count; each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if ((command != 0u) && cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_member_string, CommandsCtx,
                                                                                  .object = at, .name = "command"),
                                                                .other = args->word_text,
                                                                .cap = CLI_POOL_BYTES}))
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = args->item);
                return 1;
            }
            if ((command == 0u) && (nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY) &&
                EMBED_CALL(cli_flat_find, CommandsCtx, .list = nested, .word_text = args->word_text,
                           .item = args->item))
            {
                return 1;
            }
        }
        at = s_tokens[at].next;
    }
    return 0;
}

// every item of the entries of the array at token `list`, those of its groups among them, in order, added to `items`
// from `count`: the count after
static unsigned int cli_flat(const CommandsCtx *args)
{
    unsigned int count = args->count;
    unsigned int at = args->list + 1u;
    for (unsigned int each = 0u; each < s_tokens[args->list].count; each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if ((command != 0u) && (count < args->capacity_u32))
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = &args->items[count]);
                count += 1u;
            }
            else if ((command == 0u) && (nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY))
            {
                count = EMBED_CALL(cli_flat, CommandsCtx, .list = nested, .items = args->items, .count = count,
                                   .capacity_u32 = args->capacity_u32);
            }
        }
        at = s_tokens[at].next;
    }
    return count;
}

unsigned int cli_menu_rows(unsigned int list, CliItem *items, unsigned int capacity)
{
    unsigned int count = 0u;
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; (list != 0u) && (each < s_tokens[list].count) && (count < capacity); each += 1u)
    {
        CliItem *const item = &items[count];
        if (s_tokens[at].kind == CFG_JSON_OBJECT)
        {
            EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = item);
            count += 1u;
        }
        else if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            memor.set(&(MemoriaCfg){.dst = item, .val = 0, .bytes = sizeof(*item)});
            item->label = "";
            item->command = "";
            item->keys = "";
            item->args = "";
            item->needs = "";
            item->checks = "";
            item->line = 1;
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

unsigned int cli_menu_list(unsigned int menu)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[menu], "items");
    return ((list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY)) ? list : 0u;
}

int cli_menu_shown(unsigned int menu)
{
    const unsigned int list = cli_menu_list(menu);
    const unsigned int popup = cfg_json_member(s_text, s_tokens, s_menu[menu], "popup");
    return (list != 0u) && (s_tokens[list].count != 0u) &&
           (cfg_json_member(s_text, s_tokens, s_menu[menu], "strip") == 0u) &&
           !((popup != 0u) && (s_tokens[popup].kind == CFG_JSON_TRUE));
}

unsigned int cli_menu_items(const CliCommandsMenuCapacityCfg *args)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[args->menu], "items");
    return ((list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY))
               ? EMBED_CALL(cli_flat, CommandsCtx, .list = list, .items = args->items, .count = 0u,
                            .capacity_u32 = args->capacity)
               : 0u;
}

int cli_menu_command(const CliCommandsMenuCfg *args)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[args->menu], "items");
    return (list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY) &&
           EMBED_CALL(cli_flat_find, CommandsCtx, .list = list, .word_text = args->word_text, .item = args->item);
}

unsigned int cli_item_words_need(const CliCommandsItemCfg *args)
{
    unsigned int needed = 0u;
    unsigned int depth = 0u;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->item_read->args, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                            .text = args->item_read->args, .at = 0ULL, .size = size, .set = "[]<");
         at < size; at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                    .text = args->item_read->args, .at = at + 1ULL, .size = size, .set = "[]<"))
    {
        const char mark = args->item_read->args[at];
        depth += (mark == '[') ? 1u : 0u;
        depth -= ((mark == ']') && (depth > 0u)) ? 1u : 0u;
        needed += ((mark == '<') && (depth == 0u)) ? 1u : 0u;
    }
    return needed;
}

int cli_menu_by_name(const CliCommandsWordCfg *args)
{
    unsigned int *menu = args->menu_pointer;
    for (unsigned int at = 0u; at < s_menus; at += 1u)
    {
        char named[CLI_LABEL_MAX_LEN];
        EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = at, .word_out = named);
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->word_text, .cap = CLI_POOL_BYTES}))
        {
            *menu = at;
            return 1;
        }
    }
    return 0;
}

const char *cli_commands_text(const CliCommandsSizeCfg *args)
{
    unsigned long long *size = args->size;
    *size = s_size;
    return s_text;
}

CliMenusOpen g_cli_menus_open = {.open = -1};

unsigned int cli_menu_titles(void)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_menus; at += 1u)
    {
        count += cli_menu_shown(at) ? 1u : 0u;
    }
    return count;
}

unsigned int cli_menu_title_menu(unsigned int at)
{
    unsigned int seen = 0u;
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        if (cli_menu_shown(menu))
        {
            if (seen == at)
            {
                return menu;
            }
            seen += 1u;
        }
    }
    return 0u;
}

const char *cli_menu_title_text(unsigned int menu)
{
    return (menu < s_menus) ? EMBED_CALL(cli_member_string, CommandsCtx, .object = s_menu[menu], .name = "title") : "";
}

// the row `at` of the list `list` as a menu shows it, a line or an item neither hidden nor filled in as the menu opens,
// read into `item`, where `item` is not NULL: 1, or 0 past them; and how many rows it shows
static int commands_list_row(unsigned int list, unsigned int at, CliItem *item)
{
    unsigned int seen = 0u;
    unsigned int token = list + 1u;
    for (unsigned int each = 0u; (list != 0u) && (each < s_tokens[list].count); each += 1u)
    {
        CliItem read;
        if (s_tokens[token].kind == CFG_JSON_OBJECT)
        {
            EMBED_CALL(cli_item_read, CommandsCtx, .at = token, .item = &read);
        }
        else
        {
            read = (CliItem){"", "", "", "", "", "", 0, 1, 0, 0, 0u};
        }
        const int shows = (s_tokens[token].kind == CFG_JSON_STRING) ||
                          ((s_tokens[token].kind == CFG_JSON_OBJECT) && !read.hidden && !read.fill);
        if (shows && (seen == at))
        {
            if (item != NULL)
            {
                *item = read;
            }
            return 1;
        }
        seen += shows ? 1u : 0u;
        token = s_tokens[token].next;
    }
    return 0;
}

static unsigned int commands_list_rows(unsigned int list)
{
    unsigned int count = 0u;
    while (commands_list_row(list, count, NULL))
    {
        count += 1u;
    }
    return count;
}

int cli_menus_row(unsigned int level, unsigned int at, CliItem *item)
{
    const CliMenuOpen *const menu = &g_cli_menus_open.level[level];
    if ((level >= g_cli_menus_open.levels) || (at >= menu->rows))
    {
        return 0;
    }
    if (menu->own != NULL)
    {
        *item = menu->own[at];
        return 1;
    }
    if (menu->list != 0u)
    {
        return commands_list_row(menu->list, at, item);
    }
    // the titles folded from the bar, each a row of its menu's items
    const unsigned int title_menu = cli_menu_title_menu(g_cli_menus_open.shown + at);
    *item = (CliItem){cli_menu_title_text(title_menu), "", "", "", "", "", 0, 0, 0, 0, cli_menu_list(title_menu)};
    return 1;
}

int cli_menus_selectable(unsigned int level, unsigned int at)
{
    CliItem item;
    if (!cli_menus_row(level, at, &item) || item.line)
    {
        return 0;
    }
    const int disabled =
        (item.list == 0u) &&
        ((item.command[0] == '\0') || ((g_cli_menus_open.disabled != NULL) && g_cli_menus_open.disabled(&item)));
    return !disabled;
}

// the row the mark steps to from `from` by `by` around the menu at `level`, past lines and rows that cannot be chosen:
// its place, or -1 where none can
static int commands_row_step(unsigned int level, int from, int by)
{
    // the rows are fewer than an int counts
    const int rows = (int)g_cli_menus_open.level[level].rows;
    int at = from;
    for (int step = 0; (rows != 0) && (step < rows); step += 1)
    {
        at = ((at + by) % rows + rows) % rows;
        if (cli_menus_selectable(level, (unsigned int)at))
        {
            return at;
        }
    }
    return -1;
}

// the menu at `level` opened on `list` or `own` rows, the next number taken
static void commands_level_open(unsigned int level, unsigned int list, const CliItem *own, unsigned int rows)
{
    g_cli_menus_open.serials += 1u;
    g_cli_menus_open.level[level] = (CliMenuOpen){list, own, rows, -1, -1, g_cli_menus_open.serials};
}

void cli_menus_close(unsigned int level)
{
    g_cli_menus_open.rest_due_ms = 0ULL;
    if (level < g_cli_menus_open.levels)
    {
        g_cli_menus_open.levels = level;
        if (level != 0u)
        {
            g_cli_menus_open.level[level - 1u].opened = -1;
        }
    }
    if (level == 0u)
    {
        g_cli_menus_open.open = -1;
    }
}

void cli_menus_bar(unsigned int shown, int folded)
{
    g_cli_menus_open.shown = shown;
    g_cli_menus_open.folded = folded;
}

void cli_menus_title_open(unsigned int at, int marked)
{
    cli_menus_close(0u);
    const unsigned int titles = cli_menu_titles();
    if (g_cli_menus_open.folded && (at >= g_cli_menus_open.shown))
    {
        commands_level_open(0u, 0u, NULL, (titles > g_cli_menus_open.shown) ? (titles - g_cli_menus_open.shown) : 0u);
    }
    else
    {
        const unsigned int list = (at < titles) ? cli_menu_list(cli_menu_title_menu(at)) : 0u;
        commands_level_open(0u, list, NULL, commands_list_rows(list));
    }
    g_cli_menus_open.levels = 1u;
    // the titles are fewer than an int counts
    g_cli_menus_open.open = (int)at;
    g_cli_menus_open.level[0].pointed = marked ? commands_row_step(0u, -1, 1) : -1;
}

void cli_menus_row_open(unsigned int level, unsigned int at, int marked)
{
    CliItem item;
    if (!cli_menus_row(level, at, &item) || (item.list == 0u) || ((level + 1u) >= CLI_MENU_LEVELS))
    {
        return;
    }
    cli_menus_close(level + 1u);
    commands_level_open(level + 1u, item.list, NULL, commands_list_rows(item.list));
    g_cli_menus_open.levels = level + 2u;
    // the row is among the rows, which an int counts
    g_cli_menus_open.level[level].opened = (int)at;
    g_cli_menus_open.level[level + 1u].pointed = marked ? commands_row_step(level + 1u, -1, 1) : -1;
}

void cli_menus_own_open(const CliItem *rows, unsigned int count, unsigned int place, int x, int y)
{
    cli_menus_close(0u);
    commands_level_open(0u, 0u, rows, count);
    g_cli_menus_open.levels = 1u;
    g_cli_menus_open.place = place;
    g_cli_menus_open.x = x;
    g_cli_menus_open.y = y;
    g_cli_menus_open.level[0].pointed = commands_row_step(0u, -1, 1);
}

// a part's own rows as built, each pointing into the texts of the part that built it, and how many stand
static CliItem s_own[CLI_MENU_OWN_ROWS];
static unsigned int s_own_count;

void cli_menus_own_begin(void)
{
    s_own_count = 0u;
}

void cli_menus_own_row(const CliItem *item)
{
    if (s_own_count >= CLI_MENU_OWN_ROWS)
    {
        return;
    }
    s_own[s_own_count] = *item;
    s_own_count += 1u;
}

void cli_menus_own_line(void)
{
    cli_menus_own_row(&(CliItem){"", "", "", "", "", "", 0, 1, 0, 0, 0u});
}

int cli_menus_own_named(const char *command, const char *args)
{
    CliItem item;
    if (!cli_menus_named(command, &item))
    {
        return 0;
    }
    item.args = args;
    cli_menus_own_row(&item);
    return 1;
}

const CliItem *cli_menus_own_rows(unsigned int *count_out)
{
    *count_out = s_own_count;
    return s_own;
}

const char *cli_menus_own_word(const char *args)
{
    // the catalog's items carry the pattern of their args in the catalog's strings; a part's row carries the word it
    // runs with, which stands in the part's own texts
    const char *const strings = (s_string != NULL) ? (const char *)(const void *)(s_string + s_count) : NULL;
    const int catalog = (strings != NULL) && ((uintptr_t)args >= (uintptr_t)strings) &&
                        ((uintptr_t)args < (uintptr_t)(strings + s_size + s_count));
    return ((args == NULL) || (args[0] == '\0') || catalog) ? NULL : args;
}

void cli_menus_title_press(unsigned int at)
{
    // a title's place on the bar is far inside what an int represents
    if (g_cli_menus_open.open == (int)at)
    {
        cli_menus_close(0u);
    }
    else
    {
        cli_menus_title_open(at, 1);
    }
}

// the row `at` of the menu at `level` chosen into `chosen`, every menu closed: 1 where it has a command
static int commands_choose(unsigned int level, unsigned int at, CliItem *chosen)
{
    const int read = cli_menus_row(level, at, chosen);
    cli_menus_close(0u);
    return read && (chosen->command[0] != '\0');
}

int cli_menus_row_press(unsigned int level, unsigned int at, CliItem *chosen)
{
    CliItem item;
    if (!cli_menus_selectable(level, at) || !cli_menus_row(level, at, &item))
    {
        return 0;
    }
    if (item.list != 0u)
    {
        // a row's place is far inside what an int represents
        g_cli_menus_open.level[level].pointed = (int)at;
        cli_menus_row_open(level, at, 1);
        return 0;
    }
    return commands_choose(level, at, chosen);
}

void cli_menus_title_move(unsigned int at)
{
    // a title's place on the bar is far inside what an int represents
    if ((g_cli_menus_open.open >= 0) && ((int)at != g_cli_menus_open.open))
    {
        cli_menus_title_open(at, 0);
    }
}

void cli_menus_row_move(unsigned int level, unsigned int at)
{
    CliItem item;
    if ((level >= g_cli_menus_open.levels) || !cli_menus_selectable(level, at) || !cli_menus_row(level, at, &item))
    {
        return;
    }
    CliMenuOpen *const menu = &g_cli_menus_open.level[level];
    // a row's place is far inside what an int represents
    const int index = (int)at;
    if (menu->pointed == index)
    {
        return;
    }
    menu->pointed = index;
    // the row's items open once the pointer has rested on it, unless they are open already; the items open beside a
    // sibling close once it has rested on a row without any; a rest begun before is let go
    const int more = item.list != 0u;
    const int opens = more && (menu->opened != index);
    const int closes = !more && (g_cli_menus_open.levels > (level + 1u));
    g_cli_menus_open.rest_level = level;
    g_cli_menus_open.rest_index = opens ? index : -1;
    g_cli_menus_open.rest_due_ms = (opens || closes) ? (cli_clock_milliseconds() + CLI_MENU_REST_MS) : 0ULL;
}

int cli_menus_rest(void)
{
    if ((g_cli_menus_open.rest_due_ms == 0ULL) || (cli_clock_milliseconds() < g_cli_menus_open.rest_due_ms))
    {
        return 0;
    }
    g_cli_menus_open.rest_due_ms = 0ULL;
    if (g_cli_menus_open.rest_index >= 0)
    {
        // the index saved is a row's
        cli_menus_row_open(g_cli_menus_open.rest_level, (unsigned int)g_cli_menus_open.rest_index, 0);
    }
    else
    {
        cli_menus_close(g_cli_menus_open.rest_level + 1u);
    }
    return 1;
}

// a letter in lower case
EMBED_INLINE char commands_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
}

// whether the key `name` is `required`
EMBED_INLINE int commands_is(const char *name, const char *required)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = required, .cap = 32ULL});
}

// the title on the bar whose letter is the single letter `name`: each title's letter the first of its letters a to z no
// title before it has; -1 for none
static int commands_title_letter(const char *name)
{
    if ((name[0] == '\0') || (name[1] != '\0'))
    {
        return -1;
    }
    const char required = commands_lower(name[0]);
    int taken[26] = {0};
    const unsigned int titles = cli_menu_titles();
    for (unsigned int at = 0u; (at < titles) && (at < g_cli_menus_open.shown); at += 1u)
    {
        const char *const text = cli_menu_title_text(cli_menu_title_menu(at));
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

// a key's name read: whether Alt is down, whether any other modifier is, and the key's own name after them
static const char *commands_key_read(const char *name, int *alt, int *modded)
{
    *alt = 0;
    *modded = 0;
    for (;;)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Alt+", .cap = 64ULL}))
        {
            *alt = 1;
            name += 4;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Mod+", .cap = 64ULL}))
        {
            *modded = 1;
            name += 4;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Shift+", .cap = 64ULL}))
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

// the key `name` taken by the menu open last, `level`: Left and Right step between the menus or out of and into the
// items under a row, Enter and Space choose or open, a letter marks the next row it begins
static int commands_level_key(const char *name, unsigned int level, CliItem *chosen, int *chose)
{
    CliMenuOpen *const menu = &g_cli_menus_open.level[level];
    const unsigned int slots = g_cli_menus_open.shown + (g_cli_menus_open.folded ? 1u : 0u);
    CliItem row;
    const int pointed = (menu->pointed >= 0) && cli_menus_row(level, (unsigned int)menu->pointed, &row);
    if (commands_is(name, "Right"))
    {
        if (pointed && (row.list != 0u))
        {
            cli_menus_row_open(level, (unsigned int)menu->pointed, 1);
        }
        else if ((level == 0u) && (slots != 0u) && (g_cli_menus_open.open >= 0))
        {
            // the title open is among the titles
            cli_menus_title_open(((unsigned int)g_cli_menus_open.open + 1u) % slots, 1);
        }
        return 1;
    }
    if (commands_is(name, "Left"))
    {
        if (level > 0u)
        {
            cli_menus_close(level);
        }
        else if ((slots != 0u) && (g_cli_menus_open.open >= 0))
        {
            // the title open is among the titles
            cli_menus_title_open(((unsigned int)g_cli_menus_open.open + slots - 1u) % slots, 1);
        }
        return 1;
    }
    if (commands_is(name, "Enter") || commands_is(name, "Space"))
    {
        if (pointed && (row.list != 0u))
        {
            cli_menus_row_open(level, (unsigned int)menu->pointed, 1);
        }
        else if (pointed && cli_menus_selectable(level, (unsigned int)menu->pointed))
        {
            *chose = commands_choose(level, (unsigned int)menu->pointed, chosen);
        }
        return 1;
    }
    if ((name[0] != '\0') && (name[1] == '\0'))
    {
        // a letter marks the next row it begins, around the end; the rows are fewer than an int counts
        const char required = commands_lower(name[0]);
        const int rows = (int)menu->rows;
        for (int step = 1; step <= rows; step += 1)
        {
            const int at = ((menu->pointed + step) % rows + rows) % rows;
            CliItem item;
            if (cli_menus_selectable(level, (unsigned int)at) && cli_menus_row(level, (unsigned int)at, &item) &&
                (item.label[0] != '\0') && (commands_lower(item.label[0]) == required))
            {
                menu->pointed = at;
                break;
            }
        }
    }
    return 1;
}

int cli_menus_key(const char *name, CliItem *chosen, int *chose)
{
    *chose = 0;
    int alt = 0;
    int modded = 0;
    const char *const key = commands_key_read(name, &alt, &modded);
    if (alt && !modded)
    {
        const int title = commands_title_letter(key);
        if (title >= 0)
        {
            // the title is among the titles
            cli_menus_title_open((unsigned int)title, 1);
            return 1;
        }
        return 0;
    }
    if ((g_cli_menus_open.levels == 0u) || alt || modded)
    {
        return 0;
    }
    const unsigned int level = g_cli_menus_open.levels - 1u;
    CliMenuOpen *const menu = &g_cli_menus_open.level[level];
    if (commands_is(key, "Escape") || commands_is(key, "Tab"))
    {
        cli_menus_close(commands_is(key, "Tab") ? 0u : level);
        return 1;
    }
    if (commands_is(key, "Down") || commands_is(key, "Up"))
    {
        menu->pointed = commands_row_step(level, menu->pointed, commands_is(key, "Down") ? 1 : -1);
        return 1;
    }
    if (commands_is(key, "Home") || commands_is(key, "End"))
    {
        // the rows are fewer than an int counts
        menu->pointed =
            commands_is(key, "Home") ? commands_row_step(level, -1, 1) : commands_row_step(level, (int)menu->rows, -1);
        return 1;
    }
    return commands_level_key(key, level, chosen, chose);
}

// the modifiers of a key's name read off its front, Ctrl and Mod the same bit, into `mods`, and the key's own name
// after them, its bytes into `rest`
static const char *commands_mods_read(const char *name, unsigned long long length, unsigned int *mods,
                                      unsigned long long *rest)
{
    static const struct
    {
        const char *mark;
        unsigned long long bytes;
        unsigned int bit;
    } s_marks[] = {{"Ctrl+", 5ULL, 1u}, {"Mod+", 4ULL, 1u}, {"Alt+", 4ULL, 2u}, {"Shift+", 6ULL, 4u}};
    *mods = 0u;
    for (unsigned int at = 0u; at < (sizeof(s_marks) / sizeof(s_marks[0]));)
    {
        if ((length >= s_marks[at].bytes) &&
            (memor.cmp(&(MemoriaCfg){.src = name, .other = s_marks[at].mark, .bytes = s_marks[at].bytes}) == 0))
        {
            *mods |= s_marks[at].bit;
            name += s_marks[at].bytes;
            length -= s_marks[at].bytes;
            at = 0u;
        }
        else
        {
            at += 1u;
        }
    }
    *rest = length;
    return name;
}

int cli_menus_keys_name(const char *keys, const char *name)
{
    unsigned int keys_mods = 0u;
    unsigned long long keys_rest = 0ULL;
    const char *const keys_key = commands_mods_read(
        keys, cellul.len(&(CatenaFinitaCfg){.src = keys, .cap = CLI_LABEL_MAX_LEN}), &keys_mods, &keys_rest);
    unsigned int mods = 0u;
    unsigned long long rest = 0ULL;
    const char *const key =
        commands_mods_read(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64ULL}), &mods, &rest);
    return (keys_rest != 0ULL) && (keys_rest == rest) && (keys_mods == mods) &&
           (memor.cmp(&(MemoriaCfg){.src = keys_key, .other = key, .bytes = rest}) == 0);
}

// the item among the entries of the array at token `list`, those of its groups among them, whose keys are `name` and
// that can be chosen, or whose command is `name` where `by_command` is 1, read into `item`: 1, or 0 where none is
static int commands_flat_keyed(unsigned int list, const char *name, int by_command, CliItem *item)
{
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; each < s_tokens[list].count; each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if (command != 0u)
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = item);
                const int matched =
                    by_command ? cellul.eq(&(CatenaFinitaCfg){.src = item->command, .other = name, .cap = 64ULL})
                               : ((item->keys[0] != '\0') && cli_menus_keys_name(item->keys, name) &&
                                  ((g_cli_menus_open.disabled == NULL) || !g_cli_menus_open.disabled(item)));
                if (matched)
                {
                    return 1;
                }
            }
            else if ((nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY) &&
                     commands_flat_keyed(nested, name, by_command, item))
            {
                return 1;
            }
        }
        at = s_tokens[at].next;
    }
    return 0;
}

int cli_menus_keyed(const char *name, CliItem *item)
{
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        const unsigned int list = cli_menu_list(menu);
        if ((list != 0u) && commands_flat_keyed(list, name, 0, item))
        {
            return 1;
        }
    }
    return 0;
}

int cli_menus_named(const char *name, CliItem *item)
{
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        const unsigned int list = cli_menu_list(menu);
        if ((list != 0u) && commands_flat_keyed(list, name, 1, item))
        {
            return 1;
        }
    }
    return 0;
}
