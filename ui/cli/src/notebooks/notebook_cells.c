// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_cells.c: the open notebooks as models: each read from its file's text into a tree of its own, its cells'
// sources and its outputs' texts stored in the document store, written back through the notebook writer, and its
// kernel's messages copied by the receiver into the model's ring on the kernel's thread and applied by the step on the
// view's thread to the cell whose run each answers
#include "../command_line/command_line.h"
#include "../plugins/plugins.h"
#include "notebook_cells_internal.h"
#include "notebook_internal.h"

// the nodes and text bytes a notebook's tree has past what its text counts, the most times a read is tried again in a
// tree twice as large, the milliseconds a kernel has to stop as its notebook closes, and the nodes and bytes a
// message's tree has for its output's type put into it
#define CELLS_NODES_MORE 64u
#define CELLS_NODES_LINE 16u
#define CELLS_TEXT_MORE 4096ULL
#define CELLS_READS 4u
#define CELLS_STOP_MS 2000ULL
#define CELLS_MESSAGE_NODES 8u
#define CELLS_MESSAGE_TEXT 64ULL

// the models open, a pointer each in one movable cell taken once at their most, and what wakes the view's thread
static const CarcerMovableEntry *s_model_cell;
static unsigned int s_models;
static void (*s_woken)(void);

typedef struct
{
    CliNotebookModel *model;
    const CliJson *json_read;
    unsigned int node;
    const char *name;
    unsigned int cell;
    unsigned int output;
    unsigned int type;
    int stderr_stream;
    const char *text;
    unsigned long long size;
    const char *json_text;
    unsigned long long json_size;
    unsigned long long *length_pointer;
    VerbaLinea *out;
    CliJson *json_out;
    CfgJsonToken *token;
    unsigned int tokens;
} NotebookCellsCtx;

// the member `name` of the node `node` of the tree, CLI_JSON_NONE for none
EMBED_INLINE unsigned int member_of(const NotebookCellsCtx *args)
{
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json_read,
                      .object = args->node, .name = args->name);
}

// the letters of the string member `name` of the node `node`, the empty text where it is none, its bytes into
// `length_pointer` where that is not NULL
EMBED_INLINE const char *string_of(const NotebookCellsCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_read,
                   .node = EMBED_CALL(member_of, NotebookCellsCtx, .json_read = args->json_read, .node = args->node,
                                      .name = args->name),
                   .length_pointer = &length);
    if (args->length_pointer != NULL)
    {
        *args->length_pointer = (text != NULL) ? length : 0ULL;
    }
    return (text != NULL) ? text : "";
}

// the whole number the member `name` of the node `node` writes, CLI_NOTEBOOK_NONE where it is no number
EMBED_INLINE unsigned int number_of(const NotebookCellsCtx *args)
{
    const unsigned int node =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = args->json_read, .node = args->node, .name = args->name);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json_read, .node = node,
                    .type = CFG_JSON_NUMBER))
    {
        return CLI_NOTEBOOK_NONE;
    }
    const CliJsonNode *const number = &args->json_read->node[node];
    unsigned long long value = 0ULL;
    for (unsigned long long at = 0ULL; at < number->length; at += 1ULL)
    {
        const char digit = args->json_read->text[number->text + at];
        if ((digit < '0') || (digit > '9'))
        {
            break;
        }
        value = (value * 10ULL) + (unsigned long long)(digit - '0');
    }
    // a run count is far below the 32-bit place no count reaches
    return (value < CLI_NOTEBOOK_NONE) ? (unsigned int)value : CLI_NOTEBOOK_NONE;
}

// the ring's gate shut, the receiver and the step each shutting it only to copy into the ring or to take its cell
EMBED_INLINE void ring_locked(const NotebookCellsCtx *args)
{
    cli_gate_shut(&args->model->ring_lock);
}

// the ring's movable cell taken anew at `capacity` bytes, the messages pending carried into it through a cell of
// this call and the cell they stood in given back: 1, or 0 where the cells cannot meet it, the ring as it was
EMBED_INLINE int ring_grown(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const CarcerMovableEntry *const grown = g_cli_guard->movable_buf_alloc(args->size, g_cli_notebook_open_owner);
    if (grown == NULL)
    {
        return 0;
    }
    if (model->ring_used != 0ULL)
    {
        unsigned char *const carried = g_cli_guard->temporary_buf_alloc(model->ring_used, g_cli_notebook_open_owner);
        if (carried == NULL)
        {
            (void)g_cli_guard->movable_buf_release(grown, g_cli_notebook_open_owner);
            return 0;
        }
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = model->ring_cell, .offset = 0ULL, .dst = carried, .bytes = (embed_word)model->ring_used});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = grown, .offset = 0ULL, .src = carried, .bytes = (embed_word)model->ring_used});
        g_cli_guard->persistent_buf_release(carried, g_cli_notebook_open_owner);
    }
    if (model->ring_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(model->ring_cell, g_cli_notebook_open_owner);
    }
    model->ring_cell = grown;
    model->ring_capacity = args->size;
    return 1;
}

// a message of `size` bytes of `text` put after the ring's messages, its size before it, the ring's cell taken anew at
// twice its capacity, or the bytes pending where that is more, where it does not fit: 1, or 0 where the cells cannot
// meet it, the message lost
EMBED_INLINE int ring_put(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned long long need = model->ring_used + sizeof(unsigned long long) + args->size;
    EMBED_CALL(ring_locked, NotebookCellsCtx, .model = model);
    int put = 1;
    if (need > model->ring_capacity)
    {
        const unsigned long long doubled = model->ring_capacity * 2ULL;
        const unsigned long long least = (doubled > CLI_NOTEBOOK_RING_LEAST) ? doubled : CLI_NOTEBOOK_RING_LEAST;
        put = EMBED_CALL(ring_grown, NotebookCellsCtx, .model = model, .size = (need > least) ? need : least);
    }
    if (put)
    {
        const unsigned long long size = args->size;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = model->ring_cell, .offset = (embed_word)model->ring_used, .src = &size, .bytes = sizeof(size)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = model->ring_cell,
                                .offset = (embed_word)(model->ring_used + sizeof(size)),
                                .src = args->text,
                                .bytes = (embed_word)args->size});
        model->ring_used = need;
    }
    cli_gate_opened(&model->ring_lock);
    return put;
}

void cli_notebook_model_received(const CliKernelRecvCfg *args)
{
    CliNotebookModel *const model = args->context;
    if ((model == NULL) ||
        !EMBED_CALL(ring_put, NotebookCellsCtx, .model = model, .text = args->message, .size = args->size))
    {
        return;
    }
    if (s_woken != NULL)
    {
        s_woken();
    }
}

void cli_notebook_model_ring_cleared(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    EMBED_CALL(ring_locked, NotebookCellsCtx, .model = model);
    const CarcerMovableEntry *const ring = model->ring_cell;
    model->ring_cell = NULL;
    model->ring_used = 0ULL;
    model->ring_capacity = 0ULL;
    cli_gate_opened(&model->ring_lock);
    if (ring != NULL)
    {
        (void)g_cli_guard->movable_buf_release(ring, g_cli_notebook_open_owner);
    }
}

