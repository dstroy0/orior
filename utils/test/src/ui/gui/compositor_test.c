// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compositor_test.c: the compositor's check, with no window: the block it is placed in, a surface opened all dirty,
// a compose that makes the dirty tiles alone, the same commands recorded again that change nothing, a fill's edge
// covered by its 16.16 share, the layers in their order, the clip ring and its fold, a list recorded on one surface
// replayed on another to the same pixels, an image put, the retained tiles given back, and the vector blend the same
// as the scalar blend. Then the atlas: a reserved range found by its place, a glyph past it by the hash, a run drawn at
// its pens, a run recorded again that changes nothing, the contrast bands, and a full atlas that lets its oldest page
// go and keeps the page a range holds
#include "../../../../../includes/compositor/compositor.h"
#include "../cli/cli_check.h"

#define TEST_WIDTH 200u
#define TEST_HEIGHT 130u
#define TEST_CLEAR GUI_RGB(0x10u, 0x10u, 0x10u)
#define TEST_BACKDROP GUI_RGB(0x20u, 0x40u, 0x60u)
#define TEST_CONTENT GUI_RGB(0xc0u, 0x30u, 0x30u)
#define TEST_CURSOR GUI_RGB(0xf0u, 0xf0u, 0xf0u)
#define TEST_SENTINEL GUI_RGB(0x01u, 0x02u, 0x03u)
#define TEST_BLEND_PIXELS 1027u

static unsigned char s_block[1u << 20u];
static GuiCompositor s_compositor;
static embed_u32 s_first[TEST_WIDTH * TEST_HEIGHT];
static embed_u32 s_second[TEST_WIDTH * TEST_HEIGHT];
static embed_u32 s_target[TEST_BLEND_PIXELS];
static embed_u32 s_scalar[TEST_BLEND_PIXELS];
static embed_u32 s_source[TEST_BLEND_PIXELS];
static unsigned char s_letter[4u * 5u];
static unsigned char s_half[1];
static unsigned char s_large[60u * 60u];
static const embed_u32 s_image[6] = {GUI_RGB(1u, 2u, 3u), GUI_RGB(4u, 5u, 6u), GUI_RGB(7u, 8u, 9u),
                                     GUI_RGB(10u, 11u, 12u), GUI_RGB(13u, 14u, 15u), GUI_RGB(16u, 17u, 18u)};

// the pixel at x, y of `pixels`, a raster of the check's width
static embed_u32 pixel(const embed_u32 *pixels, embed_u32 x, embed_u32 y)
{
    return pixels[(y * TEST_WIDTH) + x];
}

// `source` put over `target`, one pixel, by the scalar blend
static embed_u32 over(embed_u32 target, embed_u32 source)
{
    embed_u32 made = target;
    EMBED_CALL(gui_compositor_blend.over_scalar, GuiCompositorOverCfg, .target = &made, .source = &source, .count = 1u);
    return made;
}

// the layer `layer` of surface 0 recorded with one fill, 16.16 place and size, and ended: the tiles it marked
static embed_u32 filled(embed_u32 layer, embed_i32 x, embed_i32 y, embed_i32 width, embed_i32 height, embed_u32 color)
{
    EMBED_CALL(gui_compositor_record.begin, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
               .layer = layer);
    (void)EMBED_CALL(gui_compositor_record.fill, GuiCompositorFillCfg, .compositor = &s_compositor, .surface = 0u,
                     .layer = layer, .x = x, .y = y, .width = width, .height = height, .color = color);
    return EMBED_CALL(gui_compositor_record.end, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
                      .layer = layer);
}

// the dirty tiles of `surface` made into `pixels`
static embed_u32 composed(embed_u32 surface, embed_u32 *pixels)
{
    GuiRaster raster = {TEST_WIDTH, TEST_HEIGHT, pixels, NULL, 0ULL, 0ULL, {0, 0, 0, 0}};
    return EMBED_CALL(gui_compositor.compose, GuiCompositorComposeCfg, .compositor = &s_compositor, .surface = surface,
                      .raster = &raster);
}

