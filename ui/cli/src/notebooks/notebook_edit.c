// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_edit.c: an open notebook's cells edited: the cell that has the keys and its view, the notebook's keys, the
// cells laid anew as one is added or deleted and their outputs moved with them as one is moved, and a cell's menu
#include "notebook_edit.h"

#include "../command_line/commands.h"
#include "notebook_cells_internal.h"
#include "notebook_kernel.h"

typedef struct
{
    CliNotebookModel *model;
    unsigned int cell;
    unsigned int other;
    unsigned int place;
    unsigned long long rows;
} NotebookEditCtx;

// the view of the cell `cell` sized to `rows` rows
EMBED_INLINE void view_sized(const NotebookEditCtx *args)
{
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    // a cell's lines stand far below the 32-bit place no count reaches
    const CliViewInput sized = {
        .type = CLI_VIEW_SIZE, .rows = (unsigned int)args->rows, .columns = CLI_NOTEBOOK_EDIT_COLUMNS};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized, .err_msg = &err_msg);
}

// the model's cells marked changed: the cell `cell` edited at the next clock where it is one, the out of date marks
// settled
EMBED_INLINE void edited(const NotebookEditCtx *args)
{
    CliNotebookModel *const model = args->model;
    model->dirty = 1;
    if (args->cell < model->cells)
    {
        model->clock += 1u;
        model->cell[args->cell].edited = model->clock;
    }
    EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
}

int cli_notebook_cell_focus(const CliNotebookEditFocusCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        mmgr_line_put(args->err_msg, "no such cell is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = args->err_msg);
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    if (!EMBED_CALL(cli_view_table.open, CliViewCfg, .key = key, .path = ""))
    {
        mmgr_line_put(args->err_msg, "no view can open on the cell");
        return 0;
    }
    model->focused = args->cell;
    EMBED_CALL(view_sized, NotebookEditCtx, .model = model, .cell = args->cell,
               .rows = model->cell[args->cell].lines + 1ULL);
    const CliViewInput placed = {.type = CLI_VIEW_POSITION, .row = args->line, .column = args->column};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &placed,
                     .err_msg = args->err_msg);
    (void)EMBED_CALL(cli_view_table.changed_alloc, CliViewCfg, .key = key);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    return 1;
}

void cli_notebook_cell_blur(const CliNotebookEditFocusCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->focused == CLI_NOTEBOOK_NONE))
    {
        return;
    }
    const unsigned int cell = model->focused;
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = key);
    model->focused = CLI_NOTEBOOK_NONE;
    EMBED_CALL(cli_notebook_model_internal.lines_read, CliNotebookModelCtx, .model = model, .cell = cell);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = cell);
}

int cli_notebook_cell_input(const CliNotebookEditFocusCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->focused == CLI_NOTEBOOK_NONE))
    {
        return 0;
    }
    const unsigned int cell = model->focused;
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = args->input,
                     .err_msg = args->err_msg);
    if (!EMBED_CALL(cli_view_table.changed_alloc, CliViewCfg, .key = key))
    {
        return 1;
    }
    const unsigned int was = model->cell[cell].lines;
    EMBED_CALL(cli_notebook_model_internal.lines_read, CliNotebookModelCtx, .model = model, .cell = cell);
    if (model->cell[cell].lines != was)
    {
        EMBED_CALL(view_sized, NotebookEditCtx, .model = model, .cell = cell, .rows = model->cell[cell].lines + 1ULL);
        EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = cell);
    }
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = cell);
    return 1;
}

int cli_notebook_cell_key(const CliNotebookEditKeyCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->focused == CLI_NOTEBOOK_NONE))
    {
        return 0;
    }
    const unsigned int cell = model->focused;
    const int next = cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = "Shift+Enter", .cap = 32ULL});
    const int stay = cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = "Ctrl+Enter", .cap = 32ULL}) ||
                     cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = "Meta+Enter", .cap = 32ULL});
    const int escape = cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = "Escape", .cap = 32ULL});
    if (!next && !stay && !escape)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (escape)
    {
        EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = &err_msg);
        return 1;
    }
    (void)EMBED_CALL(cli_notebook_kernel_run.run, CliNotebookKernelRunCfg, .key = args->key, .cell = cell,
                     .err_msg = &err_msg);
    if (next && ((cell + 1u) < model->cells))
    {
        err_msg.at = 0ULL;
        (void)EMBED_CALL(cli_notebook_edit_focus.focus, CliNotebookEditFocusCfg, .key = args->key, .cell = cell + 1u,
                         .line = 0ULL, .column = 0ULL, .err_msg = &err_msg);
    }
    else if (next)
    {
        EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = &err_msg);
    }
    return 1;
}

// the place `place` of a cell moved after the cell `cell` was put in, `other` 1, or taken out, `other` 0: a place at
// or past it one on or one back, the place of the cell taken out none
EMBED_INLINE unsigned int place_moved(const NotebookEditCtx *args)
{
    const unsigned int place = args->place;
    if ((place == CLI_NOTEBOOK_NONE) || (place < args->cell))
    {
        return place;
    }
    if (args->other == 1u)
    {
        return place + 1u;
    }
    return (place == args->cell) ? CLI_NOTEBOOK_NONE : (place - 1u);
}