CliNotebookModel *cli_notebook_model_of(const CliNotebookModelCtx *args)
{
    for (unsigned int at = 0u; (args->key != NULL) && (at < s_models); at += 1u)
    {
        CliNotebookModel *model = NULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                .offset = CLI_NOTEBOOK_MODEL_AT(at),
                                                                .dst = &model,
                                                                .bytes = sizeof(CliNotebookModel *)});
        if (cellul.eq(&(CatenaFinitaCfg){.src = model->key, .other = args->key, .cap = CLI_PATH_MAX_LEN}))
        {
            return model;
        }
    }
    return NULL;
}

CliNotebookModel *cli_notebook_model_at(const CliNotebookModelCtx *args)
{
    CliNotebookModel *model = NULL;
    if (args->at < s_models)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                .offset = CLI_NOTEBOOK_MODEL_AT(args->at),
                                                                .dst = &model,
                                                                .bytes = sizeof(CliNotebookModel *)});
    }
    return model;
}

void cli_notebook_model_cell_read(const CliNotebookModelCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->model->cells_cell,
                                                            .offset = CLI_NOTEBOOK_CELL_AT(args->cell),
                                                            .dst = args->cell_out,
                                                            .bytes = sizeof(CliNotebookCell)});
}

void cli_notebook_model_cell_written(const CliNotebookModelCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->model->cells_cell,
                                                             .offset = CLI_NOTEBOOK_CELL_AT(args->cell),
                                                             .src = args->cell_read,
                                                             .bytes = sizeof(CliNotebookCell)});
}

void cli_notebook_model_output_read(const CliNotebookModelCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->model->outputs_cell,
                                                            .offset = CLI_NOTEBOOK_OUTPUT_AT(args->output),
                                                            .dst = args->output_out,
                                                            .bytes = sizeof(CliNotebookOutput)});
}

void cli_notebook_model_output_written(const CliNotebookModelCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->model->outputs_cell,
                                                             .offset = CLI_NOTEBOOK_OUTPUT_AT(args->output),
                                                             .src = args->output_read,
                                                             .bytes = sizeof(CliNotebookOutput)});
}

unsigned int cli_notebook_model_first_row(const CliNotebookModelCtx *args)
{
    unsigned int row = 0u;
    if (args->model->cells_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->model->cells_cell,
                                .offset = CLI_NOTEBOOK_FIRST_ROW_AT(args->model, args->cell),
                                .dst = &row,
                                .bytes = sizeof(row)});
    }
    return row;
}

void cli_notebook_model_gone_read(const CliNotebookModelCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->model->gone_cell,
                                                            .offset = CLI_NOTEBOOK_GONE_AT(args->at),
                                                            .dst = args->gone_out,
                                                            .bytes = sizeof(CliNotebookGone)});
}

void cli_notebook_model_key(const CliNotebookModelCtx *args)
{
    const CliNotebookModel *const model = args->model;
    VerbaLinea line = {
        args->out, cellul.len(&(CatenaFinitaCfg){.src = model->key, .cap = CLI_PATH_MAX_LEN}) + CLI_NOTEBOOK_KEY_MORE,
        0ULL};
    mmgr_line_put(&line, model->key);
    mmgr_line_ch(&line, '#');
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .cell_out = &cell);
    mmgr_line_decimal(&line, cell.serial, 1u);
    if (args->output != CLI_NOTEBOOK_NONE)
    {
        CliNotebookOutput output;
        EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = args->model,
                   .output = args->output, .output_out = &output);
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, output.serial, 1u);
    }
    (void)mmgr_line_finish(&line);
}

// the text under `text` colored by the grammar of the language plugin `name`, none where the plugin has none
EMBED_INLINE void text_colored(const NotebookCellsCtx *args)
{
    char grammar[CLI_NAME_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    grammar[0] = '\0';
    if ((args->name[0] == '\0') || !EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = args->name,
                                               .key = grammar, .err_msg = &err_msg))
    {
        grammar[0] = '\0';
    }
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = args->text,
                     .grammar = grammar, .named = 0);
}

int cli_notebook_model_source_stored(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell);
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    CliShape shape = {0ULL, 0ULL};
    if (!EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = args->text, .size = args->size, .key = key,
                    .shape = &shape))
    {
        return 0;
    }
    const char *const grammar = (cell.type == CLI_NOTEBOOK_CODE)       ? model->kernel_language
                                : (cell.type == CLI_NOTEBOOK_MARKDOWN) ? "markdown"
                                                                       : "";
    EMBED_CALL(text_colored, NotebookCellsCtx, .name = grammar, .text = key);
    // a cell's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    cell.lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_read = &cell);
    return 1;
}

void cli_notebook_model_lines_read(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    CliShape shape = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = key, .shape = &shape);
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell);
    // a cell's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    cell.lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_read = &cell);
}

int cli_notebook_model_cells_laid(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned int count = args->count;
    const unsigned long long cell_bytes = CLI_NOTEBOOK_CELL_AT(count);
    const unsigned long long bytes = cell_bytes + ((count + 1ULL) * sizeof(unsigned int));
    // the cells laid in a cell of this call, the cells before and after the change read out of the cell they stand in,
    // then written once into a movable cell sized to them with the rows' starts after them
    unsigned char *const laid = g_cli_guard->temporary_buf_alloc(bytes, g_cli_notebook_open_owner);
    if (laid == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = laid, .val = 0, .bytes = bytes});
    const unsigned int before = (args->cell < model->cells) ? args->cell : model->cells;
    if (before != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = model->cells_cell, .offset = 0ULL, .dst = laid, .bytes = CLI_NOTEBOOK_CELL_AT(before)});
    }
    // one more: a cell of none at `cell`; one fewer: the cell at `cell` left out; as many: every cell as it stands
    const unsigned int skipped = (count < model->cells) ? 1u : 0u;
    const unsigned int opened = (count > model->cells) ? 1u : 0u;
    const unsigned int after = model->cells - before - ((skipped && (before < model->cells)) ? 1u : 0u);
    if (after != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = model->cells_cell,
                                                                .offset = CLI_NOTEBOOK_CELL_AT(before + skipped),
                                                                .dst = laid + CLI_NOTEBOOK_CELL_AT(before + opened),
                                                                .bytes = CLI_NOTEBOOK_CELL_AT(after)});
    }
    const CarcerMovableEntry *const cells_cell = g_cli_guard->movable_buf_alloc(bytes, g_cli_notebook_open_owner);
    if (cells_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cells_cell, .offset = 0ULL, .src = laid, .bytes = bytes});
        if (model->cells_cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(model->cells_cell, g_cli_notebook_open_owner);
        }
        model->cells_cell = cells_cell;
        model->first_row_at = cell_bytes;
        model->cells = count;
    }
    g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
    return cells_cell != NULL;
}

void cli_notebook_model_bound(const CliNotebookModelCtx *args)
{
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .cell_out = &cell);
    VerbaLinea line = {cell.run_id, sizeof(cell.run_id), 0ULL};
    mmgr_line_put(&line, args->id);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .cell_read = &cell);
}

