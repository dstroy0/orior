// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.h: the Structure tool's kind of side pane, the Outline: a row each declaration of the file shown, read
// where the symbols' outline stands (code/symbols.h), the kind's mark before each row's name and a guide down each
// level it stands under; the row of the declaration the cursor stands in is the current one, and a row pressed puts the
// cursor on its line (structure.c)
#ifndef GUI_PARTS_STRUCTURE_H
#define GUI_PARTS_STRUCTURE_H
#include "pane.h"

// the sizes in the page's pixels: the mark's box, the gap after it and the mark's text size
#define GUI_STRUCTURE_MARK 16
#define GUI_STRUCTURE_GAP 6
#define GUI_STRUCTURE_MARK_SIZE 11u

// the Structure tool's kind of pane
extern const GuiPaneKind gui_structure_kind;

#endif
