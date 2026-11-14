// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_cells_internal.h: what notebook_cells.c, notebook_cells_rows.c, notebook_edit.c and notebook_kernel.c
// share: the model of an open notebook, found by its key, and the settling every change of it runs
#ifndef NOTEBOOK_CELLS_INTERNAL_H
#define NOTEBOOK_CELLS_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"
#include "jupyter.h"
#include "notebook_cells.h"

typedef struct CliNotebookModelCtx CliNotebookModelCtx;

// an output's types: a stream, a run's value, data displayed, and an error
#define CLI_NOTEBOOK_STREAM 0u
#define CLI_NOTEBOOK_RESULT 1u
#define CLI_NOTEBOOK_DISPLAY 2u
#define CLI_NOTEBOOK_ERROR 3u
// a kernel's states: none, starting, waiting for work and running
#define CLI_NOTEBOOK_KERNEL_NONE 0u
#define CLI_NOTEBOOK_KERNEL_STARTING 1u
#define CLI_NOTEBOOK_KERNEL_IDLE 2u
#define CLI_NOTEBOOK_KERNEL_BUSY 3u
// the bytes a cell's or an output's document key takes past the model's key: `#`, two numbers of ten digits, `.`, and
// the end
#define CLI_NOTEBOOK_KEY_MORE 24u

// A cell of a model: the serial its document is keyed by, its type, its node among the cells of the tree read where it
// was read, CLI_JSON_NONE for one added, its source's lines, its outputs' place among the model's and how many, the
// rows its outputs take, its run count, CLI_NOTEBOOK_NONE before one, the clock of its last edit, of its first and of
// its last run, 0 for none, whether it runs, failed and is out of date, and the id its run's messages answer, empty
// for none
typedef struct
{
    unsigned int serial;
    unsigned int type;
    unsigned int origin;
    unsigned int lines;
    unsigned int output_first;
    unsigned int outputs;
    unsigned int output_rows;
    unsigned int count;
    unsigned int edited;
    unsigned int first_run;
    unsigned int last_run;
    int running;
    int failed;
    int stale;
    char run_id[CLI_KERNEL_ID];
} CliNotebookCell;

// An output: the serial its document is keyed by, its type, whether it is a stream written to stderr, its lines, the
// rows it takes, the rows before it among its cell's outputs, whether it is displayed whole past its cut, whether its text
// ends a line, which its document leaves off, and its JSON as a notebook stores it, where it starts among the model's
// output texts and its bytes
typedef struct
{
    unsigned int serial;
    unsigned int type;
    int stderr_stream;
    unsigned int lines;
    unsigned int rows;
    unsigned int row;
    int whole;
    int line_ended;
    unsigned long long json;
    unsigned long long json_length;
} CliNotebookOutput;

// A cell deleted after it ran, the clocks of its first and its last run, for the cells that are on it
typedef struct
{
    unsigned int first_run;
    unsigned int last_run;
} CliNotebookGone;

// the max bytes of the sentence last asked of a model, with its end
#define CLI_NOTEBOOK_SENTENCE 4096u

// the least bytes the ring of a model's kernel messages holds once a message comes
#define CLI_NOTEBOOK_RING_LEAST 16384u

// the notebooks open as models at once, at max, and the place in the models' cell of the model `at_`
#define CLI_NOTEBOOK_MODELS_MAX 64u
#define CLI_NOTEBOOK_MODEL_AT(at_) ((unsigned long long)(at_) * sizeof(CliNotebookModel *))

