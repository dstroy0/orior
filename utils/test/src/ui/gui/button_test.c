// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// button_test.c: the buttons' check: they start at rest with nothing baked; their beads' atlas arranges over the
// block's bead part in the palette's colors; a press flattens the button's own bead over the press's time and is
// at the last frame; the release springs it back on the bead's curve, wobbles past rest and settles there; a release
// with no button held and a press past the buttons do nothing; and a button drawn positions its bead at its cell
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/button.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/lights.h"
#include "../cli/cli_check.h"

#define BUTTON_TEST_SIDE 31u
#define BUTTON_TEST_PAD 7u
#define BUTTON_TEST_REACH 6u
#define BUTTON_TEST_WIDTH 1440u
#define BUTTON_TEST_HEIGHT 920u

// the frame the button `tool`'s press is at, at `time_ms`, as the draw reads it
static unsigned int frame_at(const GuiButtons *buttons, unsigned int tool, unsigned long long time_ms, unsigned long long lasts_ms)
{
    const int press = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &buttons->press[tool],
                                 .time_ms = time_ms, .lasts_ms = lasts_ms);
    return EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = press - GUI_FIXED_ONE, .one = GUI_FIXED_ONE);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = BUTTON_TEST_WIDTH,
               .height = BUTTON_TEST_HEIGHT, .scale = 100);
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = &frame);
    GuiButtons buttons;
    EMBED_CALL(gui_buttons.init, GuiButtonsCfg, .buttons = &buttons);
    cli_check((buttons.pressed == GUI_BEAD_TOOLS_MAX) && !buttons.atlas.made, "buttons start at rest with nothing baked");
    cli_check(EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &buttons, .time_ms = 1000ULL) == 0ULL,
              "buttons at rest end nothing");
    // two buttons: one at the window's head and one at its foot
    const GuiRect cells[2] = {{18, 60, 0, 0}, {18, 800, 0, 0}};
    const unsigned long long bytes =
        EMBED_CALL(gui_buttons_bake_table.bytes, GuiButtonsBakeCfg, .width = BUTTON_TEST_SIDE, .height = BUTTON_TEST_SIDE,
                   .pad = BUTTON_TEST_PAD, .reach = BUTTON_TEST_REACH, .tools = 2u);
    GuiColor *const pixels_span = (GuiColor *)(void *)EMBED_CALL(gui_block_part.span, GuiBlockPartCfg,
                                                                  .part = GUI_BLOCK_BEAD, .segment = 0u, .bytes = bytes);
    cli_check((bytes > 0ULL) && (pixels_span != NULL), "the sizes take bytes, which the block's bead part passes over");
    cli_check(EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = &buttons, .palette = palette,
                         .width = BUTTON_TEST_SIDE, .height = BUTTON_TEST_SIDE, .pad = BUTTON_TEST_PAD, .reach = BUTTON_TEST_REACH, .cells = cells,
                         .tools = 2u, .lights = lights, .light_count = light_count, .pixels = pixels_span),
              "the atlas arranges over the pixels passed in");
    cli_check(buttons.atlas.made && (buttons.atlas.tools == 2u) && (buttons.atlas.color[GUI_BEAD_COLOR_GLINT] == palette->bead_glint) &&
                  (buttons.atlas.color[GUI_BEAD_COLOR_SHADOW] == palette->bead_shadow),
              "in the palette's bead colors");
    const unsigned int idle = EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = 0, .one = GUI_FIXED_ONE);

    // the presses
    EMBED_CALL(gui_buttons_move.press, GuiButtonsMoveCfg, .buttons = &buttons, .tool = 1u, .time_ms = 4000ULL);
    cli_check((buttons.pressed == 1u) && (buttons.press[1].to == 2 * GUI_FIXED_ONE) && (buttons.press[0].to == GUI_FIXED_ONE) &&
                  (EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &buttons, .time_ms = 4000ULL) ==
                   4000ULL + GUI_BUTTONS_PRESS_MS),
              "a press flattens the button's own bead over the press's time");
    cli_check(frame_at(&buttons, 1u, 4000ULL + GUI_BUTTONS_PRESS_MS, GUI_BUTTONS_PRESS_MS) == GUI_BEAD_FRAMES - 1u,
              "pressed, it is at the last frame");
    EMBED_CALL(gui_buttons_move.release, GuiButtonsMoveCfg, .buttons = &buttons, .time_ms = 4200ULL);
    cli_check((buttons.pressed == GUI_BEAD_TOOLS_MAX) && (buttons.press[1].to == GUI_FIXED_ONE) &&
                  (buttons.press[1].from == 2 * GUI_FIXED_ONE) && (buttons.press[1].curve == GUI_CURVE_BEAD),
              "let go, it springs back on the bead's curve");
    unsigned int least = GUI_BEAD_FRAMES;
    for (unsigned long long at = 4200ULL; at <= 4200ULL + GUI_BUTTONS_RELEASE_MS; at += 4ULL)
    {
        const unsigned int frame_now = frame_at(&buttons, 1u, at, GUI_BUTTONS_RELEASE_MS);
        least = (frame_now < least) ? frame_now : least;
    }
    cli_check(least < idle, "the release wobbles past rest");
    cli_check(frame_at(&buttons, 1u, 5000ULL, GUI_BUTTONS_RELEASE_MS) == idle, "and settles at rest");
    EMBED_CALL(gui_buttons_move.release, GuiButtonsMoveCfg, .buttons = &buttons, .time_ms = 5000ULL);
    cli_check(EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &buttons, .time_ms = 5000ULL) == 0ULL,
              "a release with no button held does nothing");
    EMBED_CALL(gui_buttons_move.press, GuiButtonsMoveCfg, .buttons = &buttons, .tool = GUI_BEAD_TOOLS_MAX, .time_ms = 5000ULL);
    cli_check(buttons.pressed == GUI_BEAD_TOOLS_MAX, "a press past the buttons does nothing");

    // a button drawn with its press located is blitted at the pressed frame, the cell's top right corner off its frost
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 120u, .height = 120u,
                         .part = GUI_BLOCK_ICON_WORK),
              "a raster is made");
    const GuiColor ground = GUI_RGB(0x20u, 0x20u, 0x40u);
    const int at_x = 40;
    const int at_y = 30;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 120, 120}, .color = ground);
    EMBED_CALL(gui_buttons_move.press, GuiButtonsMoveCfg, .buttons = &buttons, .tool = 0u, .time_ms = 6000ULL);
    EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = &buttons, .raster = &raster, .tool = 0u, .x = at_x,
               .y = at_y, .time_ms = 6000ULL + GUI_BUTTONS_PRESS_MS);
    const unsigned int glint_row = (unsigned int)at_y + BUTTON_TEST_PAD + 4u;
    const unsigned int glint_x = (unsigned int)at_x + BUTTON_TEST_PAD + 6u;
    cli_check((raster.pixels[(glint_row * 120u) + glint_x] != ground) &&
                  (raster.pixels[((unsigned int)at_y * 120u) + (unsigned int)at_x + buttons.atlas.cell_width - 1u] == ground),
              "a button drawn positions its bead at its cell");
    EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = &buttons, .raster = &raster, .tool = 0u, .x = at_x,
               .y = at_y, .fade = 255u, .time_ms = 6000ULL + GUI_BUTTONS_PRESS_MS);
    cli_check(raster.pixels[(glint_row * 120u) + glint_x] != ground, "a button faded whole positions nothing more");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
