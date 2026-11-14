// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// references.h: the References tool's side pane table, the references Find Usages found as the CLI lists them
// (code/references.h): under the pane's head the report, the references and files counted with the name or that none
// were found; then a row a file with its mark, name, folder and count of references, and under a file not closed a row
// a reference with its line and column at the right and its line's text, the name in signal. A file's row pressed
// closes or opens its references; a reference's row opens the file at the name (references.c)
#ifndef GUI_WINDOW_REFERENCES_H
#define GUI_WINDOW_REFERENCES_H
#include "pane.h"

// the sizes in the page's pixels: the report row, a count pill's padding, the text sizes of the report, a place and a
// line's text, and the bytes of a line displayed before the name where the name is past the lead
#define GUI_REFERENCES_REPORT 20
#define GUI_REFERENCES_COUNT_PAD 6
#define GUI_REFERENCES_REPORT_SIZE 12u
#define GUI_REFERENCES_PLACE_SIZE 11u
#define GUI_REFERENCES_TEXT_SIZE 12u
#define GUI_REFERENCES_LEAD 30ULL
#define GUI_REFERENCES_LEAD_PAST 40ULL

// the References tool's pane table
extern const GuiPaneTable gui_references_table;

#endif
