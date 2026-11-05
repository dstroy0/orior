// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_quick.c: the quick open's rows. What is typed stands here with the row chosen; its first letter chooses what
// the rows are: the tree's files that answer the rest, a name the same as what was typed first, then one that starts
// with it, ahead of a longer name that only contains it, a file opened lately ahead of one not, and with nothing typed
// the files opened last; > the menus' commands that can run now, those run last first; : a line of the file shown; @
// what the file shown declares; and # what the tree's files declare. The files opened last are saved in orior's folder
// under a key of the tree's own, the commands run last under one key for every tree
#include "files.h"

#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "files_internal.h"
#include "view.h"

// the bytes the rows' texts take, the bytes of a command's key among those run last, the most bytes of a name a match
// reads, the key the commands run last are saved under, the start of the key a tree's files opened last are saved
// under, what a file whose name is what was typed scores over its match, what one whose name starts so scores, what a
// file opened lately scores over its match, what a command run last scores with nothing typed, the latest highest, and
// the texts of the rows that only inform
#define QUICK_POOL (1u << 20u)
#define QUICK_COMMAND_KEY (2u * CLI_LABEL_MAX_LEN)
#define QUICK_NAME_MAX 1024u
#define QUICK_COMMANDS_KEY "palette.recent"
#define QUICK_RECENT_KEY "recent."
#define QUICK_EXACT 100000.0
#define QUICK_STARTS 50000.0
#define QUICK_LATELY 6.0
#define QUICK_LAST 1000.0
#define QUICK_READING "Reading the tree's files: more may be found."
#define QUICK_NONE "No matching results"
#define QUICK_NO_COMMANDS "No matching commands"
#define QUICK_NO_FILE_LINE "Open a file to go to a line in it."
#define QUICK_NO_FILE_SYMBOLS "Open a file to go to what it declares."
#define QUICK_NO_TREE "Open a tree to go to what its files declare."
#define QUICK_OPENED_LATELY "recently opened"
#define QUICK_RAN_LATELY "recently used"

// a byte folded to lower case where it is an ASCII capital
#define QUICK_LOWER(byte_) ((((byte_) >= 'A') && ((byte_) <= 'Z')) ? (unsigned char)((byte_) + ('a' - 'A')) : (byte_))

// A row as it is built: the places of its label, its detail and its keys in the pool, the label's and the detail's
// bytes and where each matched, counted in letters; and what a choice of it does: the kind, the file's path's place in
// the pool with the line and the column in the file, from 0, where `placed`, or the command's place and the place of
// the key it is counted among those run last by
typedef struct
{
    unsigned int label;
    unsigned int label_length;
    unsigned int detail;
    unsigned int detail_length;
    unsigned int keys;
    unsigned int hits[CLI_QUICK_HITS];
    unsigned int hit_count;
    unsigned int detail_hits[CLI_QUICK_HITS];
    unsigned int detail_hit_count;
    unsigned int kind;
    unsigned int path;
    unsigned long long line;
    unsigned long long col;
    int placed;
    unsigned int command;
    unsigned int recent_key;
} QuickRow;

// A candidate ranked before its row is built: its score, its place among what it was read from, and its menu where it
// is a command
typedef struct
{
    double score;
    unsigned int index;
    unsigned int menu;
} QuickFound;

// The place a query ends in: the bytes of the path before it, the line and the column, each from 1, and whether a line
// stands
typedef struct
{
    unsigned long long path_length;
    unsigned long long line;
    unsigned long long col;
    int placed;
} QuickPlace;

// The quick open: whether it is open; what is typed and its bytes; the row chosen; the rows and their count, one more
// than the most for the row that the tree is being read; the candidates ranked, the best first; the pool the rows'
// texts stand in and its bytes used; whether the tree is being read, and when the rows are asked for again; what the
// last choice does and whether one waits; the tree the rows are of; the files opened last of the tree at `recent_root`;
// the commands run last; the view whose outline was read and whether it was; a menu's items as they are read; and the
// chosen row's label and detail before the rows are asked for again
typedef struct
{
    int open;
    char text[CLI_QUICK_TEXT_MAX];
    unsigned long long text_length;
    unsigned int chosen;
    unsigned int rows;
    QuickRow row[CLI_QUICK_ROWS + 1u];
    QuickFound found[CLI_QUICK_ROWS];
    unsigned int founds;
    char pool[QUICK_POOL];
    unsigned int used;
    int reading;
    unsigned long long again_ms;
    CliQuickGo go;
    int went;
    char root[CLI_PATH_MAX_LEN];
    char recent[CLI_QUICK_RECENT][CLI_PATH_MAX_LEN];
    unsigned int recents;
    char recent_root[CLI_PATH_MAX_LEN];
    char recent_command[CLI_QUICK_RECENT_COMMANDS][QUICK_COMMAND_KEY];
    unsigned int recent_commands;
    char outline_key[CLI_PATH_MAX_LEN];
    int outline_read;
    CliItem items[CLI_ITEMS_MAX];
    char was_label[QUICK_NAME_MAX];
    char was_detail[QUICK_NAME_MAX];
} Quick;

static Quick *s_quick;
static const CliStateEntries *s_entries;
// the match last read: its score, where it matched in letters and how many places
static double s_score;
static unsigned int s_hits[CLI_FUZZY_LETTERS];
static unsigned int s_hit_count;

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *query;
    const char *path;
    const char *root;
    const char *key;
    const char *title;
    const char *what;
    const char *label;
    unsigned long long label_length;
    const char *detail;
    unsigned long long detail_length;
    const CliItem *item;
    unsigned long long from;
    double score;
    unsigned int index;
    unsigned int menu;
    unsigned int at;
    unsigned int most;
    unsigned int kind;
    unsigned long long line;
    unsigned long long col;
    int placed;
    int lately;
    int matched;
    unsigned long long now_ms;
    int again;
    unsigned int *hits;
    char *out;
    unsigned long long capacity;
} QuickCtx;

