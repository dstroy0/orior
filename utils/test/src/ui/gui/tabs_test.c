// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs_test.c: the tabs row's check: a tab whose text has changes not saved is wider by the mark and the gap after it,
// the mark drawn before its name in the hover's color, and a tab whose close waits on a second shows ×? for its close
// mark, which stands wider and still at the tab's right
#include "../../../../../ui/cli/src/view/parts/tabs.h"
#include "../cli/cli_check.h"

#include <math.h>

// whether any pixel of `rect` is `color`
static int colored(const GuiRaster *raster, GuiRect rect, GuiColor color)
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
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    cli_check(frame.tabs.width > 0, "the frame lays a tabs row");
    // the same file's tab as it opened, changed, and changed with its close waiting
    const CliTabRow tabs[3] = {{"src/view.c", "view.c", 0, 0}, {"src/view.c", "view.c", 1, 0}, {"src/view.c", "view.c", 1, 1}};
    const GuiRect plain = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tabs, .count = 3u, .at = 0u);
    const GuiRect changed = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tabs, .count = 3u, .at = 1u);
    const GuiRect waiting = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tabs, .count = 3u, .at = 2u);
    const float mark = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = GUI_TAB_SIZE, .text = "\xe2\x97\x8f", .length = 3u);
    cli_check(mark > 0.0f, "the code face, or the face it falls back on, draws the mark");
    cli_check(changed.width - plain.width == (int)roundf(mark) + GUI_TAB_GAP, "a changed tab is wider by the mark and the gap after it");
    cli_check(changed.x == plain.x + plain.width, "the tabs stand in a row");
    const GuiRect plain_close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tabs, .count = 3u, .at = 1u);
    const GuiRect waiting_close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .tab = tabs, .count = 3u, .at = 2u);
    cli_check(waiting_close.width > plain_close.width, "the close mark waiting on a second, ×?, is wider than ×");
    cli_check((waiting.width - changed.width) == (waiting_close.width - plain_close.width), "the tab grows by what its close mark grows");
    cli_check(waiting_close.x + waiting_close.width + GUI_TAB_PAD_RIGHT == waiting.x + waiting.width, "the close mark stands at the tab's right, its padding after it");

    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u), "the raster is taken");
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .tab = tabs, .glyphs = &glyphs, .frame = &frame, .count = 3u, .raster = &raster, .palette = palette, .active = -1);
    const int left = GUI_TAB_PAD_LEFT;
    const GuiRect plain_mark = {plain.x + left, plain.y, (int)roundf(mark), plain.height};
    const GuiRect changed_mark = {changed.x + left, changed.y, (int)roundf(mark), changed.height};
    cli_check(colored(&raster, changed_mark, palette->hover), "the mark stands before a changed tab's name in the hover's color");
    cli_check(!colored(&raster, plain_mark, palette->hover), "a tab as it opened has no mark");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    return cli_on_check();
}
