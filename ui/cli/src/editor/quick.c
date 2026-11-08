// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick.c: the quick open's whole logic. The typed text stands in a cell sized to it; each change of it asks the rows
// anew: they are written into a cell for that ask alone, at most CLI_QUICK_MOST of them, then into one cell sized to
// them, each row a set of offsets into the cell's text and its marks. The menus' commands are read once into a cell
// sized to them; the best rows of a type are found in one pass into a bounded list in score order, a row's place in it
// found by a binary search
#include "quick.h"

#include "../code/symbols.h"
#include "../command_line/cli_internal.h"
#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../machine/recent.h"
#include "files.h"
#include "view.h"

// the sentences of the rows that go nowhere, the details, and the field's sentences while the text is empty
#define QUICK_READING "Reading the tree's files: more may be found."
#define QUICK_LINE_NONE "Open a file to go to a line in it."
#define QUICK_LINE_ASKED "Type a line number from 1 to "
#define QUICK_LINE_ASKED_END " to go to it."
#define QUICK_LINE_GO "Go to line "
#define QUICK_LINE_COLUMN ", column "
#define QUICK_SYMBOLS_NONE "Open a file to go to what it declares."
#define QUICK_TREE_NONE "Open a tree to go to what its files declare."
#define QUICK_OPENED "recently opened"
#define QUICK_USED "recently used"
#define QUICK_LINE_AT ", line "
#define QUICK_NO_COMMANDS "No matching commands"
#define QUICK_NO_RESULTS "No matching results"
#define QUICK_FILES "Files"
#define QUICK_SYMBOLS "Symbols"
#define QUICK_ACTIONS "Actions"
#define QUICK_PLACEHOLDER_EVERYWHERE "Search everywhere: files, symbols and actions"
#define QUICK_PLACEHOLDER_COMMANDS "Type the name of a command to run."
#define QUICK_PLACEHOLDER_FILES                                                                                        \
    "Search files by name (append :line to go to a line, or type > for commands, : for a line, @ for a symbol here, "  \
    "# for one in the tree)"

// the most bytes of one row's texts, the most commands the menus carry, the most lists of rows read at once, and the
// score a command run lately has over one run before it with nothing typed
#define QUICK_ROW_BYTES (3u * CLI_PATH_MAX_LEN)
#define QUICK_COMMANDS 2048u
#define QUICK_LISTS 256u
#define QUICK_RECENT_SCORE 1000.0

// the commands that open the quick open, what each puts first in the typed text, and the one that opens Search
// Everywhere
#define QUICK_OPENERS 6u
static const char *const s_openers[QUICK_OPENERS] = {"file",   "palette",     "line",
                                                     "symbol", "symbol-tree", "search-everywhere"};
static const char *const s_prefixes[QUICK_OPENERS] = {"", ">", ":", "@", "#", ""};
#define QUICK_OPENER_EVERYWHERE 5u

// the keys that edit the typed text: Backspace, Delete, Left, Right, Home, End and the paste
#define QUICK_KEYS 7u

// A row as it stands in its cell: its type; its label, detail and keys, each an offset into the cell's text with its
// bytes; its marks, the first an offset into the cell's marks, the label's pairs then the detail's; what it opens, a
// path or a command word, an offset with its bytes; and the place in the path where `placed`
typedef struct
{
    unsigned int type;
    unsigned int label;
    unsigned int label_length;
    unsigned int detail;
    unsigned int detail_length;
    unsigned int keys;
    unsigned int keys_length;
    unsigned int marks;
    unsigned int label_marks;
    unsigned int detail_marks;
    unsigned int target;
    unsigned int target_length;
    unsigned int line;
    unsigned int column;
    int placed;
} QuickRow;

// A command of the menus: the title of its menu, how many letters the title has, and the item
typedef struct
{
    const char *title;
    unsigned int title_letters;
    CliItem item;
} QuickCommand;

// A row in a bounded list: its score and its place among what it was found in
typedef struct
{
    double score;
    unsigned int at;
} QuickTop;

// The rows of one ask as they are written, in a cell for that ask alone: the rows and how many, the marks and how many
// numbers of them, the text, and whether the tree is still being read
typedef struct
{
    QuickRow *row;
    unsigned int rows;
    unsigned int *mark;
    unsigned int marks;
    VerbaLinea text;
    int reading;
} QuickBuild;

// The quick open: whether it is open and is Search Everywhere; the tree, and the shown view's key and its file's path,
// empty for none; the typed text in a cell sized to it with its end, its bytes and the caret's byte; the rows' cell,
// the rows, their marks and text, and how many; the chosen row, the rows scrolled off the top and the rows the box
// shows; whether the rows were asked while the tree was read and the millisecond they were; the first Shift's
// millisecond, 0 for none; the menus' commands in a cell sized to them, how many, and whether they were read; and the
// texts of the last choice
typedef struct
{
    int open;
    int everywhere;
    const char *root;
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char *text;
    unsigned long long length;
    unsigned long long caret;
    unsigned char *cell;
    const QuickRow *row;
    const unsigned int *mark;
    const char *row_text;
    unsigned int rows;
    unsigned int chosen;
    unsigned int scroll;
    unsigned int shown;
    int reading;
    unsigned long long asked_ms;
    unsigned long long shift_ms;
    QuickCommand *command;
    unsigned int commands;
    int commands_read;
    char chosen_path[CLI_PATH_MAX_LEN];
    char chosen_command[CLI_LABEL_MAX_LEN];
    char chosen_keys[CLI_LABEL_MAX_LEN];
} Quick;

static Quick s_quick;

// the bounded list and the places of a match's letters, for one ask at a time on the view's thread
static QuickTop s_top[CLI_QUICK_MOST];
static unsigned int s_hit[CLI_FUZZY_LETTERS];

typedef struct
{
    QuickBuild *build;
    QuickRow *row;
    const char *text;
    unsigned long long length;
    const unsigned int *hits;
    unsigned int hit_count;
    unsigned int from;
    unsigned int at;
    unsigned int type;
    unsigned int most;
    unsigned int count;
    int grouped;
    int keep;
    int opened;
    int in_tree;
    double score;
    unsigned long long now_ms;
    unsigned long long first;
    unsigned long long last;
    const char *query;
    const char *type_text;
    const char *path;
    char *label_out;
    unsigned long long *path_length;
    unsigned long long *line;
    unsigned long long *column;
} QuickCtx;

// how many letters `length` bytes of UTF-8 `text` hold: its lead bytes
EMBED_INLINE unsigned int quick_letters(const QuickCtx *args)
{
    unsigned int letters = 0u;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        letters += (((unsigned char)args->text[at] & 0xC0u) != 0x80u) ? 1u : 0u;
    }
    return letters;
}

// the row `at` with `score` put in the bounded list of `count` rows, at most `most`, after every row scoring as much or
// more, its place found by a binary search, the last let go where the list is full: the list's count
EMBED_INLINE unsigned int quick_put(const QuickCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_top[middle].score >= args->score)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if (low >= args->most)
    {
        return args->count;
    }
    const unsigned int count = (args->count < args->most) ? (args->count + 1u) : args->most;
    for (unsigned int at = count - 1u; at > low; at -= 1u)
    {
        s_top[at] = s_top[at - 1u];
    }
    s_top[low] = (QuickTop){args->score, args->at};
    return count;
}

