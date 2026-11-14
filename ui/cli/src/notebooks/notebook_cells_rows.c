// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_cells_rows.c: an open notebook's rows: the row each cell starts at settled as the cells change, a row found
// by a binary search over those starts and over its cell's outputs' starts, the rows from the top positioned into a frame
// with their texts, runs and sentences, the top moved by a wheel or to a place, and the sentences the rows show, each
// written into a cell sized to it
#include "../command_line/cli_internal.h"
#include "notebook_cells_internal.h"

// the max bytes a row's sentence takes, a count or a cut line, and the bytes the out of date sentence takes past a
// cell's name each
#define ROWS_SENTENCE 64u
#define ROWS_STALE_HEAD 160ULL
#define ROWS_STALE_CELL 24ULL

// A run of rows of one text's consecutive lines, as the frame's take gathers it: the cell and the output whose text
// it is, CLI_NOTEBOOK_NONE for the source, the first line and the last, the first of its rows, whether its one row
// shows the text whole, and its lines read
typedef struct
{
    unsigned int cell;
    unsigned int output;
    unsigned long long first;
    unsigned long long last;
    unsigned int row;
    int whole;
    char *lines;
    unsigned long long size;
    unsigned long long taken;
} RowsPart;

typedef struct
{
    const CliNotebookModel *model_read;
    unsigned int at;
    unsigned int cell;
    unsigned int output;
    unsigned int which;
    CliNotebookRow *row;
    VerbaLinea *out;
    unsigned int first_run;
    unsigned int last_run;
    unsigned int edited;
    int gone;
    unsigned int count;
    const CliNotebookRow *rows_read;
    RowsPart *part;
    unsigned int *row_part;
} NotebookRowsCtx;

// the cell `cell` of the model read out of its cells
EMBED_INLINE CliNotebookCell cell_of(const NotebookRowsCtx *args)
{
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx,
               .model = (CliNotebookModel *)(embed_word)args->model_read, .cell = args->cell, .cell_out = &cell);
    return cell;
}

// the output `output` of the model read out of its outputs
EMBED_INLINE CliNotebookOutput output_of(const NotebookRowsCtx *args)
{
    CliNotebookOutput output;
    EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx,
               .model = (CliNotebookModel *)(embed_word)args->model_read, .output = args->output,
               .output_out = &output);
    return output;
}

// the row the cell `cell` of the model starts at, the rows of the model for the cell past the last
EMBED_INLINE unsigned int first_row_of(const NotebookRowsCtx *args)
{
    return EMBED_CALL(cli_notebook_model_internal.first_row, CliNotebookModelCtx,
                      .model = (CliNotebookModel *)(embed_word)args->model_read, .cell = args->cell);
}

// the rows the source of the cell `cell` takes: one for a markdown cell not being edited, else a row a line
EMBED_INLINE unsigned int source_rows(const NotebookRowsCtx *args)
{
    const CliNotebookCell cell =
        EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = args->model_read, .cell = args->cell);
    return ((cell.type == CLI_NOTEBOOK_MARKDOWN) && (args->model_read->focused != args->cell)) ? 1u : cell.lines;
}

void cli_notebook_model_rows_settled(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    // every cell's first row counted anew from the first and written into the cells' cell after the cells
    unsigned int row = 0u;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = model->cells_cell, .offset = CLI_NOTEBOOK_FIRST_ROW_AT(model, 0u), .src = &row, .bytes = sizeof(row)});
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        const CliNotebookCell cell = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = at);
        row += EMBED_CALL(source_rows, NotebookRowsCtx, .model_read = model, .cell = at) + cell.output_rows + 1u;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = model->cells_cell,
                                                                 .offset = CLI_NOTEBOOK_FIRST_ROW_AT(model, at + 1u),
                                                                 .src = &row,
                                                                 .bytes = sizeof(row)});
    }
    const unsigned long long rows = row;
    const unsigned long long displayed = (model->displayed != 0u) ? model->displayed : 1ULL;
    const unsigned long long max = (rows > displayed) ? (rows - displayed) : 0ULL;
    model->top = (model->top < max) ? model->top : max;
}

