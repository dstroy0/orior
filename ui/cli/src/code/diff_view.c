// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_view.c: the open view as the window reads it: its options from the settings and saved to them, its rows, the
// step from change to change by a binary search over the changes' first rows, and its sentences; the view itself is
// opened and laid in diff_view_open.c
#include "../machine/state.h"
#include "diff_view_internal.h"

// the settings the options stand under and the words each reads while on and while off, as the page stores them
#define DIFF_VIEW_SPACE_KEY "diff-space"
#define DIFF_VIEW_SPACE_ON "aside"
#define DIFF_VIEW_SPACE_OFF "shown"
#define DIFF_VIEW_STRUCTURE_KEY "diff-structure"
#define DIFF_VIEW_STRUCTURE_ON "on"

// the boxes' words and the sentence for two texts the diff will not set side by side, as the page shows them
#define DIFF_VIEW_SPACE_WORDS "Set white space aside"
#define DIFF_VIEW_STRUCTURE_WORDS "Compare by structure"
#define DIFF_VIEW_FAR "The two texts are too far apart to set side by side."

typedef struct
{
    const CliStateEntries *entries;
    const char *key;
    const char *value;
    unsigned int at;
    CliDiffViewLaid *laid_out;
    const CliDiffViewSide *side;
    unsigned int line;
    unsigned short *units_out;
} DiffViewOptionCtx;

// the row `at`'s record read out of the rows cell into `laid_out`
EMBED_INLINE void laid_read(const DiffViewOptionCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_diff_view.rows_cell,
                                                            .offset = CLI_DIFF_VIEW_ROW_AT(args->at),
                                                            .dst = args->laid_out,
                                                            .bytes = sizeof(CliDiffViewLaid)});
}

// the first row of the change `at`, read out of the rows cell
EMBED_INLINE unsigned int change_row_read(const DiffViewOptionCtx *args)
{
    unsigned int row = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_diff_view.rows_cell,
                            .offset = g_cli_diff_view.change_row_at + ((unsigned long long)args->at * sizeof(row)),
                            .dst = &row,
                            .bytes = sizeof(row)});
    return row;
}

// the line `line` of `side` copied into `units_out`, a row's units, cut to CLI_DIFF_VIEW_ROW_UNITS: how many
EMBED_INLINE unsigned long long line_units_read(const DiffViewOptionCtx *args)
{
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_diff_view.lines_cell,
                            .offset = args->side->start_at + ((unsigned long long)args->line * sizeof(start)),
                            .dst = &start,
                            .bytes = sizeof(start)});
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_diff_view.lines_cell,
                            .offset = args->side->length_at + ((unsigned long long)args->line * sizeof(length)),
                            .dst = &length,
                            .bytes = sizeof(length)});
    const unsigned long long copied = (length < CLI_DIFF_VIEW_ROW_UNITS) ? length : CLI_DIFF_VIEW_ROW_UNITS;
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_diff_view.lines_cell,
                                .offset = args->side->unit_at + (start * sizeof(unsigned short)),
                                .dst = args->units_out,
                                .bytes = copied * sizeof(unsigned short)});
    }
    return copied;
}

// whether the setting `key` among the settings `entries` reads `value`
EMBED_INLINE int setting_is(const DiffViewOptionCtx *args)
{
    char read_text[64];
    const char *const read = cli_state_entry_value(args->entries, args->key, read_text, sizeof(read_text));
    return (read != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = read, .other = args->value, .cap = 16ULL});
}

// the first row whose right lines above it are at least `at`, by a binary search over the rows, which rise with them
EMBED_INLINE unsigned int row_at_right(const DiffViewOptionCtx *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    unsigned int low = 0u;
    unsigned int high = view->rows;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        CliDiffViewLaid laid;
        EMBED_CALL(laid_read, DiffViewOptionCtx, .at = middle, .laid_out = &laid);
        if (laid.right_at < args->at)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return (low < view->rows) ? low : ((view->rows != 0u) ? view->rows - 1u : 0u);
}

