// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_edit.h: an open notebook's cells edited: one cell at a time has an editor's view on its document, opened
// as it takes the keys and closed as it lets them go, every input passed to that view and the cell's rows following
// its lines; the notebook's own keys taken before the view sees them; cells added, changed in type, moved, cleared and
// deleted; and the menu of a cell built as the menus' own rows, with the commands its rows run (notebook_edit.c)
#ifndef NOTEBOOK_EDIT_H
#define NOTEBOOK_EDIT_H
#include "../command_line/cli_internal.h"
#include "../editor/view.h"
#include "embed_dispatch_layout.h"

typedef struct CliNotebookEditFocusCfg CliNotebookEditFocusCfg;
typedef struct CliNotebookEditKeyCfg CliNotebookEditKeyCfg;
typedef struct CliNotebookEditCellCfg CliNotebookEditCellCfg;
typedef struct CliNotebookEditMenuCfg CliNotebookEditMenuCfg;

// the columns the view of the cell being edited positions its rows out to
#define CLI_NOTEBOOK_EDIT_COLUMNS 1024u

// the keys passed to the cell `cell` of the notebook under `key`, the cell that had them let go first: a view opened on
// its document, sized to its lines and one more, its cursor at `line` and `column`: 1, or 0 with why written; the
// cell that has the keys let go, its view closed and its rows following its lines; and `input` passed to that cell's
// view, the cell's rows and the rows after it following a change in its lines, the cell marked edited and the
// notebook changed where its text changed: 1, or 0 where no cell has the keys
int cli_notebook_cell_focus(const CliNotebookEditFocusCfg *args);
void cli_notebook_cell_blur(const CliNotebookEditFocusCfg *args);
int cli_notebook_cell_input(const CliNotebookEditFocusCfg *args);
// the key `word` pressed while a cell of the notebook under `key` has the keys: Shift+Enter runs the cell and gives
// the keys to the next, letting them go at the last; Ctrl+Enter and Meta+Enter run it, the keys staying on it; Escape
// lets them go: 1 where the key was the notebook's, before the view sees it
int cli_notebook_cell_key(const CliNotebookEditKeyCfg *args);
// a cell of `type` added after the cell `cell`, at the end where `cell` is CLI_NOTEBOOK_NONE, the keys passed to it;
// the cell `cell` changed from code to markdown or back, its outputs cleared; moved by `by`, one up or one down; its
// outputs cleared; and deleted, a cell that ran located among the cells gone for the cells that are on it. Each 1,
// or 0 where no such cell is or the cells cannot meet it, the notebook marked changed where it changed
int cli_notebook_cell_add(const CliNotebookEditCellCfg *args);
int cli_notebook_cell_retype(const CliNotebookEditCellCfg *args);
int cli_notebook_cell_move(const CliNotebookEditCellCfg *args);
int cli_notebook_cell_outputs_clear(const CliNotebookEditCellCfg *args);
int cli_notebook_cell_remove(const CliNotebookEditCellCfg *args);
// the menu of the cell `cell` built as the menus' own rows: Run, Debug, which waits on the debugger, Change to
// Markdown or Change to Code, Move Up and Move Down, each refused at its end, Clear Outputs and Delete: 1, or 0 where
// no such cell is; and the command `word` of a row run on the cell `cell`, the cell the menu was built for where
// `cell` is CLI_NOTEBOOK_NONE, notebook-show-all showing the output `output` whole: 1 where the word was the
// notebook's
int cli_notebook_cell_menu(const CliNotebookEditMenuCfg *args);
int cli_notebook_cell_command(const CliNotebookEditMenuCfg *args);

struct CliNotebookEditFocusCfg
{
    const char *key;
    unsigned int cell;
    unsigned long long line;
    unsigned long long column;
    const CliViewInput *input;
    VerbaLinea *err_msg;
};

struct CliNotebookEditKeyCfg
{
    const char *key;
    const char *word;
};

struct CliNotebookEditCellCfg
{
    const char *key;
    unsigned int cell;
    unsigned int type;
    int by;
};

struct CliNotebookEditMenuCfg
{
    const char *key;
    unsigned int cell;
    unsigned int output;
    const char *word;
};

typedef struct
{
    int (*focus)(const CliNotebookEditFocusCfg *args);
    void (*blur)(const CliNotebookEditFocusCfg *args);
    int (*input)(const CliNotebookEditFocusCfg *args);
} CliNotebookEditFocusNs;
EMBED_TABLE_LAYOUT(CliNotebookEditFocusNs, focus, blur, input);
EMBED_TABLE_STORAGE CliNotebookEditFocusNs cli_notebook_edit_focus EMBED_UNUSED = {
    .focus = cli_notebook_cell_focus,
    .blur = cli_notebook_cell_blur,
    .input = cli_notebook_cell_input,
};

typedef struct
{
    int (*key)(const CliNotebookEditKeyCfg *args);
} CliNotebookEditKeyNs;
EMBED_TABLE_LAYOUT(CliNotebookEditKeyNs, key);
EMBED_TABLE_STORAGE CliNotebookEditKeyNs cli_notebook_edit_key EMBED_UNUSED = {
    .key = cli_notebook_cell_key,
};

typedef struct
{
    int (*add)(const CliNotebookEditCellCfg *args);
    int (*retype)(const CliNotebookEditCellCfg *args);
    int (*move)(const CliNotebookEditCellCfg *args);
    int (*outputs_clear)(const CliNotebookEditCellCfg *args);
    int (*remove)(const CliNotebookEditCellCfg *args);
} CliNotebookEditCellNs;
EMBED_TABLE_LAYOUT(CliNotebookEditCellNs, add, retype, move, outputs_clear, remove);
EMBED_TABLE_STORAGE CliNotebookEditCellNs cli_notebook_edit_cell EMBED_UNUSED = {
    .add = cli_notebook_cell_add,
    .retype = cli_notebook_cell_retype,
    .move = cli_notebook_cell_move,
    .outputs_clear = cli_notebook_cell_outputs_clear,
    .remove = cli_notebook_cell_remove,
};

typedef struct
{
    int (*menu)(const CliNotebookEditMenuCfg *args);
    int (*command)(const CliNotebookEditMenuCfg *args);
} CliNotebookEditMenuNs;
EMBED_TABLE_LAYOUT(CliNotebookEditMenuNs, menu, command);
EMBED_TABLE_STORAGE CliNotebookEditMenuNs cli_notebook_edit_menu EMBED_UNUSED = {
    .menu = cli_notebook_cell_menu,
    .command = cli_notebook_cell_command,
};

#endif
