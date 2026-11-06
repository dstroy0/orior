// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// crumbs.c: the crumbs of the file shown. A read splits the path at its separators and finds the declarations the line
// stands inside in the outline, the last whose line is at most the cursor's by halving and the ones it stands in by
// their depths before it, then lays every crumb and its text in one cell sized to them. The lines the crumbs answer
// for stand beside the cell, and a read inside them on the same file reads nothing. A menu's texts stand in a cell of
// their own sized to them
#include "crumbs.h"

#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../editor/files.h"
#include "symbols.h"

// no line past the last; and the most digits a line takes as a menu row's word
#define CRUMBS_PAST (~0ULL)
#define CRUMBS_DIGITS 24u

// A crumb as it stands in the cell: its kind, where its text starts among the texts and its bytes, its depth, and for
// a declaration its row of the outline
typedef struct
{
    unsigned int kind;
    unsigned int text;
    unsigned int length;
    unsigned int depth;
    unsigned int outline;
} CrumbStored;

// The crumbs: the cell, its rows and the texts after them, how many; the path read and the lines from `from` to
// before `to` the declarations answer for, which a read inside on the same path leaves standing; whether the crumbs
// show; and the menu's texts
typedef struct
{
    unsigned char *cell;
    const CrumbStored *row;
    const char *text;
    unsigned int count;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long from;
    unsigned long long to;
    int shown;
    char *menu;
} Crumbs;

static Crumbs s_crumbs = {NULL, NULL, NULL, 0u, {0}, 0ULL, 0ULL, 1, NULL};

typedef struct
{
    const char *path;
    const char *root;
    unsigned long long line;
    unsigned int at;
    unsigned int depth;
    unsigned int *chain;
    unsigned int *chain_count;
} CrumbsCtx;

// whether the byte at `at` of `path` separates the folders of a path
EMBED_INLINE int crumbs_separator(const CrumbsCtx *args)
{
    return (args->path[args->at] == '/') || (args->path[args->at] == '\\');
}

// the crumbs released, none standing
EMBED_INLINE void crumbs_released(const CrumbsCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_crumbs.cell, g_cli_widgets_owner);
    s_crumbs.cell = NULL;
    s_crumbs.row = NULL;
    s_crumbs.text = NULL;
    s_crumbs.count = 0u;
    s_crumbs.path[0] = '\0';
}

// the declarations the line `line` stands inside, innermost first, into `chain`, their count into `chain_count`, and
// the lines they answer for into the crumbs' `from` and `to`: the last row whose line is at most the line by halving,
// then each row before it standing less deep than the last taken
EMBED_INLINE void crumbs_chain_read(const CrumbsCtx *args)
{
    const unsigned int rows = EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
    unsigned int low = 0u;
    unsigned int high = rows;
    CliOutlineRow row;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        (void)EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = middle, .row = &row);
        if (row.line <= args->line)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    *args->chain_count = 0u;
    s_crumbs.from = 0ULL;
    s_crumbs.to = CRUMBS_PAST;
    if ((low < rows) && EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = low, .row = &row))
    {
        s_crumbs.to = row.line;
    }
    if (low == 0u)
    {
        return;
    }
    unsigned int depth = CLI_OUTLINE_DEEPEST + 1u;
    for (unsigned int at = low; (at > 0u) && (*args->chain_count <= CLI_OUTLINE_DEEPEST); at -= 1u)
    {
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at - 1u, .row = &row) || (row.depth >= depth))
        {
            continue;
        }
        if (*args->chain_count == 0u)
        {
            s_crumbs.from = row.line;
        }
        args->chain[*args->chain_count] = at - 1u;
        *args->chain_count += 1u;
        depth = row.depth;
        if (depth == 0u)
        {
            break;
        }
    }
}

void cli_crumbs_begin(const CliCrumbsCfg *args)
{
    const char *const saved = (args->entries != NULL) ? cli_state_entry_value(args->entries, CLI_CRUMBS_KEY) : NULL;
    s_crumbs.shown = (saved == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "false", .cap = 8ULL});
}