void cli_diff_view_where_written(const CliDiffViewAtRowLengthCfg *args)
{
    (void)args;
    CliDiffView *const view = &g_cli_diff_view;
    VerbaLinea where = {view->where, sizeof(view->where), 0ULL};
    if (view->changes != 0u)
    {
        mmgr_line_put(&where, "change ");
        mmgr_line_decimal(&where, view->change_at + 1u, 1u);
        mmgr_line_put(&where, " of ");
        mmgr_line_decimal(&where, view->changes, 1u);
    }
    view->where_length = mmgr_line_finish(&where);
}

void cli_diff_view_option_begin(const CliDiffViewOptionOnCfg *args)
{
    g_cli_diff_view.space = EMBED_CALL(setting_is, DiffViewOptionCtx, .entries = args->entries,
                                       .key = DIFF_VIEW_SPACE_KEY, .value = DIFF_VIEW_SPACE_ON);
    g_cli_diff_view.structure = EMBED_CALL(setting_is, DiffViewOptionCtx, .entries = args->entries,
                                           .key = DIFF_VIEW_STRUCTURE_KEY, .value = DIFF_VIEW_STRUCTURE_ON);
}

void cli_diff_view_option_get(const CliDiffViewOptionOnCfg *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    const int space = args->option == CLI_DIFF_VIEW_SPACE;
    *args->on_out = space ? view->space : view->structure;
    *args->enabled_out = space && !view->structure;
}

unsigned int cli_diff_view_option_toggle(const CliDiffViewOptionOnCfg *args)
{
    CliDiffView *const view = &g_cli_diff_view;
    if ((args->option != CLI_DIFF_VIEW_SPACE) || view->structure)
    {
        return args->at;
    }
    view->space = !view->space;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){.folder = folder,
                                                       .key = DIFF_VIEW_SPACE_KEY,
                                                       .value = view->space ? DIFF_VIEW_SPACE_ON : DIFF_VIEW_SPACE_OFF,
                                                       .err_msg = &err_msg});
    }
    if (!view->open)
    {
        return args->at;
    }
    // the right line the row `at` stands at, which the row laid again at that line stands for
    CliDiffViewLaid laid = {0};
    if (args->at < view->rows)
    {
        EMBED_CALL(laid_read, DiffViewOptionCtx, .at = args->at, .laid_out = &laid);
    }
    const unsigned int right_at = (args->at < view->rows) ? laid.right_at : 0u;
    err_msg.at = 0ULL;
    // the view's sources copied out before the open releases them: the texts into this call's bytes, the clipboard's
    // text into a cell of this call
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char other_path[CLI_PATH_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
    char other_id[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = root, .src = view->root, .bytes = sizeof(root)});
    memor.cpy(&(MemoriaCfg){.dst = path, .src = view->path, .bytes = sizeof(path)});
    memor.cpy(&(MemoriaCfg){.dst = other_path, .src = view->other_path, .bytes = sizeof(other_path)});
    memor.cpy(&(MemoriaCfg){.dst = id, .src = view->id, .bytes = sizeof(id)});
    memor.cpy(&(MemoriaCfg){.dst = other_id, .src = view->other_id, .bytes = sizeof(other_id)});
    const int has_other_path = view->has_other_path;
    const int has_id = view->has_id;
    const int has_other_id = view->has_other_id;
    const unsigned long long size = view->size;
    char *const text = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_diff_view_owner);
    if (text == NULL)
    {
        return 0u;
    }
    if ((size != 0ULL) && (view->text_cell != NULL))
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = view->text_cell, .offset = 0ULL, .dst = text, .bytes = size});
    }
    text[size] = '\0';
    const int opened =
        EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = root, .source = view->source,
                   .path = path, .other_path = has_other_path ? other_path : NULL, .id = has_id ? id : NULL,
                   .other_id = has_other_id ? other_id : NULL, .at_ms = view->at_ms, .text = text, .size = size,
                   .err_msg = &err_msg);
    g_cli_guard->persistent_buf_release(text, g_cli_diff_view_owner);
    if (!opened)
    {
        return 0u;
    }
    return EMBED_CALL(row_at_right, DiffViewOptionCtx, .at = right_at);
}

