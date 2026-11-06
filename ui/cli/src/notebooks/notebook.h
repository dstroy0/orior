// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook.h: notebooks as files: a Jupyter notebook's JSON, and a script of `# %%` cells, as Jupytext's percent form
// writes one (notebook.c reads and writes them, notebook_merge.c shows and merges them). Each reads as the same
// notebook, a list of cells, each with its kind, its source as one text, its outputs, the count of its last run and
// what else the file stored of it; and each writes back as it was read.
//
// A notebook's JSON is written as Jupyter writes it: its keys in order, one space to each level, and each cell's
// source and each output's text as lists of lines, which confines a diff of it to the lines that changed. A script
// retains its lines before the first cell, writes a markdown cell's lines behind the language's comment, and contains
// no outputs.
//
// Every call works in a tree its caller hands it, `tokens` sized for `tokens_max` for the JSON it reads
#ifndef NOTEBOOK_H
#define NOTEBOOK_H
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliNotebookTextCfg CliNotebookTextCfg;
typedef struct CliNotebookPathCfg CliNotebookPathCfg;
typedef struct CliNotebookCfg CliNotebookCfg;
typedef struct CliNotebookPathBookCfg CliNotebookPathBookCfg;

// the most cells of one side a merge aligns
#define CLI_NOTEBOOK_CELLS 1024u

// whether a file is a notebook: a .ipynb, or a script whose lines mark cells with `# %%`
int cli_notebook_is(const CliNotebookPathCfg *args);
// the notebook at `path` read from `size` bytes of its text: its node, or CLI_JSON_NONE with why given
unsigned int cli_notebook_read(const CliNotebookPathCfg *args);
// the text of the notebook `book`, to write to `path`, into `out`
void cli_notebook_write(const CliNotebookPathBookCfg *args);
// a notebook's JSON without its outputs and run counts, as a commit that leaves them out writes it, into `out`: 1, or
// 0 with why given
int cli_notebook_without_outputs(const CliNotebookTextCfg *args);
// a notebook's cells as a diff compares them, into `out`: each cell's kind, its source and its outputs' text, the run
// counts, ids and metadata that change on every run left out. 1, or 0 with why given
int cli_notebook_for_diff(const CliNotebookTextCfg *args);
// the cells two sides of a merge make of the cells of their base, as Jupyter's notebooks store them: each cell one side
// changed taken from it, each cell both changed alike taken once, and each cell they changed apart, or one changed and
// the other deleted, a conflict of the two to choose between. Run counts, ids and metadata never conflict; a cell's
// outputs go with its source. Cells either side added stand after the cell they follow there. `{"book": ours with no
// cells, "entries": [{"take": cell} or {"conflict": {"base", "ours", "theirs"}}]}`
unsigned int cli_notebook_merge(const CliNotebookCfg *args);

struct CliNotebookTextCfg
{
    const char *text;
    unsigned long long size;
    CliJson *json;
    CfgJsonToken *tokens;
    unsigned int tokens_max;
    VerbaLinea *err_msg;
    VerbaLinea *out;
};

struct CliNotebookPathCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    CliJson *json;
    CfgJsonToken *tokens;
    unsigned int tokens_max;
    VerbaLinea *err_msg;
};

struct CliNotebookCfg
{
    CliJson *json;
    unsigned int base;
    unsigned int ours;
    unsigned int theirs;
};

struct CliNotebookPathBookCfg
{
    const char *path;
    CliJson *json;
    unsigned int book;
    VerbaLinea *out;
};

typedef struct
{
    int (*is)(const CliNotebookPathCfg *args);
    unsigned int (*read)(const CliNotebookPathCfg *args);
} CliNotebookPathNs;
EMBED_TABLE_LAYOUT(CliNotebookPathNs, is, read);
EMBED_TABLE_STORAGE CliNotebookPathNs cli_notebook_path EMBED_UNUSED = {
    .is = cli_notebook_is,
    .read = cli_notebook_read,
};

typedef struct
{
    void (*write)(const CliNotebookPathBookCfg *args);
} CliNotebookPathBookNs;
EMBED_TABLE_LAYOUT(CliNotebookPathBookNs, write);
EMBED_TABLE_STORAGE CliNotebookPathBookNs cli_notebook_path_book EMBED_UNUSED = {
    .write = cli_notebook_write,
};

typedef struct
{
    int (*without_outputs)(const CliNotebookTextCfg *args);
    int (*for_diff)(const CliNotebookTextCfg *args);
} CliNotebookTextNs;
EMBED_TABLE_LAYOUT(CliNotebookTextNs, without_outputs, for_diff);
EMBED_TABLE_STORAGE CliNotebookTextNs cli_notebook_text EMBED_UNUSED = {
    .without_outputs = cli_notebook_without_outputs,
    .for_diff = cli_notebook_for_diff,
};

typedef struct
{
    unsigned int (*merge)(const CliNotebookCfg *args);
} CliNotebookNs;
EMBED_TABLE_LAYOUT(CliNotebookNs, merge);
EMBED_TABLE_STORAGE CliNotebookNs cli_notebook EMBED_UNUSED = {
    .merge = cli_notebook_merge,
};

#endif
