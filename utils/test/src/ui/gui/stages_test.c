// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages_test.c: the stages' check: every stage of the claim's slide positioned in one cell, the first at the slide's
// start and the last at its rest, a moment read to its stage, each stage's map the base's bands, and the claim's slats
// put back to a stage
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/palette.h"
#include "../../../../../ui/gui/src/stages.h"
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
    GuiBase base;
    cli_check(EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 1440u, .height = 920u),
              "a base for the window is made");
    static GuiMenubar s_menubar;
    EMBED_CALL(gui_menubar.init, GuiMenubarCfg, .menubar = &s_menubar);
    GuiSideMenu side_menu;
    EMBED_CALL(gui_side_menu_table.init, GuiSideMenuInitCfg, .side_menu = &side_menu);
    GuiSides sides;
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &sides);
    GuiDocks docks;
    EMBED_CALL(gui_docks.init, GuiDocksCfg, .docks = &docks);
    GuiAutohide claim;
    EMBED_CALL(gui_autohide_still_table.init, GuiAutohideStillCfg, .claim = &claim, .now_ms = 1000ULL);
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 1, .now_ms = 1000ULL);
    const int grown_top_before = claim.grown_top;

    GuiStages stages = {0};
    cli_check(EMBED_CALL(gui_stages_autohide.init, GuiStagesAutohideCfg, .stages = &stages, .claim = &claim,
                         .sides_read = &sides, .side_menu_read = &side_menu, .docks_read = &docks, .frame = &frame,
                         .base = &base, .palette = palette, .menubar = &s_menubar, .glyphs = &glyphs,
                         .time_ms = 1000ULL),
              "the stages of the claim's slide are positioned");
    cli_check((stages.count >= 2u) && (stages.start_ms == 1000ULL) && (stages.end_ms > 1000ULL) &&
                  (stages.created_for == claim.sent),
              "the stages run from the slide's start to its end, made for the claim's sends");
    const GuiStage *const first = &stages.stage[0];
    const GuiStage *const last = &stages.stage[stages.count - 1u];
    cli_check((first->frame.claim.grown_top == grown_top_before) && (claim.grown_top == last->frame.claim.grown_top),
              "the first stage's slats are the claim's at the start, and the claim's come out as the last stage has them");
    cli_check(last->frame.claim.claim > first->frame.claim.claim, "the last stage's claim is further on than the first's");

    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .time_ms = 500ULL, .stages_read = &stages) == first,
              "a moment before the first stage is the first's");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .time_ms = 1000ULL + GUI_STAGE_MS,
                         .stages_read = &stages) == &stages.stage[1],
              "a stage's time on from the start is the second stage");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .time_ms = stages.end_ms + 1000ULL,
                         .stages_read = &stages) == last,
              "a moment past the slides' end is the last stage");
    const GuiStages none = {0};
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .time_ms = 1000ULL, .stages_read = &none) == NULL,
              "stages not made have no stage at any moment");
    cli_check((last->map.bands == base.bands) && (last->map.band_from[0] == base.band_from[0]) &&
                  (last->map.run_first[last->map.bands] == base.run_first[base.bands]),
              "the last stage's map holds the bands of the base as it was positioned last");
    cli_check(first->hole_count <= 4u, "a stage has no more holes than the work holds");

    EMBED_CALL(gui_stages_table.stage_autohide_alloc, GuiStagesCfg, .claim = &claim, .stage = first);
    cli_check((claim.grown_top == first->frame.claim.grown_top) &&
                  (claim.grown_left == first->frame.claim.grown_left) &&
                  (claim.grown_bottom == first->frame.claim.grown_bottom) &&
                  (claim.last_top == first->frame.claim.reveal_top),
              "the claim's slats are put back to where the first stage has them");

    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