// A notebook open as a model, in one cell of its own with its key and its path after it, its place fixed while it
// is open, which the kernel's receiver is passed as its context. The tree its file was read into and the root of
// it; the cells and the row each starts at, one more than the cells, in one movable cell, the rows' starts from
// `first_row_at`; the outputs, in the order of their cells, in a movable cell, and their JSON texts in another; the
// cells deleted after they ran, in a movable cell; each movable cell read by copy and written in place. The next
// serial, the clock its edits and runs count by; the top row, the rows the work shows; the cell being edited and the
// cell its menu was built for, CLI_NOTEBOOK_NONE for none; whether it changed since it was saved; its kernel, the
// kernel's state, and the kernel's name, display text and language each in the model's own bytes; the sentence last
// asked of it in the model's own bytes with its bytes; and the ring its kernel's messages wait in, a word locking it,
// its cell, the bytes the messages pending take and the bytes it has
typedef struct
{
    int reserved;
    char key_text[CLI_PATH_MAX_LEN];
    char path_text[CLI_PATH_MAX_LEN];
    const char *key;
    const char *path;
    CliJson json;
    unsigned int root;
    const CarcerMovableEntry *cells_cell;
    unsigned long long first_row_at;
    unsigned int cells;
    const CarcerMovableEntry *outputs_cell;
    unsigned int outputs;
    const CarcerMovableEntry *text_cell;
    unsigned long long output_used;
    const CarcerMovableEntry *gone_cell;
    unsigned int gone_count;
    unsigned int next_serial;
    unsigned int clock;
    unsigned long long top;
    unsigned int displayed;
    unsigned int focused;
    unsigned int menu_cell;
    int dirty;
    CliKernel *kernel;
    unsigned int state;
    char kernel_name[CLI_NAME_MAX_LEN];
    char kernel_display[CLI_NAME_MAX_LEN];
    char kernel_language[CLI_NAME_MAX_LEN];
    char sentence[CLI_NOTEBOOK_SENTENCE];
    unsigned long long sentence_length;
    CliGate ring_lock;
    const CarcerMovableEntry *ring_cell;
    unsigned long long ring_used;
    unsigned long long ring_capacity;
} CliNotebookModel;

// the places in the model's movable cells of the cell `at_`, of the row the cell `at_` starts at, of the output `at_`
// and of the gone cell `at_`
#define CLI_NOTEBOOK_CELL_AT(at_) ((unsigned long long)(at_) * sizeof(CliNotebookCell))
#define CLI_NOTEBOOK_FIRST_ROW_AT(model_, at_)                                                                         \
    ((model_)->first_row_at + ((unsigned long long)(at_) * sizeof(unsigned int)))
#define CLI_NOTEBOOK_OUTPUT_AT(at_) ((unsigned long long)(at_) * sizeof(CliNotebookOutput))
#define CLI_NOTEBOOK_GONE_AT(at_) ((unsigned long long)(at_) * sizeof(CliNotebookGone))

// the model open under `key`, NULL for none; the model a model's place is for, NULL past the last, for the step
// and the kernels; the key of the cell `cell`'s document, or of its output `output` where that is not
// CLI_NOTEBOOK_NONE, written into `out`, sized for the model's key and CLI_NOTEBOOK_KEY_MORE
CliNotebookModel *cli_notebook_model_of(const CliNotebookModelCtx *args);
CliNotebookModel *cli_notebook_model_at(const CliNotebookModelCtx *args);
void cli_notebook_model_key(const CliNotebookModelCtx *args);
// the cell `cell` read out of the model's cells into `cell_out`, and `cell_read` written as the cell `cell`; the output
// `output` read into `output_out`, and `output_read` written as the output `output`; the row the cell `cell` starts at;
// and the gone cell `at` read into `gone_out`
void cli_notebook_model_cell_read(const CliNotebookModelCtx *args);
void cli_notebook_model_cell_written(const CliNotebookModelCtx *args);
void cli_notebook_model_output_read(const CliNotebookModelCtx *args);
void cli_notebook_model_output_written(const CliNotebookModelCtx *args);
unsigned int cli_notebook_model_first_row(const CliNotebookModelCtx *args);
void cli_notebook_model_gone_read(const CliNotebookModelCtx *args);
// the rows each cell starts at settled after a cell's rows changed, from the cell `cell` on; and every cell's out of
// date mark settled after a run, an edit or a deletion
void cli_notebook_model_rows_settled(const CliNotebookModelCtx *args);
void cli_notebook_model_stale_settled(const CliNotebookModelCtx *args);
// the source lines of the cell `cell` read again from its document after an edit
void cli_notebook_model_lines_read(const CliNotebookModelCtx *args);
// the outputs of the cell `cell` let go, their documents and their texts, the cell's failed mark cleared; and the
// rows the cell's outputs take settled, each output's rows before it among them, after an output was displayed whole
void cli_notebook_model_outputs_released(const CliNotebookModelCtx *args);
void cli_notebook_model_outputs_settled(const CliNotebookModelCtx *args);
// the model's kernel name, display text and language positioned from `size` bytes of `text`, the three each with its end:
// 1, or 0 where the cells cannot meet them; and every message waiting in the model's ring let go
int cli_notebook_model_kernel_positioned(const CliNotebookModelCtx *args);
void cli_notebook_model_ring_cleared(const CliNotebookModelCtx *args);
// the cell `cell`'s document stored with `size` bytes of `text` and colored by the grammar its type and the kernel's
// language take: 1, or 0 where the store refused it
int cli_notebook_model_source_stored(const CliNotebookModelCtx *args);
// the cells positioned anew for `count` cells, the cells before `cell` located, a cell of none at `cell` where `count` is
// one more than the cells, the cell `cell` taken out where it is one fewer: 1, or 0 where the cells cannot meet it,
// the model as it was
int cli_notebook_model_cells_positioned(const CliNotebookModelCtx *args);
// the run `id` bound to the cell `cell`, its messages applied to it from the next step
void cli_notebook_model_bound(const CliNotebookModelCtx *args);
// what a kernel sent, copied into the ring of the model its context is, the ring taken anew larger where the message
// does not fit, and the view's thread woken: the receiver every notebook's kernel is started with
void cli_notebook_model_received(const CliKernelRecvCfg *args);

