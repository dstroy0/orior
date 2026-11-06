// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_grid.h: the Database tool's right column as the window reads it, a statement's result as numbers. A run's answer
// is adopted into two cells: the head, every column's label and kind, the result's sentence and its message lines;
// and the stretch of rows stored, one start a cell into one text of every cell's bytes, a null flag a cell. Each
// column's width in letters is the longest of its label and its first rows' cells, cut to a most, and its start the
// sum of the widths before it, both laid once at the adopt. The window's draw reaches the first and last row it shows
// as two numbers; the step reads the rows the stretch lacks from the run in one read, as far ahead the way the rows
// move as their speed carries over two reads, and lets go the rows far behind the motion first (db_grid.c)
#ifndef DB_GRID_H
#define DB_GRID_H
#include "db.h"
#include "embed_dispatch_layout.h"

typedef struct CliDbGridCfg CliDbGridCfg;
typedef struct CliDbGridStepCfg CliDbGridStepCfg;

// the rows' kinds: a row of cells, a row not stored yet, the row past the rows read while the server has more, a line
// of a failure, and a notice or what a statement with no rows did
#define CLI_DB_GRID_ROW_CELLS 0u
#define CLI_DB_GRID_ROW_WAIT 1u
#define CLI_DB_GRID_ROW_MORE 2u
#define CLI_DB_GRID_ROW_FAILURE 3u
#define CLI_DB_GRID_ROW_NOTICE 4u

// the rows a width is laid from, the most letters a column is laid at and the letters between columns; the rows a
// read carries at the least; the spare rows stored each side of the view; the rows a page is; the rest after which
// the rows stand still; and the share in hundredths a new reading moves a smoothed one
#define CLI_DB_GRID_WIDTH_ROWS 50u
#define CLI_DB_GRID_WIDTH_MOST 40u
#define CLI_DB_GRID_WIDTH_GAP 2u
#define CLI_DB_GRID_SPARE 40ULL
#define CLI_DB_GRID_PAGE 200ULL
#define CLI_DB_GRID_REST_MS 400ULL
#define CLI_DB_GRID_SHARE 40ULL

// A column: its label and its kind with their bytes, whether its values are numbers, its width in letters and its
// start in letters from the column the grid is panned to, below 0 for one panned off to the left
typedef struct
{
    const char *label;
    unsigned long long label_length;
    const char *kind;
    unsigned long long kind_length;
    int numeric;
    unsigned int width;
    long long start;
} CliDbGridColumn;

// A row: its kind and its text with its bytes, a failure's or a notice's line, or the word a waiting row stands as
typedef struct
{
    unsigned int kind;
    const char *text;
    unsigned long long length;
} CliDbGridRow;

// A cell: its text with its bytes, the word NULL for a null one, whether it is null and whether its column's values
// are numbers
typedef struct
{
    const char *text;
    unsigned long long length;
    int null;
    int numeric;
} CliDbGridCell;

// a run's answer adopted: `size` bytes of `answer`, the JSON cli_db_run writes, its rows read on from `open`, the
// connection `title`, or its `failure` where `answer` is NULL: 1, or 0 where the cells cannot meet it; the grid
// emptied, standing as a run on `title` that has not answered where `title` is not NULL, its cells released, and
// only where `open` is NULL or the run it carries is `open`'s; the grid's title with its bytes into `length_out`, and
// the result's sentence the same; how many columns, and the column `column_at` into `column`: 1, or 0 past the last;
// how many rows, the rows read and one more while the server has more, or the message lines; the row `at` into `row`;
// the cell of the row `at` at the column `column_at` into `cell`: 1, or 0 where the row is not stored or past the
// last; the first and last row the window shows, `first` and `last`, stored, no read and no take; how many rows
// stand stored and the first of them into `first_out`; and the open connection the rows are read from, NULL for none
int cli_db_grid_adopt(const CliDbGridCfg *args);
void cli_db_grid_clear(const CliDbGridCfg *args);
const char *cli_db_grid_title(const CliDbGridCfg *args);
const char *cli_db_grid_head_text(const CliDbGridCfg *args);
unsigned int cli_db_grid_columns(const CliDbGridCfg *args);
int cli_db_grid_column(const CliDbGridCfg *args);
unsigned long long cli_db_grid_rows(const CliDbGridCfg *args);
int cli_db_grid_row(const CliDbGridCfg *args);
int cli_db_grid_cell(const CliDbGridCfg *args);
void cli_db_grid_reach(const CliDbGridCfg *args);
unsigned long long cli_db_grid_stored(const CliDbGridCfg *args);
CliDbOpen *cli_db_grid_open(const CliDbGridCfg *args);
struct CliDbGridCfg
{
    const char *answer;
    unsigned long long size;
    const CliDbFailure *failure;
    CliDbOpen *open;
    const char *title;
    unsigned long long at;
    unsigned int column_at;
    CliDbGridColumn *column;
    CliDbGridRow *row;
    CliDbGridCell *cell;
    unsigned long long *length_out;
    unsigned long long first;
    unsigned long long last;
    unsigned long long *first_out;
};

