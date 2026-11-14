// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_kernel.c: an open notebook's kernel: the machine's kernels read once from Jupyter's specs into one cell, a
// row each with its texts after the rows, the kernel started with the notebook receiver and the model as its context,
// cells run on it and bound to the ids their messages answer, and the kernel interrupted, restarted and stopped
#include "notebook_kernel.h"

#include "../command_line/command_line.h"
#include "../command_line/commands.h"
#include "notebook_cells_internal.h"
#include "notebook_edit.h"

// the bytes the specs are first written into, the most times they are written again into twice as many, and the
// milliseconds a kernel has to stop before its process is ended
#define KERNEL_SPECS_BYTES (1ULL << 20u)
#define KERNEL_SPECS_TRIES 4u
#define KERNEL_STOP_MS 2000ULL

// the kernels the machine has, a row each in one movable cell read out by copy, how many, and whether they were read
static const CarcerMovableEntry *s_kernel_cell;
static unsigned int s_kernels;
static int s_kernels_read;

typedef struct
{
    CliNotebookModel *model;
    const char *name;
    unsigned int at;
    unsigned int cell;
    VerbaLinea *err_msg;
} NotebookKernelCtx;

// `text` written after the line's text and posted as a notice, failed where `at` is 1
EMBED_INLINE void notice_posted(const NotebookKernelCtx *args)
{
    char sentence[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {sentence, sizeof(sentence), 0ULL};
    mmgr_line_put(&line, args->name);
    (void)mmgr_line_finish(&line);
    cli_notice_post(sentence, args->at == 1u);
}

// the specs Jupyter has written into a cell sized to them, taken anew twice as large while they do not fit, and read
// into the kernels' cell
EMBED_INLINE void kernels_read(const NotebookKernelCtx *args)
{
    (void)args;
    if (s_kernels_read)
    {
        return;
    }
    s_kernels_read = 1;
    unsigned long long bytes = KERNEL_SPECS_BYTES;
    char *specs = NULL;
    VerbaLinea out = {NULL, 0ULL, 0ULL};
    for (unsigned int tried = 0u; tried < KERNEL_SPECS_TRIES; tried += 1u)
    {
        specs = g_cli_guard->temporary_buf_alloc(bytes, g_cli_notebook_open_owner);
        if (specs == NULL)
        {
            return;
        }
        out = (VerbaLinea){specs, bytes, 0ULL};
        EMBED_CALL(cli_jupyter_out.specs_write, CliJupyterOutCfg, .out = &out);
        if (mmgr_line_ok(&out))
        {
            break;
        }
        g_cli_guard->persistent_buf_release(specs, g_cli_notebook_open_owner);
        specs = NULL;
        bytes *= 2ULL;
    }
    if (specs == NULL)
    {
        return;
    }
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = specs, .size = out.at, .err_msg = &err_msg,
                                         .owner = g_cli_notebook_open_owner, .nodes_more = 0u, .text_more = 0ULL);
    g_cli_guard->persistent_buf_release(specs, g_cli_notebook_open_owner);
    const unsigned int count = (root != CLI_JSON_NONE) ? json.node[root].count : 0u;
    static const char *const s_fields[4] = {"name", "display_name", "language", "install"};
    // the rows laid in a cell of this call, each text cut to its field, then written once into the kernels' cell
    CliNotebookKernelRow *const laid =
        (count != 0u) ? g_cli_guard->temporary_buf_alloc((unsigned long long)count * sizeof(CliNotebookKernelRow),
                                                         g_cli_notebook_open_owner)
                      : NULL;
    unsigned int at = 0u;
    for (unsigned int spec = (laid != NULL) ? json.node[root].first : CLI_JSON_NONE; spec != CLI_JSON_NONE;
         spec = json.node[spec].next)
    {
        CliNotebookKernelRow *const row = &laid[at];
        memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
        char *const field_text[4] = {row->name, row->display, row->language, row->install};
        const unsigned long long field_most[4] = {sizeof(row->name), sizeof(row->display), sizeof(row->language),
                                                  sizeof(row->install)};
        for (unsigned int field = 0u; field < 4u; field += 1u)
        {
            unsigned long long length = 0ULL;
            const char *const value =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = spec, .name = s_fields[field]),
                           .length_pointer = &length);
            if (value == NULL)
            {
                continue;
            }
            VerbaLinea line = {field_text[field], field_most[field], 0ULL};
            mmgr_line_put_most(&line, value, (length < field_most[field]) ? length : field_most[field] - 1ULL);
            (void)mmgr_line_finish(&line);
            row->lacks = (field == 3u) ? 1 : row->lacks;
        }
        at += 1u;
    }
    s_kernel_cell = (at != 0u) ? g_cli_guard->movable_buf_alloc((unsigned long long)at * sizeof(CliNotebookKernelRow),
                                                                g_cli_notebook_open_owner)
                               : NULL;
    s_kernels = (s_kernel_cell != NULL) ? at : 0u;
    if (s_kernel_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_kernel_cell, .offset = 0u, .src = laid, .bytes = (unsigned long long)at * sizeof(*laid)});
    }
    g_cli_guard->persistent_buf_release(laid, g_cli_notebook_open_owner);
    g_cli_guard->persistent_buf_release(json.node, g_cli_notebook_open_owner);
}

