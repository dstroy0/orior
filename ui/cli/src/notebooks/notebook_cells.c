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
// message's tree has for its output's kind put into it
#define CELLS_NODES_MORE 64u
#define CELLS_NODES_LINE 16u
#define CELLS_TEXT_MORE 4096ULL
#define CELLS_READS 4u
#define CELLS_STOP_MS 2000ULL
#define CELLS_MESSAGE_NODES 8u
#define CELLS_MESSAGE_TEXT 64ULL

// the models open, a pointer each in one cell sized to them, and what wakes the view's thread
static CliNotebookModel **s_model;
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
    unsigned int kind;
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
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json_read, .node = node,
                    .kind = CFG_JSON_NUMBER))
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

// the ring's word locked, the receiver and the step each locking it only to copy into it or to take its cell
EMBED_INLINE void ring_locked(const NotebookCellsCtx *args)
{
    while (EMBED_ATOMIC_EXCHANGE_32(&args->model->ring_lock, 1u) != 0u)
    {
        // the other side copies one message at most before it lets go
    }
}

// a message of `size` bytes of `text` put after the ring's messages, the ring taken anew sized to the bytes pending
// where it does not fit: 1, or 0 where the cells cannot meet it, the message lost
EMBED_INLINE int ring_put(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned long long need = model->ring_used + sizeof(unsigned long long) + args->size;
    EMBED_CALL(ring_locked, NotebookCellsCtx, .model = model);
    int put = 1;
    if (need > model->ring_capacity)
    {
        unsigned char *const grown = g_cli_guard->persistent_buf_alloc(need, g_cli_notebook_open_owner);
        if ((grown != NULL) && (model->ring_used != 0ULL))
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = model->ring, .bytes = model->ring_used});
        }
        if (grown != NULL)
        {
            g_cli_guard->persistent_buf_release(model->ring, g_cli_notebook_open_owner);
            model->ring = grown;
            model->ring_capacity = need;
        }
        put = grown != NULL;
    }
    if (put)
    {
        const unsigned long long size = args->size;
        memor.cpy(&(MemoriaCfg){.dst = model->ring + model->ring_used, .src = &size, .bytes = sizeof(size)});
        memor.cpy(&(MemoriaCfg){
            .dst = model->ring + model->ring_used + sizeof(size), .src = args->text, .bytes = args->size});
        model->ring_used = need;
    }
    EMBED_ATOMIC_STORE(&model->ring_lock, 0u);
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
    unsigned char *const ring = model->ring;
    model->ring = NULL;
    model->ring_used = 0ULL;
    model->ring_capacity = 0ULL;
    EMBED_ATOMIC_STORE(&model->ring_lock, 0u);
    g_cli_guard->persistent_buf_release(ring, g_cli_notebook_open_owner);
}

CliNotebookModel *cli_notebook_model_of(const CliNotebookModelCtx *args)
{
    for (unsigned int at = 0u; (args->key != NULL) && (at < s_models); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_model[at]->key, .other = args->key, .cap = CLI_PATH_MAX_LEN}))
        {
            return s_model[at];
        }
    }
    return NULL;
}

CliNotebookModel *cli_notebook_model_at(const CliNotebookModelCtx *args)
{
    return (args->at < s_models) ? s_model[args->at] : NULL;
}

