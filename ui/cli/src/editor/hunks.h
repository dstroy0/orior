// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hunks.h: the changes of the view shown against the file as the last commit has it, as the gutter marks them and the
// change peek shows them. Each change stands as four numbers, the newer text's first line and the line after its last,
// then the older text's, read from the document store's changes when the view's text changes; the gutter's mark of a
// line is a binary search over the changes' first lines; the change shown is one of them, its lines the older text's
// then the newer's, its sentence `Change i of n`, stepped through with wrapping, and its revert the edit that puts the
// older lines back; a scroll within CLI_HUNKS_SETTLE_MS of a change shown leaves it standing (hunks.c)
#ifndef HUNKS_H
#define HUNKS_H
#include "document.h"
#include "embed_dispatch_layout.h"

typedef struct CliHunksCfg CliHunksCfg;

// no change; a line's mark: none, lines put in, lines changed, and lines taken out after it; the milliseconds a change
// shown stands against a scroll; and the most bytes of the sentence over a change shown
#define CLI_HUNKS_NONE 0xffffffffu
#define CLI_HUNKS_MARK_NONE 0u
#define CLI_HUNKS_MARK_ADDED 1u
#define CLI_HUNKS_MARK_CHANGED 2u
#define CLI_HUNKS_MARK_REMOVED 3u
#define CLI_HUNKS_SETTLE_MS 300ULL
#define CLI_HUNKS_TITLE 64u

// A line of the change shown: 1 in `removed` for a line of the older text, else a line of the newer, and its `count`
// units where they stand in the document store, until the text is next edited
typedef struct
{
    int removed;
    const unsigned short *units;
    unsigned long long count;
} CliHunkRow;

// the changes read anew of the text stored under `key`, the file at `path` of the tree at `root`: the file as the last
// commit has it is `size` bytes of `text` where `text` is not NULL, else read from git once for each key, none where
// git has no such file; none where `key` is NULL; the change shown closed where the key is another or the change is
// gone. How many changes stand; and the change the line `line` stands in, or for lines taken out the line above them,
// its mark into `mark_out` where it is not NULL: its place, or CLI_HUNKS_NONE with CLI_HUNKS_MARK_NONE
void cli_hunks_read(const CliHunksCfg *args);
unsigned int cli_hunks_count(const CliHunksCfg *args);
unsigned int cli_hunks_at_line(const CliHunksCfg *args);
// the change the line `line` stands in shown: 1, or 0 where it stands in none; the change `by` changes on from the one
// shown, or from the line `line` where none is, shown, the last wrapping to the first and the first to the last, its
// first line into `line_out`: 1, or 0 where no change stands; and the change shown closed
int cli_hunks_show(const CliHunksCfg *args);
int cli_hunks_step(const CliHunksCfg *args);
void cli_hunks_close(const CliHunksCfg *args);
// the edit that puts the older lines of the change shown back into `edit_out`, its text in a cell of the changes' own
// standing until the next revert, 1 into `edit_count`: 1, or 0 with why written into `err_msg` where no change is
// shown or the cells cannot meet it
int cli_hunks_revert(const CliHunksCfg *args);
// the key `name` while a change is shown: Escape closes it: 1 where taken
int cli_hunks_key(const CliHunksCfg *args);
// the sentence over the change shown, its bytes into `length_out`, empty where none is; and whether a scroll moves the
// view: 1 where no change is shown or CLI_HUNKS_SETTLE_MS have passed since it was shown, else 0
const char *cli_hunks_title(const CliHunksCfg *args);
int cli_hunks_settled(const CliHunksCfg *args);
// how many lines the change shown has, its older lines then its newer, 0 where none is shown, the line the peek stands
// under into `line_out` where it is not NULL: its last newer line, or the line above lines taken out; and the line
// `at` of it into `row`: 1, or 0 past the last
unsigned int cli_hunks_row_count(const CliHunksCfg *args);
int cli_hunks_row(const CliHunksCfg *args);
struct CliHunksCfg
{
    const char *key;
    const char *path;
    const char *root;
    const char *text;
    unsigned long long size;
    unsigned long long line;
    int by;
    unsigned int at;
    CliHunkRow *row;
    const char *name;
    unsigned long long *length_out;
    unsigned long long *line_out;
    unsigned int *mark_out;
    CliEdit *edit_out;
    unsigned int *edit_count;
    VerbaLinea *err_msg;
};

typedef struct
{
    void (*read)(const CliHunksCfg *args);
    unsigned int (*count)(const CliHunksCfg *args);
    unsigned int (*at_line)(const CliHunksCfg *args);
    int (*show)(const CliHunksCfg *args);
    int (*step)(const CliHunksCfg *args);
    int (*revert)(const CliHunksCfg *args);
    void (*close)(const CliHunksCfg *args);
    int (*key)(const CliHunksCfg *args);
    const char *(*title)(const CliHunksCfg *args);
    int (*settled)(const CliHunksCfg *args);
    unsigned int (*row_count)(const CliHunksCfg *args);
    int (*row)(const CliHunksCfg *args);
} CliHunksNs;
EMBED_TABLE_LAYOUT(CliHunksNs, read, count, at_line, show, step, revert, close, key, title, settled, row_count, row);
EMBED_TABLE_STORAGE CliHunksNs cli_hunks EMBED_UNUSED = {
    .read = cli_hunks_read,
    .count = cli_hunks_count,
    .at_line = cli_hunks_at_line,
    .show = cli_hunks_show,
    .step = cli_hunks_step,
    .revert = cli_hunks_revert,
    .close = cli_hunks_close,
    .key = cli_hunks_key,
    .title = cli_hunks_title,
    .settled = cli_hunks_settled,
    .row_count = cli_hunks_row_count,
    .row = cli_hunks_row,
};

#endif
