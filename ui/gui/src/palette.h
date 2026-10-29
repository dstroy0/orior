// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette.h: the colors the window draws in, one palette a scheme, each the colors style.css names for it: the
// grounds, the texts, the lines and signals, the bar's and the status bar's, the editor's, and a token's color by the
// name its grammar gives it (palette.c)
#ifndef GUI_PALETTE_H
#define GUI_PALETTE_H
#include "raster.h"

// the schemes: the dark one; and the colors a file's mark in the explorer is drawn in, --t1 to --t8
#define GUI_SCHEME_DARK 0u
#define GUI_SCHEMES 1u
#define GUI_MARKS 8u

// A token's color: the name its grammar gives it, and the color
typedef struct
{
    const char *name;
    GuiColor color;
} GuiTokenColor;

// A scheme's colors, named as style.css names them: the window's ground, surface, well and deep; its text at three
// strengths; its hairline; the signal, link, hover and fail colors; the ink the bar, the strip and the status bar are
// tinted with and the tint itself, the ink at its opacity; the bar's text, its titles' text, the ground of a title
// pointed or open and its text then, a path's text on the bars, and the status bar's text; the editor's ground, text, line numbers, current
// line, selection, the selection's ground where the keys are elsewhere, caret, indent guide, the ground of a mark
// among the text, a fold's, the line of a widget over the text and the grounds of find's matches, the current one's
// stronger; the shown tab's ground and the line along it; the ground of the strip's tool whose window shows, the
// signal at a share; the side pane's ground, the ink at half; the eight colors a file's mark takes; and its tokens,
// `token_count` of them
typedef struct
{
    GuiColor ground;
    GuiColor surface;
    GuiColor well;
    GuiColor deep;
    GuiColor text;
    GuiColor text_2;
    GuiColor text_3;
    GuiColor line;
    GuiColor signal;
    GuiColor link;
    GuiColor hover;
    GuiColor fail;
    GuiColor ink;
    GuiColor glass;
    GuiColor bar_text;
    GuiColor bar_title;
    GuiColor bar_title_hover;
    GuiColor bar_bright;
    GuiColor bar_path;
    GuiColor status_text;
    GuiColor editor_ground;
    GuiColor editor_text;
    GuiColor number;
    GuiColor current;
    GuiColor selection;
    GuiColor selection_idle;
    GuiColor caret;
    GuiColor guide;
    GuiColor same;
    GuiColor widget_line;
    GuiColor match;
    GuiColor match_on;
    GuiColor tab_ground;
    GuiColor tab_line;
    GuiColor strip_on;
    GuiColor side_ground;
    GuiColor mark[GUI_MARKS];
    const GuiTokenColor *token;
    unsigned int token_count;
} GuiPalette;

// the palette of `scheme`, the dark one for any scheme not known
const GuiPalette *gui_palette(unsigned int scheme);
// the color of the token named by `length` bytes of `name`, the editor's text color where no token has the name
GuiColor gui_palette_token(const GuiPalette *palette, const char *name, unsigned long long length);
#endif
