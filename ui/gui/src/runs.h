// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runs.h: the Runs tool's side pane table, the tree's query runs as the runs listing lists them (runs/qry.h): a row
// each folder of runs, under it a row each run with `live` in signal or `panicked` in the error's color while its
// writer runs and its bytes where none does, under an opened run its detail, each panic's head and the lines of its
// text behind a bar in the error's color, each conference's question and its accounts behind a quiet bar with the
// overseer's ruling in signal, the count of its records and the latest of them each with its sequence, name and bytes,
// and under the folder's runs its lessons. A run's row pressed opens or closes it; a tree with no run shows one row
// that reads so (runs.c)
#ifndef GUI_WINDOW_RUNS_H
#define GUI_WINDOW_RUNS_H
#include "pane.h"

// the sizes in the page's pixels: the detail's indent past a row's left padding, a panic's or a conference's bar and
// the padding after it, the gap between a row's texts, and the small and the plain text sizes
#define GUI_RUNS_DETAIL_PAD 24
#define GUI_RUNS_BAR 2
#define GUI_RUNS_BAR_PAD 8
#define GUI_RUNS_GAP 8
#define GUI_RUNS_SMALL_SIZE 11u
#define GUI_RUNS_TEXT_SIZE 12u

// the Runs tool's pane table
extern const GuiPaneTable gui_runs_table;

#endif