// the kernel row `at` copied out of the kernels' cell
EMBED_INLINE CliNotebookKernelRow kernel_read(const NotebookKernelCtx *args)
{
    CliNotebookKernelRow row;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_kernel_cell,
                                                            .offset = (unsigned long long)args->at * sizeof(row),
                                                            .dst = &row,
                                                            .bytes = sizeof(row)});
    return row;
}

// the kernel row `name`, else the first of the model's language lacking nothing: its place, CLI_NOTEBOOK_NONE for
// none
EMBED_INLINE unsigned int kernel_found(const NotebookKernelCtx *args)
{
    EMBED_CALL(kernels_read, NotebookKernelCtx, .model = args->model);
    for (unsigned int at = 0u; (args->name[0] != '\0') && (at < s_kernels); at += 1u)
    {
        const CliNotebookKernelRow row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = at);
        if (cellul.eq(&(CatenaFinitaCfg){.src = row.name, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            return at;
        }
    }
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        const CliNotebookKernelRow row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = at);
        if (!row.lacks && cellul.eq(&(CatenaFinitaCfg){
                              .src = row.language, .other = args->model->kernel_language, .cap = CLI_NAME_MAX_LEN}))
        {
            return at;
        }
    }
    return CLI_NOTEBOOK_NONE;
}

// every cell of the model left not running, its run unbound, and the messages waiting for them let go
EMBED_INLINE void runs_ended(const NotebookKernelCtx *args)
{
    CliNotebookModel *const model = args->model;
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        CliNotebookCell cell;
        EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_out = &cell);
        cell.running = 0;
        cell.run_id[0] = '\0';
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_read = &cell);
    }
    EMBED_CALL(cli_notebook_model_internal.ring_cleared, CliNotebookModelCtx, .model = model);
}

// the cell `cell` of the model read out of its cells
EMBED_INLINE CliNotebookCell cell_of(const NotebookKernelCtx *args)
{
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = args->model, .cell = args->at,
               .cell_out = &cell);
    return cell;
}

// the model's kernel stopped and let go: 1 where its process had to be ended
EMBED_INLINE int kernel_stopped(const NotebookKernelCtx *args)
{
    CliNotebookModel *const model = args->model;
    if (model->kernel == NULL)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int ended = EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = model->kernel,
                                 .wait = KERNEL_STOP_MS, .err_msg = &err_msg);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = model->kernel,
               .err_msg = &err_msg);
    model->kernel = NULL;
    model->state = CLI_NOTEBOOK_KERNEL_NONE;
    EMBED_CALL(runs_ended, NotebookKernelCtx, .model = model);
    return ended;
}

