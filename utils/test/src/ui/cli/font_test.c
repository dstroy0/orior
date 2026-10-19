// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font_test.c: the font's check: a square is covered whole inside and by half on its edge, and a system monospace font
// draws its letters
#include "../../../../../ui/cli/src/print/font.h"
#include "cli_check.h"

static CliContours s_contours;
static float s_cover[1u << 20u];

// a square of four corners made the only contour
static void font_square(float left, float bottom, float right, float top)
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
}

// the distance between two covers
static float font_off(float one, float other)
{
    return (one > other) ? one - other : other - one;
}

int main(void)
{
    CliMask mask;
    font_square(0.0f, 0.0f, 4.0f, 4.0f);
    cli_font_rasterize(&s_contours, &mask, s_cover, sizeof(s_cover) / sizeof(s_cover[0]));
    cli_check((mask.width == 5ull) && (mask.height == 4ull), "a square of four draws five by four");
    cli_check(font_off(mask.cover[mask.width + 1u], 1.0f) < 1e-4f, "a pixel inside is covered whole");
    cli_check(mask.cover[mask.width + 4u] < 1e-4f, "a pixel past the edge is not covered");
    font_square(0.5f, 0.0f, 2.0f, 2.0f);
    cli_font_rasterize(&s_contours, &mask, s_cover, sizeof(s_cover) / sizeof(s_cover[0]));
    cli_check(font_off(mask.cover[0], 0.5f) < 1e-3f, "a pixel the edge halves is half covered");
    static char s_paths[4][CLI_PATH_LONGEST];
    cli_font_monospace(s_paths);
    if (s_paths[0][0] == '\0')
    {
        cli_check(1, "the system has no monospace font, and none is drawn");
        return cli_checked();
    }
    static unsigned char s_bytes[CLI_FONT_BYTES];
    static unsigned int s_ranges[CLI_FONT_RANGES][3];
    static CliFont s_font;
    const unsigned long long size = qry_member_file_read(s_paths[0], s_bytes, sizeof(s_bytes));
    cli_check((size != QRY_MEMBER_UNREAD) && cli_font_read(&s_font, s_bytes, size, s_ranges), "the font reads");
    const unsigned int glyph = cli_font_glyph(&s_font, 'H');
    cli_check(glyph > 0u, "H has a glyph");
    cli_check(cli_font_advance(&s_font, glyph) == cli_font_advance(&s_font, cli_font_glyph(&s_font, 'i')),
              "the font is monospace");
    const float scale = 32.0f / s_font.units;
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, 0.0f, 0.0f};
    s_contours.points = 0u;
    s_contours.contours = 0u;
    cli_font_outline(&s_font, glyph, matrix, 0u, &s_contours);
    cli_check(s_contours.contours >= 1u, "H has an outline");
    cli_font_rasterize(&s_contours, &mask, s_cover, sizeof(s_cover) / sizeof(s_cover[0]));
    unsigned long long inked = 0ull;
    for (unsigned long long at = 0ull; at < mask.width * mask.height; at += 1ull)
    {
        inked += (mask.cover[at] > 0.5f) ? 1ull : 0ull;
    }
    cli_check((inked > 20ull) && (inked < mask.width * mask.height), "H draws ink, and not everywhere");
    cli_check(cli_font_glyph(&s_font, 0x10FFFFu) == 0u, "a letter the font lacks has no glyph");
    s_contours.points = 0u;
    s_contours.contours = 0u;
    cli_font_outline(&s_font, cli_font_glyph(&s_font, 0xE9u), matrix, 0u, &s_contours);
    cli_check(s_contours.contours > 0u, "an accented letter, often made of other glyphs, has an outline");
    return cli_checked();
}
