// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug.h: the Debug tool's pane table, a foot table, the Debug pane in the panel at the work's foot, every word and
// count of it the debug module's (debug/debug_pane.h). Its head: the bar, a box each step the module shows, drawn
// quieter where the step cannot act, its icon where the icons carry one and else its label, and the status line after
// them, in the first mark's color where it reports a failure; the views' choices, the one shown lit; and the field of
// the view shown, which takes the keys. Its rows are the view shown's: a frame with its function and its file and line
// at the right, the chosen frame marked; a scope or a variable at its depth with its arrow where it opens, its name,
// its value and its type; a watch with its box, its expression and its value; and a console line in its type's color.
// A press on a step runs it, on a choice shows its view, on a frame chooses it and opens its file, on an arrow opens
// or closes the row, and on a watch's box takes the watch out (debug.c)
#ifndef GUI_PARTS_DEBUG_H
#define GUI_PARTS_DEBUG_H
#include "pane.h"

// the head's actions: the step `at` as GUI_DEBUG_STEP_FIRST and `at` after it, the view `view` as GUI_DEBUG_VIEW_FIRST
// and `view` after it, and the field
#define GUI_DEBUG_STEP_FIRST 1u
#define GUI_DEBUG_VIEW_FIRST 16u
#define GUI_DEBUG_FIELD 32u

// the sizes in the page's pixels: a step's box across and the gap after it, its icon's side, a view's choice across,
// the gap before a row's second text and before its type at the right, a watch's box, and the text sizes of the
// status line and of a row
#define GUI_DEBUG_STEP 84
#define GUI_DEBUG_STEP_GAP 4
#define GUI_DEBUG_STEP_ICON 14
#define GUI_DEBUG_VIEW 96
#define GUI_DEBUG_GAP 12
#define GUI_DEBUG_BOX 11
#define GUI_DEBUG_STATUS_SIZE 12u
#define GUI_DEBUG_ROW_SIZE 12u

// the Debug tool's pane table
extern const GuiPaneTable gui_debug_table;

#endif
