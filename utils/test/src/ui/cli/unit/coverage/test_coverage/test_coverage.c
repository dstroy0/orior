// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_coverage.c: the one rasterizer fills paths in 16.16 into a scene and renders the scene once into a target: whole
// pixels inside, shares on the edges, gaps between strips filled by the winding, later paths over earlier ones, and
// nothing written outside the clip
#include "ui/cli/src/coverage/coverage.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define TEST_WIDTH 300u
#define TEST_HEIGHT 12u
#define TEST_ONE 65536

static CliCoverageScene s_scene;
static unsigned int s_target[TEST_WIDTH * TEST_HEIGHT];

// the rectangle from left, top to right, foot in pixels filled in `color`
static void rect_filled(float left, float top, float right, float foot, unsigned int color)
{
    const CliCoveragePoint corner[4] = {{(int)(left * TEST_ONE), (int)(top * TEST_ONE)},
                                        {(int)(right * TEST_ONE), (int)(top * TEST_ONE)},
                                        {(int)(right * TEST_ONE), (int)(foot * TEST_ONE)},
                                        {(int)(left * TEST_ONE), (int)(foot * TEST_ONE)}};
    const unsigned int end = 4u;
    EMBED_CALL(cli_coverage_scene.fill, CliCoverageSceneCfg, .scene = &s_scene, .point = corner, .end = &end,
               .contours = 1u, .color = color);
}

static unsigned int red_at(unsigned int x, unsigned int y)
{
    return (s_target[(y * TEST_WIDTH) + x] >> 16u) & 0xffu;
}

static void rendered(int x, int y, int width, int height)
{
    EMBED_CALL(cli_coverage_scene.render, CliCoverageSceneCfg, .scene = &s_scene, .target = s_target,
               .pitch = TEST_WIDTH, .clip_x = x, .clip_y = y, .clip_width = width, .clip_height = height);
}

void setUp(void)
{
    cli_unity_begun();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_coverage_scene.init, CliCoverageSceneCfg, .scene = &s_scene,
                                .width = TEST_WIDTH, .height = TEST_HEIGHT, .capacity = 1u << 14u));
    for (unsigned int at = 0u; at < TEST_WIDTH * TEST_HEIGHT; at += 1u)
    {
        s_target[at] = 0xff000000u;
    }
}

void tearDown(void)
{
    EMBED_CALL(cli_coverage_scene.free, CliCoverageSceneCfg, .scene = &s_scene);
}

void test_a_pixel_inside_a_rectangle_takes_the_color_whole(void)
{
    rect_filled(1.0f, 1.0f, 5.0f, 5.0f, 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(2u * TEST_WIDTH) + 2u]);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(4u * TEST_WIDTH) + 4u]);
}

void test_a_pixel_outside_a_rectangle_keeps_its_ground(void)
{
    rect_filled(1.0f, 1.0f, 5.0f, 5.0f, 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[0]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(5u * TEST_WIDTH) + 5u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(2u * TEST_WIDTH) + 5u]);
}

void test_a_pixel_an_edge_halves_takes_half_the_color(void)
{
    rect_filled(1.5f, 1.0f, 5.0f, 5.0f, 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_UINT_WITHIN(2u, 128u, red_at(1u, 2u));
}

void test_a_slope_covers_each_pixel_by_its_area(void)
{
    // the triangle (0, 2), (4, 2), (0, 0) with y down: its slope x = 2y crosses two pixels of each row
    const CliCoveragePoint corner[3] = {{0, 2 * TEST_ONE}, {4 * TEST_ONE, 2 * TEST_ONE}, {0, 0}};
    const unsigned int end = 3u;
    EMBED_CALL(cli_coverage_scene.fill, CliCoverageSceneCfg, .scene = &s_scene, .point = corner, .end = &end,
               .contours = 1u, .color = 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_UINT_WITHIN(2u, 255u, red_at(1u, 1u));
    TEST_ASSERT_UINT_WITHIN(2u, 191u, red_at(2u, 1u));
    TEST_ASSERT_UINT_WITHIN(2u, 64u, red_at(3u, 1u));
    TEST_ASSERT_UINT_WITHIN(2u, 191u, red_at(0u, 0u));
    TEST_ASSERT_UINT_WITHIN(2u, 64u, red_at(1u, 0u));
}

void test_a_wide_rectangle_fills_the_gap_between_its_edges_and_across_wide_tiles(void)
{
    rect_filled(2.0f, 1.0f, 290.0f, 7.0f, 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(3u * TEST_WIDTH) + 100u]);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(6u * TEST_WIDTH) + 260u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(3u * TEST_WIDTH) + 295u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(8u * TEST_WIDTH) + 100u]);
}

void test_a_later_path_lies_over_an_earlier_one(void)
{
    rect_filled(0.0f, 0.0f, 8.0f, 8.0f, 0xffff0000u);
    rect_filled(2.0f, 2.0f, 6.0f, 6.0f, 0xff0000ffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xff0000ffu, s_target[(3u * TEST_WIDTH) + 3u]);
    TEST_ASSERT_EQUAL_HEX32(0xffff0000u, s_target[(1u * TEST_WIDTH) + 1u]);
}

void test_a_color_at_half_alpha_blends_over_the_ground(void)
{
    rect_filled(0.0f, 0.0f, 8.0f, 8.0f, 0x80ffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_UINT_WITHIN(2u, 128u, red_at(3u, 3u));
}

void test_a_render_writes_nothing_outside_its_clip(void)
{
    rect_filled(0.0f, 0.0f, 16.0f, 8.0f, 0xffffffffu);
    rendered(4, 4, 4, 4);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(5u * TEST_WIDTH) + 5u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(1u * TEST_WIDTH) + 1u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(5u * TEST_WIDTH) + 10u]);
}

void test_a_cleared_scene_draws_nothing(void)
{
    rect_filled(0.0f, 0.0f, 8.0f, 8.0f, 0xffffffffu);
    EMBED_CALL(cli_coverage_scene.clear, CliCoverageSceneCfg, .scene = &s_scene);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(3u * TEST_WIDTH) + 3u]);
}

void test_a_ring_leaves_its_hole_unfilled(void)
{
    // two contours wound the same way, nested, under nonzero: the hole has winding 2 and fills; wound against each
    // other the hole has winding 0 and stays clear
    const CliCoveragePoint corner[8] = {{0, 0}, {12 * TEST_ONE, 0}, {12 * TEST_ONE, 8 * TEST_ONE}, {0, 8 * TEST_ONE},
                                        {4 * TEST_ONE, 2 * TEST_ONE}, {4 * TEST_ONE, 6 * TEST_ONE},
                                        {8 * TEST_ONE, 6 * TEST_ONE}, {8 * TEST_ONE, 2 * TEST_ONE}};
    const unsigned int end[2] = {4u, 8u};
    EMBED_CALL(cli_coverage_scene.fill, CliCoverageSceneCfg, .scene = &s_scene, .point = corner, .end = end,
               .contours = 2u, .color = 0xffffffffu);
    rendered(0, 0, TEST_WIDTH, TEST_HEIGHT);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(1u * TEST_WIDTH) + 1u]);
    TEST_ASSERT_EQUAL_HEX32(0xff000000u, s_target[(4u * TEST_WIDTH) + 6u]);
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, s_target[(4u * TEST_WIDTH) + 10u]);
}
