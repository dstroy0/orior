// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs_test.c: the tabs row's check: each tab is named by the last part of its path and stands after the one before
// it in the row, a pixel on a tab reads as it and one on its close mark as closing it, and the row drawn shows the
// tab shown on its ground with the line along its top
#include "../../../../../ui/gui/src/base.h"
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
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 900u, .height = 400u, .scale = 100);
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
    const char *name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .tab = &tab[0], .length = &length);
    cli_check((length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "raster.c", .bytes = 8u}) == 0),
              "a tab is named by the last part of its path");
    name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .tab = &tab[1], .length = &length);
    cli_check((length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "glyphs.c", .bytes = 8u}) == 0),
              "a path with the system's own slashes names the tab the same");
    name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .tab = &tab[2], .length = &length);
    cli_check((length == 7ull) && (name == tab[2].path), "a bare name is the tab's name whole");

    const GuiRect one = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .at = 0u);
    const GuiRect two = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .at = 1u);
    const GuiRect three = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .at = 2u);
    cli_check((one.x == frame.tabs.x) && (two.x == one.x + one.width) && (three.x == two.x + two.width) &&
                  (one.height == frame.tabs.height - 1),
              "the tabs stand in a row from the row's left, each as tall as the row less its line");
    cli_check(three.width < one.width, "a tab is as wide as its name");
    unsigned int at = 9u;
    int closing = 1;
    cli_check(EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .x = two.x + 10, .y = two.y + 10, .at_pointer = &at, .closing = &closing) && (at == 1u) && !closing,
              "a pixel on a tab's name reads as the tab");
    const GuiRect close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .at = 1u);
    cli_check(EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .x = close.x + (close.width / 2), .y = close.y + 10, .at_pointer = &at, .closing = &closing) && (at == 1u) && closing,
              "a pixel on its close mark reads as closing it");
    cli_check(!EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .x = three.x + three.width + 20, .y = two.y + 10, .at_pointer = &at, .closing = &closing),
              "a pixel past the last tab is no tab's");
    cli_check(!EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &glyphs, .frame = &frame, .tab = tab, .count = 3u, .x = two.x + 10, .y = frame.editor.y + 10, .at_pointer = &at, .closing = &closing),
              "a pixel in the editor is no tab's");

    GuiRaster raster;
    GuiBase base;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 900u, .height = 400u) && EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 900u, .height = 400u), "a raster for the row and the base are made");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = &raster, .glyphs = &glyphs, .palette = palette, .frame = &frame, .tab = tab, .count = 3u, .active = 1);
    cli_check(raster.pixels[((unsigned int)two.y * 900u) + (unsigned int)(two.x + 10)] == palette->tab_line,
              "the tab shown has the line along its top");
    cli_check(raster.pixels[((unsigned int)(two.y + 10) * 900u) + (unsigned int)(two.x + 5)] == palette->tab_ground,
              "and its own ground");
    cli_check(raster.pixels[((unsigned int)(one.y + 10) * 900u) + (unsigned int)(one.x + 5)] == palette->surface,
              "a tab not shown keeps the row's ground");
    cli_check(painted(&raster, (GuiRect){one.x + 14, one.y + 6, 20, 18}, palette->surface), "a tab's name is drawn");
    cli_check(painted(&raster, (GuiRect){close.x, close.y + 6, close.width, 18}, palette->tab_ground), "its close mark is drawn");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &base);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
