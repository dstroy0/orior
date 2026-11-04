// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tests.h: the Tests tool's kind of side pane, the tree's tests as the tests pane lists them (tests/testing.h): under
// the pane's head a bar of actions, Run All, Run Failed, Coverage and Stop with the Parallel choice at its right, and
// under the bar the run's summary while one goes or after one ended; then a row a file with the worst of its tests'
// outcomes as its mark, its icon, its name and folder and the counts of its outcomes, under a file not closed a row
// each test with its outcome's mark, its label and its time, and under a failed test its message's first line; and
// after the files the coverage of the last run that recorded it, a head and a row a file with its share. A file's row
// pressed closes or opens its tests, a test's opens the file where it failed or where it is written, and a coverage
// row's opens the file at its first line missed (tests.c)
#ifndef GUI_PARTS_TESTS_H
#define GUI_PARTS_TESTS_H
#include "pane.h"

// the sizes in the page's pixels: the bar's height, an action's padding across and its text size, the gap between
// actions, the Parallel box's side, the summary row's height, the mark's side and the gap after it, the file's icon
// box, the folder's and a count's text sizes, a message's padding at the left and its size, and the coverage bar's
// width, height and corner radius
#define GUI_TESTS_BAR 30
#define GUI_TESTS_ACTION_PAD 8
#define GUI_TESTS_ACTION_SIZE 12u
#define GUI_TESTS_ACTION_GAP 5
#define GUI_TESTS_BOX 12
#define GUI_TESTS_SAID 22
#define GUI_TESTS_MARK 8
#define GUI_TESTS_GAP 8
#define GUI_TESTS_ICON 16
#define GUI_TESTS_WHERE_SIZE 11u
#define GUI_TESTS_COUNT_SIZE 11u
#define GUI_TESTS_MESSAGE_PAD 42
#define GUI_TESTS_MESSAGE_SIZE 11u
#define GUI_TESTS_COVER_WIDTH 56
#define GUI_TESTS_COVER_HEIGHT 5
#define GUI_TESTS_COVER_RADIUS 3

// the Tests tool's kind of pane
extern const GuiPaneKind gui_tests_kind;

#endif
