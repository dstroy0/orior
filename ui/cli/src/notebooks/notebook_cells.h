// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_cells.h: a notebook open in the window as cells: the notebook a file's text reads as, a Jupyter notebook's
// JSON or a percent script, arranged as a model under the key its view is under. Each cell's source is a text of
// the document store under `<key>#<id>`, colored by the kernel language's grammar or the markdown grammar, and each
// output's text one under `<key>#<id>.<output>`, colored by the xterm grammar. The model lists the cells in order,
// each with its type, its run count, its outputs, whether it runs, failed or is on cells changed since it ran,
// and the rows it takes in the work: a row each line of its source, one for a markdown cell not being edited, a row
// each line of its outputs, an output of more than twice CLI_NOTEBOOK_CUT_LINES lines cut to its first and last
// lines with a row between them, and the row that adds a cell after it. The row each cell starts at is in one
// cell of the model, sorted, and a row is found among them by a binary search. A kernel's messages reach the model
// through a ring the receiver fills on the kernel's thread and the step drains on the view's (notebook_cells.c,
// notebook_cells_rows.c)
#ifndef NOTEBOOK_CELLS_H
#define NOTEBOOK_CELLS_H
#include "../command_line/cli_internal.h"
#include "../editor/view.h"
#include "embed_dispatch_layout.h"

typedef struct CliNotebookCellsOpenCfg CliNotebookCellsOpenCfg;
typedef struct CliNotebookCellsSaveCfg CliNotebookCellsSaveCfg;
typedef struct CliNotebookCellsStepCfg CliNotebookCellsStepCfg;
typedef struct CliNotebookCellsRowCfg CliNotebookCellsRowCfg;
typedef struct CliNotebookCellsFrameCfg CliNotebookCellsFrameCfg;
typedef struct CliNotebookCellsScrollCfg CliNotebookCellsScrollCfg;
typedef struct CliNotebookCellsTextCfg CliNotebookCellsTextCfg;

// the cells the open notebooks occupy: each model, its cells and outputs, its kernel's ring, its frames and the

// no cell, output or place
#define CLI_NOTEBOOK_NONE 0xffffffffu
// a cell's types
#define CLI_NOTEBOOK_CODE 0u
#define CLI_NOTEBOOK_MARKDOWN 1u
#define CLI_NOTEBOOK_RAW 2u
// the lines an output shows at its start and at its end once it is cut
#define CLI_NOTEBOOK_CUT_LINES 40u

// A row's types: the row that adds a cell after its cell, a line of a cell's source, a markdown cell not being edited
// positioned as one row, a line of an output, and the row located for the lines of a cut output not displayed
#define CLI_NOTEBOOK_ROW_ADD 0u
#define CLI_NOTEBOOK_ROW_SOURCE 1u
#define CLI_NOTEBOOK_ROW_MARKDOWN 2u
#define CLI_NOTEBOOK_ROW_OUTPUT 3u
#define CLI_NOTEBOOK_ROW_CUT 4u
// A row's flags: the first row of its cell, its last, a row of the cell being edited, of a cell that runs, of a cell
// whose outputs carry an error, of a cell out of date, of a markdown cell, and a line of a stream written to stderr or
// of an error
#define CLI_NOTEBOOK_FLAG_FIRST 1u
#define CLI_NOTEBOOK_FLAG_LAST 2u
#define CLI_NOTEBOOK_FLAG_FOCUSED 4u
#define CLI_NOTEBOOK_FLAG_RUNNING 8u
#define CLI_NOTEBOOK_FLAG_FAILED 16u
#define CLI_NOTEBOOK_FLAG_STALE 32u
#define CLI_NOTEBOOK_FLAG_MARKDOWN 64u
#define CLI_NOTEBOOK_FLAG_STDERR 128u

// The sentences the rows show: a code cell's count, `[3]`, `[*]` while it runs, `[ ]` before it ran; the kernel's
// state, `<display>: <state>` or `no kernel`; why a cell is out of date, the cells it is on; the line of a cut
// output, `... N lines ...`; its button, `Show all N lines`; and the line an empty markdown cell shows
#define CLI_NOTEBOOK_TEXT_COUNT 0u
#define CLI_NOTEBOOK_TEXT_STATE 1u
#define CLI_NOTEBOOK_TEXT_STALE 2u
#define CLI_NOTEBOOK_TEXT_CUT 3u
#define CLI_NOTEBOOK_TEXT_MORE 4u
#define CLI_NOTEBOOK_TEXT_PLACEHOLDER 5u

