// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bead_test.c: the bead's check: the atlas arranges for a size over the pixels its caller passes it and retains a
// button's frames for the same size, place, lights and colors; the idle frame's center reads itself, its inner edge
// reads toward the center, the ground just outside it is frosted, the cell's corner is untouched, the glint and the
// shadow are lit; the pressed frame refracts less than the idle one and its glint is elsewhere, as a button at
// the window's foot has its glint elsewhere than one at its head; the frames of the press shares; and the blit leaves
// untouched pixels alone, lights the glint, shifts a line inside the bead away from the center, positions nothing at no
// opacity and nothing outside its clip
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/lights.h"
#include "../../../../../includes/window/palette.h"
#include "../../../../../includes/glass/bead.h"
#include "../cli/cli_check.h"

#define BEAD_TEST_SIDE 31u
#define BEAD_TEST_PAD 7u
#define BEAD_TEST_REACH 6u
#define BEAD_TEST_WIDTH 1440u
#define BEAD_TEST_HEIGHT 920u
// the press share's whole, as the check reads frames by
#define BEAD_TEST_ONE 1000LL

// plane A's word at x, y of `frame` of the button `tool`
static GuiColor plane_a_at(const GuiBeadAtlas *atlas, unsigned int tool, unsigned int frame, unsigned int x, unsigned int y)
{
    return atlas->cells.pixels[(((((tool * GUI_BEAD_FRAMES) + frame) * 2u * atlas->cell_height) + y) * atlas->stride) + x];
}

// plane B's pixel at x, y of `frame` of the button `tool`
static GuiColor plane_b_at(const GuiBeadAtlas *atlas, unsigned int tool, unsigned int frame, unsigned int x, unsigned int y)
{
    return atlas->cells.pixels[(((((tool * GUI_BEAD_FRAMES) + frame) * 2u * atlas->cell_height) + atlas->cell_height + y) * atlas->stride) + x];
}

// the brightest pixel of plane B of `frame` of `tool`, as its row by the cell's side plus its column
static unsigned int glint_at(const GuiBeadAtlas *atlas, unsigned int tool, unsigned int frame)
{
    unsigned int max = 0u;
    unsigned int where = 0u;
    for (unsigned int y = 0u; y < atlas->cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < atlas->cell_width; x += 1u)
        {
            const GuiColor lit = plane_b_at(atlas, tool, frame, x, y);
            const unsigned int white = GUI_RED_OF(lit) + GUI_GREEN_OF(lit) + GUI_BLUE_OF(lit);
            where = (white > max) ? (y * atlas->cell_width) + x : where;
            max = (white > max) ? white : max;
        }
    }
    return where;
}

// the max any pixel of `frame` reads away across, and how far every pixel of it reads away across, summed
static int reach_max(const GuiBeadAtlas *atlas, unsigned int frame)
{
    int max = 0;
    for (unsigned int y = 0u; y < atlas->cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < atlas->cell_width; x += 1u)
        {
            const int dx = GUI_BEAD_DX_OF(plane_a_at(atlas, 0u, frame, x, y));
            const int size = (dx < 0) ? -dx : dx;
            max = (size > max) ? size : max;
        }
    }
    return max;
}