// whether the cell `other`, a cell of the model or a cell gone, run first at `first_run` and last at `last_run` and
// edited at `edited`, changed, ran again or was deleted since the cell `cell` last ran, after it had run before it
EMBED_INLINE int is_on(const NotebookRowsCtx *args)
{
    const unsigned int ran =
        EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = args->model_read, .cell = args->cell).last_run;
    if ((args->first_run == 0u) || (args->first_run >= ran))
    {
        return 0;
    }
    return args->gone || (args->last_run > ran) || (args->edited > args->last_run);
}

void cli_notebook_model_stale_settled(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        CliNotebookCell cell = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = at);
        int stale = 0;
        for (unsigned int other = 0u;
             (cell.type == CLI_NOTEBOOK_CODE) && (cell.last_run != 0u) && !stale && (other < model->cells); other += 1u)
        {
            const CliNotebookCell one = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = other);
            stale = (other != at) &&
                    EMBED_CALL(is_on, NotebookRowsCtx, .model_read = model, .cell = at, .first_run = one.first_run,
                               .last_run = one.last_run, .edited = one.edited, .gone = 0);
        }
        for (unsigned int gone = 0u;
             (cell.type == CLI_NOTEBOOK_CODE) && (cell.last_run != 0u) && !stale && (gone < model->gone_count);
             gone += 1u)
        {
            CliNotebookGone one;
            EMBED_CALL(cli_notebook_model_internal.gone_read, CliNotebookModelCtx, .model = model, .at = gone,
                       .gone_out = &one);
            stale = EMBED_CALL(is_on, NotebookRowsCtx, .model_read = model, .cell = at, .first_run = one.first_run,
                               .last_run = one.last_run, .edited = 0u, .gone = 1);
        }
        cell.stale = stale;
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_read = &cell);
    }
}

// the last of the `count` cells' first rows that is at max `at`: the place a row is under
EMBED_INLINE unsigned int place_under(const NotebookRowsCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->count;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = args->model_read, .cell = middle) <= args->at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the output of the cell `cell` whose rows the row `at` among its outputs' rows is in: a binary search over the
// rows before each
EMBED_INLINE unsigned int output_under(const NotebookRowsCtx *args)
{
    const CliNotebookModel *const model = args->model_read;
    const CliNotebookCell cell = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = args->cell);
    unsigned int low = 0u;
    unsigned int high = cell.outputs;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(output_of, NotebookRowsCtx, .model_read = model, .output = cell.output_first + middle).row <=
            args->at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return cell.output_first + low;
}

// the row `at` of the model into `row`: 1, or 0 past the last
EMBED_INLINE int row_read(const NotebookRowsCtx *args)
{
    const CliNotebookModel *const model = args->model_read;
    if ((model->cells == 0u) ||
        (args->at >= EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = model->cells)))
    {
        return 0;
    }
    const unsigned int place =
        EMBED_CALL(place_under, NotebookRowsCtx, .model_read = model, .count = model->cells, .at = args->at);
    const CliNotebookCell cell = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = place);
    const unsigned int first = EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = place);
    const unsigned int inner = args->at - first;
    const unsigned int sources = EMBED_CALL(source_rows, NotebookRowsCtx, .model_read = model, .cell = place);
    const unsigned int last =
        EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = place + 1u) - first - 1u;
    CliNotebookRow *const row = args->row;
    *row = (CliNotebookRow){CLI_NOTEBOOK_ROW_ADD, place, 0ULL, CLI_NOTEBOOK_NONE, 0u};
    row->flags = ((inner == 0u) ? CLI_NOTEBOOK_FLAG_FIRST : 0u) | ((inner == last) ? CLI_NOTEBOOK_FLAG_LAST : 0u) |
                 ((model->focused == place) ? CLI_NOTEBOOK_FLAG_FOCUSED : 0u) |
                 (cell.running ? CLI_NOTEBOOK_FLAG_RUNNING : 0u) | (cell.failed ? CLI_NOTEBOOK_FLAG_FAILED : 0u) |
                 (cell.stale ? CLI_NOTEBOOK_FLAG_STALE : 0u) |
                 ((cell.type == CLI_NOTEBOOK_MARKDOWN) ? CLI_NOTEBOOK_FLAG_MARKDOWN : 0u);
    if (inner < sources)
    {
        const int positioned_whole = (cell.type == CLI_NOTEBOOK_MARKDOWN) && (model->focused != place);
        row->type = positioned_whole ? CLI_NOTEBOOK_ROW_MARKDOWN : CLI_NOTEBOOK_ROW_SOURCE;
        row->line = inner;
        return 1;
    }
    if (inner >= (sources + cell.output_rows))
    {
        return 1;
    }
    const unsigned int within = inner - sources;
    const unsigned int place_output =
        EMBED_CALL(output_under, NotebookRowsCtx, .model_read = model, .cell = place, .at = within);
    const CliNotebookOutput output =
        EMBED_CALL(output_of, NotebookRowsCtx, .model_read = model, .output = place_output);
    const unsigned int at = within - output.row;
    row->output = place_output;
    row->flags |= (output.stderr_stream ? CLI_NOTEBOOK_FLAG_STDERR : 0u);
    row->type = CLI_NOTEBOOK_ROW_OUTPUT;
    if ((output.rows == output.lines) || (at < CLI_NOTEBOOK_CUT_LINES))
    {
        row->line = at;
        return 1;
    }
    row->type = (at == CLI_NOTEBOOK_CUT_LINES) ? CLI_NOTEBOOK_ROW_CUT : CLI_NOTEBOOK_ROW_OUTPUT;
    row->line = (at == CLI_NOTEBOOK_CUT_LINES) ? CLI_NOTEBOOK_CUT_LINES : (output.lines - (output.rows - at));
    return 1;
}