// a row of `type` begun after the build's rows, its texts empty at the text's end and its marks after the build's:
// the row, or NULL where the rows are CLI_QUICK_MOST
EMBED_INLINE QuickRow *quick_row_begun(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    if (build->rows >= CLI_QUICK_MOST)
    {
        return NULL;
    }
    QuickRow *const row = &build->row[build->rows];
    build->rows += 1u;
    // the build's text is CLI_QUICK_MOST rows of QUICK_ROW_BYTES at the most, far inside an unsigned int
    const unsigned int end = (unsigned int)build->text.at;
    *row = (QuickRow){args->type, end, 0u, end, 0u, end, 0u, build->marks, 0u, 0u, end, 0u, 0u, 0u, 0};
    return row;
}

// the bytes written to the build's text since its offset `at`
EMBED_INLINE unsigned int quick_written(const QuickCtx *args)
{
    // the build's text is far inside an unsigned int, as quick_row_begun reads it
    return (unsigned int)args->build->text.at - args->at;
}

// the marks of the letters `hits`, `hit_count` of them in order, that stand in the `length` bytes of the build's text
// at `at`, whose first letter is the letter `from` of what the hits count in, put after the build's marks as pairs of a
// byte offset from `at` and bytes, a run of letters side by side one pair, at most `most` pairs: how many
EMBED_INLINE unsigned int quick_marked(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    const char *const text = build->text.out + args->at;
    unsigned int hit = 0u;
    while ((hit < args->hit_count) && (args->hits[hit] < args->from))
    {
        hit += 1u;
    }
    unsigned int letter = args->from;
    unsigned int pairs = 0u;
    int marking = 0;
    unsigned int start = 0u;
    unsigned int end = 0u;
    // a row's text is QUICK_ROW_BYTES at the most, far inside an unsigned int
    const unsigned int length = (unsigned int)args->length;
    for (unsigned int byte = 0u; byte <= length; byte += 1u)
    {
        const int lead = (byte == length) || (((unsigned char)text[byte] & 0xC0u) != 0x80u);
        if (lead)
        {
            const int marked = (byte < length) && (hit < args->hit_count) && (args->hits[hit] == letter);
            hit += marked ? 1u : 0u;
            letter += 1u;
            if (marking && !marked && (pairs < args->most))
            {
                build->mark[build->marks + (2u * pairs)] = start;
                build->mark[build->marks + (2u * pairs) + 1u] = end - start;
                pairs += 1u;
            }
            start = (marked && !marking) ? byte : start;
            marking = marked;
        }
        end = marking ? (byte + 1u) : end;
    }
    build->marks += 2u * pairs;
    return pairs;
}

// a row that goes nowhere, of `type`, reading `text`
EMBED_INLINE void quick_said(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = args->type);
    if (row == NULL)
    {
        return;
    }
    mmgr_line_put(&build->text, args->text);
    row->label_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->label);
}

// a file's row for `length` bytes of `text`, its path, with the places `hits` of its letters, `hit_count` of them, at
// the place `line` and `column` from 1 where `line` is not 0, and the detail QUICK_OPENED after its folder where it is
// one of the files `opened` last: its name the label, its folder the detail, the path what it opens
EMBED_INLINE void quick_file_row(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = CLI_QUICK_ROW_FILE);
    if (row == NULL)
    {
        return;
    }
    unsigned long long name = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        name = (args->text[at] == '/') ? (at + 1ULL) : name;
    }
    const unsigned int cut = EMBED_CALL(quick_letters, QuickCtx, .text = args->text, .length = name);
    mmgr_line_put_n(&build->text, args->text, args->length);
    row->target_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->target);
    // the name stands within the path, which is a row's text at the most
    row->label = row->target + (unsigned int)name;
    row->label_length = row->target_length - (unsigned int)name;
    row->label_marks =
        EMBED_CALL(quick_marked, QuickCtx, .build = build, .at = row->label, .length = row->label_length,
                   .hits = args->hits, .hit_count = args->hit_count, .from = cut, .most = CLI_QUICK_MARKS);
    // the detail: the folder before the name's slash, then what the row was found among
    row->detail = (unsigned int)build->text.at;
    const unsigned long long folder = (name != 0ULL) ? (name - 1ULL) : 0ULL;
    mmgr_line_put_n(&build->text, args->text, folder);
    row->detail_marks =
        EMBED_CALL(quick_marked, QuickCtx, .build = build, .at = row->detail, .length = folder, .hits = args->hits,
                   .hit_count = args->hit_count, .from = 0u, .most = CLI_QUICK_MARKS - row->label_marks);
    if (args->opened)
    {
        mmgr_line_put(&build->text, (folder != 0ULL) ? "  " QUICK_OPENED : QUICK_OPENED);
    }
    row->detail_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->detail);
    // the place is the typed number's, which an unsigned int carries
    row->placed = *args->line != 0ULL;
    row->line = row->placed ? (unsigned int)(*args->line - 1ULL) : 0u;
    row->column = (row->placed && (*args->column != 0ULL)) ? (unsigned int)(*args->column - 1ULL) : 0u;
}

// the place that ends `length` bytes of `text`: a colon and digits, or two, cut off its end, the path's bytes before
// them into `path_length` and the numbers into `line` and `column`, 0 for none
EMBED_INLINE void quick_place(const QuickCtx *args)
{
    unsigned long long end = args->length;
    unsigned long long numbers[2] = {0ULL, 0ULL};
    unsigned int found = 0u;
    for (unsigned int each = 0u; each < 2u; each += 1u)
    {
        unsigned long long digits = end;
        unsigned long long value = 0ULL;
        unsigned long long scale = 1ULL;
        while ((digits > 0ULL) && (args->text[digits - 1ULL] >= '0') && (args->text[digits - 1ULL] <= '9') &&
               ((end - digits) < 9ULL))
        {
            // a digit's value is below ten
            value += (unsigned long long)(args->text[digits - 1ULL] - '0') * scale;
            scale *= 10ULL;
            digits -= 1ULL;
        }
        if ((digits == end) || (digits == 0ULL) || (args->text[digits - 1ULL] != ':'))
        {
            break;
        }
        numbers[found] = value;
        found += 1u;
        end = digits - 1ULL;
    }
    *args->path_length = end;
    *args->line = (found == 2u) ? numbers[1] : numbers[0];
    *args->column = (found == 2u) ? numbers[0] : 0ULL;
}