// the cell taken where none is: it, or NULL where none can be taken
EMBED_INLINE Quick *quick_of(const QuickCtx *args)
{
    (void)args;
    if (s_quick == NULL)
    {
        s_quick = g_cli_cells->persistent_buf_alloc(sizeof(Quick), &g_cli_quick_cells);
        if (s_quick != NULL)
        {
            memor.set(&(MemoriaCfg){.dst = s_quick, .val = 0, .bytes = sizeof(Quick)});
            s_quick->used = 1u;
        }
    }
    return s_quick;
}

// `length` bytes of `text` copied into the pool with a NUL after them: their place, or 0, the empty text's place, where
// they do not fit
EMBED_INLINE unsigned int pool_put(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (args->length + 1ULL > (unsigned long long)(QUICK_POOL - quick->used))
    {
        return 0u;
    }
    const unsigned int at = quick->used;
    memor.cpy(&(MemoriaCfg){.dst = quick->pool + at, .src = args->text, .bytes = args->length});
    quick->pool[at + args->length] = '\0';
    // the pool's bytes are counted by an unsigned int
    quick->used += (unsigned int)args->length + 1u;
    return at;
}

// how many letters `length` bytes of UTF-8 `text` contain, a letter each lead byte
EMBED_INLINE unsigned int quick_letters(const QuickCtx *args)
{
    unsigned int letters = 0u;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        letters += (((unsigned char)args->text[at] & 0xC0u) != 0x80u) ? 1u : 0u;
    }
    return letters;
}

// whether `query` matches `text`, a letter past `from` scoring more: the score and the places into the match last read
EMBED_INLINE int quick_fuzzy(const QuickCtx *args)
{
    return EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = args->query, .text = args->text,
                      .from_u64 = args->from, .score = &s_score, .hits = s_hits, .count_pointer = &s_hit_count);
}

// `score` ranked among the candidates, as `index` of `menu`: the best first, one that scores the same after those
// before it, and the worst dropped past the most
EMBED_INLINE void found_add(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    unsigned int at = quick->founds;
    if ((at == CLI_QUICK_ROWS) && (quick->found[at - 1u].score >= args->score))
    {
        return;
    }
    at = (at == CLI_QUICK_ROWS) ? at - 1u : at;
    while ((at > 0u) && (quick->found[at - 1u].score < args->score))
    {
        quick->found[at] = quick->found[at - 1u];
        at -= 1u;
    }
    quick->found[at] = (QuickFound){args->score, args->index, args->menu};
    quick->founds = (quick->founds < CLI_QUICK_ROWS) ? quick->founds + 1u : CLI_QUICK_ROWS;
}

// the next row, emptied, of `kind`: it, or NULL where the rows are full
EMBED_INLINE QuickRow *row_next(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (quick->rows >= CLI_QUICK_ROWS)
    {
        return NULL;
    }
    QuickRow *const row = &quick->row[quick->rows];
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    row->kind = args->kind;
    quick->rows += 1u;
    return row;
}

// a row that only informs, its label `text`
EMBED_INLINE void row_informs(const QuickCtx *args)
{
    QuickRow *const row = EMBED_CALL(row_next, QuickCtx, .kind = CLI_QUICK_GO_NONE);
    if (row != NULL)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = QUICK_NAME_MAX});
        row->label = EMBED_CALL(pool_put, QuickCtx, .text = args->text, .length = length);
        // the texts are far inside what an unsigned int counts
        row->label_length = (unsigned int)length;
    }
}

// the match last read copied into `hits`, CLI_QUICK_HITS at the most, those from the letter `from` on and before
// `from` plus `length`, each less `from`: their count
EMBED_INLINE unsigned int hits_from(const QuickCtx *args)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < s_hit_count) && (count < CLI_QUICK_HITS); at += 1u)
    {
        if ((s_hits[at] >= args->from) && ((unsigned long long)s_hits[at] < args->from + args->length))
        {
            // a hit is a place in letters, below CLI_FUZZY_LETTERS
            args->hits[count] = s_hits[at] - (unsigned int)args->from;
            count += 1u;
        }
    }
    return count;
}

// the place `length` bytes of `text` end in: `path:line:col`, each of the last two a colon and digits, the line alone
// where one group stands
EMBED_INLINE QuickPlace quick_place(const QuickCtx *args)
{
    unsigned long long end = args->length;
    unsigned long long values[2] = {0ULL, 0ULL};
    unsigned int groups = 0u;
    while (groups < 2u)
    {
        unsigned long long at = end;
        unsigned long long value = 0ULL;
        while ((at > 0ULL) && (args->text[at - 1ULL] >= '0') && (args->text[at - 1ULL] <= '9'))
        {
            at -= 1ULL;
        }
        if ((at == end) || (at == 0ULL) || (args->text[at - 1ULL] != ':'))
        {
            break;
        }
        for (unsigned long long digit = at; digit < end; digit += 1ULL)
        {
            value = (value * 10ULL) + (unsigned long long)(args->text[digit] - '0');
        }
        values[groups] = value;
        groups += 1u;
        end = at - 1ULL;
    }
    const unsigned long long line = (groups == 2u) ? values[1] : values[0];
    return (QuickPlace){end, line, (groups == 2u) ? values[0] : 0ULL, (groups != 0u) && (line != 0ULL)};
}

// whether the path `path` is among the files opened last
EMBED_INLINE int quick_lately(const QuickCtx *args)
{
    const Quick *const quick = s_quick;
    for (unsigned int at = 0u; at < quick->recents; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = quick->recent[at], .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return 1;
        }
    }
    return 0;
}

// where the name of `path`, `length` bytes, starts: the byte after its last slash
EMBED_INLINE unsigned long long quick_name_at(const QuickCtx *args)
{
    unsigned long long cut = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        cut = (args->path[at] == '/') ? at + 1ULL : cut;
    }
    return cut;
}

