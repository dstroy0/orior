// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages_test.c: the stages' check: a slide's stages run from where it stands to where it rests, each stage's frame the
// frame the claim's numbers lay at that moment, the stage read by the clock's index, a stage's map drawing the pixels
// the base's own map draws for that frame, and the cell given back
#include "../../../../../ui/gui/src/stages.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

// whether two rasters of one size hold the same pixels
static int same_pixels(const GuiRaster *one, const GuiRaster *other)
{
    for (unsigned long long at = 0ull; at < (unsigned long long)one->width * one->height; at += 1ull)
    {
        if (one->pixels[at] != other->pixels[at])
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    cli_check(EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why), "the glyphs are read");
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 800u, .height = 500u, .scale = 100);
    GuiBase base;
    cli_check(EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u), .height = 500u), "the base is made");
    GuiMenubar menubar;
    EMBED_CALL(gui_menubar_glyphs.init, GuiMenubarGlyphsCfg, .menubar = &menubar);
    GuiClaim claim;
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &claim);
    GuiStages stages = {NULL, 0u, 0ull, 0u};
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1000ull) == NULL, "no stages made, none stands");

    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    cli_check(EMBED_CALL(gui_stages_claim.init, GuiStagesClaimCfg, .stages = &stages, .claim = &claim, .frame = &frame, .base = &base, .palette = palette, .menubar = &menubar, .glyphs = &glyphs, .time_ms = 1000ull), "the stages of the claim coming on are made");
    cli_check(stages.count == (unsigned int)(GUI_CLAIM_ANIMATION_MS / GUI_STAGE_MS) + 2u, "a stage a stage's time through the slide, and one for where it rests");
    cli_check(stages.made_for == claim.sent, "they are made for the slides as they were sent");
    const GuiStage *const first = EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1000ull);
    const GuiStage *const last = EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1000ull + GUI_CLAIM_ANIMATION_MS);
    cli_check((first == &stages.stage[0]) && (first->frame.claim.claim == 0), "the first stage stands where the slide began");
    cli_check((last == &stages.stage[stages.count - 1u]) && (last->frame.claim.claim == GUI_WHOLE), "the last where it rests");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 999ull) == first, "a moment before them reads the first");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 900000ull) == last, "and one long after them the last");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1000ull + (5ull * GUI_STAGE_MS) + 1ull) == &stages.stage[5], "the clock reads its stage by index");

    // each stage's frame the one the numbers lay, its holes the frame's, with no arithmetic left for the frame
    GuiClaim again;
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &again);
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &again, .on = 1);
    GuiFrame direct = frame;
    int frames_same = 1;
    int holes_same = 1;
    for (unsigned int at = 0u; at < stages.count; at += 1u)
    {
        GuiClaimNumbers numbers;
        const unsigned long long moment = (at + 1u == stages.count) ? 1000ull + GUI_CLAIM_ANIMATION_MS : 1000ull + ((unsigned long long)at * GUI_STAGE_MS);
        EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &again, .time_ms = moment, .numbers = &numbers);
        EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &direct, .numbers = &numbers);
        frames_same = frames_same && (memor.cmp(&(MemoriaCfg){.src = &direct, .other = &stages.stage[at].frame, .bytes = sizeof(direct)}) == 0);
        GuiRect holes[4];
        holes_same = holes_same && (EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &direct, .holes = holes) == stages.stage[at].hole_count);
    }
    cli_check(frames_same, "every stage's frame is the one the claim's numbers lay at its moment");
    cli_check(holes_same, "and its holes that frame's");

    // a stage's map draws what the base's own map draws for its frame
    GuiRaster from_stage;
    GuiRaster from_base;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &from_stage, .width = 800u, .height = 500u) && EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &from_base, .width = 800u, .height = 500u), "the rasters are made");
    int maps_same = 1;
    for (unsigned int at = 0u; at < stages.count; at += 7u)
    {
        const GuiStage *const one = &stages.stage[at];
        EMBED_CALL(gui_base_raster.map_draw, GuiBaseRasterCfg, .base_read = &base, .map = &one->map, .raster = &from_stage, .ground = NULL, .color = palette->deep);
        (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &one->frame, .palette = palette, .controls_steady = one->frame.controls_pane_share >= GUI_WHOLE);
        EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &from_base, .ground = NULL, .color = palette->deep);
        maps_same = maps_same && same_pixels(&from_stage, &from_base);
    }
    cli_check(maps_same, "a stage's map draws the pixels the base's own map draws for its frame");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &from_stage);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &from_base);

    // sent again partway, the stages run on from where the slide stands
    const GuiStage *const partway = EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1000ull + 120ull);
    const int stood = partway->frame.claim.claim;
    cli_check((stood > 0) && (stood < GUI_WHOLE), "partway the claim stands between");
    EMBED_CALL(gui_stages_table.stage_claim_take, GuiStagesCfg, .stage = partway, .claim = &claim);
    g_test_clock_ms = 1120ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 0);
    cli_check(claim.sent != stages.made_for, "a slide sent again leaves the stages out of date");
    cli_check(EMBED_CALL(gui_stages_claim.init, GuiStagesClaimCfg, .stages = &stages, .claim = &claim, .frame = &frame, .base = &base, .palette = palette, .menubar = &menubar, .glyphs = &glyphs, .time_ms = 1120ull), "and they are made again");
    cli_check(stages.stage[0].frame.claim.claim == stood, "from where it stood");
    cli_check(EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 1120ull + GUI_CLAIM_ANIMATION_MS)->frame.claim.claim == 0, "to where it is now bound");

    // less motion asked for: the one stage, where the slides are bound
    EMBED_CALL(cli_claim_still.still, GuiClaimStillCfg, .claim = &claim, .still = 1);
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    cli_check(EMBED_CALL(gui_stages_claim.init, GuiStagesClaimCfg, .stages = &stages, .claim = &claim, .frame = &frame, .base = &base, .palette = palette, .menubar = &menubar, .glyphs = &glyphs, .time_ms = 2000ull) && (stages.count == 1u) &&
                  (stages.stage[0].frame.claim.claim == GUI_WHOLE),
              "less motion asked for, one stage, where the slides are bound");

    EMBED_CALL(gui_stages_claim.free, GuiStagesClaimCfg, .stages = &stages);
    cli_check((stages.stage == NULL) && (EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &stages, .time_ms = 2000ull) == NULL), "given back, none stands");
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &base);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