// the files' rows for `length` bytes of `text`, a path and a place after it: with no path the files opened last, else
// the tree's files that answer it, at most `most`, and the reading row while the tree is read
EMBED_INLINE void quick_files(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    unsigned long long path_length = 0ULL;
    unsigned long long line = 0ULL;
    unsigned long long column = 0ULL;
    EMBED_CALL(quick_place, QuickCtx, .text = args->text, .length = args->length, .path_length = &path_length,
               .line = &line, .column = &column);
    const char *recent[CLI_RECENT_MOST];
    const unsigned int recents = EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_FILES,
                                            .most = CLI_RECENT_MOST, .rows_out = recent);
    if (path_length == 0ULL)
    {
        for (unsigned int at = 0u; (at < recents) && (at < args->most); at += 1u)
        {
            EMBED_CALL(quick_file_row, QuickCtx, .build = build, .text = recent[at],
                       .length = cellul.len(&(CatenaFinitaCfg){.src = recent[at], .cap = CLI_PATH_MAX_LEN}),
                       .opened = 1, .line = &line, .column = &column);
        }
        return;
    }
    char query[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea typed = {query, sizeof(query), 0ULL};
    mmgr_line_put_n(&typed, args->text, path_length);
    (void)mmgr_line_finish(&typed);
    CliFilesRanked ranked = {NULL, 0u, NULL, NULL, 0};
    (void)EMBED_CALL(cli_files_root_rows.ranked_find, CliFilesRootRowsCfg, .root = s_quick.root, .rows = &ranked,
                     .query = query, .recent = recent, .recents = recents, .most = args->most, .fuzzy = 1,
                     .owner = g_cli_quick_owner);
    for (unsigned int at = 0u; at < ranked.count; at += 1u)
    {
        const CliFileRanked *const one = &ranked.row[at];
        EMBED_CALL(quick_file_row, QuickCtx, .build = build, .text = one->path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_PATH_MAX_LEN}),
                   .hits = ranked.hit + one->hit_first, .hit_count = one->hit_count, .from = 0u, .line = &line,
                   .column = &column);
    }
    if (ranked.reading)
    {
        build->reading = 1;
        EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_READING, .text = QUICK_READING);
    }
    g_cli_guard->persistent_buf_release(ranked.cell, g_cli_quick_owner);
}

// the menus' commands read once into a cell sized to them, every item of a menu the bar shows that runs a command,
// the items of its rows of items among them, each with its menu's title
EMBED_INLINE void quick_commands_read(const QuickCtx *args)
{
    (void)args;
    const unsigned int menus = cli_menus();
    if (s_quick.commands_read || (menus == 0u))
    {
        return;
    }
    // the lists still to read with their menus, one list's rows, and the commands found, for this read alone
    const unsigned long long lists_bytes = (unsigned long long)QUICK_LISTS * sizeof(unsigned int);
    const unsigned long long items_bytes = (unsigned long long)CLI_ITEMS_MAX * sizeof(CliItem);
    const unsigned long long found_bytes = (unsigned long long)QUICK_COMMANDS * sizeof(QuickCommand);
    unsigned char *const scratch =
        g_cli_guard->temporary_buf_alloc(found_bytes + items_bytes + lists_bytes, g_cli_quick_owner);
    if (scratch == NULL)
    {
        return;
    }
    // the commands lie first in the scratch, which its alignment saves aligned, the items after them and the lists last
    QuickCommand *const found = (QuickCommand *)(void *)scratch;
    CliItem *const items = (CliItem *)(void *)(scratch + found_bytes);
    unsigned int *const lists = (unsigned int *)(void *)(scratch + found_bytes + items_bytes);
    unsigned int count = 0u;
    for (unsigned int menu = 0u; menu < menus; menu += 1u)
    {
        if (!cli_menu_shown(menu))
        {
            continue;
        }
        const char *const title = cli_menu_title_text(menu);
        const unsigned int title_letters =
            EMBED_CALL(quick_letters, QuickCtx, .text = title,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_LABEL_MAX_LEN}));
        unsigned int pending = 1u;
        lists[0] = cli_menu_list(menu);
        while (pending != 0u)
        {
            pending -= 1u;
            const unsigned int rows = cli_menu_rows(lists[pending], items, CLI_ITEMS_MAX);
            for (unsigned int at = 0u; at < rows; at += 1u)
            {
                const CliItem *const item = &items[at];
                if ((item->list != 0u) && (pending < QUICK_LISTS))
                {
                    lists[pending] = item->list;
                    pending += 1u;
                }
                else if ((item->list == 0u) && !item->line && !item->hidden && !item->fill &&
                         (item->command[0] != '\0') && (count < QUICK_COMMANDS))
                {
                    found[count] = (QuickCommand){title, title_letters, *item};
                    count += 1u;
                }
            }
        }
    }
    QuickCommand *const cell =
        (count != 0u) ? g_cli_guard->persistent_buf_alloc(count * sizeof(QuickCommand), g_cli_quick_owner) : NULL;
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = found, .bytes = count * sizeof(QuickCommand)});
        s_quick.command = cell;
        s_quick.commands = count;
        s_quick.commands_read = 1;
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_quick_owner);
}

// the label of the command `at`, its menu's title, a colon and its own label, into `label_out`, sized for two labels:
// its bytes
EMBED_INLINE unsigned long long quick_command_label(const QuickCtx *args)
{
    const QuickCommand *const command = &s_quick.command[args->at];
    VerbaLinea label = {args->label_out, 2ULL * CLI_LABEL_MAX_LEN, 0ULL};
    mmgr_line_put(&label, command->title);
    mmgr_line_put(&label, ": ");
    mmgr_line_put(&label, command->item.label);
    return mmgr_line_finish(&label);
}

