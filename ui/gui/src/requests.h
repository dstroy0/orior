// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests.h: the Requests view of the Comms pane, the table the Comms table hands its head, rows and presses to
// while the Requests view is shown. Its head is two rows of choices the CLI answers (requests/requests.h): Send and
// Copy as curl, then the environments, the one chosen lit. Its rows are the CLI's: the status in the tone of its
// class, the times, the groups with their arrows, the headers and what was sent in the code face one guide in, the
// body in the code face, the rows that read why the body is not drawn quieter, each answer with its box, and a
// comparison's lines each with its sign. A press on a group opens or closes it, on an answer's box checks it, on an
// answer shows it, on Compare compares the two checked (requests.c)
#ifndef GUI_WINDOW_REQUESTS_H
#define GUI_WINDOW_REQUESTS_H
#include "pane.h"

// the sizes in the page's pixels: an answer's box and the gap after it, and a row's text size
#define GUI_REQUESTS_BOX 11
#define GUI_REQUESTS_BOX_GAP 6
#define GUI_REQUESTS_SIZE 12u

// the Requests view's table, reached through the Comms table and in no tool's place of its own
extern const GuiPaneTable gui_requests_view;

#endif
