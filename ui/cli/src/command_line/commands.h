// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.h: the commands as commands.json lists them, built into the program and read once (commands.c): the menus,
// each its title, its word, its groups and split, and its items flattened, groups among them, in order
#ifndef COMMANDS_H
#define COMMANDS_H

#include "cli_internal.h"

// the most menus, the most items of one menu, and the longest label, command, keys or words of one
#define CLI_MENUS_MOST 32u

#define CLI_ITEMS_MOST 512u

#define CLI_LABEL_LONGEST 256u

// One item of a menu: its label, the command it runs, the keys that run it in the window, the words it takes after
// its name, and 1 where the command line runs it in the terminal
typedef struct
{
    char label[CLI_LABEL_LONGEST];
    char command[CLI_LABEL_LONGEST];
    char keys[CLI_LABEL_LONGEST];
    char args[CLI_LABEL_LONGEST];
    int console;
} CliItem;

// the bytes of commands.json as built into the program (plugins_bundled.c, written as the program is built)
extern const unsigned char g_cli_commands_json[];
extern const unsigned long long g_cli_commands_json_size;

// the list read: 1, or 0 with why said
int cli_commands_read(CliLine *said);
unsigned int cli_menus(void);
// the menu's title, which holds CLI_LABEL_LONGEST; the word the command line takes for it, its title in lower case;
// its split; its groups, their count
void cli_menu_title(unsigned int menu, char *title);
void cli_menu_word(unsigned int menu, char *word);
void cli_menu_split(unsigned int menu, char *split);
unsigned int cli_menu_groups(unsigned int menu, char (*groups)[CLI_LABEL_LONGEST], unsigned int room);
// every item of the menu, those of its groups among them, in order: their count
unsigned int cli_menu_items(unsigned int menu, CliItem *items, unsigned int room);
// the item of the menu whose command is `word`: 1, or 0 where it has none; and the words an item must be given, each
// `<word>` of its args outside a bracket
int cli_menu_command(unsigned int menu, const char *word, CliItem *item);
unsigned int cli_item_words_needed(const CliItem *item);
// the menu whose word is `word`: 1, or 0 where none is
int cli_menu_named(const char *word, unsigned int *menu);
// the list's text as read, for the window, its size into `size`
const char *cli_commands_text(unsigned long long *size);

#endif