// the rectangle pushed on surface 0's clip ring
static void damaged(embed_i32 x, embed_i32 y, embed_i32 width, embed_i32 height)
{
    EMBED_CALL(gui_compositor.damage, GuiCompositorDamageCfg, .compositor = &s_compositor, .surface = 0u,
               .rect = (GuiCompositorRect){x, y, width, height});
}

// whether the vector blend and the scalar blend give the same pixels over pixels of every alpha
static int blends_same(void)
{
    embed_u32 seed = 0x2545f491u;
    for (embed_u32 at = 0u; at < TEST_BLEND_PIXELS; at += 1u)
    {
        seed = (seed * 1664525u) + 1013904223u;
        const embed_u32 alpha = (seed >> 24u) & 0xffu;
        // a pixel multiplied by its alpha has no channel above its alpha
        const embed_u32 red = ((seed >> 16u) & 0xffu) % (alpha + 1u);
        const embed_u32 green = ((seed >> 8u) & 0xffu) % (alpha + 1u);
        const embed_u32 blue = (seed & 0xffu) % (alpha + 1u);
        s_source[at] = (alpha << 24u) | (red << 16u) | (green << 8u) | blue;
        seed = (seed * 1664525u) + 1013904223u;
        s_target[at] = seed | 0xff000000u;
        s_scalar[at] = s_target[at];
    }
    EMBED_CALL(gui_compositor_blend.over, GuiCompositorOverCfg, .target = s_target, .source = s_source,
               .count = TEST_BLEND_PIXELS);
    EMBED_CALL(gui_compositor_blend.over_scalar, GuiCompositorOverCfg, .target = s_scalar, .source = s_source,
               .count = TEST_BLEND_PIXELS);
    int same = 1;
    for (embed_u32 at = 0u; at < TEST_BLEND_PIXELS; at += 1u)
    {
        same &= (s_target[at] == s_scalar[at]);
    }
    return same;
}

// `color` with each channel scaled by `cover` of 256, as the compositor scales a covered pixel
static embed_u32 covered(embed_u32 color, embed_u32 cover)
{
    const embed_u32 red_blue = (((color & 0x00ff00ffu) * cover) >> 8u) & 0x00ff00ffu;
    const embed_u32 alpha_green = ((((color >> 8u) & 0x00ff00ffu) * cover) >> 8u) & 0x00ff00ffu;
    return (cover >= 256u) ? color : (red_blue | (alpha_green << 8u));
}

// the glyph layer of surface 0 recorded with the run `glyphs` in `color`, and ended: the tiles it marked
static embed_u32 ran(const GuiCompositorGlyph *glyphs, embed_u32 count, embed_u32 color)
{
    EMBED_CALL(gui_compositor_record.begin, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
               .layer = GUI_COMPOSITOR_GLYPHS);
    (void)EMBED_CALL(gui_compositor_record.glyphs, GuiCompositorGlyphsCfg, .compositor = &s_compositor, .surface = 0u,
                     .layer = GUI_COMPOSITOR_GLYPHS, .color = color, .glyphs = glyphs, .count = count);
    return EMBED_CALL(gui_compositor_record.end, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
                      .layer = GUI_COMPOSITOR_GLYPHS);
}

// the glyph `glyph` of face 0 at size 12 baked at `phase` from `coverage`: its entry
static embed_u32 baked(embed_u32 face, embed_u32 glyph, embed_u32 phase, const unsigned char *coverage,
                       embed_u32 width, embed_u32 height)
{
    return EMBED_CALL(gui_compositor_atlas.bake, GuiCompositorGlyphBakeCfg, .compositor = &s_compositor, .face = face,
                      .size = 12u, .glyph = glyph, .phase = phase, .coverage = coverage, .width = width,
                      .height = height, .left = 1, .top = 5, .advance = GUI_COMPOSITOR_FIXED(6));
}

