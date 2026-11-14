// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_view_internal.h: the open view the two sources of diff_view share (diff_view.c, diff_view_open.c)
#ifndef DIFF_VIEW_INTERNAL_H
#define DIFF_VIEW_INTERNAL_H
#include "../editor/document.h"
#include "diff_view.h"

// no line on a row's side, and the max bytes of the where sentence with its end
#define CLI_DIFF_VIEW_NONE 0xffffffffu
#define CLI_DIFF_VIEW_WHERE_MAX 48u

// A row as the view positions it: the left line's number and the right's, CLI_DIFF_VIEW_NONE where the side has none, what
// it is, and how many right lines are above it, which rises with the rows, for a binary search by the right side
typedef struct
{
    unsigned int left;
    unsigned int right;
    unsigned int type;
    unsigned int right_at;
} CliDiffViewLaid;

// the max bytes of the bar's label, of the sides sentence and of the sentence for two texts that do not differ, each
// with its end
#define CLI_DIFF_VIEW_LABEL_MAX ((2u * CLI_PATH_MAX_LEN) + 8u)
#define CLI_DIFF_VIEW_SIDES_MAX ((2u * CLI_PATH_MAX_LEN) + (2u * CLI_NAME_MAX_LEN) + 64u)
#define CLI_DIFF_VIEW_SAME_MAX 128u

// One side's lines as the view retains them: the places of its starts, its lengths and its units in the lines cell, how
// many lines and how many units
typedef struct
{
    unsigned long long start_at;
    unsigned long long length_at;
    unsigned long long unit_at;
    unsigned int lines;
    unsigned long long used;
} CliDiffViewSide;

// The open view. The source, the tree, the paths and the commits in the view's own bytes, each with its end, with
// whether the second path and the commits were given; the clipboard's text in a movable cell of its bytes; the bar's
// label, the sides sentence and the sentence for two texts that do not differ in the view's own bytes. The two texts'
// lines in one movable cell, each side's starts and lengths then its units, read by copy. The rows in one movable cell,
// with the changes' first rows after them, sorted, read by copy. Beside the cells: how many rows and changes, the
// change the view is at, whether a view is open, whether the two texts were too far apart, the two options, and
// the where sentence with its bytes
typedef struct
{
    unsigned int source;
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char other_path[CLI_PATH_MAX_LEN];
    int has_other_path;
    char id[CLI_NAME_MAX_LEN];
    int has_id;
    char other_id[CLI_NAME_MAX_LEN];
    int has_other_id;
    const CarcerMovableEntry *text_cell;
    unsigned long long size;
    unsigned long long at_ms;
    char label[CLI_DIFF_VIEW_LABEL_MAX];
    unsigned long long label_length;
    char sides[CLI_DIFF_VIEW_SIDES_MAX];
    unsigned long long sides_length;
    char same[CLI_DIFF_VIEW_SAME_MAX];
    unsigned long long same_length;
    const CarcerMovableEntry *lines_cell;
    CliDiffViewSide left;
    CliDiffViewSide right;
    const CarcerMovableEntry *rows_cell;
    unsigned long long change_row_at;
    unsigned int rows;
    unsigned int changes;
    unsigned int change_at;
    int open;
    int refused;
    int space;
    int structure;
    char where[CLI_DIFF_VIEW_WHERE_MAX];
    unsigned long long where_length;
} CliDiffView;

// the place of the row `at_`'s record in the rows cell
#define CLI_DIFF_VIEW_ROW_AT(at_) ((unsigned long long)(at_) * sizeof(CliDiffViewLaid))

extern CliDiffView g_cli_diff_view;

// the where sentence written for the change the view is at, empty while none differ (diff_view.c)
void cli_diff_view_where_written(const CliDiffViewAtRowLengthCfg *args);

#endif