int cli_notebook_cell_add(const CliNotebookEditCellCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = &err_msg);
    const unsigned int place = (args->cell < model->cells) ? (args->cell + 1u) : model->cells;
    if (!EMBED_CALL(cli_notebook_model_internal.cells_laid, CliNotebookModelCtx, .model = model,
                    .count = model->cells + 1u, .cell = place))
    {
        return 0;
    }
    CliNotebookCell *const cell = &model->cell[place];
    cell->serial = model->next_serial;
    model->next_serial += 1u;
    cell->type = (args->type == CLI_NOTEBOOK_MARKDOWN) ? CLI_NOTEBOOK_MARKDOWN : CLI_NOTEBOOK_CODE;
    cell->origin = CLI_JSON_NONE;
    cell->count = CLI_NOTEBOOK_NONE;
    cell->lines = 1u;
    cell->output_first = ((place + 1u) < model->cells) ? model->cell[place + 1u].output_first : model->outputs;
    model->menu_cell = EMBED_CALL(place_moved, NotebookEditCtx, .cell = place, .other = 1u, .place = model->menu_cell);
    (void)EMBED_CALL(cli_notebook_model_internal.source_stored, CliNotebookModelCtx, .model = model, .cell = place,
                     .text = "", .size = 0ULL);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = place);
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = CLI_NOTEBOOK_NONE);
    return EMBED_CALL(cli_notebook_edit_focus.focus, CliNotebookEditFocusCfg, .key = args->key, .cell = place,
                      .line = 0ULL, .column = 0ULL, .err_msg = &err_msg);
}

int cli_notebook_cell_retype(const CliNotebookEditCellCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int focused = model->focused == args->cell;
    EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = &err_msg);
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = g_cli_notebook_open_owner);
    if (text == NULL)
    {
        return 0;
    }
    CliNotebookCell *const cell = &model->cell[args->cell];
    cell->type = (cell->type == CLI_NOTEBOOK_CODE) ? CLI_NOTEBOOK_MARKDOWN : CLI_NOTEBOOK_CODE;
    EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = args->cell);
    (void)EMBED_CALL(cli_notebook_model_internal.source_stored, CliNotebookModelCtx, .model = model, .cell = args->cell,
                     .text = text, .size = size);
    g_cli_guard->persistent_buf_release(text, g_cli_notebook_open_owner);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = args->cell);
    if (focused)
    {
        (void)EMBED_CALL(cli_notebook_edit_focus.focus, CliNotebookEditFocusCfg, .key = args->key, .cell = args->cell,
                         .line = 0ULL, .column = 0ULL, .err_msg = &err_msg);
    }
    return 1;
}

int cli_notebook_cell_move(const CliNotebookEditCellCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    const long long target = (long long)args->cell + (long long)((args->by < 0) ? -1 : 1);
    if ((model == NULL) || (args->cell >= model->cells) || (target < 0LL) || (target >= (long long)model->cells))
    {
        return 0;
    }
    // the target stands among the cells, which an unsigned int counts
    const unsigned int other = (unsigned int)target;
    const unsigned int upper = (other < args->cell) ? other : args->cell;
    const unsigned int lower = upper + 1u;
    // the two cells' outputs swapped with them, the outputs standing in the order of their cells
    CliNotebookCell *const up = &model->cell[upper];
    CliNotebookCell *const down = &model->cell[lower];
    const unsigned int count = up->outputs + down->outputs;
    CliNotebookOutput *const swapped =
        (count != 0u) ? g_cli_guard->temporary_buf_alloc(count * sizeof(CliNotebookOutput), g_cli_notebook_open_owner)
                      : NULL;
    if ((count != 0u) && (swapped == NULL))
    {
        return 0;
    }
    if (down->outputs != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = swapped,
                                .src = &model->output[down->output_first],
                                .bytes = down->outputs * sizeof(CliNotebookOutput)});
    }
    if (up->outputs != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = &swapped[down->outputs],
                                .src = &model->output[up->output_first],
                                .bytes = up->outputs * sizeof(CliNotebookOutput)});
    }
    if (count != 0u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &model->output[up->output_first], .src = swapped, .bytes = count * sizeof(CliNotebookOutput)});
    }
    g_cli_guard->persistent_buf_release(swapped, g_cli_notebook_open_owner);
    const unsigned int first = up->output_first;
    const CliNotebookCell upper_cell = *up;
    *up = *down;
    *down = upper_cell;
    up->output_first = first;
    down->output_first = first + up->outputs;
    model->focused = (model->focused == upper) ? lower : ((model->focused == lower) ? upper : model->focused);
    model->menu_cell = (model->menu_cell == upper) ? lower : ((model->menu_cell == lower) ? upper : model->menu_cell);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = upper);
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = CLI_NOTEBOOK_NONE);
    return 1;
}

int cli_notebook_cell_outputs_clear(const CliNotebookEditCellCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        return 0;
    }
    EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = args->cell);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = CLI_NOTEBOOK_NONE);
    return 1;
}