// whether the file at `path` answers `query`: its match into the match last read, with a name the same as the query
// scoring over any, one that starts with it half that, and a file opened lately over its match
EMBED_INLINE int file_score(const QuickCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long cut = EMBED_CALL(quick_name_at, QuickCtx, .path = args->path, .length = length);
    const unsigned int from = EMBED_CALL(quick_letters, QuickCtx, .text = args->path, .length = cut);
    if (!EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = args->path, .from = from))
    {
        return 0;
    }
    const unsigned long long name_length = length - cut;
    const unsigned long long query_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_QUICK_TEXT_MAX});
    int starts = name_length >= query_length;
    for (unsigned long long at = 0ULL; starts && (at < query_length); at += 1ULL)
    {
        starts = QUICK_LOWER((unsigned char)args->path[cut + at]) == QUICK_LOWER((unsigned char)args->query[at]);
    }
    s_score += starts ? ((name_length == query_length) ? QUICK_EXACT : QUICK_STARTS) : 0.0;
    s_score += EMBED_CALL(quick_lately, QuickCtx, .path = args->path) ? QUICK_LATELY : 0.0;
    return 1;
}

// a file's row for the file at `path`: its name the label and its folder the detail, `lately` adding that it was
// opened lately, what matched `query` marked in each, the row opening the file at `line` and `col`, from 1, where
// `placed`
EMBED_INLINE void file_row(const QuickCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long cut = EMBED_CALL(quick_name_at, QuickCtx, .path = args->path, .length = length);
    const unsigned int from = EMBED_CALL(quick_letters, QuickCtx, .text = args->path, .length = cut);
    QuickRow *const row = EMBED_CALL(row_next, QuickCtx, .kind = CLI_QUICK_GO_FILE);
    if (row == NULL)
    {
        return;
    }
    row->path = EMBED_CALL(pool_put, QuickCtx, .text = args->path, .length = length);
    if (row->path != 0u)
    {
        // the path is inside the pool's bytes, which an unsigned int counts
        row->label = row->path + (unsigned int)cut;
        row->label_length = (unsigned int)(length - cut);
    }
    char detail[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea line = {detail, sizeof(detail), 0ULL};
    mmgr_line_put_n(&line, args->path, (cut > 0ULL) ? cut - 1ULL : 0ULL);
    if (args->lately)
    {
        mmgr_line_put(&line, (cut > 0ULL) ? "  " QUICK_OPENED_LATELY : QUICK_OPENED_LATELY);
    }
    const unsigned long long detail_length = mmgr_line_finish(&line);
    row->detail = EMBED_CALL(pool_put, QuickCtx, .text = detail, .length = detail_length);
    row->detail_length = (unsigned int)detail_length;
    if ((args->query[0] != '\0') &&
        EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = args->path, .from = from))
    {
        row->hit_count = EMBED_CALL(hits_from, QuickCtx, .hits = row->hits, .from = from, .length = CLI_FUZZY_LETTERS);
        row->detail_hit_count = EMBED_CALL(hits_from, QuickCtx, .hits = row->detail_hits, .from = 0ULL,
                                           .length = (from > 0u) ? (unsigned long long)from - 1ULL : 0ULL);
    }
    row->placed = args->placed;
    row->line = args->placed ? args->line - 1ULL : 0ULL;
    row->col = (args->col > 0ULL) ? args->col - 1ULL : 0ULL;
}

// the files opened last of the tree at `root` read from the saved entries where they are another tree's
EMBED_INLINE void recents_read(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (cellul.eq(&(CatenaFinitaCfg){.src = quick->recent_root, .other = args->root, .cap = CLI_PATH_MAX_LEN}))
    {
        return;
    }
    VerbaLinea line = {quick->recent_root, sizeof(quick->recent_root), 0ULL};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
    quick->recents = 0u;
    char key[CLI_PATH_MAX_LEN + 16u];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, QUICK_RECENT_KEY);
    mmgr_line_put(&keyed, args->root);
    (void)mmgr_line_finish(&keyed);
    const char *const saved = (s_entries != NULL) ? cli_state_entry_value(s_entries, key) : NULL;
    if (saved == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ULL;
    for (unsigned long long at = 0ULL; (at <= size) && (quick->recents < CLI_QUICK_RECENT); at += 1ULL)
    {
        if ((at == size) || (saved[at] == '\n'))
        {
            if ((at > from) && (at - from < CLI_PATH_MAX_LEN))
            {
                memor.cpy(&(MemoriaCfg){.dst = quick->recent[quick->recents], .src = saved + from, .bytes = at - from});
                quick->recent[quick->recents][at - from] = '\0';
                quick->recents += 1u;
            }
            from = at + 1ULL;
        }
    }
}

// `at` lines of `capacity` bytes each from `text`, one after another, saved under `key` in orior's folder, joined by
// line breaks
EMBED_INLINE void lines_saved(const QuickCtx *args)
{
    static char s_joined[CLI_QUICK_RECENT * CLI_PATH_MAX_LEN];
    VerbaLinea joined = {s_joined, sizeof(s_joined), 0ULL};
    for (unsigned int at = 0u; at < args->at; at += 1u)
    {
        if (at != 0u)
        {
            mmgr_line_ch(&joined, '\n');
        }
        mmgr_line_put(&joined, args->text + ((unsigned long long)at * args->capacity));
    }
    (void)mmgr_line_finish(&joined);
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = args->key, .value = s_joined, .err_msg = &err_msg});
    }
}

// `text` put first among `at` lines of `capacity` bytes each after `out`, the same line elsewhere dropped and the last
// dropped past `most`: the count after
EMBED_INLINE unsigned int line_first(const QuickCtx *args)
{
    char *const lines = args->out;
    unsigned int count = args->at;
    unsigned int found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = lines + ((unsigned long long)at * args->capacity), .other = args->text, .cap = args->capacity}))
        {
            found = at;
        }
    }
    count = (found < count) ? count : ((count < args->most) ? count + 1u : args->most);
    const unsigned int last = (found < count) ? found : count - 1u;
    for (unsigned int at = last; at > 0u; at -= 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = lines + ((unsigned long long)at * args->capacity),
                                .src = lines + ((unsigned long long)(at - 1u) * args->capacity),
                                .bytes = args->capacity});
    }
    VerbaLinea line = {lines, args->capacity, 0ULL};
    mmgr_line_put(&line, args->text);
    (void)mmgr_line_finish(&line);
    return count;
}

