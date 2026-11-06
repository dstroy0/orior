// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek.h: the change peek over the editor, under the lines of the change the CLI shows (editor/hunks.h), above
// them where the editor's foot cannot fit it: a head with the CLI's sentence over the change and its controls, revert,
// the change before, the change after and close, from the right; and under it a row a line of the change, the lines
// the last commit had with a minus sign in the first mark's color, then the lines now with a plus sign in the second's,
// in the code face. The box and the controls stand where they were last drawn, which a press reads (change_peek.c)
#ifndef GUI_PARTS_CHANGE_PEEK_H
#define GUI_PARTS_CHANGE_PEEK_H
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"
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
// text size; the most rows the box shows; and the most units of a line a row shows with the bytes they may take
#define GUI_CHANGE_PEEK_HEAD 26
#define GUI_CHANGE_PEEK_ROW 20
#define GUI_CHANGE_PEEK_PAD 8
#define GUI_CHANGE_PEEK_CONTROL 20
#define GUI_CHANGE_PEEK_ICON 14
#define GUI_CHANGE_PEEK_TEXT_SIZE 12u
#define GUI_CHANGE_PEEK_ROWS 12u
#define GUI_CHANGE_PEEK_LINE_UNITS 256ULL
#define GUI_CHANGE_PEEK_LINE_BYTES 769u

// the peek drawn for the editor `editor_read` drawn in `place` at `scale` with the code cell `cell`, inside `bounds`:
// nothing, and no box standing, where no change is shown or its line is out of the rows drawn; what a press at x, y
// stands on into `action_out`: 1, or 0 off the box; and the box as last drawn, empty where none stands
void gui_change_peek_draw(const GuiChangePeekCfg *args);
int gui_change_peek_hit(const GuiChangePeekCfg *args);
GuiRect gui_change_peek_box(const GuiChangePeekCfg *args);
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
};

typedef struct
{
    void (*draw)(const GuiChangePeekCfg *args);
    int (*hit)(const GuiChangePeekCfg *args);
    GuiRect (*box)(const GuiChangePeekCfg *args);
} GuiChangePeekNs;
EMBED_TABLE_LAYOUT(GuiChangePeekNs, draw, hit, box);
EMBED_TABLE_STORAGE GuiChangePeekNs gui_change_peek EMBED_UNUSED = {
    .draw = gui_change_peek_draw,
    .hit = gui_change_peek_hit,
    .box = gui_change_peek_box,
};

#endif