void cli_crumbs_read(const CliCrumbsCfg *args)
{
    if (!s_crumbs.shown || (args->path == NULL) || (args->path[0] == '\0'))
    {
        EMBED_CALL(crumbs_released, CrumbsCtx, .at = 0u);
        return;
    }
    const int same =
        (s_crumbs.cell != NULL) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_crumbs.path, .other = args->path, .cap = CLI_PATH_MAX_LEN}) &&
        (args->line >= s_crumbs.from) && (args->line < s_crumbs.to);
    if (same)
    {
        return;
    }
    // the outline read anew for the file, and the declarations the line stands in
    unsigned int chain[CLI_OUTLINE_DEEPEST + 1u];
    unsigned int chained = 0u;
    if (args->key != NULL)
    {
        EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = args->key, .path = args->path);
        EMBED_CALL(crumbs_chain_read, CrumbsCtx, .line = args->line, .chain = chain, .chain_count = &chained);
    }
    else
    {
        s_crumbs.from = 0ULL;
        s_crumbs.to = CRUMBS_PAST;
    }
    // the path's parts counted with their bytes, then the declarations'
    const unsigned long long path_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN - 1u});
    unsigned int parts = 1u;
    for (unsigned int at = 0u; at < path_length; at += 1u)
    {
        parts += EMBED_CALL(crumbs_separator, CrumbsCtx, .path = args->path, .at = at) ? 1u : 0u;
    }
    unsigned long long bytes = path_length + parts;
    CliOutlineRow row;
    for (unsigned int each = 0u; each < chained; each += 1u)
    {
        (void)EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = chain[each], .row = &row);
        bytes += row.length + 1ULL;
    }
    const unsigned int count = (parts + chained < CLI_CRUMBS_MOST) ? parts + chained : CLI_CRUMBS_MOST;
    const unsigned long long rows_bytes = (unsigned long long)count * sizeof(CrumbStored);
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(rows_bytes + bytes, g_cli_widgets_owner);
    EMBED_CALL(crumbs_released, CrumbsCtx, .at = 0u);
    if (cell == NULL)
    {
        return;
    }
    CrumbStored *const stored = (CrumbStored *)(void *)cell;
    char *const text = (char *)(cell + rows_bytes);
    // the path copied whole, each separator ended, so each part is a text of its own
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->path, .bytes = path_length});
    text[path_length] = '\0';
    unsigned int laid = 0u;
    unsigned int start = 0u;
    for (unsigned int at = 0u; (at <= path_length) && (laid < count); at += 1u)
    {
        const int ends = (at == path_length) || EMBED_CALL(crumbs_separator, CrumbsCtx, .path = args->path, .at = at);
        if (!ends)
        {
            continue;
        }
        text[at] = '\0';
        stored[laid] =
            (CrumbStored){(at == path_length) ? CLI_CRUMB_FILE : CLI_CRUMB_FOLDER, start, at - start, laid, 0u};
        laid += 1u;
        start = at + 1u;
    }
    // the declarations outermost first, each text after the path's
    unsigned long long used = path_length + 1ULL;
    for (unsigned int each = chained; (each > 0u) && (laid < count); each -= 1u)
    {
        (void)EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = chain[each - 1u], .row = &row);
        memor.cpy(&(MemoriaCfg){.dst = text + used, .src = row.name, .bytes = row.length});
        text[used + row.length] = '\0';
        // the texts stand inside a cell no larger than the pool, which an unsigned int counts
        stored[laid] =
            (CrumbStored){CLI_CRUMB_SYMBOL, (unsigned int)used, (unsigned int)row.length, row.depth, chain[each - 1u]};
        laid += 1u;
        used += row.length + 1ULL;
    }
    s_crumbs.cell = cell;
    s_crumbs.row = stored;
    s_crumbs.text = text;
    s_crumbs.count = laid;
    memor.cpy(&(MemoriaCfg){.dst = s_crumbs.path, .src = args->path, .bytes = path_length});
    s_crumbs.path[path_length] = '\0';
}

