// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diffview.h: a file's two texts side by side over the editor, the work host's view of the CLI's compare
// (code/diff_view.h): a bar at its top with the file's label, the sides sentence and the where sentence, the box that
// sets white space aside and the box that compares by structure, each drawn pressed as the CLI answers it and quieter
// where the CLI answers it cannot be pressed, and the next change, the change before and close tools; under the bar
// the summary where the CLI answers one; and the body, a row each row of the CLI's in two halves, the older text's
// line on the left and the newer's on the right, each a number cell and a text cell, a half tinted by what its row is,
// a side with no line a gap; or the CLI's sentence in place of rows. A press on a tool steps or closes, on a box turns
// its option; the wheel scrolls the rows; F7 and Shift+F7 step; Escape closes through the host (diffview.c)
#ifndef GUI_WINDOW_DIFFVIEW_H
#define GUI_WINDOW_DIFFVIEW_H
#include "../../../includes/window/work.h"

// the sizes in the page's pixels: the bar's height, the summary's, a row's, a number cell's width, the padding, a
// tool's side and its icon's, a box's side, the gap after it and the width a box takes with its words, and the text
// sizes of the bar, the summary, a line and a line's number
#define GUI_DIFFVIEW_BAR 30
#define GUI_DIFFVIEW_SUMMARY 22
#define GUI_DIFFVIEW_ROW 20
#define GUI_DIFFVIEW_NUMBER 44
#define GUI_DIFFVIEW_PAD 8
#define GUI_DIFFVIEW_TOOL 22
#define GUI_DIFFVIEW_TOOL_ICON 14
#define GUI_DIFFVIEW_BOX 11
#define GUI_DIFFVIEW_BOX_GAP 6
#define GUI_DIFFVIEW_BOX_WIDTH 170
#define GUI_DIFFVIEW_BAR_SIZE 12u
#define GUI_DIFFVIEW_SUMMARY_SIZE 12u
#define GUI_DIFFVIEW_LINE_SIZE 12u
#define GUI_DIFFVIEW_NUMBER_SIZE 11u
// the max units of a line a half shows, with the bytes they may take as UTF-8 and the end; and the alpha a half's
// tint is positioned at
#define GUI_DIFFVIEW_LINE_UNITS 256ULL
#define GUI_DIFFVIEW_LINE_BYTES 769u
#define GUI_DIFFVIEW_TINT 0x38u

// what a press is on: the next change, the change before, close, the white space box, the structure box, and from
// GUI_DIFFVIEW_ROWS on, the row its index less GUI_DIFFVIEW_ROWS
#define GUI_DIFFVIEW_NEXT 1u
#define GUI_DIFFVIEW_PREVIOUS 2u
#define GUI_DIFFVIEW_CLOSE 3u
#define GUI_DIFFVIEW_SPACE 4u
#define GUI_DIFFVIEW_STRUCTURE 5u
#define GUI_DIFFVIEW_ROWS 16u

// the compare's table of the work host
extern const GuiWorkTable gui_diffview_table;

#endif
