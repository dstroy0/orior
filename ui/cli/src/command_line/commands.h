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

// the max menus, the max items of one menu, and the longest label, command, keys or words of one
#define CLI_MENUS_MAX 32u

#define CLI_ITEMS_MAX 512u

#define CLI_LABEL_MAX_LEN 256u

// One row of a menu: its label, the command it runs, the keys that run it in the window, the words it takes after its
// name, or for a part's own row the one word it runs with, what it needs before it can be chosen, the name of the state
// it shows on or off, empty where it is neither, 1 where the command line runs it in the terminal, 1 where it is a
// setting whose key is its chain of commands after its menu's word, joined by dots, 1 where every key its chain starts,
// a dot after it, is its own, 1 where its key stays on this machine and is never carried, 1 where only the reader's own
// settings.json sets it and a tree's never does, 1 where the command line runs it in a terminal where no window is
// open, whether it is a line, hidden, or filled in as its menu opens, and the list of the rows under it, 0 where it has
// none
typedef struct
{
    const char *label;
    const char *command;
    const char *keys;
    const char *args;
    const char *needs;
    const char *checks;
    int console;
    int setting;
    int each;
    int machine;
    int reader;
    int terminal;
    int line;
    int hidden;
    int fill;
    unsigned int list;
} CliItem;

// the menus open at once at max; a pointer's rest on a row before its items open or a sibling's close; and where
// a menu asked for and not opened under a title is: at its point, or at the terminal's key field
#define CLI_MENU_LEVELS 4u
#define CLI_MENU_REST_MS 180ULL
#define CLI_MENU_PLACE_POINT 0u
#define CLI_MENU_PLACE_TERMINAL_KEYS 1u

// the rows of a part's own menu built as the menu is asked for: how many at max
#define CLI_MENU_OWN_ROWS 24u

// One menu open: the list its rows are read from, 0 where they are the titles folded from the bar or a part's own rows;
// those own rows where a part hands them located, NULL for none; whether its rows are the part's own rows built in
// their cell; how many rows it has; the row marked and the row whose items are open under it, each -1 for none; and
// the number it took as it opened, a new one each time
typedef struct
{
    unsigned int list;
    const CliItem *own;
    int own_rows;
    unsigned int rows;
    int pointed;
    int opened;
    unsigned int serial;
} CliMenuOpen;