// the commands' rows for `length` bytes of `text` typed, at most `most`, the best match first, and with nothing typed
// the ones run last first, each with its keys
EMBED_INLINE void quick_commands(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    EMBED_CALL(quick_commands_read, QuickCtx, .build = build);
    char query[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea typed = {query, sizeof(query), 0ULL};
    mmgr_line_put_n(&typed, args->text, args->length);
    (void)mmgr_line_finish(&typed);
    const char *recent[CLI_RECENT_MOST];
    const unsigned int recents = EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_COMMANDS,
                                            .most = CLI_RECENT_MOST, .rows_out = recent);
    char label[2u * CLI_LABEL_MAX_LEN];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_quick.commands; at += 1u)
    {
        const CliItem *const item = &s_quick.command[at].item;
        if ((g_cli_menus_open.disabled != NULL) && g_cli_menus_open.disabled(item))
        {
            continue;
        }
        unsigned int ran = CLI_RECENT_MOST;
        for (unsigned int each = 0u; (ran == CLI_RECENT_MOST) && (each < recents); each += 1u)
        {
            ran = cellul.eq(&(CatenaFinitaCfg){.src = recent[each], .other = item->command, .cap = CLI_LABEL_MAX_LEN})
                      ? each
                      : CLI_RECENT_MOST;
        }
        double score = (ran < CLI_RECENT_MOST) ? (QUICK_RECENT_SCORE - (double)ran) : 0.0;
        unsigned int hits = 0u;
        if (args->length != 0ULL)
        {
            (void)EMBED_CALL(quick_command_label, QuickCtx, .at = at, .label_out = label);
            if (!EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = label,
                            .from_u64 = s_quick.command[at].title_letters + 2ULL, .score = &score, .hits = s_hit,
                            .count_pointer = &hits))
            {
                continue;
            }
        }
        count = EMBED_CALL(quick_put, QuickCtx, .at = at, .score = score, .count = count, .most = args->most);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const QuickCommand *const command = &s_quick.command[s_top[at].at];
        QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = CLI_QUICK_ROW_COMMAND);
        if (row == NULL)
        {
            return;
        }
        const unsigned long long length =
            EMBED_CALL(quick_command_label, QuickCtx, .at = s_top[at].at, .label_out = label);
        unsigned int hits = 0u;
        double score = 0.0;
        if (args->length != 0ULL)
        {
            (void)EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = label,
                             .from_u64 = command->title_letters + 2ULL, .score = &score, .hits = s_hit,
                             .count_pointer = &hits);
        }
        mmgr_line_put_n(&build->text, label, length);
        row->label_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->label);
        row->label_marks =
            EMBED_CALL(quick_marked, QuickCtx, .build = build, .at = row->label, .length = row->label_length,
                       .hits = s_hit, .hit_count = hits, .from = 0u, .most = CLI_QUICK_MARKS);
        row->detail = (unsigned int)build->text.at;
        if ((args->length == 0ULL) && (s_top[at].score >= (QUICK_RECENT_SCORE - (double)CLI_RECENT_MOST)))
        {
            mmgr_line_put(&build->text, QUICK_USED);
        }
        row->detail_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->detail);
        row->keys = (unsigned int)build->text.at;
        mmgr_line_put(&build->text, command->item.keys);
        row->keys_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->keys);
        row->target = (unsigned int)build->text.at;
        mmgr_line_put(&build->text, command->item.command);
        row->target_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->target);
    }
}

// the line's row for `length` bytes of `text` typed, `line` or `line:column` from 1: the line, no further than the
// shown view's last, and the column, or a sentence where no view is shown or no line is typed
EMBED_INLINE void quick_line(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    CliShape shape = {0ULL, 0ULL};
    if ((s_quick.key[0] == '\0') || (s_quick.path[0] == '\0') ||
        !EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = s_quick.key, .shape = &shape))
    {
        EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_TEXT, .text = QUICK_LINE_NONE);
        return;
    }
    // the line, then the column after a colon, each of its digits
    unsigned long long numbers[2] = {0ULL, 0ULL};
    unsigned int found = 0u;
    int whole = args->length != 0ULL;
    for (unsigned long long at = 0ULL; whole && (at < args->length); at += 1ULL)
    {
        const char letter = args->text[at];
        const int digit = (letter >= '0') && (letter <= '9') && (numbers[found] < 1000000000ULL);
        const int colon = (letter == ':') && (found == 0u) && (at != 0ULL);
        // a digit's value is below ten
        numbers[found] = digit ? ((numbers[found] * 10ULL) + (unsigned long long)(letter - '0')) : numbers[found];
        found += colon ? 1u : 0u;
        whole = digit || colon;
    }
    const unsigned long long lines = (shape.lines != 0ULL) ? shape.lines : 1ULL;
    if (!whole || (numbers[0] == 0ULL))
    {
        QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = CLI_QUICK_ROW_TEXT);
        if (row != NULL)
        {
            mmgr_line_put(&build->text, QUICK_LINE_ASKED);
            mmgr_line_decimal(&build->text, lines, 1u);
            mmgr_line_put(&build->text, QUICK_LINE_ASKED_END);
            row->label_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->label);
        }
        return;
    }
    QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = CLI_QUICK_ROW_LINE);
    if (row == NULL)
    {
        return;
    }
    const unsigned long long shown = (numbers[0] < lines) ? numbers[0] : lines;
    mmgr_line_put(&build->text, QUICK_LINE_GO);
    mmgr_line_decimal(&build->text, shown, 1u);
    if (numbers[1] != 0ULL)
    {
        mmgr_line_put(&build->text, QUICK_LINE_COLUMN);
        mmgr_line_decimal(&build->text, numbers[1], 1u);
    }
    row->label_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->label);
    row->target = (unsigned int)build->text.at;
    mmgr_line_put(&build->text, s_quick.path);
    row->target_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->target);
    // the line is no further than the text's last and the column below a billion, each inside an unsigned int
    row->line = (unsigned int)(shown - 1ULL);
    row->column = (numbers[1] != 0ULL) ? (unsigned int)(numbers[1] - 1ULL) : 0u;
    row->placed = 1;
}

// a declaration's row: its name, `length` bytes of `text`, marked where `query` matches it, its `kind_text` and line
// after it, and the file `path` at the line `at` what it opens, the path in the detail where it is one `in_tree`
EMBED_INLINE void quick_symbol_row(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    QuickRow *const row = EMBED_CALL(quick_row_begun, QuickCtx, .build = build, .type = CLI_QUICK_ROW_SYMBOL);
    if (row == NULL)
    {
        return;
    }
    mmgr_line_put_n(&build->text, args->text, args->length);
    row->label_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->label);
    unsigned int hits = 0u;
    double score = 0.0;
    if (args->query[0] != '\0')
    {
        char name[CLI_PATH_MAX_LEN];
        VerbaLinea name_line = {name, sizeof(name), 0ULL};
        mmgr_line_put_n(&name_line, args->text, args->length);
        (void)mmgr_line_finish(&name_line);
        (void)EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = args->query, .text = name, .from_u64 = 0ULL,
                         .score = &score, .hits = s_hit, .count_pointer = &hits);
    }
    row->label_marks = EMBED_CALL(quick_marked, QuickCtx, .build = build, .at = row->label, .length = row->label_length,
                                  .hits = s_hit, .hit_count = hits, .from = 0u, .most = CLI_QUICK_MARKS);
    row->detail = (unsigned int)build->text.at;
    mmgr_line_put(&build->text, args->type_text);
    const char *const path = args->path;
    if (args->in_tree)
    {
        mmgr_line_put(&build->text, ", ");
        mmgr_line_put(&build->text, path);
        mmgr_line_ch(&build->text, ':');
    }
    else
    {
        mmgr_line_put(&build->text, QUICK_LINE_AT);
    }
    mmgr_line_decimal(&build->text, args->at + 1ULL, 1u);
    row->detail_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->detail);
    row->target = (unsigned int)build->text.at;
    mmgr_line_put(&build->text, path);
    row->target_length = EMBED_CALL(quick_written, QuickCtx, .build = build, .at = row->target);
    row->line = args->at;
    row->placed = 1;
}

