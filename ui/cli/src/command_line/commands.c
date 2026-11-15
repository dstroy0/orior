// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the commands, as commands.json lists them: the window's menus, and the command line's words. A menu is a
// title and its items, and a menu of jobs names the catalog's groups it lists, split by what each job works on where
// `split` specifies so. An item is a label, the command it runs, the keys that run it in the window, the words it takes
// after its name, and whether the command line runs it itself, in the terminal, or opens the window to run it there.
// The window reads the same file for the rest of an item: what it needs before it can act, and a label that changes
// with the scheme. The list is built into the program, as the window builds in the same one, and read once
#include "commands.h"
#include "../../../../includes/formats/cfg_json/cfg_json.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "pool.h"

// the list as it is built in, with a zero after it, read where it lies; its tokens, a cell of their count for as long
// as the program runs; and each string token's text, read once, in one cell for as long as the program runs. The two
// cells are the process's own internal takes, persistent, taken once and never given back
static const char *s_text;
static unsigned long long s_size;
static CfgJsonToken *s_tokens;
static unsigned int s_count;
static const char **s_string;
// the token of each menu, in the order the list contains them, in a movable cell of their count
static const CarcerMovableEntry *s_menu;
static unsigned int s_menus;
// the array of the command line's own words, 0 where the list has none
static unsigned int s_line;

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
    // a list far below what a embed_word counts
    if (!cfg_json_parse_metadata(s_text, (embed_word)s_size, s_tokens, tokens, &parse))
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
        if (s_tokens[at].type == CFG_JSON_STRING)
        {
            (void)cfg_json_string(s_text, &s_tokens[at], string_text, (embed_word)string_left);
            const unsigned long long used =
                cellul.len(&(CatenaFinitaCfg){.src = string_text, .cap = string_left}) + 1ULL;
            s_string[at] = string_text;
            string_text += used;
            string_left -= used;
        }
    }
    const unsigned int menus = cfg_json_member(s_text, s_tokens, 0u, "menus");
    if ((menus == 0u) || (s_tokens[menus].type != CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, "commands.json contains no menus");
        s_count = 0u;
        return 0;
    }
    const unsigned int line = cfg_json_member(s_text, s_tokens, 0u, "line");
    s_line = ((line != 0u) && (s_tokens[line].type == CFG_JSON_ARRAY)) ? line : 0u;
    s_menu = ((s_menu == NULL) && (s_tokens[menus].count != 0u))
                 ? g_cli_guard->movable_buf_alloc((unsigned long long)s_tokens[menus].count * sizeof(unsigned int),
                                                  g_cli_command_line_owner)
                 : s_menu;
    if ((s_menu == NULL) && (s_tokens[menus].count != 0u))
    {
        mmgr_line_put(args->err_msg, "commands.json: the cells cannot meet its menus");
        s_count = 0u;
        return 0;
    }
    unsigned int at = menus + 1u;
    for (unsigned int each = 0u; each < s_tokens[menus].count; each += 1u)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_menu,
                                                                 .offset = (embed_word)s_menus * sizeof(unsigned int),
                                                                 .src = &at,
                                                                 .bytes = sizeof(unsigned int)});
        s_menus += 1u;
        at = s_tokens[at].next;
    }
    return 1;
}

unsigned int cli_menus(void)
{
    return s_menus;
}

unsigned int cli_line_list(void)
{
    return s_line;
}

typedef struct CommandsCell CommandsCell;

typedef struct
{
    unsigned int object;
    const char *name;
    char *out;
    embed_word capacity;
    unsigned int at;
    CliItem *item;
    unsigned int list;
    const char *word_text;
    CliItem *items;
    unsigned int count;
    unsigned int capacity_u32;
    const char *keys_tail;
    CommandsCell *held;
    unsigned long long bytes;
    const char *text;
    unsigned long long length;
    const char *command;
    unsigned int level;
    int from;
    int by;
    const CliItem *own;
    unsigned int rows;
    unsigned long long cap;
    CliItem *chosen;
    char character;
    const char *required;
    int *alt;
    int *modded;
    int *chose;
    unsigned int *mods;
    unsigned long long *rest;
    int by_command;
} CommandsCtx;
// the string member `name` of the object at token `object` into `out`, sized for `capacity`; empty where it has none
EMBED_INLINE void cli_member_text(const CommandsCtx *args)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, args->object, args->name);
    args->out[0] = '\0';
    if ((value != 0u) && (s_tokens[value].type == CFG_JSON_STRING))
    {
        cfg_json_string(s_text, &s_tokens[value], args->out, args->capacity);
    }
}

void cli_menu_title(const CliCommandsMenuTitleCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    EMBED_CALL(cli_member_text, CommandsCtx, .object = menu_token, .name = "title", .out = args->title,
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
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    EMBED_CALL(cli_member_text, CommandsCtx, .object = menu_token, .name = "split", .out = args->split,
               .capacity = CLI_LABEL_MAX_LEN);
}

unsigned int cli_menu_groups(const CliCommandsMenuGroupsCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int list = cfg_json_member(s_text, s_tokens, menu_token, "groups");
    if ((list == 0u) || (s_tokens[list].type != CFG_JSON_ARRAY))
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned int at = list + 1u;
    for (unsigned int each = 0u; (each < s_tokens[list].count) && (count < args->capacity); each += 1u)
    {
        if (s_tokens[at].type == CFG_JSON_STRING)
        {
            cfg_json_string(s_text, &s_tokens[at], args->groups[count], CLI_LABEL_MAX_LEN);
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

// the string member `name` of the object at token `object`, located as long as the program runs; empty where it has
// none
EMBED_INLINE const char *cli_member_string(const CommandsCtx *args)
{
    const unsigned int value = cfg_json_member(s_text, s_tokens, args->object, args->name);
    return ((value != 0u) && (s_tokens[value].type == CFG_JSON_STRING)) ? s_string[value] : "";
}

// A movable cell of records grown as records are added: the cell, how many bytes are used and its capacity
struct CommandsCell
{
    const CarcerMovableEntry *cell;
    unsigned long long used;
    unsigned long long capacity;
};

// `bytes` more of `held` claimed: where they start, or CLI_TEXT_NONE where the cells refuse the growth. A cell they
// outgrow is replaced by one of twice the capacity until they fit, its bytes copied through and the old given back
#define CLI_TEXT_NONE 0xffffffffffffffffULL
static unsigned long long commands_cell_claim(const CommandsCtx *args)
{
    if (args->held->used + args->bytes > args->held->capacity)
    {
        unsigned long long capacity = (args->held->capacity != 0ULL) ? args->held->capacity : 1024ULL;
        while (capacity < args->held->used + args->bytes)
        {
            capacity *= 2ULL;
        }
        const CarcerMovableEntry *const grown = g_cli_guard->movable_buf_alloc(capacity, g_cli_command_line_owner);
        if (grown == NULL)
        {
            return CLI_TEXT_NONE;
        }
        for (unsigned long long at = 0ULL; at < args->held->used; at += CLI_LABEL_MAX_LEN)
        {
            char piece[CLI_LABEL_MAX_LEN];
            const unsigned long long left = args->held->used - at;
            const unsigned long long size = (left < sizeof(piece)) ? left : sizeof(piece);
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = args->held->cell, .offset = (embed_word)at, .dst = piece, .bytes = (embed_word)size});
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = grown, .offset = (embed_word)at, .src = piece, .bytes = (embed_word)size});
        }
        if (args->held->cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(args->held->cell, g_cli_command_line_owner);
        }
        args->held->cell = grown;
        args->held->capacity = capacity;
    }
    const unsigned long long at = args->held->used;
    args->held->used += args->bytes;
    return at;
}

