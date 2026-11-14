// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run.h: the Run tool's pane table, located in the panel at the work's foot, the rows the CLI's job runs list
// (runs/job_runs.h): a group with its arrow and its count, a job with a dot while a run of it goes, under the job
// chosen its file and about, a field a param, its key in signal where it is required and its default quieter, the
// choices of the param focused each with its box, a run a row in its state's color, and the lines of the run displayed in
// the code face in their stream's color and their spans' colors, its end last. Over the rows the pane's head: the
// filter's field, Start and Stop, Stop quieter while no run displayed goes, and the refusal of the last start under them
// (run.c)
#ifndef GUI_WINDOW_RUN_H
#define GUI_WINDOW_RUN_H
#include "pane.h"

// the head's actions: the filter's field, which takes the keys, Start and Stop
#define GUI_RUN_FIELD 1u
#define GUI_RUN_START 2u
#define GUI_RUN_STOP 3u

// the sizes in the page's pixels: the refusal's row and its text size, a line's text size, the gap between a row's two
// texts, the live dot's side, a choice's box side and its corners' radius
#define GUI_RUN_REFUSAL 20
#define GUI_RUN_REFUSAL_SIZE 12u
#define GUI_RUN_LINE_SIZE 12u
#define GUI_RUN_GAP 8
#define GUI_RUN_DOT 6
#define GUI_RUN_BOX 11
#define GUI_RUN_BOX_RADIUS 2

// the pane `pane` of the table in `frame` scrolled to the top the CLI answers as its rows grew, read with the rows the
// pane is tall
void gui_run_follow(const GuiPaneCfg *args);

typedef struct
{
    void (*follow)(const GuiPaneCfg *args);
} GuiRunPaneNs;
EMBED_TABLE_LAYOUT(GuiRunPaneNs, follow);
EMBED_TABLE_STORAGE GuiRunPaneNs gui_run_pane EMBED_UNUSED = {
    .follow = gui_run_follow,
};

// the Run tool's pane table
extern const GuiPaneTable gui_run_table;

#endif