// the rows of what the shown view declares for `length` bytes of `text` typed, at most `most`, the best match first and
// with nothing typed in the order of their lines, or a sentence where no view is shown and the rows are not `grouped`
EMBED_INLINE void quick_outline(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    if ((s_quick.key[0] == '\0') || (s_quick.path[0] == '\0'))
    {
        if (!args->grouped)
        {
            EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_TEXT, .text = QUICK_SYMBOLS_NONE);
        }
        return;
    }
    char query[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea typed = {query, sizeof(query), 0ULL};
    mmgr_line_put_n(&typed, args->text, args->length);
    (void)mmgr_line_finish(&typed);
    EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = s_quick.key, .path = s_quick.path);
    const unsigned int rows = EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        CliOutlineRow row;
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &row))
        {
            continue;
        }
        // the line counts as a score below every match's, the lines far fewer than a double counts exactly
        double score = -(double)row.line;
        if (args->length != 0ULL)
        {
            char name[CLI_PATH_MAX_LEN];
            VerbaLinea name_line = {name, sizeof(name), 0ULL};
            mmgr_line_put_n(&name_line, row.name, row.length);
            (void)mmgr_line_finish(&name_line);
            unsigned int hits = 0u;
            if (!EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = name, .from_u64 = 0ULL,
                            .score = &score, .hits = s_hit, .count_pointer = &hits))
            {
                continue;
            }
        }
        count = EMBED_CALL(quick_put, QuickCtx, .at = at, .score = score, .count = count, .most = args->most);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliOutlineRow row;
        if (EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = s_top[at].at, .row = &row))
        {
            // an outline's line is below CLI_OUTLINE_LINES_MOST, inside an unsigned int
            EMBED_CALL(quick_symbol_row, QuickCtx, .build = build, .text = row.name, .length = row.length,
                       .query = query, .type_text = row.type, .path = s_quick.path, .at = (unsigned int)row.line);
        }
    }
}

// the rows of what the tree's files declare for `length` bytes of `text` typed, at most `most`, the best first, and the
// reading row while the tree is read; a sentence where no tree is open and the rows are not `grouped`
EMBED_INLINE void quick_tree(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    if (s_quick.root == NULL)
    {
        if (!args->grouped)
        {
            EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_TEXT, .text = QUICK_TREE_NONE);
        }
        return;
    }
    char query[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea typed = {query, sizeof(query), 0ULL};
    mmgr_line_put_n(&typed, args->text, args->length);
    (void)mmgr_line_finish(&typed);
    CliFoundDeclarations *const found = EMBED_CALL(cli_symbols_query_max.tree_declarations_alloc, CliSymbolsQueryMaxCfg,
                                                   .query = query, .most = args->most, .owner = g_cli_quick_owner);
    for (unsigned int at = 0u; (found != NULL) && (at < found->count); at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        EMBED_CALL(quick_symbol_row, QuickCtx, .build = build, .text = one->name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_PATH_MAX_LEN}), .query = query,
                   .type_text = one->type, .path = one->path, .at = one->line, .in_tree = 1);
    }
    if ((found != NULL) && found->reading)
    {
        build->reading = 1;
        EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_READING, .text = QUICK_READING);
    }
    g_cli_guard->persistent_buf_release(found, g_cli_quick_owner);
}

// Search Everywhere's rows for `length` bytes of `text`: the files, then with something typed what the tree declares
// and with nothing what the shown view declares, then the commands, CLI_QUICK_EACH of each under its heading, a type
// with no rows standing with no heading
EMBED_INLINE void quick_everywhere(const QuickCtx *args)
{
    QuickBuild *const build = args->build;
    static const char *const s_headings[3] = {QUICK_FILES, QUICK_SYMBOLS, QUICK_ACTIONS};
    for (unsigned int group = 0u; group < 3u; group += 1u)
    {
        const unsigned int rows = build->rows;
        const unsigned int marks = build->marks;
        const unsigned long long text = build->text.at;
        EMBED_CALL(quick_said, QuickCtx, .build = build, .type = CLI_QUICK_ROW_HEADING, .text = s_headings[group]);
        if (group == 0u)
        {
            EMBED_CALL(quick_files, QuickCtx, .build = build, .text = args->text, .length = args->length,
                       .most = CLI_QUICK_EACH);
        }
        else if ((group == 1u) && (args->length != 0ULL))
        {
            EMBED_CALL(quick_tree, QuickCtx, .build = build, .text = args->text, .length = args->length,
                       .most = CLI_QUICK_EACH, .grouped = 1);
        }
        else if (group == 1u)
        {
            EMBED_CALL(quick_outline, QuickCtx, .build = build, .text = args->text, .length = args->length,
                       .most = CLI_QUICK_EACH, .grouped = 1);
        }
        else
        {
            EMBED_CALL(quick_commands, QuickCtx, .build = build, .text = args->text, .length = args->length,
                       .most = CLI_QUICK_EACH);
        }
        if (build->rows == (rows + 1u))
        {
            build->rows = rows;
            build->marks = marks;
            build->text.at = text;
        }
    }
}

// what the quick open reads from its typed text: Search Everywhere's, else by the text's first letter
EMBED_INLINE unsigned int quick_mode(const QuickCtx *args)
{
    (void)args;
    const char first = (s_quick.length != 0ULL) ? s_quick.text[0] : '\0';
    if (s_quick.everywhere)
    {
        return CLI_QUICK_EVERYWHERE;
    }
    switch (first)
    {
    case '>':
        return CLI_QUICK_COMMANDS;
    case ':':
        return CLI_QUICK_LINE;
    case '@':
        return CLI_QUICK_SYMBOLS;
    case '#':
        return CLI_QUICK_TREE;
    default:
        return CLI_QUICK_FILES;
    }
}

// the row `at` of the cell standing has the label and detail of the build's row `count`
EMBED_INLINE int quick_same(const QuickCtx *args)
{
    const QuickBuild *const build = args->build;
    const QuickRow *const was = &s_quick.row[args->at];
    const QuickRow *const now = &build->row[args->count];
    return (was->type == now->type) && (was->label_length == now->label_length) &&
           (was->detail_length == now->detail_length) &&
           (memor.cmp(&(MemoriaCfg){.src = s_quick.row_text + was->label,
                                    .other = build->text.out + now->label,
                                    .bytes = was->label_length}) == 0) &&
           (memor.cmp(&(MemoriaCfg){.src = s_quick.row_text + was->detail,
                                    .other = build->text.out + now->detail,
                                    .bytes = was->detail_length}) == 0);
}

// the chosen row standing in sight of the rows the box shows, the scroll no further than the last rows fill the box
EMBED_INLINE void quick_followed(const QuickCtx *args)
{
    (void)args;
    const unsigned int shown = (s_quick.shown != 0u) ? s_quick.shown : 1u;
    const unsigned int most = (s_quick.rows > shown) ? (s_quick.rows - shown) : 0u;
    s_quick.scroll = (s_quick.chosen < s_quick.scroll) ? s_quick.chosen : s_quick.scroll;
    s_quick.scroll = (s_quick.chosen >= (s_quick.scroll + shown)) ? ((s_quick.chosen + 1u) - shown) : s_quick.scroll;
    s_quick.scroll = (s_quick.scroll > most) ? most : s_quick.scroll;
}