// The menus open: the title on the bar whose menu is open, -1 for none; how many titles the bar shows and whether the
// rest fold into a last; the menus open, one under another; where the first is where it was asked for and not under
// a title, at the place `place` names, its top left at x, y in the window's pixels where that is its point; the numbers
// the menus took; the rest the pointer is in: when it ends, 0 for none, the level of the row it rests on, and the row
// whose items open as it ends, -1 where it closes the items open beside a sibling; the window's answer to whether an
// item cannot be chosen, NULL where every item with a command can be; and its answer to the state an item shows by the
// name in its `checks`, 1 on and 0 off, NULL where every state is off
typedef struct
{
    int open;
    unsigned int displayed;
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
// located as long as the program runs
unsigned int cli_menu_titles(void);
// the menus: the title `at` and the menu `menu`; the row `at` of the open menu `level` into `item`, selectable,
// pressed into `chosen`, moved to; the menu `level` closed; the bar with `displayed` titles, `folded`; a title or
// a row opened, `marked`; the part's own `rows`, `count` of them, opened at `place`, `x`, `y`; an own row `item`
// added, named `command` with `item_args`, the popup `title`, the row `at` relabeled `label`, read into `item`, the
// word of `item_args`; the key `name`, `chose` where it chose; `keys` named `name`; the menu `menu`'s items, its
// `list`'s rows into `items` of `capacity`, `count` items taken and released, its list and whether displayed; and
// the command `command`'s own `keys`
typedef struct
{
    unsigned int at;
    unsigned int menu;
    unsigned int level;
    CliItem *item;
    CliItem *chosen;
    unsigned int displayed;
    int folded;
    int marked;
    const CliItem *rows;
    unsigned int count;
    unsigned int place;
    int x;
    int y;
    const char *command;
    const char *item_args;
    const char *title;
    const char *label;
    const char *name;
    int *chose;
    const char *keys;
    unsigned int list;
    CliItem *items;
    unsigned int capacity;
} CliMenusCfg;
unsigned int cli_menu_title_menu(const CliMenusCfg *args);
const char *cli_menu_title_text(const CliMenusCfg *args);
// the row `at` of the menu open at `level` read into `item`: 1, or 0 past its rows; and whether it can be marked: a
// row of items, or an item that can be chosen
int cli_menus_row(const CliMenusCfg *args);
int cli_menus_selectable(const CliMenusCfg *args);
// the menus from `level` on closed; the titles the bar shows, `displayed`, the rest folded into a last where `folded`
// is 1; the menu of the title `at` opened, the last where the titles fold located for those folded, every other closed,
// its first row that can be chosen marked where `marked` is 1; the items of the row `at` of the menu at `level` opened
// beside it the same way; and `count` rows of a part's own, located while they are open, or the part's own rows
// built in their cell where `rows` is NULL, opened as its menu at the place `place` names, at x, y where that is
// its point
void cli_menus_close(const CliMenusCfg *args);
void cli_menus_bar(const CliMenusCfg *args);
void cli_menus_title_open(const CliMenusCfg *args);
void cli_menus_row_open(const CliMenusCfg *args);
void cli_menus_own_open(const CliMenusCfg *args);
// a part's own rows, built as its menu is asked for and located while it is open: begun empty; `item` added after
// them, its texts the part's own, located while the menu is open; a line added; the catalog's item whose command is
// `command` added with `args`: 1, or 0 where the catalog has none; the rows of the popup menu titled `title` added,
// each item's args `args`, its lines retained: 1, or 0 where the list has no such popup; the label of the row `at`
// written anew; how many rows there are; the row `at` read into `item`, its label and args in the texts of the one row
// last read, which stand until the next row is read: 1, or 0 past the rows; and the word a chosen row's `args` runs
// with, NULL where the args are a catalog item's pattern or empty
void cli_menus_own_begin(void);
void cli_menus_own_row(const CliMenusCfg *args);
void cli_menus_own_line(void);
int cli_menus_own_named(const CliMenusCfg *args);
int cli_menus_popup(const CliMenusCfg *args);
void cli_menus_own_relabel(const CliMenusCfg *args);
unsigned int cli_menus_own_count(void);
int cli_menus_own_read(const CliMenusCfg *args);
const char *cli_menus_own_word(const CliMenusCfg *args);
// a press on the title `at`, its menu closed where it is open and else opened; a press on the row `at` of the menu at
// `level`, its items opened or it chosen, every menu closed: 1 with the row chosen into `chosen`, else 0; and a press
// on neither, every menu closed
void cli_menus_title_press(const CliMenusCfg *args);
int cli_menus_row_press(const CliMenusCfg *args);
// the pointer moved onto the title `at`, its menu opened where another title's is; and onto the row `at` of the menu
// at `level`, it marked and a rest begun that opens its items or closes a sibling's
void cli_menus_title_move(const CliMenusCfg *args);
void cli_menus_row_move(const CliMenusCfg *args);
// the key `name` taken by the menus: Alt with a title's character opens its menu; while one is open Up, Down, Home and
// End step the mark, Left and Right step between the menus, Enter and Space choose, Escape closes the last, Tab closes
// them all and a character marks the next row it begins: 1 where the key was theirs, a row chosen into `chosen` with
// `*chose` 1
int cli_menus_key(const CliMenusCfg *args);
// the rest's end taken where it has come: 1 where a menu opened or closed
int cli_menus_rest(void);
// whether `keys` names the key `name`, `Ctrl+` read as `Mod+` and the modifiers in any order; and the item of any menu
// whose keys are `name` and that can be chosen, or whose command is `name`, into `item`: 1, or 0 where none is
int cli_menus_keys_name(const CliMenusCfg *args);
int cli_menus_keyed(const CliMenusCfg *args);
int cli_menus_named(const CliMenusCfg *args);

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
// every item of the menu, those of its groups among them, in order: their count; and their count alone, the cell a
// caller sizes for them
unsigned int cli_menu_items(const CliCommandsMenuCapacityCfg *args);
unsigned int cli_menu_items_count(const CliMenusCfg *args);
// the rows of the list `list`, a row's own or a menu's, in order and as they are: their count; their count alone, the
// cell a caller sizes for them; the list of a menu's own rows; and whether a menu takes a title on the window's bar:
// it has rows, and it is neither the strip's nor a popup
unsigned int cli_menu_rows(const CliMenusCfg *args);
unsigned int cli_rows_count(const CliMenusCfg *args);
// a cell of `count` items for one read of a menu's items or a list's rows, taken from the temporary tier and given
// back after the read, the reader's own keys' text copied after the items so the items' keys point into it: NULL
// where the count is 0 or the tier refuses it. cli_menu_items and cli_menu_rows read into such a cell alone
CliItem *cli_items_alloc(const CliMenusCfg *args);
void cli_items_release(const CliMenusCfg *args);
// the list of the command line's own words, as `line` in the list names them, read by cli_menu_rows; 0 where it has
// none
unsigned int cli_line_list(void);
unsigned int cli_menu_list(const CliMenusCfg *args);
int cli_menu_displayed(const CliMenusCfg *args);
// the item of the menu whose command is `word`: 1, or 0 where it has none; and the words an item must be given, each
// `<word>` of its args outside a bracket
int cli_menu_command(const CliCommandsMenuCfg *args);
// the item among the rows under an item, its `list`, whose command is `word`, the next node of a chain: 1, or 0 where
// none is
int cli_item_command(const CliCommandsMenuCfg *args);
// the node of the settings, the rows of the menu CLI_SETTINGS_MENU, whose chain is the key `word`, its parts joined by
// dots, read into `item`, a node marked `each` the node of every key its chain starts: 1, or 0 where none is
#define CLI_SETTINGS_MENU "preferences"
int cli_setting_node(const CliCommandsMenuCfg *args);
// the node of any menu whose chain after the menu's word is `word`, read into `item`: 1, or 0 where none is
int cli_chain_item(const CliCommandsMenuCfg *args);
// the reader's own keys of `command`, the setting `keys.<command>`, positioned over the keys the list gives it from
// here on, in every item read of it
void cli_commands_keys_own(const CliMenusCfg *args);
unsigned int cli_item_words_need(const CliCommandsItemCfg *args);
// the menu whose word is `word`: 1, or 0 where none is
int cli_menu_by_name(const CliCommandsWordCfg *args);
// the list's text as read, for the window, its size into `size`
const char *cli_commands_text(const CliCommandsSizeCfg *args);

struct CliCommandsMenuCfg
{
    unsigned int menu;
    unsigned int list;
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
    int (*item_command)(const CliCommandsMenuCfg *args);
} CliCommandsMenuNs;
EMBED_TABLE_LAYOUT(CliCommandsMenuNs, menu_command, item_command);
EMBED_TABLE_STORAGE CliCommandsMenuNs cli_commands_menu EMBED_UNUSED = {
    .menu_command = cli_menu_command,
    .item_command = cli_item_command,
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