// the kernel row `at` started for the model in the folder its file stands in, the model's context its receiver is
// handed, the model's kernel texts laid from the row: 1, or 0 with why written
EMBED_INLINE int kernel_started(const NotebookKernelCtx *args)
{
    CliNotebookModel *const model = args->model;
    const CliNotebookKernelRow row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = args->at);
    CliKernelSpec *const spec = g_cli_guard->temporary_buf_alloc(sizeof(CliKernelSpec), g_cli_notebook_open_owner);
    if ((spec == NULL) || !EMBED_CALL(cli_jupyter_name.spec_by_name, CliJupyterNameCfg, .name = row.name, .spec = spec))
    {
        g_cli_guard->persistent_buf_release(spec, g_cli_notebook_open_owner);
        mmgr_line_put(args->err_msg, "no kernel's spec reads as ");
        mmgr_line_put(args->err_msg, row.name);
        return 0;
    }
    // the folder: the path up to its last part, the working folder where it has none
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = model->path, .cap = CLI_PATH_MAX_LEN});
    unsigned long long cut = 0ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        cut = ((model->path[at] == '/') || (model->path[at] == '\\')) ? at : cut;
    }
    if (cut != 0ULL)
    {
        mmgr_line_put_n(&folder_line, model->path, cut);
    }
    else
    {
        mmgr_line_ch(&folder_line, '.');
    }
    (void)mmgr_line_finish(&folder_line);
    model->state = CLI_NOTEBOOK_KERNEL_STARTING;
    model->kernel = EMBED_CALL(cli_jupyter_spec.start, CliJupyterSpecCfg, .spec_read = spec, .folder = folder,
                               .recv = cli_notebook_model_received, .context = model, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(spec, g_cli_notebook_open_owner);
    model->state = (model->kernel != NULL) ? CLI_NOTEBOOK_KERNEL_IDLE : CLI_NOTEBOOK_KERNEL_NONE;
    if (model->kernel == NULL)
    {
        return 0;
    }
    char text[3u * CLI_NAME_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put_most(&line, row.name, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    mmgr_line_put_most(&line, row.display, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    mmgr_line_put_most(&line, row.language, CLI_NAME_MAX_LEN - 1u);
    mmgr_line_ch(&line, '\0');
    (void)EMBED_CALL(cli_notebook_model_internal.kernel_laid, CliNotebookModelCtx, .model = model, .text = text,
                     .size = line.at);
    return 1;
}

// the model's kernel running: the one it runs, else the one its notebook records, else the first of its language
// lacking nothing, started: 1, or 0 with why written and posted
EMBED_INLINE int kernel_ready(const NotebookKernelCtx *args)
{
    CliNotebookModel *const model = args->model;
    if (model->kernel != NULL)
    {
        return 1;
    }
    const unsigned int found = EMBED_CALL(kernel_found, NotebookKernelCtx, .model = model, .name = model->kernel_name);
    if (found == CLI_NOTEBOOK_NONE)
    {
        const unsigned long long said = args->err_msg->at;
        mmgr_line_put(args->err_msg, "no kernel on this machine runs ");
        mmgr_line_put(args->err_msg, model->kernel_language);
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = args->err_msg->out + said, .at = 1u);
        return 0;
    }
    if (!EMBED_CALL(kernel_started, NotebookKernelCtx, .model = model, .at = found, .err_msg = args->err_msg))
    {
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = args->err_msg->out, .at = 1u);
        return 0;
    }
    return 1;
}

const char *cli_notebook_kernel_label(const CliNotebookKernelStateCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    const char *const label = ((model != NULL) && (model->kernel != NULL))
                                  ? EMBED_CALL(cli_jupyter.display_name, CliJupyterCfg, .kernel_read = model->kernel)
                                  : "no kernel";
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_NAME_MAX_LEN});
    return label;
}

unsigned int cli_notebook_kernel_tools(const CliNotebookKernelStateCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        return 0u;
    }
    int code = 0;
    for (unsigned int at = 0u; !code && (at < model->cells); at += 1u)
    {
        code = EMBED_CALL(cell_of, NotebookKernelCtx, .model = (CliNotebookModel *)(embed_word)model, .at = at).type ==
               CLI_NOTEBOOK_CODE;
    }
    const unsigned int running =
        (model->kernel != NULL) ? (CLI_NOTEBOOK_TOOL_INTERRUPT | CLI_NOTEBOOK_TOOL_RESTART | CLI_NOTEBOOK_TOOL_STOP)
                                : 0u;
    return (code ? (CLI_NOTEBOOK_TOOL_RUN_ALL | CLI_NOTEBOOK_TOOL_RESTART_RUN_ALL) : 0u) | running;
}

