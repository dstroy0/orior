// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek.h: the change peek over the editor, under the lines of the change it shows, above them where the editor's
// foot cannot fit it: a head with the sentence `Change i of n` and its controls, revert, the change before, the change
// after and close, from the right; and under it a row a line of the change, the lines the last commit had with a minus
// sign in the first mark's color, then the lines now with a plus sign in the second's, in the code face. The peek keeps
// the place of the change it shows and the key it shows it on; the change itself is asked of the store's diff
// (editor/document_changes.h) each time. The box and the controls stand where they were last drawn, which a press reads
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
// what a press on the peek stands on: the box off its controls, revert, the change before, the change after, and close;
// and how many controls stand in the head
#define GUI_CHANGE_PEEK_BOX 0u
#define GUI_CHANGE_PEEK_REVERT 1u
#define GUI_CHANGE_PEEK_PREVIOUS 2u
#define GUI_CHANGE_PEEK_NEXT 3u
#define GUI_CHANGE_PEEK_CLOSE 4u
#define GUI_CHANGE_PEEK_CONTROLS 4u
// the sizes in the page's pixels: the head, a line's row, the padding, a control's side and its icon's, and the head's
// text size; the most rows the box shows; the most units of a line a row shows with the bytes they may take; and the
// milliseconds a change shown stands against a scroll
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
// the peek drawn for the editor `editor_read` drawn in `place` at `scale` with the code cell `cell`, inside `bounds`:
// nothing, and no box standing, where no change is shown or its line is out of the rows drawn; what a press at x, y
// stands on into `action_out`: 1, or 0 off the box; and the box as last drawn, empty where none stands
void gui_change_peek_draw(const GuiChangePeekCfg *args);
int gui_change_peek_hit(const GuiChangePeekCfg *args);
GuiRect gui_change_peek_box(const GuiChangePeekCfg *args);
// the change the line `line` of the text under `key` stands in shown: 1, or 0 where it stands in none; the change `by`
// changes on from the one shown, or from `line` where none is, shown, the ends wrapping, its first marked line into
// `line_out`: 1, or 0 where no change stands; the change shown closed; the key `name` while a change is shown, Escape
// closing it: 1 where taken; whether a scroll moves the view: 1 where no change is shown or GUI_CHANGE_PEEK_SETTLE_MS
// have passed since it was shown; and the edit that puts the change shown's older lines back into `edit_out`, its text
// given back with revert_end once the edit is applied: 1, or 0 with why into `err_msg`
int gui_change_peek_show(const GuiChangePeekCfg *args);
int gui_change_peek_step(const GuiChangePeekCfg *args);
void gui_change_peek_close(const GuiChangePeekCfg *args);
int gui_change_peek_key(const GuiChangePeekCfg *args);
int gui_change_peek_settled(const GuiChangePeekCfg *args);
int gui_change_peek_revert(const GuiChangePeekCfg *args);
void gui_change_peek_revert_end(const GuiChangePeekCfg *args);
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
    CliEdit *edit_out;
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
    void (*revert_end)(const GuiChangePeekCfg *args);
} GuiChangePeekNs;
EMBED_TABLE_LAYOUT(GuiChangePeekNs, draw, hit, box, show, step, close, key, settled, revert, revert_end);
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
    .revert_end = gui_change_peek_revert_end,
};
#endif