// the output texts laid anew in a movable cell sized to them, the output `output` taking `json_size` bytes of
// `json_text` in place of its own, every other output's standing, each output's place in them written: 1, or 0 where
// the cells cannot meet it, the texts as they were
EMBED_INLINE int texts_laid(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned long long records = CLI_NOTEBOOK_OUTPUT_AT(model->outputs);
    unsigned long long bytes = 0ULL;
    // the outputs read into a cell of this call, their places moved as the texts are laid, then written back whole
    CliNotebookOutput *const output =
        (model->outputs != 0u) ? g_cli_guard->temporary_buf_alloc(records, g_cli_notebook_open_owner) : NULL;
    if ((model->outputs != 0u) && (output == NULL))
    {
        return 0;
    }
    if (output != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = model->outputs_cell, .offset = 0ULL, .dst = output, .bytes = records});
    }
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        bytes += (at == args->output) ? args->json_size : output[at].json_length;
    }
    char *const laid = (bytes != 0ULL) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_notebook_open_owner) : NULL;
    const CarcerMovableEntry *const text_cell =
        (bytes != 0ULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_notebook_open_owner) : NULL;
    if ((bytes != 0ULL) && ((laid == NULL) || (text_cell == NULL)))
    {
        if (text_cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(text_cell, g_cli_notebook_open_owner);
        }
        g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
        g_cli_guard->persistent_buf_release(output, g_cli_notebook_open_owner);
        return 0;
    }
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        const unsigned long long length = (at == args->output) ? args->json_size : output[at].json_length;
        if ((length != 0ULL) && (at == args->output))
        {
            memor.cpy(&(MemoriaCfg){.dst = laid + used, .src = args->json_text, .bytes = length});
        }
        else if (length != 0ULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = model->text_cell, .offset = output[at].json, .dst = laid + used, .bytes = length});
        }
        output[at].json = used;
        output[at].json_length = length;
        used += length;
    }
    if (bytes != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = text_cell, .offset = 0ULL, .src = laid, .bytes = bytes});
    }
    if (output != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = model->outputs_cell, .offset = 0ULL, .src = output, .bytes = records});
    }
    if (model->text_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(model->text_cell, g_cli_notebook_open_owner);
    }
    model->text_cell = text_cell;
    model->output_used = used;
    g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(output, g_cli_notebook_open_owner);
    return 1;
}

void cli_notebook_model_outputs_settled(const CliNotebookModelCtx *args)
{
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .cell_out = &cell);
    unsigned int rows = 0u;
    for (unsigned int at = 0u; at < cell.outputs; at += 1u)
    {
        CliNotebookOutput output;
        EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = args->model,
                   .output = cell.output_first + at, .output_out = &output);
        output.rows = ((output.lines > (2u * CLI_NOTEBOOK_CUT_LINES)) && !output.whole)
                          ? ((2u * CLI_NOTEBOOK_CUT_LINES) + 1u)
                          : output.lines;
        output.row = rows;
        rows += output.rows;
        EMBED_CALL(cli_notebook_model_internal.output_written, CliNotebookModelCtx, .model = args->model,
                   .output = cell.output_first + at, .output_read = &output);
    }
    cell.output_rows = rows;
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = args->model, .cell = args->cell,
               .cell_read = &cell);
}

// the cells from `cell` on each moved `count` outputs along, the outputs before theirs put in or taken out, `at` 1
// for put in and 0 for taken out
EMBED_INLINE void outputs_shifted(const NotebookCellsCtx *args)
{
    for (unsigned int at = args->cell; at < args->model->cells; at += 1u)
    {
        CliNotebookCell cell;
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = at,
                   .cell_out = &cell);
        cell.output_first = (args->type != 0u) ? cell.output_first + args->tokens : cell.output_first - args->tokens;
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = args->model, .cell = at,
                   .cell_read = &cell);
    }
}

// `size` bytes of `text` stored as the text of the output `output`, its last line end left off, colored by the xterm
// grammar, its lines counted: 1, or 0 where the store refused it
EMBED_INLINE int output_text_stored(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = args->output, .out = key);
    const unsigned long long size =
        ((args->size != 0ULL) && (args->text[args->size - 1ULL] == '\n')) ? args->size - 1ULL : args->size;
    CliShape shape = {0ULL, 0ULL};
    if (!EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = args->text, .size = size, .key = key,
                    .shape = &shape))
    {
        return 0;
    }
    EMBED_CALL(text_colored, NotebookCellsCtx, .name = "xterm", .text = key);
    CliNotebookOutput output;
    EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = model, .output = args->output,
               .output_out = &output);
    // an output's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    output.lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
    output.line_ended = size != args->size;
    EMBED_CALL(cli_notebook_model_internal.output_written, CliNotebookModelCtx, .model = model, .output = args->output,
               .output_read = &output);
    return 1;
}

// the outputs laid anew in a movable cell of `tokens` records, the records before the place `output` as they stand,
// `node` records at the place left out and the records after them following, after a gap of the records the
// change puts in: 1, or 0 where the cells cannot meet it, the outputs as they were
EMBED_INLINE int outputs_laid(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned long long bytes = CLI_NOTEBOOK_OUTPUT_AT(args->tokens);
    const unsigned int before = args->output;
    const unsigned int after = model->outputs - before - args->node;
    const unsigned int opened = args->tokens - before - after;
    const CarcerMovableEntry *const grown =
        (args->tokens != 0u) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_notebook_open_owner) : NULL;
    unsigned char *const laid =
        (args->tokens != 0u) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_notebook_open_owner) : NULL;
    if ((args->tokens != 0u) && ((grown == NULL) || (laid == NULL)))
    {
        if (grown != NULL)
        {
            (void)g_cli_guard->movable_buf_release(grown, g_cli_notebook_open_owner);
        }
        g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
        return 0;
    }
    if (laid != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = laid, .val = 0, .bytes = bytes});
    }
    if (before != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = model->outputs_cell, .offset = 0ULL, .dst = laid, .bytes = CLI_NOTEBOOK_OUTPUT_AT(before)});
    }
    if (after != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = model->outputs_cell,
                                                                .offset = CLI_NOTEBOOK_OUTPUT_AT(before + args->node),
                                                                .dst = laid + CLI_NOTEBOOK_OUTPUT_AT(before + opened),
                                                                .bytes = CLI_NOTEBOOK_OUTPUT_AT(after)});
    }
    if (laid != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = grown, .offset = 0ULL, .src = laid, .bytes = bytes});
    }
    if (model->outputs_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(model->outputs_cell, g_cli_notebook_open_owner);
    }
    model->outputs_cell = grown;
    model->outputs = args->tokens;
    g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
    return 1;
}