// the place of the command key `text` among those run last, the count where it is not among them
EMBED_INLINE unsigned int command_lately(const QuickCtx *args)
{
    const Quick *const quick = s_quick;
    for (unsigned int at = 0u; at < quick->recent_commands; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = quick->recent_command[at], .other = args->text, .cap = QUICK_COMMAND_KEY}))
        {
            return at;
        }
    }
    return quick->recent_commands;
}

// the files' rows for `query`, the rest of what is typed: with no path in it the files opened last; else the tree at
// `root`'s files that answer it, those opened last alone until the tree's files are read
EMBED_INLINE void file_rows(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_QUICK_TEXT_MAX});
    const QuickPlace place = EMBED_CALL(quick_place, QuickCtx, .text = args->query, .length = length);
    char want[CLI_QUICK_TEXT_MAX];
    memor.cpy(&(MemoriaCfg){.dst = want, .src = args->query, .bytes = place.path_length});
    want[place.path_length] = '\0';
    if (args->root != NULL)
    {
        EMBED_CALL(recents_read, QuickCtx, .root = args->root);
    }
    if (place.path_length == 0ULL)
    {
        for (unsigned int at = 0u; at < quick->recents; at += 1u)
        {
            EMBED_CALL(file_row, QuickCtx, .path = quick->recent[at], .query = want, .lately = 1, .line = place.line,
                       .col = place.col, .placed = place.placed);
        }
        return;
    }
    if (args->root == NULL)
    {
        return;
    }
    const CliFiles *files = NULL;
    if (EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = args->root))
    {
        EMBED_CALL(cli_files_root_paths.read_behind, CliFilesRootPathsCfg, .root = args->root);
        quick->reading = 1;
        for (unsigned int at = 0u; at < quick->recents; at += 1u)
        {
            if (EMBED_CALL(file_score, QuickCtx, .path = quick->recent[at], .query = want))
            {
                EMBED_CALL(found_add, QuickCtx, .score = s_score, .index = at);
            }
        }
    }
    else
    {
        files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->root);
        for (unsigned int at = 0u; (files != NULL) && (at < files->count); at += 1u)
        {
            if (EMBED_CALL(file_score, QuickCtx, .path = files->text + files->start[at], .query = want))
            {
                EMBED_CALL(found_add, QuickCtx, .score = s_score, .index = at);
            }
        }
    }
    for (unsigned int at = 0u; at < quick->founds; at += 1u)
    {
        const char *const path = (files != NULL) ? files->text + files->start[quick->found[at].index]
                                                 : quick->recent[quick->found[at].index];
        EMBED_CALL(file_row, QuickCtx, .path = path, .query = want, .lately = 0, .line = place.line, .col = place.col,
                   .placed = place.placed);
    }
    if (files != NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    }
}

// whether a menu's `item` is offered: a command, not the quick open's own, that can be chosen now and needs no word
EMBED_INLINE int item_offered(const QuickCtx *args)
{
    const CliItem *const item = args->item;
    return !item->line && !item->hidden && (item->command[0] != '\0') &&
           !cellul.eq(&(CatenaFinitaCfg){.src = item->command, .other = "palette", .cap = CLI_LABEL_MAX_LEN}) &&
           ((g_cli_menus_open.disabled == NULL) || !g_cli_menus_open.disabled(item)) &&
           (EMBED_CALL(cli_commands_item.item_words_need, CliCommandsItemCfg, .item_read = item) == 0u);
}

// a command's label, `Menu: Label`, the label's ending dots dropped, written after `out` of `capacity`: its bytes
EMBED_INLINE unsigned long long item_label(const QuickCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put(&line, args->title);
    mmgr_line_put(&line, ": ");
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->item->label, .cap = CLI_LABEL_MAX_LEN});
    const unsigned char *const bytes = (const unsigned char *)args->item->label;
    if ((length >= 3ULL) && (bytes[length - 3ULL] == 0xE2u) && (bytes[length - 2ULL] == 0x80u) &&
        (bytes[length - 1ULL] == 0xA6u))
    {
        length -= 3ULL;
    }
    mmgr_line_put_n(&line, args->item->label, length);
    return mmgr_line_finish(&line);
}

// a command's key among those run last, `Menu/command`, written after `out` of `capacity`
EMBED_INLINE void item_key(const QuickCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put(&line, args->title);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->item->command);
    (void)mmgr_line_finish(&line);
}

// the title of the menu `menu` into `out`, sized CLI_LABEL_MAX_LEN: its letters
EMBED_INLINE unsigned long long menu_title_letters(const QuickCtx *args)
{
    EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = args->menu, .title = args->out);
    return EMBED_CALL(quick_letters, QuickCtx, .text = args->out,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_LABEL_MAX_LEN}));
}