unsigned int cli_notebook_kernels(const CliNotebookKernelPickCfg *args)
{
    EMBED_CALL(kernels_read, NotebookKernelCtx, .name = args->key);
    return s_kernels;
}

int cli_notebook_kernel_at(const CliNotebookKernelPickCfg *args)
{
    EMBED_CALL(kernels_read, NotebookKernelCtx, .name = args->key);
    if (args->at >= s_kernels)
    {
        return 0;
    }
    *args->row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = args->at);
    return 1;
}

int cli_notebook_kernels_menu(const CliNotebookKernelPickCfg *args)
{
    EMBED_CALL(kernels_read, NotebookKernelCtx, .name = args->key);
    if (s_kernels == 0u)
    {
        return 0;
    }
    // the rows run with the kernel's name, which the menus copy into their own rows
    cli_menus_own_begin();
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        const CliNotebookKernelRow row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = at);
        const char *const label = (row.display[0] != '\0') ? row.display : row.name;
        cli_menus_own_row(&(CliItem){label, "notebook-kernel", "", row.name, "", "", 0, 0, 0, 0, 0u});
    }
    return 1;
}

int cli_notebook_kernel_choose(const CliNotebookKernelPickCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        mmgr_line_put(args->err_msg, "no notebook is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    EMBED_CALL(kernels_read, NotebookKernelCtx, .model = model);
    unsigned int at = args->at;
    for (unsigned int place = 0u; (args->name != NULL) && (place < s_kernels); place += 1u)
    {
        const CliNotebookKernelRow row = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = place);
        at = cellul.eq(&(CatenaFinitaCfg){.src = row.name, .other = args->name, .cap = CLI_NAME_MAX_LEN}) ? place : at;
    }
    if (at >= s_kernels)
    {
        mmgr_line_put(args->err_msg, "no such kernel is on this machine");
        return 0;
    }
    const CliNotebookKernelRow chosen = EMBED_CALL(kernel_read, NotebookKernelCtx, .at = at);
    if (chosen.lacks)
    {
        const unsigned long long said = args->err_msg->at;
        mmgr_line_put(args->err_msg, chosen.display);
        mmgr_line_put(args->err_msg, " lacks ipykernel, which this line installs: ");
        mmgr_line_put(args->err_msg, chosen.install);
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = args->err_msg->out + said, .at = 1u);
        return 0;
    }
    (void)EMBED_CALL(kernel_stopped, NotebookKernelCtx, .model = model);
    if (!EMBED_CALL(kernel_started, NotebookKernelCtx, .model = model, .at = at, .err_msg = args->err_msg))
    {
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = args->err_msg->out, .at = 1u);
        return 0;
    }
    model->dirty = 1;
    return 1;
}