// an output of `type` put after the outputs of the cell `cell`, its text `size` bytes of `text` and its JSON
// `json_size` bytes of `json_text`, the outputs taken anew one more: 1, or 0 where the cells cannot meet it
EMBED_INLINE int output_put(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell);
    const unsigned int place = cell.output_first + cell.outputs;
    if (!EMBED_CALL(outputs_laid, NotebookCellsCtx, .model = model, .output = place, .node = 0u,
                    .tokens = model->outputs + 1u))
    {
        return 0;
    }
    const CliNotebookOutput put = {model->next_serial, args->type, args->stderr_stream, 1u, 1u, 0u, 0, 0, 0ULL, 0ULL};
    EMBED_CALL(cli_notebook_model_internal.output_written, CliNotebookModelCtx, .model = model, .output = place,
               .output_read = &put);
    model->next_serial += 1u;
    cell.outputs += 1u;
    cell.failed = cell.failed || (args->type == CLI_NOTEBOOK_ERROR);
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_read = &cell);
    EMBED_CALL(outputs_shifted, NotebookCellsCtx, .model = model, .cell = args->cell + 1u, .tokens = 1u, .type = 1u);
    (void)EMBED_CALL(texts_laid, NotebookCellsCtx, .model = model, .output = place, .json_text = args->json_text,
                     .json_size = args->json_size);
    (void)EMBED_CALL(output_text_stored, NotebookCellsCtx, .model = model, .cell = args->cell, .output = place,
                     .text = args->text, .size = args->size);
    EMBED_CALL(cli_notebook_model_internal.outputs_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    return 1;
}

void cli_notebook_model_outputs_released(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell);
    for (unsigned int at = 0u; at < cell.outputs; at += 1u)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
                   .output = cell.output_first + at, .out = key);
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    }
    const unsigned int gone = cell.outputs;
    if (gone != 0u)
    {
        (void)EMBED_CALL(outputs_laid, NotebookCellsCtx, .model = model, .output = cell.output_first, .node = gone,
                         .tokens = model->outputs - gone);
    }
    EMBED_CALL(outputs_shifted, NotebookCellsCtx, .model = model, .cell = args->cell + 1u, .tokens = gone, .type = 0u);
    cell.outputs = 0u;
    cell.output_rows = 0u;
    cell.failed = 0;
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_read = &cell);
    (void)EMBED_CALL(texts_laid, NotebookCellsCtx, .model = model, .output = CLI_NOTEBOOK_NONE);
}

// the JSON of the stream `name` of `size` bytes of `text`, as a notebook stores one, written into `out`
EMBED_INLINE void stream_json_written(const NotebookCellsCtx *args)
{
    mmgr_line_put(args->out, "{\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN}));
    mmgr_line_put(args->out, ",\"output_type\":\"stream\",\"text\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->text,
               .length = args->size);
    mmgr_line_ch(args->out, '}');
}

// the stream `name` of `size` bytes of `text` put after the outputs of the cell `cell`, joined to its last output
// where that is a stream of the same name: 1, or 0 where the cells cannot meet it
EMBED_INLINE int stream_put(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell);
    const int stderr_stream = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "stderr", .cap = 8ULL});
    const unsigned int last = (cell.outputs != 0u) ? (cell.output_first + cell.outputs - 1u) : CLI_NOTEBOOK_NONE;
    CliNotebookOutput last_output = {0};
    if (last != CLI_NOTEBOOK_NONE)
    {
        EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = model, .output = last,
                   .output_out = &last_output);
    }
    const int joined = (last != CLI_NOTEBOOK_NONE) && (last_output.type == CLI_NOTEBOOK_STREAM) &&
                       (last_output.stderr_stream == stderr_stream);
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    unsigned long long before = 0ULL;
    char *standing = NULL;
    if (joined)
    {
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
                   .output = last, .out = key);
        standing = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &before,
                              .owner = g_cli_notebook_open_owner);
    }
    // the stream's text: its last line end, where the store left one off, put back before the text joined to it
    const int line_ended = (standing != NULL) && last_output.line_ended;
    const unsigned long long ended = (standing != NULL) ? (before + (line_ended ? 1ULL : 0ULL)) : 0ULL;
    const unsigned long long bytes = ended + args->size;
    const unsigned long long json_capacity = (6ULL * bytes) + CELLS_MESSAGE_TEXT;
    char *const scratch = g_cli_guard->temporary_buf_alloc(bytes + json_capacity + 1ULL, g_cli_notebook_open_owner);
    if (scratch == NULL)
    {
        g_cli_guard->persistent_buf_release(standing, g_cli_notebook_open_owner);
        return 0;
    }
    if (standing != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = scratch, .src = standing, .bytes = before});
    }
    if (line_ended)
    {
        scratch[before] = '\n';
    }
    memor.cpy(&(MemoriaCfg){.dst = scratch + ended, .src = args->text, .bytes = args->size});
    g_cli_guard->persistent_buf_release(standing, g_cli_notebook_open_owner);
    VerbaLinea json = {scratch + bytes, json_capacity, 0ULL};
    EMBED_CALL(stream_json_written, NotebookCellsCtx, .out = &json, .name = args->name, .text = scratch, .size = bytes);
    int put = 0;
    if (!joined)
    {
        put = EMBED_CALL(output_put, NotebookCellsCtx, .model = model, .cell = args->cell, .type = CLI_NOTEBOOK_STREAM,
                         .stderr_stream = stderr_stream, .text = scratch, .size = bytes, .json_text = json.out,
                         .json_size = json.at);
    }
    else
    {
        put = EMBED_CALL(texts_laid, NotebookCellsCtx, .model = model, .output = last, .json_text = json.out,
                         .json_size = json.at) &&
              EMBED_CALL(output_text_stored, NotebookCellsCtx, .model = model, .cell = args->cell, .output = last,
                         .text = scratch, .size = bytes);
        EMBED_CALL(cli_notebook_model_internal.outputs_settled, CliNotebookModelCtx, .model = model,
                   .cell = args->cell);
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_notebook_open_owner);
    return put;
}

