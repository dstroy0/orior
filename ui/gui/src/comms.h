// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.h: the Comms tool's pane table, which is in the panel at the work's foot. Its head is a row of four
// choices, Channels, Requests, Network and Report, the view displayed lit; under it the Channels view positions the comms
// layer's count of parts and channels and Read Again, and a row each group, part, channel and message the CLI lists
// (comms/comms.h), a part's or a channel's with its dot, in signal while it refused nothing and in the error's color
// while it did, its name, its status quieter and its detail quieter still. While another view is displayed the head's
// second part, the rows, their drawing and their presses are that view's table's (comms.c)
#ifndef GUI_WINDOW_COMMS_H
#define GUI_WINDOW_COMMS_H
#include "pane.h"

// the sizes in the page's pixels: the dot's side and the gap after it, the width of Read Again, and the gap between a
// row's name, status and detail
#define GUI_COMMS_DOT 7
#define GUI_COMMS_DOT_GAP 6
#define GUI_COMMS_AGAIN 96
#define GUI_COMMS_GAP 10

// the Comms tool's pane table
extern const GuiPaneTable gui_comms_table;

#endif