unsigned int cli_crumbs_rows(const CliCrumbsCfg *args)
{
    (void)args;
    return s_crumbs.shown ? s_crumbs.count : 0u;
}

int cli_crumbs_row(const CliCrumbsCfg *args)
{
    if (!s_crumbs.shown || (args->at >= s_crumbs.count))
    {
        return 0;
    }
    const CrumbStored *const stored = &s_crumbs.row[args->at];
    *args->row = (CliCrumbRow){stored->kind, s_crumbs.text + stored->text, stored->length, stored->depth};
    return 1;
}

// A menu row as the crumbs lay it: its label and its word, each `length` bytes
typedef struct
{
    const char *label;
    unsigned long long label_length;
    const char *word;
    unsigned long long word_length;
} CrumbsMenuRow;

typedef struct
{
    const CrumbsMenuRow *rows;
    unsigned int count;
    const char *command;
} CrumbsMenuCtx;

// the menu's rows, `count` of them, built as the menus' own, each running `command` with its word, their texts copied
// into a cell sized to them: 1, or 0 where none stands or the cell cannot be taken
EMBED_INLINE int crumbs_menu_built(const CrumbsMenuCtx *args)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += args->rows[at].label_length + args->rows[at].word_length + 2ULL;
    }
    char *const texts = (args->count != 0u) ? g_cli_guard->persistent_buf_alloc(bytes, g_cli_widgets_owner) : NULL;
    if (texts == NULL)
    {
        return 0;
    }
    g_cli_guard->persistent_buf_release(s_crumbs.menu, g_cli_widgets_owner);
    s_crumbs.menu = texts;
    cli_menus_own_begin();
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CrumbsMenuRow *const row = &args->rows[at];
        char *const label = texts + used;
        memor.cpy(&(MemoriaCfg){.dst = label, .src = row->label, .bytes = row->label_length});
        label[row->label_length] = '\0';
        char *const word = label + row->label_length + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = word, .src = row->word, .bytes = row->word_length});
        word[row->word_length] = '\0';
        used += row->label_length + row->word_length + 2ULL;
        cli_menus_own_row(&(CliItem){label, args->command, "", word, "", "", 0, 0, 0, 0, 0u});
    }
    return 1;
}

// the entries of the folder of the first `length` bytes of the path read, the top where 0, in the tree at `root`, as
// the menu's rows
EMBED_INLINE int crumbs_folder_menu(const CrumbsCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = s_crumbs.path, .bytes = args->at});
    folder[args->at] = '\0';
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliFileEntries *const entries = EMBED_CALL(cli_files_listing.entries_alloc, CliFilesEntriesCfg, .root = args->root,
                                               .dir = folder, .owner = g_cli_widgets_owner, .err_msg = &err_msg);
    if (entries == NULL)
    {
        return 0;
    }
    CrumbsMenuRow rows[CLI_MENU_OWN_ROWS];
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < entries->count) && (count < CLI_MENU_OWN_ROWS); at += 1u)
    {
        const CliFileEntry *const entry = &entries->entry[at];
        rows[count] =
            (CrumbsMenuRow){entry->name, cellul.len(&(CatenaFinitaCfg){.src = entry->name, .cap = CLI_PATH_MAX_LEN}),
                            entry->path, cellul.len(&(CatenaFinitaCfg){.src = entry->path, .cap = CLI_PATH_MAX_LEN})};
        count += 1u;
    }
    const int built =
        EMBED_CALL(crumbs_menu_built, CrumbsMenuCtx, .rows = rows, .count = count, .command = "file-open");
    g_cli_guard->persistent_buf_release(entries, g_cli_widgets_owner);
    return built;
}