// the output node `node` of the tree, a notebook's or a message's content, put after the outputs of the cell `cell`
// as `type`, its text the stream's, the error's traceback, or the text/plain of its data, and its JSON the node
// written: 1, or 0 where the cells cannot meet it
EMBED_INLINE int node_output_put(const NotebookCellsCtx *args)
{
    const CliJson *const json = args->json_read;
    const unsigned int node = args->node;
    if (args->type == CLI_NOTEBOOK_STREAM)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = node,
                                            .name = "text", .length_pointer = &size);
        const char *const name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = node,
                                            .name = "name", .length_pointer = NULL);
        return EMBED_CALL(stream_put, NotebookCellsCtx, .model = args->model, .cell = args->cell,
                          .name = (name[0] != '\0') ? name : "stdout", .text = text, .size = size);
    }
    // the shown text: the traceback's lines joined, or the data's text/plain
    const unsigned int traceback =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = node, .name = "traceback");
    unsigned long long shown = 0ULL;
    for (unsigned int line = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = traceback, .type = CFG_JSON_ARRAY)
                                 ? json->node[traceback].first
                                 : CLI_JSON_NONE;
         line != CLI_JSON_NONE; line = json->node[line].next)
    {
        shown += json->node[line].length + 1ULL;
    }
    unsigned long long plain = 0ULL;
    const char *const plain_text =
        EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json,
                   .node = EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = node, .name = "data"),
                   .name = "text/plain", .length_pointer = &plain);
    shown += (args->type == CLI_NOTEBOOK_ERROR) ? 0ULL : plain;
    const unsigned long long json_capacity =
        EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = json, .node = node,
                   .out = NULL, .indent = 0u, .sorted = 0);
    char *const scratch = g_cli_guard->temporary_buf_alloc(shown + json_capacity + 1ULL, g_cli_notebook_open_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    VerbaLinea text = {scratch, shown + 1ULL, 0ULL};
    if (args->type == CLI_NOTEBOOK_ERROR)
    {
        for (unsigned int line = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                            .node = traceback, .type = CFG_JSON_ARRAY)
                                     ? json->node[traceback].first
                                     : CLI_JSON_NONE;
             line != CLI_JSON_NONE; line = json->node[line].next)
        {
            if (text.at != 0ULL)
            {
                mmgr_line_ch(&text, '\n');
            }
            mmgr_line_put_n(&text, json->text + json->node[line].text, json->node[line].length);
        }
    }
    else
    {
        mmgr_line_put_n(&text, plain_text, plain);
    }
    VerbaLinea written = {scratch + shown + 1ULL, json_capacity, 0ULL};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = node,
               .out = &written, .indent = 0u, .sorted = 0);
    const int put = EMBED_CALL(output_put, NotebookCellsCtx, .model = args->model, .cell = args->cell,
                               .type = args->type, .stderr_stream = args->type == CLI_NOTEBOOK_ERROR, .text = scratch,
                               .size = text.at, .json_text = written.out, .json_size = written.at);
    g_cli_guard->persistent_buf_release(scratch, g_cli_notebook_open_owner);
    return put;
}

// the type an output's type `name` stands for, CLI_NOTEBOOK_NONE for none
EMBED_INLINE unsigned int output_type(const NotebookCellsCtx *args)
{
    static const char *const s_types[] = {"stream", "execute_result", "display_data", "error"};
    for (unsigned int at = 0u; at < (sizeof(s_types) / sizeof(s_types[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_types[at], .cap = 16ULL}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

int cli_notebook_model_kernel_laid(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    // the three texts, each ended by a 0, copied into the model's own bytes in turn, each cut to its bytes
    char *const texts[3] = {model->kernel_name, model->kernel_display, model->kernel_language};
    unsigned long long at = 0ULL;
    for (unsigned int which = 0u; which < 3u; which += 1u)
    {
        const unsigned long long left = (at < args->size) ? args->size - at : 0ULL;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text + at, .cap = left});
        const unsigned long long copied = (length < CLI_NAME_MAX_LEN) ? length : CLI_NAME_MAX_LEN - 1ULL;
        if (copied != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = texts[which], .src = args->text + at, .bytes = copied});
        }
        texts[which][copied] = '\0';
        at += length + 1ULL;
    }
    return 1;
}

// the kernel the notebook's metadata records and its language, the script's language where it records no kernel
EMBED_INLINE int kernel_read(const NotebookCellsCtx *args)
{
    const CliJson *const json = &args->model->json;
    const unsigned int metadata =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = args->model->root, .name = "metadata");
    const unsigned int spec =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = metadata, .name = "kernelspec");
    const unsigned int info =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = metadata, .name = "language_info");
    const char *const name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = spec, .name = "name",
                                        .length_pointer = NULL);
    const char *const display = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = spec,
                                           .name = "display_name", .length_pointer = NULL);
    const char *const spec_language = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = spec,
                                                 .name = "language", .length_pointer = NULL);
    const char *const info_language = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = info,
                                                 .name = "name", .length_pointer = NULL);
    const char *const script_language = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = metadata,
                                                   .name = "language", .length_pointer = NULL);
    const char *const language = (spec_language[0] != '\0')     ? spec_language
                                 : (info_language[0] != '\0')   ? info_language
                                 : (script_language[0] != '\0') ? script_language
                                                                : "python";
    char text[3u * CLI_NAME_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put_most(&line, name, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    mmgr_line_put_most(&line, display, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    mmgr_line_put_most(&line, language, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    return EMBED_CALL(cli_notebook_model_internal.kernel_laid, CliNotebookModelCtx, .model = args->model, .text = text,
                      .size = line.at);
}

// the cells of the tree read laid into the model, each source stored and colored, each output put: 1, or 0 where the
// cells cannot meet them
EMBED_INLINE int cells_read(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const CliJson *const json = &model->json;
    const unsigned int cells =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = model->root, .name = "cells");
    const unsigned int count = json->node[cells].count;
    if (!EMBED_CALL(cli_notebook_model_internal.cells_laid, CliNotebookModelCtx, .model = model, .count = count,
                    .cell = 0u))
    {
        return 0;
    }
    unsigned int at = 0u;
    for (unsigned int node = json->node[cells].first; (node != CLI_JSON_NONE) && (at < count);
         node = json->node[node].next)
    {
        CliNotebookCell cell = {0};
        const char *const type_name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = node,
                                                 .name = "cell_type", .length_pointer = NULL);
        cell.serial = model->next_serial;
        model->next_serial += 1u;
        cell.type = cellul.eq(&(CatenaFinitaCfg){.src = type_name, .other = "markdown", .cap = 16ULL})
                        ? CLI_NOTEBOOK_MARKDOWN
                    : cellul.eq(&(CatenaFinitaCfg){.src = type_name, .other = "raw", .cap = 16ULL}) ? CLI_NOTEBOOK_RAW
                                                                                                    : CLI_NOTEBOOK_CODE;
        cell.origin = node;
        cell.count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = json, .node = node, .name = "execution_count");
        cell.output_first = model->outputs;
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_read = &cell);
        unsigned long long size = 0ULL;
        const char *const source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                              .json_read = json, .cell = node, .length_pointer = &size);
        if (!EMBED_CALL(cli_notebook_model_internal.source_stored, CliNotebookModelCtx, .model = model, .cell = at,
                        .text = source, .size = size))
        {
            return 0;
        }
        const unsigned int outputs =
            EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = node, .name = "outputs");
        for (unsigned int output = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                              .json_read = json, .node = outputs, .type = CFG_JSON_ARRAY)
                                       ? json->node[outputs].first
                                       : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = json->node[output].next)
        {
            const unsigned int type =
                EMBED_CALL(output_type, NotebookCellsCtx,
                           .name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = output,
                                              .name = "output_type", .length_pointer = NULL));
            if ((type != CLI_NOTEBOOK_NONE) && !EMBED_CALL(node_output_put, NotebookCellsCtx, .model = model,
                                                           .json_read = json, .node = output, .cell = at, .type = type))
            {
                return 0;
            }
        }
        at += 1u;
    }
    return 1;
}

