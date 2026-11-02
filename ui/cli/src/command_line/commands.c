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

// the list as it is built in, with a zero after it, read where it lies; and its tokens, a cell of their count allocated
// for as long as the program runs
static const char *s_text;
static unsigned long long s_size;
static CfgJsonToken *s_tokens;
static unsigned int s_count;
// the token of each menu, in the order the list contains them
static unsigned int s_menu[CLI_MENUS_MOST];
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
    const unsigned int menus = cfg_json_member(s_text, s_tokens, 0u, "menus");
    if ((menus == 0u) || (s_tokens[menus].kind != CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, "commands.json contains no menus");
        s_count = 0u;
        return 0;
    }
    unsigned int at = menus + 1u;
    for (unsigned int each = 0u; (each < s_tokens[menus].count) && (s_menus < CLI_MENUS_MOST); each += 1u)
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
    size_t room;
    unsigned int at;
    CliItem *item;
    unsigned int list;
    const char *word;
    CliItem *items;
    unsigned int count;
    unsigned int room_u32;
} CommandsCtx;
// the string member `name` of the object at token `object` into `out`, sized for `room`; empty where it has none
EMBED_INLINE void cli_member_text(const CommandsCtx *args)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, args->object, args->name);
    args->out[0] = '\0';
    if ((value != 0u) && (s_tokens[value].kind == CFG_JSON_STRING))
    {
        cfg_json_string(s_text, &s_tokens[value], args->out, args->room);
    }
}

void cli_menu_title(const CliCommandsMenuTitleCfg *args)
{
    EMBED_CALL(cli_member_text, CommandsCtx, .object = s_menu[args->menu], .name = "title", .out = args->title,
               .room = CLI_LABEL_MAX_LEN);
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
               .room = CLI_LABEL_MAX_LEN);
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
    for (unsigned int each = 0u; (each < s_tokens[list].count) && (count < args->room); each += 1u)
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

// the item of the object at token `at` read into `item`
EMBED_INLINE void cli_item_read(const CommandsCtx *args)
{
    EMBED_CALL(cli_member_text, CommandsCtx, .object = args->at, .name = "label", .out = args->item->label,
               .room = sizeof(args->item->label));
    EMBED_CALL(cli_member_text, CommandsCtx, .object = args->at, .name = "command", .out = args->item->command,
               .room = sizeof(args->item->command));
    EMBED_CALL(cli_member_text, CommandsCtx, .object = args->at, .name = "keys", .out = args->item->keys,
               .room = sizeof(args->item->keys));
    EMBED_CALL(cli_member_text, CommandsCtx, .object = args->at, .name = "args", .out = args->item->args,
               .room = sizeof(args->item->args));
    const unsigned int console = cfg_json_member(s_text, s_tokens, args->at, "console");
    args->item->console = (console != 0u) && (s_tokens[console].kind == CFG_JSON_TRUE);
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
                char named[sizeof(args->item->command)];
                EMBED_CALL(cli_member_text, CommandsCtx, .object = at, .name = "command", .out = named,
                           .room = sizeof(named));
                if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->word, .cap = CLI_CELLS_POOL}))
                {
                    EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = args->item);
                    return 1;
                }
            }
            else if ((nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY) &&
                     EMBED_CALL(cli_flat_find, CommandsCtx, .list = nested, .word = args->word, .item = args->item))
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
            if ((command != 0u) && (count < args->room_u32))
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = &args->items[count]);
                count += 1u;
            }
            else if ((command == 0u) && (nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY))
            {
                count = EMBED_CALL(cli_flat, CommandsCtx, .list = nested, .items = args->items, .count = count,
                                   .room_u32 = args->room_u32);
            }
        }
        at = s_tokens[at].next;
    }
    return count;
}

unsigned int cli_menu_items(const CliCommandsMenuRoomCfg *args)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[args->menu], "items");
    return ((list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY))
               ? EMBED_CALL(cli_flat, CommandsCtx, .list = list, .items = args->items, .count = 0u,
                            .room_u32 = args->room)
               : 0u;
}

int cli_menu_command(const CliCommandsMenuCfg *args)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[args->menu], "items");
    return (list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY) &&
           EMBED_CALL(cli_flat_find, CommandsCtx, .list = list, .word = args->word, .item = args->item);
}

unsigned int cli_item_words_need(const CliCommandsItemCfg *args)
{
    unsigned int needed = 0u;
    unsigned int depth = 0u;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->item_read->args, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                            .text = args->item_read->args, .at = 0ull, .size = size, .set = "[]<");
         at < size; at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                    .text = args->item_read->args, .at = at + 1ull, .size = size, .set = "[]<"))
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->word, .cap = CLI_CELLS_POOL}))
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