// `length` bytes of `text` and a zero after them written at the end of `held`: where they start, or CLI_TEXT_NONE
// where the cells refuse the growth
static unsigned long long commands_text_put(const CommandsCtx *args)
{
    const unsigned long long at =
        EMBED_CALL(commands_cell_claim, CommandsCtx, .held = args->held, .bytes = args->length + 1ULL);
    if (at == CLI_TEXT_NONE)
    {
        return at;
    }
    const char zero = '\0';
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = args->held->cell, .offset = (embed_word)at, .src = args->text, .bytes = (embed_word)args->length});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = args->held->cell, .offset = (embed_word)(at + args->length), .src = &zero, .bytes = 1u});
    return at;
}

// The reader's own keys, each a command and the keys `keys.<command>` in settings.json gives it, positioned over the
// keys the list gives the command: the pairs of where each command and its keys start in the keys' text, in a
// movable cell, how many pairs there are, and the keys' text in a movable cell of its own
typedef struct
{
    unsigned long long command_at;
    unsigned long long keys_at;
} CommandsOwnKey;

typedef struct
{
    CommandsCell pair;
    unsigned int count;
    CommandsCell text;
} CommandsOwnKeys;
static CommandsOwnKeys s_keys_own;

// where the keys the reader gives the command `command` start in the own keys' text, or CLI_TEXT_NONE where the
// reader gives it none
static unsigned long long commands_own_keys_at(const CommandsCtx *args)
{
    for (unsigned int at = 0u; at < s_keys_own.count; at += 1u)
    {
        CommandsOwnKey pair = {0ULL, 0ULL};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.pair.cell,
                                                                .offset = (embed_word)at * sizeof(pair),
                                                                .dst = &pair,
                                                                .bytes = sizeof(pair)});
        char named[CLI_LABEL_MAX_LEN];
        const unsigned long long named_measured = g_cli_guard->movable_buf_measure(
            &(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                .offset = (embed_word)pair.command_at,
                                .bytes = (embed_word)(s_keys_own.text.used - pair.command_at)});
        const unsigned long long named_length =
            (named_measured < sizeof(named)) ? named_measured : (sizeof(named) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                                                .offset = (embed_word)pair.command_at,
                                                                .dst = named,
                                                                .bytes = (embed_word)named_length});
        named[named_length] = '\0';
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->command, .cap = CLI_LABEL_MAX_LEN}))
        {
            return pair.keys_at;
        }
    }
    return CLI_TEXT_NONE;
}

void cli_commands_keys_own(const CliMenusCfg *args)
{
    const unsigned long long command_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->command, .cap = CLI_LABEL_MAX_LEN});
    const unsigned long long keys_length = cellul.len(&(CatenaFinitaCfg){.src = args->keys, .cap = CLI_LABEL_MAX_LEN});
    if ((command_length == 0ULL) || (keys_length == 0ULL))
    {
        return;
    }
    // the pair naming the args->command, where one does, else the count: the pair at `at`, read each in turn
    unsigned int at = 0u;
    CommandsOwnKey pair = {0ULL, 0ULL};
    int named_already = 0;
    while (!named_already && (at < s_keys_own.count))
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.pair.cell,
                                                                .offset = (embed_word)at * sizeof(pair),
                                                                .dst = &pair,
                                                                .bytes = sizeof(pair)});
        char named[CLI_LABEL_MAX_LEN];
        const unsigned long long named_measured = g_cli_guard->movable_buf_measure(
            &(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                .offset = (embed_word)pair.command_at,
                                .bytes = (embed_word)(s_keys_own.text.used - pair.command_at)});
        const unsigned long long named_length =
            (named_measured < sizeof(named)) ? named_measured : (sizeof(named) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                                                .offset = (embed_word)pair.command_at,
                                                                .dst = named,
                                                                .bytes = (embed_word)named_length});
        named[named_length] = '\0';
        named_already = cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->command, .cap = CLI_LABEL_MAX_LEN});
        at += named_already ? 0u : 1u;
    }
    const unsigned long long pair_at =
        (at == s_keys_own.count)
            ? EMBED_CALL(commands_cell_claim, CommandsCtx, .held = &s_keys_own.pair, .bytes = sizeof(CommandsOwnKey))
            : (unsigned long long)at * sizeof(CommandsOwnKey);
    const unsigned long long command_at = (at == s_keys_own.count)
                                              ? EMBED_CALL(commands_text_put, CommandsCtx, .held = &s_keys_own.text,
                                                           .text = args->command, .length = command_length)
                                              : pair.command_at;
    const unsigned long long keys_at =
        EMBED_CALL(commands_text_put, CommandsCtx, .held = &s_keys_own.text, .text = args->keys, .length = keys_length);
    if ((pair_at == CLI_TEXT_NONE) || (command_at == CLI_TEXT_NONE) || (keys_at == CLI_TEXT_NONE))
    {
        return;
    }
    pair = (CommandsOwnKey){command_at, keys_at};
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_keys_own.pair.cell, .offset = (embed_word)pair_at, .src = &pair, .bytes = sizeof(pair)});
    s_keys_own.count += (at == s_keys_own.count) ? 1u : 0u;
}

// The texts of the one row last read through cli_menus_row or cli_menus_own_read: its label, its args and its keys,
// copied out of the cells they stand in, which a move may move; the row's texts stand until the next row is read
static char s_row_label[CLI_LABEL_MAX_LEN];
static char s_row_args[CLI_PATH_MAX_LEN];
static char s_row_keys[CLI_LABEL_MAX_LEN];