// the notebook in the model's text read into a tree of its own, the tree taken anew twice as large while the text
// does not fit it: its root, or CLI_JSON_NONE with why written
EMBED_INLINE unsigned int tree_read(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    unsigned long long lines = 1ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        lines += (args->text[at] == '\n') ? 1ULL : 0ULL;
    }
    const unsigned int tokens = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = args->text,
                                           .size = args->size, .out = NULL) +
                                1u;
    // a notebook's lines and tokens are bounded by the document store's, far below the 32-bit place no count reaches
    unsigned int nodes = tokens + (CELLS_NODES_LINE * (unsigned int)lines) + CELLS_NODES_MORE;
    unsigned long long text_max = (2ULL * args->size) + CELLS_TEXT_MORE;
    CfgJsonToken *const token =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens * sizeof(CfgJsonToken), g_cli_notebook_open_owner);
    unsigned int root = CLI_JSON_NONE;
    for (unsigned int tried = 0u; (token != NULL) && (root == CLI_JSON_NONE) && (tried < CELLS_READS); tried += 1u)
    {
        g_cli_guard->persistent_buf_release(model->json.node, g_cli_notebook_open_owner);
        model->json = (CliJson){0};
        if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &model->json,
                        .nodes_max = nodes, .text_max = text_max, .what = "a notebook open in the window",
                        .owner = g_cli_notebook_open_owner))
        {
            break;
        }
        const unsigned long long said = args->out->at;
        root = EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .path = model->path, .text = args->text,
                          .size = args->size, .json = &model->json, .tokens = token, .tokens_max = tokens,
                          .err_msg = args->out);
        if ((root == CLI_JSON_NONE) && model->json.full && (tried + 1u < CELLS_READS))
        {
            args->out->at = said;
            nodes *= 2u;
            text_max *= 2ULL;
        }
        else if (root == CLI_JSON_NONE)
        {
            break;
        }
    }
    g_cli_guard->persistent_buf_release(token, g_cli_notebook_open_owner);
    return root;
}

// the model's cells and every text it stored let go, its cells released, its place among the models taken out
EMBED_INLINE void model_released(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    for (unsigned int cell = 0u; cell < model->cells; cell += 1u)
    {
        EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = cell);
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = cell,
                   .output = CLI_NOTEBOOK_NONE, .out = key);
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    }
    const CarcerMovableEntry *const cells[5] = {model->cells_cell, model->outputs_cell, model->text_cell,
                                                model->gone_cell, model->ring_cell};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (cells[at] != NULL)
        {
            (void)g_cli_guard->movable_buf_release(cells[at], g_cli_notebook_open_owner);
        }
    }
    g_cli_guard->persistent_buf_release(model->json.node, g_cli_notebook_open_owner);
    unsigned int place = s_models;
    for (unsigned int at = 0u; at < s_models; at += 1u)
    {
        CliNotebookModel *one = NULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                .offset = CLI_NOTEBOOK_MODEL_AT(at),
                                                                .dst = &one,
                                                                .bytes = sizeof(CliNotebookModel *)});
        place = (one == model) ? at : place;
    }
    model->reserved = 0;
    if (place == s_models)
    {
        return;
    }
    // the models after the one taken out moved down a place, each read and written one place lower
    for (unsigned int at = place + 1u; at < s_models; at += 1u)
    {
        CliNotebookModel *moved = NULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                .offset = CLI_NOTEBOOK_MODEL_AT(at),
                                                                .dst = &moved,
                                                                .bytes = sizeof(CliNotebookModel *)});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                 .offset = CLI_NOTEBOOK_MODEL_AT(at - 1u),
                                                                 .src = &moved,
                                                                 .bytes = sizeof(CliNotebookModel *)});
    }
    s_models -= 1u;
    if (s_models == 0u)
    {
        (void)g_cli_guard->movable_buf_release(s_model_cell, g_cli_notebook_open_owner);
        s_model_cell = NULL;
    }
}

int cli_notebook_model_open(const CliNotebookCellsOpenCfg *args)
{
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = args->key);
    const unsigned long long key_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    if (s_models == CLI_NOTEBOOK_MODELS_MOST)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": no more notebooks are open at once");
        return 0;
    }
    // a model holds the lock its kernel's ring is read and written under, so the models stand fixed outside the pool,
    // one place for each notebook open at once, where they punch no hole in the persistent tier
    static CliNotebookModel s_places[CLI_NOTEBOOK_MODELS_MOST];
    CliNotebookModel *model = NULL;
    for (unsigned int at = 0u; (model == NULL) && (at < CLI_NOTEBOOK_MODELS_MOST); at += 1u)
    {
        model = (s_places[at].reserved == 0) ? &s_places[at] : NULL;
    }
    if ((model != NULL) && (s_model_cell == NULL))
    {
        s_model_cell =
            g_cli_guard->movable_buf_alloc(CLI_NOTEBOOK_MODEL_AT(CLI_NOTEBOOK_MODELS_MOST), g_cli_notebook_open_owner);
    }
    if ((model == NULL) || (s_model_cell == NULL) || (key_bytes > sizeof(model->key_text)) ||
        (path_bytes > sizeof(model->path_text)))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        return 0;
    }
    *model = (CliNotebookModel){0};
    model->reserved = 1;
    memor.cpy(&(MemoriaCfg){.dst = model->key_text, .src = args->key, .bytes = key_bytes});
    memor.cpy(&(MemoriaCfg){.dst = model->path_text, .src = args->path, .bytes = path_bytes});
    model->key = model->key_text;
    model->path = model->path_text;
    model->focused = CLI_NOTEBOOK_NONE;
    model->menu_cell = CLI_NOTEBOOK_NONE;
    model->next_serial = 1u;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_model_cell,
                                                             .offset = CLI_NOTEBOOK_MODEL_AT(s_models),
                                                             .src = &model,
                                                             .bytes = sizeof(CliNotebookModel *)});
    s_models += 1u;
    model->root = EMBED_CALL(tree_read, NotebookCellsCtx, .model = model, .text = args->text, .size = args->size,
                             .out = args->err_msg);
    const int read = (model->root != CLI_JSON_NONE) && EMBED_CALL(kernel_read, NotebookCellsCtx, .model = model) &&
                     EMBED_CALL(cells_read, NotebookCellsCtx, .model = model);
    if (!read)
    {
        if (model->root != CLI_JSON_NONE)
        {
            mmgr_line_put(args->err_msg, args->path);
            mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        }
        EMBED_CALL(model_released, NotebookCellsCtx, .model = model);
        return 0;
    }
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = 0u);
    EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
    return 1;
}

void cli_notebook_model_close(const CliNotebookCellsOpenCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        return;
    }
    if (model->kernel != NULL)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = model->kernel, .wait = CELLS_STOP_MS,
                         .err_msg = &err_msg);
        EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = model->kernel,
                   .err_msg = &err_msg);
        model->kernel = NULL;
    }
    if (model->focused != CLI_NOTEBOOK_NONE)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = model->focused,
                   .output = CLI_NOTEBOOK_NONE, .out = key);
        EMBED_CALL(cli_view_table.close, CliViewCfg, .key = key);
    }
    EMBED_CALL(model_released, NotebookCellsCtx, .model = model);
}

