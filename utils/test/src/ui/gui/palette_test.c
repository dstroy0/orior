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
    cli_check((dark->fuse_ash == GUI_RGB(50u, 18u, 98u)) && (dark->fuse_burn == GUI_RGB(94u, 40u, 166u)) && (dark->fuse_fire == GUI_RGB(148u, 94u, 232u)),
              "a fuse's colors are the stylesheet's --fuse-ash, --fuse-burn and --fuse-fire");
    cli_check(gui_palette_token(dark, "keyword", 7ull) == GUI_RGB(0xa4u, 0x8cu, 0xffu), "keyword finds --t-keyword");
    cli_check(gui_palette_token(dark, "comment", 7ull) == GUI_RGB(0x6eu, 0x6bu, 0x99u), "comment finds --t-comment");
    cli_check(gui_palette_token(dark, "operator-memory", 15ull) == GUI_RGB(0xf2u, 0x9bu, 0xb0u),
              "a name with a dash finds its color");
    cli_check(gui_palette_token(dark, "keywords", 8ull) == dark->editor_text, "a name no token has is the editor's text");
    cli_check(gui_palette_token(dark, "", 0ull) == dark->editor_text, "an empty name is the editor's text");
    cli_check(GUI_ALPHA_OF(dark->line) < 0xffu, "the line color is translucent");
    cli_check((dark->lattice[0] == GUI_RGB(0x6fu, 0xdcu, 0xb4u)) && (dark->lattice[2] == GUI_RGB(0xb4u, 0x9cu, 0xffu)),
              "the lattice's colors are the stylesheet's --lattice-1 to --lattice-3");
    cli_check(dark->open_text == GUI_RGB(0xd7u, 0xd6u, 0xdfu), "the large name stands on the ladder's rung of 86, under the text's 91");
    cli_check(dark->bar_text == GUI_RGB(0xd7u, 0xd6u, 0xdfu), "the bar's resting text stands on the rung of 86, under the text's 91");
    cli_check(dark->bar_bright == GUI_RGB(0xe5u, 0xe4u, 0xedu), "what the pointer lights stands on the rung of 91, under the operators' 100");
    cli_check(dark->bar_mark == GUI_RGB(0xdfu, 0xe0u, 0xf0u), "the eye in the bar is the stylesheet's --bar-mark");
    cli_check(dark->lifted == GUI_RGBA(0x08u, 0x06u, 0x1eu, 0x73u), "the shade is the stylesheet's --lifted");
    cli_check(dark->incised == GUI_RGB(0x11u, 0x10u, 0x2bu), "the cuts are --deep at 80% with black, its lightness by 1.15");
    return cli_checked();
}