unsigned int cli_notebook_model_rows(const CliNotebookCellsRowCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    return ((model != NULL) && (model->cells != 0u))
               ? EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = model->cells)
               : 0u;
}

int cli_notebook_model_row(const CliNotebookCellsRowCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    return (model != NULL) &&
           EMBED_CALL(row_read, NotebookRowsCtx, .model_read = model, .at = args->at, .row = args->row);
}

int cli_notebook_model_block(const CliNotebookCellsRowCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->at >= model->cells))
    {
        return 0;
    }
    const CliNotebookCell cell = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = args->at);
    const unsigned int first = EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = args->at);
    *args->block = (CliNotebookBlock){
        first,       EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = args->at + 1u) - first,
        cell.type,   cell.stale,
        cell.failed, cell.running};
    return 1;
}

// the sentence `which` of the cell `cell` and its output `output` written into `out`
EMBED_INLINE void sentence_written(const NotebookRowsCtx *args)
{
    const CliNotebookModel *const model = args->model_read;
    VerbaLinea *const out = args->out;
    const int has_cell = args->cell < model->cells;
    CliNotebookCell cell_read = {0};
    if (has_cell)
    {
        cell_read = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = args->cell);
    }
    const CliNotebookCell *const cell = has_cell ? &cell_read : NULL;
    const int has_output = (args->output < model->outputs);
    const unsigned int lines =
        has_output ? EMBED_CALL(output_of, NotebookRowsCtx, .model_read = model, .output = args->output).lines : 0u;
    switch (args->which)
    {
    case CLI_NOTEBOOK_TEXT_COUNT: {
        if ((cell == NULL) || (cell->type != CLI_NOTEBOOK_CODE))
        {
            break;
        }
        mmgr_line_ch(out, '[');
        if (cell->running)
        {
            mmgr_line_ch(out, '*');
        }
        else if (cell->count != CLI_NOTEBOOK_NONE)
        {
            mmgr_line_decimal(out, cell->count, 1u);
        }
        else
        {
            mmgr_line_ch(out, ' ');
        }
        mmgr_line_ch(out, ']');
        break;
    }
    case CLI_NOTEBOOK_TEXT_STATE: {
        static const char *const s_states[] = {"no kernel", "starting", "idle", "busy"};
        if ((model->kernel == NULL) || (model->state == CLI_NOTEBOOK_KERNEL_NONE))
        {
            mmgr_line_put(out, s_states[CLI_NOTEBOOK_KERNEL_NONE]);
            break;
        }
        mmgr_line_put(out, (model->kernel_display[0] != '\0') ? model->kernel_display : model->kernel_name);
        mmgr_line_put(out, ": ");
        mmgr_line_put(out, s_states[model->state]);
        break;
    }
    case CLI_NOTEBOOK_TEXT_STALE: {
        if ((cell == NULL) || !cell->stale)
        {
            break;
        }
        mmgr_line_put(out, "Out of date: it is on ");
        int listed = 0;
        for (unsigned int other = 0u; other < model->cells; other += 1u)
        {
            const CliNotebookCell one = EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = other);
            if ((other == args->cell) ||
                !EMBED_CALL(is_on, NotebookRowsCtx, .model_read = model, .cell = args->cell,
                            .first_run = one.first_run, .last_run = one.last_run, .edited = one.edited, .gone = 0))
            {
                continue;
            }
            mmgr_line_put(out, listed ? ", cell " : "cell ");
            mmgr_line_decimal(out, other + 1ULL, 1u);
            listed = 1;
        }
        for (unsigned int gone = 0u; gone < model->gone_count; gone += 1u)
        {
            CliNotebookGone one;
            EMBED_CALL(cli_notebook_model_internal.gone_read, CliNotebookModelCtx,
                       .model = (CliNotebookModel *)(embed_word)model, .at = gone, .gone_out = &one);
            if (EMBED_CALL(is_on, NotebookRowsCtx, .model_read = model, .cell = args->cell,
                           .first_run = one.first_run, .last_run = one.last_run, .edited = 0u, .gone = 1))
            {
                mmgr_line_put(out, listed ? ", a cell since deleted" : "a cell since deleted");
                listed = 1;
            }
        }
        mmgr_line_put(out, ", changed, run again or deleted since this cell ran");
        break;
    }
    case CLI_NOTEBOOK_TEXT_CUT: {
        if (lines <= (2u * CLI_NOTEBOOK_CUT_LINES))
        {
            break;
        }
        mmgr_line_put(out, "... ");
        mmgr_line_decimal(out, lines - (2u * CLI_NOTEBOOK_CUT_LINES), 1u);
        mmgr_line_put(out, " lines ...");
        break;
    }
    case CLI_NOTEBOOK_TEXT_MORE: {
        if (lines <= (2u * CLI_NOTEBOOK_CUT_LINES))
        {
            break;
        }
        mmgr_line_put(out, "Show all ");
        mmgr_line_decimal(out, lines, 1u);
        mmgr_line_put(out, " lines");
        break;
    }
    case CLI_NOTEBOOK_TEXT_PLACEHOLDER: {
        mmgr_line_put(out, "Markdown");
        break;
    }
    default: {
        break;
    }
    }
}