int cli_notebook_kernel_execute(const CliNotebookKernelRunCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (args->cell >= model->cells))
    {
        mmgr_line_put(args->err_msg, "no such cell is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    CliNotebookCell cell_read = EMBED_CALL(cell_of, NotebookKernelCtx, .model = model, .at = args->cell);
    CliNotebookCell *const cell = &cell_read;
    if (cell->type != CLI_NOTEBOOK_CODE)
    {
        if (model->focused == args->cell)
        {
            EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = args->key,
                       .err_msg = args->err_msg);
        }
        return 1;
    }
    if (!EMBED_CALL(kernel_ready, NotebookKernelCtx, .model = model, .err_msg = args->err_msg))
    {
        return 0;
    }
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    unsigned long long size = 0ULL;
    char *const code = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = g_cli_notebook_open_owner);
    if (code == NULL)
    {
        mmgr_line_put(args->err_msg, "the cell's text cannot be read");
        return 0;
    }
    EMBED_CALL(cli_notebook_model_internal.outputs_released, CliNotebookModelCtx, .model = model, .cell = args->cell);
    // the cell read again, the outputs' release having written it, and written with its run
    cell_read = EMBED_CALL(cell_of, NotebookKernelCtx, .model = model, .at = args->cell);
    model->clock += 1u;
    cell->first_run = (cell->first_run != 0u) ? cell->first_run : model->clock;
    cell->last_run = model->clock;
    cell->running = 1;
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
               .cell_read = cell);
    char id[CLI_KERNEL_ID];
    id[0] = '\0';
    const unsigned long long said = args->err_msg->at;
    const int sent =
        EMBED_CALL(cli_jupyter_err_msg_kernel_length.execute, CliJupyterErrMsgKernelLengthCfg, .kernel = model->kernel,
                   .code = code, .length = size, .id = id, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(code, g_cli_notebook_open_owner);
    if (sent)
    {
        EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = model, .cell = args->cell,
                   .id = id);
    }
    else
    {
        cell->running = 0;
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = args->cell,
                   .cell_read = cell);
        (void)mmgr_line_finish(args->err_msg);
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = args->err_msg->out + said, .at = 1u);
    }
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = args->cell);
    EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
    return sent;
}

int cli_notebook_kernel_execute_all(const CliNotebookKernelRunCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    int ran = model != NULL;
    for (unsigned int at = 0u; ran && (at < model->cells); at += 1u)
    {
        if (EMBED_CALL(cell_of, NotebookKernelCtx, .model = (CliNotebookModel *)(embed_word)model, .at = at).type ==
            CLI_NOTEBOOK_CODE)
        {
            ran = EMBED_CALL(cli_notebook_kernel_run.run, CliNotebookKernelRunCfg, .key = args->key, .cell = at,
                             .err_msg = args->err_msg);
        }
    }
    return ran;
}

int cli_notebook_kernel_restart_execute_all(const CliNotebookKernelRunCfg *args)
{
    return EMBED_CALL(cli_notebook_kernel_stop.restart, CliNotebookKernelStopCfg, .key = args->key,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(cli_notebook_kernel_run.run_all, CliNotebookKernelRunCfg, .key = args->key,
                      .err_msg = args->err_msg);
}

int cli_notebook_kernel_interrupt(const CliNotebookKernelStopCfg *args)
{
    const CliNotebookModel *const model =
        EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->kernel == NULL))
    {
        mmgr_line_put(args->err_msg, "no kernel runs for ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    return EMBED_CALL(cli_jupyter_err_msg_kernel_file.interrupt, CliJupyterErrMsgKernelFileCfg, .kernel = model->kernel,
                      .err_msg = args->err_msg);
}

int cli_notebook_kernel_restart(const CliNotebookKernelStopCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if (model == NULL)
    {
        mmgr_line_put(args->err_msg, "no notebook is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    (void)EMBED_CALL(kernel_stopped, NotebookKernelCtx, .model = model);
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        CliNotebookCell cell = EMBED_CALL(cell_of, NotebookKernelCtx, .model = model, .at = at);
        cell.first_run = 0u;
        cell.last_run = 0u;
        cell.edited = 0u;
        EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = at,
                   .cell_read = &cell);
    }
    if (model->gone_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(model->gone_cell, g_cli_notebook_open_owner);
    }
    model->gone_cell = NULL;
    model->gone_count = 0u;
    EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = 0u);
    return EMBED_CALL(kernel_ready, NotebookKernelCtx, .model = model, .err_msg = args->err_msg);
}

int cli_notebook_kernel_end(const CliNotebookKernelStopCfg *args)
{
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = args->key);
    if ((model == NULL) || (model->kernel == NULL))
    {
        return 0;
    }
    if (EMBED_CALL(kernel_stopped, NotebookKernelCtx, .model = model))
    {
        EMBED_CALL(notice_posted, NotebookKernelCtx, .name = "the kernel did not stop when asked and was ended",
                   .at = 1u);
    }
    EMBED_CALL(cli_notebook_model_internal.rows_settled, CliNotebookModelCtx, .model = model, .cell = 0u);
    return 1;
}