// the rows asked anew for the typed text at `now_ms`, the chosen row chosen still where `keep` and it stands among
// them, else the first that is no heading; the rows as they were where the cells cannot meet the ask
EMBED_INLINE void quick_asked(const QuickCtx *args)
{
    const unsigned long long row_bytes = (unsigned long long)CLI_QUICK_MOST * sizeof(QuickRow);
    const unsigned long long mark_bytes = 2ULL * CLI_QUICK_MOST * CLI_QUICK_MARKS * sizeof(unsigned int);
    const unsigned long long text_bytes = (unsigned long long)CLI_QUICK_MOST * QUICK_ROW_BYTES;
    unsigned char *const scratch =
        g_cli_guard->temporary_buf_alloc(row_bytes + mark_bytes + text_bytes, g_cli_quick_owner);
    if (scratch == NULL)
    {
        return;
    }
    // the rows lie first in the scratch, which its alignment saves aligned, the marks after them and the text last
    QuickBuild build = {(QuickRow *)(void *)scratch,
                        0u,
                        (unsigned int *)(void *)(scratch + row_bytes),
                        0u,
                        {(char *)(scratch + row_bytes + mark_bytes), text_bytes, 0ULL},
                        0};
    const unsigned int mode = EMBED_CALL(quick_mode, QuickCtx, .build = &build);
    // the text read past its first letter where it reads one, its spaces at either end left out
    const unsigned long long skip = ((mode == CLI_QUICK_FILES) || (mode == CLI_QUICK_EVERYWHERE)) ? 0ULL : 1ULL;
    unsigned long long from = (skip < s_quick.length) ? skip : s_quick.length;
    unsigned long long to = s_quick.length;
    while ((from < to) && (s_quick.text[from] == ' '))
    {
        from += 1ULL;
    }
    while ((to > from) && (s_quick.text[to - 1ULL] == ' '))
    {
        to -= 1ULL;
    }
    const char *const query = (s_quick.text != NULL) ? (s_quick.text + from) : "";
    switch (mode)
    {
    case CLI_QUICK_COMMANDS:
        EMBED_CALL(quick_commands, QuickCtx, .build = &build, .text = query, .length = to - from,
                   .most = CLI_QUICK_MOST);
        break;
    case CLI_QUICK_LINE:
        EMBED_CALL(quick_line, QuickCtx, .build = &build, .text = query, .length = to - from);
        break;
    case CLI_QUICK_SYMBOLS:
        EMBED_CALL(quick_outline, QuickCtx, .build = &build, .text = query, .length = to - from,
                   .most = CLI_QUICK_MOST);
        break;
    case CLI_QUICK_TREE:
        EMBED_CALL(quick_tree, QuickCtx, .build = &build, .text = query, .length = to - from, .most = CLI_QUICK_MOST);
        break;
    case CLI_QUICK_EVERYWHERE:
        EMBED_CALL(quick_everywhere, QuickCtx, .build = &build, .text = query, .length = to - from);
        break;
    default:
        EMBED_CALL(quick_files, QuickCtx, .build = &build, .text = query, .length = to - from, .most = CLI_QUICK_MOST);
        break;
    }
    if (build.rows == 0u)
    {
        EMBED_CALL(quick_said, QuickCtx, .build = &build, .type = CLI_QUICK_ROW_TEXT,
                   .text = (mode == CLI_QUICK_COMMANDS) ? QUICK_NO_COMMANDS : QUICK_NO_RESULTS);
    }
    // the chosen row found again among the new, else the first that is no heading
    unsigned int chosen = build.rows;
    for (unsigned int at = 0u;
         args->keep && (s_quick.chosen < s_quick.rows) && (at < build.rows) && (chosen == build.rows); at += 1u)
    {
        chosen = EMBED_CALL(quick_same, QuickCtx, .build = &build, .at = s_quick.chosen, .count = at) ? at : chosen;
    }
    for (unsigned int at = 0u; (chosen == build.rows) && (at < build.rows); at += 1u)
    {
        chosen = (build.row[at].type != CLI_QUICK_ROW_HEADING) ? at : chosen;
    }
    const unsigned long long rows_bytes = (unsigned long long)build.rows * sizeof(QuickRow);
    const unsigned long long marks_bytes = (unsigned long long)build.marks * sizeof(unsigned int);
    unsigned char *const cell =
        g_cli_guard->persistent_buf_alloc(rows_bytes + marks_bytes + build.text.at, g_cli_quick_owner);
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = build.row, .bytes = rows_bytes});
        if (marks_bytes != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = cell + rows_bytes, .src = build.mark, .bytes = marks_bytes});
        }
        memor.cpy(&(MemoriaCfg){.dst = cell + rows_bytes + marks_bytes, .src = build.text.out, .bytes = build.text.at});
        g_cli_guard->persistent_buf_release(s_quick.cell, g_cli_quick_owner);
        s_quick.cell = cell;
        s_quick.row = (const QuickRow *)(const void *)cell;
        s_quick.mark = (const unsigned int *)(const void *)(cell + rows_bytes);
        s_quick.row_text = (const char *)(cell + rows_bytes + marks_bytes);
        s_quick.rows = build.rows;
        s_quick.chosen = (chosen < build.rows) ? chosen : 0u;
        s_quick.reading = build.reading;
        s_quick.asked_ms = args->now_ms;
        EMBED_CALL(quick_followed, QuickCtx, .build = &build);
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_quick_owner);
}

// the typed text laid anew from `length` bytes of `text` with the caret at the byte `first`, in a cell sized to it with
// its end, none where it is empty; the text as it was where the cells cannot meet it
EMBED_INLINE void quick_text_laid(const QuickCtx *args)
{
    char *const cell =
        (args->length != 0ULL) ? g_cli_guard->persistent_buf_alloc(args->length + 1ULL, g_cli_quick_owner) : NULL;
    if ((args->length != 0ULL) && (cell == NULL))
    {
        return;
    }
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = args->length});
        cell[args->length] = '\0';
    }
    g_cli_guard->persistent_buf_release(s_quick.text, g_cli_quick_owner);
    s_quick.text = cell;
    s_quick.length = args->length;
    s_quick.caret = (args->first < args->length) ? args->first : args->length;
}

