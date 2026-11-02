// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.h: the problems pane in the side beside the editor, as the controller publishes it: a row a file with its
// count and under it a row a diagnostic, the worst first; the pane's head offers the tree's check and shows how far it
// has gone; and the shown file's spans underlined in the editor (problems.c)
#ifndef GUI_PARTS_PROBLEMS_H
#define GUI_PARTS_PROBLEMS_H
typedef struct GuiProblemsRowFrameCfg GuiProblemsRowFrameCfg;
typedef struct GuiProblemsPathRasterCfg GuiProblemsPathRasterCfg;
typedef struct GuiProblemsFrameCfg GuiProblemsFrameCfg;
typedef struct GuiProblemsCfg GuiProblemsCfg;
#include "../../controller/controller_parts.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "editor.h"
#include "embed_dispatch_layout.h"

// no row; the sizes in the page's
// pixels: a file's row, a diagnostic's row, the pane's head, a row's padding at the left, the guide's width before a
// diagnostic, the mark's side, the gap after it, the count's padding and its corner radius, the file's name size, the
// folder's size, the message's size, the count's size and the head's; and the severities
#define GUI_PROBLEMS_NONE (-1)
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
#define GUI_PROBLEMS_ERROR 1u
#define GUI_PROBLEMS_WARNING 2u

// The problems: the rows as last published, NULL for none, the controller's and read alone; the row the pointer stands
// on, GUI_PROBLEMS_NONE for none; and how many rows are scrolled off the pane's top
typedef struct
{
    const CliProblems *problems;
    int pointed;
    unsigned int scroll;
} GuiProblems;

// problems holding none
void gui_problems_init(const GuiProblemsCfg *args);
// how many rows there are
unsigned int gui_problems_rows(const GuiProblemsRowFrameCfg *args);
// the top of the rows in the frame's side, under the group's title and the pane's head; the rectangle of the row
// `row` as the rows are scrolled; and the row at x, y: 1 with its number into `row`, else 0
int gui_problems_rows_top(const GuiProblemsRowFrameCfg *args);
GuiRect gui_problems_row_rect(const GuiProblemsRowFrameCfg *args);
int gui_problems_hit(const GuiProblemsRowFrameCfg *args);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_problems_move(const GuiProblemsFrameCfg *args);
// whether the pixel at x, y stands on the pane's head, which offers the check
int gui_problems_head_holds(const GuiProblemsFrameCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_problems_wheel(const GuiProblemsRowFrameCfg *args);
// the pane drawn in the frame's side under the group's title: its head, the check's progress while one runs, else
// the count of problems and files or the check offered; then the rows, a file's its arrow, name, folder and count,
// a diagnostic's its guide, its mark in its severity's color, its message and its place, the row pointed on the
// pointed ground
void gui_problems_draw(const GuiProblemsRowFrameCfg *args);
// the spans of the file shown underlined in the editor drawn in `place` at `scale` with the code cell `cell`, each on
// the rows it stands in, in its severity's color
void gui_problems_underlined(const GuiProblemsPathRasterCfg *args);
struct GuiProblemsRowFrameCfg
{
    GuiProblems *problems_out;
    const GuiProblems *problems;
    unsigned int row;
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *row_pointer;
    int rows;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
};

struct GuiProblemsPathRasterCfg
{
    const GuiProblems *problems;
    GuiRaster *raster;
    const GuiPalette *palette;
    const GuiEditor *editor;
    GuiRect place;
    float cell;
    int scale;
};

struct GuiProblemsFrameCfg
{
    GuiProblems *problems_out;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiProblemsCfg
{
    GuiProblems *problems_out;
};

typedef struct
{
    void (*init)(const GuiProblemsCfg *args);
} CliProblemsNs;
EMBED_TABLE_LAYOUT(CliProblemsNs, init);
EMBED_TABLE_STORAGE CliProblemsNs cli_problems EMBED_UNUSED = {
    .init = gui_problems_init,
};

typedef struct
{
    unsigned int (*rows)(const GuiProblemsRowFrameCfg *args);
    int (*rows_top)(const GuiProblemsRowFrameCfg *args);
    GuiRect (*row_rect)(const GuiProblemsRowFrameCfg *args);
    int (*hit)(const GuiProblemsRowFrameCfg *args);
    int (*wheel)(const GuiProblemsRowFrameCfg *args);
    void (*draw)(const GuiProblemsRowFrameCfg *args);
} GuiProblemsRowFrameNs;
EMBED_TABLE_LAYOUT(GuiProblemsRowFrameNs, rows, rows_top, row_rect, hit, wheel, draw);
EMBED_TABLE_STORAGE GuiProblemsRowFrameNs gui_problems_row_frame EMBED_UNUSED = {
    .rows = gui_problems_rows,
    .rows_top = gui_problems_rows_top,
    .row_rect = gui_problems_row_rect,
    .hit = gui_problems_hit,
    .wheel = gui_problems_wheel,
    .draw = gui_problems_draw,
};

typedef struct
{
    int (*move)(const GuiProblemsFrameCfg *args);
    int (*head_holds)(const GuiProblemsFrameCfg *args);
} GuiProblemsFrameNs;
EMBED_TABLE_LAYOUT(GuiProblemsFrameNs, move, head_holds);
EMBED_TABLE_STORAGE GuiProblemsFrameNs gui_problems_frame EMBED_UNUSED = {
    .move = gui_problems_move,
    .head_holds = gui_problems_head_holds,
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