void cli_notebook_model_key(const CliNotebookModelCtx *args)
{
    const CliNotebookModel *const model = args->model;
    VerbaLinea line = {
        args->out, cellul.len(&(CatenaFinitaCfg){.src = model->key, .cap = CLI_PATH_MAX_LEN}) + CLI_NOTEBOOK_KEY_MORE,
        0ULL};
    mmgr_line_put(&line, model->key);
    mmgr_line_ch(&line, '#');
    mmgr_line_decimal(&line, model->cell[args->cell].serial, 1u);
    if (args->output != CLI_NOTEBOOK_NONE)
    {
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, model->output[args->output].serial, 1u);
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
    CliNotebookCell *const cell = &model->cell[args->cell];
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    CliShape shape = {0ULL, 0ULL};
    if (!EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = args->text, .size = args->size, .key = key,
                    .shape = &shape))
    {
        return 0;
    }
    const char *const grammar = (cell->kind == CLI_NOTEBOOK_CODE)       ? model->kernel_language
                                : (cell->kind == CLI_NOTEBOOK_MARKDOWN) ? "markdown"
                                                                        : "";
    EMBED_CALL(text_colored, NotebookCellsCtx, .name = grammar, .text = key);
    // a cell's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    cell->lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
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
    // a cell's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    model->cell[args->cell].lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
}

int cli_notebook_model_cells_laid(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    const unsigned int count = args->count;
    const unsigned long long bytes =
        ((unsigned long long)count * sizeof(CliNotebookCell)) + ((count + 1ULL) * sizeof(unsigned int));
    unsigned char *const laid = g_cli_guard->persistent_buf_alloc(bytes, g_cli_notebook_open_owner);
    if (laid == NULL)
    {
        return 0;
    }
    CliNotebookCell *const cell = (CliNotebookCell *)(void *)laid;
    const unsigned int before = (args->cell < model->cells) ? args->cell : model->cells;
    if (before != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = model->cell, .bytes = before * sizeof(CliNotebookCell)});
    }
    // one more: a cell of none at `cell`; one fewer: the cell at `cell` left out; as many: every cell as it stands
    const unsigned int skipped = (count < model->cells) ? 1u : 0u;
    const unsigned int opened = (count > model->cells) ? 1u : 0u;
    if (opened)
    {
        memor.set(&(MemoriaCfg){.dst = &cell[before], .val = 0, .bytes = sizeof(CliNotebookCell)});
    }
    const unsigned int after = model->cells - before - ((skipped && (before < model->cells)) ? 1u : 0u);
    if (after != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = &cell[before + opened],
                                .src = &model->cell[before + skipped],
                                .bytes = after * sizeof(CliNotebookCell)});
    }
    g_cli_guard->persistent_buf_release(model->cell, g_cli_notebook_open_owner);
    model->cell = cell;
    model->first_row = (unsigned int *)(void *)(laid + ((unsigned long long)count * sizeof(CliNotebookCell)));
    model->cells = count;
    model->first_row[0] = 0u;
    return 1;
}

void cli_notebook_model_bound(const CliNotebookModelCtx *args)
{
    CliNotebookCell *const cell = &args->model->cell[args->cell];
    VerbaLinea line = {cell->run_id, sizeof(cell->run_id), 0ULL};
    mmgr_line_put(&line, args->id);
    (void)mmgr_line_finish(&line);
}

// the output texts laid anew in a cell sized to them, the output `output` taking `json_size` bytes of `json_text` in
// place of its own, every other output's standing: 1, or 0 where the cells cannot meet it, the texts as they were
EMBED_INLINE int texts_laid(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        bytes += (at == args->output) ? args->json_size : model->output[at].json_length;
    }
    char *const laid = (bytes != 0ULL) ? g_cli_guard->persistent_buf_alloc(bytes, g_cli_notebook_open_owner) : NULL;
    if ((bytes != 0ULL) && (laid == NULL))
    {
        return 0;
    }
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        CliNotebookOutput *const output = &model->output[at];
        const char *const from = (at == args->output) ? args->json_text : (model->output_text + output->json);
        const unsigned long long length = (at == args->output) ? args->json_size : output->json_length;
        if (length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = laid + used, .src = from, .bytes = length});
        }
        output->json = used;
        output->json_length = length;
        used += length;
    }
    g_cli_guard->persistent_buf_release(model->output_text, g_cli_notebook_open_owner);
    model->output_text = laid;
    model->output_used = used;
    return 1;
}

