// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.h: the problems' side pane table, read where the servers' reports stand (servers/servers.h): the pane's
// head offers the tree's check and shows how far it has gone, then a row a file with its count and under it a row a
// diagnostic, the worst first, a row pressed opening the file at the place; and the shown file's spans underlined in
// the editor (problems.c)
#ifndef GUI_WINDOW_PROBLEMS_H
#define GUI_WINDOW_PROBLEMS_H
#include "../../cli/src/servers/servers.h"
#include "editor.h"
#include "embed_dispatch_layout.h"
#include "pane.h"

typedef struct GuiProblemsPathRasterCfg GuiProblemsPathRasterCfg;

// the sizes in the page's pixels: a file's row, a diagnostic's row, the pane's head, a row's padding at the left, the
// guide's width before a diagnostic, the mark's side, the gap after it, the count's padding and its corner radius, the
// file's name size, the folder's size, the message's size, the count's size and the head's; the head's one action, the
// check; and the severities
#define GUI_PROBLEMS_FILE_ROW 24
#define GUI_PROBLEMS_ROW 22
#define GUI_PROBLEMS_HEAD 22
#define GUI_PROBLEMS_PAD_LEFT 10
#define GUI_PROBLEMS_GUIDE 12
#define GUI_PROBLEMS_MARK 8
#define GUI_PROBLEMS_GAP 6
#define GUI_PROBLEMS_COUNT_PAD 6
#define GUI_PROBLEMS_COUNT_RADIUS 9
#define GUI_PROBLEMS_NAME_SIZE 13u
#define GUI_PROBLEMS_WHERE_SIZE 11u
#define GUI_PROBLEMS_MESSAGE_SIZE 12u
#define GUI_PROBLEMS_COUNT_SIZE 11u
#define GUI_PROBLEMS_HEAD_SIZE 12u
#define GUI_PROBLEMS_CHECK 1u
#define GUI_PROBLEMS_ERROR 1u
#define GUI_PROBLEMS_WARNING 2u

// the problems' pane table
extern const GuiPaneTable gui_problems_table;

// the spans of the file shown underlined in the editor drawn in `place` at `scale` with the code cell `cell`, each on
// the rows it stands in, in its severity's color
void gui_problems_underlined(const GuiProblemsPathRasterCfg *args);
struct GuiProblemsPathRasterCfg
{
    GuiRaster *raster;
    const GuiPalette *palette;
    const GuiEditor *editor;
    GuiRect place;
    float cell;
    int scale;
};

typedef struct
{
    void (*underlined)(const GuiProblemsPathRasterCfg *args);
} GuiProblemsPathRasterNs;
EMBED_TABLE_LAYOUT(GuiProblemsPathRasterNs, underlined);
EMBED_TABLE_STORAGE GuiProblemsPathRasterNs gui_problems_path_raster EMBED_UNUSED = {
    .underlined = gui_problems_underlined,
};

#endif