// the item of the object at token `at` read into `item`
EMBED_INLINE void cli_item_read(const CommandsCtx *args)
{
    args->item->label = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "label");
    args->item->command = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "command");
    args->item->keys = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "keys");
    // the reader's own keys over the list's: in the items' cell's tail, a copy of the own keys' text, where the
    // item is one of a cell of items; else in the one row's keys
    const unsigned long long keys_at =
        (s_keys_own.count != 0u) ? EMBED_CALL(commands_own_keys_at, CommandsCtx, .command = args->item->command)
                                 : CLI_TEXT_NONE;
    if ((keys_at != CLI_TEXT_NONE) && (args->keys_tail != NULL))
    {
        args->item->keys = args->keys_tail + keys_at;
    }
    else if (keys_at != CLI_TEXT_NONE)
    {
        const unsigned long long keys_measured = g_cli_guard->movable_buf_measure(
            &(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                .offset = (embed_word)keys_at,
                                .bytes = (embed_word)(s_keys_own.text.used - keys_at)});
        const unsigned long long keys_length =
            (keys_measured < sizeof(s_row_keys)) ? keys_measured : (sizeof(s_row_keys) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                                                .offset = (embed_word)keys_at,
                                                                .dst = s_row_keys,
                                                                .bytes = (embed_word)keys_length});
        s_row_keys[keys_length] = '\0';
        args->item->keys = s_row_keys;
    }
    args->item->args = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "args");
    args->item->needs = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "needs");
    args->item->checks = EMBED_CALL(cli_member_string, CommandsCtx, .object = args->at, .name = "checks");
    const unsigned int console = cfg_json_member(s_text, s_tokens, args->at, "console");
    args->item->console = (console != 0u) && (s_tokens[console].type == CFG_JSON_TRUE);
    const unsigned int setting = cfg_json_member(s_text, s_tokens, args->at, "setting");
    args->item->setting = (setting != 0u) && (s_tokens[setting].type == CFG_JSON_TRUE);
    const unsigned int each = cfg_json_member(s_text, s_tokens, args->at, "each");
    args->item->each = (each != 0u) && (s_tokens[each].type == CFG_JSON_TRUE);
    const unsigned int machine = cfg_json_member(s_text, s_tokens, args->at, "machine");
    args->item->machine = (machine != 0u) && (s_tokens[machine].type == CFG_JSON_TRUE);
    const unsigned int reader = cfg_json_member(s_text, s_tokens, args->at, "reader");
    args->item->reader = (reader != 0u) && (s_tokens[reader].type == CFG_JSON_TRUE);
    const unsigned int terminal = cfg_json_member(s_text, s_tokens, args->at, "terminal");
    args->item->terminal = (terminal != 0u) && (s_tokens[terminal].type == CFG_JSON_TRUE);
    const unsigned int hidden = cfg_json_member(s_text, s_tokens, args->at, "hidden");
    args->item->hidden = (hidden != 0u) && (s_tokens[hidden].type == CFG_JSON_TRUE);
    args->item->fill = cfg_json_member(s_text, s_tokens, args->at, "fill") != 0u;
    const unsigned int nested = cfg_json_member(s_text, s_tokens, args->at, "items");
    args->item->list = ((nested != 0u) && (s_tokens[nested].type == CFG_JSON_ARRAY)) ? nested : 0u;
    args->item->line = 0;
}

// the item among the entries of the array at token `list`, those of its groups among them, whose command is `word`,
// read into `item`: 1, or 0 where none is
static int cli_flat_find(const CommandsCtx *args)
{
    unsigned int at = args->list + 1u;
    for (unsigned int each = 0u; each < s_tokens[args->list].count; each += 1u)
    {
        if (s_tokens[at].type == CFG_JSON_OBJECT)
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
            if ((command == 0u) && (nested != 0u) && (s_tokens[nested].type == CFG_JSON_ARRAY) &&
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
        if (s_tokens[at].type == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if ((command != 0u) && (count < args->capacity_u32))
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = &args->items[count],
                           .keys_tail = args->keys_tail);
                count += 1u;
            }
            else if ((command == 0u) && (nested != 0u) && (s_tokens[nested].type == CFG_JSON_ARRAY))
            {
                count = EMBED_CALL(cli_flat, CommandsCtx, .list = nested, .items = args->items, .count = count,
                                   .capacity_u32 = args->capacity_u32, .keys_tail = args->keys_tail);
            }
        }
        at = s_tokens[at].next;
    }
    return count;
}

// the count of the entries of the array at token `list`, those of its groups among them, that name a command
static unsigned int cli_flat_count(const CommandsCtx *args)
{
    unsigned int count = 0u;
    unsigned int at = args->list + 1u;
    for (unsigned int each = 0u; each < s_tokens[args->list].count; each += 1u)
    {
        if (s_tokens[at].type == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            count += (command != 0u) ? 1u : 0u;
            count += ((command == 0u) && (nested != 0u) && (s_tokens[nested].type == CFG_JSON_ARRAY))
                         ? EMBED_CALL(cli_flat_count, CommandsCtx, .list = nested)
                         : 0u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

unsigned int cli_menu_items_count(const CliMenusCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int list = cfg_json_member(s_text, s_tokens, menu_token, "items");
    return ((list != 0u) && (s_tokens[list].type == CFG_JSON_ARRAY))
               ? EMBED_CALL(cli_flat_count, CommandsCtx, .list = list)
               : 0u;
}

unsigned int cli_rows_count(const CliMenusCfg *args)
{
    return (args->list != 0u) ? s_tokens[args->list].count : 0u;
}

CliItem *cli_items_alloc(const CliMenusCfg *args)
{
    if (args->count == 0u)
    {
        return NULL;
    }
    // the items, and after them a copy of the own keys' text, which the items' keys point into
    CliItem *const items = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)args->count * sizeof(CliItem)) + s_keys_own.text.used, g_cli_command_line_owner);
    if ((items != NULL) && (s_keys_own.text.used != 0ULL))
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_keys_own.text.cell,
                                                                .dst = (void *)(items + args->count),
                                                                .bytes = (embed_word)s_keys_own.text.used});
    }
    return items;
}

void cli_items_release(const CliMenusCfg *args)
{
    g_cli_guard->persistent_buf_release(args->items, g_cli_command_line_owner);
}

