// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap.h: the minimap part: the text drawn small down the editor's right edge, each word a bar in its token's color
// as the CLI's view hands the lines' bars, the part in view marked by a slider that drags; where every line fits the
// map shows them all, where they do not it scrolls with the text so the slider stays over the part in view; along its
// right edge the overview strip marks the whole text, the caret's line and find's matches; a press goes to that part
// of the text (minimap.c)
#ifndef GUI_PARTS_MINIMAP_H
#define GUI_PARTS_MINIMAP_H
typedef struct GuiMinimapPlaceScaleCfg GuiMinimapPlaceScaleCfg;
typedef struct GuiMinimapPlaceCfg GuiMinimapPlaceCfg;
typedef struct GuiMinimapCfg GuiMinimapCfg;
typedef struct GuiMinimapJsonCfg GuiMinimapJsonCfg;
typedef struct GuiMinimapFirstCfg GuiMinimapFirstCfg;
#include "editor.h"
#include "embed_dispatch_layout.h"

// the sizes in the page's pixels: a row's height, a cell's width, the margin before the bars, the most cells a line's
// bars reach, the overview strip's width, a mark's least height and the slider's least height; and the most lines and
// bars the map holds
#define GUI_MAP_ROW 2
#define GUI_MAP_CHAR 1
#define GUI_MAP_MARGIN 4
#define GUI_MAP_WIDEST 160u
#define GUI_MAP_STRIP 8
#define GUI_MAP_MARK 2
#define GUI_MAP_SLIDER_LEAST 8
#define GUI_MAP_LINES_MOST 4096u
#define GUI_MAP_BARS_MOST 65536u

// A bar: the cell it starts at, its cells and the index of its token's name among the grammar's
typedef struct
{
    unsigned int start;
    unsigned int width;
    unsigned int name;
} GuiBar;

// Where the map stands beside the view: the rows it has room for; the file's lines; the line at its first row; the
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
    int slider_tall;
} GuiMapPlace;

// The minimap: the lines stored, from `from`, `count` of them, each line's first bar's index among the bars with one
// index more where the bars end, and the bars; whether the lines stored are stale; the view's top the map last stood
// beside and how many rows above the slider the map began, stored so the map stands still while the view does; the
// place it last stood at; and the drag, whether one runs, where it began and the view's top then
typedef struct
{
    unsigned long long from;
    unsigned int count;
    unsigned int *line_at;
    GuiBar *bar;
    unsigned int bars;
    int stale;
    int anchored;
    unsigned long long anchor_top;
    long long anchor_offset;
    GuiMapPlace place;
    int dragging;
    int drag_y;
    unsigned long long drag_top;
} GuiMinimap;

// a map with no lines, its cells allocated: 1, or 0 where they could not be; its cells freed; and the lines stored
// dropped, the map stale and the view's top it stood beside forgotten, as a tab is shown
int gui_minimap_init(const GuiMinimapJsonCfg *args);
void gui_minimap_free(const GuiMinimapJsonCfg *args);
void gui_minimap_clear(const GuiMinimapJsonCfg *args);
// the lines of the answer `root` of `json`, `{"from", "bars"}`, stored in place of those stored before, each line's
// bars `[start, width, name]`, the map stale no more: 1, or 0 where the answer is no such object
int gui_minimap_put(const GuiMinimapJsonCfg *args);
// the map placed in `place` at `scale` beside the view of `editor`, which shows `on_screen` lines: its rows, the line
// at its first row and the slider
void gui_minimap_on_place(const GuiMinimapPlaceCfg *args);
// the lines the map placed stands for, first and last: 1, or 0 where the text has none
int gui_minimap_span(const GuiMinimapFirstCfg *args);
// whether the lines stored are those from `first` to `last` and not stale
int gui_minimap_current(const GuiMinimapCfg *args);
// the map drawn in `place` at `scale`: the editor's ground and the line along its left; then, while the text has lines,
// the selection's rows, the matches' rows, each line's bars in their tokens' colors, the slider, and the overview strip
// at its right with the matches and the caret's line
void gui_minimap_draw(const GuiMinimapPlaceCfg *args);
// a press at x, y: on the strip, the view's top given as that part of the text; on the map outside the slider, the top
// given as the row's line centered, and the slider's drag begun; on the slider, the top given as it is and the drag
// begun: 1, or 0 where the press is outside the map or the text has no lines; the pointer moved to `y` while the drag
// runs, the top given as the slider's run gives it: 1, or 0 where no drag runs; and the drag ended
int gui_minimap_press(const GuiMinimapPlaceScaleCfg *args);
int gui_minimap_drag(const GuiMinimapPlaceScaleCfg *args);
void gui_minimap_release(const GuiMinimapJsonCfg *args);
struct GuiMinimapPlaceScaleCfg
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

struct GuiMinimapPlaceCfg
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
    const GuiMinimap *map_read;
    unsigned long long first_value;
    unsigned long long last_value;
};

struct GuiMinimapJsonCfg
{
    GuiMinimap *map;
    const CliJson *json;
    unsigned int root;
};

struct GuiMinimapFirstCfg
{
    const GuiMinimap *map_read;
    unsigned long long *first;
    unsigned long long *last;
};

typedef struct
{
    int (*init)(const GuiMinimapJsonCfg *args);
    void (*free)(const GuiMinimapJsonCfg *args);
    void (*clear)(const GuiMinimapJsonCfg *args);
    int (*put)(const GuiMinimapJsonCfg *args);
    void (*release)(const GuiMinimapJsonCfg *args);
} GuiMinimapJsonNs;
EMBED_TABLE_LAYOUT(GuiMinimapJsonNs, init, free, clear, put, release);
EMBED_TABLE_STORAGE GuiMinimapJsonNs gui_minimap_json EMBED_UNUSED = {
    .init = gui_minimap_init,
    .free = gui_minimap_free,
    .clear = gui_minimap_clear,
    .put = gui_minimap_put,
    .release = gui_minimap_release,
};

typedef struct
{
    void (*place)(const GuiMinimapPlaceCfg *args);
    void (*draw)(const GuiMinimapPlaceCfg *args);
} GuiMinimapPlaceNs;
EMBED_TABLE_LAYOUT(GuiMinimapPlaceNs, place, draw);
EMBED_TABLE_STORAGE GuiMinimapPlaceNs gui_minimap_place EMBED_UNUSED = {
    .place = gui_minimap_on_place,
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
    int (*current)(const GuiMinimapCfg *args);
} CliMinimapNs;
EMBED_TABLE_LAYOUT(CliMinimapNs, current);
EMBED_TABLE_STORAGE CliMinimapNs cli_minimap EMBED_UNUSED = {
    .current = gui_minimap_current,
};

typedef struct
{
    int (*press)(const GuiMinimapPlaceScaleCfg *args);
    int (*drag)(const GuiMinimapPlaceScaleCfg *args);
} GuiMinimapPlaceScaleNs;
EMBED_TABLE_LAYOUT(GuiMinimapPlaceScaleNs, press, drag);
EMBED_TABLE_STORAGE GuiMinimapPlaceScaleNs gui_minimap_place_scale EMBED_UNUSED = {
    .press = gui_minimap_press,
    .drag = gui_minimap_drag,
};

#endif
