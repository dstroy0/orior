// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette_test.c: the palette's check: the dark scheme's colors are the stylesheet's, a token's name finds its color
// and a name no token has finds the editor's text color
#include "../../../../../ui/cli/src/view/palette.h"
#include "../cli/cli_check.h"

// the named color called `required`: whether one has the name and its color is `color`
static int named_is(const GuiPalette *palette, const char *required, GuiColor color)
{
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[64];
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = palette, .at = at, .name = name, .size = sizeof(name));
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = required, .cap = sizeof(name)}))
        {
            return EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette, .at = at) == color;
        }
    }
    return 0;
}

// whether every named color's name differs from every other's
static int named_differ(const GuiPalette *palette)
{
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[64];
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = palette, .at = at, .name = name, .size = sizeof(name));
        for (unsigned int other = at + 1u; other < count; other += 1u)
        {
            char other_name[64];
            EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = palette, .at = other, .name = other_name, .size = sizeof(other_name));
            if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = other_name, .cap = sizeof(name)}))
            {
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    const GuiPalette *const dark = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    cli_check(dark->background_fill == GUI_RGB(0x13u, 0x13u, 0x31u), "the dark ground is the stylesheet's --bg");
    cli_check(dark->text == GUI_RGB(0xe4u, 0xe3u, 0xf5u), "the dark text is the stylesheet's --fg");
    cli_check(dark->text_color == GUI_RGB(0x1du, 0x18u, 0x50u), "the dark ink is the stylesheet's --ink");
    cli_check(dark->number == GUI_RGB(0x5eu, 0x5bu, 0x8au), "the line numbers are the stylesheet's --ed-num");
    // a palette the window can change: its colors as the scheme's until they are changed, each named color found by the
    // stylesheet's name, the ink's tint and the editor's ground laid at their opacities
    static GuiPaletteOwn own;
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &own, .scheme = GUI_SCHEME_DARK);
    cli_check((own.colors.glass == dark->glass) && (own.colors.token == own.token) && (own.token[0].color == dark->token[0].color), "a palette of its own starts as the scheme's");
    EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &own, .bar_percent = 30u, .editor_percent = 80u);
    cli_check((own.colors.glass == dark->glass) && (own.colors.editor_background == dark->editor_background), "the opacities the scheme stands at lay the colors the scheme gives");
    EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &own, .bar_percent = 50u, .editor_percent = 100u);
    cli_check((GUI_ALPHA_OF(own.colors.glass) == 128u) && (GUI_ALPHA_OF(own.colors.editor_background) == 255u), "other opacities lay the alpha");
    cli_check((own.colors.glass & 0x00ffffffu) == (dark->text_color & 0x00ffffffu), "and leave the color");
    char name[32];
    EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = &own.colors, .at = 0u, .name = name, .size = sizeof(name));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "window.background", .cap = sizeof(name)}) && (EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = &own.colors, .at = 0u) == dark->background_fill), "the first color named is the window's background");
    // each color goes by the name of what it colors
    cli_check(named_differ(dark), "no two colors share a name");
    cli_check(named_is(dark, "lattice.background", dark->deep), "what the lattice is drawn on");
    cli_check(named_is(dark, "widget.background", dark->surface) && named_is(dark, "widget.border", dark->widget_line) && named_is(dark, "widget.shadow", dark->lifted), "a widget's ground, line and shadow");
    cli_check(named_is(dark, "input.background", dark->well), "an input's ground");
    cli_check(named_is(dark, "text.primary", dark->text) && named_is(dark, "text.secondary", dark->text_2) && named_is(dark, "text.tertiary", dark->text_3), "the text at its three strengths");
    cli_check(named_is(dark, "divider", dark->line) && named_is(dark, "accent", dark->signal), "the hairline and the accent");
    cli_check(named_is(dark, "link.foreground", dark->link) && named_is(dark, "hover.foreground", dark->hover) && named_is(dark, "failure.foreground", dark->fail), "the link, what the pointer lights and a failure");
    cli_check(named_is(dark, "glass.tint", dark->text_color), "the tint of the panes");
    cli_check(named_is(dark, "titleBar.foreground", dark->bar_text) && named_is(dark, "menubar.foreground", dark->bar_title), "the bar's text and its menu titles");
    cli_check(named_is(dark, "toolbar.hoverBackground", dark->bar_title_hover) && named_is(dark, "toolbar.hoverForeground", dark->bar_bright), "a pointed title or tool");
    cli_check(named_is(dark, "breadcrumb.separator", dark->bar_path) && named_is(dark, "statusBar.foreground", dark->status_text), "the status bar's text and the marks between its crumbs");
    cli_check(named_is(dark, "wordmark.foreground", dark->open_text) && named_is(dark, "wordmark.eye", dark->bar_mark), "the large name and the eye");
    cli_check(named_is(dark, "editor.background", dark->editor_background) && named_is(dark, "editor.foreground", dark->editor_text), "the editor's ground and text");
    cli_check(named_is(dark, "editor.lineNumber", dark->number) && named_is(dark, "editor.lineHighlight", dark->current), "the line numbers and the current line");
    cli_check(named_is(dark, "editor.selection", dark->selection) && named_is(dark, "editor.inactiveSelection", dark->selection_idle), "the selection, with the keys and without them");
    cli_check(named_is(dark, "editor.cursor", dark->caret) && named_is(dark, "editor.indentGuide", dark->guide) && named_is(dark, "editor.foldBackground", dark->same), "the cursor, the indent guide and a closed fold's mark");
    cli_check(named_is(dark, "editor.findMatch", dark->match_on) && named_is(dark, "editor.findMatchHighlight", dark->match), "the current match and the others");
    cli_check(named_is(dark, "minimap.selection", dark->map_selection) && named_is(dark, "minimap.findMatch", dark->map_match) && named_is(dark, "minimap.slider", dark->map_slider) && named_is(dark, "minimap.sliderActive", dark->map_slider_on), "the minimap's");
    cli_check(named_is(dark, "diagnostic.error", dark->error) && named_is(dark, "diagnostic.warning", dark->warning) && named_is(dark, "diagnostic.information", dark->note), "a diagnostic's severities");
    cli_check(named_is(dark, "tab.activeBackground", dark->tab_background), "the shown tab's ground");
    cli_check(named_is(dark, "fuse.ash", dark->fuse_ash) && named_is(dark, "fuse.burn", dark->fuse_burn) && named_is(dark, "fuse.fire", dark->fuse_fire), "the busy circle's fuse");
    cli_check(named_is(dark, "lattice.inPlace", dark->lattice[0]) && named_is(dark, "lattice.moving", dark->lattice[1]) && named_is(dark, "lattice.displaced", dark->lattice[2]), "a lattice point in place and moved");
    cli_check(named_is(dark, "fileIcon.red", dark->mark[0]) && named_is(dark, "fileIcon.green", dark->mark[1]) && named_is(dark, "fileIcon.yellow", dark->mark[2]) && named_is(dark, "fileIcon.blue", dark->mark[3]) &&
                  named_is(dark, "fileIcon.violet", dark->mark[4]) && named_is(dark, "fileIcon.cyan", dark->mark[5]) && named_is(dark, "fileIcon.white", dark->mark[6]) && named_is(dark, "fileIcon.gray", dark->mark[7]),
              "a file's icon, each by its hue");
    const unsigned int named = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = &own.colors);
    cli_check(named > own.colors.token_count, "the named colors count the tokens among them");
    EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = &own.colors, .at = named - own.colors.token_count, .name = name, .size = sizeof(name));
    cli_check((name[0] == 't') && (name[5] == '.') && (EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = &own.colors, .at = named - own.colors.token_count) == own.token[0].color), "a token goes by the name its grammar gives it");
    EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &own, .at = 0u, .color = GUI_RGB(1u, 2u, 3u));
    cli_check(own.colors.background_fill == GUI_RGB(1u, 2u, 3u), "a color named is set");
    EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &own, .at = named - 1u, .color = GUI_RGB(4u, 5u, 6u));
    cli_check(own.token[own.colors.token_count - 1u].color == GUI_RGB(4u, 5u, 6u), "and a token");
    cli_check(dark->background_fill == GUI_RGB(0x13u, 0x13u, 0x31u), "and the scheme's own stands");
    // a value as text and read back
    char value[16];
    EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = GUI_RGB(0x12u, 0xa4u, 0xefu), .text = value, .size = sizeof(value));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "#12a4ef", .cap = sizeof(value)}), "an opaque color is written in six digits");
    EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = GUI_RGBA(0x12u, 0xa4u, 0xefu, 0x33u), .text = value, .size = sizeof(value));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "#12a4ef33", .cap = sizeof(value)}), "a translucent one in eight");
    GuiColor read_color = 0u;
    cli_check(EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = "#12A4ef", .length = 7ull, .current = GUI_RGBA(0u, 0u, 0u, 0x40u), .color_pointer = &read_color) && (read_color == GUI_RGBA(0x12u, 0xa4u, 0xefu, 0x40u)), "six digits are read under the alpha kept");
    cli_check(EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = "12a4ef33", .length = 8ull, .current = 0u, .color_pointer = &read_color) && (read_color == GUI_RGBA(0x12u, 0xa4u, 0xefu, 0x33u)), "eight with their own, with or without the mark");
    cli_check(!EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = "#12a4e", .length = 6ull, .current = 0u, .color_pointer = &read_color) && !EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = "#12a4eg", .length = 7ull, .current = 0u, .color_pointer = &read_color) && !EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = "", .length = 0ull, .current = 0u, .color_pointer = &read_color), "and what is no color is refused");
    // contrast in whole tenths of a ratio, rounded down, as WCAG 2 measures it
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = white, .back = black, .background_fill = black) == 210u, "white on black is 21:1");
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = black, .back = white, .background_fill = white) == 210u, "and black on white the same");
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = black, .back = black, .background_fill = black) == 10u, "a color on itself is 1:1");
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = GUI_RGB(0x77u, 0x77u, 0x77u), .back = white, .background_fill = white) == 44u, "#777777 on white is 4.47:1, under the least");
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u), .back = black, .background_fill = black) == 53u, "a translucent color is laid over what is under it");
    cli_check(EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = dark->editor_text, .back = dark->editor_background, .background_fill = dark->background_fill) == 142u, "a translucent ground is laid over the window's");
    // the colors checked for text, each named with the color under it
    unsigned int tenths = 0u;
    char under[32];
    const unsigned int colors_named = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = dark);
    unsigned int number_at = colors_named;
    unsigned int comment_at = colors_named;
    unsigned int foreground_at = colors_named;
    unsigned int low_count = 0u;
    for (unsigned int at = 0u; at < colors_named; at += 1u)
    {
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = dark, .at = at, .name = name, .size = sizeof(name));
        number_at = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "editor.lineNumber", .cap = sizeof(name)}) ? at : number_at;
        comment_at = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "token.comment", .cap = sizeof(name)}) ? at : comment_at;
        foreground_at = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "editor.foreground", .cap = sizeof(name)}) ? at : foreground_at;
        low_count += (unsigned int)EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = dark, .at = at, .tenths = &tenths, .under = under, .size = sizeof(under));
    }
    cli_check(low_count == 2u, "two of the dark scheme's colors are under the least");
    cli_check(EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = dark, .at = number_at, .tenths = &tenths, .under = under, .size = sizeof(under)) && (tenths == 28u) && cellul.eq(&(CatenaFinitaCfg){.src = under, .other = "editor.background", .cap = sizeof(under)}), "the line numbers are 2.8:1 on the editor's background");
    cli_check(EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = dark, .at = comment_at, .tenths = &tenths, .under = under, .size = sizeof(under)) && (tenths == 36u) && cellul.eq(&(CatenaFinitaCfg){.src = under, .other = "editor.background", .cap = sizeof(under)}), "a comment is 3.6:1 on the editor's background");
    cli_check(!EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = dark, .at = foreground_at, .tenths = &tenths, .under = under, .size = sizeof(under)), "the editor's text is over the least");
    cli_check(!EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = dark, .at = 0u, .tenths = &tenths, .under = under, .size = sizeof(under)), "a color that is no text is not checked");
    static GuiPaletteOwn dim;
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &dim, .scheme = GUI_SCHEME_DARK);
    dim.colors.text_2 = GUI_RGB(0x20u, 0x20u, 0x40u);
    unsigned int secondary_at = colors_named;
    for (unsigned int at = 0u; at < colors_named; at += 1u)
    {
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = &dim.colors, .at = at, .name = name, .size = sizeof(name));
        secondary_at = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "text.secondary", .cap = sizeof(name)}) ? at : secondary_at;
    }
    cli_check(EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = &dim.colors, .at = secondary_at, .tenths = &tenths, .under = under, .size = sizeof(under)) && cellul.eq(&(CatenaFinitaCfg){.src = under, .other = "window.background", .cap = sizeof(under)}), "a color made dim goes under the least on the window's background");
    char ratio[16];
    EMBED_CALL(gui_palette_size.contrast_text, GuiPaletteSizeCfg, .tenths_value = 28u, .text = ratio, .size = sizeof(ratio));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = ratio, .other = "2.8:1", .cap = sizeof(ratio)}), "a contrast reads as its ratio");
    EMBED_CALL(gui_palette_size.contrast_text, GuiPaletteSizeCfg, .tenths_value = 210u, .text = ratio, .size = sizeof(ratio));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = ratio, .other = "21.0:1", .cap = sizeof(ratio)}), "and the most");
    cli_check((dark->fuse_ash == GUI_RGB(50u, 18u, 98u)) && (dark->fuse_burn == GUI_RGB(94u, 40u, 166u)) && (dark->fuse_fire == GUI_RGB(148u, 94u, 232u)),
              "a fuse's colors are the stylesheet's --fuse-ash, --fuse-burn and --fuse-fire");
    cli_check(EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = dark, .name_read = "keyword", .length = 7ull) == GUI_RGB(0xa4u, 0x8cu, 0xffu), "keyword finds --t-keyword");
    cli_check(EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = dark, .name_read = "comment", .length = 7ull) == GUI_RGB(0x6eu, 0x6bu, 0x99u), "comment finds --t-comment");
    cli_check(EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = dark, .name_read = "operator-memory", .length = 15ull) == GUI_RGB(0xf2u, 0x9bu, 0xb0u),
              "a name with a dash finds its color");
    cli_check(EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = dark, .name_read = "keywords", .length = 8ull) == dark->editor_text, "a name no token has is the editor's text");
    cli_check(EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = dark, .name_read = "", .length = 0ull) == dark->editor_text, "an empty name is the editor's text");
    cli_check(GUI_ALPHA_OF(dark->line) < 0xffu, "the line color is translucent");
    cli_check((dark->lattice[0] == GUI_RGB(0x6fu, 0xdcu, 0xb4u)) && (dark->lattice[2] == GUI_RGB(0xb4u, 0x9cu, 0xffu)),
              "the lattice's colors are the stylesheet's --lattice-1 to --lattice-3");
    cli_check(dark->open_text == GUI_RGB(0xd7u, 0xd6u, 0xdfu), "the large name stands on the ladder's rung of 86, under the text's 91");
    cli_check(dark->bar_text == GUI_RGB(0xd7u, 0xd6u, 0xdfu), "the bar's resting text stands on the rung of 86, under the text's 91");
    cli_check(dark->bar_bright == GUI_RGB(0xe5u, 0xe4u, 0xedu), "what the pointer lights stands on the rung of 91, under the operators' 100");
    cli_check(dark->bar_mark == GUI_RGB(0xdfu, 0xe0u, 0xf0u), "the eye in the bar is the stylesheet's --bar-mark");
    cli_check(dark->lifted == GUI_RGBA(0x08u, 0x06u, 0x1eu, 0x73u), "the shade is the stylesheet's --lifted");
    cli_check(dark->incised == GUI_RGB(0x11u, 0x10u, 0x2bu), "the cuts are --deep at 80% with black, its lightness by 1.15");
    return cli_on_check();
}