// the cell `cell` of the model written into the save's tree `json` as a notebook stores it: its node, CLI_JSON_NONE
// where the tree or the cells cannot meet it
EMBED_INLINE unsigned int cell_written(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliJson *const json = args->json_out;
    CliNotebookCell cell_read;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_out = &cell_read);
    const CliNotebookCell *const cell = &cell_read;
    const unsigned int node =
        (cell->origin != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = cell->origin, .to = json,
                         .from = &model->json)
            : EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_OBJECT);
    if (cell->origin == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "metadata",
                   .value = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json,
                                       .type = CFG_JSON_OBJECT));
    }
    const char *const type_name = (cell->type == CLI_NOTEBOOK_MARKDOWN) ? "markdown"
                                  : (cell->type == CLI_NOTEBOOK_RAW)    ? "raw"
                                                                        : "code";
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node,
               .name = "cell_type",
               .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                   .json = json, .text = type_name,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = type_name, .cap = 16ULL})));
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    unsigned long long size = 0ULL;
    char *const source = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                    .owner = g_cli_notebook_open_owner);
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node, .name = "source",
        .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg, .json = json,
                            .text = (source != NULL) ? source : "", .length = (source != NULL) ? size : 0ULL));
    g_cli_guard->persistent_buf_release(source, g_cli_notebook_open_owner);
    if (cell->type != CLI_NOTEBOOK_CODE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "outputs");
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "execution_count");
        return json->full ? CLI_JSON_NONE : node;
    }
    const unsigned int outputs =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < cell->outputs; at += 1u)
    {
        CliNotebookOutput output;
        EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = model,
                   .output = cell->output_first + at, .output_out = &output);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        // the output's JSON read out of the texts the save copied out of their cell
        const unsigned int read = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                             .json = json, .text = args->text + output.json, .size = output.json_length,
                                             .tokens = args->token, .tokens_max = args->tokens, .err_msg = &err_msg);
        if (read != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = outputs,
                       .value = read);
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node,
               .name = "outputs", .value = outputs);
    char count[16];
    VerbaLinea count_line = {count, sizeof(count), 0ULL};
    mmgr_line_decimal(&count_line, cell->count, 1u);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node,
               .name = "execution_count",
               .value = (cell->count != CLI_NOTEBOOK_NONE)
                            ? EMBED_CALL(cli_json_tree_text_length_type.json_number_init, CliJsonTreeTextLengthTypeCfg,
                                         .json = json, .text = count, .length = count_line.at)
                            : EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json,
                                         .type = CFG_JSON_NULL));
    return json->full ? CLI_JSON_NONE : node;
}

// the kernel picked written into the metadata of the save's tree's root `node` as a notebook's kernelspec
EMBED_INLINE void kernelspec_written(const NotebookCellsCtx *args)
{
    const CliNotebookModel *const model = args->model;
    CliJson *const json = args->json_out;
    unsigned int metadata =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = json, .node = args->node, .name = "metadata");
    if (metadata == CLI_JSON_NONE)
    {
        metadata = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->node,
                   .name = "metadata", .value = metadata);
    }
    const unsigned int spec =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_OBJECT);
    const char *const field[3] = {"display_name", "language", "name"};
    const char *const value[3] = {model->kernel_display, model->kernel_language, model->kernel_name};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = spec,
                   .name = field[at],
                   .value =
                       EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                  .json = json, .text = value[at],
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = value[at], .cap = CLI_NAME_MAX_LEN})));
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = metadata,
               .name = "kernelspec", .value = spec);
}

