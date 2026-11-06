// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_view_internal.h: the open view the two sources of diff_view share (diff_view.c, diff_view_open.c)
#ifndef DIFF_VIEW_INTERNAL_H
#define DIFF_VIEW_INTERNAL_H
#include "../editor/document.h"
#include "diff_view.h"

// no line on a row's side, and the most bytes of the where sentence with its end
#define CLI_DIFF_VIEW_NONE 0xffffffffu
#define CLI_DIFF_VIEW_WHERE_MAX 48u

// A row as the view lays it: the left line's number and the right's, CLI_DIFF_VIEW_NONE where the side has none, what
// it is, and how many right lines stand above it, which rises with the rows, for a binary search by the right side
typedef struct
{
    unsigned int left;
    unsigned int right;
    unsigned int kind;
    unsigned int right_at;
} CliDiffViewLaid;

// The open view. The sources in one cell: the source, the tree, the paths, the commits and the clipboard's text, and
// the bar's label, the sides sentence and the sentence for two texts that do not differ, each ended by a 0. The two
// texts' lines in one cell, each side's starts and lengths then its units. The rows in one cell, with the changes'
// first rows after them, sorted. Beside the cells: how many rows and changes, the change the view stands at, whether a
// view is open, whether the two texts were too far apart, the two options, and the where sentence with its bytes
typedef struct
{
    unsigned char *sources;
    unsigned int source;
    const char *root;
    const char *path;
    const char *other_path;
    const char *id;
    const char *other_id;
    const char *text;
    unsigned long long size;
    unsigned long long at_ms;
    const char *label;
    unsigned long long label_length;
    const char *sides;
    unsigned long long sides_length;
    const char *same;
    unsigned long long same_length;
    unsigned char *lines_cell;
    CliLines left;
    CliLines right;
    unsigned char *rows_cell;
    CliDiffViewLaid *row;
    unsigned int rows;
    unsigned int *change_row;
    unsigned int changes;
    unsigned int change_at;
    int open;
    int refused;
    int space;
    int structure;
    char where[CLI_DIFF_VIEW_WHERE_MAX];
    unsigned long long where_length;
} CliDiffView;

extern CliDiffView g_cli_diff_view;

// the where sentence written for the change the view stands at, empty while none differ (diff_view.c)
void cli_diff_view_where_written(const CliDiffViewAtRowLengthCfg *args);

#endif