unsigned int cli_diff_view_rows(const CliDiffViewAtRowLengthCfg *args)
{
    (void)args;
    return g_cli_diff_view.rows;
}

int cli_diff_view_row(const CliDiffViewAtRowLengthCfg *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    if (args->at >= view->rows)
    {
        return 0;
    }
    CliDiffViewLaid laid;
    EMBED_CALL(laid_read, DiffViewOptionCtx, .at = args->at, .laid_out = &laid);
    CliDiffViewRow *const row = args->row;
    *row = (CliDiffViewRow){0};
    row->type = laid.type;
    row->has_left = laid.left != CLI_DIFF_VIEW_NONE;
    row->has_right = laid.right != CLI_DIFF_VIEW_NONE;
    row->left_units = row->left_units_copied;
    row->right_units = row->right_units_copied;
    if (row->has_left)
    {
        row->left_line = laid.left;
        row->left_count = EMBED_CALL(line_units_read, DiffViewOptionCtx, .side = &view->left, .line = laid.left,
                                     .units_out = row->left_units_copied);
    }
    if (row->has_right)
    {
        row->right_line = laid.right;
        row->right_count = EMBED_CALL(line_units_read, DiffViewOptionCtx, .side = &view->right, .line = laid.right,
                                      .units_out = row->right_units_copied);
    }
    return 1;
}

unsigned int cli_diff_view_marks(const CliDiffViewAtRowLengthCfg *args)
{
    (void)args;
    return 0u;
}

unsigned int cli_diff_view_step(const CliDiffViewAtRowLengthCfg *args)
{
    CliDiffView *const view = &g_cli_diff_view;
    if (view->changes == 0u)
    {
        return 0u;
    }
    if (args->direction > 0)
    {
        view->change_at = (view->change_at + 1u) % view->changes;
    }
    else if (args->direction < 0)
    {
        view->change_at = (view->change_at + view->changes - 1u) % view->changes;
    }
    else
    {
        // the last change whose first row is at most `at`, the first where none is
        unsigned int low = 0u;
        unsigned int high = view->changes;
        while ((high - low) > 1u)
        {
            const unsigned int middle = low + ((high - low) / 2u);
            if (EMBED_CALL(change_row_read, DiffViewOptionCtx, .at = middle) <= args->at)
            {
                low = middle;
            }
            else
            {
                high = middle;
            }
        }
        view->change_at = low;
    }
    cli_diff_view_where_written(args);
    return EMBED_CALL(change_row_read, DiffViewOptionCtx, .at = view->change_at);
}

const char *cli_diff_view_where(const CliDiffViewAtRowLengthCfg *args)
{
    *args->length_out = g_cli_diff_view.where_length;
    return g_cli_diff_view.where;
}

const char *cli_diff_view_bar(const CliDiffViewAtRowLengthCfg *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    switch (args->part)
    {
    case CLI_DIFF_VIEW_BAR_LABEL:
        *args->length_out = view->open ? view->label_length : 0ULL;
        return view->open ? view->label : "";
    case CLI_DIFF_VIEW_BAR_SIDES:
        *args->length_out = view->open ? view->sides_length : 0ULL;
        return view->open ? view->sides : "";
    case CLI_DIFF_VIEW_BAR_SPACE:
        *args->length_out = sizeof(DIFF_VIEW_SPACE_WORDS) - 1ULL;
        return DIFF_VIEW_SPACE_WORDS;
    default:
        *args->length_out = sizeof(DIFF_VIEW_STRUCTURE_WORDS) - 1ULL;
        return DIFF_VIEW_STRUCTURE_WORDS;
    }
}

const char *cli_diff_view_summary(const CliDiffViewAtRowLengthCfg *args)
{
    *args->length_out = 0ULL;
    return "";
}

const char *cli_diff_view_empty(const CliDiffViewAtRowLengthCfg *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    if (view->open && view->refused)
    {
        *args->length_out = sizeof(DIFF_VIEW_FAR) - 1ULL;
        return DIFF_VIEW_FAR;
    }
    if (view->open && (view->changes == 0u))
    {
        *args->length_out = view->same_length;
        return view->same;
    }
    *args->length_out = 0ULL;
    return "";
}
