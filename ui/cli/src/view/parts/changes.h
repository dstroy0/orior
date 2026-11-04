// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.h: the Changes pane in the side under the strip's Commit tool, the files that differ from the last commit
// read where the git module lists them (code/git.h): a row a file, its type's mark, its name with its folder quieter
// after it, and its state letter at the right, the name and the letter in the state's color; a row pressed opens the
// file (changes.c)
#ifndef GUI_PARTS_CHANGES_H
#define GUI_PARTS_CHANGES_H
#include "../../code/git.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiChangesRowFrameCfg GuiChangesRowFrameCfg;
typedef struct GuiChangesFrameCfg GuiChangesFrameCfg;
typedef struct GuiChangesCfg GuiChangesCfg;

// the sizes in the page's pixels: the mark's box and the gap after it, the folder's text size and the state letter's
#define GUI_CHANGES_ICON 16
#define GUI_CHANGES_GAP 8
#define GUI_CHANGES_WHERE_SIZE 11u
#define GUI_CHANGES_STATE_SIZE 11u

// The changes pane: the row the pointer stands on, GUI_PANE_NONE for none; and how many rows are scrolled off the
// pane's top
typedef struct
{
    int pointed;
    unsigned int scroll;
} GuiChanges;

// a changes pane pointing at no row, scrolled to its top
void gui_changes_init(const GuiChangesCfg *args);
// how many rows there are; and the row at x, y: 1 with its number into `row_pointer`, else 0
unsigned int gui_changes_rows(const GuiChangesRowFrameCfg *args);
int gui_changes_hit(const GuiChangesRowFrameCfg *args);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_changes_move(const GuiChangesFrameCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_changes_wheel(const GuiChangesRowFrameCfg *args);
// the pane drawn in the frame's side under the group's title: its line, its head, Changes, and the rows, the row
// pointed on the line's ground
void gui_changes_draw(const GuiChangesRowFrameCfg *args);
struct GuiChangesRowFrameCfg
{
    GuiChanges *changes_out;
    const GuiChanges *changes;
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *row_pointer;
    int rows;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
};

struct GuiChangesFrameCfg
{
    GuiChanges *changes_out;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiChangesCfg
{
    GuiChanges *changes_out;
};

typedef struct
{
    void (*init)(const GuiChangesCfg *args);
} GuiChangesNs;
EMBED_TABLE_LAYOUT(GuiChangesNs, init);
EMBED_TABLE_STORAGE GuiChangesNs gui_changes EMBED_UNUSED = {
    .init = gui_changes_init,
};

typedef struct
{
    unsigned int (*rows)(const GuiChangesRowFrameCfg *args);
    int (*hit)(const GuiChangesRowFrameCfg *args);
    int (*wheel)(const GuiChangesRowFrameCfg *args);
    void (*draw)(const GuiChangesRowFrameCfg *args);
} GuiChangesRowFrameNs;
EMBED_TABLE_LAYOUT(GuiChangesRowFrameNs, rows, hit, wheel, draw);
EMBED_TABLE_STORAGE GuiChangesRowFrameNs gui_changes_row_frame EMBED_UNUSED = {
    .rows = gui_changes_rows,
    .hit = gui_changes_hit,
    .wheel = gui_changes_wheel,
    .draw = gui_changes_draw,
};

typedef struct
{
    int (*move)(const GuiChangesFrameCfg *args);
} GuiChangesFrameNs;
EMBED_TABLE_LAYOUT(GuiChangesFrameNs, move);
EMBED_TABLE_STORAGE GuiChangesFrameNs gui_changes_frame EMBED_UNUSED = {
    .move = gui_changes_move,
};

#endif
