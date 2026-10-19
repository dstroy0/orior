// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the commands, as commands.json lists them: the window's menus, and the command line's words. A menu is a
// title and its items, and a menu of jobs names the catalog's groups it lists, split by what each job works on where
// `split` says so. An item is a label, the command it runs, the keys that run it in the window, the words it takes
// after its name, and whether the command line runs it itself, in the terminal, or opens the window to run it there.
// The window reads the same file for the rest of an item: what it needs before it can act, and a label that changes
// with the scheme. The list is built into the program, as the window builds in the same one, and read once
#include "commands.h"
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"

// the most tokens the list holds, and its longest text
#define CLI_COMMANDS_TOKENS 16384u

#define CLI_COMMANDS_BYTES (1u << 20u)

static char s_text[CLI_COMMANDS_BYTES];
static unsigned long long s_size;
static CfgJsonToken s_tokens[CLI_COMMANDS_TOKENS];
static unsigned int s_count;
// the token of each menu, in the order the list holds them
static unsigned int s_menu[CLI_MENUS_MOST];
static unsigned int s_menus;

int cli_commands_read(CliLine *said)
{
    if (s_count != 0u)
    {
        return 1;
    }
    if (g_cli_commands_json_size >= sizeof(s_text))
    {
        cli_line_text(said, "commands.json: it could not be read");
        return 0;
    }
    s_size = g_cli_commands_json_size;
    memor.cpy(&(MemoriaCfg){.dst = s_text, .src = g_cli_commands_json, .bytes = s_size});
    s_text[s_size] = '\0';
    CfgJsonParse parse;
    // a list far below what a size_t counts
    if (!cfg_json_parse_metadata(s_text, (size_t)s_size, s_tokens, CLI_COMMANDS_TOKENS, &parse))
    {
        cli_line_text(said, "commands.json: ");
        cli_line_text(said, parse.reason);
        cli_line_text(said, " at line ");
        cli_line_decimal(said, parse.line, 0u);
        return 0;
    }
    s_count = parse.tokens;
    const unsigned int menus = cfg_json_member(s_text, s_tokens, 0u, "menus");
    if ((menus == 0u) || (s_tokens[menus].kind != CFG_JSON_ARRAY))
    {
        cli_line_text(said, "commands.json holds no menus");
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

// the string member `name` of the object at token `object` into `out`, which holds `room`; empty where it has none
static void cli_member_text(unsigned int object, const char *name, char *out, size_t room)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, object, name);
    out[0] = '\0';
    if ((value != 0u) && (s_tokens[value].kind == CFG_JSON_STRING))
    {
        cfg_json_string(s_text, &s_tokens[value], out, room);
    }
}

void cli_menu_title(unsigned int menu, char *title)
{
    cli_member_text(s_menu[menu], "title", title, CLI_LABEL_LONGEST);
}

void cli_menu_word(unsigned int menu, char *word)
{
    cli_menu_title(menu, word);
    for (char *at = word; *at != '\0'; at += 1)
    {
        *at = ((*at >= 'A') && (*at <= 'Z')) ? (char)(*at - 'A' + 'a') : *at;
    }
}

void cli_menu_split(unsigned int menu, char *split)
{
    cli_member_text(s_menu[menu], "split", split, CLI_LABEL_LONGEST);
}

unsigned int cli_menu_groups(unsigned int menu, char (*groups)[CLI_LABEL_LONGEST], unsigned int room)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[menu], "groups");
    if ((list == 0u) || (s_tokens[list].kind != CFG_JSON_ARRAY))
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; (each < s_tokens[list].count) && (count < room); each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            cfg_json_string(s_text, &s_tokens[at], groups[count], CLI_LABEL_LONGEST);
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

// every item of the entries of the array at token `list`, those of its groups among them, in order, added to `items`
// from `count`: the count after
static unsigned int cli_flat(unsigned int list, CliItem *items, unsigned int count, unsigned int room)
{
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; each < s_tokens[list].count; each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if ((command != 0u) && (count < room))
            {
                CliItem *const item = &items[count];
                cli_member_text(at, "label", item->label, sizeof(item->label));
                cli_member_text(at, "command", item->command, sizeof(item->command));
                cli_member_text(at, "keys", item->keys, sizeof(item->keys));
                cli_member_text(at, "args", item->args, sizeof(item->args));
                const unsigned int console = cfg_json_member(s_text, s_tokens, at, "console");
                item->console = (console != 0u) && (s_tokens[console].kind == CFG_JSON_TRUE);
                count += 1u;
            }
            else if ((command == 0u) && (nested != 0u) && (s_tokens[nested].kind == CFG_JSON_ARRAY))
            {
                count = cli_flat(nested, items, count, room);
            }
        }
        at = s_tokens[at].next;
    }
    return count;
}

unsigned int cli_menu_items(unsigned int menu, CliItem *items, unsigned int room)
{
    const unsigned int list = cfg_json_member(s_text, s_tokens, s_menu[menu], "items");
    return ((list != 0u) && (s_tokens[list].kind == CFG_JSON_ARRAY)) ? cli_flat(list, items, 0u, room) : 0u;
}

int cli_menu_command(unsigned int menu, const char *word, CliItem *item)
{
    static CliItem s_items[CLI_ITEMS_MOST];
    const unsigned int count = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_items[at].command, .other = word, .cap = CLI_CELLS_POOL}))
        {
            *item = s_items[at];
            return 1;
        }
    }
    return 0;
}

int cli_menu_named(const char *word, unsigned int *menu)
{
    for (unsigned int at = 0u; at < s_menus; at += 1u)
    {
        char named[CLI_LABEL_LONGEST];
        cli_menu_word(at, named);
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = word, .cap = CLI_CELLS_POOL}))
        {
            *menu = at;
            return 1;
        }
    }
    return 0;
}

const char *cli_commands_text(unsigned long long *size)
{
    *size = s_size;
    return s_text;
}