struct CliNotebookModelCtx
{
    const char *key;
    CliNotebookModel *model;
    unsigned int at;
    unsigned int cell;
    unsigned int output;
    unsigned int count;
    const char *id;
    const char *text;
    unsigned long long size;
    char *out;
    CliNotebookCell *cell_out;
    const CliNotebookCell *cell_read;
    CliNotebookOutput *output_out;
    const CliNotebookOutput *output_read;
    CliNotebookGone *gone_out;
};

typedef struct
{
    CliNotebookModel *(*of)(const CliNotebookModelCtx *args);
    CliNotebookModel *(*at)(const CliNotebookModelCtx *args);
    void (*key)(const CliNotebookModelCtx *args);
    void (*cell_read)(const CliNotebookModelCtx *args);
    void (*cell_written)(const CliNotebookModelCtx *args);
    void (*output_read)(const CliNotebookModelCtx *args);
    void (*output_written)(const CliNotebookModelCtx *args);
    unsigned int (*first_row)(const CliNotebookModelCtx *args);
    void (*gone_read)(const CliNotebookModelCtx *args);
    void (*rows_settled)(const CliNotebookModelCtx *args);
    void (*stale_settled)(const CliNotebookModelCtx *args);
    void (*lines_read)(const CliNotebookModelCtx *args);
    void (*outputs_released)(const CliNotebookModelCtx *args);
    void (*outputs_settled)(const CliNotebookModelCtx *args);
    int (*kernel_positioned)(const CliNotebookModelCtx *args);
    void (*ring_cleared)(const CliNotebookModelCtx *args);
    int (*source_stored)(const CliNotebookModelCtx *args);
    int (*cells_positioned)(const CliNotebookModelCtx *args);
    void (*bound)(const CliNotebookModelCtx *args);
} CliNotebookModelNs;
EMBED_TABLE_LAYOUT(CliNotebookModelNs, of, at, key, cell_read, cell_written, output_read, output_written, first_row,
                   gone_read, rows_settled, stale_settled, lines_read, outputs_released, outputs_settled, kernel_positioned,
                   ring_cleared, source_stored, cells_positioned, bound);
EMBED_TABLE_STORAGE CliNotebookModelNs cli_notebook_model_internal EMBED_UNUSED = {
    .of = cli_notebook_model_of,
    .at = cli_notebook_model_at,
    .key = cli_notebook_model_key,
    .cell_read = cli_notebook_model_cell_read,
    .cell_written = cli_notebook_model_cell_written,
    .output_read = cli_notebook_model_output_read,
    .output_written = cli_notebook_model_output_written,
    .first_row = cli_notebook_model_first_row,
    .gone_read = cli_notebook_model_gone_read,
    .rows_settled = cli_notebook_model_rows_settled,
    .stale_settled = cli_notebook_model_stale_settled,
    .lines_read = cli_notebook_model_lines_read,
    .outputs_released = cli_notebook_model_outputs_released,
    .outputs_settled = cli_notebook_model_outputs_settled,
    .kernel_positioned = cli_notebook_model_kernel_positioned,
    .ring_cleared = cli_notebook_model_ring_cleared,
    .source_stored = cli_notebook_model_source_stored,
    .cells_positioned = cli_notebook_model_cells_positioned,
    .bound = cli_notebook_model_bound,
};

#endif