unsigned int cli_menu_rows(const CliMenusCfg *args)
{
    unsigned int count = 0u;
    unsigned int at = args->list + 1u;
    // the own keys' text lies after the args->items of a cell of cli_items_alloc
    const char *const keys_tail = (const char *)(const void *)(args->items + args->capacity);
    for (unsigned int each = 0u; (args->list != 0u) && (each < s_tokens[args->list].count) && (count < args->capacity);
         each += 1u)
    {
        CliItem *const item = &args->items[count];
        if (s_tokens[at].type == CFG_JSON_OBJECT)
        {
            EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = item, .keys_tail = keys_tail);
            count += 1u;
        }
        else if (s_tokens[at].type == CFG_JSON_STRING)
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

unsigned int cli_menu_list(const CliMenusCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int list = cfg_json_member(s_text, s_tokens, menu_token, "items");
    return ((list != 0u) && (s_tokens[list].type == CFG_JSON_ARRAY)) ? list : 0u;
}

int cli_menu_displayed(const CliMenusCfg *args)
{
    const unsigned int list = cli_menu_list(&(CliMenusCfg){.menu = args->menu});
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int popup = cfg_json_member(s_text, s_tokens, menu_token, "popup");
    return (list != 0u) && (s_tokens[list].count != 0u) &&
           (cfg_json_member(s_text, s_tokens, menu_token, "strip") == 0u) &&
           !((popup != 0u) && (s_tokens[popup].type == CFG_JSON_TRUE));
}

unsigned int cli_menu_items(const CliCommandsMenuCapacityCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int list = cfg_json_member(s_text, s_tokens, menu_token, "items");
    // the own keys' text lies after the items of a cell of cli_items_alloc
    return ((list != 0u) && (s_tokens[list].type == CFG_JSON_ARRAY))
               ? EMBED_CALL(cli_flat, CommandsCtx, .list = list, .items = args->items, .count = 0u,
                            .capacity_u32 = args->capacity,
                            .keys_tail = (const char *)(const void *)(args->items + args->capacity))
               : 0u;
}

int cli_menu_command(const CliCommandsMenuCfg *args)
{
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    const unsigned int list = cfg_json_member(s_text, s_tokens, menu_token, "items");
    return (list != 0u) && (s_tokens[list].type == CFG_JSON_ARRAY) &&
           EMBED_CALL(cli_flat_find, CommandsCtx, .list = list, .word_text = args->word_text, .item = args->item);
}

int cli_item_command(const CliCommandsMenuCfg *args)
{
    return (args->list != 0u) && (s_tokens[args->list].type == CFG_JSON_ARRAY) &&
           EMBED_CALL(cli_flat_find, CommandsCtx, .list = args->list, .word_text = args->word_text, .item = args->item);
}

// the node among the rows of `list` and the rows under them whose chain is `word_text`, its parts joined by dots, read
// into `item`, a node marked `each` the node of every key its chain starts: 1, or 0 where none is
static int cli_chain_walk(const CommandsCtx *args)
{
    unsigned int list = args->list;
    const char *part = args->word_text;
    while (list != 0u)
    {
        // the key's next part, up to its next dot or its end
        char named[CLI_LABEL_MAX_LEN];
        const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = sizeof(named) - 1ULL});
        const char *const dot = memor.chr(&(MemoriaCfg){.src = part, .val = '.', .bytes = whole});
        const unsigned long long length = (dot != NULL) ? (unsigned long long)(dot - part) : whole;
        memor.cpy(&(MemoriaCfg){.dst = named, .src = part, .bytes = length});
        named[length] = '\0';
        if (!EMBED_CALL(cli_flat_find, CommandsCtx, .list = list, .word_text = named, .item = args->item))
        {
            return 0;
        }
        if ((part[length] == '\0') || args->item->each)
        {
            return 1;
        }
        list = args->item->list;
        part += length + 1ULL;
    }
    return 0;
}

int cli_setting_node(const CliCommandsMenuCfg *args)
{
    char why[CLI_LABEL_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned int menu = 0u;
    return cli_commands_read(&(CliCommandsCfg){.err_msg = &err_msg}) &&
           cli_menu_by_name(&(CliCommandsWordCfg){.word_text = CLI_SETTINGS_MENU, .menu_pointer = &menu}) &&
           EMBED_CALL(cli_chain_walk, CommandsCtx, .list = cli_menu_list(&(CliMenusCfg){.menu = menu}),
                      .word_text = args->word_text, .item = args->item);
}

int cli_chain_item(const CliCommandsMenuCfg *args)
{
    char why[CLI_LABEL_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!cli_commands_read(&(CliCommandsCfg){.err_msg = &err_msg}))
    {
        return 0;
    }
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        if (EMBED_CALL(cli_chain_walk, CommandsCtx, .list = cli_menu_list(&(CliMenusCfg){.menu = menu}),
                       .word_text = args->word_text, .item = args->item))
        {
            return 1;
        }
    }
    return 0;
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
        count += cli_menu_displayed(&(CliMenusCfg){.menu = at}) ? 1u : 0u;
    }
    return count;
}

unsigned int cli_menu_title_menu(const CliMenusCfg *args)
{
    unsigned int seen = 0u;
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        if (cli_menu_displayed(&(CliMenusCfg){.menu = menu}))
        {
            if (seen == args->at)
            {
                return menu;
            }
            seen += 1u;
        }
    }
    return 0u;
}

const char *cli_menu_title_text(const CliMenusCfg *args)
{
    if (args->menu >= s_menus)
    {
        return "";
    }
    unsigned int menu_token = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_menu,
                                                            .offset = (embed_word)args->menu * sizeof(unsigned int),
                                                            .dst = &menu_token,
                                                            .bytes = sizeof(menu_token)});
    return EMBED_CALL(cli_member_string, CommandsCtx, .object = menu_token, .name = "title");
}

// the row `at` of the list `list` as a menu shows it, a line or an item neither hidden nor filled in as the menu opens,
// read into `item`, where `item` is not NULL: 1, or 0 past them; and how many rows it shows
static int commands_list_row(const CommandsCtx *args)
{
    unsigned int seen = 0u;
    unsigned int token = args->list + 1u;
    for (unsigned int each = 0u; (args->list != 0u) && (each < s_tokens[args->list].count); each += 1u)
    {
        CliItem read;
        if (s_tokens[token].type == CFG_JSON_OBJECT)
        {
            EMBED_CALL(cli_item_read, CommandsCtx, .at = token, .item = &read);
        }
        else
        {
            read = (CliItem){.label = "", .command = "", .keys = "", .args = "", .needs = "", .checks = "", .line = 1};
        }
        const int shows = (s_tokens[token].type == CFG_JSON_STRING) ||
                          ((s_tokens[token].type == CFG_JSON_OBJECT) && !read.hidden && !read.fill);
        if (shows && (seen == args->at))
        {
            if (args->item != NULL)
            {
                *args->item = read;
            }
            return 1;
        }
        seen += shows ? 1u : 0u;
        token = s_tokens[token].next;
    }
    return 0;
}

static unsigned int commands_list_rows(const CommandsCtx *args)
{
    unsigned int count = 0u;
    while (EMBED_CALL(commands_list_row, CommandsCtx, .list = args->list, .at = count, .item = NULL))
    {
        count += 1u;
    }
    return count;
}