// the commands' rows for `query`: every command the menus with a title list that can run now, those that answer the
// query where one is typed, else all of them, the ones run last first
EMBED_INLINE void command_rows(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    const int typed = args->query[0] != '\0';
    const unsigned int menus = cli_menus();
    char title[CLI_LABEL_MAX_LEN];
    char label[QUICK_COMMAND_KEY];
    char key[QUICK_COMMAND_KEY];
    for (unsigned int menu = 0u; menu < menus; menu += 1u)
    {
        if (!cli_menu_shown(menu))
        {
            continue;
        }
        const unsigned long long title_letters = EMBED_CALL(menu_title_letters, QuickCtx, .menu = menu, .out = title);
        const unsigned int count = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .capacity = CLI_ITEMS_MAX, .items = quick->items);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const CliItem *const item = &quick->items[at];
            if (!EMBED_CALL(item_offered, QuickCtx, .item = item))
            {
                continue;
            }
            double score = 0.0;
            if (typed)
            {
                (void)EMBED_CALL(item_label, QuickCtx, .title = title, .item = item, .out = label,
                                 .capacity = sizeof(label));
                if (!EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = label,
                                .from = title_letters + 2ULL))
                {
                    continue;
                }
                score = s_score;
            }
            else
            {
                EMBED_CALL(item_key, QuickCtx, .title = title, .item = item, .out = key, .capacity = sizeof(key));
                const unsigned int lately = EMBED_CALL(command_lately, QuickCtx, .text = key);
                score = (lately < quick->recent_commands) ? QUICK_LAST - (double)lately : 0.0;
            }
            EMBED_CALL(found_add, QuickCtx, .score = score, .index = at, .menu = menu);
        }
    }
    for (unsigned int at = 0u; at < quick->founds; at += 1u)
    {
        const QuickFound found = quick->found[at];
        const unsigned long long title_letters =
            EMBED_CALL(menu_title_letters, QuickCtx, .menu = found.menu, .out = title);
        (void)EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg, .menu = found.menu,
                         .capacity = CLI_ITEMS_MAX, .items = quick->items);
        const CliItem *const item = &quick->items[found.index];
        QuickRow *const row = EMBED_CALL(row_next, QuickCtx, .kind = CLI_QUICK_GO_COMMAND);
        if (row == NULL)
        {
            return;
        }
        const unsigned long long label_length =
            EMBED_CALL(item_label, QuickCtx, .title = title, .item = item, .out = label, .capacity = sizeof(label));
        row->label = EMBED_CALL(pool_put, QuickCtx, .text = label, .length = label_length);
        // a label is far inside what an unsigned int counts
        row->label_length = (unsigned int)label_length;
        EMBED_CALL(item_key, QuickCtx, .title = title, .item = item, .out = key, .capacity = sizeof(key));
        row->recent_key = EMBED_CALL(pool_put, QuickCtx, .text = key,
                                     .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = sizeof(key)}));
        row->command =
            EMBED_CALL(pool_put, QuickCtx, .text = item->command,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = item->command, .cap = CLI_LABEL_MAX_LEN}));
        row->keys = EMBED_CALL(pool_put, QuickCtx, .text = item->keys,
                               .length = cellul.len(&(CatenaFinitaCfg){.src = item->keys, .cap = CLI_LABEL_MAX_LEN}));
        if (typed)
        {
            if (EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = label, .from = title_letters + 2ULL))
            {
                row->hit_count =
                    EMBED_CALL(hits_from, QuickCtx, .hits = row->hits, .from = 0ULL, .length = CLI_FUZZY_LETTERS);
            }
        }
        else if (EMBED_CALL(command_lately, QuickCtx, .text = key) < quick->recent_commands)
        {
            row->detail =
                EMBED_CALL(pool_put, QuickCtx, .text = QUICK_RAN_LATELY, .length = sizeof(QUICK_RAN_LATELY) - 1u);
            row->detail_length = sizeof(QUICK_RAN_LATELY) - 1u;
        }
    }
}

// a row that goes to `line` and `col`, from 0, of the file at `path`, with `label` of `label_length` bytes and `detail`
// of `detail_length`, what `matched` in the label marked from the match last read
EMBED_INLINE void place_row(const QuickCtx *args)
{
    QuickRow *const row = EMBED_CALL(row_next, QuickCtx, .kind = CLI_QUICK_GO_FILE);
    if (row == NULL)
    {
        return;
    }
    row->path = EMBED_CALL(pool_put, QuickCtx, .text = args->path,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}));
    row->label = EMBED_CALL(pool_put, QuickCtx, .text = args->label, .length = args->label_length);
    // the texts are far inside what an unsigned int counts
    row->label_length = (unsigned int)args->label_length;
    row->detail = EMBED_CALL(pool_put, QuickCtx, .text = args->detail, .length = args->detail_length);
    row->detail_length = (unsigned int)args->detail_length;
    row->hit_count = args->matched
                         ? EMBED_CALL(hits_from, QuickCtx, .hits = row->hits, .from = 0ULL, .length = CLI_FUZZY_LETTERS)
                         : 0u;
    row->placed = 1;
    row->line = args->line;
    row->col = args->col;
}

// the line's rows for `query`: a line and a column of the file shown, counted from 1, the line no further than the
// last
EMBED_INLINE void line_rows(const QuickCtx *args)
{
    const int shown = cli_view_shown();
    if (shown < 0)
    {
        EMBED_CALL(row_informs, QuickCtx, .text = QUICK_NO_FILE_LINE);
        return;
    }
    // the view shown is among the views
    const char *const key = cli_view_key((unsigned int)shown);
    const char *const path = cli_view_path_at((unsigned int)shown);
    CliShape shape = {0ULL, 0ULL};
    const int shaped = EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = key, .shape = &shape);
    const unsigned long long lines = shaped ? shape.lines : 0ULL;
    char typed[CLI_QUICK_TEXT_MAX + 1u];
    VerbaLinea colon = {typed, sizeof(typed), 0ULL};
    mmgr_line_ch(&colon, ':');
    mmgr_line_put(&colon, args->query);
    const QuickPlace place = EMBED_CALL(quick_place, QuickCtx, .text = typed, .length = mmgr_line_finish(&colon));
    char label[128];
    VerbaLinea line = {label, sizeof(label), 0ULL};
    if (!place.placed)
    {
        mmgr_line_put(&line, "Type a line number");
        if (lines != 0ULL)
        {
            mmgr_line_put(&line, " from 1 to ");
            mmgr_line_decimal(&line, lines, 1u);
        }
        mmgr_line_put(&line, " to go to it.");
        (void)mmgr_line_finish(&line);
        EMBED_CALL(row_informs, QuickCtx, .text = label);
        return;
    }
    const unsigned long long shown_line = ((lines != 0ULL) && (place.line > lines)) ? lines : place.line;
    mmgr_line_put(&line, "Go to line ");
    mmgr_line_decimal(&line, shown_line, 1u);
    if (place.col != 0ULL)
    {
        mmgr_line_put(&line, ", column ");
        mmgr_line_decimal(&line, place.col, 1u);
    }
    const unsigned long long label_length = mmgr_line_finish(&line);
    EMBED_CALL(place_row, QuickCtx, .path = path, .label = label, .label_length = label_length, .detail = "",
               .detail_length = 0ULL, .line = shown_line - 1ULL, .col = (place.col != 0ULL) ? place.col - 1ULL : 0ULL,
               .matched = 0);
}

