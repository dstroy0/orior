// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane.h: the side's one pane, drawn and driven the same for every tool from a table of kinds. The side carries the
// group's title at its head; under it the tool's kind supplies the pane's head, what stands under the head before the
// rows, the rows as the CLI lists them, a row's height, how a row draws, the row current, what a press on a row opens,
// what a press on the head does, and the reading of its data as the events that change it come. The pane's own state
// is the row the pointer stands on, the head's action it stands on and the rows scrolled off the top, one state a tool
// (pane.c)
#ifndef GUI_PARTS_PANE_H
#define GUI_PARTS_PANE_H
#include "../../editor/document.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiPaneCfg GuiPaneCfg;
typedef struct GuiPaneKind GuiPaneKind;
typedef struct GuiPanePartCfg GuiPanePartCfg;
typedef struct GuiPaneFieldCfg GuiPaneFieldCfg;

// no row; the most tools the side has a pane for; and the sizes in the page's pixels: the side's head with the group's
// title, the line under it, the pane's head, a row, a row's padding at the left, a guide's width, the arrow's side,
// the group title's text size, the pane head's and a row's
#define GUI_PANE_NONE (-1)
#define GUI_PANE_TOOLS 16u
#define GUI_PANE_SIDE_HEAD 30
#define GUI_PANE_LINE 1
#define GUI_PANE_HEAD 22
#define GUI_PANE_ROW 22
#define GUI_PANE_PAD_LEFT 8
#define GUI_PANE_GUIDE 12
#define GUI_PANE_ARROW 5
#define GUI_PANE_SIDE_HEAD_SIZE 12u
#define GUI_PANE_HEAD_SIZE 12u
#define GUI_PANE_ROW_SIZE 13u

// the sizes in the page's pixels of what a kind's head stands under the pane's head: a field's row, the field's padding
// inside its row and its text's padding inside its border, its corners' radius, the caret's width, a choice's row, and
// the text sizes of the field and of a choice
#define GUI_PANE_BOX 30
#define GUI_PANE_BOX_PAD 4
#define GUI_PANE_FIELD_PAD 8
#define GUI_PANE_RADIUS 5
#define GUI_PANE_CARET 2
#define GUI_PANE_CHOICE 24
#define GUI_PANE_FIELD_SIZE 12u
#define GUI_PANE_CHOICE_SIZE 12u

// the events a kind reads its data on: its pane shown, the view shown or edited, and a file saved
#define GUI_PANE_SHOWN 1u
#define GUI_PANE_VIEW 2u
#define GUI_PANE_SAVED 3u

// the row number of the pane itself, off its rows, whose menu is the pane's own
#define GUI_PANE_OFF_ROWS 0xFFFFFFFFu

// A pane: the row the pointer stands on, GUI_PANE_NONE for none; the head's action it stands on, 0 for none; how many
// rows are scrolled off the pane's top; and whether the pane's head has the keys, as it does from a press on its field
// until a press elsewhere or Escape
typedef struct
{
    int pointed;
    unsigned int action;
    unsigned int scroll;
    int keys;
} GuiPane;

// What a row pressed opens: the file's path in the tree, and the place in it where `placed`
typedef struct
{
    const char *path;
    CliPosition place;
    int placed;
} GuiPaneOpen;

// A kind of pane, the one of the tool `tool`: the pane head's title, NULL where the kind draws no pane head of its
// own; how many rows the CLI lists; a row's height in the page's pixels, GUI_PANE_ROW where NULL; the page pixels under
// the head before the rows, none where NULL, drawn by `head_draw` in `rect` cut to `clip`, and the action at x, y in
// them by `head_hit`, 0 for none, run by `action` as `which` for the tree at `root`; the row `row` drawn in `rect` cut
// to `clip`; the row current for `cursor_line`, GUI_PANE_NONE where none or NULL; the row `row` pressed, 1 with what
// it opens into `open_out`; the data read anew on `event` for the tree at `root` and the view `key` at `path`; and
// while the pane has the keys, `length` bytes of `text` typed and the key `name` pressed, each 1 where taken, NULL
// where the kind takes none; and the menu of the row `row`, or of the pane itself where `row` is GUI_PANE_OFF_ROWS,
// built as the menus' own rows, 1 where it has any, NULL where the kind has no menu. An action that runs the tree's
// code saves every view first through `save_all` with `context`; one that takes the keys sets them on `pane`
struct GuiPaneKind
{
    const char *tool;
    const char *(*title)(const GuiPaneCfg *args);
    unsigned int (*rows)(const GuiPaneCfg *args);
    int (*row_height)(const GuiPaneCfg *args);
    int (*head)(const GuiPaneCfg *args);
    void (*head_draw)(const GuiPaneCfg *args);
    unsigned int (*head_hit)(const GuiPaneCfg *args);
    void (*action)(const GuiPaneCfg *args);
    void (*row_draw)(const GuiPaneCfg *args);
    int (*current)(const GuiPaneCfg *args);
    int (*press)(const GuiPaneCfg *args);
    void (*read)(const GuiPaneCfg *args);
    int (*text)(const GuiPaneCfg *args);
    int (*key)(const GuiPaneCfg *args);
    int (*menu)(const GuiPaneCfg *args);
};