int cli_notebook_model_save(const CliNotebookCellsSaveCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        mmgr_line_put(args->err_msg, "no notebook is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    // the outputs' texts copied out of their cell for the save, and the save's tree sized to the tree read, the sources
    // as they stand and the outputs' texts
    char *const texts = g_cli_guard->temporary_buf_alloc(model->output_used + 1ULL, g_cli_notebook_open_owner);
    if (texts == NULL)
    {
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        return 0;
    }
    if (model->output_used != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = model->text_cell, .offset = 0ULL, .dst = texts, .bytes = model->output_used});
    }
    texts[model->output_used] = '\0';
    unsigned int nodes = 0u;
    unsigned long long text = 0ULL;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = &model->json,
               .node = model->root, .nodes_pointer = &nodes, .text_u64s = &text);
    unsigned int tokens = 1u;
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        CliNotebookOutput output;
        EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx, .model = model, .output = at,
                   .output_out = &output);
        const unsigned int counted = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                                .text = texts + output.json, .size = output.json_length, .out = NULL);
        tokens = (counted > tokens) ? counted : tokens;
        nodes += counted;
    }
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        CliNotebookCell cell;
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_out = &cell);
        text += (4ULL * CELLS_MESSAGE_TEXT) + (3ULL * cell.lines * CLI_ERR_MSG_MAX_LEN);
    }
    nodes += (CELLS_NODES_LINE * (model->cells + 1u)) + CELLS_NODES_MORE;
    text += model->output_used + CELLS_TEXT_MORE + (3ULL * CLI_NAME_MAX_LEN);
    CliJson json = {0};
    CfgJsonToken *const token =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens * sizeof(CfgJsonToken), g_cli_notebook_open_owner);
    if ((token == NULL) ||
        !EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &json, .nodes_max = nodes,
                    .text_max = text, .what = "a notebook being saved", .owner = g_cli_notebook_open_owner))
    {
        g_cli_guard->persistent_buf_release(token, g_cli_notebook_open_owner);
        g_cli_guard->persistent_buf_release(texts, g_cli_notebook_open_owner);
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        return 0;
    }
    const unsigned int root = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = model->root,
                                         .to = &json, .from = &model->json);
    const unsigned int cells =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &json, .type = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        const unsigned int written = EMBED_CALL(cell_written, NotebookCellsCtx, .model = model, .cell = at,
                                                .json_out = &json, .token = token, .tokens = tokens, .text = texts);
        if (written != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &json, .array = cells,
                       .value = written);
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = root,
               .name = "cells", .value = cells);
    if (model->kernel_name[0] != '\0')
    {
        EMBED_CALL(kernelspec_written, NotebookCellsCtx, .model = model, .json_out = &json, .node = root);
    }
    g_cli_guard->persistent_buf_release(token, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(texts, g_cli_notebook_open_owner);
    // the file's text, Jupyter's lines a line end and a level's space past the tree's one line each
    const unsigned long long capacity =
        (2ULL * EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity, CliJsonTreeNodeOutCfg, .json_read = &json,
                           .node = root, .out = NULL, .indent = 1u, .sorted = 0)) +
        (4ULL * json.used) + CELLS_TEXT_MORE;
    char *const file = json.full ? NULL : g_cli_guard->temporary_buf_alloc(capacity, g_cli_notebook_open_owner);
    int saved = 0;
    if (file != NULL)
    {
        VerbaLinea out = {file, capacity, 0ULL};
        EMBED_CALL(cli_notebook_path_book.write, CliNotebookPathBookCfg, .path = model->path, .json = &json,
                   .book = root, .out = &out);
        const CliFilePiece piece = {file, out.at};
        saved = mmgr_line_ok(&out) && EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx,
                                                 .path = args->full, .count = 1u, .pieces = &piece);
    }
    g_cli_guard->persistent_buf_release(file, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(json.node, g_cli_notebook_open_owner);
    if (!saved)
    {
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    model->dirty = 0;
    return 1;
}

int cli_notebook_model_dirty(const CliNotebookCellsSaveCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    return (model != NULL) && model->dirty;
}

void cli_notebook_model_begin(const CliNotebookCellsStepCfg *args)
{
    s_woken = args->woken;
}

// the cell whose run `id` answers, CLI_NOTEBOOK_NONE for none
EMBED_INLINE unsigned int cell_answered(const NotebookCellsCtx *args)
{
    CliNotebookCell cell;
    for (unsigned int at = 0u; (args->name[0] != '\0') && (at < args->model->cells); at += 1u)
    {
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = at,
                   .cell_out = &cell);
        if (cellul.eq(&(CatenaFinitaCfg){.src = cell.run_id, .other = args->name, .cap = CLI_KERNEL_ID}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

// the type of a message `name` of a cell's run, the output types, then one more for each of the run's other messages,
// CLI_NOTEBOOK_NONE for a message of no run
#define CELLS_INPUT 4u
#define CELLS_CLEAR 5u
#define CELLS_REPLY 6u
EMBED_INLINE unsigned int message_type(const NotebookCellsCtx *args)
{
    static const char *const s_messages[] = {"stream",        "execute_result", "display_data", "error",
                                             "execute_input", "clear_output",   "execute_reply"};
    for (unsigned int at = 0u; at < (sizeof(s_messages) / sizeof(s_messages[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_messages[at], .cap = 16ULL}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

// the kernel's state a status message's content sets, the state as it was where it sets none known
EMBED_INLINE void status_read(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const char *const state = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = args->json_read, .node = args->node,
                                         .name = "execution_state", .length_pointer = NULL);
    model->state =
        cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "busy", .cap = 16ULL})       ? CLI_NOTEBOOK_KERNEL_BUSY
        : cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "idle", .cap = 16ULL})     ? CLI_NOTEBOOK_KERNEL_IDLE
        : cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "starting", .cap = 16ULL}) ? CLI_NOTEBOOK_KERNEL_STARTING
                                                                                         : model->state;
}

// one message of `size` bytes of `text` applied to the model: 1 where it changed it, 0 where it is of no run or of no
// type a cell takes, and 2 where the run it answers is not yet bound, the message to stand in the ring
EMBED_INLINE unsigned int message_applied(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->text, .size = args->size, .err_msg = &err_msg, .owner = g_cli_notebook_open_owner,
                   .nodes_more = CELLS_MESSAGE_NODES, .text_more = CELLS_MESSAGE_TEXT);
    const char *const type_name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = &json, .node = root,
                                             .name = "msg_type", .length_pointer = NULL);
    const char *const parent = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = &json, .node = root,
                                          .name = "parent", .length_pointer = NULL);
    const unsigned int content =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = &json, .node = root, .name = "content");
    const unsigned int type = EMBED_CALL(message_type, NotebookCellsCtx, .name = type_name);
    const unsigned int cell = EMBED_CALL(cell_answered, NotebookCellsCtx, .model = model, .name = parent);
    unsigned int applied = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = type_name, .other = "status", .cap = 16ULL}))
    {
        EMBED_CALL(status_read, NotebookCellsCtx, .model = model, .json_read = &json, .node = content);
        applied = 1u;
    }
    else if ((type != CLI_NOTEBOOK_NONE) && (cell == CLI_NOTEBOOK_NONE))
    {
        applied = (parent[0] != '\0') ? 2u : 0u;
    }
    else if (type == CELLS_INPUT)
    {
        CliNotebookCell one;
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = cell,
                   .cell_out = &one);
        one.count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = &json, .node = content, .name = "execution_count");
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = cell,
                   .cell_read = &one);
        applied = 1u;
    }
    else if (type == CELLS_CLEAR)
    {
        EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = cell);
        applied = 1u;
    }
    else if (type == CELLS_REPLY)
    {
        const unsigned int count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = &json, .node = content, .name = "execution_count");
        CliNotebookCell one;
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = cell,
                   .cell_out = &one);
        one.count = (count != CLI_NOTEBOOK_NONE) ? count : one.count;
        one.running = 0;
        one.run_id[0] = '\0';
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = cell,
                   .cell_read = &one);
        applied = 1u;
    }
    else if (type != CLI_NOTEBOOK_NONE)
    {
        // an output's content becomes the output a notebook stores, its type put into it and its transient data let go
        const char *const output_type = (type == CLI_NOTEBOOK_STREAM)   ? "stream"
                                        : (type == CLI_NOTEBOOK_RESULT) ? "execute_result"
                                        : (type == CLI_NOTEBOOK_ERROR)  ? "error"
                                                                        : "display_data";
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = content,
                   .name = "output_type",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = &json, .text = output_type,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = output_type, .cap = 16ULL})));
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &json, .object = content,
                   .name = "transient");
        (void)EMBED_CALL(node_output_put, NotebookCellsCtx, .model = model, .json_read = &json, .node = content,
                         .cell = cell, .type = type);
        applied = 1u;
    }
    if ((applied == 1u) && (cell != CLI_NOTEBOOK_NONE))
    {
        EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = cell);
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_notebook_open_owner);
    return applied;
}

int cli_notebook_model_step(const CliNotebookCellsStepCfg *args)
{
    (void)args;
    int changed = 0;
    for (unsigned int place = 0u; place < s_models; place += 1u)
    {
        CliNotebookModel *model = NULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_model_cell,
                                                                .offset = CLI_NOTEBOOK_MODEL_AT(place),
                                                                .dst = &model,
                                                                .bytes = sizeof(CliNotebookModel *)});
        // the messages pending read into a cell of this call and the ring emptied, the receiver filling it again
        // from here on
        EMBED_CALL(ring_locked, NotebookCellsCtx, .model = model);
        const unsigned long long used = model->ring_used;
        unsigned char *const ring =
            (used != 0ULL) ? g_cli_guard->temporary_buf_alloc(used, g_cli_notebook_open_owner) : NULL;
        if (ring != NULL)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = model->ring_cell, .offset = 0ULL, .dst = ring, .bytes = (embed_word)used});
            model->ring_used = 0ULL;
        }
        cli_gate_opened(&model->ring_lock);
        int ran = 0;
        for (unsigned long long at = 0ULL; (ring != NULL) && (at + sizeof(unsigned long long) <= used);)
        {
            unsigned long long size = 0ULL;
            memor.cpy(&(MemoriaCfg){.dst = &size, .src = ring + at, .bytes = sizeof(size)});
            const char *const message = (const char *)(ring + at + sizeof(size));
            const unsigned int applied =
                EMBED_CALL(message_applied, NotebookCellsCtx, .model = model, .text = message, .size = size);
            if (applied == 2u)
            {
                (void)EMBED_CALL(ring_put, NotebookCellsCtx, .model = model, .text = message, .size = size);
            }
            ran = ran || (applied == 1u);
            at += sizeof(size) + size;
        }
        g_cli_guard->persistent_buf_release(ring, g_cli_notebook_open_owner);
        if (ran)
        {
            EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
        }
        changed = changed || ran;
    }
    return changed;
}
