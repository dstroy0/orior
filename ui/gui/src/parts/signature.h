// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature.h: the parameter info panel: an opening bracket or a comma typed asks the CLI, after a short wait, for
// the signature of the call the cursor is in, the tree's and the server's; the label is shown in a box over the
// cursor's row in the code face, the parameter the cursor's argument answers in the type's color and underlined, the
// parameters the tree's check finds the arguments differ from underlined in the fail color, and the documentation
// under it; the box is asked again as the cursor moves while it is shown, and goes as both answers are null, on
// Escape or as the view loses the keys (signature.c)
#ifndef GUI_PARTS_SIGNATURE_H
#define GUI_PARTS_SIGNATURE_H
#include "../../../cli/src/text/json_tree.h"
#include "panel.h"

// the milliseconds between the bracket or comma typed and the ask; the box's most width and height in the page's
// pixels, 40rem by 12rem, its padding down and across and the gap between the label and the documentation; the most
// parameters a label marks; the bytes the label and the documentation are stored in before they grow
#define GUI_SIGNATURE_WAIT 120ull
#define GUI_SIGNATURE_WIDE 640
#define GUI_SIGNATURE_TALL 192
#define GUI_SIGNATURE_PAD_Y 5
#define GUI_SIGNATURE_PAD_X 10
#define GUI_SIGNATURE_GAP 4
#define GUI_SIGNATURE_PARAMS 64u
#define GUI_SIGNATURE_FIRST 2048u

// The signature: whether the box is shown; the label in a cell, `length` bytes; each parameter's span in the label,
// in bytes, how many; the parameter the cursor's argument answers, -1 for none; the parameters the arguments differ
// from, how many; the documentation in a cell, `doc_length` bytes, NULL where none came; how many signatures the
// server offers; when the bracket or comma was typed, 0 where no ask waits; and the box's rectangle where it was drawn
// last
typedef struct
{
    int shown;
    char *label;
    unsigned long long length;
    unsigned long long span[GUI_SIGNATURE_PARAMS][2];
    unsigned int params;
    int active;
    unsigned int differs[GUI_SIGNATURE_PARAMS];
    unsigned int differing;
    char *doc;
    unsigned long long doc_length;
    unsigned int count;
    unsigned long long typed_ms;
    GuiRect rect;
} GuiSignature;

// a signature showing nothing; its cells given back; and the box hidden, its cells given back and no ask waiting
void gui_signature_made(GuiSignature *signature);
void gui_signature_given(GuiSignature *signature);
void gui_signature_hidden(GuiSignature *signature);
// `size` bytes of text typed at `now`: an opening bracket or a comma among them begins the wait: 1 where it did
int gui_signature_typed(GuiSignature *signature, const char *text, unsigned long long size, unsigned long long now);
// the milliseconds until the signature next wants the clock, 0 for never; and the clock come at `now`: 1 where the wait
// is up and the signature is to be asked for, the wait ended
unsigned long long gui_signature_due(const GuiSignature *signature, unsigned long long now);
int gui_signature_ticked(GuiSignature *signature, unsigned long long now);
// the answer `root` of `json` taken, `{"own", "server"}`: the server's `{"label", "params", "active", "doc", "count"}`
// shown where it is one, else the tree's first declaration, `{"label", "params", "differs"}` with the call's `active`;
// both null hides the box: 1, or 0 where a cell could not be taken
int gui_signature_put(GuiSignature *signature, const CliJson *json, unsigned int root);
// whether the pixel at x, y stands in the box shown
int gui_signature_holds(const GuiSignature *signature, int x, int y);
// the box drawn over the row of the cell `anchor`, the cursor's, inside `bounds` at `scale`, below it where the room
// above is wanting: the label in the code face, the active parameter in the type's color underlined, the differing
// parameters underlined in the fail color, and the documentation as Markdown under it; the rectangle recorded
void gui_signature_drawn(GuiSignature *signature, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                         GuiRect anchor, GuiRect bounds, float scale);
#endif
