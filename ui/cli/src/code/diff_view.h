// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_view.h: two texts of a file side by side, as the window draws them over the editor: the older on the left and
// the newer on the right, each line with its number, the lines the two share level with each other, a line changed
// standing on both sides, a line put in on the right across from a gap on the left and a line taken out the other way.
// The two texts come from one of the sources: the shown view's text against the file as the last commit has it, a
// commit's text against the file as it stands, two commits' texts the older first, a commit's first parent's text
// against the commit's, a text from the clipboard against the file, two files of the tree, and a Local History
// snapshot against the file. The option set from the settings `diff-space` sets white space aside, each run of it one
// space and none at a line's ends, as the lines are compared; the option `diff-structure` is not answered enabled.
// The rows stand in one cell, each the two lines' numbers into the two texts' lines, which stand in one cell of units
// with their starts; the changes' first rows stand sorted, and a step is a binary search over them. Every sentence
// the bar and the body show is written here (diff_view.c, diff_view_open.c)
#ifndef DIFF_VIEW_H
#define DIFF_VIEW_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../machine/state.h"
#include "embed_dispatch_layout.h"
#include "structure.h"

typedef struct CliDiffViewSourceErrMsgCfg CliDiffViewSourceErrMsgCfg;
typedef struct CliDiffViewOptionOnCfg CliDiffViewOptionOnCfg;
typedef struct CliDiffViewAtRowLengthCfg CliDiffViewAtRowLengthCfg;

// the sources the two texts come from
#define CLI_DIFF_VIEW_HEAD 0u
#define CLI_DIFF_VIEW_COMMIT 1u
#define CLI_DIFF_VIEW_COMMITS 2u
#define CLI_DIFF_VIEW_PARENT 3u
#define CLI_DIFF_VIEW_CLIPBOARD 4u
#define CLI_DIFF_VIEW_FILES 5u
#define CLI_DIFF_VIEW_SNAPSHOT 6u

// the two options: white space set aside, and the comparison by structure
#define CLI_DIFF_VIEW_SPACE 0u
#define CLI_DIFF_VIEW_STRUCTURE 1u

// the bar's texts: the file's label, the sentence that sets the two sides apart, and each option's box's words
#define CLI_DIFF_VIEW_BAR_LABEL 0u
#define CLI_DIFF_VIEW_BAR_SIDES 1u
#define CLI_DIFF_VIEW_BAR_SPACE 2u
#define CLI_DIFF_VIEW_BAR_STRUCTURE 3u

// what a row is: the same on both sides and changed, from structure.h's list, and a line put in on the right alone
// and one taken out of the left alone, the two after the last of that list
#define CLI_DIFF_VIEW_ADDED ((unsigned int)CLI_RENAMED + 1u)
#define CLI_DIFF_VIEW_REMOVED ((unsigned int)CLI_RENAMED + 2u)

// A row: what it is; on each side whether a line stands there, the line's number from 0, its units where they stand
// in the view's cell and how many; the first of its token marks and how many, and how many review comments stand under
// it, each none in this view
typedef struct
{
    unsigned int type;
    int has_left;
    unsigned int left_line;
    const unsigned short *left_units;
    unsigned long long left_count;
    int has_right;
    unsigned int right_line;
    const unsigned short *right_units;
    unsigned long long right_count;
    unsigned int marks_first;
    unsigned int marks_count;
    unsigned int comments;
} CliDiffViewRow;

// A token mark: the side it stands on, 1 for the right, its first unit and the unit after its last, and what it is
typedef struct
{
    int right;
    unsigned int from;
    unsigned int to;
    unsigned int type;
} CliDiffViewMark;

// the view opened from `source` for the tree at `root`: the file at `path`, or the shown view's file where `path` is
// NULL, its text the open view's where one is open on it, else the file's own; `other_path` the second file; `id` a
// commit and `other_id` a second; `size` bytes of `text` the clipboard's; and `at_ms` the snapshot's milliseconds: 1,
// or 0 with why in `err_msg`, the view open before closed either way; and the view closed, its cells released
int cli_diff_view_open(const CliDiffViewSourceErrMsgCfg *args);
void cli_diff_view_close(const CliDiffViewSourceErrMsgCfg *args);
struct CliDiffViewSourceErrMsgCfg
{
    const char *root;
    unsigned int source;
    const char *path;
    const char *other_path;
    const char *id;
    const char *other_id;
    unsigned long long at_ms;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*open)(const CliDiffViewSourceErrMsgCfg *args);
    void (*close)(const CliDiffViewSourceErrMsgCfg *args);
} CliDiffViewSourceErrMsgNs;
EMBED_TABLE_LAYOUT(CliDiffViewSourceErrMsgNs, open, close);
EMBED_TABLE_STORAGE CliDiffViewSourceErrMsgNs cli_diff_view_source_err_msg EMBED_UNUSED = {
    .open = cli_diff_view_open,
    .close = cli_diff_view_close,
};