// a pane pointing at no row and no action, scrolled to its top
void gui_pane_init(const GuiPaneCfg *args);
// the kind of the tool `tool`, NULL where the tool has no pane
const GuiPaneKind *gui_pane_kind_of(const GuiPaneCfg *args);
// the top of the kind's rows in `frame`'s side, under the group's title, the pane's head and what the kind stands
// under it; the height of the row `row` in pixels; the rectangle of the row `row` as `pane_read` is scrolled; and the
// row at x, y: 1 with its number into `row_pointer`, else 0
int gui_pane_rows_top(const GuiPaneCfg *args);
int gui_pane_row_height(const GuiPaneCfg *args);
GuiRect gui_pane_row_rect(const GuiPaneCfg *args);
int gui_pane_hit(const GuiPaneCfg *args);
// the head's action at x, y, 0 for none
unsigned int gui_pane_head_hit(const GuiPaneCfg *args);
// the pointer moved to x, y: the row and the action it stands on marked: 1 where either changed
int gui_pane_move(const GuiPaneCfg *args);
// the wheel turned `rows` rows, down where positive: the rows scrolled, no further than the last fills the pane: 1
// where the scroll changed
int gui_pane_wheel(const GuiPaneCfg *args);
// the side drawn: its edge and `title`, the group's, at its head; and where `kind` is not NULL its pane: the line, the
// pane's head with the arrow open and the kind's title, what the kind stands under it, and the rows on the editor's
// ground inside the work's inside, the row pointed on the line's ground and the row current on the idle selection's
void gui_pane_draw(const GuiPaneCfg *args);
// a guide down each of `depth` levels from the row `rect`'s left padding, cut to `clip`: the x past them
int gui_pane_guides(const GuiPaneCfg *args);
// an arrow `side` pixels across at x, y, down where `open` and right where not, drawn whole inside `clip` or not at
// all, in `color`
void gui_pane_arrow_draw(const GuiPaneCfg *args);
// the row `row` pressed: 1 with what it opens into `open_out`; the head's action `which` run for the tree at `root`;
// the kind's data read anew on `event`; `length` bytes of `text` typed and the key `name` pressed while `pane` has the
// keys, each 1 where the kind took it; and the menu of the row `row` built as the menus' own rows: 1 where it has any
int gui_pane_press(const GuiPaneCfg *args);
void gui_pane_action(const GuiPaneCfg *args);
void gui_pane_read(const GuiPaneCfg *args);
int gui_pane_text(const GuiPaneCfg *args);
int gui_pane_key(const GuiPaneCfg *args);
int gui_pane_menu(const GuiPaneCfg *args);
struct GuiPaneCfg
{
    GuiPane *pane;
    const GuiPane *pane_read;
    const GuiPaneKind *kind;
    const GuiFrame *frame;
    const char *tool;
    unsigned int row;
    int rows;
    int x;
    int y;
    unsigned int *row_pointer;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    GuiIcons *icons;
    const GuiPalette *palette;
    const char *title;
    const char *tree;
    GuiRect rect;
    GuiRect clip;
    unsigned int depth;
    int side;
    int open;
    GuiColor color;
    unsigned long long cursor_line;
    unsigned int which;
    GuiPaneOpen *open_out;
    const char *root;
    const char *key;
    const char *path;
    unsigned int event;
    void (*save_all)(void *context);
    void *context;
    const char *text;
    unsigned long long length;
    const char *name;
};

// what a kind draws in its pane: `x` pixels of the page at the pane's frame's scale; the width of `length` bytes of
// `text` in `face` at `size` page pixels; and the text drawn from `x_f32`, in `color` faded with the side, its baseline
// centered in `clip` and cut to `clip` inside the pane's clip where `clipped`, else centered in the pane's row and cut
// to the pane's clip: the x after it
int gui_pane_part_scaled(const GuiPanePartCfg *args);
float gui_pane_part_text_width(const GuiPanePartCfg *args);
float gui_pane_part_text_draw(const GuiPanePartCfg *args);
struct GuiPanePartCfg
{
    const GuiPaneCfg *pane;
    int x;
    unsigned int face;
    unsigned int size;
    float x_f32;
    const char *text;
    unsigned long long length;
    GuiColor color;
    GuiRect clip;
    int clipped;
};