// the detail of a declaration of kind `what`: `what, path:line` or `what, line N`, the line from 1, written after
// `out` of `capacity`: its bytes
EMBED_INLINE unsigned long long declared_detail(const QuickCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put(&line, args->what);
    mmgr_line_put(&line, ", ");
    if (args->path != NULL)
    {
        mmgr_line_put(&line, args->path);
        mmgr_line_ch(&line, ':');
    }
    else
    {
        mmgr_line_put(&line, "line ");
    }
    mmgr_line_decimal(&line, args->line + 1ULL, 1u);
    return mmgr_line_finish(&line);
}

// the symbols' rows for `query`: what the file shown declares, those answering the query by name where one is typed,
// the best first, else all in the order of their lines
EMBED_INLINE void symbol_rows(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    const int shown = cli_view_shown();
    if (shown < 0)
    {
        EMBED_CALL(row_informs, QuickCtx, .text = QUICK_NO_FILE_SYMBOLS);
        return;
    }
    // the view shown is among the views
    const char *const key = cli_view_key((unsigned int)shown);
    const char *const path = cli_view_path_at((unsigned int)shown);
    if (!quick->outline_read ||
        !cellul.eq(&(CatenaFinitaCfg){.src = quick->outline_key, .other = key, .cap = CLI_PATH_MAX_LEN}))
    {
        EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = key, .path = path);
        VerbaLinea line = {quick->outline_key, sizeof(quick->outline_key), 0ULL};
        mmgr_line_put(&line, key);
        (void)mmgr_line_finish(&line);
        quick->outline_read = 1;
    }
    const int typed = args->query[0] != '\0';
    const unsigned int count = EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
    CliOutlineRow declared;
    char name[QUICK_NAME_MAX];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &declared))
        {
            break;
        }
        double score = -(double)declared.line;
        if (typed)
        {
            VerbaLinea line = {name, sizeof(name), 0ULL};
            mmgr_line_put_n(&line, declared.name, declared.length);
            (void)mmgr_line_finish(&line);
            if (!EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = name, .from = 0ULL))
            {
                continue;
            }
            score = s_score;
        }
        EMBED_CALL(found_add, QuickCtx, .score = score, .index = at);
    }
    for (unsigned int at = 0u; at < quick->founds; at += 1u)
    {
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = quick->found[at].index, .row = &declared))
        {
            break;
        }
        VerbaLinea line = {name, sizeof(name), 0ULL};
        mmgr_line_put_n(&line, declared.name, declared.length);
        const unsigned long long name_length = mmgr_line_finish(&line);
        char detail[CLI_PATH_MAX_LEN + 64u];
        const unsigned long long detail_length =
            EMBED_CALL(declared_detail, QuickCtx, .what = declared.kind, .path = NULL, .line = declared.line,
                       .out = detail, .capacity = sizeof(detail));
        const int matched =
            typed && EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = name, .from = 0ULL);
        EMBED_CALL(place_row, QuickCtx, .path = path, .label = name, .label_length = name_length, .detail = detail,
                   .detail_length = detail_length, .line = declared.line, .col = 0ULL, .matched = matched);
    }
}

// the tree's rows for `query`: what the tree at `root`'s files declare that answers it by name, the best first, and
// the row that the tree is being read while it is
EMBED_INLINE void tree_rows(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (args->root == NULL)
    {
        EMBED_CALL(row_informs, QuickCtx, .text = QUICK_NO_TREE);
        return;
    }
    CliFoundDeclarations *const found =
        EMBED_CALL(cli_symbols_query_max.tree_declarations_alloc, CliSymbolsQueryMaxCfg, .query = args->query,
                   .most = CLI_QUICK_ROWS, .owner = &g_cli_quick_cells);
    if (found == NULL)
    {
        EMBED_CALL(row_informs, QuickCtx, .text = QUICK_NO_TREE);
        return;
    }
    const int typed = args->query[0] != '\0';
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliFoundDeclaration *const declared = &found->found[at];
        char detail[CLI_PATH_MAX_LEN + 64u];
        const unsigned long long detail_length =
            EMBED_CALL(declared_detail, QuickCtx, .what = declared->kind, .path = declared->path,
                       .line = declared->line, .out = detail, .capacity = sizeof(detail));
        const int matched =
            typed && EMBED_CALL(quick_fuzzy, QuickCtx, .query = args->query, .text = declared->name, .from = 0ULL);
        EMBED_CALL(place_row, QuickCtx, .path = declared->path, .label = declared->name,
                   .label_length = cellul.len(&(CatenaFinitaCfg){.src = declared->name, .cap = QUICK_NAME_MAX}),
                   .detail = detail, .detail_length = detail_length, .line = declared->line, .col = 0ULL,
                   .matched = matched);
    }
    quick->reading = found->reading;
    g_cli_cells->persistent_buf_release(found, &g_cli_quick_cells);
}

