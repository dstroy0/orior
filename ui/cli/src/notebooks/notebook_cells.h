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
// the lines an output shows at its start and at its end once the notebook cuts it
#define CLI_NOTEBOOK_CUT_LINES 40u

// A row's types: the row that adds a cell after its cell, and a line of a cell's source. Then a markdown cell out of
// the edit, laid out as one row, and a line of an output. Last, the row for the lines of a cut output not on display
#define CLI_NOTEBOOK_ROW_ADD 0u
#define CLI_NOTEBOOK_ROW_SOURCE 1u
#define CLI_NOTEBOOK_ROW_MARKDOWN 2u
#define CLI_NOTEBOOK_ROW_OUTPUT 3u
#define CLI_NOTEBOOK_ROW_CUT 4u
// A row's flags: the first row of its cell, its last, and a row of the cell in the edit. Then a row of a cell that
// runs, or whose outputs carry an error. Then a row of a cell out of date, and of a markdown cell. Last, a line of a
// stream written to stderr or of an error
#define CLI_NOTEBOOK_FLAG_FIRST 1u
#define CLI_NOTEBOOK_FLAG_LAST 2u
#define CLI_NOTEBOOK_FLAG_FOCUSED 4u
#define CLI_NOTEBOOK_FLAG_RUNNING 8u
#define CLI_NOTEBOOK_FLAG_FAILED 16u
#define CLI_NOTEBOOK_FLAG_STALE 32u
#define CLI_NOTEBOOK_FLAG_MARKDOWN 64u
#define CLI_NOTEBOOK_FLAG_STDERR 128u

// The sentences the rows show: a code cell's count, `[3]`, `[*]` while it runs, `[ ]` before it ran. Then the kernel's
// state, `<display>: <state>` or `no kernel`, and why a cell is out of date, the cells it is on. Then the line of a
// cut output, `... N lines ...`, and its button, `Show all N lines`. Last, the line an empty markdown cell shows
#define CLI_NOTEBOOK_TEXT_COUNT 0u
#define CLI_NOTEBOOK_TEXT_STATE 1u
#define CLI_NOTEBOOK_TEXT_STALE 2u
#define CLI_NOTEBOOK_TEXT_CUT 3u
#define CLI_NOTEBOOK_TEXT_MORE 4u
#define CLI_NOTEBOOK_TEXT_PLACEHOLDER 5u

// A row: its type, its cell, and the line of its source or of its output. Then the output it is in, CLI_NOTEBOOK_NONE
// for a row of none, and its flags
typedef struct
{
    unsigned int type;
    unsigned int cell;
    unsigned long long line;
    unsigned int output;
    unsigned int flags;
} CliNotebookRow;

// A cell as the map lays it out: the row it starts at, the rows it takes, and its type. Last, whether it is out of
// date, failed or runs
typedef struct
{
    unsigned int first_row;
    unsigned int row_count;
    unsigned int type;
    int stale;
    int failed;
    int running;
} CliNotebookBlock;

// A row of a frame. It holds the row, and its text among the frame's bytes with its bytes, a line of a source or an
// output as UTF-8. Then the sentence it shows among the bytes: the count of a cell's first code row, or the line of a
// cut row. It is the placeholder of an empty markdown row, and no bytes for every other. Last, the part its runs are
// in, with its place among the part's lines, CLI_NOTEBOOK_NONE for a row of none
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

// A notebook's frame, every count and pointer the window draws from, which stays as it is once laid out. It holds the
// model's top row, the rows laid out from it, and the rows of the notebook. Then the cell in the edit,
// CLI_NOTEBOOK_NONE for none. Then the bytes of the frame its view keeps, 0 for none, which a reader reads from the
// view. Last, the rows, and the bytes the rows' texts and sentences are in
typedef struct
{
    unsigned long long top;
    unsigned int count;
    unsigned int rows;
    unsigned int focused;
    CliNotebookFrameRow *row;
    char *bytes;
    unsigned long long view_bytes;
} CliNotebookFrame;

// The notebook in `size` bytes of `text`, the file at `path`, opened as a model under `key`, in place of a model open
// under the key. Each cell's source and each output's text goes into the store with its colors. Returns 1, or 0 with
// why written where the text reads as no notebook or the cells cannot meet it.
//
// Then the model under `key` closed: its kernel stops, and its cell in the edit closes. Every text it stored goes, and
// its cells go back
int cli_notebook_model_open(const CliNotebookCellsOpenCfg *args);
void cli_notebook_model_close(const CliNotebookCellsOpenCfg *args);
// The model under `key` written to the file at `full` as the notebook it came from. Each cell's source, outputs and
// run count go as they are, and the kernel picked goes in its metadata. The model then reads as saved. Returns 1, or
// 0 with why written. Then 1 where a cell's source changed since the last read or save. It is also 1 where a cell
// came, moved, changed in type, cleared or went, or a kernel came
int cli_notebook_model_save(const CliNotebookCellsSaveCfg *args);
int cli_notebook_model_dirty(const CliNotebookCellsSaveCfg *args);
// The models begun: `woken` wakes the view's thread as a kernel's message reaches a model's ring. Then every ring
// drained, each message applied to the cell whose run it answers. A message whose run has no cell yet stays in the
// ring until it has one. Returns 1 where a model changed
void cli_notebook_model_begin(const CliNotebookCellsStepCfg *args);
int cli_notebook_model_step(const CliNotebookCellsStepCfg *args);
// the rows of the notebook under `key`, 0 for none open; the row `at` into `row`: 1, or 0 past the last; and the cell
// `at` into `block`: 1, or 0 past the last
unsigned int cli_notebook_model_rows(const CliNotebookCellsRowCfg *args);
int cli_notebook_model_row(const CliNotebookCellsRowCfg *args);
int cli_notebook_model_block(const CliNotebookCellsRowCfg *args);
// `count` rows of the notebook under `key` laid out from its top row into a frame in one cell, through `frame_out`.
// The rows the work shows go on record for the scroll. Returns 1, or 0 with NULL where no notebook is open under the
// key or the cells cannot meet it. Then the frame through `frame_out` released, NULL after
int cli_notebook_model_frame_alloc(const CliNotebookCellsFrameCfg *args);
void cli_notebook_model_frame_release(const CliNotebookCellsFrameCfg *args);
// The top row moved by a wheel's `rows`, down positive. Then the top moved to the row at `permille` of the notebook's
// rows, centered on the rows the work shows. Each stops where the last row reaches the work's foot
void cli_notebook_model_scroll(const CliNotebookCellsScrollCfg *args);
void cli_notebook_model_to(const CliNotebookCellsScrollCfg *args);
// The sentence `which` of the notebook under `key`, of its cell `cell` and output `output` where it is one's. It lies
// in a cell of the model sized to it, until the next ask for a sentence, with its bytes into `length_out`. It is the
// empty text where no notebook is open under the key or no such cell or output is
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