void cli_notebook_model_outputs_settled(const CliNotebookModelCtx *args)
{
    CliNotebookCell *const cell = &args->model->cell[args->cell];
    unsigned int rows = 0u;
    for (unsigned int at = 0u; at < cell->outputs; at += 1u)
    {
        CliNotebookOutput *const output = &args->model->output[cell->output_first + at];
        output->rows = ((output->lines > (2u * CLI_NOTEBOOK_CUT_LINES)) && !output->whole)
                           ? ((2u * CLI_NOTEBOOK_CUT_LINES) + 1u)
                           : output->lines;
        output->row = rows;
        rows += output->rows;
    }
    cell->output_rows = rows;
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
    // an output's lines are bounded by the document store's lines, far below the 32-bit place no count reaches
    model->output[args->output].lines = (shape.lines != 0ULL) ? (unsigned int)shape.lines : 1u;
    model->output[args->output].line_ended = size != args->size;
    return 1;
}

// an output of `kind` put after the outputs of the cell `cell`, its text `size` bytes of `text` and its JSON
// `json_size` bytes of `json_text`, the outputs taken anew one more: 1, or 0 where the cells cannot meet it
EMBED_INLINE int output_put(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell *const cell = &model->cell[args->cell];
    const unsigned int place = cell->output_first + cell->outputs;
    CliNotebookOutput *const grown = g_cli_guard->persistent_buf_alloc(
        (model->outputs + 1ULL) * sizeof(CliNotebookOutput), g_cli_notebook_open_owner);
    if (grown == NULL)
    {
        return 0;
    }
    if (place != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = model->output, .bytes = place * sizeof(CliNotebookOutput)});
    }
    if (place < model->outputs)
    {
        memor.cpy(&(MemoriaCfg){.dst = &grown[place + 1u],
                                .src = &model->output[place],
                                .bytes = (model->outputs - place) * sizeof(CliNotebookOutput)});
    }
    grown[place] =
        (CliNotebookOutput){model->next_serial, args->kind, args->stderr_stream, 1u, 1u, 0u, 0, 0, 0ULL, 0ULL};
    model->next_serial += 1u;
    g_cli_guard->persistent_buf_release(model->output, g_cli_notebook_open_owner);
    model->output = grown;
    model->outputs += 1u;
    cell->outputs += 1u;
    for (unsigned int at = args->cell + 1u; at < model->cells; at += 1u)
    {
        model->cell[at].output_first += 1u;
    }
    (void)EMBED_CALL(texts_laid, NotebookCellsCtx, .model = model, .output = place, .json_text = args->json_text,
                     .json_size = args->json_size);
    (void)EMBED_CALL(output_text_stored, NotebookCellsCtx, .model = model, .cell = args->cell, .output = place,
                     .text = args->text, .size = args->size);
    cell->failed = cell->failed || (args->kind == CLI_NOTEBOOK_ERROR);
    EMBED_CALL(cli_notebook_model_internal.outputs_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    return 1;
}

