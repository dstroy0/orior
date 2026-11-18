// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy_test.c: the busy circle's check: an ask is located from when it begins, the circle comes once the ask is past
// the wait and not before, its stage steps with the clock and stands at the first while it is stationary, it sits at
// the editor's foot on its right, it is drawn again only once drawn and then at its own rectangle, and it goes once
// no ask is and it has been displayed its least
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/busy.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/palette.h"
#include "../cli/cli_check.h"

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
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
              "the raster is taken");

    static GuiBusy s_busy;
    EMBED_CALL(gui_busy_still_table.init, GuiBusyStillCfg, .busy = &s_busy, .still = 0);
    unsigned long long since = 0ULL;
    cli_check(!s_busy.displayed && !EMBED_CALL(gui_busy_since.waiting, GuiBusySinceCfg, .busy_read = &s_busy,
                                               .since = &since),
              "a circle with nothing asked is not displayed and locates no ask");

    const unsigned long long begun = 10000ULL;
    EMBED_CALL(gui_busy_running.work, GuiBusyRunningCfg, .now_ms = begun, .busy = &s_busy, .running = 1);
    cli_check(EMBED_CALL(gui_busy_since.waiting, GuiBusySinceCfg, .busy_read = &s_busy, .since = &since) &&
                  (since == begun),
              "an ask is located from when it begins");
    cli_check(!EMBED_CALL(gui_busy_running.tick, GuiBusyRunningCfg, .now_ms = begun + (GUI_BUSY_SHOW_DELAY_MS / 2ULL),
                          .busy = &s_busy, .running = 1) &&
                  !s_busy.displayed,
              "the circle does not come before the ask is past the wait");
    const unsigned long long shown = begun + GUI_BUSY_SHOW_DELAY_MS + 1ULL;
    cli_check(EMBED_CALL(gui_busy_running.tick, GuiBusyRunningCfg, .now_ms = shown, .busy = &s_busy, .running = 1) &&
                  s_busy.displayed,
              "the circle comes once the ask is past the wait");

    const unsigned int first = EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .now_ms = s_busy.visible_start_ms,
                                          .busy_read = &s_busy);
    const unsigned int third = EMBED_CALL(gui_busy.stage_at, GuiBusyCfg,
                                          .now_ms = s_busy.visible_start_ms + (3ULL * GUI_BUSY_STAGE_MS),
                                          .busy_read = &s_busy);
    cli_check((first < GUI_BUSY_STAGES) && (third == ((first + 3u) % GUI_BUSY_STAGES)),
              "the stage steps one a stage's milliseconds");
    EMBED_CALL(gui_busy_still_table.still, GuiBusyStillCfg, .busy = &s_busy, .still = 1);
    cli_check(EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .now_ms = s_busy.visible_start_ms + (5ULL * GUI_BUSY_STAGE_MS),
                         .busy_read = &s_busy) == 0u,
              "a stationary circle stands at the first stage");
    EMBED_CALL(gui_busy_still_table.still, GuiBusyStillCfg, .busy = &s_busy, .still = 0);

    const GuiRect circle = EMBED_CALL(gui_busy_frame.rect, GuiBusyFrameCfg, .now_ms = shown, .busy = &s_busy,
                                      .frame = &frame, .raster = &raster, .palette = palette);
    cli_check((circle.width > 0) && (circle.x + circle.width <= frame.editor.x + frame.editor.width) &&
                  (circle.y + circle.height <= frame.editor.y + frame.editor.height) &&
                  (circle.x > frame.editor.x + (frame.editor.width / 2)),
              "the circle sits at the editor's foot on its right");
    s_busy.under_saved = 0;
    const GuiRect none = EMBED_CALL(gui_busy_frame.redrawn, GuiBusyFrameCfg, .now_ms = shown, .busy = &s_busy,
                                    .frame = &frame, .raster = &raster, .palette = palette);
    cli_check((none.width == 0) || (none.height == 0), "a circle never drawn is not drawn again");
    EMBED_CALL(gui_busy_frame.draw, GuiBusyFrameCfg, .now_ms = shown, .busy = &s_busy, .frame = &frame,
               .raster = &raster, .palette = palette);
    const GuiRect again = EMBED_CALL(gui_busy_frame.redrawn, GuiBusyFrameCfg, .now_ms = shown + GUI_BUSY_STAGE_MS,
                                     .busy = &s_busy, .frame = &frame, .raster = &raster, .palette = palette);
    cli_check((again.width > 0) && (again.height > 0) && (again.x == s_busy.under_rect.x) &&
                  (again.y == s_busy.under_rect.y),
              "a circle drawn is drawn again at its own rectangle");

    EMBED_CALL(gui_busy_running.work, GuiBusyRunningCfg, .now_ms = shown + 1ULL, .busy = &s_busy, .running = 0);
    (void)EMBED_CALL(gui_busy_running.tick, GuiBusyRunningCfg, .now_ms = shown + GUI_BUSY_MIN_VISIBLE_MS + 1ULL,
                     .busy = &s_busy, .running = 0);
    cli_check(!s_busy.displayed, "the circle goes once no ask is and it has been displayed its least");

    EMBED_CALL(gui_busy_still_table.init, GuiBusyStillCfg, .busy = &s_busy, .still = 0);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