// the rows built for what is typed, for the tree at `root` at `now_ms`: the first letter's kind of rows, a row that
// nothing matched where none is, and the row that the tree is being read last while it is, asked for again a rest
// later; the chosen row the first, or where the rows are asked `again` the one chosen before where it still stands
EMBED_INLINE void rows_build(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (args->again && (quick->chosen < quick->rows))
    {
        const QuickRow *const was = &quick->row[quick->chosen];
        VerbaLinea label = {quick->was_label, sizeof(quick->was_label), 0ULL};
        mmgr_line_put_n(&label, quick->pool + was->label, was->label_length);
        (void)mmgr_line_finish(&label);
        VerbaLinea detail = {quick->was_detail, sizeof(quick->was_detail), 0ULL};
        mmgr_line_put_n(&detail, quick->pool + was->detail, was->detail_length);
        (void)mmgr_line_finish(&detail);
    }
    quick->rows = 0u;
    quick->founds = 0u;
    quick->used = 1u;
    quick->pool[0] = '\0';
    quick->reading = 0;
    quick->again_ms = 0ULL;
    // the query is what is typed after the first letter's kind, its ends trimmed of blanks
    const char kind = (quick->text_length != 0ULL) ? quick->text[0] : '\0';
    const int prefixed = (kind == '>') || (kind == ':') || (kind == '@') || (kind == '#');
    unsigned long long from = prefixed ? 1ULL : 0ULL;
    unsigned long long to = quick->text_length;
    while ((from < to) && ((quick->text[from] == ' ') || (quick->text[from] == '\t')))
    {
        from += 1ULL;
    }
    while ((to > from) && ((quick->text[to - 1ULL] == ' ') || (quick->text[to - 1ULL] == '\t')))
    {
        to -= 1ULL;
    }
    char query[CLI_QUICK_TEXT_MAX];
    memor.cpy(&(MemoriaCfg){.dst = query, .src = quick->text + from, .bytes = to - from});
    query[to - from] = '\0';
    if (kind == '>')
    {
        EMBED_CALL(command_rows, QuickCtx, .query = query);
    }
    else if (kind == ':')
    {
        EMBED_CALL(line_rows, QuickCtx, .query = query);
    }
    else if (kind == '@')
    {
        EMBED_CALL(symbol_rows, QuickCtx, .query = query);
    }
    else if (kind == '#')
    {
        EMBED_CALL(tree_rows, QuickCtx, .query = query, .root = args->root);
    }
    else
    {
        EMBED_CALL(file_rows, QuickCtx, .query = query, .root = args->root);
    }
    if (quick->rows == 0u)
    {
        EMBED_CALL(row_informs, QuickCtx, .text = (kind == '>') ? QUICK_NO_COMMANDS : QUICK_NONE);
    }
    if (quick->reading)
    {
        // the reading row stands past the most, in the row set aside for it
        QuickRow *const row = &quick->row[quick->rows];
        memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
        row->label = EMBED_CALL(pool_put, QuickCtx, .text = QUICK_READING, .length = sizeof(QUICK_READING) - 1u);
        row->label_length = sizeof(QUICK_READING) - 1u;
        quick->rows += 1u;
        quick->again_ms = args->now_ms + CLI_QUICK_AGAIN_MS;
    }
    quick->chosen = 0u;
    for (unsigned int at = 0u; args->again && (at < quick->rows); at += 1u)
    {
        const QuickRow *const row = &quick->row[at];
        if ((row->kind != CLI_QUICK_GO_NONE) &&
            cellul.eq(&(CatenaFinitaCfg){
                .src = quick->pool + row->label, .other = quick->was_label, .cap = sizeof(quick->was_label)}) &&
            cellul.eq(&(CatenaFinitaCfg){
                .src = quick->pool + row->detail, .other = quick->was_detail, .cap = sizeof(quick->was_detail)}))
        {
            quick->chosen = at;
            break;
        }
    }
}

// the row `at` chosen and gone: what it does written into the choice waiting, a command counted among those run last,
// and the quick open closed: 1, or 0 where the row only informs
EMBED_INLINE int row_gone(const QuickCtx *args)
{
    Quick *const quick = s_quick;
    if (args->at >= quick->rows)
    {
        return 0;
    }
    const QuickRow *const row = &quick->row[args->at];
    if (row->kind == CLI_QUICK_GO_NONE)
    {
        return 0;
    }
    quick->go = (CliQuickGo){row->kind,
                             quick->pool + row->path,
                             row->line,
                             row->col,
                             row->placed,
                             quick->pool + row->command,
                             quick->pool + row->keys};
    quick->went = 1;
    quick->open = 0;
    if (row->kind == CLI_QUICK_GO_COMMAND)
    {
        quick->recent_commands = EMBED_CALL(line_first, QuickCtx, .out = quick->recent_command[0],
                                            .capacity = QUICK_COMMAND_KEY, .at = quick->recent_commands,
                                            .most = CLI_QUICK_RECENT_COMMANDS, .text = quick->pool + row->recent_key);
        EMBED_CALL(lines_saved, QuickCtx, .key = QUICK_COMMANDS_KEY, .text = quick->recent_command[0],
                   .capacity = QUICK_COMMAND_KEY, .at = quick->recent_commands);
    }
    return 1;
}

void cli_quick_begin(const CliQuickCfg *args)
{
    s_entries = args->entries;
    Quick *const quick = EMBED_CALL(quick_of, QuickCtx, .at = 0u);
    if (quick == NULL)
    {
        return;
    }
    quick->recent_commands = 0u;
    const char *const saved = (s_entries != NULL) ? cli_state_entry_value(s_entries, QUICK_COMMANDS_KEY) : NULL;
    if (saved == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ULL;
    for (unsigned long long at = 0ULL; (at <= size) && (quick->recent_commands < CLI_QUICK_RECENT_COMMANDS); at += 1ULL)
    {
        if ((at == size) || (saved[at] == '\n'))
        {
            if ((at > from) && (at - from < QUICK_COMMAND_KEY))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = quick->recent_command[quick->recent_commands], .src = saved + from, .bytes = at - from});
                quick->recent_command[quick->recent_commands][at - from] = '\0';
                quick->recent_commands += 1u;
            }
            from = at + 1ULL;
        }
    }
}

void cli_quick_open(const CliQuickCfg *args)
{
    Quick *const quick = EMBED_CALL(quick_of, QuickCtx, .at = 0u);
    if (quick == NULL)
    {
        return;
    }
    VerbaLinea line = {quick->text, sizeof(quick->text), 0ULL};
    mmgr_line_put(&line, (args->text != NULL) ? args->text : "");
    quick->text_length = mmgr_line_finish(&line);
    VerbaLinea root = {quick->root, sizeof(quick->root), 0ULL};
    mmgr_line_put(&root, (args->root != NULL) ? args->root : "");
    (void)mmgr_line_finish(&root);
    quick->open = 1;
    quick->went = 0;
    quick->outline_read = 0;
    EMBED_CALL(rows_build, QuickCtx, .root = args->root, .now_ms = args->now_ms, .again = 0);
}

void cli_quick_close(const CliQuickCfg *args)
{
    (void)args;
    if (s_quick != NULL)
    {
        s_quick->open = 0;
    }
}

int cli_quick_shown(const CliQuickCfg *args)
{
    (void)args;
    return (s_quick != NULL) && s_quick->open;
}

