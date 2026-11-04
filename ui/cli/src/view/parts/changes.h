// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.h: the Commit tool's kind of side pane, the Changes: a row each file that differs from the last commit, read
// where the git module lists them (code/git.h), the file's mark, its name in its state's color, its folder quieter
// after it and its state's letter at the right; a row pressed opens the file (changes.c)
#ifndef GUI_PARTS_CHANGES_H
#define GUI_PARTS_CHANGES_H
#include "pane.h"

// the sizes in the page's pixels: the icon's box, the gap after it, the folder's text size and the state letter's
#define GUI_CHANGES_ICON 16
#define GUI_CHANGES_GAP 8
#define GUI_CHANGES_WHERE_SIZE 11u
#define GUI_CHANGES_STATE_SIZE 11u

// the Commit tool's kind of pane
extern const GuiPaneKind gui_changes_kind;

#endif
