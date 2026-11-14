// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview.h: the preview part: the CLI's items drawn in the frame's preview rectangle on the editor's ground, beside
// the text or alone over it as the mode positions the frame, each item at its page pixels times the frame's scale less the
// CLI's scroll top, from the first item the CLI finds for the top to the first block below the rectangle, a thin thumb
// at the right where the items pass the rectangle; the wheel and a press passed to the CLI (preview.c)
#ifndef GUI_WINDOW_PREVIEW_H
#define GUI_WINDOW_PREVIEW_H
#include "../../../includes/window/frame.h"
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/preview/preview.h"
#include "embed_dispatch_layout.h"

typedef struct GuiPreviewCfg GuiPreviewCfg;

// the sizes in the page's pixels: a wheel row's scroll, the scroll thumb's width, its least height and its gap from the
// rectangle's right, and how far a block's mark is out from the block
#define GUI_PREVIEW_ROW 40
#define GUI_PREVIEW_THUMB 4
#define GUI_PREVIEW_THUMB_MIN 16
#define GUI_PREVIEW_THUMB_GAP 2
#define GUI_PREVIEW_MARK_OUT 4

// the preview drawn on `raster` in `frame`'s preview rectangle with `glyphs` in `palette`'s colors, nothing where the
// rectangle is empty; the wheel turned `rows` rows over it, down where positive, passed to the CLI's scroll: 1 where
// the top moved; and a press at x, y: off the rectangle nothing, on it the CLI's answer to the press at the page's
// pixels from its top left, where it goes into `open_out`: the target, CLI_PREVIEW_OPEN_NONE for none
void gui_preview_draw(const GuiPreviewCfg *args);
int gui_preview_wheel(const GuiPreviewCfg *args);
unsigned int gui_preview_press(const GuiPreviewCfg *args);
struct GuiPreviewCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    const GuiFrame *frame;
    int rows;
    int x;
    int y;
    CliPreviewOpen *open_out;
};

typedef struct
{
    void (*draw)(const GuiPreviewCfg *args);
    int (*wheel)(const GuiPreviewCfg *args);
    unsigned int (*press)(const GuiPreviewCfg *args);
} GuiPreviewNs;
EMBED_TABLE_LAYOUT(GuiPreviewNs, draw, wheel, press);
EMBED_TABLE_STORAGE GuiPreviewNs gui_preview EMBED_UNUSED = {
    .draw = gui_preview_draw,
    .wheel = gui_preview_wheel,
    .press = gui_preview_press,
};

#endif
