// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.h: the Git tool's kind of side pane, the graph as the Git pane lists it (code/git.h): under the pane's head the
// choice of repository where the tree has more than one, the next chosen by a press, and a field the query is typed
// into, which searches the whole history. Then the branch's row with its name and the commits counted, a row a commit
// with its columns' strokes and its dot, the branches and tags at it in pills, HEAD's in signal and a
// tag's in the third mark, its subject, its author where authors show and its day, and under a commit opened a row each
// file it changed with its mark, name, folder and state letter, or the notice that none under the tree did. A press on
// the field takes the keys, which a press elsewhere or Escape lets go; a commit's row pressed opens or closes its
// files; a file's row opens the file (git.c)
#ifndef GUI_PARTS_GIT_H
#define GUI_PARTS_GIT_H
#include "pane.h"

// the sizes in the page's pixels: a column of the graph, the commit row's left padding and the gap between its parts,
// the row's right padding, the branch row, its padding and the gap between its parts, a name's padding, its least and
// most widths, its height and its corners' radius, the author's most width, the notice's padding, the text sizes of a
// name, of the author and the day, and of the notice; the strokes' width, the dot's radius and a merge's dot's, in the
// page's pixels; and the segments a stroke that changes column is drawn in
#define GUI_GIT_LANE 12
#define GUI_GIT_PAD 5
#define GUI_GIT_RIGHT_PAD 10
#define GUI_GIT_BRANCH_ROW 24
#define GUI_GIT_BRANCH_PAD 10
#define GUI_GIT_BRANCH_GAP 6
#define GUI_GIT_REF_PAD 5
#define GUI_GIT_REF_LEAST 48
#define GUI_GIT_REF_MOST 144
#define GUI_GIT_REF_HEIGHT 16
#define GUI_GIT_REF_RADIUS 4
#define GUI_GIT_AUTHOR_MOST 112
#define GUI_GIT_NONE_PAD 32
#define GUI_GIT_REF_SIZE 11u
#define GUI_GIT_SMALL_SIZE 11u
#define GUI_GIT_NONE_SIZE 12u
#define GUI_GIT_STROKE_F32 1.5f
#define GUI_GIT_DOT_F32 3.5f
#define GUI_GIT_MERGE_DOT_F32 3.0f
#define GUI_GIT_CURVE_STEPS 6u

// the Git tool's kind of pane
extern const GuiPaneKind gui_git_kind;

#endif
