// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.h: the commands as commands.json lists them, built into the program and read once (commands.c): the menus,
// each its title, its word, its groups and split, and its items flattened, groups among them, in order
#ifndef COMMANDS_H
#define COMMANDS_H
#include "cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliCommandsMenuCfg CliCommandsMenuCfg;
typedef struct CliCommandsMenuGroupsCfg CliCommandsMenuGroupsCfg;
typedef struct CliCommandsMenuCapacityCfg CliCommandsMenuCapacityCfg;
typedef struct CliCommandsWordCfg CliCommandsWordCfg;
typedef struct CliCommandsMenuSplitCfg CliCommandsMenuSplitCfg;
typedef struct CliCommandsMenuTitleCfg CliCommandsMenuTitleCfg;
typedef struct CliCommandsMenuWordCfg CliCommandsMenuWordCfg;
typedef struct CliCommandsCfg CliCommandsCfg;
typedef struct CliCommandsSizeCfg CliCommandsSizeCfg;
typedef struct CliCommandsItemCfg CliCommandsItemCfg;

// the most menus, the most items of one menu, and the longest label, command, keys or words of one
#define CLI_MENUS_MAX 32u

#define CLI_ITEMS_MAX 512u

#define CLI_LABEL_MAX_LEN 256u

// One row of a menu: its label, the command it runs, the keys that run it in the window, the words it takes after its
// name, what it needs before it can be chosen, 1 where the command line runs it in the terminal, whether it is a line,
// hidden, or filled in as its menu opens, and the list of the rows under it, 0 where it has none
typedef struct
{
    const char *label;
    const char *command;
    const char *keys;
    const char *args;
    const char *needs;
    int console;
    int line;
    int hidden;
    int fill;
    unsigned int list;
} CliItem;

// the bytes of commands.json as built into the program (plugins_bundled.c, written as the program is built)
extern const unsigned char g_cli_commands_json[];
extern const unsigned long long g_cli_commands_json_size;

// the list read: 1, or 0 with why given
int cli_commands_read(const CliCommandsCfg *args);
unsigned int cli_menus(void);
// the menu's title, sized for CLI_LABEL_MAX_LEN; the word the command line takes for it, its title in lower case;
// its split; its groups, their count
void cli_menu_title(const CliCommandsMenuTitleCfg *args);
void cli_menu_word(const CliCommandsMenuWordCfg *args);
void cli_menu_split(const CliCommandsMenuSplitCfg *args);
unsigned int cli_menu_groups(const CliCommandsMenuGroupsCfg *args);
// every item of the menu, those of its groups among them, in order: their count
unsigned int cli_menu_items(const CliCommandsMenuCapacityCfg *args);
// the rows of the list `list`, a row's own or a menu's, in order and as they stand: their count; the list of a menu's
// own rows; and whether a menu takes a title on the window's bar: it has rows, and it is neither the strip's nor a
// popup
unsigned int cli_menu_rows(unsigned int list, CliItem *items, unsigned int capacity);
unsigned int cli_menu_list(unsigned int menu);
int cli_menu_shown(unsigned int menu);
// the item of the menu whose command is `word`: 1, or 0 where it has none; and the words an item must be given, each
// `<word>` of its args outside a bracket
int cli_menu_command(const CliCommandsMenuCfg *args);
unsigned int cli_item_words_need(const CliCommandsItemCfg *args);
// the menu whose word is `word`: 1, or 0 where none is
int cli_menu_by_name(const CliCommandsWordCfg *args);
// the list's text as read, for the window, its size into `size`
const char *cli_commands_text(const CliCommandsSizeCfg *args);

struct CliCommandsMenuCfg
{
    unsigned int menu;
    const char *word_text;
    CliItem *item;
};

struct CliCommandsMenuGroupsCfg
{
    unsigned int menu;
    char (*groups)[CLI_LABEL_MAX_LEN];
    unsigned int capacity;
};

struct CliCommandsMenuCapacityCfg
{
    unsigned int menu;
    unsigned int capacity;
    CliItem *items;
};

