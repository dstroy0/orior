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
// name, or for a part's own row the one word it runs with, what it needs before it can be chosen, the name of the state
// it shows on or off, empty where it is neither, 1 where the command line runs it in the terminal, whether it is a
// line, hidden, or filled in as its menu opens, and the list of the rows under it, 0 where it has none
typedef struct
{
    const char *label;
    const char *command;
    const char *keys;
    const char *args;
    const char *needs;
    const char *checks;
    int console;
    int line;
    int hidden;
    int fill;
    unsigned int list;
} CliItem;

// the menus open at once at the most; a pointer's rest on a row before its items open or a sibling's close; and where
// a menu asked for and not opened under a title stands: at its point, or at the terminal's key field
#define CLI_MENU_LEVELS 4u
#define CLI_MENU_REST_MS 180ULL
#define CLI_MENU_PLACE_POINT 0u
#define CLI_MENU_PLACE_TERMINAL_KEYS 1u

// the rows of a part's own menu built as the menu is asked for: how many at the most
#define CLI_MENU_OWN_ROWS 24u

// One menu open: the list its rows are read from, 0 where they are the titles folded from the bar or a part's own rows;
// those own rows, NULL for none; how many rows it has; the row marked and the row whose items are open under it, each
// -1 for none; and the number it took as it opened, a new one each time
typedef struct
{
    unsigned int list;
    const CliItem *own;
    unsigned int rows;
    int pointed;
    int opened;
    unsigned int serial;
} CliMenuOpen;

// The menus open: the title on the bar whose menu is open, -1 for none; how many titles the bar shows and whether the
// rest fold into a last; the menus open, one under another; where the first stands where it was asked for and not under
// a title, at the place `place` names, its top left at x, y in the window's pixels where that is its point; the numbers
// the menus took; the rest the pointer is in: when it ends, 0 for none, the level of the row it rests on, and the row
// whose items open as it ends, -1 where it closes the items open beside a sibling; the window's answer to whether an
// item cannot be chosen, NULL where every item with a command can be; and its answer to the state an item shows by the
// name in its `checks`, 1 on and 0 off, NULL where every state is off
typedef struct
{
    int open;
    unsigned int shown;
    int folded;
    CliMenuOpen level[CLI_MENU_LEVELS];
    unsigned int levels;
    unsigned int place;
    int x;
    int y;
    unsigned int serials;
    unsigned long long rest_due_ms;
    unsigned int rest_level;
    int rest_index;
    int (*disabled)(const CliItem *item);
    int (*checked)(const CliItem *item);
} CliMenusOpen;

extern CliMenusOpen g_cli_menus_open;

// the titles on the bar: how many menus take one, and the menu of the title `at`; and the title of the menu `menu`,
// standing as long as the program runs
unsigned int cli_menu_titles(void);
unsigned int cli_menu_title_menu(unsigned int at);
const char *cli_menu_title_text(unsigned int menu);
// the row `at` of the menu open at `level` read into `item`: 1, or 0 past its rows; and whether it can be marked: a
// row of items, or an item that can be chosen
int cli_menus_row(unsigned int level, unsigned int at, CliItem *item);
int cli_menus_selectable(unsigned int level, unsigned int at);
// the menus from `level` on closed; the titles the bar shows, `shown`, the rest folded into a last where `folded` is 1;
// the menu of the title `at` opened, the last where the titles fold standing for those folded, every other closed, its
// first row that can be chosen marked where `marked` is 1; the items of the row `at` of the menu at `level` opened
// beside it the same way; and `count` rows of a part's own, standing while they are open, opened as its menu at the
// place `place` names, at x, y where that is its point
void cli_menus_close(unsigned int level);
void cli_menus_bar(unsigned int shown, int folded);
void cli_menus_title_open(unsigned int at, int marked);
void cli_menus_row_open(unsigned int level, unsigned int at, int marked);
void cli_menus_own_open(const CliItem *rows, unsigned int count, unsigned int place, int x, int y);
// a part's own rows, built as its menu is asked for and standing while it is open: begun empty; `item` added after
// them, its texts the part's own, standing while the menu is open; a line added; the catalog's item whose command is
// `command` added with `args`: 1, or 0 where the catalog has none; the rows, their count into `count_out`; and the
// word a chosen row's `args` runs with, NULL where the args are a catalog item's pattern or empty
void cli_menus_own_begin(void);
void cli_menus_own_row(const CliItem *item);
void cli_menus_own_line(void);
int cli_menus_own_named(const char *command, const char *args);
const CliItem *cli_menus_own_rows(unsigned int *count_out);
const char *cli_menus_own_word(const char *args);
// a press on the title `at`, its menu closed where it is open and else opened; a press on the row `at` of the menu at
// `level`, its items opened or it chosen, every menu closed: 1 with the row chosen into `chosen`, else 0; and a press
// on neither, every menu closed
void cli_menus_title_press(unsigned int at);
int cli_menus_row_press(unsigned int level, unsigned int at, CliItem *chosen);
// the pointer moved onto the title `at`, its menu opened where another title's is; and onto the row `at` of the menu
// at `level`, it marked and a rest begun that opens its items or closes a sibling's
void cli_menus_title_move(unsigned int at);
void cli_menus_row_move(unsigned int level, unsigned int at);
// the key `name` taken by the menus: Alt with a title's letter opens its menu; while one is open Up, Down, Home and End
// step the mark, Left and Right step between the menus, Enter and Space choose, Escape closes the last, Tab closes them
// all and a letter marks the next row it begins: 1 where the key was theirs, a row chosen into `chosen` with
// `*chose` 1
int cli_menus_key(const char *name, CliItem *chosen, int *chose);
// the rest's end taken where it has come: 1 where a menu opened or closed
int cli_menus_rest(void);
// whether `keys` names the key `name`, `Ctrl+` read as `Mod+` and the modifiers in any order; and the item of any menu
// whose keys are `name` and that can be chosen, or whose command is `name`, into `item`: 1, or 0 where none is
int cli_menus_keys_name(const char *keys, const char *name);
int cli_menus_keyed(const char *name, CliItem *item);
int cli_menus_named(const char *name, CliItem *item);

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
