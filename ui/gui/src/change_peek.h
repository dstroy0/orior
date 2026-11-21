// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek.h: the change peek over the editor, under the lines of the change it shows, above them where the editor's
// foot cannot fit it: a head with the sentence `Change i of n` and its controls, revert, the change before, the change
// after and close, from the right; and under it a row a line of the change, the lines the last commit had with a minus
// sign in the first mark's color, then the lines now with a plus sign in the second's, in the code face. The peek
// retains the place of the change it shows and the key it shows it on; the change itself is asked of the store's diff
// (editor/document_changes.h) each time. The box and the controls are where they were last drawn, which a press reads
// (change_peek.c)
#ifndef GUI_WINDOW_CHANGE_PEEK_H
#define GUI_WINDOW_CHANGE_PEEK_H
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/icons.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/editor/document.h"
#include "editor.h"
#include "embed_dispatch_layout.h"
typedef struct GuiChangePeekCfg GuiChangePeekCfg;
// What a press on the peek is on: the box off its controls, revert, the change before, the change after, and close.
// Then how many controls are in the head
#define GUI_CHANGE_PEEK_BOX 0u
#define GUI_CHANGE_PEEK_REVERT 1u
#define GUI_CHANGE_PEEK_PREVIOUS 2u
#define GUI_CHANGE_PEEK_NEXT 3u
#define GUI_CHANGE_PEEK_CLOSE 4u
#define GUI_CHANGE_PEEK_CONTROLS 4u
// The sizes in the page's pixels: the head, a line's row, the padding, a control's side and its icon's. Then the
// head's text size, and the max rows the box shows. Then the max units of a line a row shows, with the bytes they can
// take. Last, the milliseconds a change on display stops a scroll
#define GUI_CHANGE_PEEK_HEAD 26
#define GUI_CHANGE_PEEK_ROW 20
#define GUI_CHANGE_PEEK_PAD 8
#define GUI_CHANGE_PEEK_CONTROL 20
#define GUI_CHANGE_PEEK_ICON 14
#define GUI_CHANGE_PEEK_TEXT_SIZE 12u
#define GUI_CHANGE_PEEK_ROWS 12u
#define GUI_CHANGE_PEEK_LINE_UNITS 256ULL
#define GUI_CHANGE_PEEK_LINE_BYTES 769u
#define GUI_CHANGE_PEEK_SETTLE_MS 300ULL
// The peek drawn for the editor `editor_read` drawn in `place` at `scale` with the code cell `cell`, inside `bounds`.
// It draws nothing and keeps no box where no change is on display, or its line is out of the rows drawn. Then what a
// press at x, y is on, into `action_out`: 1, or 0 off the box. Last, the box as last drawn, empty where none is
void gui_change_peek_draw(const GuiChangePeekCfg *args);
int gui_change_peek_hit(const GuiChangePeekCfg *args);
GuiRect gui_change_peek_box(const GuiChangePeekCfg *args);
// The peek shows the change that the line `line` of the text under `key` is in: 1, or 0 where it is in none. Then it
// shows the change `by` changes on from the one on display, or from `line` where none is, and the ends wrap. Its first
// marked line goes into `line_out`: 1, or 0 where no change is. Then the change on display closed.
//
// Then the key `name` while a change is on display, and Escape closes it: 1 where taken. Then whether a scroll moves
// the view: 1 where no change is on display, or GUI_CHANGE_PEEK_SETTLE_MS passed since it came on display.
//
// Last, the older lines of the change on display put back into the view under `key` as one step. Their text goes into
// a buffer of the call, which goes back before the call returns. Returns 1, or 0 with why into `err_msg`
int gui_change_peek_show(const GuiChangePeekCfg *args);
int gui_change_peek_step(const GuiChangePeekCfg *args);
void gui_change_peek_close(const GuiChangePeekCfg *args);
int gui_change_peek_key(const GuiChangePeekCfg *args);
int gui_change_peek_settled(const GuiChangePeekCfg *args);
int gui_change_peek_revert(const GuiChangePeekCfg *args);
struct GuiChangePeekCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    GuiIcons *icons;
    const GuiPalette *palette;
    const GuiEditor *editor_read;
    GuiRect place;
    GuiRect bounds;
    float cell;
    int scale;
    int x;
    int y;
    unsigned int *action_out;
    const char *key;
    unsigned long long line;
    int by;
    unsigned long long *line_out;
    const char *name;
    VerbaLinea *err_msg;
};
typedef struct
{
    void (*draw)(const GuiChangePeekCfg *args);
    int (*hit)(const GuiChangePeekCfg *args);
    GuiRect (*box)(const GuiChangePeekCfg *args);
    int (*show)(const GuiChangePeekCfg *args);
    int (*step)(const GuiChangePeekCfg *args);
    void (*close)(const GuiChangePeekCfg *args);
    int (*key)(const GuiChangePeekCfg *args);
    int (*settled)(const GuiChangePeekCfg *args);
    int (*revert)(const GuiChangePeekCfg *args);
} GuiChangePeekNs;
EMBED_TABLE_LAYOUT(GuiChangePeekNs, draw, hit, box, show, step, close, key, settled, revert);
EMBED_TABLE_STORAGE GuiChangePeekNs gui_change_peek EMBED_UNUSED = {
    .draw = gui_change_peek_draw,
    .hit = gui_change_peek_hit,
    .box = gui_change_peek_box,
    .show = gui_change_peek_show,
    .step = gui_change_peek_step,
    .close = gui_change_peek_close,
    .key = gui_change_peek_key,
    .settled = gui_change_peek_settled,
    .revert = gui_change_peek_revert,
};
#endif
