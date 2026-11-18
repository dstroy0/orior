// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark_test.c: the wordmark's check: made with no eye home, the eye lands home, the tree read is told, the eye's
// square in the bar has a size once the eye is home and the eye is drawn on it, and TABVLA's way down and opacity stay
// in their ranges
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/icons.h"
#include "../../../../../includes/window/palette.h"
#include "../../../../../includes/window/wordmark.h"
#include "../cli/cli_check.h"

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    static GuiIcons s_icons;
    EMBED_CALL(gui_icons.init, GuiIconsCfg, .icons = &s_icons);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
              "the raster is taken");

    static GuiWordmark s_wordmark;
    EMBED_CALL(gui_wordmark_still_table.init, GuiWordmarkStillCfg, .wordmark = &s_wordmark, .still = 0);
    cli_check(!s_wordmark.home && !s_wordmark.tree_read, "a wordmark made has no eye home and no tree read");
    int down = -1;
    unsigned int opacity = 999u;
    EMBED_CALL(gui_wordmark_read.under_at, GuiWordmarkReadCfg, .now_ms = 1000ULL, .wordmark_read = &s_wordmark,
               .down = &down, .opacity = &opacity);
    cli_check((down >= 0) && (opacity <= 255u), "TABVLA's way down and its opacity are in their ranges");
    EMBED_CALL(gui_wordmark_still_table.tree_read, GuiWordmarkStillCfg, .wordmark = &s_wordmark, .still = 0);
    cli_check(s_wordmark.tree_read, "the tree read is told to the wordmark");
    EMBED_CALL(gui_wordmark_still_table.land, GuiWordmarkStillCfg, .wordmark = &s_wordmark, .still = 0);
    cli_check(s_wordmark.home, "the eye lands home");

    const GuiRect square = EMBED_CALL(gui_wordmark_frame.logo_square, GuiWordmarkFrameCfg, .now_ms = 1000ULL,
                                      .wordmark = &s_wordmark, .glyphs = &glyphs, .frame = &frame,
                                      .wordmark_read = &s_wordmark, .icons = &s_icons, .palette = palette,
                                      .raster = &raster);
    cli_check((square.width > 0) && (square.height > 0), "the eye's square in the bar has a size once the eye is home");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 920},
               .color = 0u);
    EMBED_CALL(gui_wordmark_frame.logo_draw, GuiWordmarkFrameCfg, .now_ms = 1000ULL, .wordmark = &s_wordmark,
               .glyphs = &glyphs, .frame = &frame, .wordmark_read = &s_wordmark, .icons = &s_icons,
               .palette = palette, .raster = &raster);
    int drawn = 0;
    for (int y = square.y; (y < square.y + square.height) && (y < 920); y += 1)
    {
        for (int x = square.x; (x < square.x + square.width) && (x < 1440); x += 1)
        {
            // the square lies inside the raster, as the loop's bounds hold it
            drawn |= raster.pixels[((unsigned int)y * raster.width) + (unsigned int)x] != 0u;
        }
    }
    cli_check(drawn, "the eye is drawn on its square");

    EMBED_CALL(gui_wordmark_still_table.init, GuiWordmarkStillCfg, .wordmark = &s_wordmark, .still = 0);
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