// `length` bytes of `text` put at the caret, a control character and what follows it left out, no more than the most
// bytes typed take, the caret after them
EMBED_INLINE void quick_text_put(const QuickCtx *args)
{
    unsigned long long length = 0ULL;
    while ((length < args->length) && ((unsigned char)args->text[length] >= 0x20u) && (args->text[length] != 0x7f))
    {
        length += 1ULL;
    }
    const unsigned long long space_left = CLI_QUICK_TEXT_MAX - s_quick.length;
    length = (length < space_left) ? length : space_left;
    // the length cut back to the lead byte of a letter, so that no letter is cut in two
    while ((length > 0ULL) && (length < args->length) && (((unsigned char)args->text[length] & 0xC0u) == 0x80u))
    {
        length -= 1ULL;
    }
    char joined[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    mmgr_line_put_n(&line, (s_quick.text != NULL) ? s_quick.text : "", s_quick.caret);
    mmgr_line_put_n(&line, args->text, length);
    mmgr_line_put_n(&line, (s_quick.text != NULL) ? (s_quick.text + s_quick.caret) : "",
                    s_quick.length - s_quick.caret);
    EMBED_CALL(quick_text_laid, QuickCtx, .text = joined, .length = s_quick.length + length,
               .first = s_quick.caret + length);
}

// the byte the letter before the caret starts at, and the byte after the letter at the caret
EMBED_INLINE unsigned long long quick_before(const QuickCtx *args)
{
    (void)args;
    unsigned long long at = s_quick.caret;
    at -= (at > 0ULL) ? 1ULL : 0ULL;
    while ((at > 0ULL) && (((unsigned char)s_quick.text[at] & 0xC0u) == 0x80u))
    {
        at -= 1ULL;
    }
    return at;
}

EMBED_INLINE unsigned long long quick_after(const QuickCtx *args)
{
    (void)args;
    unsigned long long at = s_quick.caret;
    at += (at < s_quick.length) ? 1ULL : 0ULL;
    while ((at < s_quick.length) && (((unsigned char)s_quick.text[at] & 0xC0u) == 0x80u))
    {
        at += 1ULL;
    }
    return at;
}

// the bytes of the typed text from `first` to before `last` taken out, the caret at `first`
EMBED_INLINE void quick_text_cut(const QuickCtx *args)
{
    char joined[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    mmgr_line_put_n(&line, s_quick.text, args->first);
    mmgr_line_put_n(&line, s_quick.text + args->last, s_quick.length - args->last);
    EMBED_CALL(quick_text_laid, QuickCtx, .text = joined, .length = s_quick.length - (args->last - args->first),
               .first = args->first);
}

// the shown view's key and its file's path taken from `text`, NULL for none
EMBED_INLINE void quick_view_taken(const QuickCtx *args)
{
    s_quick.key[0] = '\0';
    s_quick.path[0] = '\0';
    if (args->text == NULL)
    {
        return;
    }
    VerbaLinea key = {s_quick.key, sizeof(s_quick.key), 0ULL};
    mmgr_line_put(&key, args->text);
    if (mmgr_line_finish(&key) == 0ULL)
    {
        s_quick.key[0] = '\0';
        return;
    }
    if (!EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = s_quick.key, .path_out = s_quick.path))
    {
        s_quick.path[0] = '\0';
    }
}

int cli_quick_open(const CliQuickCfg *args)
{
    unsigned int opener = QUICK_OPENERS;
    for (unsigned int at = 0u; (opener == QUICK_OPENERS) && (args->command != NULL) && (at < QUICK_OPENERS); at += 1u)
    {
        opener = cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = s_openers[at], .cap = CLI_LABEL_MAX_LEN})
                     ? at
                     : QUICK_OPENERS;
    }
    if (opener == QUICK_OPENERS)
    {
        return 0;
    }
    s_quick.open = 1;
    s_quick.everywhere = opener == QUICK_OPENER_EVERYWHERE;
    s_quick.root = args->root;
    s_quick.shown = (args->shown != 0u) ? args->shown : s_quick.shown;
    s_quick.chosen = 0u;
    s_quick.scroll = 0u;
    s_quick.shift_ms = 0ULL;
    EMBED_CALL(quick_view_taken, QuickCtx, .text = args->key);
    EMBED_CALL(quick_text_laid, QuickCtx, .text = s_prefixes[opener],
               .length = cellul.len(&(CatenaFinitaCfg){.src = s_prefixes[opener], .cap = 2ULL}), .first = 2ULL);
    if ((args->text != NULL) && (args->length != 0ULL))
    {
        EMBED_CALL(quick_text_put, QuickCtx, .text = args->text, .length = args->length);
    }
    if ((args->root != NULL) && ((opener == 0u) || s_quick.everywhere))
    {
        EMBED_CALL(cli_files_root_paths.read_behind, CliFilesRootPathsCfg, .root = args->root);
    }
    EMBED_CALL(quick_asked, QuickCtx, .now_ms = args->now_ms);
    return 1;
}

void cli_quick_type(const CliQuickCfg *args)
{
    if (!s_quick.open || (args->text == NULL))
    {
        return;
    }
    EMBED_CALL(quick_text_put, QuickCtx, .text = args->text, .length = args->length);
    EMBED_CALL(quick_asked, QuickCtx, .now_ms = args->now_ms);
}

int cli_quick_key(const CliQuickCfg *args)
{
    if (args->name == NULL)
    {
        return 0;
    }
    if (!s_quick.open)
    {
        CliItem item;
        return cli_menus_keyed(args->name, &item) &&
               EMBED_CALL(cli_quick.open, CliQuickCfg, .command = item.command, .root = args->root, .key = args->key,
                          .shown = args->shown, .now_ms = args->now_ms);
    }
    static const char *const s_keys[QUICK_KEYS] = {"Backspace", "Delete", "Left", "Right", "Home", "End", "Mod+V"};
    unsigned int which = QUICK_KEYS;
    for (unsigned int at = 0u; (which == QUICK_KEYS) && (at < QUICK_KEYS); at += 1u)
    {
        which = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_keys[at], .cap = 32ULL}) ? at : QUICK_KEYS;
    }
    const unsigned long long before = EMBED_CALL(quick_before, QuickCtx, .now_ms = 0ULL);
    const unsigned long long after = EMBED_CALL(quick_after, QuickCtx, .now_ms = 0ULL);
    switch (which)
    {
    case 0u:
        if (s_quick.caret == 0ULL)
        {
            return 1;
        }
        EMBED_CALL(quick_text_cut, QuickCtx, .first = before, .last = s_quick.caret);
        break;
    case 1u:
        if (s_quick.caret == s_quick.length)
        {
            return 1;
        }
        EMBED_CALL(quick_text_cut, QuickCtx, .first = s_quick.caret, .last = after);
        break;
    case 2u:
        s_quick.caret = before;
        return 1;
    case 3u:
        s_quick.caret = after;
        return 1;
    case 4u:
        s_quick.caret = 0ULL;
        return 1;
    case 5u:
        s_quick.caret = s_quick.length;
        return 1;
    case 6u: {
        char pasted[CLI_QUICK_TEXT_MAX + 1u];
        VerbaLinea line = {pasted, sizeof(pasted), 0ULL};
        (void)EMBED_CALL(cli_internal_table.clipboard_text_read, CliCtx, .text_verba_linea_pointer = &line);
        (void)mmgr_line_finish(&line);
        // a clipboard past the most bytes typed is read to as many as the line takes, which the typing cuts again
        const unsigned long long length = mmgr_line_ok(&line) ? line.at : (sizeof(pasted) - 1ULL);
        if (length == 0ULL)
        {
            return 1;
        }
        EMBED_CALL(quick_text_put, QuickCtx, .text = pasted, .length = length);
        break;
    }
    default:
        return 0;
    }
    EMBED_CALL(quick_asked, QuickCtx, .now_ms = args->now_ms);
    return 1;
}

