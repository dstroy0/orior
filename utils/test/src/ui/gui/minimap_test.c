// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap_test.c: the minimap's check: the map is whole, its left at its column's left, and shows only within its
// column as the claim closes it, nothing of it past the editor's right edge; a press past the column's edge misses the
// map, a press within it lands; the column whole shows the overview side_menu at the map's right
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../ui/gui/src/minimap.h"
#include "../cli/cli_check.h"

// the color the raster is filled with before the map is drawn, which no part of the palette is
#define MINIMAP_TEST_UNDER 0xff123456u

// whether every pixel of `rect` is `color`
static int all_colored(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != color)
            {
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = frame.width, .height = frame.height),
              "a raster for the window is taken");
    CliViewFrame view = {0};
    view.lines = 200ull;
    view.cursor.line = 100ull;
    GuiEditor painter = {0};
    painter.frame = &view;
    GuiMinimap map = {0};
    // halfway through the claim the column is half closed and the map is whole past the editor's right edge
    GuiAutohideNumbers numbers = frame.claim;
    numbers.claim = GUI_FIXED_ONE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.map.width > 0) && (frame.map.width < frame.map_whole.width), "halfway through the claim the column is half closed");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, (int)frame.width, (int)frame.height},
               .color = MINIMAP_TEST_UNDER);
    EMBED_CALL(gui_minimap_position.draw, GuiMinimapPositionCfg, .map = &map, .place = frame.map_whole, .clip = frame.map, .scale = 100,
               .editor = &painter, .on_screen = 40.0f, .raster = &raster, .palette = palette);
    const GuiRect past = {frame.map.x + frame.map.width, frame.map.y, frame.map_whole.width - frame.map.width, frame.map.height};
    cli_check(all_colored(&raster, past, MINIMAP_TEST_UNDER), "nothing of the map is drawn past the editor's right edge");
    cli_check(!all_colored(&raster, frame.map, MINIMAP_TEST_UNDER), "the map shows within its column");
    unsigned long long top = 0ull;
    cli_check(!EMBED_CALL(gui_minimap_position_scale.press, GuiMinimapPositionScaleCfg, .map = &map, .place = frame.map_whole,
                          .clip = frame.map, .scale = 100, .editor = &painter, .on_screen = 40.0f, .x = past.x + 2,
                          .y = frame.map.y + 100, .top = &top),
              "a press past the column's edge misses the map");
    cli_check(EMBED_CALL(gui_minimap_position_scale.press, GuiMinimapPositionScaleCfg, .map = &map, .place = frame.map_whole,
                         .clip = frame.map, .scale = 100, .editor = &painter, .on_screen = 40.0f, .x = frame.map.x + 4,
                         .y = frame.map.y + 100, .top = &top),
              "a press within the column lands on the map");
    map.dragging = 0;
    // the column whole shows the map whole, the overview side_menu at its right with the caret's line
    numbers.claim = 0;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.map.width == frame.map_whole.width) && (frame.map.x == frame.map_whole.x), "the column whole is the map whole");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, (int)frame.width, (int)frame.height},
               .color = MINIMAP_TEST_UNDER);
    EMBED_CALL(gui_minimap_position.draw, GuiMinimapPositionCfg, .map = &map, .place = frame.map_whole, .clip = frame.map, .scale = 100,
               .editor = &painter, .on_screen = 40.0f, .raster = &raster, .palette = palette);
    const int side_menu = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = 100);
    const GuiRect side_menu_rect = {frame.map.x + frame.map.width - side_menu, frame.map.y, side_menu, frame.map.height};
    cli_check(!all_colored(&raster, side_menu_rect, MINIMAP_TEST_UNDER), "the overview side_menu is at the map's right");
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