// the options read from the settings `entries`, white space set aside where `diff-space` reads `aside`; whether
// `option` is on into `on_out` and whether its box can be pressed into `enabled_out`, white space's while structure is
// off and structure's never in this view; and `option` turned, where its box can be pressed, its setting saved in
// orior's folder and the open view laid again from the same sources: the row that stands where the row `at` stood
void cli_diff_view_option_begin(const CliDiffViewOptionOnCfg *args);
void cli_diff_view_option_get(const CliDiffViewOptionOnCfg *args);
unsigned int cli_diff_view_option_toggle(const CliDiffViewOptionOnCfg *args);
struct CliDiffViewOptionOnCfg
{
    const CliStateEntries *entries;
    unsigned int option;
    unsigned int at;
    int *on_out;
    int *enabled_out;
};

typedef struct
{
    void (*begin)(const CliDiffViewOptionOnCfg *args);
    void (*option_get)(const CliDiffViewOptionOnCfg *args);
    unsigned int (*option_toggle)(const CliDiffViewOptionOnCfg *args);
} CliDiffViewOptionOnNs;
EMBED_TABLE_LAYOUT(CliDiffViewOptionOnNs, begin, option_get, option_toggle);
EMBED_TABLE_STORAGE CliDiffViewOptionOnNs cli_diff_view_option_on EMBED_UNUSED = {
    .begin = cli_diff_view_option_begin,
    .option_get = cli_diff_view_option_get,
    .option_toggle = cli_diff_view_option_toggle,
};

// how many rows the open view has, none while it has no change; the row `at` into `row`: 1, or 0 past the last; the
// token marks of the row `at` into `mark`, `marks_max` at most: how many, none in this view; the change stepped to in
// `direction`, 1 the next and -1 the one before, each wrapping at the ends, or for 0 the change the row `at` stands in
// or after, found by a binary search over the changes' first rows: the change's first row, the where sentence
// following; and the sentences, each with its bytes into `length_out`: where the view stands among the changes,
// `change N of M`, empty while none differ; the bar's text `part`; the summary of a comparison by structure, empty in
// this view; and the sentence the body shows in place of rows, empty while rows differ
unsigned int cli_diff_view_rows(const CliDiffViewAtRowLengthCfg *args);
int cli_diff_view_row(const CliDiffViewAtRowLengthCfg *args);
unsigned int cli_diff_view_marks(const CliDiffViewAtRowLengthCfg *args);
unsigned int cli_diff_view_step(const CliDiffViewAtRowLengthCfg *args);
const char *cli_diff_view_where(const CliDiffViewAtRowLengthCfg *args);
const char *cli_diff_view_bar(const CliDiffViewAtRowLengthCfg *args);
const char *cli_diff_view_summary(const CliDiffViewAtRowLengthCfg *args);
const char *cli_diff_view_empty(const CliDiffViewAtRowLengthCfg *args);
struct CliDiffViewAtRowLengthCfg
{
    unsigned int at;
    int direction;
    unsigned int part;
    CliDiffViewRow *row;
    CliDiffViewMark *mark;
    unsigned int marks_max;
    unsigned long long *length_out;
};

typedef struct
{
    unsigned int (*rows)(const CliDiffViewAtRowLengthCfg *args);
    int (*row)(const CliDiffViewAtRowLengthCfg *args);
    unsigned int (*marks)(const CliDiffViewAtRowLengthCfg *args);
    unsigned int (*step)(const CliDiffViewAtRowLengthCfg *args);
    const char *(*where)(const CliDiffViewAtRowLengthCfg *args);
    const char *(*bar)(const CliDiffViewAtRowLengthCfg *args);
    const char *(*summary)(const CliDiffViewAtRowLengthCfg *args);
    const char *(*empty)(const CliDiffViewAtRowLengthCfg *args);
} CliDiffViewAtRowLengthNs;
EMBED_TABLE_LAYOUT(CliDiffViewAtRowLengthNs, rows, row, marks, step, where, bar, summary, empty);
EMBED_TABLE_STORAGE CliDiffViewAtRowLengthNs cli_diff_view_at_row_length EMBED_UNUSED = {
    .rows = cli_diff_view_rows,
    .row = cli_diff_view_row,
    .marks = cli_diff_view_marks,
    .step = cli_diff_view_step,
    .where = cli_diff_view_where,
    .bar = cli_diff_view_bar,
    .summary = cli_diff_view_summary,
    .empty = cli_diff_view_empty,
};

#endif