// the atlas's range, hash, runs and contrast bands, on surface 0 over its backdrop
static void glyphs_checked(void)
{
    for (embed_u32 at = 0u; at < sizeof(s_letter); at += 1u)
    {
        s_letter[at] = 255u;
    }
    s_half[0] = 128u;
    const GuiCompositorGlyphFindCfg first = {&s_compositor, 0u, 12u, 'A', 0u};
    cli_check(EMBED_CALL(gui_compositor_atlas.range, GuiCompositorGlyphRangeCfg, .compositor = &s_compositor,
                         .face = 0u, .size = 12u, .first = 'A', .count = 4u) &&
                  (gui_compositor_atlas.find(&first) == GUI_COMPOSITOR_NO_GLYPH),
              "a range is reserved, and a glyph of it not yet baked is found as none");
    const embed_u32 letter = baked(0u, 'A', 0u, s_letter, 4u, 5u);
    const embed_u32 second = baked(0u, 'B', 2u, s_letter, 4u, 5u);
    const GuiCompositorGlyphFindCfg second_find = {&s_compositor, 0u, 12u, 'B', 2u};
    cli_check((letter == 0u) && (second == ((1u * GUI_COMPOSITOR_PHASES) + 2u)) &&
                  (gui_compositor_atlas.find(&first) == letter) && (gui_compositor_atlas.find(&second_find) == second),
              "a range's glyph takes the entry at its place in the range, four places a glyph");
    const embed_u32 hashed = baked(1u, 1000u, 0u, s_half, 1u, 1u);
    const GuiCompositorGlyphFindCfg hashed_find = {&s_compositor, 1u, 12u, 1000u, 0u};
    cli_check((hashed >= (4u * GUI_COMPOSITOR_PHASES)) && (gui_compositor_atlas.find(&hashed_find) == hashed),
              "a glyph past the ranges takes an entry after them and is found by the hash");

    const GuiCompositorGlyph run[2] = {{GUI_COMPOSITOR_FIXED(10), GUI_COMPOSITOR_FIXED(40), letter},
                                       {GUI_COMPOSITOR_FIXED(16), GUI_COMPOSITOR_FIXED(40), second}};
    cli_check((ran(run, 2u, TEST_CURSOR) == 1u) && (composed(0u, s_first) == 1u) &&
                  (pixel(s_first, 11u, 35u) == TEST_CURSOR) && (pixel(s_first, 14u, 39u) == TEST_CURSOR) &&
                  (pixel(s_first, 17u, 35u) == TEST_CURSOR) && (pixel(s_first, 10u, 35u) == TEST_BACKDROP) &&
                  (pixel(s_first, 11u, 40u) == TEST_BACKDROP),
              "a run draws each glyph's box from its pen: left of it by its left, up from the baseline by its top");
    const GuiCompositorGlyph other[2] = {{GUI_COMPOSITOR_FIXED(10), GUI_COMPOSITOR_FIXED(40), letter},
                                         {GUI_COMPOSITOR_FIXED(16), GUI_COMPOSITOR_FIXED(40), letter}};
    cli_check((ran(run, 2u, TEST_CURSOR) == 0u) && (ran(other, 2u, TEST_CURSOR) == 1u),
              "a run recorded again marks nothing, and a run with another glyph in it marks its tile");

    const GuiCompositorGlyph half_run[1] = {{GUI_COMPOSITOR_FIXED(30), GUI_COMPOSITOR_FIXED(40), hashed}};
    const embed_u32 white = GUI_RGB(0xffu, 0xffu, 0xffu);
    (void)ran(half_run, 1u, white);
    (void)composed(0u, s_first);
    const embed_u32 far = pixel(s_first, 31u, 35u);
    const embed_u32 near_color = GUI_RGB(0x28u, 0x48u, 0x68u);
    (void)ran(half_run, 1u, near_color);
    (void)composed(0u, s_first);
    const embed_u32 near = pixel(s_first, 31u, 35u);
    const embed_u32 lift_far = s_compositor.contrast[0][128];
    const embed_u32 lift_near = s_compositor.contrast[3][128];
    cli_check((lift_far == 128u) && (lift_near > 160u) &&
                  (far == over(TEST_BACKDROP, covered(white, lift_far + (lift_far >> 7u)))) &&
                  (near == over(TEST_BACKDROP, covered(near_color, lift_near + (lift_near >> 7u)))),
              "text far from its ground's brightness takes its coverage as it is, and text near it is lifted more");
    (void)ran(run, 2u, TEST_CURSOR);
    (void)composed(0u, s_first);
}

