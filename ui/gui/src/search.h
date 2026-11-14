// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// search.h: the Search tool's side pane table, Find in Files as the search pane lists it (editor/files.h): under the
// pane's head a field the query is typed into, with the case, whole word and regular expression choices at its right,
// each lit in signal while on; under the field the set of files searched in, there while Preferences lists a set, the
// next chosen by a press; and under that the report, the results and files counted or that none were found. Then a
// row a file with its mark, name, folder and count of lines, and under a file not closed a row a line with the match
// in signal, the line cut to start a little before the match. A press on the field takes the keys, which a press
// elsewhere or Escape lets go; a file's row pressed closes or opens its lines; a line's row opens the file at the
// match (search.c)
#ifndef GUI_WINDOW_SEARCH_H
#define GUI_WINDOW_SEARCH_H
#include "pane.h"

// the sizes in the page's pixels: a choice's padding across, the gap between choices and the radius of a choice's
// corners, the report row, a count pill's padding, the text sizes of the choices, the report and a line found, and the
// bytes of a line shown before its match where the match stands past the lead
#define GUI_SEARCH_OPTION_PAD 5
#define GUI_SEARCH_OPTION_GAP 2
#define GUI_SEARCH_OPTION_RADIUS 2
#define GUI_SEARCH_REPORT 20
#define GUI_SEARCH_COUNT_PAD 6
#define GUI_SEARCH_OPTION_SIZE 11u
#define GUI_SEARCH_REPORT_SIZE 12u
#define GUI_SEARCH_HIT_SIZE 12u
#define GUI_SEARCH_LEAD 30ULL
#define GUI_SEARCH_LEAD_PAST 40ULL

// the Search tool's pane table
extern const GuiPaneTable gui_search_table;

#endif