void cli_quick_step(const CliQuickCfg *args)
{
    s_quick.shown = (args->shown != 0u) ? args->shown : s_quick.shown;
    if (!s_quick.open || (s_quick.rows == 0u))
    {
        return;
    }
    if (args->by != 0)
    {
        // the rows are fewer than an int counts
        const int count = (int)s_quick.rows;
        const int way = (args->by > 0) ? 1 : -1;
        int next = (int)s_quick.chosen + args->by;
        next = (next < 0) ? 0 : ((next >= count) ? (count - 1) : next);
        // the next is among the rows, as clamped
        while ((s_quick.row[next].type == CLI_QUICK_ROW_HEADING) && ((next + way) >= 0) && ((next + way) < count))
        {
            next += way;
        }
        s_quick.chosen = (s_quick.row[next].type != CLI_QUICK_ROW_HEADING) ? (unsigned int)next : s_quick.chosen;
        EMBED_CALL(quick_followed, QuickCtx, .now_ms = 0ULL);
    }
    if (args->scroll_by != 0)
    {
        const unsigned int shown = (s_quick.shown != 0u) ? s_quick.shown : 1u;
        // the rows and the scroll are fewer than an int counts
        const int most = (s_quick.rows > shown) ? (int)(s_quick.rows - shown) : 0;
        const int scroll = (int)s_quick.scroll + args->scroll_by;
        s_quick.scroll = (scroll < 0) ? 0u : ((scroll > most) ? (unsigned int)most : (unsigned int)scroll);
    }
}

int cli_quick_row(const CliQuickCfg *args)
{
    if (!s_quick.open || (args->at >= s_quick.rows))
    {
        return 0;
    }
    const QuickRow *const row = &s_quick.row[args->at];
    const unsigned int *const marks = s_quick.mark + row->marks;
    *args->row_out = (CliQuickRow){
        row->type,          s_quick.row_text + row->label,   row->label_length, s_quick.row_text + row->detail,
        row->detail_length, s_quick.row_text + row->keys,    row->keys_length,  marks,
        row->label_marks,   marks + (2u * row->label_marks), row->detail_marks};
    return 1;
}

void cli_quick_view(const CliQuickCfg *args)
{
    const unsigned int mode = EMBED_CALL(quick_mode, QuickCtx, .now_ms = 0ULL);
    const char *placeholder = QUICK_PLACEHOLDER_FILES;
    unsigned long long placeholder_length = sizeof(QUICK_PLACEHOLDER_FILES) - 1ULL;
    if (mode == CLI_QUICK_EVERYWHERE)
    {
        placeholder = QUICK_PLACEHOLDER_EVERYWHERE;
        placeholder_length = sizeof(QUICK_PLACEHOLDER_EVERYWHERE) - 1ULL;
    }
    else if (mode == CLI_QUICK_COMMANDS)
    {
        placeholder = QUICK_PLACEHOLDER_COMMANDS;
        placeholder_length = sizeof(QUICK_PLACEHOLDER_COMMANDS) - 1ULL;
    }
    *args->view_out = (CliQuickView){s_quick.open,
                                     mode,
                                     (s_quick.text != NULL) ? s_quick.text : "",
                                     s_quick.length,
                                     s_quick.caret,
                                     placeholder,
                                     placeholder_length,
                                     s_quick.open ? s_quick.rows : 0u,
                                     s_quick.chosen,
                                     s_quick.scroll};
}

int cli_quick_choose(const CliQuickCfg *args)
{
    const unsigned int at = (args->at == CLI_QUICK_CHOSEN) ? s_quick.chosen : args->at;
    if (!s_quick.open || (at >= s_quick.rows))
    {
        return 0;
    }
    const QuickRow *const row = &s_quick.row[at];
    const int path = ((row->type == CLI_QUICK_ROW_FILE) || (row->type == CLI_QUICK_ROW_LINE) ||
                      (row->type == CLI_QUICK_ROW_SYMBOL)) &&
                     (row->target_length != 0u) && (row->target_length < sizeof(s_quick.chosen_path));
    const int command = (row->type == CLI_QUICK_ROW_COMMAND) && (row->target_length < sizeof(s_quick.chosen_command)) &&
                        (row->keys_length < sizeof(s_quick.chosen_keys));
    if (!path && !command)
    {
        return 0;
    }
    char *const into = path ? s_quick.chosen_path : s_quick.chosen_command;
    memor.cpy(&(MemoriaCfg){.dst = into, .src = s_quick.row_text + row->target, .bytes = row->target_length});
    into[row->target_length] = '\0';
    memor.cpy(
        &(MemoriaCfg){.dst = s_quick.chosen_keys, .src = s_quick.row_text + row->keys, .bytes = row->keys_length});
    s_quick.chosen_keys[row->keys_length] = '\0';
    *args->choice_out = (CliQuickChoice){path ? CLI_QUICK_CHOSE_PATH : CLI_QUICK_CHOSE_COMMAND,
                                         path ? s_quick.chosen_path : "",
                                         {row->line, row->column},
                                         row->placed,
                                         command ? s_quick.chosen_command : "",
                                         s_quick.chosen_keys};
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = path ? CLI_RECENT_FILES : CLI_RECENT_COMMANDS, .entry = into);
    EMBED_CALL(cli_quick.close, CliQuickCfg, .at = 0u);
    return 1;
}

int cli_quick_again(const CliQuickCfg *args)
{
    if (!s_quick.open || !s_quick.reading || (args->now_ms < (s_quick.asked_ms + CLI_QUICK_AGAIN_MS)))
    {
        return 0;
    }
    EMBED_CALL(quick_asked, QuickCtx, .now_ms = args->now_ms, .keep = 1);
    return 1;
}

unsigned long long cli_quick_deadline_ms(const CliQuickCfg *args)
{
    (void)args;
    return (s_quick.open && s_quick.reading) ? (s_quick.asked_ms + CLI_QUICK_AGAIN_MS) : 0ULL;
}

int cli_quick_shift(const CliQuickCfg *args)
{
    const int shift =
        (args->name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Shift", .cap = 8ULL});
    if (!shift)
    {
        s_quick.shift_ms = 0ULL;
        return 0;
    }
    if ((s_quick.shift_ms == 0ULL) || (args->now_ms >= (s_quick.shift_ms + CLI_QUICK_SHIFT_MS)))
    {
        s_quick.shift_ms = args->now_ms;
        return 0;
    }
    return EMBED_CALL(cli_quick.open, CliQuickCfg, .command = s_openers[QUICK_OPENER_EVERYWHERE], .root = args->root,
                      .key = args->key, .shown = args->shown, .now_ms = args->now_ms);
}

void cli_quick_close(const CliQuickCfg *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_quick.cell, g_cli_quick_owner);
    g_cli_guard->persistent_buf_release(s_quick.text, g_cli_quick_owner);
    s_quick.cell = NULL;
    s_quick.row = NULL;
    s_quick.mark = NULL;
    s_quick.row_text = NULL;
    s_quick.rows = 0u;
    s_quick.text = NULL;
    s_quick.length = 0ULL;
    s_quick.caret = 0ULL;
    s_quick.chosen = 0u;
    s_quick.scroll = 0u;
    s_quick.reading = 0;
    s_quick.open = 0;
    s_quick.everywhere = 0;
}
