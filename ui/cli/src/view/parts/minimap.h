// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap.h: the minimap part: the text drawn small down the editor's right edge, each word a bar in its token's color
// as the CLI's view hands the lines' bars, the part in view marked by a slider that drags; where every line fits the
// map shows them all, where they do not it scrolls with the text so the slider stays over the part in view; along its
// right edge the overview strip marks the whole text, the caret's line and find's matches; a press goes to that part
// of the text (minimap.c)
#ifndef GUI_PARTS_MINIMAP_H
#define GUI_PARTS_MINIMAP_H
#include "editor.h"
#include "embed_dispatch_layout.h"

typedef struct GuiMinimapPositionScaleCfg GuiMinimapPositionScaleCfg;
typedef struct GuiMinimapPositionCfg GuiMinimapPositionCfg;
typedef struct GuiMinimapCfg GuiMinimapCfg;
typedef struct GuiMinimapFirstCfg GuiMinimapFirstCfg;

// the sizes in the page's pixels: a row's height, a cell's width, the margin before the bars, the most cells a line's
// bars reach, the overview strip's width, a mark's least height and the slider's least height
#define GUI_MAP_ROW 2
#define GUI_MAP_CHAR 1
#define GUI_MAP_MARGIN 4
#define GUI_MAP_WIDEST 160u
#define GUI_MAP_STRIP 8
#define GUI_MAP_MARK 2
#define GUI_MAP_SLIDER_MIN 8

// Where the map stands beside the view: the rows it has capacity for; the file's lines; the line at its first row; the
// view's rows as a line count, with its fraction; the lines the view scrolls over; and the slider's top and height in
// pixels from the map's top
typedef struct
{
    unsigned int fit;
    unsigned long long lines;
    unsigned long long start;
    float on_screen;
    float range;
    int slider_top;
    int slider_height;
} GuiMapPosition;

// The minimap: the lines as last published, NULL for none, the controller's and read alone; the view's top the map last
// stood beside and how many rows above the slider the map began, stored so the map stands still while the view does;
// the place it last stood at; and the drag, whether one runs, where it began and the view's top then
typedef struct
{
    const CliDocumentMap *lines;
    int anchored;
    unsigned long long anchor_top;
    long long anchor_offset;
    GuiMapPosition place;
    int dragging;
    int drag_y;
    unsigned long long drag_top;
} GuiMinimap;

// the view's top it stood beside forgotten and any drag ended, as a tab is shown
void gui_minimap_clear(const GuiMinimapCfg *args);
// the map placed in `place` at `scale` beside the view of `editor`, which shows `on_screen` lines: its rows, the line
// at its first row and the slider
void gui_minimap_on_position(const GuiMinimapPositionCfg *args);
// the lines the map placed stands for, first and last: 1, or 0 where the text has none
int gui_minimap_span(const GuiMinimapFirstCfg *args);
// the map drawn in `place` at `scale`: the editor's ground and the line along its left; then, while the text has lines,
// the selection's rows, the matches' rows, each line's bars in their tokens' colors, the slider, and the overview strip
// at its right with the matches and the caret's line
void gui_minimap_draw(const GuiMinimapPositionCfg *args);
// a press at x, y: on the strip, the view's top given as that part of the text; on the map outside the slider, the top
// given as the row's line centered, and the slider's drag begun; on the slider, the top given as it is and the drag
// begun: 1, or 0 where the press is outside the map or the text has no lines; the pointer moved to `y` while the drag
// runs, the top given as the slider's run gives it: 1, or 0 where no drag runs; and the drag ended
int gui_minimap_press(const GuiMinimapPositionScaleCfg *args);
int gui_minimap_drag(const GuiMinimapPositionScaleCfg *args);
void gui_minimap_release(const GuiMinimapCfg *args);
struct GuiMinimapPositionScaleCfg
{
    GuiMinimap *map;
    GuiRect place;
    int scale;
    const GuiEditor *editor;
    float on_screen;
    int x;
    int y;
    unsigned long long *top;
};

struct GuiMinimapPositionCfg
{
    GuiMinimap *map;
    GuiRect place;
    int scale;
    const GuiEditor *editor;
    float on_screen;
    GuiRaster *raster;
    const GuiPalette *palette;
};

struct GuiMinimapCfg
{
    GuiMinimap *map;
};

struct GuiMinimapFirstCfg
{
    const GuiMinimap *map_read;
    unsigned long long *first;
    unsigned long long *last;
};

typedef struct
{
    void (*clear)(const GuiMinimapCfg *args);
    void (*release)(const GuiMinimapCfg *args);
} GuiMinimapNs;
EMBED_TABLE_LAYOUT(GuiMinimapNs, clear, release);
EMBED_TABLE_STORAGE GuiMinimapNs gui_minimap EMBED_UNUSED = {
    .clear = gui_minimap_clear,
    .release = gui_minimap_release,
};

typedef struct
{
    void (*place)(const GuiMinimapPositionCfg *args);
    void (*draw)(const GuiMinimapPositionCfg *args);
} GuiMinimapPositionNs;
EMBED_TABLE_LAYOUT(GuiMinimapPositionNs, place, draw);
EMBED_TABLE_STORAGE GuiMinimapPositionNs gui_minimap_position EMBED_UNUSED = {
    .place = gui_minimap_on_position,
    .draw = gui_minimap_draw,
};

typedef struct
{
    int (*span)(const GuiMinimapFirstCfg *args);
} GuiMinimapFirstNs;
EMBED_TABLE_LAYOUT(GuiMinimapFirstNs, span);
EMBED_TABLE_STORAGE GuiMinimapFirstNs gui_minimap_first EMBED_UNUSED = {
    .span = gui_minimap_span,
};

typedef struct
{
    int (*press)(const GuiMinimapPositionScaleCfg *args);
    int (*drag)(const GuiMinimapPositionScaleCfg *args);
} GuiMinimapPositionScaleNs;
EMBED_TABLE_LAYOUT(GuiMinimapPositionScaleNs, press, drag);
EMBED_TABLE_STORAGE GuiMinimapPositionScaleNs gui_minimap_position_scale EMBED_UNUSED = {
    .press = gui_minimap_press,
    .drag = gui_minimap_drag,
};

#endif