int cli_notebook_cell_remove(const CliNotebookEditCellCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        return 0;
    }
    const CliNotebookCell cell = model->cell[args->cell];
    CliNotebookGone *const gone =
        (cell.first_run != 0u) ? g_cli_guard->persistent_buf_alloc((model->gone_count + 1ULL) * sizeof(CliNotebookGone),
                                                                   g_cli_notebook_open_owner)
                               : NULL;
    if ((cell.first_run != 0u) && (gone == NULL))
    {
        return 0;
    }
    if (model->focused == args->cell)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key, .err_msg = &err_msg);
    }
    if (gone != NULL)
    {
        if (model->gone_count != 0u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = gone, .src = model->gone, .bytes = model->gone_count * sizeof(CliNotebookGone)});
        }
        gone[model->gone_count] = (CliNotebookGone){cell.first_run, cell.last_run};
        g_cli_guard->persistent_buf_release(model->gone, g_cli_notebook_open_owner);
        model->gone = gone;
        model->gone_count += 1u;
    }
    EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = args->cell);
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    if (!EMBED_CALL(cli_notebook_model_internal.cells_laid, CliNotebookModelCtx, .model = model,
                    .count = model->cells - 1u, .cell = args->cell))
    {
        return 0;
    }
    model->focused = EMBED_CALL(place_moved, NotebookEditCtx, .cell = args->cell, .other = 0u, .place = model->focused);
    model->menu_cell =
        EMBED_CALL(place_moved, NotebookEditCtx, .cell = args->cell, .other = 0u, .place = model->menu_cell);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    EMBED_CALL(edited, NotebookEditCtx, .model = model, .cell = CLI_NOTEBOOK_NONE);
    return 1;
}

int cli_notebook_cell_menu(const CliNotebookEditMenuCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        return 0;
    }
    model->menu_cell = args->cell;
    const CliNotebookCell *const cell = &model->cell[args->cell];
    const int code = cell->type == CLI_NOTEBOOK_CODE;
    // the rows run with the notebook's key, which stands in its model while the menu is open
    cli_menus_own_begin();
    cli_menus_own_row(&(CliItem){"Run", "notebook-run", "Shift+Enter", model->key, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Debug", "notebook-debug", "", model->key, "never", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){code ? "Change to Markdown" : "Change to Code", "notebook-retype", "", model->key, "",
                                 "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(
        &(CliItem){"Move Up", "notebook-up", "", model->key, (args->cell == 0u) ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Move Down", "notebook-down", "", model->key,
                                 ((args->cell + 1u) >= model->cells) ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(
        &(CliItem){"Clear Outputs", "notebook-clear", "", model->key, code ? "" : "never", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Delete", "notebook-delete", "", model->key, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

int cli_notebook_cell_command(const CliNotebookEditMenuCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        return 0;
    }
    const unsigned int cell = (args->cell != CLI_NOTEBOOK_NONE) ? args->cell : model->menu_cell;
    static const char *const s_words[] = {"notebook-run",   "notebook-retype", "notebook-up",    "notebook-down",
                                          "notebook-clear", "notebook-delete", "notebook-debug", "notebook-show-all"};
    unsigned int word = CLI_NOTEBOOK_NONE;
    for (unsigned int at = 0u; at < (sizeof(s_words) / sizeof(s_words[0])); at += 1u)
    {
        word = cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = s_words[at], .cap = 32ULL}) ? at : word;
    }
    if ((word == CLI_NOTEBOOK_NONE) || (cell >= model->cells))
    {
        return word != CLI_NOTEBOOK_NONE;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    switch (word)
    {
    case 0u: {
        (void)EMBED_CALL(cli_notebook_kernel_run.run, CliNotebookKernelRunCfg, .key = args->key, .cell = cell,
                         .err_msg = &err_msg);
        break;
    }
    case 1u: {
        (void)EMBED_CALL(cli_notebook_edit_cell.retype, CliNotebookEditCellCfg, .key = args->key, .cell = cell);
        break;
    }
    case 2u:
    case 3u: {
        (void)EMBED_CALL(cli_notebook_edit_cell.move, CliNotebookEditCellCfg, .key = args->key, .cell = cell,
                         .by = (word == 2u) ? -1 : 1);
        break;
    }
    case 4u: {
        (void)EMBED_CALL(cli_notebook_edit_cell.outputs_clear, CliNotebookEditCellCfg, .key = args->key, .cell = cell);
        break;
    }
    case 5u: {
        (void)EMBED_CALL(cli_notebook_edit_cell.remove, CliNotebookEditCellCfg, .key = args->key, .cell = cell);
        break;
    }
    case 7u: {
        const CliNotebookCell *const one = &model->cell[cell];
        if ((args->output >= one->output_first) && (args->output < (one->output_first + one->outputs)))
        {
            model->output[args->output].whole = 1;
            EMBED_CALL(cli_notebook_model_internal.outputs_settled, CliNotebookModelCtx, .model = model, .cell = cell);
            EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = cell);
        }
        break;
    }
    default: {
        break;
    }
    }
    return 1;
}