void cli_notebook_model_outputs_released(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliNotebookCell *const cell = &model->cell[args->cell];
    for (unsigned int at = 0u; at < cell->outputs; at += 1u)
    {
        char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
        EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
                   .output = cell->output_first + at, .out = key);
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    }
    const unsigned int gone = cell->outputs;
    const unsigned int after = model->outputs - cell->output_first - gone;
    if ((gone != 0u) && (after != 0u))
    {
        memor.cpy(&(MemoriaCfg){.dst = &model->output[cell->output_first],
                                .src = &model->output[cell->output_first + gone],
                                .bytes = after * sizeof(CliNotebookOutput)});
    }
    model->outputs -= gone;
    for (unsigned int at = args->cell + 1u; at < model->cells; at += 1u)
    {
        model->cell[at].output_first -= gone;
    }
    cell->outputs = 0u;
    cell->output_rows = 0u;
    cell->failed = 0;
    if (model->outputs == 0u)
    {
        g_cli_guard->persistent_buf_release(model->output, g_cli_notebook_open_owner);
        model->output = NULL;
    }
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
    const CliNotebookCell *const cell = &model->cell[args->cell];
    const int stderr_stream = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "stderr", .cap = 8ULL});
    const unsigned int last = (cell->outputs != 0u) ? (cell->output_first + cell->outputs - 1u) : CLI_NOTEBOOK_NONE;
    const int joined = (last != CLI_NOTEBOOK_NONE) && (model->output[last].kind == CLI_NOTEBOOK_STREAM) &&
                       (model->output[last].stderr_stream == stderr_stream);
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
    const int line_ended = (standing != NULL) && model->output[last].line_ended;
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
        put = EMBED_CALL(output_put, NotebookCellsCtx, .model = model, .cell = args->cell, .kind = CLI_NOTEBOOK_STREAM,
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
// as `kind`, its text the stream's, the error's traceback, or the text/plain of its data, and its JSON the node
// written: 1, or 0 where the cells cannot meet it
EMBED_INLINE int node_output_put(const NotebookCellsCtx *args)
{
    const CliJson *const json = args->json_read;
    const unsigned int node = args->node;
    if (args->kind == CLI_NOTEBOOK_STREAM)
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
    for (unsigned int line = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = traceback, .kind = CFG_JSON_ARRAY)
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
    shown += (args->kind == CLI_NOTEBOOK_ERROR) ? 0ULL : plain;
    const unsigned long long json_capacity =
        EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = json, .node = node,
                   .out = NULL, .indent = 0u, .sorted = 0);
    char *const scratch = g_cli_guard->temporary_buf_alloc(shown + json_capacity + 1ULL, g_cli_notebook_open_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    VerbaLinea text = {scratch, shown + 1ULL, 0ULL};
    if (args->kind == CLI_NOTEBOOK_ERROR)
    {
        for (unsigned int line = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = traceback, .kind = CFG_JSON_ARRAY)
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
                               .kind = args->kind, .stderr_stream = args->kind == CLI_NOTEBOOK_ERROR, .text = scratch,
                               .size = text.at, .json_text = written.out, .json_size = written.at);
    g_cli_guard->persistent_buf_release(scratch, g_cli_notebook_open_owner);
    return put;
}

// the kind an output's type `name` stands for, CLI_NOTEBOOK_NONE for none
EMBED_INLINE unsigned int output_kind(const NotebookCellsCtx *args)
{
    static const char *const s_kinds[] = {"stream", "execute_result", "display_data", "error"};
    for (unsigned int at = 0u; at < (sizeof(s_kinds) / sizeof(s_kinds[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_kinds[at], .cap = 16ULL}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

int cli_notebook_model_kernel_laid(const CliNotebookModelCtx *args)
{
    CliNotebookModel *const model = args->model;
    char *const laid = g_cli_guard->persistent_buf_alloc(args->size, g_cli_notebook_open_owner);
    if (laid == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = laid, .src = args->text, .bytes = args->size});
    g_cli_guard->persistent_buf_release(model->kernel_text, g_cli_notebook_open_owner);
    model->kernel_text = laid;
    model->kernel_name = laid;
    model->kernel_display =
        model->kernel_name + cellul.len(&(CatenaFinitaCfg){.src = model->kernel_name, .cap = args->size}) + 1;
    model->kernel_language =
        model->kernel_display + cellul.len(&(CatenaFinitaCfg){.src = model->kernel_display, .cap = args->size}) + 1;
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
        CliNotebookCell *const cell = &model->cell[at];
        const char *const type = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = node,
                                            .name = "cell_type", .length_pointer = NULL);
        *cell = (CliNotebookCell){0};
        cell->serial = model->next_serial;
        model->next_serial += 1u;
        cell->kind = cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "markdown", .cap = 16ULL})
                         ? CLI_NOTEBOOK_MARKDOWN
                     : cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "raw", .cap = 16ULL}) ? CLI_NOTEBOOK_RAW
                                                                                                : CLI_NOTEBOOK_CODE;
        cell->origin = node;
        cell->count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = json, .node = node, .name = "execution_count");
        cell->output_first = model->outputs;
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
        for (unsigned int output = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = json, .node = outputs, .kind = CFG_JSON_ARRAY)
                                       ? json->node[outputs].first
                                       : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = json->node[output].next)
        {
            const unsigned int kind =
                EMBED_CALL(output_kind, NotebookCellsCtx,
                           .name = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = json, .node = output,
                                              .name = "output_type", .length_pointer = NULL));
            if ((kind != CLI_NOTEBOOK_NONE) && !EMBED_CALL(node_output_put, NotebookCellsCtx, .model = model,
                                                           .json_read = json, .node = output, .cell = at, .kind = kind))
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
    g_cli_guard->persistent_buf_release(model->cell, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->output, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->output_text, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->gone, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->kernel_text, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->sentence, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->ring, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(model->json.node, g_cli_notebook_open_owner);
    unsigned int place = s_models;
    for (unsigned int at = 0u; at < s_models; at += 1u)
    {
        place = (s_model[at] == model) ? at : place;
    }
    g_cli_guard->persistent_buf_release(model, g_cli_notebook_open_owner);
    if (place == s_models)
    {
        return;
    }
    for (unsigned int at = place + 1u; at < s_models; at += 1u)
    {
        s_model[at - 1u] = s_model[at];
    }
    s_models -= 1u;
    if (s_models == 0u)
    {
        g_cli_guard->persistent_buf_release(s_model, g_cli_notebook_open_owner);
        s_model = NULL;
    }
}