static long long reach_summed(const GuiBeadAtlas *atlas, unsigned int frame)
{
    long long summed = 0LL;
    for (unsigned int y = 0u; y < atlas->cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < atlas->cell_width; x += 1u)
        {
            const int dx = GUI_BEAD_DX_OF(plane_a_at(atlas, 0u, frame, x, y));
            summed += (dx < 0) ? -dx : dx;
        }
    }
    return summed;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiLight lights[GUI_LIGHTS_MAX];
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = BEAD_TEST_WIDTH,
               .height = BEAD_TEST_HEIGHT, .scale = 100);
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = &frame);
    GuiBeadAtlas atlas_store;
    EMBED_CALL(gui_bead_bake_table.init, GuiBeadBakeCfg, .atlas = &atlas_store);
    cli_check(!atlas_store.made && (atlas_store.tools == 0u), "an atlas starts with nothing arranged or baked");
    // the atlas's pixels are the block's bead part, as many bytes as the sizes take, and its colors the palette's
    const GuiColor colors[GUI_BEAD_COLORS] = {palette->bead_glint, palette->bead_caustic, palette->bead_rim,
                                              palette->bead_shadow};
    const unsigned long long bytes =
        EMBED_CALL(gui_bead_bake_table.bytes, GuiBeadBakeCfg, .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD,
                   .reach = BEAD_TEST_REACH, .tools = 2u);
    cli_check(bytes > 0ULL, "the sizes take bytes");
    GuiColor *const pixels_span = (GuiColor *)(void *)EMBED_CALL(gui_block_part.span, GuiBlockPartCfg,
                                                                  .part = GUI_BLOCK_BEAD, .segment = 0u, .bytes = bytes);
    cli_check(pixels_span != NULL, "the block passes the atlas its pixels");
    cli_check(!EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .colors = colors,
                          .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD, .reach = BEAD_TEST_REACH, .tools = 2u,
                          .lights = lights, .light_count = light_count),
              "a bake with no pixels is refused");
    // two buttons: one at the window's head and one at its foot
    const GuiRect cells[2] = {{18, 60, 0, 0}, {18, 800, 0, 0}};
    cli_check(EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                         .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD,
                         .reach = BEAD_TEST_REACH, .cells = cells, .tools = 2u, .lights = lights,
                         .light_count = light_count),
              "the atlas arranges for the side_menu's buttons");
    const GuiBeadAtlas *const atlas = &atlas_store;
    cli_check(atlas->made && (atlas->baked == 0u) && (atlas->tools == 2u) &&
                  (atlas->cell_width == BEAD_TEST_SIDE + (2u * BEAD_TEST_PAD)) &&
                  (atlas->cell_height == atlas->cell_width) && (atlas->stride == atlas->cell_width + (2u * BEAD_TEST_REACH)),
              "the cell is the button with its margin, the stride the cell with its reach, no button baked yet");
    cli_check(EMBED_CALL(gui_bead_blit_table.tool_bake, GuiBeadBlitCfg, .atlas = &atlas_store, .tool = 0u) &&
                  (atlas->baked == 1u),
              "a button's frames bake when asked");
    const GuiColor *const pixels = atlas->cells.pixels;
    cli_check(EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                         .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD,
                         .reach = BEAD_TEST_REACH, .cells = cells, .tools = 2u, .lights = lights,
                         .light_count = light_count) &&
                  (atlas->cells.pixels == pixels) && (atlas->baked == 1u),
              "an atlas arranged again for the same sizes, places and lights retains its baked frames");
    const GuiRect moved[2] = {{18, 60, 0, 0}, {18, 700, 0, 0}};
    (void)EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                     .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD, .reach = BEAD_TEST_REACH,
                     .cells = moved, .tools = 2u, .lights = lights, .light_count = light_count);
    cli_check(atlas->baked == 1u, "a button moved unbakes its own frames, the others retained");
    // a button of its own width in the cell: its bead positioned from the cell's top left inside the margin, the cell's
    // right side past it untouched
    const GuiRect narrow[2] = {{18, 60, 0, 0}, {18, 700, (int)BEAD_TEST_SIDE / 2, (int)BEAD_TEST_SIDE}};
    (void)EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                     .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD, .reach = BEAD_TEST_REACH,
                     .cells = narrow, .tools = 2u, .lights = lights, .light_count = light_count);
    cli_check((atlas->baked == 1u) && EMBED_CALL(gui_bead_blit_table.tool_bake, GuiBeadBlitCfg, .atlas = &atlas_store, .tool = 1u),
              "a button sized anew unbakes its own frames and bakes again when asked");
    const unsigned int narrow_idle = EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = 0, .one = BEAD_TEST_ONE);
    const unsigned int narrow_middle = atlas->cell_height / 2u;
    cli_check((GUI_BEAD_MASK_OF(plane_a_at(atlas, 1u, narrow_idle, BEAD_TEST_PAD + (BEAD_TEST_SIDE / 4u), narrow_middle)) != 0u) &&
                  (GUI_BEAD_MASK_OF(plane_a_at(atlas, 1u, narrow_idle, atlas->cell_width - 2u, narrow_middle)) == 0u),
              "a narrower button's bead covers its own width from the cell's left and leaves the cell's right untouched");
    // the tab shape: the bead fills the button's box to its foot and its lower corners, its top corners rounded
    (void)EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                     .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD, .reach = BEAD_TEST_REACH,
                     .shape = GUI_BEAD_SHAPE_TAB, .radius = 6u, .cells = moved, .tools = 2u, .lights = lights,
                     .light_count = light_count);
    cli_check((atlas->baked == 0u) && (atlas->shape == GUI_BEAD_SHAPE_TAB) &&
                  EMBED_CALL(gui_bead_blit_table.tool_bake, GuiBeadBlitCfg, .atlas = &atlas_store, .tool = 0u),
              "a shape chosen anew unbakes every button, and a button bakes again in the tab's shape");
    const unsigned int tab_idle = EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = 0, .one = BEAD_TEST_ONE);
    cli_check((GUI_BEAD_MIX_OF(plane_a_at(atlas, 0u, tab_idle, BEAD_TEST_PAD + 1u, BEAD_TEST_PAD + BEAD_TEST_SIDE - 2u)) == 0u) &&
                  (GUI_BEAD_MIX_OF(plane_a_at(atlas, 0u, tab_idle, BEAD_TEST_PAD + 1u, BEAD_TEST_PAD + 1u)) > 0u),
              "a tab's bead covers its box's lower corner and leaves its top corner outside the rounding");
    (void)EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &atlas_store, .pixels = pixels_span, .colors = colors,
                     .width = BEAD_TEST_SIDE, .height = BEAD_TEST_SIDE, .pad = BEAD_TEST_PAD, .reach = BEAD_TEST_REACH,
                     .cells = moved, .tools = 2u, .lights = lights, .light_count = light_count);

    const unsigned int idle = EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = 0, .one = BEAD_TEST_ONE);
    const unsigned int center = atlas->cell_width / 2u;
    const GuiColor middle = plane_a_at(atlas, 0u, idle, center, center);
    cli_check((GUI_BEAD_DX_OF(middle) == 0) && (GUI_BEAD_DY_OF(middle) == 0) && (GUI_BEAD_MIX_OF(middle) == 0u) &&
                  (GUI_BEAD_MASK_OF(middle) != 0u),
              "the idle frame's center reads itself, sharp, and is drawn");
    const GuiColor inner = plane_a_at(atlas, 0u, idle, BEAD_TEST_PAD + 2u, center);
    cli_check((GUI_BEAD_DX_OF(inner) > 0) && (GUI_BEAD_MIX_OF(inner) == 0u), "the inner left edge reads toward the center");
    const GuiColor outer = plane_a_at(atlas, 0u, idle, BEAD_TEST_PAD - 2u, center);
    cli_check((GUI_BEAD_MIX_OF(outer) > 64u) && (GUI_BEAD_MASK_OF(outer) != 0u), "the ground just outside the bead is frosted");
    // the key light casts the shadow toward the bottom right and the fill light toward the top left: the top right
    // corner lies off both
    cli_check(GUI_BEAD_MASK_OF(plane_a_at(atlas, 0u, idle, atlas->cell_width - 1u, 0u)) == 0u, "the cell's top right corner is untouched");
    unsigned int glint = 0u;
    for (unsigned int y = BEAD_TEST_PAD; y < center; y += 1u)
    {
        for (unsigned int x = BEAD_TEST_PAD; x < center; x += 1u)
        {
            const GuiColor lit = plane_b_at(atlas, 0u, idle, x, y);
            glint = ((GUI_ALPHA_OF(lit) > 32u) && (GUI_RED_OF(lit) > GUI_ALPHA_OF(lit) / 2u)) ? glint + 1u : glint;
        }
    }
    cli_check(glint > 0u, "the key light's glint lights the top left of the dome");
    const GuiColor under = plane_b_at(atlas, 0u, idle, center, BEAD_TEST_PAD + BEAD_TEST_SIDE + 2u);
    cli_check((GUI_ALPHA_OF(under) > 0u) && (GUI_RED_OF(under) < GUI_ALPHA_OF(under) / 2u), "the shadow darkens the glass below the bead");
    cli_check(reach_max(atlas, idle) > 0, "the idle frame refracts");
    cli_check(reach_summed(atlas, GUI_BEAD_FRAMES - 1u) < reach_summed(atlas, idle), "the pressed frame refracts less than the idle one");
    cli_check(reach_max(atlas, idle) <= (int)BEAD_TEST_REACH, "no pixel reads past the reach");
    cli_check(glint_at(atlas, 0u, GUI_BEAD_FRAMES - 1u) != glint_at(atlas, 0u, idle), "the glint moves as the bead sinks under the press");
    cli_check(EMBED_CALL(gui_bead_blit_table.tool_bake, GuiBeadBlitCfg, .atlas = &atlas_store, .tool = 1u) &&
                  (glint_at(atlas, 1u, idle) != glint_at(atlas, 0u, idle)),
              "a button at the window's foot has its glint elsewhere than one at its head");

    cli_check(EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = BEAD_TEST_ONE, .one = BEAD_TEST_ONE) == GUI_BEAD_FRAMES - 1u, "the press whole is the last frame");
    cli_check(EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = -BEAD_TEST_ONE, .one = BEAD_TEST_ONE) == 0u, "a wobble past the first frame is the first frame");
    cli_check((idle > 0u) && (idle < GUI_BEAD_FRAMES / 2u), "rest is near the first frames, the wobble before it");

    // the blit over a flat ground with one bright line inside the bead
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 120u, .height = 120u),
              "a raster is made");
    const GuiColor ground = GUI_RGB(0x20u, 0x20u, 0x40u);
    const GuiColor bright = GUI_RGB(0xffu, 0xffu, 0xffu);
    const int at_x = 40;
    const int at_y = 30;
    // the line is near the bead's left edge, where the low dome bends the read the max
    const int line = at_x + (int)center - 12;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 120, 120}, .color = ground);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){line, 0, 1, 120}, .color = bright);
    EMBED_CALL(gui_bead_blit_table.blit, GuiBeadBlitCfg, .atlas = &atlas_store, .raster = &raster, .x = at_x, .y = at_y,
               .frame = idle, .opacity = 255u);
    const unsigned int row = (unsigned int)(at_y + (int)center);
    cli_check(raster.pixels[((unsigned int)at_y * 120u) + (unsigned int)at_x + atlas->cell_width - 1u] == ground,
              "the cell's untouched corner stays the ground");
    cli_check(raster.pixels[((unsigned int)(at_y - 1) * 120u) + (unsigned int)line] == bright, "the line above the cell is");
    // the dome magnifies: a pixel between the line and the edge reads the line, and the line's own pixel reads
    // nearer the center
    // the light over the read leaves the line's white near white
    unsigned int shifted = 0u;
    for (int x = line - (int)BEAD_TEST_REACH; x < line; x += 1)
    {
        shifted = (GUI_GREEN_OF(raster.pixels[(row * 120u) + (unsigned int)x]) > 0xc0u) ? shifted + 1u : shifted;
    }
    cli_check(shifted > 0u, "a line inside the bead reads again outside itself, away from the center, magnified");
    const unsigned int glint_row = (unsigned int)at_y + BEAD_TEST_PAD + 4u;
    const unsigned int glint_x = (unsigned int)at_x + BEAD_TEST_PAD + 6u;
    cli_check(GUI_RED_OF(raster.pixels[(glint_row * 120u) + glint_x]) > GUI_RED_OF(ground), "the glint lightens the ground under it");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 120, 120}, .color = ground);
    EMBED_CALL(gui_bead_blit_table.blit, GuiBeadBlitCfg, .atlas = &atlas_store, .raster = &raster, .x = at_x, .y = at_y,
               .frame = idle, .opacity = 0u);
    cli_check(raster.pixels[(glint_row * 120u) + glint_x] == ground, "a blit at no opacity positions nothing");
    EMBED_CALL(gui_bead_blit_table.blit, GuiBeadBlitCfg, .atlas = &atlas_store, .raster = &raster, .x = at_x, .y = at_y,
               .frame = idle, .opacity = 255u, .clip = (GuiRect){0, 0, 10, 10});
    cli_check(raster.pixels[(glint_row * 120u) + glint_x] == ground, "a blit positions nothing outside its clip");
    EMBED_CALL(gui_bead_blit_table.blit, GuiBeadBlitCfg, .atlas = &atlas_store, .raster = &raster, .x = -20, .y = -20,
               .frame = idle, .opacity = 255u);
    cli_check(raster.pixels[0] != 0u, "a blit past the raster's corner positions what lands on it");

    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