int cli_menus_row(const CliMenusCfg *args)
{
    const CliMenuOpen *const menu = &g_cli_menus_open.level[args->level];
    if ((args->level >= g_cli_menus_open.levels) || (args->at >= menu->rows))
    {
        return 0;
    }
    if (menu->own_rows)
    {
        return cli_menus_own_read(&(CliMenusCfg){.at = args->at, .item = args->item});
    }
    if (menu->own != NULL)
    {
        *args->item = menu->own[args->at];
        return 1;
    }
    if (menu->list != 0u)
    {
        return EMBED_CALL(commands_list_row, CommandsCtx, .list = menu->list, .at = args->at, .item = args->item);
    }
    // the titles folded from the bar, each a row of its menu's items
    const unsigned int title_menu = cli_menu_title_menu(&(CliMenusCfg){.at = g_cli_menus_open.displayed + args->at});
    *args->item = (CliItem){.label = cli_menu_title_text(&(CliMenusCfg){.menu = title_menu}),
                            .command = "",
                            .keys = "",
                            .args = "",
                            .needs = "",
                            .checks = "",
                            .list = cli_menu_list(&(CliMenusCfg){.menu = title_menu})};
    return 1;
}

int cli_menus_selectable(const CliMenusCfg *args)
{
    CliItem item;
    if (!cli_menus_row(&(CliMenusCfg){.level = args->level, .at = args->at, .item = &item}) || item.line)
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
static int commands_row_step(const CommandsCtx *args)
{
    // the rows are fewer than an int counts
    const int rows = (int)g_cli_menus_open.level[args->level].rows;
    int at = args->from;
    for (int step = 0; (rows != 0) && (step < rows); step += 1)
    {
        at = ((at + args->by) % rows + rows) % rows;
        if (cli_menus_selectable(&(CliMenusCfg){.level = args->level, .at = (unsigned int)at}))
        {
            return at;
        }
    }
    return -1;
}

// the menu at `level` opened on `list` or `own` rows, the next number taken
static void commands_level_open(const CommandsCtx *args)
{
    g_cli_menus_open.serials += 1u;
    g_cli_menus_open.level[args->level] =
        (CliMenuOpen){args->list, args->own, 0, args->rows, -1, -1, g_cli_menus_open.serials};
}

void cli_menus_close(const CliMenusCfg *args)
{
    g_cli_menus_open.rest_due_ms = 0ULL;
    if (args->level < g_cli_menus_open.levels)
    {
        g_cli_menus_open.levels = args->level;
        if (args->level != 0u)
        {
            g_cli_menus_open.level[args->level - 1u].opened = -1;
        }
    }
    if (args->level == 0u)
    {
        g_cli_menus_open.open = -1;
    }
}

void cli_menus_bar(const CliMenusCfg *args)
{
    g_cli_menus_open.displayed = args->displayed;
    g_cli_menus_open.folded = args->folded;
}

void cli_menus_title_open(const CliMenusCfg *args)
{
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    const unsigned int titles = cli_menu_titles();
    if (g_cli_menus_open.folded && (args->at >= g_cli_menus_open.displayed))
    {
        EMBED_CALL(commands_level_open, CommandsCtx, .level = 0u, .list = 0u, .own = NULL,
                   .rows = (titles > g_cli_menus_open.displayed) ? (titles - g_cli_menus_open.displayed) : 0u);
    }
    else
    {
        const unsigned int list =
            (args->at < titles)
                ? cli_menu_list(&(CliMenusCfg){.menu = cli_menu_title_menu(&(CliMenusCfg){.at = args->at})})
                : 0u;
        EMBED_CALL(commands_level_open, CommandsCtx, .level = 0u, .list = list, .own = NULL,
                   .rows = EMBED_CALL(commands_list_rows, CommandsCtx, .list = list));
    }
    g_cli_menus_open.levels = 1u;
    // the titles are fewer than an int counts
    g_cli_menus_open.open = (int)args->at;
    g_cli_menus_open.level[0].pointed =
        args->marked ? EMBED_CALL(commands_row_step, CommandsCtx, .level = 0u, .from = -1, .by = 1) : -1;
}

void cli_menus_row_open(const CliMenusCfg *args)
{
    CliItem item;
    if (!cli_menus_row(&(CliMenusCfg){.level = args->level, .at = args->at, .item = &item}) || (item.list == 0u) ||
        ((args->level + 1u) >= CLI_MENU_LEVELS))
    {
        return;
    }
    cli_menus_close(&(CliMenusCfg){.level = args->level + 1u});
    EMBED_CALL(commands_level_open, CommandsCtx, .level = args->level + 1u, .list = item.list, .own = NULL,
               .rows = EMBED_CALL(commands_list_rows, CommandsCtx, .list = item.list));
    g_cli_menus_open.levels = args->level + 2u;
    // the row is among the rows, which an int counts
    g_cli_menus_open.level[args->level].opened = (int)args->at;
    g_cli_menus_open.level[args->level + 1u].pointed =
        args->marked ? EMBED_CALL(commands_row_step, CommandsCtx, .level = args->level + 1u, .from = -1, .by = 1) : -1;
}

void cli_menus_own_open(const CliMenusCfg *args)
{
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    EMBED_CALL(commands_level_open, CommandsCtx, .level = 0u, .list = 0u, .own = args->rows, .rows = args->count);
    g_cli_menus_open.level[0].own_rows = args->rows == NULL;
    g_cli_menus_open.levels = 1u;
    g_cli_menus_open.place = args->place;
    g_cli_menus_open.x = args->x;
    g_cli_menus_open.y = args->y;
    g_cli_menus_open.level[0].pointed = EMBED_CALL(commands_row_step, CommandsCtx, .level = 0u, .from = -1, .by = 1);
}

// A row of a part's own, as built: the item, and where its label and its args stand in the own rows' text, each
// copied there since the part's texts may move or go while the menu is open, CLI_TEXT_NONE where the item has none
typedef struct
{
    CliItem item;
    unsigned long long label_at;
    unsigned long long args_at;
} CommandsOwnRow;

// A part's own rows as built: the rows in a movable cell, how many there are, and their texts in a movable cell of
// their own
typedef struct
{
    CommandsCell row;
    unsigned int count;
    CommandsCell text;
} CommandsOwnRows;
static CommandsOwnRows s_own;

// `text` copied to the end of the own rows' text: where the copy starts, or CLI_TEXT_NONE where `text` is NULL or
// the cells refuse the copy
static unsigned long long commands_own_text(const CommandsCtx *args)
{
    return (args->text != NULL)
               ? EMBED_CALL(commands_text_put, CommandsCtx, .held = &s_own.text, .text = args->text,
                            .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = args->cap}))
               : CLI_TEXT_NONE;
}