int cli_notebook_model_open(const CliNotebookCellsOpenCfg *args)
{
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = args->key);
    const unsigned long long key_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    unsigned char *const cell =
        g_cli_guard->persistent_buf_alloc(sizeof(CliNotebookModel) + key_bytes + path_bytes, g_cli_notebook_open_owner);
    CliNotebookModel **const models =
        (cell != NULL) ? g_cli_guard->persistent_buf_alloc((s_models + 1ULL) * sizeof(CliNotebookModel *),
                                                           g_cli_notebook_open_owner)
                       : NULL;
    if (models == NULL)
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_notebook_open_owner);
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        return 0;
    }
    CliNotebookModel *const model = (CliNotebookModel *)(void *)cell;
    *model = (CliNotebookModel){0};
    memor.cpy(&(MemoriaCfg){.dst = cell + sizeof(CliNotebookModel), .src = args->key, .bytes = key_bytes});
    memor.cpy(
        &(MemoriaCfg){.dst = cell + sizeof(CliNotebookModel) + key_bytes, .src = args->path, .bytes = path_bytes});
    model->key = (const char *)(cell + sizeof(CliNotebookModel));
    model->path = (const char *)(cell + sizeof(CliNotebookModel) + key_bytes);
    model->focused = CLI_NOTEBOOK_NONE;
    model->menu_cell = CLI_NOTEBOOK_NONE;
    model->next_serial = 1u;
    if (s_models != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = models, .src = s_model, .bytes = s_models * sizeof(CliNotebookModel *)});
    }
    g_cli_guard->persistent_buf_release(s_model, g_cli_notebook_open_owner);
    s_model = models;
    s_model[s_models] = model;
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
    const CliNotebookCell *const cell = &model->cell[args->cell];
    const unsigned int node =
        (cell->origin != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = cell->origin, .to = json,
                         .from = &model->json)
            : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    if (cell->origin == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "metadata",
                   .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json,
                                       .kind = CFG_JSON_OBJECT));
    }
    const char *const type = (cell->kind == CLI_NOTEBOOK_MARKDOWN) ? "markdown"
                             : (cell->kind == CLI_NOTEBOOK_RAW)    ? "raw"
                                                                   : "code";
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node, .name = "cell_type",
        .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                            .text = type, .length = cellul.len(&(CatenaFinitaCfg){.src = type, .cap = 16ULL})));
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    unsigned long long size = 0ULL;
    char *const source = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                    .owner = g_cli_notebook_open_owner);
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = node, .name = "source",
        .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                            .text = (source != NULL) ? source : "", .length = (source != NULL) ? size : 0ULL));
    g_cli_guard->persistent_buf_release(source, g_cli_notebook_open_owner);
    if (cell->kind != CLI_NOTEBOOK_CODE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "outputs");
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json, .object = node,
                   .name = "execution_count");
        return json->full ? CLI_JSON_NONE : node;
    }
    const unsigned int outputs =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < cell->outputs; at += 1u)
    {
        const CliNotebookOutput *const output = &model->output[cell->output_first + at];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const unsigned int read =
            EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = json,
                       .text = model->output_text + output->json, .size = output->json_length, .tokens = args->token,
                       .tokens_max = args->tokens, .err_msg = &err_msg);
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
                            ? EMBED_CALL(cli_json_tree_text_length_kind.json_number_init, CliJsonTreeTextLengthKindCfg,
                                         .json = json, .text = count, .length = count_line.at)
                            : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json,
                                         .kind = CFG_JSON_NULL));
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
        metadata = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->node,
                   .name = "metadata", .value = metadata);
    }
    const unsigned int spec =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    const char *const field[3] = {"display_name", "language", "name"};
    const char *const value[3] = {model->kernel_display, model->kernel_language, model->kernel_name};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = spec,
                   .name = field[at],
                   .value =
                       EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
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
    // the save's tree sized to the tree read, the sources as they stand and the outputs' texts
    unsigned int nodes = 0u;
    unsigned long long text = 0ULL;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = &model->json,
               .node = model->root, .nodes_pointer = &nodes, .text_u64s = &text);
    unsigned int tokens = 1u;
    for (unsigned int at = 0u; at < model->outputs; at += 1u)
    {
        const unsigned int counted = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                                .text = model->output_text + model->output[at].json,
                                                .size = model->output[at].json_length, .out = NULL);
        tokens = (counted > tokens) ? counted : tokens;
        nodes += counted;
    }
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        text += (4ULL * CELLS_MESSAGE_TEXT) + (3ULL * model->cell[at].lines * CLI_ERR_MSG_MAX_LEN);
    }
    nodes += (CELLS_NODES_LINE * (model->cells + 1u)) + CELLS_NODES_MORE;
    text += model->output_used + CELLS_TEXT_MORE + (3ULL * CLI_NAME_MAX_LEN);
    CliJson json = {0};
    CfgJsonToken *const token =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens * sizeof(CfgJsonToken), g_cli_notebook_open_owner);
    if ((token == NULL) || !EMBED_CALL(cli_json_tree_nodes_max.json_reserved_temporary, CliJsonTreeNodesMaxCfg,
                                       .json = &json, .nodes_max = nodes, .text_max = text,
                                       .what = "a notebook being saved", .owner = g_cli_notebook_open_owner))
    {
        g_cli_guard->persistent_buf_release(token, g_cli_notebook_open_owner);
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": the cells cannot meet the notebook");
        return 0;
    }
    const unsigned int root = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = model->root,
                                         .to = &json, .from = &model->json);
    const unsigned int cells =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        const unsigned int written = EMBED_CALL(cell_written, NotebookCellsCtx, .model = model, .cell = at,
                                                .json_out = &json, .token = token, .tokens = tokens);
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
    for (unsigned int at = 0u; (args->name[0] != '\0') && (at < args->model->cells); at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->model->cell[at].run_id, .other = args->name, .cap = CLI_KERNEL_ID}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

