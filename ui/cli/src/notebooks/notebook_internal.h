// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_internal.h: what notebook.c and notebook_merge.c share
#ifndef NOTEBOOK_INTERNAL_H
#define NOTEBOOK_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "notebook.h"

typedef struct CliNotebookTextCtx CliNotebookTextCtx;
typedef struct CliNotebookAtCtx CliNotebookAtCtx;
typedef struct CliNotebookJsonCtx CliNotebookJsonCtx;
typedef struct CliNotebookJsonCellCtx CliNotebookJsonCellCtx;
typedef struct CliNotebookJsonValueCtx CliNotebookJsonValueCtx;
typedef struct CliNotebookCtx CliNotebookCtx;

// a cell's source or an output's text as one text, from a list of lines or a text: a string node made of it
unsigned int cli_notebook_join(const CliNotebookJsonValueCtx *args);
// a cell's source, the empty text where it has none, its length into `length`
const char *cli_notebook_source(const CliNotebookJsonCellCtx *args);
// whether `text` ends with `ending`
int cli_notebook_ends(const CliNotebookTextCtx *args);
// the comment a script's cells are marked with in the language of `path`, by its extension
const char *cli_notebook_comment(const CliNotebookCtx *args);
// the line of a text that starts at `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`;
// where the next starts
const char *cli_notebook_line(const CliNotebookAtCtx *args);
// a text as Jupyter stores it: an array node of its lines, each with its line end but the last
unsigned int cli_notebook_lines_get(const CliNotebookJsonCtx *args);

struct CliNotebookTextCtx
{
    const char *text;
    unsigned long long length;
    const char *ending;
};

struct CliNotebookAtCtx
{
    const char *at;
    const char *end;
    const char **stop;
};

struct CliNotebookJsonCtx
{
    CliJson *json;
    const char *text;
    unsigned long long length;
};

struct CliNotebookJsonCellCtx
{
    const CliJson *json_read;
    unsigned int cell;
    unsigned long long *length_pointer;
};

struct CliNotebookJsonValueCtx
{
    CliJson *json;
    unsigned int value;
};

struct CliNotebookCtx
{
    const char *path;
};

typedef struct
{
    unsigned int (*join)(const CliNotebookJsonValueCtx *args);
} CliNotebookJsonValueNs;
EMBED_TABLE_LAYOUT(CliNotebookJsonValueNs, join);
EMBED_TABLE_STORAGE CliNotebookJsonValueNs cli_notebook_json_value_internal EMBED_UNUSED = {
    .join = cli_notebook_join,
};

typedef struct
{
    const char *(*source)(const CliNotebookJsonCellCtx *args);
} CliNotebookJsonCellNs;
EMBED_TABLE_LAYOUT(CliNotebookJsonCellNs, source);
EMBED_TABLE_STORAGE CliNotebookJsonCellNs cli_notebook_json_cell_internal EMBED_UNUSED = {
    .source = cli_notebook_source,
};

typedef struct
{
    int (*ends)(const CliNotebookTextCtx *args);
} CliNotebookTextNsTable;
EMBED_TABLE_LAYOUT(CliNotebookTextNsTable, ends);
EMBED_TABLE_STORAGE CliNotebookTextNsTable cli_notebook_text_internal EMBED_UNUSED = {
    .ends = cli_notebook_ends,
};

typedef struct
{
    const char *(*comment)(const CliNotebookCtx *args);
} CliNotebookNsTable;
EMBED_TABLE_LAYOUT(CliNotebookNsTable, comment);
EMBED_TABLE_STORAGE CliNotebookNsTable cli_notebook_internal EMBED_UNUSED = {
    .comment = cli_notebook_comment,
};

typedef struct
{
    const char *(*line)(const CliNotebookAtCtx *args);
} CliNotebookAtNs;
EMBED_TABLE_LAYOUT(CliNotebookAtNs, line);
EMBED_TABLE_STORAGE CliNotebookAtNs cli_notebook_at_internal EMBED_UNUSED = {
    .line = cli_notebook_line,
};

typedef struct
{
    unsigned int (*lines_get)(const CliNotebookJsonCtx *args);
} CliNotebookJsonNs;
EMBED_TABLE_LAYOUT(CliNotebookJsonNs, lines_get);
EMBED_TABLE_STORAGE CliNotebookJsonNs cli_notebook_json_internal EMBED_UNUSED = {
    .lines_get = cli_notebook_lines_get,
};

#endif