void cli_menus_own_begin(void)
{
    s_own.count = 0u;
    s_own.row.used = 0ULL;
    s_own.text.used = 0ULL;
}

void cli_menus_own_row(const CliMenusCfg *args)
{
    const unsigned long long at =
        EMBED_CALL(commands_cell_claim, CommandsCtx, .held = &s_own.row, .bytes = sizeof(CommandsOwnRow));
    if (at == CLI_TEXT_NONE)
    {
        return;
    }
    const CommandsOwnRow row = {
        *args->item, EMBED_CALL(commands_own_text, CommandsCtx, .text = args->item->label, .cap = CLI_LABEL_MAX_LEN),
        EMBED_CALL(commands_own_text, CommandsCtx, .text = args->item->args, .cap = CLI_PATH_MAX_LEN)};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_own.row.cell, .offset = (embed_word)at, .src = &row, .bytes = sizeof(row)});
    s_own.count += 1u;
}

void cli_menus_own_line(void)
{
    cli_menus_own_row(&(CliMenusCfg){
        .item = &(CliItem){.label = "", .command = "", .keys = "", .args = "", .needs = "", .checks = "", .line = 1}});
}

int cli_menus_own_named(const CliMenusCfg *args)
{
    CliItem item;
    if (!cli_menus_named(&(CliMenusCfg){.name = args->command, .item = &item}))
    {
        return 0;
    }
    item.args = args->item_args;
    cli_menus_own_row(&(CliMenusCfg){.item = &item});
    return 1;
}

int cli_menus_popup(const CliMenusCfg *args)
{
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        if (cli_menu_displayed(&(CliMenusCfg){.menu = menu}) ||
            !cellul.eq(&(CatenaFinitaCfg){.src = cli_menu_title_text(&(CliMenusCfg){.menu = menu}),
                                          .other = args->title,
                                          .cap = CLI_LABEL_MAX_LEN}))
        {
            continue;
        }
        const unsigned int list = cli_menu_list(&(CliMenusCfg){.menu = menu});
        const unsigned int rows = EMBED_CALL(commands_list_rows, CommandsCtx, .list = list);
        for (unsigned int at = 0u; at < rows; at += 1u)
        {
            CliItem item;
            (void)EMBED_CALL(commands_list_row, CommandsCtx, .list = list, .at = at, .item = &item);
            // a row runs with the part's word in place of the pattern the list gives
            item.args = item.line ? "" : args->item_args;
            cli_menus_own_row(&(CliMenusCfg){.item = &item});
        }
        return 1;
    }
    return 0;
}

void cli_menus_own_relabel(const CliMenusCfg *args)
{
    if (args->at >= s_own.count)
    {
        return;
    }
    CommandsOwnRow row;
    memor.set(&(MemoriaCfg){.dst = &row, .val = 0, .bytes = sizeof(row)});
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_own.row.cell, .offset = (embed_word)args->at * sizeof(row), .dst = &row, .bytes = sizeof(row)});
    row.label_at = EMBED_CALL(commands_own_text, CommandsCtx, .text = args->label, .cap = CLI_LABEL_MAX_LEN);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_own.row.cell, .offset = (embed_word)args->at * sizeof(row), .src = &row, .bytes = sizeof(row)});
}

unsigned int cli_menus_own_count(void)
{
    return s_own.count;
}

int cli_menus_own_read(const CliMenusCfg *args)
{
    if (args->at >= s_own.count)
    {
        return 0;
    }
    CommandsOwnRow row;
    memor.set(&(MemoriaCfg){.dst = &row, .val = 0, .bytes = sizeof(row)});
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_own.row.cell, .offset = (embed_word)args->at * sizeof(row), .dst = &row, .bytes = sizeof(row)});
    *args->item = row.item;
    if (row.label_at != CLI_TEXT_NONE)
    {
        const unsigned long long label_measured = g_cli_guard->movable_buf_measure(
            &(CarcerMovableCfg){.cell = s_own.text.cell,
                                .offset = (embed_word)row.label_at,
                                .bytes = (embed_word)(s_own.text.used - row.label_at)});
        const unsigned long long label_length =
            (label_measured < sizeof(s_row_label)) ? label_measured : (sizeof(s_row_label) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_own.text.cell,
                                                                .offset = (embed_word)row.label_at,
                                                                .dst = s_row_label,
                                                                .bytes = (embed_word)label_length});
        s_row_label[label_length] = '\0';
        args->item->label = s_row_label;
    }
    if (row.args_at != CLI_TEXT_NONE)
    {
        const unsigned long long args_measured =
            g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = s_own.text.cell,
                                                                 .offset = (embed_word)row.args_at,
                                                                 .bytes = (embed_word)(s_own.text.used - row.args_at)});
        const unsigned long long args_length =
            (args_measured < sizeof(s_row_args)) ? args_measured : (sizeof(s_row_args) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_own.text.cell,
                                                                .offset = (embed_word)row.args_at,
                                                                .dst = s_row_args,
                                                                .bytes = (embed_word)args_length});
        s_row_args[args_length] = '\0';
        args->item->args = s_row_args;
    }
    return 1;
}

const char *cli_menus_own_word(const CliMenusCfg *args)
{
    // the catalog's items carry the pattern of their args->item_args in the catalog's strings; a part's row carries the
    // word it runs with, which is in the part's own texts
    const char *const strings = (s_string != NULL) ? (const char *)(const void *)(s_string + s_count) : NULL;
    const int catalog = (strings != NULL) && ((embed_word)args->item_args >= (embed_word)strings) &&
                        ((embed_word)args->item_args < (embed_word)(strings + s_size + s_count));
    return ((args->item_args == NULL) || (args->item_args[0] == '\0') || catalog) ? NULL : args->item_args;
}

void cli_menus_title_press(const CliMenusCfg *args)
{
    // a title's place on the bar is far inside what an int represents
    if (g_cli_menus_open.open == (int)args->at)
    {
        cli_menus_close(&(CliMenusCfg){.level = 0u});
    }
    else
    {
        cli_menus_title_open(&(CliMenusCfg){.at = args->at, .marked = 1});
    }
}

// the row `at` of the menu at `level` chosen into `chosen`, every menu closed: 1 where it has a command
static int commands_choose(const CommandsCtx *args)
{
    const int read = cli_menus_row(&(CliMenusCfg){.level = args->level, .at = args->at, .item = args->chosen});
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    return read && (args->chosen->command[0] != '\0');
}

