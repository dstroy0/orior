// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.h: the Outline pane in the side under the strip's Structure tool, the declarations of the view shown read
// where the symbols' outline lists them (code/symbols.h): a row a declaration, as deep as its line's indent with a
// guide down each level, its kind's mark before its name, the one the cursor stands in on the idle selection's ground;
// a row pressed puts the cursor on its line (structure.c)
#ifndef GUI_PARTS_STRUCTURE_H
#define GUI_PARTS_STRUCTURE_H
#include "../../code/symbols.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiStructureRowFrameCfg GuiStructureRowFrameCfg;
typedef struct GuiStructureFrameCfg GuiStructureFrameCfg;
typedef struct GuiStructureCfg GuiStructureCfg;

// the sizes in the page's pixels: the mark's box, the gap after it and the mark's text size
#define GUI_STRUCTURE_MARK 16
#define GUI_STRUCTURE_GAP 6
#define GUI_STRUCTURE_MARK_SIZE 11u

// The outline pane: the row the pointer stands on, GUI_PANE_NONE for none; and how many rows are scrolled off the
// pane's top
typedef struct
{
    int pointed;
    unsigned int scroll;
} GuiStructure;

// an outline pane pointing at no row, scrolled to its top
void gui_structure_init(const GuiStructureCfg *args);
// how many rows there are; and the row at x, y: 1 with its number into `row_pointer`, else 0
unsigned int gui_structure_rows(const GuiStructureRowFrameCfg *args);
int gui_structure_hit(const GuiStructureRowFrameCfg *args);
// the row the cursor on `cursor_line` stands in, the last whose line is at or before it: its number, or GUI_PANE_NONE
// where none is
int gui_structure_current(const GuiStructureRowFrameCfg *args);
// the pointer moved to x, y: the row it stands on marked: 1 where that changed
int gui_structure_move(const GuiStructureFrameCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_structure_wheel(const GuiStructureRowFrameCfg *args);
// the pane drawn in the frame's side under the group's title: its line, its head, Outline, and the rows, each its
// guides, its kind's mark in the mark's color and its name, the row pointed on the line's ground and the row the
// cursor on `cursor_line` stands in on the idle selection's
void gui_structure_draw(const GuiStructureRowFrameCfg *args);
struct GuiStructureRowFrameCfg
{
    GuiStructure *structure_out;
    const GuiStructure *structure;
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *row_pointer;
    int rows;
    unsigned long long cursor_line;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
};

struct GuiStructureFrameCfg
{
    GuiStructure *structure_out;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiStructureCfg
{
    GuiStructure *structure_out;
};

typedef struct
{
    void (*init)(const GuiStructureCfg *args);
} GuiStructureNs;
EMBED_TABLE_LAYOUT(GuiStructureNs, init);
EMBED_TABLE_STORAGE GuiStructureNs gui_structure EMBED_UNUSED = {
    .init = gui_structure_init,
};

typedef struct
{
    unsigned int (*rows)(const GuiStructureRowFrameCfg *args);
    int (*hit)(const GuiStructureRowFrameCfg *args);
    int (*current)(const GuiStructureRowFrameCfg *args);
    int (*wheel)(const GuiStructureRowFrameCfg *args);
    void (*draw)(const GuiStructureRowFrameCfg *args);
} GuiStructureRowFrameNs;
EMBED_TABLE_LAYOUT(GuiStructureRowFrameNs, rows, hit, current, wheel, draw);
EMBED_TABLE_STORAGE GuiStructureRowFrameNs gui_structure_row_frame EMBED_UNUSED = {
    .rows = gui_structure_rows,
    .hit = gui_structure_hit,
    .current = gui_structure_current,
    .wheel = gui_structure_wheel,
    .draw = gui_structure_draw,
};

typedef struct
{
    int (*move)(const GuiStructureFrameCfg *args);
} GuiStructureFrameNs;
EMBED_TABLE_LAYOUT(GuiStructureFrameNs, move);
EMBED_TABLE_STORAGE GuiStructureFrameNs gui_structure_frame EMBED_UNUSED = {
    .move = gui_structure_move,
};

#endif
