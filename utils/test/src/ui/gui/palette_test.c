// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette_test.c: the palette's check: the dark scheme's colors are the stylesheet's, a token's name finds its color
// and a name no token has finds the editor's text color
#include "../../../../../ui/gui/src/palette.h"
#include "../cli/cli_check.h"

int main(void)
{
    const GuiPalette *const dark = gui_palette(GUI_SCHEME_DARK);
    cli_check(dark->ground == GUI_RGB(0x13u, 0x13u, 0x31u), "the dark ground is the stylesheet's --bg");
    cli_check(dark->text == GUI_RGB(0xe4u, 0xe3u, 0xf5u), "the dark text is the stylesheet's --fg");
    cli_check(dark->ink == GUI_RGB(0x1du, 0x18u, 0x50u), "the dark ink is the stylesheet's --ink");
    cli_check(dark->number == GUI_RGB(0x5eu, 0x5bu, 0x8au), "the line numbers are the stylesheet's --ed-num");
    cli_check(gui_palette_token(dark, "keyword", 7ull) == GUI_RGB(0xa4u, 0x8cu, 0xffu), "keyword finds --t-keyword");
    cli_check(gui_palette_token(dark, "comment", 7ull) == GUI_RGB(0x6eu, 0x6bu, 0x99u), "comment finds --t-comment");
    cli_check(gui_palette_token(dark, "operator-memory", 15ull) == GUI_RGB(0xf2u, 0x9bu, 0xb0u),
              "a name with a dash finds its color");
    cli_check(gui_palette_token(dark, "keywords", 8ull) == dark->editor_text, "a name no token has is the editor's text");
    cli_check(gui_palette_token(dark, "", 0ull) == dark->editor_text, "an empty name is the editor's text");
    cli_check(GUI_ALPHA_OF(dark->line) < 0xffu, "the line color is translucent");
    return cli_checked();
}
