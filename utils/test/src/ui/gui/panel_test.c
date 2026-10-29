// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// panel_test.c: the panels' shared box and Markdown: the box filled with the widget's ground inside its line, a
// paragraph one line high and two paragraphs two with the margin between, a fenced block a code line a line, a rule
// its margins and line, a long paragraph wrapped in a narrow width, inline code drawn in the type's color, and a box
// placed below its cell where the room allows, above where it does not, and kept inside its bounds
#include "../../../../../ui/gui/src/parts/panel.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` is `color`
static int has(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] == color)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!gui_glyphs_made(&glyphs, &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 400u, 300u), "a raster is made");
    const GuiRect all = {0, 0, 400, 300};
    gui_raster_filled(&raster, all, palette->ground);

    // the box
    gui_panel_box_drawn(&raster, palette, (GuiRect){20, 20, 200, 100}, 1.0f);
    cli_check(raster.pixels[(70u * 400u) + 120u] == palette->surface, "the box is filled with the widget's ground");
    cli_check(raster.pixels[(70u * 400u) + 20u] == palette->widget_line, "the box's left edge is the widget's line");
    cli_check(raster.pixels[(20u * 400u) + 120u] == palette->widget_line, "the box's top edge is the widget's line");
    cli_check(raster.pixels[(20u * 400u) + 20u] == palette->ground, "the box's corner is rounded off to the ground");

    // measured
    const int one = gui_panel_markdown(NULL, &glyphs, palette, "alpha beta", 10ull, 0, 0, 300, 1.0f, all);
    cli_check(one == GUI_PANEL_TEXT_LINE, "a paragraph of two words is one line high");
    const int two = gui_panel_markdown(NULL, &glyphs, palette, "alpha\n\nbeta", 11ull, 0, 0, 300, 1.0f, all);
    cli_check(two == (2 * GUI_PANEL_TEXT_LINE) + GUI_PANEL_PARAGRAPH, "two paragraphs are two lines with the margin between");
    const int joined = gui_panel_markdown(NULL, &glyphs, palette, "alpha\nbeta", 10ull, 0, 0, 300, 1.0f, all);
    cli_check(joined == GUI_PANEL_TEXT_LINE, "two lines with no blank between are one paragraph");
    const int fenced = gui_panel_markdown(NULL, &glyphs, palette, "```\nint a;\nint b;\n```", 21ull, 0, 0, 300, 1.0f, all);
    cli_check(fenced == 2 * GUI_PANEL_CODE_LINE, "a fenced block is a code line a line");
    const int ruled = gui_panel_markdown(NULL, &glyphs, palette, "---", 3ull, 0, 0, 300, 1.0f, all);
    cli_check(ruled == (2 * GUI_PANEL_RULE) + 1, "a rule is its margins and its line");
    static const char s_long[] = "one two three four five six seven eight nine ten eleven twelve thirteen fourteen";
    const int wrapped = gui_panel_markdown(NULL, &glyphs, palette, s_long, sizeof(s_long) - 1u, 0, 0, 120, 1.0f, all);
    cli_check(wrapped >= 3 * GUI_PANEL_TEXT_LINE, "a long paragraph wraps in a narrow width");
    const int wide = gui_panel_markdown(NULL, &glyphs, palette, s_long, sizeof(s_long) - 1u, 0, 0, 2000, 1.0f, all);
    cli_check(wide == GUI_PANEL_TEXT_LINE, "the same paragraph is one line in a wide width");
    const int broken = gui_panel_markdown(NULL, &glyphs, palette, "abcdefghijklmnopqrstuvwxyz", 26ull, 0, 0, 40, 1.0f, all);
    cli_check(broken >= 2 * GUI_PANEL_TEXT_LINE, "a word wider than the width is broken by letters");

    // drawn
    gui_raster_filled(&raster, all, palette->surface);
    const int drawn = gui_panel_markdown(&raster, &glyphs, palette, "see `alpha` here\n\n---\n\n```\ncode\n```", 36ull, 10, 10, 300, 1.0f, all);
    cli_check(drawn == GUI_PANEL_TEXT_LINE + GUI_PANEL_PARAGRAPH + (2 * GUI_PANEL_RULE) + 1 + GUI_PANEL_CODE_LINE,
              "the drawn text is a paragraph, a rule and a code line high");
    const GuiColor type = gui_palette_token(palette, "type", 4ull);
    cli_check(has(&raster, (GuiRect){10, 10, 300, GUI_PANEL_TEXT_LINE}, type), "inline code is drawn in the type's color");
    const int rule_y = 10 + GUI_PANEL_TEXT_LINE + GUI_PANEL_PARAGRAPH + GUI_PANEL_RULE;
    cli_check(raster.pixels[((unsigned int)rule_y * 400u) + 100u] == palette->widget_line, "the rule is drawn in the widget's line");
    cli_check(has(&raster, (GuiRect){10, rule_y + GUI_PANEL_RULE + 1, 300, GUI_PANEL_CODE_LINE}, palette->editor_text),
              "the code line is drawn in the editor's text color");

    // placed
    const GuiRect bounds = {0, 0, 400, 300};
    const GuiRect below = gui_panel_placed((GuiRect){100, 50, 8, 20}, 200, 60, 0, bounds, 0, 1.0f);
    cli_check((below.x == 100) && (below.y == 50 + 20 + GUI_PANEL_OFF), "a box with room below stands under its cell");
    const GuiRect above = gui_panel_placed((GuiRect){100, 270, 8, 20}, 200, 60, 0, bounds, 0, 1.0f);
    cli_check(above.y == 270 - GUI_PANEL_OFF - 60, "a box with no room below stands over its cell");
    const GuiRect preferred = gui_panel_placed((GuiRect){100, 150, 8, 20}, 200, 60, 0, bounds, 1, 1.0f);
    cli_check(preferred.y == 150 - GUI_PANEL_OFF - 60, "a box that prefers above stands over its cell where the room allows");
    const GuiRect clamped = gui_panel_placed((GuiRect){350, 50, 8, 20}, 200, 60, 30, bounds, 0, 1.0f);
    cli_check(clamped.x == 200, "a box is kept inside its bounds at the right");
    const GuiRect left = gui_panel_placed((GuiRect){10, 50, 8, 20}, 200, 60, 30, bounds, 0, 1.0f);
    cli_check(left.x == 0, "a box is kept inside its bounds at the left");
    gui_raster_given(&raster);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
