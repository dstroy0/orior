// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick.h: the quick open's box near the top of the work, over every part: its field with the text typed and the
// caret, and under it the rows as the CLI lists them (editor/quick.h), a row's label with the characters that matched
// marked, its detail quieter after it and a command's keys at its right end, a heading quiet and bold, a sentence
// quiet, the chosen row on the idle selection's ground and the pointed on the line's. Every key, text, press and wheel
// goes to it first while it is open, and the keys of its commands open it (quick.c)
#ifndef GUI_WINDOW_QUICK_H
#define GUI_WINDOW_QUICK_H
#include "../../../includes/window/frame.h"
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/editor/quick.h"
#include "embed_dispatch_layout.h"
#include "thread.h"

typedef struct GuiQuickCfg GuiQuickCfg;

// the sizes in the page's pixels: the box's max width, its gap under the work's top, its least margin at either
// side, the field's row, a row, the max rows displayed, the padding inside the box and the field, the gap between a
// label and its detail, the corners' radius, the caret's width, the text sizes of the field, a label, a detail and
// keys, and the rows PageUp and PageDown step
#define GUI_QUICK_WIDTH 640
#define GUI_QUICK_TOP 8
#define GUI_QUICK_MARGIN 16
#define GUI_QUICK_FIELD 36
#define GUI_QUICK_ROW 24
#define GUI_QUICK_ROWS 12u
#define GUI_QUICK_PAD 6
#define GUI_QUICK_GAP 10
#define GUI_QUICK_RADIUS 6
#define GUI_QUICK_CARET 2
#define GUI_QUICK_FIELD_SIZE 13u
#define GUI_QUICK_LABEL_SIZE 13u
#define GUI_QUICK_DETAIL_SIZE 12u
#define GUI_QUICK_KEYS_SIZE 11u
#define GUI_QUICK_PAGE 10

// the box's rectangle in `frame` for the rows the CLI lists, empty while the quick open is closed; and how many rows
// the box shows at max in the frame
GuiRect gui_quick_box(const GuiQuickCfg *args);
unsigned int gui_quick_displayed(const GuiQuickCfg *args);
// the box drawn into `raster` with `glyphs` in `palette`'s colors, the row `pointed` on the line's ground, none where
// it is negative
void gui_quick_draw(const GuiQuickCfg *args);
// what the pixel at x, y is on while the box is open, the row's place into `index_out`: CLI_HIT_QUICK_ROW,
// CLI_HIT_QUICK_BOX, CLI_HIT_QUICK_OUTSIDE, or CLI_HIT_NONE while it is closed
unsigned int gui_quick_hit(const GuiQuickCfg *args);
// the key `name` for the tree at `root` and the view displayed under `key` at `now_ms`: while open, Up and Down step the
// chosen row, PageUp and PageDown GUI_QUICK_PAGE rows, Enter chooses it into `choice_out`, Escape closes the box, and
// every other key goes to the typed text; while closed, the keys of a command of the quick open open it: 1 where the
// key was taken. `length` bytes of `text` typed while open: 1 where they were. The row `at` pressed, chosen into
// `choice_out`: 1 where it opens something. And the wheel turned `rows` rows over the open box, the rows scrolled: 1
// where it was open
int gui_quick_key(const GuiQuickCfg *args);
int gui_quick_text(const GuiQuickCfg *args);
int gui_quick_press(const GuiQuickCfg *args);
int gui_quick_wheel(const GuiQuickCfg *args);
struct GuiQuickCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    const GuiFrame *frame;
    int x;
    int y;
    int rows;
    int pointed;
    unsigned int at;
    unsigned int *index_out;
    const char *name;
    const char *text;
    unsigned long long length;
    const char *root;
    const char *key;
    unsigned long long now_ms;
    CliQuickChoice *choice_out;
};

typedef struct
{
    GuiRect (*box)(const GuiQuickCfg *args);
    unsigned int (*displayed)(const GuiQuickCfg *args);
    void (*draw)(const GuiQuickCfg *args);
    unsigned int (*hit)(const GuiQuickCfg *args);
    int (*key)(const GuiQuickCfg *args);
    int (*text)(const GuiQuickCfg *args);
    int (*press)(const GuiQuickCfg *args);
    int (*wheel)(const GuiQuickCfg *args);
} GuiQuickNs;
EMBED_TABLE_LAYOUT(GuiQuickNs, box, displayed, draw, hit, key, text, press, wheel);
EMBED_TABLE_STORAGE GuiQuickNs gui_quick EMBED_UNUSED = {
    .box = gui_quick_box,
    .displayed = gui_quick_displayed,
    .draw = gui_quick_draw,
    .hit = gui_quick_hit,
    .key = gui_quick_key,
    .text = gui_quick_text,
    .press = gui_quick_press,
    .wheel = gui_quick_wheel,
};

#endif
