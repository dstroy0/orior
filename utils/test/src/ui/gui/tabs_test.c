// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs_test.c: the tabs row's check: each tab is named by the last part of its path and stands after the one before
// it in the row, a pixel on a tab reads as it and one on its close mark as closing it, and the row drawn shows the
// tab shown on its ground with the line along its top
#include "../../../../../ui/gui/src/parts/tabs.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` is not `ground`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
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
    GuiFrame frame;
    gui_frame_laid(&frame, 900u, 400u, 1.0f);
    GuiTab tab[3];
    memor.set(&(MemoriaCfg){.dst = tab, .val = 0, .bytes = sizeof(tab)});
    VerbaLinea first = {tab[0].path, sizeof(tab[0].path), 0ull};
    mmgr_line_put(&first, "ui/gui/src/raster.c");
    (void)mmgr_line_finish(&first);
    VerbaLinea second = {tab[1].path, sizeof(tab[1].path), 0ull};
    mmgr_line_put(&second, "ui\\gui\\src\\glyphs.c");
    (void)mmgr_line_finish(&second);
    VerbaLinea third = {tab[2].path, sizeof(tab[2].path), 0ull};
    mmgr_line_put(&third, "frame.c");
    (void)mmgr_line_finish(&third);
    unsigned long long length = 0ull;
    const char *name = gui_tab_name(&tab[0], &length);
    cli_check((length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "raster.c", .bytes = 8u}) == 0),
              "a tab is named by the last part of its path");
    name = gui_tab_name(&tab[1], &length);
    cli_check((length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "glyphs.c", .bytes = 8u}) == 0),
              "a path with the system's own slashes names the tab the same");
    name = gui_tab_name(&tab[2], &length);
    cli_check((length == 7ull) && (name == tab[2].path), "a bare name is the tab's name whole");

    const GuiRect one = gui_tab_rect(&glyphs, &frame, tab, 3u, 0u);
    const GuiRect two = gui_tab_rect(&glyphs, &frame, tab, 3u, 1u);
    const GuiRect three = gui_tab_rect(&glyphs, &frame, tab, 3u, 2u);
    cli_check((one.x == frame.tabs.x) && (two.x == one.x + one.width) && (three.x == two.x + two.width) &&
                  (one.height == frame.tabs.height - 1),
              "the tabs stand in a row from the row's left, each as tall as the row less its line");
    cli_check(three.width < one.width, "a tab is as wide as its name");
    unsigned int at = 9u;
    int closing = 1;
    cli_check(gui_tabs_hit(&glyphs, &frame, tab, 3u, two.x + 10, two.y + 10, &at, &closing) && (at == 1u) && !closing,
              "a pixel on a tab's name reads as the tab");
    const GuiRect close = gui_tab_close_rect(&glyphs, &frame, tab, 3u, 1u);
    cli_check(gui_tabs_hit(&glyphs, &frame, tab, 3u, close.x + (close.width / 2), close.y + 10, &at, &closing) && (at == 1u) && closing,
              "a pixel on its close mark reads as closing it");
    cli_check(!gui_tabs_hit(&glyphs, &frame, tab, 3u, three.x + three.width + 20, two.y + 10, &at, &closing),
              "a pixel past the last tab is no tab's");
    cli_check(!gui_tabs_hit(&glyphs, &frame, tab, 3u, two.x + 10, frame.editor.y + 10, &at, &closing),
              "a pixel in the editor is no tab's");

    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 900u, 400u), "a raster for the row is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 900, 400}, palette->ground);
    gui_tabs_drawn(&raster, &glyphs, palette, &frame, tab, 3u, 1);
    cli_check(raster.pixels[((unsigned int)two.y * 900u) + (unsigned int)(two.x + 10)] == palette->tab_line,
              "the tab shown has the line along its top");
    cli_check(raster.pixels[((unsigned int)(two.y + 10) * 900u) + (unsigned int)(two.x + 5)] == palette->tab_ground,
              "and its own ground");
    cli_check(raster.pixels[((unsigned int)(one.y + 10) * 900u) + (unsigned int)(one.x + 5)] == palette->surface,
              "a tab not shown keeps the row's ground");
    cli_check(painted(&raster, (GuiRect){one.x + 14, one.y + 6, 20, 18}, palette->surface), "a tab's name is drawn");
    cli_check(painted(&raster, (GuiRect){close.x, close.y + 6, close.width, 18}, palette->tab_ground), "its close mark is drawn");
    gui_raster_given(&raster);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
