// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature.h: the parameter info panel, as the panels over the editor carry it (servers/servers.h): the label in a box
// over the cursor's row in the code face, the parameter the cursor's argument answers in the type's color and
// underlined, the parameters the tree's check finds the arguments differ from underlined in the fail color, and the
// documentation under it. When it is asked for and when it goes are the panels' (signature.c)
#ifndef GUI_PARTS_SIGNATURE_H
#define GUI_PARTS_SIGNATURE_H
#include "../../text/edits.h"
#include "embed_dispatch_layout.h"
#include "panel.h"

typedef struct GuiSignatureRasterCfg GuiSignatureRasterCfg;
typedef struct GuiSignatureXCfg GuiSignatureXCfg;
typedef struct GuiSignatureCfg GuiSignatureCfg;

// the box's most width and height in the page's pixels, 40rem by 12rem, its padding down and across and the gap between
// the label and the documentation
#define GUI_SIGNATURE_WIDTH 640
#define GUI_SIGNATURE_HEIGHT 192
#define GUI_SIGNATURE_PAD_Y 5
#define GUI_SIGNATURE_PAD_X 10
#define GUI_SIGNATURE_GAP 4

// The signature: the panel shown, NULL for none, the panels' and read alone; and the box's rectangle where it was drawn
// last
typedef struct
{
    const CliSignaturePanel *panel;
    GuiRect rect;
} GuiSignature;

// a signature showing nothing
void gui_signature_init(const GuiSignatureCfg *args);
// whether the pixel at x, y stands in the box shown
int gui_signature_holds(const GuiSignatureXCfg *args);
// the box drawn over the row of the cell `anchor`, the cursor's, inside `bounds` at `scale`, below it where the
// capacity above is wanting: the label in the code face, the active parameter in the type's color underlined, the
// differing parameters underlined in the fail color, and the documentation as Markdown under it; the rectangle recorded
void gui_signature_draw(const GuiSignatureRasterCfg *args);
struct GuiSignatureRasterCfg
{
    GuiSignature *signature;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect bounds;
    int scale;
};

struct GuiSignatureXCfg
{
    const GuiSignature *signature_read;
    int x;
    int y;
};

struct GuiSignatureCfg
{
    GuiSignature *signature;
};

typedef struct
{
    void (*init)(const GuiSignatureCfg *args);
} GuiSignatureNs;
EMBED_TABLE_LAYOUT(GuiSignatureNs, init);
EMBED_TABLE_STORAGE GuiSignatureNs gui_signature EMBED_UNUSED = {
    .init = gui_signature_init,
};

typedef struct
{
    int (*holds)(const GuiSignatureXCfg *args);
} GuiSignatureXNs;
EMBED_TABLE_LAYOUT(GuiSignatureXNs, holds);
EMBED_TABLE_STORAGE GuiSignatureXNs gui_signature_x EMBED_UNUSED = {
    .holds = gui_signature_holds,
};

typedef struct
{
    void (*draw)(const GuiSignatureRasterCfg *args);
} GuiSignatureRasterNs;
EMBED_TABLE_LAYOUT(GuiSignatureRasterNs, draw);
EMBED_TABLE_STORAGE GuiSignatureRasterNs gui_signature_raster EMBED_UNUSED = {
    .draw = gui_signature_draw,
};

#endif
