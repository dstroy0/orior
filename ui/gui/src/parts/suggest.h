// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest.h: the completion panel: the items the CLI answers for the word before the cursor, the server's and the
// file's own words, filtered by the prefix and ranked, a start beating a part and a part beating letters in order,
// listed under the prefix's first cell, the chosen row on the selection's ground, each row its kind's mark, its label
// with the prefix in signal and its detail quieter at the right; a detail pane beside the list for the chosen item;
// Up, Down, PageUp and PageDown move the choice, Enter and Tab accept it, Escape closes, a press on a row accepts it
// and the wheel scrolls the rows (suggest.c)
#ifndef GUI_PARTS_SUGGEST_H
#define GUI_PARTS_SUGGEST_H
typedef struct GuiSuggestJsonCfg GuiSuggestJsonCfg;
typedef struct GuiSuggestRasterCfg GuiSuggestRasterCfg;
typedef struct GuiSuggestXCfg GuiSuggestXCfg;
typedef struct GuiSuggestXYCfg GuiSuggestXYCfg;
typedef struct GuiSuggestLengthCfg GuiSuggestLengthCfg;
typedef struct GuiSuggestCfg GuiSuggestCfg;
typedef struct GuiSuggestRowsCfg GuiSuggestRowsCfg;
#include "../../../cli/src/text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "panel.h"

// the sizes in the page's pixels: a row's height, the rows shown at once, the list's least and most width, 16rem and
// 26rem, the detail pane's width, 18rem, and its most height, 14rem, a kind's mark, 0.55rem, a row's padding across,
// 0.55rem, the gap between a row's parts, 0.5rem, the list's padding down, 0.2rem, how far the list's left stands
// before the prefix's cell, and the detail's padding; the most items stored, the most bytes a prefix takes, and the
// bytes the items' texts are stored in
#define GUI_SUGGEST_ROW 22
#define GUI_SUGGEST_ROWS 10u
#define GUI_SUGGEST_LEAST 256
#define GUI_SUGGEST_MOST 416
#define GUI_SUGGEST_DETAIL 288
#define GUI_SUGGEST_DETAIL_TALL 224
#define GUI_SUGGEST_KIND 9
#define GUI_SUGGEST_PAD_X 9
#define GUI_SUGGEST_GAP 8
#define GUI_SUGGEST_PAD_Y 3
#define GUI_SUGGEST_BACK 22
#define GUI_SUGGEST_DETAIL_PAD_X 11
#define GUI_SUGGEST_DETAIL_PAD_Y 6
#define GUI_SUGGEST_ITEMS_MOST 8192u
#define GUI_SUGGEST_PREFIX 256u
#define GUI_SUGGEST_TEXT_ROOM (1ull << 20u)

// what a key did: nothing of the list's, the list changed, the item chosen accepted, or the list closed
#define GUI_SUGGEST_NONE 0u
#define GUI_SUGGEST_MOVED 1u
#define GUI_SUGGEST_ACCEPTED 2u
#define GUI_SUGGEST_CLOSED 3u

// An item: its label, kind, detail and the text it inserts, each where it starts among the stored texts and its
// length; and its score against the prefix, 0 for an item the prefix leaves out
typedef struct
{
    unsigned long long label;
    unsigned long long label_length;
    unsigned long long kind;
    unsigned long long kind_length;
    unsigned long long detail;
    unsigned long long detail_length;
    unsigned long long insert;
    unsigned long long insert_length;
    unsigned int score;
} GuiItem;

// The completion: whether the list is open and whether the keys forced it open; the items in a cell, how many, and
// their texts in a line in a cell of its own; the items shown, indexes in a cell in rank order, how many;
// the row chosen and the first row shown; the prefix the items answer, its line and its first unit; and the list's
// rectangle, the rows' and the detail pane's, where they were drawn last
typedef struct
{
    int open;
    int forced;
    GuiItem *item;
    unsigned int items;
    VerbaLinea texts;
    unsigned int *shown;
    unsigned int count;
    unsigned int index;
    unsigned int first;
    char prefix[GUI_SUGGEST_PREFIX];
    unsigned long long line;
    unsigned long long from;
    GuiRect rect;
    GuiRect list;
    GuiRect detail;
} GuiSuggest;