int cli_menus_row_press(const CliMenusCfg *args)
{
    CliItem item;
    if (!cli_menus_selectable(&(CliMenusCfg){.level = args->level, .at = args->at}) ||
        !cli_menus_row(&(CliMenusCfg){.level = args->level, .at = args->at, .item = &item}))
    {
        return 0;
    }
    if (item.list != 0u)
    {
        // a row's place is far inside what an int represents
        g_cli_menus_open.level[args->level].pointed = (int)args->at;
        cli_menus_row_open(&(CliMenusCfg){.level = args->level, .at = args->at, .marked = 1});
        return 0;
    }
    return EMBED_CALL(commands_choose, CommandsCtx, .level = args->level, .at = args->at, .chosen = args->chosen);
}

void cli_menus_title_move(const CliMenusCfg *args)
{
    // a title's place on the bar is far inside what an int represents
    if ((g_cli_menus_open.open >= 0) && ((int)args->at != g_cli_menus_open.open))
    {
        cli_menus_title_open(&(CliMenusCfg){.at = args->at, .marked = 0});
    }
}

void cli_menus_row_move(const CliMenusCfg *args)
{
    CliItem item;
    if ((args->level >= g_cli_menus_open.levels) ||
        !cli_menus_selectable(&(CliMenusCfg){.level = args->level, .at = args->at}) ||
        !cli_menus_row(&(CliMenusCfg){.level = args->level, .at = args->at, .item = &item}))
    {
        return;
    }
    CliMenuOpen *const menu = &g_cli_menus_open.level[args->level];
    // a row's place is far inside what an int represents
    const int index = (int)args->at;
    if (menu->pointed == index)
    {
        return;
    }
    menu->pointed = index;
    // the row's items open once the pointer has rested on it, unless they are open already; the items open beside a
    // sibling close once it has rested on a row without any; a rest begun before is let go
    const int more = item.list != 0u;
    const int opens = more && (menu->opened != index);
    const int closes = !more && (g_cli_menus_open.levels > (args->level + 1u));
    g_cli_menus_open.rest_level = args->level;
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
        cli_menus_row_open(&(CliMenusCfg){
            .level = g_cli_menus_open.rest_level, .at = (unsigned int)g_cli_menus_open.rest_index, .marked = 0});
    }
    else
    {
        cli_menus_close(&(CliMenusCfg){.level = g_cli_menus_open.rest_level + 1u});
    }
    return 1;
}

// a character in lower case
EMBED_INLINE char commands_lower(const CommandsCtx *args)
{
    return ((args->character >= 'A') && (args->character <= 'Z')) ? (char)(args->character - 'A' + 'a')
                                                                  : args->character;
}

// whether the key `name` is `required`
EMBED_INLINE int commands_is(const CommandsCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->required, .cap = 32ULL});
}

