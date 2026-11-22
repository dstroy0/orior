// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dots_test.c: the state dots' check: each state has the palette's color of its kind; one dot draws for each record of
// a kind, right to left from the right edge given; a record that blinks shows for half of each blink; the remote dot
// shows the heaviest state of its kind; and no record draws no dot
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/machine/overseer.h"
#include "../../../../../ui/gui/src/dots.h"
#include "../cli/cli_check.h"

#define DOTS_TEST_WIDTH 120u
#define DOTS_TEST_HEIGHT 20u

// whether the pixel at x, y of `raster` is not `ground`
static int lit_at(const GuiRaster *raster, int x, int y, GuiColor ground)
{
    // the place lies inside the raster
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    cli_check((EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = palette, .state = CLI_OVERSEER_STARTING) ==
               palette->dot_up) &&
                  (EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = palette, .state = CLI_OVERSEER_UP) ==
                   palette->dot_up) &&
                  (EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = palette,
                              .state = CLI_OVERSEER_STOPPING) == palette->dot_stopping) &&
                  (EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = palette, .state = CLI_OVERSEER_ERROR) ==
                   palette->dot_failed) &&
                  (EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = palette,
                              .state = CLI_OVERSEER_CRITICAL) == palette->dot_failed),
              "each state has its palette color: green, yellow and red");
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = DOTS_TEST_WIDTH,
                         .height = DOTS_TEST_HEIGHT, .part = GUI_BLOCK_ICON_WORK),
              "a raster is made");
    const GuiColor ground = GUI_RGB(0x20u, 0x20u, 0x40u);
    const GuiRect all = {0, 0, (int)DOTS_TEST_WIDTH, (int)DOTS_TEST_HEIGHT};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = all, .color = ground);
    cli_check(EMBED_CALL(gui_dots.draw, GuiDotsCfg, .raster = &raster, .palette = palette, .clip = all, .right = 110,
                         .top = 0, .height = (int)DOTS_TEST_HEIGHT, .scale = 100, .kind = CLI_OVERSEER_SERVER,
                         .now_ms = 0ULL) == 110,
              "no record draws no dot");

    // two servers up, one at the right edge and one left of it
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .kind = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "pyright", .kind = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_READY);
    const int left = EMBED_CALL(gui_dots.draw, GuiDotsCfg, .raster = &raster, .palette = palette, .clip = all,
                                .right = 110, .top = 0, .height = (int)DOTS_TEST_HEIGHT, .scale = 100,
                                .kind = CLI_OVERSEER_SERVER, .now_ms = 0ULL);
    const int middle = (int)DOTS_TEST_HEIGHT / 2;
    cli_check((left == 110 - (2 * GUI_DOTS_SIDE) - GUI_DOTS_GAP) &&
                  lit_at(&raster, 110 - (GUI_DOTS_SIDE / 2) - 1, middle, ground) &&
                  lit_at(&raster, 110 - GUI_DOTS_SIDE - GUI_DOTS_GAP - (GUI_DOTS_SIDE / 2) - 1, middle, ground) &&
                  !lit_at(&raster, 110 - GUI_DOTS_SIDE - 2, middle, ground),
              "a dot draws for each server, right to left, a gap between them");

    // a server that starts blinks: it shows in one half of the blink and not in the other
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .event = CLI_OVERSEER_CONNECTING);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = all, .color = ground);
    (void)EMBED_CALL(gui_dots.draw, GuiDotsCfg, .raster = &raster, .palette = palette, .clip = all, .right = 110,
                     .top = 0, .height = (int)DOTS_TEST_HEIGHT, .scale = 100, .kind = CLI_OVERSEER_SERVER,
                     .now_ms = CLI_OVERSEER_BLINK_MS);
    const int clangd_x = 110 - GUI_DOTS_SIDE - GUI_DOTS_GAP - (GUI_DOTS_SIDE / 2) - 1;
    cli_check(!lit_at(&raster, clangd_x, middle, ground) &&
                  lit_at(&raster, 110 - (GUI_DOTS_SIDE / 2) - 1, middle, ground),
              "a starting server's dot is dark in the second half of its blink, the up one beside it lit");
    (void)EMBED_CALL(gui_dots.draw, GuiDotsCfg, .raster = &raster, .palette = palette, .clip = all, .right = 110,
                     .top = 0, .height = (int)DOTS_TEST_HEIGHT, .scale = 100, .kind = CLI_OVERSEER_SERVER,
                     .now_ms = 0ULL);
    cli_check(lit_at(&raster, clangd_x, middle, ground), "and lit in the first half");

    // the remote link's one dot shows the heaviest state of the sessions
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "a", .kind = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "b", .kind = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_LINK_LOST);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = all, .color = ground);
    const int remote_left = EMBED_CALL(gui_dots.draw, GuiDotsCfg, .raster = &raster, .palette = palette, .clip = all,
                                       .right = 30, .top = 0, .height = (int)DOTS_TEST_HEIGHT, .scale = 100,
                                       .kind = CLI_OVERSEER_REMOTE, .worst = 1, .now_ms = 0ULL);
    cli_check((remote_left == 30 - GUI_DOTS_SIDE) && lit_at(&raster, 30 - (GUI_DOTS_SIDE / 2) - 1, middle, ground) &&
                  !lit_at(&raster, 30 - GUI_DOTS_SIDE - 2, middle, ground),
              "the remote link draws one dot, for the heaviest state of its sessions");
    cli_overseer_end();
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
