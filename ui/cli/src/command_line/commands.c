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
#include "cells.h"

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
                                  : g_cli_cells->persistent_buf_alloc((unsigned long long)tokens * sizeof(*s_tokens),
                                                                      &g_cli_command_line_cells);
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
    s_string = g_cli_cells->persistent_buf_alloc(((unsigned long long)s_count * sizeof(*s_string)) + s_size + s_count,
                                                 &g_cli_command_line_cells);
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
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->word_out, .cap = CLI_CELLS_POOL}));
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
            if (command != 0u)
            {
                const char *const named = EMBED_CALL(cli_member_string, CommandsCtx, .object = at, .name = "command");
                if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->word_text, .cap = CLI_CELLS_POOL}))
                {
                    EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = args->item);
                    return 1;
                }
            }
            else if ((nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY) &&
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
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->item_read->args, .cap = CLI_CELLS_POOL});
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->word_text, .cap = CLI_CELLS_POOL}))
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