const char *cli_quick_text(const CliQuickCfg *args)
{
    const Quick *const quick = s_quick;
    *args->length_out = (quick != NULL) ? quick->text_length : 0ULL;
    return (quick != NULL) ? quick->text : "";
}

void cli_quick_type(const CliQuickCfg *args)
{
    Quick *const quick = s_quick;
    if ((quick == NULL) || !quick->open || (args->size == 0ULL))
    {
        return;
    }
    unsigned long long size = args->size;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        // a control character typed, a tab or a return, is no part of what is typed
        size = ((unsigned char)args->text[at] < 0x20u) ? at : size;
    }
    if ((size == 0ULL) || (quick->text_length + size + 1ULL > CLI_QUICK_TEXT_MAX))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = quick->text + quick->text_length, .src = args->text, .bytes = size});
    quick->text_length += size;
    quick->text[quick->text_length] = '\0';
    EMBED_CALL(rows_build, QuickCtx, .root = args->root, .now_ms = args->now_ms, .again = 0);
}

int cli_quick_key(const CliQuickCfg *args)
{
    Quick *const quick = s_quick;
    if ((quick == NULL) || !quick->open)
    {
        return 0;
    }
    const char *const name = args->name;
    int step = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Down", .cap = 16ULL}))
    {
        step = 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Up", .cap = 16ULL}))
    {
        step = -1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageDown", .cap = 16ULL}))
    {
        step = 10;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageUp", .cap = 16ULL}))
    {
        step = -10;
    }
    if (step != 0)
    {
        // the rows are few: their count is far inside what an int represents
        const int last = (quick->rows != 0u) ? (int)quick->rows - 1 : 0;
        int next = (int)quick->chosen + step;
        next = (next < 0) ? 0 : ((next > last) ? last : next);
        quick->chosen = (unsigned int)next;
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Enter", .cap = 16ULL}))
    {
        (void)EMBED_CALL(row_gone, QuickCtx, .at = quick->chosen);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 16ULL}))
    {
        quick->open = 0;
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Backspace", .cap = 16ULL}))
    {
        // the last letter erased: its lead byte and the continuation bytes after it
        while (quick->text_length > 0ULL)
        {
            quick->text_length -= 1ULL;
            if (((unsigned char)quick->text[quick->text_length] & 0xC0u) != 0x80u)
            {
                break;
            }
        }
        quick->text[quick->text_length] = '\0';
        EMBED_CALL(rows_build, QuickCtx, .root = args->root, .now_ms = args->now_ms, .again = 0);
        return 1;
    }
    return 0;
}

int cli_quick_step(const CliQuickCfg *args)
{
    Quick *const quick = s_quick;
    if ((quick == NULL) || !quick->open || !quick->reading || (args->now_ms < quick->again_ms))
    {
        return 0;
    }
    EMBED_CALL(rows_build, QuickCtx, .root = args->root, .now_ms = args->now_ms, .again = 1);
    return 1;
}

unsigned long long cli_quick_deadline_ms(const CliQuickCfg *args)
{
    (void)args;
    const Quick *const quick = s_quick;
    return ((quick != NULL) && quick->open && quick->reading) ? quick->again_ms : 0ULL;
}

unsigned int cli_quick_rows(const CliQuickCfg *args)
{
    (void)args;
    return (s_quick != NULL) ? s_quick->rows : 0u;
}

int cli_quick_row(const CliQuickCfg *args)
{
    const Quick *const quick = s_quick;
    if ((quick == NULL) || (args->at >= quick->rows))
    {
        return 0;
    }
    const QuickRow *const row = &quick->row[args->at];
    CliQuickRow *const out = args->row;
    out->label = quick->pool + row->label;
    out->label_length = row->label_length;
    memor.cpy(&(MemoriaCfg){.dst = out->hits, .src = row->hits, .bytes = sizeof(out->hits)});
    out->hit_count = row->hit_count;
    out->detail = quick->pool + row->detail;
    out->detail_length = row->detail_length;
    memor.cpy(&(MemoriaCfg){.dst = out->detail_hits, .src = row->detail_hits, .bytes = sizeof(out->detail_hits)});
    out->detail_hit_count = row->detail_hit_count;
    out->keys = quick->pool + row->keys;
    out->goes = row->kind != CLI_QUICK_GO_NONE;
    return 1;
}

unsigned int cli_quick_chosen(const CliQuickCfg *args)
{
    (void)args;
    return (s_quick != NULL) ? s_quick->chosen : 0u;
}

int cli_quick_go(const CliQuickCfg *args)
{
    Quick *const quick = s_quick;
    if ((quick == NULL) || !quick->went)
    {
        return 0;
    }
    quick->went = 0;
    *args->go = quick->go;
    return 1;
}

int cli_quick_press(const CliQuickCfg *args)
{
    Quick *const quick = s_quick;
    if ((quick == NULL) || !quick->open || (args->at >= quick->rows))
    {
        return 0;
    }
    quick->chosen = args->at;
    if (!EMBED_CALL(row_gone, QuickCtx, .at = args->at))
    {
        return 0;
    }
    return cli_quick_go(args);
}

void cli_quick_opened(const CliQuickCfg *args)
{
    Quick *const quick = EMBED_CALL(quick_of, QuickCtx, .at = 0u);
    if ((quick == NULL) || (args->root == NULL) || (args->path == NULL) || (args->path[0] == '\0'))
    {
        return;
    }
    EMBED_CALL(recents_read, QuickCtx, .root = args->root);
    quick->recents = EMBED_CALL(line_first, QuickCtx, .out = quick->recent[0], .capacity = CLI_PATH_MAX_LEN,
                                .at = quick->recents, .most = CLI_QUICK_RECENT, .text = args->path);
    char key[CLI_PATH_MAX_LEN + 16u];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, QUICK_RECENT_KEY);
    mmgr_line_put(&keyed, args->root);
    (void)mmgr_line_finish(&keyed);
    EMBED_CALL(lines_saved, QuickCtx, .key = key, .text = quick->recent[0], .capacity = CLI_PATH_MAX_LEN,
               .at = quick->recents);
}
