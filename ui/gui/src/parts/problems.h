// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.h: the problems pane in the side beside the editor: each file's diagnostics as the servers and the
// checkers emit them, a row a file with its count and under it a row a diagnostic, the worst first, a file's row
// pressed closing and opening its diagnostics and a diagnostic's row pressed picking its place to open; the pane's
// head starts the tree's check and shows how far it has gone; and the shown file's spans underlined in the editor
// (problems.c)
#ifndef GUI_PARTS_PROBLEMS_H
#define GUI_PARTS_PROBLEMS_H
#include "../../../cli/src/text/json_tree.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "editor.h"

// the most files and diagnostics stored and the bytes their texts are stored in; no row; the sizes in the page's
// pixels: a file's row, a diagnostic's row, the pane's head, a row's padding at the left, the guide's width before a
// diagnostic, the mark's side, the gap after it, the count's padding and its corner radius, the file's name size, the
// folder's size, the message's size, the count's size and the head's; and the severities
#define GUI_PROBLEMS_FILES 512u
#define GUI_PROBLEMS_ITEMS 8192u
#define GUI_PROBLEMS_TEXT_ROOM (4ull << 20u)
#define GUI_PROBLEMS_DEAD_MOST 65536ull
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

// what a press picks: nothing, the tree's check to start, the check to stop, or a place to open, its path from the
// tree's top with its line and column in units
#define GUI_PROBLEMS_PICK_NONE 0u
#define GUI_PROBLEMS_PICK_CHECK 1u
#define GUI_PROBLEMS_PICK_STOP 2u
#define GUI_PROBLEMS_PICK_OPEN 3u

// A file with diagnostics: its path's start among the texts and its length, where its name starts in the path, its
// first diagnostic among the diagnostics and how many, and whether its rows are closed
typedef struct
{
    unsigned long long path;
    unsigned int length;
    unsigned int name;
    unsigned int first;
    unsigned int count;
    int closed;
} GuiProblemFile;

// A diagnostic: the file it is in, its span's first place and the place after it, each a line and a column in
// units, its severity, 1 an error, 2 a warning, 3 and 4 notes, and its message's first line's start among the texts
// and its length
typedef struct
{
    unsigned int file;
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
    unsigned int severity;
    unsigned long long message;
    unsigned int message_length;
} GuiProblem;

// What a press picked: its kind, and for a place its path, line and column
typedef struct
{
    unsigned int kind;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long line;
    unsigned long long col;
} GuiProblemPick;

// The problems: the files in a cell, how many, and the diagnostics in a cell, each file's together, how many; their
// texts in a line in a cell of its own and the bytes of it no file or diagnostic reads any more; how many
// errors stand; the check's files done and total, the total 0 where no check runs; the row the pointer stands on,
// GUI_PROBLEMS_NONE for none; and how many rows are scrolled off the pane's top
typedef struct
{
    GuiProblemFile *file;
    unsigned int files;
    GuiProblem *item;
    unsigned int items;
    VerbaLinea texts;
    unsigned long long dead;
    unsigned int errors;
    unsigned long long done;
    unsigned long long total;
    int pointed;
    unsigned int scroll;
} GuiProblems;

// problems holding none, the cells taken: 1, or 0 where they could not be; and every cell given back
int gui_problems_made(GuiProblems *problems);
void gui_problems_given(GuiProblems *problems);
// the event `root` of `json`, `{"path", "items": [{"from", "to", "severity", "message"}...]}`, taken: the file's
// diagnostics replaced by the items, the worst first and the earliest among the same, the file dropped where none
// come: 1, or 0 where the event is no such object or the cells are full
int gui_problems_put(GuiProblems *problems, const CliJson *json, unsigned int root);
// the tree's check gone `done` of `total` files, a total of 0 for no check running
void gui_problems_checking(GuiProblems *problems, unsigned long long done, unsigned long long total);
// the diagnostics of the file at `path`: how many, and the first of them into `first`
unsigned int gui_problems_of(const GuiProblems *problems, const char *path, unsigned int *first);
// the rows: a row for each file and, where its rows are open, one for each of its diagnostics: how many; and the
// row `row` read: the file into `file` and the diagnostic into `item`, GUI_PROBLEMS_NONE for a file's row: 1, or 0
// past the rows
unsigned int gui_problems_rows(const GuiProblems *problems);
int gui_problems_row(const GuiProblems *problems, unsigned int row, unsigned int *file, int *item);
// the top of the rows in the frame's side, under the group's title and the pane's head; the rectangle of the row
// `row` as the rows are scrolled; and the row at x, y: 1 with its number into `row`, else 0
int gui_problems_rows_top(const GuiFrame *frame);
GuiRect gui_problems_row_rect(const GuiProblems *problems, const GuiFrame *frame, unsigned int row);
int gui_problems_hit(const GuiProblems *problems, const GuiFrame *frame, int x, int y, unsigned int *row);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_problems_moved(GuiProblems *problems, const GuiFrame *frame, int x, int y);
// a press at x, y: on the head, the check picked to start, or to stop where one runs; on a file's row its rows
// closed or opened; on a diagnostic's row its place picked: 1 where the press was the side's
int gui_problems_pressed(GuiProblems *problems, const GuiFrame *frame, int x, int y, GuiProblemPick *pick);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_problems_wheeled(GuiProblems *problems, const GuiFrame *frame, int rows);
// the pane drawn in the frame's side under the group's title: its head, the check's progress while one runs, else
// the count of problems and files or the check offered; then the rows, a file's its arrow, name, folder and count,
// a diagnostic's its guide, its mark in its severity's color, its message and its place, the row pointed on the
// pointed ground
void gui_problems_drawn(const GuiProblems *problems, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                        const GuiFrame *frame);
// the spans of the diagnostics of the file at `path` underlined in the editor drawn in `place` at `scale` with the
// code cell `cell`, each on the rows it stands in, in its severity's color
void gui_problems_underlined(const GuiProblems *problems, const char *path, const GuiEditor *editor, GuiRaster *raster,
                             const GuiPalette *palette, GuiRect place, float cell, float scale);
#endif