// a completion with no items, its cells allocated: 1, or 0 where they could not be; its cells freed; and the list
// closed, its items dropped
int gui_suggest_init(const GuiSuggestRowsCfg *args);
void gui_suggest_free(const GuiSuggestRowsCfg *args);
void gui_suggest_close(const GuiSuggestJsonCfg *args);
// the answer `root` of `json`, `{"from", "prefix", "items", "words"}`, taken: the items stored, the server's first and
// then the file's words whose labels the server did not give, an item marked `snippet` stored as the text its
// snippet inserts with each stop's placeholder in its place, filtered and ranked by the prefix: 1 with the list open
// where any item answers it and, where `forced` is 0, the one item is not the prefix itself and the prefix is not
// empty unless the list was open; else 0 with the list closed
int gui_suggest_put(const GuiSuggestJsonCfg *args);
// the list open filtered again by `length` bytes of the `prefix` the cursor now stands after, the items saved: 1 with
// the list open where any item answers it and the prefix is not empty unless the keys forced the list, else 0 with
// the list closed
int gui_suggest_prefix(const GuiSuggestJsonCfg *args);
// how well `length` bytes of `label` answer the `typed` prefix: 5 for a start, 4 for a start by case, 3 for a part,
// 2 for its letters in order, 0 for nothing; a label answers an empty prefix with 1
unsigned int gui_suggest_score(const GuiSuggestLengthCfg *args);
// a key named as the page's editor names it taken while the list is open: what it did
unsigned int gui_suggest_key(const GuiSuggestJsonCfg *args);
// the item chosen's text to insert and its length, NULL where none is chosen
const char *gui_suggest_choose(const GuiSuggestCfg *args);
// whether the pixel at x, y stands in the list or its detail pane; a press at x, y: the row's item chosen and
// accepted, GUI_SUGGEST_ACCEPTED, or GUI_SUGGEST_NONE where the press is not on a row; and the wheel turned `rows`
// over the list: 1 where the rows shown moved
int gui_suggest_holds(const GuiSuggestXCfg *args);
unsigned int gui_suggest_press(const GuiSuggestXYCfg *args);
int gui_suggest_wheel(const GuiSuggestRowsCfg *args);
// the list drawn for the prefix whose first cell is `anchor`, under its row inside `bounds` at `scale`, the detail
// pane beside it where the chosen item has a detail; the rectangles recorded
void gui_suggest_draw(const GuiSuggestRasterCfg *args);
struct GuiSuggestJsonCfg
{
    GuiSuggest *suggest;
    const CliJson *json;
    unsigned int root;
    int forced;
    const char *prefix;
    unsigned long long length;
    const char *name;
};

struct GuiSuggestRasterCfg
{
    GuiSuggest *suggest;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect bounds;
    int scale;
};

struct GuiSuggestXCfg
{
    const GuiSuggest *suggest_read;
    int x;
    int y;
};

struct GuiSuggestXYCfg
{
    GuiSuggest *suggest;
    int x;
    int y;
};

struct GuiSuggestLengthCfg
{
    unsigned long long length;
    const char *label;
    const char *typed;
};

struct GuiSuggestCfg
{
    const GuiSuggest *suggest_read;
    unsigned long long *length_pointer;
};

struct GuiSuggestRowsCfg
{
    GuiSuggest *suggest;
    int rows;
};

typedef struct
{
    int (*init)(const GuiSuggestRowsCfg *args);
    void (*free)(const GuiSuggestRowsCfg *args);
    int (*wheel)(const GuiSuggestRowsCfg *args);
} GuiSuggestRowsNs;
EMBED_TABLE_LAYOUT(GuiSuggestRowsNs, init, free, wheel);
EMBED_TABLE_STORAGE GuiSuggestRowsNs gui_suggest_rows EMBED_UNUSED = {
    .init = gui_suggest_init,
    .free = gui_suggest_free,
    .wheel = gui_suggest_wheel,
};

typedef struct
{
    void (*close)(const GuiSuggestJsonCfg *args);
    int (*put)(const GuiSuggestJsonCfg *args);
    int (*prefix)(const GuiSuggestJsonCfg *args);
    unsigned int (*key)(const GuiSuggestJsonCfg *args);
} GuiSuggestJsonNs;
EMBED_TABLE_LAYOUT(GuiSuggestJsonNs, close, put, prefix, key);
EMBED_TABLE_STORAGE GuiSuggestJsonNs gui_suggest_json EMBED_UNUSED = {
    .close = gui_suggest_close,
    .put = gui_suggest_put,
    .prefix = gui_suggest_prefix,
    .key = gui_suggest_key,
};

typedef struct
{
    unsigned int (*score)(const GuiSuggestLengthCfg *args);
} GuiSuggestLengthNs;
EMBED_TABLE_LAYOUT(GuiSuggestLengthNs, score);
EMBED_TABLE_STORAGE GuiSuggestLengthNs gui_suggest_length EMBED_UNUSED = {
    .score = gui_suggest_score,
};

typedef struct
{
    const char *(*choose)(const GuiSuggestCfg *args);
} CliSuggestNs;
EMBED_TABLE_LAYOUT(CliSuggestNs, choose);
EMBED_TABLE_STORAGE CliSuggestNs cli_suggest EMBED_UNUSED = {
    .choose = gui_suggest_choose,
};

typedef struct
{
    int (*holds)(const GuiSuggestXCfg *args);
} GuiSuggestXNs;
EMBED_TABLE_LAYOUT(GuiSuggestXNs, holds);
EMBED_TABLE_STORAGE GuiSuggestXNs gui_suggest_x EMBED_UNUSED = {
    .holds = gui_suggest_holds,
};

typedef struct
{
    unsigned int (*press)(const GuiSuggestXYCfg *args);
} GuiSuggestXYNs;
EMBED_TABLE_LAYOUT(GuiSuggestXYNs, press);
EMBED_TABLE_STORAGE GuiSuggestXYNs gui_suggest_xy EMBED_UNUSED = {
    .press = gui_suggest_press,
};

typedef struct
{
    void (*draw)(const GuiSuggestRasterCfg *args);
} GuiSuggestRasterNs;
EMBED_TABLE_LAYOUT(GuiSuggestRasterNs, draw);
EMBED_TABLE_STORAGE GuiSuggestRasterNs gui_suggest_raster EMBED_UNUSED = {
    .draw = gui_suggest_draw,
};

#endif