struct CliCommandsWordCfg
{
    const char *word_text;
    unsigned int *menu_pointer;
};

struct CliCommandsMenuSplitCfg
{
    unsigned int menu;
    char *split;
};

struct CliCommandsMenuTitleCfg
{
    unsigned int menu;
    char *title;
};

struct CliCommandsMenuWordCfg
{
    unsigned int menu;
    char *word_out;
};

struct CliCommandsCfg
{
    VerbaLinea *err_msg;
};

struct CliCommandsSizeCfg
{
    unsigned long long *size;
};

struct CliCommandsItemCfg
{
    const CliItem *item_read;
};

typedef struct
{
    int (*read)(const CliCommandsCfg *args);
} CliCommandsNs;
EMBED_TABLE_LAYOUT(CliCommandsNs, read);
EMBED_TABLE_STORAGE CliCommandsNs cli_commands EMBED_UNUSED = {
    .read = cli_commands_read,
};

typedef struct
{
    void (*menu_title)(const CliCommandsMenuTitleCfg *args);
} CliCommandsMenuTitleNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuTitleNs, menu_title);
EMBED_TABLE_STORAGE CliCommandsMenuTitleNs cli_commands_menu_title EMBED_UNUSED = {
    .menu_title = cli_menu_title,
};

typedef struct
{
    void (*menu_word)(const CliCommandsMenuWordCfg *args);
} CliCommandsMenuWordNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuWordNs, menu_word);
EMBED_TABLE_STORAGE CliCommandsMenuWordNs cli_commands_menu_word EMBED_UNUSED = {
    .menu_word = cli_menu_word,
};

typedef struct
{
    void (*menu_split)(const CliCommandsMenuSplitCfg *args);
} CliCommandsMenuSplitNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuSplitNs, menu_split);
EMBED_TABLE_STORAGE CliCommandsMenuSplitNs cli_commands_menu_split EMBED_UNUSED = {
    .menu_split = cli_menu_split,
};

typedef struct
{
    unsigned int (*menu_groups)(const CliCommandsMenuGroupsCfg *args);
} CliCommandsMenuGroupsNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuGroupsNs, menu_groups);
EMBED_TABLE_STORAGE CliCommandsMenuGroupsNs cli_commands_menu_groups EMBED_UNUSED = {
    .menu_groups = cli_menu_groups,
};

typedef struct
{
    unsigned int (*menu_items)(const CliCommandsMenuCapacityCfg *args);
} CliCommandsMenuCapacityNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuCapacityNs, menu_items);
EMBED_TABLE_STORAGE CliCommandsMenuCapacityNs cli_commands_menu_capacity EMBED_UNUSED = {
    .menu_items = cli_menu_items,
};

typedef struct
{
    int (*menu_command)(const CliCommandsMenuCfg *args);
} CliCommandsMenuNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuNs, menu_command);
EMBED_TABLE_STORAGE CliCommandsMenuNs cli_commands_menu EMBED_UNUSED = {
    .menu_command = cli_menu_command,
};

typedef struct
{
    unsigned int (*item_words_need)(const CliCommandsItemCfg *args);
} CliCommandsItemNs;
EMBED_TABLE_LAYOUT(CliCommandsItemNs, item_words_need);
EMBED_TABLE_STORAGE CliCommandsItemNs cli_commands_item EMBED_UNUSED = {
    .item_words_need = cli_item_words_need,
};

typedef struct
{
    int (*menu_by_name)(const CliCommandsWordCfg *args);
} CliCommandsWordNs;
EMBED_TABLE_LAYOUT(CliCommandsWordNs, menu_by_name);
EMBED_TABLE_STORAGE CliCommandsWordNs cli_commands_word EMBED_UNUSED = {
    .menu_by_name = cli_menu_by_name,
};

typedef struct
{
    const char *(*text)(const CliCommandsSizeCfg *args);
} CliCommandsSizeNs;
EMBED_TABLE_LAYOUT(CliCommandsSizeNs, text);
EMBED_TABLE_STORAGE CliCommandsSizeNs cli_commands_size EMBED_UNUSED = {
    .text = cli_commands_text,
};

#endif