// A row: its type, its cell, the line of its source or of its output, the output it is in, CLI_NOTEBOOK_NONE for
// a row of none, and its flags
typedef struct
{
    unsigned int type;
    unsigned int cell;
    unsigned long long line;
    unsigned int output;
    unsigned int flags;
} CliNotebookRow;

// A cell as the map positions it: the row it starts at, the rows it takes, its type, and whether it is out of date, failed
// or runs
typedef struct
{
    unsigned int first_row;
    unsigned int row_count;
    unsigned int type;
    int stale;
    int failed;
    int running;
} CliNotebookBlock;

// A row of a frame: the row; its text among the frame's bytes and its bytes, a line of a source or an output as UTF-8;
// the sentence it shows among the bytes, the count of a cell's first code row, the line of a cut row, the placeholder
// of an empty markdown row, of no bytes for every other; and the part its runs are in with its place among the
// part's lines, CLI_NOTEBOOK_NONE for a row of none
typedef struct
{
    CliNotebookRow row;
    unsigned long long text;
    unsigned long long length;
    unsigned long long label;
    unsigned long long label_length;
    unsigned int part;
    unsigned int part_line;
} CliNotebookFrameRow;

// A run of rows positioned from the lines of one text: their runs, as the walk saved them, and the labels the runs' tokens
// take, each in a cell of the open notebooks' owner, NULL where the text is not colored
typedef struct
{
    CliDocumentRuns *runs;
    CliDocumentNames *labels;
} CliNotebookFramePart;

// A notebook's frame, every count and pointer the window draws from, immutable once positioned: the model's top row, the
// rows positioned from it, the rows of the notebook, the cell being edited, CLI_NOTEBOOK_NONE for none, and its view's frame,
// NULL for none; the rows; the parts; and the bytes the rows' texts and sentences are in
typedef struct
{
    unsigned long long top;
    unsigned int count;
    unsigned int rows;
    unsigned int focused;
    unsigned int parts;
    CliNotebookFrameRow *row;
    CliNotebookFramePart *part;
    char *bytes;
    CliViewFrame *view;
} CliNotebookFrame;

// the notebook in `size` bytes of `text`, the file at `path`, opened as a model under `key`, each cell's source and
// each output's text stored and colored, in place of a model open under the key: 1, or 0 with why written where the
// text reads as no notebook or the cells cannot meet it; and the model under `key` closed, its kernel stopped, its
// cell being edited closed, every text it stored let go and its cells released
int cli_notebook_model_open(const CliNotebookCellsOpenCfg *args);
void cli_notebook_model_close(const CliNotebookCellsOpenCfg *args);
// the model under `key` written to the file at `full` as the notebook it was read as, each cell's source, outputs and
// run count as they are, the kernel picked in its metadata, the model marked saved: 1, or 0 with why written; and 1
// where a cell's source changed since it was read or saved, or a cell was added, moved, changed in type, cleared or
// deleted, or a kernel picked
int cli_notebook_model_save(const CliNotebookCellsSaveCfg *args);
int cli_notebook_model_dirty(const CliNotebookCellsSaveCfg *args);
// the models begun: `woken` wakes the view's thread as a kernel's message reaches a model's ring; and every ring
// drained, each message applied to the cell whose run it answers, a message whose run is not yet bound located in
// the ring until it is: 1 where a model changed
void cli_notebook_model_begin(const CliNotebookCellsStepCfg *args);
int cli_notebook_model_step(const CliNotebookCellsStepCfg *args);
// the rows of the notebook under `key`, 0 for none open; the row `at` into `row`: 1, or 0 past the last; and the cell
// `at` into `block`: 1, or 0 past the last
unsigned int cli_notebook_model_rows(const CliNotebookCellsRowCfg *args);
int cli_notebook_model_row(const CliNotebookCellsRowCfg *args);
int cli_notebook_model_block(const CliNotebookCellsRowCfg *args);
// `count` rows of the notebook under `key` positioned from its top row into a frame in one cell, the rows the work shows
// recorded for the scroll, through `frame_out`: 1, or 0 with NULL where no notebook is open under the key or the cells
// cannot meet it; and the frame through `frame_out` released, NULL after
int cli_notebook_model_frame_alloc(const CliNotebookCellsFrameCfg *args);
void cli_notebook_model_frame_release(const CliNotebookCellsFrameCfg *args);
// the top row moved by a wheel's `rows`, down positive, and moved to the row at `permille` of the notebook's rows,
// centered on the rows the work shows; each stopping where the last row reaches the work's foot
void cli_notebook_model_scroll(const CliNotebookCellsScrollCfg *args);
void cli_notebook_model_to(const CliNotebookCellsScrollCfg *args);
// the sentence `which` of the notebook under `key`, of its cell `cell` and output `output` where it is one's, in a
// cell of the model sized to it, located until the next sentence is asked, its bytes into `length_out`: the empty
// text where no notebook is open under the key or no such cell or output is
const char *cli_notebook_model_text(const CliNotebookCellsTextCfg *args);

