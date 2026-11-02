// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages_test.c: the stages' check: a slide's stages run from where it stands to where it rests, each stage's frame the
// frame the claim's numbers lay at that moment, the stage read by the clock's index, a stage's map drawing the pixels
// the base's own map draws for that frame, and the cell given back
#include "../../../../../ui/gui/src/stages.h"
#include "../cli/cli_check.h"

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
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    cli_check(gui_glyphs_made(&glyphs, &why), "the glyphs are read");
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 500u, 1.0f);
    GuiBase base;
    cli_check(gui_base_made(&base, gui_raster_pitch(800u), 500u), "the base is made");
    GuiMenubar menubar;
    gui_menubar_made(&menubar);
    GuiClaim claim;
    gui_claim_made(&claim);
    GuiStages stages = {NULL, 0u, 0ull, 0u};
    cli_check(gui_stages_at(&stages, 1000ull) == NULL, "no stages made, none stands");

    gui_claim_set(&claim, 1, 1000ull);
    cli_check(gui_stages_made(&stages, &claim, &frame, &base, palette, &menubar, &glyphs, 1000ull), "the stages of the claim coming on are made");
    cli_check(stages.count == (unsigned int)(GUI_CLAIM_SLIDE / GUI_STAGE_MS) + 2u, "a stage a stage's time through the slide, and one for where it rests");
    cli_check(stages.made_for == claim.sent, "they are made for the slides as they were sent");
    const GuiStage *const first = gui_stages_at(&stages, 1000ull);
    const GuiStage *const last = gui_stages_at(&stages, 1000ull + GUI_CLAIM_SLIDE);
    cli_check((first == &stages.stage[0]) && (first->frame.claim.claim == 0), "the first stage stands where the slide began");
    cli_check((last == &stages.stage[stages.count - 1u]) && (last->frame.claim.claim == GUI_WHOLE), "the last where it rests");
    cli_check(gui_stages_at(&stages, 999ull) == first, "a moment before them reads the first");
    cli_check(gui_stages_at(&stages, 900000ull) == last, "and one long after them the last");
    cli_check(gui_stages_at(&stages, 1000ull + (5ull * GUI_STAGE_MS) + 1ull) == &stages.stage[5], "the clock reads its stage by index");

    // each stage's frame the one the numbers lay, its holes the frame's, with no arithmetic left for the frame
    GuiClaim again;
    gui_claim_made(&again);
    gui_claim_set(&again, 1, 1000ull);
    GuiFrame direct = frame;
    int frames_same = 1;
    int holes_same = 1;
    for (unsigned int at = 0u; at < stages.count; at += 1u)
    {
        GuiClaimNumbers numbers;
        const unsigned long long moment = (at + 1u == stages.count) ? 1000ull + GUI_CLAIM_SLIDE : 1000ull + ((unsigned long long)at * GUI_STAGE_MS);
        gui_claim_numbers(&again, moment, &numbers);
        gui_frame_claimed(&direct, &numbers);
        frames_same = frames_same && (memor.cmp(&(MemoriaCfg){.src = &direct, .other = &stages.stage[at].frame, .bytes = sizeof(direct)}) == 0);
        GuiRect holes[4];
        holes_same = holes_same && (gui_frame_holes(&direct, holes) == stages.stage[at].hole_count);
    }
    cli_check(frames_same, "every stage's frame is the one the claim's numbers lay at its moment");
    cli_check(holes_same, "and its holes that frame's");

    // a stage's map draws what the base's own map draws for its frame
    GuiRaster from_stage;
    GuiRaster from_base;
    cli_check(gui_raster_made(&from_stage, 800u, 500u) && gui_raster_made(&from_base, 800u, 500u), "the rasters are made");
    int maps_same = 1;
    for (unsigned int at = 0u; at < stages.count; at += 7u)
    {
        const GuiStage *const one = &stages.stage[at];
        gui_base_map_drawn(&base, &one->map, &from_stage, NULL, palette->deep);
        (void)gui_base_laid(&base, &one->frame, palette, one->frame.controls_pane_share >= GUI_WHOLE);
        gui_base_drawn(&base, &from_base, NULL, palette->deep);
        maps_same = maps_same && same_pixels(&from_stage, &from_base);
    }
    cli_check(maps_same, "a stage's map draws the pixels the base's own map draws for its frame");
    gui_raster_given(&from_stage);
    gui_raster_given(&from_base);

    // sent again partway, the stages run on from where the slide stands
    const GuiStage *const partway = gui_stages_at(&stages, 1000ull + 120ull);
    const int stood = partway->frame.claim.claim;
    cli_check((stood > 0) && (stood < GUI_WHOLE), "partway the claim stands between");
    gui_stage_claim_taken(partway, &claim);
    gui_claim_set(&claim, 0, 1120ull);
    cli_check(claim.sent != stages.made_for, "a slide sent again leaves the stages out of date");
    cli_check(gui_stages_made(&stages, &claim, &frame, &base, palette, &menubar, &glyphs, 1120ull), "and they are made again");
    cli_check(stages.stage[0].frame.claim.claim == stood, "from where it stood");
    cli_check(gui_stages_at(&stages, 1120ull + GUI_CLAIM_SLIDE)->frame.claim.claim == 0, "to where it is now bound");

    // less motion asked for: the one stage, where the slides are bound
    gui_claim_still(&claim, 1);
    gui_claim_set(&claim, 1, 2000ull);
    cli_check(gui_stages_made(&stages, &claim, &frame, &base, palette, &menubar, &glyphs, 2000ull) && (stages.count == 1u) &&
                  (stages.stage[0].frame.claim.claim == GUI_WHOLE),
              "less motion asked for, one stage, where the slides are bound");

    gui_stages_given(&stages);
    cli_check((stages.stage == NULL) && (gui_stages_at(&stages, 2000ull) == NULL), "given back, none stands");
    gui_base_given(&base);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