// the title on the bar whose character is the single character `name`: each title's character the first of its
// characters a to z no title before it has; -1 for none
static int commands_title_character(const CommandsCtx *args)
{
    if ((args->name[0] == '\0') || (args->name[1] != '\0'))
    {
        return -1;
    }
    const char required = EMBED_CALL(commands_lower, CommandsCtx, .character = args->name[0]);
    int taken[26] = {0};
    const unsigned int titles = cli_menu_titles();
    for (unsigned int at = 0u; (at < titles) && (at < g_cli_menus_open.displayed); at += 1u)
    {
        const char *const text =
            cli_menu_title_text(&(CliMenusCfg){.menu = cli_menu_title_menu(&(CliMenusCfg){.at = at})});
        char character = '\0';
        unsigned int byte = 0u;
        while ((character == '\0') && (text[byte] != '\0'))
        {
            const char lower = EMBED_CALL(commands_lower, CommandsCtx, .character = text[byte]);
            character = ((lower >= 'a') && (lower <= 'z') && !taken[lower - 'a']) ? lower : '\0';
            byte += 1u;
        }
        if (character == '\0')
        {
            continue;
        }
        taken[character - 'a'] = 1;
        if (character == required)
        {
            // the titles are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// a key's name read: whether Alt is down, whether any other modifier is, and the key's own name after them
static const char *commands_key_read(const CommandsCtx *args)
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

// the key `name` taken by the menu open last, `level`: Left and Right step between the menus or out of and into the
// items under a row, Enter and Space choose or open, a character marks the next row it begins
static int commands_level_key(const CommandsCtx *args)
{
    CliMenuOpen *const menu = &g_cli_menus_open.level[args->level];
    const unsigned int slots = g_cli_menus_open.displayed + (g_cli_menus_open.folded ? 1u : 0u);
    CliItem row;
    const int pointed =
        (menu->pointed >= 0) &&
        cli_menus_row(&(CliMenusCfg){.level = args->level, .at = (unsigned int)menu->pointed, .item = &row});
    if (EMBED_CALL(commands_is, CommandsCtx, .name = args->name, .required = "Right"))
    {
        if (pointed && (row.list != 0u))
        {
            cli_menus_row_open(&(CliMenusCfg){.level = args->level, .at = (unsigned int)menu->pointed, .marked = 1});
        }
        else if ((args->level == 0u) && (slots != 0u) && (g_cli_menus_open.open >= 0))
        {
            // the title open is among the titles
            cli_menus_title_open(&(CliMenusCfg){.at = ((unsigned int)g_cli_menus_open.open + 1u) % slots, .marked = 1});
        }
        return 1;
    }
    if (EMBED_CALL(commands_is, CommandsCtx, .name = args->name, .required = "Left"))
    {
        if (args->level > 0u)
        {
            cli_menus_close(&(CliMenusCfg){.level = args->level});
        }
        else if ((slots != 0u) && (g_cli_menus_open.open >= 0))
        {
            // the title open is among the titles
            cli_menus_title_open(
                &(CliMenusCfg){.at = ((unsigned int)g_cli_menus_open.open + slots - 1u) % slots, .marked = 1});
        }
        return 1;
    }
    if (EMBED_CALL(commands_is, CommandsCtx, .name = args->name, .required = "Enter") ||
        EMBED_CALL(commands_is, CommandsCtx, .name = args->name, .required = "Space"))
    {
        if (pointed && (row.list != 0u))
        {
            cli_menus_row_open(&(CliMenusCfg){.level = args->level, .at = (unsigned int)menu->pointed, .marked = 1});
        }
        else if (pointed &&
                 cli_menus_selectable(&(CliMenusCfg){.level = args->level, .at = (unsigned int)menu->pointed}))
        {
            *args->chose = EMBED_CALL(commands_choose, CommandsCtx, .level = args->level,
                                      .at = (unsigned int)menu->pointed, .chosen = args->chosen);
        }
        return 1;
    }
    if ((args->name[0] != '\0') && (args->name[1] == '\0'))
    {
        // a character marks the next row it begins, around the end; the rows are fewer than an int counts
        const char required = EMBED_CALL(commands_lower, CommandsCtx, .character = args->name[0]);
        const int rows = (int)menu->rows;
        for (int step = 1; step <= rows; step += 1)
        {
            const int at = ((menu->pointed + step) % rows + rows) % rows;
            CliItem item;
            if (cli_menus_selectable(&(CliMenusCfg){.level = args->level, .at = (unsigned int)at}) &&
                cli_menus_row(&(CliMenusCfg){.level = args->level, .at = (unsigned int)at, .item = &item}) &&
                (item.label[0] != '\0') &&
                (EMBED_CALL(commands_lower, CommandsCtx, .character = item.label[0]) == required))
            {
                menu->pointed = at;
                break;
            }
        }
    }
    return 1;
}

int cli_menus_key(const CliMenusCfg *args)
{
    *args->chose = 0;
    int alt = 0;
    int modded = 0;
    const char *const key =
        EMBED_CALL(commands_key_read, CommandsCtx, .name = args->name, .alt = &alt, .modded = &modded);
    if (alt && !modded)
    {
        const int title = EMBED_CALL(commands_title_character, CommandsCtx, .name = key);
        if (title >= 0)
        {
            // the title is among the titles
            cli_menus_title_open(&(CliMenusCfg){.at = (unsigned int)title, .marked = 1});
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
    if (EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Escape") ||
        EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Tab"))
    {
        cli_menus_close(
            &(CliMenusCfg){.level = EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Tab") ? 0u : level});
        return 1;
    }
    if (EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Down") ||
        EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Up"))
    {
        menu->pointed =
            EMBED_CALL(commands_row_step, CommandsCtx, .level = level, .from = menu->pointed,
                       .by = EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Down") ? 1 : -1);
        return 1;
    }
    if (EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Home") ||
        EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "End"))
    {
        // the rows are fewer than an int counts
        menu->pointed =
            EMBED_CALL(commands_is, CommandsCtx, .name = key, .required = "Home")
                ? EMBED_CALL(commands_row_step, CommandsCtx, .level = level, .from = -1, .by = 1)
                : EMBED_CALL(commands_row_step, CommandsCtx, .level = level, .from = (int)menu->rows, .by = -1);
        return 1;
    }
    return EMBED_CALL(commands_level_key, CommandsCtx, .name = key, .level = level, .chosen = args->chosen,
                      .chose = args->chose);
}

// the modifiers of a key's name read off its front, Ctrl and Mod the same bit, into `mods`, and the key's own name
// after them, its bytes into `rest`
static const char *commands_mods_read(const CommandsCtx *args)
{
    const char *name = args->name;
    unsigned long long length = args->length;
    static const struct
    {
        const char *mark;
        unsigned long long bytes;
        unsigned int bit;
    } s_marks[] = {{"Ctrl+", 5ULL, 1u}, {"Mod+", 4ULL, 1u}, {"Alt+", 4ULL, 2u}, {"Shift+", 6ULL, 4u}};
    *args->mods = 0u;
    for (unsigned int at = 0u; at < (sizeof(s_marks) / sizeof(s_marks[0]));)
    {
        if ((length >= s_marks[at].bytes) &&
            (memor.cmp(&(MemoriaCfg){.src = name, .other = s_marks[at].mark, .bytes = s_marks[at].bytes}) == 0))
        {
            *args->mods |= s_marks[at].bit;
            name += s_marks[at].bytes;
            length -= s_marks[at].bytes;
            at = 0u;
        }
        else
        {
            at += 1u;
        }
    }
    *args->rest = length;
    return name;
}

int cli_menus_keys_name(const CliMenusCfg *args)
{
    unsigned int keys_mods = 0u;
    unsigned long long keys_rest = 0ULL;
    const char *const keys_key =
        EMBED_CALL(commands_mods_read, CommandsCtx, .name = args->keys,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->keys, .cap = CLI_LABEL_MAX_LEN}),
                   .mods = &keys_mods, .rest = &keys_rest);
    unsigned int mods = 0u;
    unsigned long long rest = 0ULL;
    const char *const key = EMBED_CALL(commands_mods_read, CommandsCtx, .name = args->name,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = 64ULL}),
                                       .mods = &mods, .rest = &rest);
    return (keys_rest != 0ULL) && (keys_rest == rest) && (keys_mods == mods) &&
           (memor.cmp(&(MemoriaCfg){.src = keys_key, .other = key, .bytes = rest}) == 0);
}

// the item among the entries of the array at token `list`, those of its groups among them, whose keys are `name` and
// that can be chosen, or whose command is `name` where `by_command` is 1, read into `item`: 1, or 0 where none is
static int commands_flat_keyed(const CommandsCtx *args)
{
    unsigned int at = args->list + 1u;
    for (unsigned int each = 0u; each < s_tokens[args->list].count; each += 1u)
    {
        if (s_tokens[at].type == CFG_JSON_OBJECT)
        {
            const unsigned int command = cfg_json_member(s_text, s_tokens, at, "command");
            const unsigned int nested = cfg_json_member(s_text, s_tokens, at, "items");
            if (command != 0u)
            {
                EMBED_CALL(cli_item_read, CommandsCtx, .at = at, .item = args->item);
                const int matched =
                    args->by_command
                        ? cellul.eq(&(CatenaFinitaCfg){.src = args->item->command, .other = args->name, .cap = 64ULL})
                        : ((args->item->keys[0] != '\0') &&
                           cli_menus_keys_name(&(CliMenusCfg){.keys = args->item->keys, .name = args->name}) &&
                           ((g_cli_menus_open.disabled == NULL) || !g_cli_menus_open.disabled(args->item)));
                if (matched)
                {
                    return 1;
                }
            }
            else if ((nested != 0u) && (s_tokens[nested].type == CFG_JSON_ARRAY) &&
                     EMBED_CALL(commands_flat_keyed, CommandsCtx, .list = nested, .name = args->name,
                                .by_command = args->by_command, .item = args->item))
            {
                return 1;
            }
        }
        at = s_tokens[at].next;
    }
    return 0;
}

int cli_menus_keyed(const CliMenusCfg *args)
{
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        const unsigned int list = cli_menu_list(&(CliMenusCfg){.menu = menu});
        if ((list != 0u) && EMBED_CALL(commands_flat_keyed, CommandsCtx, .list = list, .name = args->name,
                                       .by_command = 0, .item = args->item))
        {
            return 1;
        }
    }
    return 0;
}

int cli_menus_named(const CliMenusCfg *args)
{
    for (unsigned int menu = 0u; menu < s_menus; menu += 1u)
    {
        const unsigned int list = cli_menu_list(&(CliMenusCfg){.menu = menu});
        if ((list != 0u) && EMBED_CALL(commands_flat_keyed, CommandsCtx, .list = list, .name = args->name,
                                       .by_command = 1, .item = args->item))
        {
            return 1;
        }
    }
    return 0;
}