struct CliNotebookCellsOpenCfg
{
    const char *key;
    const char *path;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
};

struct CliNotebookCellsSaveCfg
{
    const char *key;
    const char *full;
    VerbaLinea *err_msg;
};

struct CliNotebookCellsStepCfg
{
    void (*woken)(void);
};

struct CliNotebookCellsRowCfg
{
    const char *key;
    unsigned int at;
    CliNotebookRow *row;
    CliNotebookBlock *block;
};

struct CliNotebookCellsFrameCfg
{
    const char *key;
    unsigned int count;
    CliNotebookFrame **frame_out;
};

struct CliNotebookCellsScrollCfg
{
    const char *key;
    int rows;
    unsigned int permille;
};

struct CliNotebookCellsTextCfg
{
    const char *key;
    unsigned int which;
    unsigned int cell;
    unsigned int output;
    unsigned long long *length_out;
};

typedef struct
{
    int (*open)(const CliNotebookCellsOpenCfg *args);
    void (*close)(const CliNotebookCellsOpenCfg *args);
} CliNotebookCellsOpenNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsOpenNs, open, close);
EMBED_TABLE_STORAGE CliNotebookCellsOpenNs cli_notebook_cells_open EMBED_UNUSED = {
    .open = cli_notebook_model_open,
    .close = cli_notebook_model_close,
};

typedef struct
{
    int (*save)(const CliNotebookCellsSaveCfg *args);
    int (*dirty)(const CliNotebookCellsSaveCfg *args);
} CliNotebookCellsSaveNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsSaveNs, save, dirty);
EMBED_TABLE_STORAGE CliNotebookCellsSaveNs cli_notebook_cells_save EMBED_UNUSED = {
    .save = cli_notebook_model_save,
    .dirty = cli_notebook_model_dirty,
};

typedef struct
{
    void (*begin)(const CliNotebookCellsStepCfg *args);
    int (*step)(const CliNotebookCellsStepCfg *args);
} CliNotebookCellsStepNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsStepNs, begin, step);
EMBED_TABLE_STORAGE CliNotebookCellsStepNs cli_notebook_cells_step EMBED_UNUSED = {
    .begin = cli_notebook_model_begin,
    .step = cli_notebook_model_step,
};

typedef struct
{
    unsigned int (*rows)(const CliNotebookCellsRowCfg *args);
    int (*row)(const CliNotebookCellsRowCfg *args);
    int (*block)(const CliNotebookCellsRowCfg *args);
} CliNotebookCellsRowNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsRowNs, rows, row, block);
EMBED_TABLE_STORAGE CliNotebookCellsRowNs cli_notebook_cells_row EMBED_UNUSED = {
    .rows = cli_notebook_model_rows,
    .row = cli_notebook_model_row,
    .block = cli_notebook_model_block,
};

typedef struct
{
    int (*alloc)(const CliNotebookCellsFrameCfg *args);
    void (*release)(const CliNotebookCellsFrameCfg *args);
} CliNotebookCellsFrameNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsFrameNs, alloc, release);
EMBED_TABLE_STORAGE CliNotebookCellsFrameNs cli_notebook_cells_frame EMBED_UNUSED = {
    .alloc = cli_notebook_model_frame_alloc,
    .release = cli_notebook_model_frame_release,
};

typedef struct
{
    void (*scroll)(const CliNotebookCellsScrollCfg *args);
    void (*to)(const CliNotebookCellsScrollCfg *args);
} CliNotebookCellsScrollNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsScrollNs, scroll, to);
EMBED_TABLE_STORAGE CliNotebookCellsScrollNs cli_notebook_cells_scroll EMBED_UNUSED = {
    .scroll = cli_notebook_model_scroll,
    .to = cli_notebook_model_to,
};

typedef struct
{
    const char *(*text)(const CliNotebookCellsTextCfg *args);
} CliNotebookCellsTextNs;
EMBED_TABLE_LAYOUT(CliNotebookCellsTextNs, text);
EMBED_TABLE_STORAGE CliNotebookCellsTextNs cli_notebook_cells_text EMBED_UNUSED = {
    .text = cli_notebook_model_text,
};

#endif