const char *cli_notebook_model_text(const CliNotebookCellsTextCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    *args->length_out = 0ULL;
    if (model == NULL)
    {
        return "";
    }
    // the sentence written into the model's own bytes, cut to them
    VerbaLinea line = {model->sentence, sizeof(model->sentence), 0ULL};
    EMBED_CALL(sentence_written, NotebookRowsCtx, .model_read = model, .which = args->which, .cell = args->cell,
               .output = args->output, .out = &line);
    model->sentence_length = mmgr_line_ok(&line) ? line.at : sizeof(model->sentence) - 1ULL;
    model->sentence[model->sentence_length] = '\0';
    *args->length_out = model->sentence_length;
    return model->sentence;
}

void cli_notebook_model_scroll(const CliNotebookCellsScrollCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->cells == 0u))
    {
        return;
    }
    const long long moved = (long long)model->top + (long long)args->rows;
    // a top moved above the first row is at it, and every row count is far below the signed 64-bit range
    model->top = (moved > 0LL) ? (unsigned long long)moved : 0ULL;
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = model->cells);
}

void cli_notebook_model_to(const CliNotebookCellsScrollCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->cells == 0u))
    {
        return;
    }
    const unsigned long long rows =
        EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = model->cells);
    const unsigned long long permille = (args->permille < 1000u) ? args->permille : 1000ULL;
    const unsigned long long center = (rows * permille) / 1000ULL;
    const unsigned long long half = model->displayed / 2ULL;
    model->top = (center > half) ? (center - half) : 0ULL;
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = model->cells);
}