// the kind of a message `name` of a cell's run, the output kinds, then one more for each of the run's other messages,
// CLI_NOTEBOOK_NONE for a message of no run
#define CELLS_INPUT 4u
#define CELLS_CLEAR 5u
#define CELLS_REPLY 6u
EMBED_INLINE unsigned int message_kind(const NotebookCellsCtx *args)
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
// kind a cell takes, and 2 where the run it answers is not yet bound, the message to stand in the ring
EMBED_INLINE unsigned int message_applied(const NotebookCellsCtx *args)
{
    CliNotebookModel *const model = args->model;
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->text, .size = args->size, .err_msg = &err_msg, .owner = g_cli_notebook_open_owner,
                   .nodes_more = CELLS_MESSAGE_NODES, .text_more = CELLS_MESSAGE_TEXT);
    const char *const type = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = &json, .node = root,
                                        .name = "msg_type", .length_pointer = NULL);
    const char *const parent = EMBED_CALL(string_of, NotebookCellsCtx, .json_read = &json, .node = root,
                                          .name = "parent", .length_pointer = NULL);
    const unsigned int content =
        EMBED_CALL(member_of, NotebookCellsCtx, .json_read = &json, .node = root, .name = "content");
    const unsigned int kind = EMBED_CALL(message_kind, NotebookCellsCtx, .name = type);
    const unsigned int cell = EMBED_CALL(cell_answered, NotebookCellsCtx, .model = model, .name = parent);
    unsigned int applied = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "status", .cap = 16ULL}))
    {
        EMBED_CALL(status_read, NotebookCellsCtx, .model = model, .json_read = &json, .node = content);
        applied = 1u;
    }
    else if ((kind != CLI_NOTEBOOK_NONE) && (cell == CLI_NOTEBOOK_NONE))
    {
        applied = (parent[0] != '\0') ? 2u : 0u;
    }
    else if (kind == CELLS_INPUT)
    {
        model->cell[cell].count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = &json, .node = content, .name = "execution_count");
        applied = 1u;
    }
    else if (kind == CELLS_CLEAR)
    {
        EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = cell);
        applied = 1u;
    }
    else if (kind == CELLS_REPLY)
    {
        const unsigned int count =
            EMBED_CALL(number_of, NotebookCellsCtx, .json_read = &json, .node = content, .name = "execution_count");
        model->cell[cell].count = (count != CLI_NOTEBOOK_NONE) ? count : model->cell[cell].count;
        model->cell[cell].running = 0;
        model->cell[cell].run_id[0] = '\0';
        applied = 1u;
    }
    else if (kind != CLI_NOTEBOOK_NONE)
    {
        // an output's content becomes the output a notebook stores, its type put into it and its transient data let go
        const char *const output_type = (kind == CLI_NOTEBOOK_STREAM)   ? "stream"
                                        : (kind == CLI_NOTEBOOK_RESULT) ? "execute_result"
                                        : (kind == CLI_NOTEBOOK_ERROR)  ? "error"
                                                                        : "display_data";
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = content,
                   .name = "output_type",
                   .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                       .json = &json, .text = output_type,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = output_type, .cap = 16ULL})));
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &json, .object = content,
                   .name = "transient");
        (void)EMBED_CALL(node_output_put, NotebookCellsCtx, .model = model, .json_read = &json, .node = content,
                         .cell = cell, .kind = kind);
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
        CliNotebookModel *const model = s_model[place];
        // the ring's cell taken whole, the receiver filling a cell of its own from here on
        EMBED_CALL(ring_locked, NotebookCellsCtx, .model = model);
        unsigned char *const ring = model->ring;
        const unsigned long long used = model->ring_used;
        model->ring = NULL;
        model->ring_used = 0ULL;
        model->ring_capacity = 0ULL;
        EMBED_ATOMIC_STORE(&model->ring_lock, 0u);
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