typedef struct
{
    int (*adopt)(const CliDbGridCfg *args);
    void (*clear)(const CliDbGridCfg *args);
    const char *(*title)(const CliDbGridCfg *args);
    const char *(*head_text)(const CliDbGridCfg *args);
    unsigned int (*columns)(const CliDbGridCfg *args);
    int (*column)(const CliDbGridCfg *args);
    unsigned long long (*rows)(const CliDbGridCfg *args);
    int (*row)(const CliDbGridCfg *args);
    int (*cell)(const CliDbGridCfg *args);
    void (*reach)(const CliDbGridCfg *args);
    unsigned long long (*stored)(const CliDbGridCfg *args);
    CliDbOpen *(*open)(const CliDbGridCfg *args);
} CliDbGridNs;
EMBED_TABLE_LAYOUT(CliDbGridNs, adopt, clear, title, head_text, columns, column, rows, row, cell, reach, stored, open);
EMBED_TABLE_STORAGE CliDbGridNs cli_db_grid EMBED_UNUSED = {
    .adopt = cli_db_grid_adopt,
    .clear = cli_db_grid_clear,
    .title = cli_db_grid_title,
    .head_text = cli_db_grid_head_text,
    .columns = cli_db_grid_columns,
    .column = cli_db_grid_column,
    .rows = cli_db_grid_rows,
    .row = cli_db_grid_row,
    .cell = cli_db_grid_cell,
    .reach = cli_db_grid_reach,
    .stored = cli_db_grid_stored,
    .open = cli_db_grid_open,
};

// the step at `now_ms`: the rows the view reached and the stretch lacks read in one read and the rows outside the
// stretch let go: 1 where the rows changed; the grid panned `columns` whole columns, inside its columns: 1 where it
// moved; and the key `name` pressed: Left and Right pan one column, Home to the first and End to the last: 1 where it
// was taken; and when the step next reads: `now_ms` while the view reaches rows the stretch lacks and the last read
// was not refused, else 0
int cli_db_grid_read_ahead(const CliDbGridStepCfg *args);
int cli_db_grid_pan(const CliDbGridStepCfg *args);
int cli_db_grid_key(const CliDbGridStepCfg *args);
unsigned long long cli_db_grid_deadline_ms(const CliDbGridStepCfg *args);
struct CliDbGridStepCfg
{
    unsigned long long now_ms;
    int columns;
    const char *name;
};

typedef struct
{
    int (*step)(const CliDbGridStepCfg *args);
    int (*pan)(const CliDbGridStepCfg *args);
    int (*key)(const CliDbGridStepCfg *args);
    unsigned long long (*deadline_ms)(const CliDbGridStepCfg *args);
} CliDbGridStepNs;
EMBED_TABLE_LAYOUT(CliDbGridStepNs, step, pan, key, deadline_ms);
EMBED_TABLE_STORAGE CliDbGridStepNs cli_db_grid_step EMBED_UNUSED = {
    .step = cli_db_grid_read_ahead,
    .pan = cli_db_grid_pan,
    .key = cli_db_grid_key,
    .deadline_ms = cli_db_grid_deadline_ms,
};

#endif