// the parts the rows gather into: rows of consecutive lines of one text one part, a markdown row a part of its own
// with its text whole; their count, and the part of each row into `row_part`
EMBED_INLINE unsigned int parts_gathered(const NotebookRowsCtx *args)
{
    const CliNotebookModel *const model = args->model_read;
    const CliNotebookRow *const row = args->rows_read;
    RowsPart *const part = args->part;
    unsigned int *const row_part = args->row_part;
    unsigned int parts = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        row_part[at] = CLI_NOTEBOOK_NONE;
        const int sourced = (row[at].type == CLI_NOTEBOOK_ROW_SOURCE) || (row[at].type == CLI_NOTEBOOK_ROW_MARKDOWN);
        if (!sourced && (row[at].type != CLI_NOTEBOOK_ROW_OUTPUT))
        {
            continue;
        }
        const unsigned int output = sourced ? CLI_NOTEBOOK_NONE : row[at].output;
        const int whole = row[at].type == CLI_NOTEBOOK_ROW_MARKDOWN;
        RowsPart *const before = (parts != 0u) ? &part[parts - 1u] : NULL;
        if ((before != NULL) && !whole && !before->whole && (before->cell == row[at].cell) &&
            (before->output == output) && (before->last + 1ULL == row[at].line))
        {
            before->last = row[at].line;
            row_part[at] = parts - 1u;
            continue;
        }
        const unsigned long long last =
            whole ? (EMBED_CALL(cell_of, NotebookRowsCtx, .model_read = model, .cell = row[at].cell).lines - 1ULL)
                  : row[at].line;
        part[parts] = (RowsPart){row[at].cell, output, row[at].line, last, at, whole, NULL, 0ULL, 0ULL};
        row_part[at] = parts;
        parts += 1u;
    }
    return parts;
}