// a full atlas: a glyph that fits no page lets the page drawn from least recently go, and never the page a range holds
static void evictions_checked(void)
{
    for (embed_u32 at = 0u; at < sizeof(s_large); at += 1u)
    {
        s_large[at] = 200u;
    }
    const embed_u32 one = baked(2u, 1u, 0u, s_large, 60u, 60u);
    const embed_u32 two = baked(2u, 2u, 0u, s_large, 60u, 60u);
    const GuiCompositorGlyphFindCfg one_find = {&s_compositor, 2u, 12u, 1u, 0u};
    const GuiCompositorGlyphFindCfg two_find = {&s_compositor, 2u, 12u, 2u, 0u};
    const GuiCompositorGlyphFindCfg letter_find = {&s_compositor, 0u, 12u, 'A', 0u};
    cli_check((one != GUI_COMPOSITOR_NO_GLYPH) && (two != GUI_COMPOSITOR_NO_GLYPH) &&
                  (gui_compositor_atlas.find(&one_find) == GUI_COMPOSITOR_NO_GLYPH) &&
                  (gui_compositor_atlas.find(&two_find) == two) && (gui_compositor_atlas.find(&letter_find) == 0u) &&
                  s_compositor.page[0].pinned,
              "a glyph too large for the room left lets the other page go, its glyphs with it, and the range's page stays");
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    const GuiCompositorLimitsCfg limits = {256u, 192u, 64u, 4u, 32u, 64u, 2u, 64u, 64u};
    const embed_u64 bytes = gui_compositor.bytes(&limits);
    GuiCompositorLimitsCfg none = limits;
    none.tiles_max = 0u;
    GuiCompositorLimitsCfg uneven = limits;
    uneven.atlas_side = 100u;
    cli_check((bytes != 0ULL) && (bytes <= sizeof(s_block)) && (gui_compositor.bytes(&none) == 0ULL) &&
                  (gui_compositor.bytes(&uneven) == 0ULL),
              "a block is sized for its limits, and a limit of 0 or a page side not a power of 2 sizes none");
    cli_check(!EMBED_CALL(gui_compositor.init, GuiCompositorInitCfg, .compositor = &s_compositor, .limits = limits,
                          .block = s_block, .bytes = bytes - 1ULL) &&
                  EMBED_CALL(gui_compositor.init, GuiCompositorInitCfg, .compositor = &s_compositor, .limits = limits,
                             .block = s_block, .bytes = sizeof(s_block)),
              "a block a byte short places nothing, and one of the size places the compositor");

    const embed_u32 retained = 1u << GUI_COMPOSITOR_BACKDROP;
    cli_check(EMBED_CALL(gui_compositor_surface.open, GuiCompositorSurfaceCfg, .compositor = &s_compositor,
                         .surface = 0u, .width = TEST_WIDTH, .height = TEST_HEIGHT, .clear = TEST_CLEAR,
                         .retained = retained) &&
                  (s_compositor.surface[0].columns == 4u) && (s_compositor.surface[0].rows == 3u) &&
                  !EMBED_CALL(gui_compositor_surface.open, GuiCompositorSurfaceCfg, .compositor = &s_compositor,
                              .surface = 1u, .width = 257u, .height = 10u, .clear = TEST_CLEAR),
              "a surface opens over a grid of 64-pixel tiles, and one wider than the limits does not");
    cli_check((composed(0u, s_first) == 12u) && (pixel(s_first, 0u, 0u) == TEST_CLEAR) &&
                  (pixel(s_first, TEST_WIDTH - 1u, TEST_HEIGHT - 1u) == TEST_CLEAR),
              "a surface opened is all dirty: the first compose makes every tile, each in the clear color");
    s_first[0] = TEST_SENTINEL;
    cli_check((composed(0u, s_first) == 0u) && (pixel(s_first, 0u, 0u) == TEST_SENTINEL),
              "a compose with nothing dirty makes no tile and writes no pixel");

    cli_check(filled(GUI_COMPOSITOR_BACKDROP, 0, 0, GUI_COMPOSITOR_FIXED(TEST_WIDTH), GUI_COMPOSITOR_FIXED(TEST_HEIGHT),
                     TEST_BACKDROP) == 12u,
              "a fill over the whole surface marks every tile of its layer");
    cli_check((composed(0u, s_first) == 12u) && (pixel(s_first, 5u, 5u) == TEST_BACKDROP) &&
                  (s_compositor.free_count == (32u - 12u)),
              "the backdrop is composed, and its retained layer takes a tile from the pool for each cell");
    cli_check((filled(GUI_COMPOSITOR_BACKDROP, 0, 0, GUI_COMPOSITOR_FIXED(TEST_WIDTH),
                      GUI_COMPOSITOR_FIXED(TEST_HEIGHT), TEST_BACKDROP) == 0u) &&
                  (composed(0u, s_first) == 0u),
              "the same commands recorded again mark nothing, and the compose makes nothing");

    // a fill from 70.5 to 90.5 across and 10 to 20 down: its left pixel half covered, all of it inside tile 1, 0
    cli_check(filled(GUI_COMPOSITOR_CONTENT, GUI_COMPOSITOR_FIXED(70) + 32768, GUI_COMPOSITOR_FIXED(10),
                     GUI_COMPOSITOR_FIXED(20), GUI_COMPOSITOR_FIXED(10), TEST_CONTENT) == 1u,
              "a fill inside one tile marks that tile alone");
    s_first[(12u * TEST_WIDTH) + 10u] = TEST_SENTINEL;
    const embed_u32 half = ((TEST_CONTENT & 0x00ff00ffu) >> 1u) & 0x00ff00ffu;
    const embed_u32 half_color = half | (((((TEST_CONTENT >> 8u) & 0x00ff00ffu) >> 1u) & 0x00ff00ffu) << 8u);
    cli_check((composed(0u, s_first) == 1u) && (pixel(s_first, 75u, 12u) == TEST_CONTENT) &&
                  (pixel(s_first, 70u, 12u) == over(TEST_BACKDROP, half_color)) &&
                  (pixel(s_first, 10u, 12u) == TEST_SENTINEL),
              "the content's tile alone is composed: a full pixel its color, the half-covered edge half of it");

    cli_check((filled(GUI_COMPOSITOR_CURSOR, GUI_COMPOSITOR_FIXED(75), GUI_COMPOSITOR_FIXED(12),
                      GUI_COMPOSITOR_FIXED(2), GUI_COMPOSITOR_FIXED(2), TEST_CURSOR) == 1u) &&
                  (composed(0u, s_first) == 1u) && (pixel(s_first, 75u, 12u) == TEST_CURSOR),
              "the cursor's layer is composed over the content's");
    EMBED_CALL(gui_compositor_record.begin, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
               .layer = GUI_COMPOSITOR_CURSOR);
    cli_check((EMBED_CALL(gui_compositor_record.end, GuiCompositorLayerCfg, .compositor = &s_compositor,
                          .surface = 0u, .layer = GUI_COMPOSITOR_CURSOR) == 1u) &&
                  (composed(0u, s_first) == 1u) && (pixel(s_first, 75u, 12u) == TEST_CONTENT),
              "the cursor's layer recorded empty marks the tile it left, and the content shows there again");

    s_first[(100u * TEST_WIDTH) + 150u] = TEST_SENTINEL;
    damaged(150, 100, 1, 1);
    cli_check((composed(0u, s_first) == 1u) && (pixel(s_first, 150u, 100u) == TEST_BACKDROP),
              "a rectangle pushed on the clip ring makes its tile again");
    damaged(0, 0, 1, 1);
    damaged(64, 0, 1, 1);
    damaged(128, 0, 1, 1);
    damaged(192, 0, 1, 1);
    damaged(0, 64, 1, 1);
    damaged(64, 64, 1, 1);
    cli_check(composed(0u, s_first) == 6u, "a ring of four that takes six rectangles folds and loses none");

    cli_check(EMBED_CALL(gui_compositor.image, GuiCompositorImageCfg, .compositor = &s_compositor, .image = 0u,
                         .pixels = s_image, .width = 3u, .height = 2u) &&
                  !EMBED_CALL(gui_compositor.image, GuiCompositorImageCfg, .compositor = &s_compositor,
                              .image = GUI_COMPOSITOR_IMAGES, .pixels = s_image, .width = 3u, .height = 2u),
              "an image is named by its handle, and a handle past the table names none");
    EMBED_CALL(gui_compositor_record.begin, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
               .layer = GUI_COMPOSITOR_OVERLAY);
    (void)EMBED_CALL(gui_compositor_record.put, GuiCompositorPutCfg, .compositor = &s_compositor, .surface = 0u,
                     .layer = GUI_COMPOSITOR_OVERLAY, .x = GUI_COMPOSITOR_FIXED(100) + 40000,
                     .y = GUI_COMPOSITOR_FIXED(100), .image = 0u);
    (void)EMBED_CALL(gui_compositor_record.end, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 0u,
                     .layer = GUI_COMPOSITOR_OVERLAY);
    cli_check((composed(0u, s_first) == 1u) && (pixel(s_first, 100u, 100u) == s_image[0]) &&
                  (pixel(s_first, 102u, 101u) == s_image[5]) && (pixel(s_first, 103u, 101u) == TEST_BACKDROP),
              "an image is put with its top left at the place's pixel, the fraction dropped");

    glyphs_checked();
    (void)EMBED_CALL(gui_compositor_surface.open, GuiCompositorSurfaceCfg, .compositor = &s_compositor,
                     .surface = 1u, .width = TEST_WIDTH, .height = TEST_HEIGHT, .clear = TEST_CLEAR,
                     .retained = retained);
    for (embed_u32 layer = 0u; layer < GUI_COMPOSITOR_LAYERS; layer += 1u)
    {
        const GuiCompositorLayer *const shown = &s_compositor.surface[0].layer[layer];
        EMBED_CALL(gui_compositor_record.begin, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 1u,
                   .layer = layer);
        (void)EMBED_CALL(gui_compositor_record.replay, GuiCompositorReplayCfg, .compositor = &s_compositor,
                         .surface = 1u, .layer = layer, .commands = shown->shown, .count = shown->shown_count,
                         .glyphs = shown->glyphs_shown, .glyph_count = shown->glyphs_shown_count);
        (void)EMBED_CALL(gui_compositor_record.end, GuiCompositorLayerCfg, .compositor = &s_compositor, .surface = 1u,
                         .layer = layer);
    }
    (void)composed(1u, s_second);
    int same = 1;
    for (embed_u32 at = 0u; at < (TEST_WIDTH * TEST_HEIGHT); at += 1u)
    {
        same &= (s_first[at] == s_second[at]);
    }
    cli_check(same, "every layer's list replayed on a second surface makes the same pixels");

    EMBED_CALL(gui_compositor_surface.shut, GuiCompositorSurfaceCfg, .compositor = &s_compositor, .surface = 0u);
    EMBED_CALL(gui_compositor_surface.shut, GuiCompositorSurfaceCfg, .compositor = &s_compositor, .surface = 1u);
    cli_check(s_compositor.free_count == 32u, "the surfaces shut give every retained tile back to the pool");

    cli_check(blends_same(), "the vector blend gives the same pixels as the scalar blend over every alpha");
    evictions_checked();
    return cli_on_check();
}