// the declarations at the depth `depth` under the one the outline's row `at` stands in, or at the top where it stands
// in none, as the menu's rows, each word its line
EMBED_INLINE int crumbs_symbol_menu(const CrumbsCtx *args)
{
    const unsigned int rows = EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
    CliOutlineRow row;
    // the row the declaration stands in: the nearest before it standing less deep
    unsigned int first = 0u;
    unsigned int parent_depth = 0u;
    int parented = 0;
    for (unsigned int at = args->at; (at > 0u) && !parented; at -= 1u)
    {
        if (EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at - 1u, .row = &row) && (row.depth < args->depth))
        {
            first = at;
            parent_depth = row.depth;
            parented = 1;
        }
    }
    CrumbsMenuRow menu[CLI_MENU_OWN_ROWS];
    char digits[CLI_MENU_OWN_ROWS][CRUMBS_DIGITS];
    unsigned int count = 0u;
    for (unsigned int at = first; (at < rows) && (count < CLI_MENU_OWN_ROWS); at += 1u)
    {
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &row) ||
            (parented && (row.depth <= parent_depth)))
        {
            break;
        }
        if (row.depth != args->depth)
        {
            continue;
        }
        VerbaLinea line = {digits[count], CRUMBS_DIGITS, 0ULL};
        mmgr_line_decimal(&line, row.line, 1u);
        menu[count] = (CrumbsMenuRow){row.name, row.length, digits[count], mmgr_line_finish(&line)};
        count += 1u;
    }
    return EMBED_CALL(crumbs_menu_built, CrumbsMenuCtx, .rows = menu, .count = count, .command = "symbol-go");
}

int cli_crumbs_menu(const CliCrumbsCfg *args)
{
    if (!s_crumbs.shown || (args->at >= s_crumbs.count))
    {
        return 0;
    }
    const CrumbStored *const stored = &s_crumbs.row[args->at];
    if (stored->kind == CLI_CRUMB_SYMBOL)
    {
        return EMBED_CALL(crumbs_symbol_menu, CrumbsCtx, .at = stored->outline, .depth = stored->depth);
    }
    // a folder's own entries, and a file's folder's: the path up to the crumb's end, or up to the separator before it
    const unsigned int end = (stored->kind == CLI_CRUMB_FOLDER) ? stored->text + stored->length
                                                                : ((stored->text > 0u) ? stored->text - 1u : 0u);
    return EMBED_CALL(crumbs_folder_menu, CrumbsCtx, .root = args->root, .at = end);
}

int cli_crumbs_go(const CliCrumbsCfg *args)
{
    if ((args->text == NULL) || (args->text[0] == '\0'))
    {
        return 0;
    }
    unsigned long long line = 0ULL;
    for (unsigned int at = 0u; args->text[at] != '\0'; at += 1u)
    {
        const char digit = args->text[at];
        if ((digit < '0') || (digit > '9') || (at >= CRUMBS_DIGITS))
        {
            return 0;
        }
        // a digit's value is 0 to 9, which the byte less '0' is
        line = (line * 10ULL) + (unsigned long long)(digit - '0');
    }
    *args->place_out = (CliPosition){line, 0ULL};
    return 1;
}

int cli_crumbs_shown(const CliCrumbsCfg *args)
{
    (void)args;
    return s_crumbs.shown;
}

void cli_crumbs_shown_set(const CliCrumbsCfg *args)
{
    s_crumbs.shown = args->on;
    if (!args->on)
    {
        EMBED_CALL(crumbs_released, CrumbsCtx, .at = 0u);
    }
    // the folder it is saved in and the error line, for the save alone
    char *const folder = g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, g_cli_widgets_owner);
    if (folder == NULL)
    {
        return;
    }
    VerbaLinea err_msg = {folder + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = CLI_CRUMBS_KEY, .value = args->on ? "true" : "false", .err_msg = &err_msg});
    }
    g_cli_guard->persistent_buf_release(folder, g_cli_widgets_owner);
}