int cli_notebook_model_frame_alloc(const CliNotebookCellsFrameCfg *args)
{
    *args->frame_out = NULL;
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        return 0;
    }
    model->displayed = args->count;
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = model->cells);
    const unsigned int total =
        (model->cells != 0u) ? EMBED_CALL(first_row_of, NotebookRowsCtx, .model_read = model, .cell = model->cells)
                             : 0u;
    // the top is below the rows, which an unsigned int counts
    const unsigned int top = (unsigned int)model->top;
    const unsigned int count = ((total - top) < args->count) ? (total - top) : args->count;
    // the rows, the parts they gather into and the part of each, for this take alone
    const unsigned long long scratch_bytes =
        ((unsigned long long)count * (sizeof(CliNotebookRow) + sizeof(RowsPart) + sizeof(unsigned int))) + 1ULL;
    unsigned char *const scratch = g_cli_guard->temporary_buf_alloc(scratch_bytes, g_cli_notebook_open_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    CliNotebookRow *const row = (CliNotebookRow *)(void *)scratch;
    RowsPart *const part = (RowsPart *)(void *)(scratch + ((unsigned long long)count * sizeof(CliNotebookRow)));
    unsigned int *const row_part =
        (unsigned int *)(void *)(scratch + ((unsigned long long)count * (sizeof(CliNotebookRow) + sizeof(RowsPart))));
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        (void)EMBED_CALL(row_read, NotebookRowsCtx, .model_read = model, .at = top + at, .row = &row[at]);
    }
    const unsigned int parts = EMBED_CALL(parts_gathered, NotebookRowsCtx, .model_read = model, .rows_read = row,
                                          .count = count, .part = part, .row_part = row_part);
    // each part's lines read, and the bytes the rows' sentences take
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = part[at].cell,
                   .output = part[at].output, .out = key);
        part[at].lines = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = key,
                                    .first = part[at].first, .last = part[at].last, .size_pointer = &part[at].size,
                                    .state = NULL, .capacity = 0ULL, .owner = g_cli_notebook_open_owner);
        bytes += (part[at].lines != NULL) ? part[at].size : 0ULL;
    }
    bytes += (unsigned long long)count * ROWS_SENTENCE;
    const unsigned long long head = sizeof(CliNotebookFrame) +
                                    ((unsigned long long)count * sizeof(CliNotebookFrameRow)) +
                                    ((unsigned long long)parts * sizeof(CliNotebookFramePart));
    // the frame is the caller's for one call, released as the caller ends with it
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(head + bytes + 1ULL, g_cli_notebook_open_owner);
    CliNotebookFrame *const frame = (CliNotebookFrame *)(void *)cell;
    if (frame != NULL)
    {
        *frame = (CliNotebookFrame){
            model->top,
            count,
            total,
            model->focused,
            parts,
            (CliNotebookFrameRow *)(void *)(cell + sizeof(CliNotebookFrame)),
            (CliNotebookFramePart *)(void *)(cell + sizeof(CliNotebookFrame) +
                                             ((unsigned long long)count * sizeof(CliNotebookFrameRow))),
            (char *)(cell + head),
            NULL};
    }
    VerbaLinea positioned = {(frame != NULL) ? frame->bytes : NULL, bytes + 1ULL, 0ULL};
    for (unsigned int at = 0u; (frame != NULL) && (at < parts); at += 1u)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = part[at].cell,
                   .output = part[at].output, .out = key);
        const unsigned long long lines = part[at].last - part[at].first + 1ULL;
        unsigned long long *const number =
            g_cli_guard->temporary_buf_alloc(lines * sizeof(unsigned long long), g_cli_notebook_open_owner);
        for (unsigned long long line = 0ULL; (number != NULL) && (line < lines); line += 1ULL)
        {
            number[line] = part[at].first + line;
        }
        frame->part[at].runs = (number != NULL) ? EMBED_CALL(cli_document_runs.runs_alloc, CliDocumentRunsCfg,
                                                             .key = key, .number = number, .count = lines,
                                                             .mirrored = 0, .owner = g_cli_notebook_open_owner)
                                                : NULL;
        frame->part[at].labels = EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = key,
                                            .owner = g_cli_notebook_open_owner);
        g_cli_guard->persistent_buf_release(number, g_cli_notebook_open_owner);
    }
    // each row's text, the next line of its part's lines, or its part's lines whole, and its sentence
    for (unsigned int at = 0u; (frame != NULL) && (at < count); at += 1u)
    {
        CliNotebookFrameRow *const framed = &frame->row[at];
        *framed = (CliNotebookFrameRow){row[at], positioned.at, 0ULL, 0ULL, 0ULL, row_part[at], 0u};
        RowsPart *const one = (row_part[at] != CLI_NOTEBOOK_NONE) ? &part[row_part[at]] : NULL;
        if ((one != NULL) && (one->lines != NULL))
        {
            const char *const from = one->lines + one->taken;
            const unsigned long long left = one->size - one->taken;
            const char *const end = (one->whole || (left == 0ULL))
                                        ? NULL
                                        : memor.chr(&(MemoriaCfg){.src = from, .val = '\n', .bytes = left});
            const unsigned long long length = (end != NULL) ? (unsigned long long)(end - from) : left;
            mmgr_line_put_n(&positioned, from, length);
            framed->length = length;
            framed->part_line = at - one->row;
            one->taken += length + ((end != NULL) ? 1ULL : 0ULL);
        }
        framed->label = positioned.at;
        const unsigned int which =
            (row[at].type == CLI_NOTEBOOK_ROW_CUT)                                      ? CLI_NOTEBOOK_TEXT_CUT
            : ((row[at].type == CLI_NOTEBOOK_ROW_SOURCE) && (row[at].line == 0ULL))     ? CLI_NOTEBOOK_TEXT_COUNT
            : ((row[at].type == CLI_NOTEBOOK_ROW_MARKDOWN) && (framed->length == 0ULL)) ? CLI_NOTEBOOK_TEXT_PLACEHOLDER
                                                                                        : CLI_NOTEBOOK_NONE;
        EMBED_CALL(sentence_written, NotebookRowsCtx, .model_read = model, .which = which, .cell = row[at].cell,
                   .output = row[at].output, .out = &positioned);
        framed->label_length = positioned.at - framed->label;
    }
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        g_cli_guard->persistent_buf_release(part[at].lines, g_cli_notebook_open_owner);
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_notebook_open_owner);
    if (frame == NULL)
    {
        return 0;
    }
    if (model->focused != CLI_NOTEBOOK_NONE)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = model->focused,
                   .output = CLI_NOTEBOOK_NONE, .out = key);
        frame->view = cli_view_frame_alloc(key);
    }
    *args->frame_out = frame;
    return 1;
}

void cli_notebook_model_frame_release(const CliNotebookCellsFrameCfg *args)
{
    CliNotebookFrame *const frame = *args->frame_out;
    if (frame == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < frame->parts; at += 1u)
    {
        g_cli_guard->persistent_buf_release(frame->part[at].runs, g_cli_notebook_open_owner);
        g_cli_guard->persistent_buf_release(frame->part[at].labels, g_cli_notebook_open_owner);
    }
    if (frame->view != NULL)
    {
        cli_view_frame_release(frame->view);
    }
    g_cli_guard->persistent_buf_release(frame, g_cli_notebook_open_owner);
    *args->frame_out = NULL;
}