typedef struct
{
    int (*scaled)(const GuiPanePartCfg *args);
    float (*text_width)(const GuiPanePartCfg *args);
    float (*text_draw)(const GuiPanePartCfg *args);
} GuiPanePartNs;
EMBED_TABLE_LAYOUT(GuiPanePartNs, scaled, text_width, text_draw);
EMBED_TABLE_STORAGE GuiPanePartNs gui_pane_part EMBED_UNUSED = {
    .scaled = gui_pane_part_scaled,
    .text_width = gui_pane_part_text_width,
    .text_draw = gui_pane_part_text_draw,
};

// what a kind stands in its head: a box `rect` with its corners rounded by `radius` page pixels drawn in `color` with
// its inside, one page pixel in from its border, in `inside`, each cut to the pane's clip; the rectangle of a field in
// the field's row at `y`, inside the row's padding; the field `rect` drawn, on the editor's ground inside a border in
// signal while it has the `keys`, with `length` bytes of `text` or `placeholder` quieter while the text is empty, the
// text cut before `right`, and the caret after the text while it has the keys; and the choice drawn in its row `rect`,
// a box on the line's ground, on the hover's where `pointed`, with `length` bytes of `text`
void gui_pane_box_draw(const GuiPaneFieldCfg *args);
GuiRect gui_pane_field_rect(const GuiPaneFieldCfg *args);
void gui_pane_field_draw(const GuiPaneFieldCfg *args);
void gui_pane_choice_draw(const GuiPaneFieldCfg *args);
struct GuiPaneFieldCfg
{
    const GuiPaneCfg *pane;
    int y;
    GuiRect rect;
    int radius;
    GuiColor color;
    GuiColor inside;
    int right;
    const char *text;
    unsigned long long length;
    const char *placeholder;
    int keys;
    int pointed;
};

typedef struct
{
    void (*box_draw)(const GuiPaneFieldCfg *args);
    GuiRect (*field_rect)(const GuiPaneFieldCfg *args);
    void (*field_draw)(const GuiPaneFieldCfg *args);
    void (*choice_draw)(const GuiPaneFieldCfg *args);
} GuiPaneFieldNs;
EMBED_TABLE_LAYOUT(GuiPaneFieldNs, box_draw, field_rect, field_draw, choice_draw);
EMBED_TABLE_STORAGE GuiPaneFieldNs gui_pane_field EMBED_UNUSED = {
    .box_draw = gui_pane_box_draw,
    .field_rect = gui_pane_field_rect,
    .field_draw = gui_pane_field_draw,
    .choice_draw = gui_pane_choice_draw,
};

typedef struct
{
    void (*init)(const GuiPaneCfg *args);
    const GuiPaneKind *(*kind_of)(const GuiPaneCfg *args);
    int (*rows_top)(const GuiPaneCfg *args);
    int (*row_height)(const GuiPaneCfg *args);
    GuiRect (*row_rect)(const GuiPaneCfg *args);
    int (*hit)(const GuiPaneCfg *args);
    unsigned int (*head_hit)(const GuiPaneCfg *args);
    int (*move)(const GuiPaneCfg *args);
    int (*wheel)(const GuiPaneCfg *args);
    void (*draw)(const GuiPaneCfg *args);
    int (*guides)(const GuiPaneCfg *args);
    void (*arrow_draw)(const GuiPaneCfg *args);
    int (*press)(const GuiPaneCfg *args);
    void (*action)(const GuiPaneCfg *args);
    void (*read)(const GuiPaneCfg *args);
    int (*text)(const GuiPaneCfg *args);
    int (*key)(const GuiPaneCfg *args);
    int (*menu)(const GuiPaneCfg *args);
} GuiPaneNs;
EMBED_TABLE_LAYOUT(GuiPaneNs, init, kind_of, rows_top, row_height, row_rect, hit, head_hit, move, wheel, draw, guides,
                   arrow_draw, press, action, read, text, key, menu);
EMBED_TABLE_STORAGE GuiPaneNs gui_pane EMBED_UNUSED = {
    .init = gui_pane_init,
    .kind_of = gui_pane_kind_of,
    .rows_top = gui_pane_rows_top,
    .row_height = gui_pane_row_height,
    .row_rect = gui_pane_row_rect,
    .hit = gui_pane_hit,
    .head_hit = gui_pane_head_hit,
    .move = gui_pane_move,
    .wheel = gui_pane_wheel,
    .draw = gui_pane_draw,
    .guides = gui_pane_guides,
    .arrow_draw = gui_pane_arrow_draw,
    .press = gui_pane_press,
    .action = gui_pane_action,
    .read = gui_pane_read,
    .text = gui_pane_text,
    .key = gui_pane_key,
    .menu = gui_pane_menu,
};

#endif
