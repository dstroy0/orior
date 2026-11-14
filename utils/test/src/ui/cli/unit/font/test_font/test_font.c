// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_font.c: the rasterizer covers a square fully inside and by half on its edge, and a system monospace font draws its glyphs
#include "includes/coverage/coverage.h"
#include "includes/font/font.h"
#include "ui/cli/src/print/font_families.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static FontContours s_contours;
static float s_cover[1u << 20u];
static unsigned char s_scratch[FONT_OUTLINE_POINTS * COVERAGE_EDGE_BYTES];
static char s_paths[4][CLI_PATH_MAX_LEN];
static unsigned char s_bytes[FONT_BYTES];
static unsigned int s_ranges[FONT_RANGES][3];
static Font s_font;

// a square rasterized into `mask`
static void square_rasterized(float left, float bottom, float right, float top, FontMask *mask)
{
    s_contours.points = 4u;
    s_contours.contours = 1u;
    s_contours.end[0] = 4u;
    s_contours.x[0] = left;
    s_contours.y[0] = bottom;
    s_contours.x[1] = right;
    s_contours.y[1] = bottom;
    s_contours.x[2] = right;
    s_contours.y[2] = top;
    s_contours.x[3] = left;
    s_contours.y[3] = top;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = &s_contours, .mask_bits = mask, .cover = s_cover, .capacity = sizeof(s_cover) / sizeof(s_cover[0]), .scratch = s_scratch, .scratch_bytes = sizeof(s_scratch));
}

// the system's monospace font read into s_font, the test ignored where the system has none
static void system_font_read(void)
{
    EMBED_CALL(cli_font_families.monospace, CliFontFamiliesCfg, .paths = s_paths);
    if (s_paths[0][0] == '\0')
    {
        TEST_IGNORE_MESSAGE("the system has no monospace font");
    }
    const unsigned long long size = qry_member_file_read(s_paths[0], s_bytes, sizeof(s_bytes));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    TEST_ASSERT_TRUE(EMBED_CALL(font_data.read, FontDataCfg, .font_out = &s_font, .data = s_bytes, .size = size, .ranges = s_ranges));
}

// the outline of `glyph` at 32 pixels into the contours
static void outlined(unsigned int glyph)
{
    const float scale = 32.0f / s_font.units;
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, 0.0f, 0.0f};
    s_contours.points = 0u;
    s_contours.contours = 0u;
    EMBED_CALL(font_glyph_matrix.outline, FontGlyphMatrixCfg, .font = &s_font, .glyph = glyph, .matrix = matrix, .depth = 0u, .out = &s_contours);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_square_of_four_draws_five_by_four(void)
{
    FontMask mask;
    square_rasterized(0.0f, 0.0f, 4.0f, 4.0f, &mask);
    TEST_ASSERT_EQUAL_UINT64(5ull, mask.width);
    TEST_ASSERT_EQUAL_UINT64(4ull, mask.height);
}

void test_a_pixel_inside_is_fully_covered(void)
{
    FontMask mask;
    square_rasterized(0.0f, 0.0f, 4.0f, 4.0f, &mask);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, mask.cover[mask.width + 1u]);
}

void test_a_pixel_end_the_edge_is_not_covered(void)
{
    FontMask mask;
    square_rasterized(0.0f, 0.0f, 4.0f, 4.0f, &mask);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, mask.cover[mask.width + 4u]);
}

void test_a_pixel_halved_by_the_edge_is_half_covered(void)
{
    FontMask mask;
    square_rasterized(0.5f, 0.0f, 2.0f, 2.0f, &mask);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, mask.cover[0]);
}

// the triangle (0, 0), (4, 0), (0, 2) rasterized into `mask`: its slope x = 4 - 2y crosses two cells of each row
static void triangle_rasterized(FontMask *mask)
{
    s_contours.points = 3u;
    s_contours.contours = 1u;
    s_contours.end[0] = 3u;
    s_contours.x[0] = 0.0f;
    s_contours.y[0] = 0.0f;
    s_contours.x[1] = 4.0f;
    s_contours.y[1] = 0.0f;
    s_contours.x[2] = 0.0f;
    s_contours.y[2] = 2.0f;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = &s_contours, .mask_bits = mask, .cover = s_cover, .capacity = sizeof(s_cover) / sizeof(s_cover[0]), .scratch = s_scratch, .scratch_bytes = sizeof(s_scratch));
}

void test_a_slope_across_two_cells_of_a_row_covers_each_by_its_area(void)
{
    FontMask mask;
    triangle_rasterized(&mask);
    TEST_ASSERT_EQUAL_UINT64(2ull, mask.height);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, mask.cover[(1u * mask.width) + 1u]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.75f, mask.cover[(1u * mask.width) + 2u]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.25f, mask.cover[(1u * mask.width) + 3u]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, mask.cover[(1u * mask.width) + 4u]);
}

void test_a_slope_in_the_top_row_covers_each_cell_by_its_area(void)
{
    FontMask mask;
    triangle_rasterized(&mask);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.75f, mask.cover[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.25f, mask.cover[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, mask.cover[2]);
}

void test_the_system_font_has_a_glyph_for_h(void)
{
    system_font_read();
    TEST_ASSERT_GREATER_THAN_UINT(0u, EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 'H'));
}

void test_the_system_font_is_monospace(void)
{
    system_font_read();
    TEST_ASSERT_EQUAL_FLOAT(EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = &s_font, .glyph = EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 'H')), EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = &s_font, .glyph = EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 'i')));
}

void test_h_draws_ink_and_not_everywhere(void)
{
    system_font_read();
    outlined(EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 'H'));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1u, s_contours.contours);
    FontMask mask;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = &s_contours, .mask_bits = &mask, .cover = s_cover, .capacity = sizeof(s_cover) / sizeof(s_cover[0]), .scratch = s_scratch, .scratch_bytes = sizeof(s_scratch));
    unsigned long long painted = 0ull;
    for (unsigned long long at = 0ull; at < mask.width * mask.height; at += 1ull)
    {
        painted += (mask.cover[at] > 0.5f) ? 1ull : 0ull;
    }
    TEST_ASSERT_GREATER_THAN_UINT64(20ull, painted);
    TEST_ASSERT_LESS_THAN_UINT64(mask.width * mask.height, painted);
}

void test_a_character_the_font_lacks_has_no_glyph(void)
{
    system_font_read();
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 0x10FFFFu));
}

void test_an_accented_character_has_an_outline(void)
{
    system_font_read();
    outlined(EMBED_CALL(font_character.glyph, FontCfg, .font = &s_font, .character = 0xE9u));
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_contours.contours);
}
