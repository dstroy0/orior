// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_test.c: the raster's check: a raster made is clear, a fill colors its rectangle and nothing about it, is
// clipped to the raster and blends by its alpha, a mask blends by its coverage inside the clip, two rectangles meet in
// their overlap, a rounded fill leaves its corners short, a line colors its ends and a dot is soft-edged
#include "../../../../../ui/gui/src/raster.h"
#include "../cli/cli_check.h"

#include <math.h>

static void row_laid(GuiRaster *raster, const unsigned char *cover, unsigned int width, int left, int right, int y, GuiColor color);

// the red channel of a pixel
static unsigned int red_of(GuiColor color)
{
    return (color >> 16u) & 0xffu;
}

// a row of coverage stretched between two places: laid one pixel to one byte, spread, squeezed and on fractions
static void rows_checked(void)
{
    const unsigned char row[2] = {255u, 0u};
    const unsigned char full[1] = {255u};
    const GuiColor over = GUI_RGB(0xffu, 0xffu, 0xffu);
    const GuiColor under = GUI_RGB(0u, 0u, 0u);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 8u, .height = 2u), "a raster for the rows is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 2}, .color = under);
    row_laid(&raster, row, 2u, 1 << 16, 3 << 16, 0, over);
    cli_check((raster.pixels[0] == under) && (raster.pixels[1] == over) && (raster.pixels[2] == under), "a row laid one to one is its bytes");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 2}, .color = under);
    row_laid(&raster, row, 2u, 0, 4 << 16, 1, over);
    cli_check((raster.pixels[8] == over) && (raster.pixels[9] == over) && (raster.pixels[10] == under) && (raster.pixels[11] == under),
              "a row spread to twice its width takes two pixels a byte");
    cli_check(raster.pixels[0] == under, "and touches its own row alone");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 2}, .color = under);
    row_laid(&raster, row, 2u, 0, 1 << 16, 0, over);
    cli_check((red_of(raster.pixels[0]) >= 127u) && (red_of(raster.pixels[0]) <= 128u) && (raster.pixels[1] == under),
              "a row squeezed to one pixel is the mean of its bytes");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 2}, .color = under);
    row_laid(&raster, full, 1u, 1 << 15, (1 << 16) + (1 << 15), 0, over);
    cli_check((red_of(raster.pixels[0]) >= 127u) && (red_of(raster.pixels[0]) <= 128u) && (red_of(raster.pixels[1]) >= 127u) &&
                  (red_of(raster.pixels[1]) <= 128u) && (raster.pixels[2] == under),
              "a pixel a row's end falls in takes its share");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 2}, .color = under);
    row_laid(&raster, full, 1u, -(4 << 16), 20 << 16, 0, over);
    cli_check((raster.pixels[0] == over) && (raster.pixels[7] == over), "a row is clipped to the raster's sides");
    row_laid(&raster, full, 1u, 0, 8 << 16, 5, over);
    row_laid(&raster, full, 1u, 0, 8 << 16, -1, over);
    row_laid(&raster, full, 1u, 5 << 16, 5 << 16, 1, over);
    cli_check(raster.pixels[13] == under, "a row off the raster or of no width lays nothing");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
}

// a picture with its own alpha: masks laid over it save it straight, and it is laid by its alpha and an opacity
static void pictures_checked(void)
{
    const GuiColor under = GUI_RGB(0u, 0u, 0u);
    GuiRaster raster;
    GuiRaster picture;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &picture, .width = 2u, .height = 1u), "a picture is made clear");
    unsigned char solid[2] = {255u, 255u};
    const GuiMask both = {2u, 1u, 0, 0, solid};
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &picture, .mask = &both, .x = 0, .y = 0, .color = GUI_RGBA(0x80u, 0u, 0u, 0x80u));
    cli_check(picture.pixels[0] == GUI_RGBA(0x80u, 0u, 0u, 0x80u), "a mask over clear is its color straight");
    unsigned char whole[1] = {255u};
    const GuiMask one = {1u, 1u, 0, 0, whole};
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &picture, .mask = &one, .x = 1, .y = 0, .color = GUI_RGB(0u, 0u, 0xffu));
    cli_check(picture.pixels[1] == GUI_RGB(0u, 0u, 0xffu), "an opaque mask takes the pixel whole");
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &picture, .mask = &one, .x = 5, .y = 0, .color = GUI_RGB(0u, 0u, 0xffu));
    cli_check(picture.pixels[0] == GUI_RGBA(0x80u, 0u, 0u, 0x80u), "a mask off the picture lays nothing");
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &picture, .mask = &one, .x = 0, .y = 0, .color = GUI_RGBA(0u, 0xffu, 0u, 0x80u));
    cli_check((GUI_ALPHA_OF(picture.pixels[0]) >= 0xbfu) && (GUI_ALPHA_OF(picture.pixels[0]) <= 0xc1u) &&
                  (GUI_GREEN_OF(picture.pixels[0]) > GUI_RED_OF(picture.pixels[0])),
              "a mask over part of an alpha adds to the alpha and leads the color");
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 4u, .height = 1u), "a raster for the picture is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 4, 1}, .color = under);
    EMBED_CALL(gui_raster_x.picture_layout, GuiRasterXCfg, .raster = &raster, .picture_read = &picture, .x = 1, .y = 0, .opacity = 255u);
    cli_check((raster.pixels[0] == under) && (raster.pixels[2] == GUI_RGB(0u, 0u, 0xffu)) && (raster.pixels[3] == under),
              "a picture is laid at its place, an opaque pixel whole");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 4, 1}, .color = under);
    EMBED_CALL(gui_raster_x.picture_layout, GuiRasterXCfg, .raster = &raster, .picture_read = &picture, .x = 1, .y = 0, .opacity = 128u);
    cli_check((GUI_BLUE_OF(raster.pixels[2]) >= 127u) && (GUI_BLUE_OF(raster.pixels[2]) <= 129u), "an opacity takes its share of it");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 4, 1}, .color = under);
    EMBED_CALL(gui_raster_x.picture_layout, GuiRasterXCfg, .raster = &raster, .picture_read = &picture, .x = 3, .y = 0, .opacity = 255u);
    cli_check((raster.pixels[3] != under) && (raster.pixels[0] == under), "a picture is clipped to the raster");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 4, 1}, .color = under);
    EMBED_CALL(gui_raster_x.picture_layout, GuiRasterXCfg, .raster = &raster, .picture_read = &picture, .x = -9, .y = 0, .opacity = 255u);
    cli_check(raster.pixels[0] == under, "and one off it lays nothing");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &picture);
}

// a row stretched to coverage and that coverage laid, as a stage of a movement is made and blitted
static void row_laid(GuiRaster *raster, const unsigned char *cover, unsigned int width, int left, int right, int y, GuiColor color)
{
    static unsigned char s_out[256];
    int x = 0;
    const unsigned int count = EMBED_CALL(gui_raster_width.row_cover, GuiRasterWidthCfg, .cover = cover, .width = width, .left = left, .right = right, .raster_width = raster->width, .out = s_out, .x_pointer = &x);
    EMBED_CALL(gui_raster_xy_color_count.span_layout, GuiRasterXYColorCountCfg, .raster = raster, .x = x, .y = y, .cover = s_out, .count = count, .color = color);
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 8u, .height = 4u), "a raster of eight by four is made");
    cli_check((raster.width == 8u) && (raster.height == 4u), "a raster made keeps its size");
    cli_check((raster.pixels[0] == 0u) && (raster.pixels[31] == 0u), "a raster made is clear");

    const GuiColor ink = GUI_RGB(0x10u, 0x20u, 0x30u);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){2, 1, 3, 2}, .color = ink);
    cli_check(raster.pixels[(1u * 8u) + 2u] == 0xff102030u, "a fill colors its top left pixel, opaque");
    cli_check(raster.pixels[(2u * 8u) + 4u] == 0xff102030u, "a fill colors its bottom right pixel");
    cli_check((raster.pixels[(1u * 8u) + 1u] == 0u) && (raster.pixels[(1u * 8u) + 5u] == 0u) &&
                  (raster.pixels[2u] == 0u) && (raster.pixels[(3u * 8u) + 2u] == 0u),
              "a fill leaves the pixels about its rectangle");

    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){-2, -2, 100, 100}, .color = black);
    cli_check((raster.pixels[0] == 0xff000000u) && (raster.pixels[31] == 0xff000000u),
              "a fill past the raster's edges is clipped to the raster");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 8, 4}, .color = GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u));
    cli_check(red_of(raster.pixels[0]) == 0x80u, "a half-clear white over black is mid gray");

    // every blend is exact: each channel the color's share of 255 over the ground's rest, rounded, over every ground
    // value, color value and share, the three channels given different values at once
    unsigned long long wrong = 0ull;
    for (unsigned int under = 0u; under < 256u; under += 1u)
    {
        for (unsigned int over = 0u; over < 256u; over += 1u)
        {
            for (unsigned int share = 0u; share < 256u; share += 1u)
            {
                const unsigned int ground_red = under;
                const unsigned int ground_green = 255u - under;
                const unsigned int ground_blue = (under * 7u) & 255u;
                const unsigned int color_red = over;
                const unsigned int color_green = (over * 3u) & 255u;
                const unsigned int color_blue = 255u - over;
                raster.pixels[0] = GUI_RGB(ground_red, ground_green, ground_blue);
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1, 1}, .color = GUI_RGBA(color_red, color_green, color_blue, share));
                const GuiColor wanted = GUI_RGB(((color_red * share) + (ground_red * (255u - share)) + 127u) / 255u,
                                                ((color_green * share) + (ground_green * (255u - share)) + 127u) / 255u,
                                                ((color_blue * share) + (ground_blue * (255u - share)) + 127u) / 255u);
                wrong += (raster.pixels[0] != wanted) ? 1ull : 0ull;
            }
        }
    }
    cli_check(wrong == 0ull, "every blend of every channel value by every share is the exact share of 255");

    unsigned char cover[4] = {255u, 0u, 128u, 64u};
    const GuiMask mask = {2u, 2u, 0, 0, cover};
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    const GuiRect whole = {0, 0, 8, 4};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = &raster, .mask = &mask, .x = 3, .y = 1, .color = white, .clip = whole);
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a pixel a mask covers whole takes the color whole");
    cli_check(raster.pixels[(1u * 8u) + 4u] == 0xff000000u, "a pixel a mask leaves bare keeps its ground");
    cli_check(red_of(raster.pixels[(2u * 8u) + 3u]) == 0x80u, "a pixel a mask covers by half takes half the color");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = &raster, .mask = &mask, .x = 3, .y = 1, .color = white, .clip = (GuiRect){4, 0, 4, 4});
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xff000000u, "a mask draws nothing outside the clip");
    cli_check(raster.pixels[(1u * 8u) + 4u] == 0xff000000u, "a mask's bare pixel inside the clip stays bare");
    EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = &raster, .mask = &mask, .x = -1, .y = -1, .color = white, .clip = whole);
    cli_check(red_of(raster.pixels[0]) == 0x40u, "a mask partly off the raster draws the pixel of it that is on");

    GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = whole, .other = (GuiRect){5, 2, 10, 10});
    cli_check((met.x == 5) && (met.y == 2) && (met.width == 3) && (met.height == 2),
              "two rectangles meet in their overlap");
    met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = (GuiRect){0, 0, 2, 2}, .other = (GuiRect){5, 5, 1, 1});
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met), "rectangles apart meet in none");
    cli_check(EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = whole, .x = 7, .y = 3) && !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = whole, .x = 8, .y = 3), "a rectangle holds its last pixel, not the one after");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .radius = 2u, .color = white);
    cli_check(raster.pixels[0] != 0xffffffffu, "a rounded fill leaves its corner pixel short of the color");
    cli_check(raster.pixels[0] != 0xff000000u, "a rounded fill covers its corner pixel in part");
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a rounded fill colors its middle whole");
    cli_check(raster.pixels[3u] == 0xffffffffu, "a rounded fill colors its top edge between the corners whole");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = white);
    const GuiUnder plain = {NULL, black};
    EMBED_CALL(gui_raster_rect_radius_ground.corners_cut, GuiRasterRectRadiusGroundCfg, .raster = &raster, .rect = whole, .radius = 2u, .ground = plain);
    cli_check(red_of(raster.pixels[0]) < red_of(raster.pixels[1u]), "a corner cut takes more ground at the corner pixel than beside it");
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a corner cut leaves the middle");
    cli_check((raster.pixels[1u] != 0xffffffffu) && (raster.pixels[1u] != 0xff000000u), "a corner cut blends along the arc");

    // a corner cut to a ground raster takes that raster's pixel, with a color over it where one is given
    GuiRaster under;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &under, .width = 8u, .height = 4u), "a ground raster is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &under, .rect = whole, .color = GUI_RGB(0x00u, 0x80u, 0x00u));
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = white);
    EMBED_CALL(gui_raster_rect_radius_ground.corners_cut, GuiRasterRectRadiusGroundCfg, .raster = &raster, .rect = whole, .radius = 3u, .ground = (GuiUnder){&under, 0u});
    cli_check((red_of(raster.pixels[0]) < 0x40u) && (((raster.pixels[0] >> 8u) & 0xffu) > 0x40u), "a corner cut to a ground raster lays its pixel bare");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = white);
    EMBED_CALL(gui_raster_rect_radius_ground.corners_cut, GuiRasterRectRadiusGroundCfg, .raster = &raster, .rect = whole, .radius = 3u, .ground = (GuiUnder){&under, GUI_RGBA(0xffu, 0x00u, 0x00u, 0x80u)});
    cli_check((red_of(raster.pixels[0]) > 0x40u) && (((raster.pixels[0] >> 8u) & 0xffu) > 0x20u), "a color over the ground raster is blended into the corner");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &under);

    // the grain: a tile of alphas laid from the raster's top left, repeated, inside the corners
    const unsigned char grain[4] = {0u, 255u, 0u, 0u};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_rect_radius.grain, GuiRasterRectRadiusCfg, .raster = &raster, .rect = whole, .radius = 0u, .alpha = grain, .tile = 2u);
    cli_check((raster.pixels[1u] == 0xffffffffu) && (raster.pixels[0] == 0xff000000u) && (raster.pixels[3u] == 0xffffffffu) &&
                  (raster.pixels[(1u * 8u) + 1u] == 0xff000000u),
              "the grain lays its tile's alphas over the raster, repeated");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_rect_radius.grain, GuiRasterRectRadiusCfg, .raster = &raster, .rect = whole, .radius = 3u, .alpha = grain, .tile = 2u);
    cli_check(red_of(raster.pixels[1u]) < 0xffu, "the grain is short at a rounded corner");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 40u, .height = 30u), "a raster of forty by thirty is made for a notch");
    const GuiRect big = {0, 0, 40, 30};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = big, .color = white);
    EMBED_CALL(gui_raster_radius_ground.notch, GuiRasterRadiusGroundCfg, .raster = &raster, .work = big, .notch = (GuiRect){10, 0, 20, 12}, .radius = 4u, .ground = plain);
    cli_check(raster.pixels[(5u * 40u) + 20u] == 0xff000000u, "a notch at the top is cut to the ground");
    cli_check(raster.pixels[(5u * 40u) + 5u] == 0xffffffffu, "the work beside the notch stands");
    cli_check(raster.pixels[(20u * 40u) + 20u] == 0xffffffffu, "the work below the notch stands");
    cli_check(red_of(raster.pixels[(11u * 40u) + 10u]) > 0x80u, "the turn into the notch's corner inside the work is filleted, the work's color left at the corner");
    cli_check(red_of(raster.pixels[(0u * 40u) + 9u]) < 0x80u, "the turn at the work's edge beside the notch is cut round");
    cli_check(raster.pixels[(8u * 40u) + 20u] == 0xff000000u, "the notch's middle is the ground whole");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 8u, .height = 4u), "the small raster is made again");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = &raster, .x0 = 0, .y0 = 3, .x1 = 7, .y1 = 3, .color = ink);
    cli_check((raster.pixels[(3u * 8u) + 0u] == 0xff102030u) && (raster.pixels[(3u * 8u) + 7u] == 0xff102030u),
              "a line colors both its ends");
    cli_check(raster.pixels[(2u * 8u) + 3u] == 0xff000000u, "a line colors nothing off its path");
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = &raster, .x0 = 0, .y0 = 0, .x1 = 3, .y1 = 3, .color = ink);
    cli_check(raster.pixels[(2u * 8u) + 2u] == 0xff102030u, "a diagonal colors the pixels it crosses");

    const unsigned char stencil_cover[32] = {0u, 255u, 128u, 0u};
    const GuiMask stencil = {8u, 4u, 0, 0, (unsigned char *)stencil_cover};
    EMBED_CALL(gui_raster_xy_color.stencil, GuiRasterXYColorCfg, .raster = &raster, .mask = &stencil, .color = GUI_RGB(0x10u, 0x20u, 0x30u));
    cli_check((raster.pixels[0] == 0x00102030u) && (raster.pixels[1u] == 0xff102030u) && (raster.pixels[2u] == 0x80102030u),
              "a stencil lays the color with the mask's cover as each pixel's alpha");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = &raster, .x_f32 = 4.0f, .y_f32 = 2.0f, .radius_f32 = 1.5f, .color = white);
    cli_check(red_of(raster.pixels[(2u * 8u) + 4u]) >= 254u, "a dot's middle takes the whole color, within the dither");
    cli_check((red_of(raster.pixels[(1u * 8u) + 2u]) > 80u) && (red_of(raster.pixels[(1u * 8u) + 2u]) < 140u),
              "a pixel on the dot's edge takes its share of the color");
    cli_check((raster.pixels[0] == 0xff000000u) && (raster.pixels[(2u * 8u) + 7u] == 0xff000000u), "a pixel a pixel out of the dot keeps its ground");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = &raster, .x_f32 = 4.0f, .y_f32 = 2.0f, .radius_f32 = 1.5f, .color = GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u));
    cli_check((red_of(raster.pixels[(2u * 8u) + 4u]) > 120u) && (red_of(raster.pixels[(2u * 8u) + 4u]) < 136u),
              "a dot's ink is its color's alpha");
    EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = &raster, .x_f32 = -3.0f, .y_f32 = 20.0f, .radius_f32 = 1.0f, .color = white);
    cli_check(raster.pixels[0] == 0xff000000u, "a dot off the raster draws nothing");

    // lanes laid: a pixel whose rest is 0 takes the ink whole, one whose rest is 255 saves its ground, one between takes
    // its share; nothing is laid outside the clip
    const unsigned int lanes_red_blue[4] = {(0x10u << 16u) * 255u + 0x30u * 255u + 0x00800080u, 0x00800080u, (0x10u << 16u) * 128u + 0x30u * 128u + 0x00800080u, 0x00800080u};
    const unsigned int lanes_green[4] = {0x20u * 255u + 0x80u, 0x80u, 0x20u * 128u + 0x80u, 0x80u};
    const unsigned int lanes_rest[4] = {0u, 255u, 127u, 255u};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_width_x.lanes_layout, GuiRasterWidthXCfg, .raster = &raster, .red_blue = lanes_red_blue, .green = lanes_green, .rest = lanes_rest, .width = 2u, .height = 2u, .x = 1, .y = 1, .clip = whole);
    cli_check(raster.pixels[(1u * 8u) + 1u] == 0xff102030u, "a lane of no rest lays the ink whole");
    cli_check(raster.pixels[(1u * 8u) + 2u] == 0xff000000u, "a lane of all rest keeps the ground");
    cli_check(red_of(raster.pixels[(2u * 8u) + 1u]) == 0x08u, "a lane of half rest takes half the ink");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_width_x.lanes_layout, GuiRasterWidthXCfg, .raster = &raster, .red_blue = lanes_red_blue, .green = lanes_green, .rest = lanes_rest, .width = 2u, .height = 2u, .x = 1, .y = 1, .clip = (GuiRect){2, 0, 6, 4});
    cli_check((raster.pixels[(1u * 8u) + 1u] == 0xff000000u) && (raster.pixels[(1u * 8u) + 2u] == 0xff000000u), "lanes lay nothing outside the clip");

    // a ground with a grain: the tint over the ground's color, and white over that by the grain's tile
    cli_check(EMBED_CALL(cli_raster.color_over, GuiRasterCfg, .ground_color = black, .color = GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u)) == 0xff808080u, "a half-clear white over black is mid gray");
    const unsigned char grained[4] = {0u, 255u, 0u, 0u};
    const GuiUnder table = {NULL, GUI_RGBA(0xffu, 0x00u, 0x00u, 0x80u), grained, 2u, black};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = black);
    EMBED_CALL(gui_raster_rect_radius_ground.corners_cut, GuiRasterRectRadiusGroundCfg, .raster = &raster, .rect = whole, .radius = 3u, .ground = table);
    cli_check((red_of(raster.pixels[0]) > 0x70u) && (red_of(raster.pixels[0]) < 0x90u) && (((raster.pixels[0] >> 8u) & 0xffu) < 0x10u),
              "a corner cut to a grained table lays the tint over the table's color where the grain is 0");
    cli_check((raster.pixels[1u] != 0xff000000u) && (red_of(raster.pixels[1u]) == ((raster.pixels[1u] >> 8u) & 0xffu)),
              "and the grain's white where its tile gives it");

    // a pane on the table: its tint inside the arcs, the table's outside, the ink thickening between
    const GuiUnder plain_table = {NULL, GUI_RGBA(0x00u, 0x00u, 0xffu, 0x40u), NULL, 0u, black};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = white);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = &raster, .ground = plain_table, .rect = whole, .radius = 2u, .inner = GUI_RGBA(0x00u, 0x00u, 0xffu, 0xc0u), .corners_only = 0);
    cli_check((raster.pixels[(1u * 8u) + 3u] & 0xffu) == 0xc0u, "a pane's middle is the ink at the pane's alpha over the table's color");
    cli_check((raster.pixels[0] & 0xffu) < 0x90u, "a pane's corner pixel lies between the table's tint and the pane's");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = white);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = &raster, .ground = plain_table, .rect = whole, .radius = 2u, .inner = GUI_RGBA(0x00u, 0x00u, 0xffu, 0xc0u), .corners_only = 1);
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a pane's corners alone leave its middle");
    cli_check((raster.pixels[0] & 0xffu) < 0x90u, "and lay its corner pixel");

    // the fillets of a notch: the squares of its corners inside the work
    GuiRect fillets[4];
    cli_check(EMBED_CALL(gui_raster_radius.notch_fillets, GuiRasterRadiusCfg, .work = (GuiRect){0, 0, 40, 30}, .notch = (GuiRect){10, 0, 20, 12}, .radius = 4u, .squares = fillets) == 2u, "a notch at the top has two fillets");
    cli_check((fillets[0].x == 10) && (fillets[0].y == 8) && (fillets[0].width == 4) && (fillets[1].x == 26) && (fillets[1].y == 8),
              "each the radius's square inside the hole at its corner");
    cli_check(EMBED_CALL(gui_raster_radius.notch_fillets, GuiRasterRadiusCfg, .work = (GuiRect){0, 0, 40, 30}, .notch = (GuiRect){10, 10, 20, 12}, .radius = 4u, .squares = fillets) == 4u, "a hole inside the work has four");
    cli_check(EMBED_CALL(gui_raster_radius.notch_fillets, GuiRasterRadiusCfg, .work = (GuiRect){0, 0, 40, 30}, .notch = (GuiRect){50, 0, 20, 12}, .radius = 4u, .squares = fillets) == 0u, "a notch off the work has none");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);

    // the work's outline: its lines along each edge, bent round its corners to the table, and round a hole
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 40u, .height = 30u), "a raster of forty by thirty is made for the outline");
    const GuiRect work = {2, 2, 36, 26};
    const GuiRect inside = {3, 4, 33, 23};
    const GuiColor top = GUI_RGB(0x10u, 0xe0u, 0xa0u);
    const GuiColor side = GUI_RGB(0x40u, 0x60u, 0xa0u);
    const GuiOutline lines = {top, side, side, GUI_RGBA(0x10u, 0xe0u, 0xa0u, 0x80u), 2, 2, 1, 1};
    const GuiUnder dark = {NULL, 0u, NULL, 0u, black};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 40, 30}, .color = white);
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = &raster, .work = work, .inside = inside, .radius = 4u, .lines = &lines, .holes = NULL, .count = 0u, .table = plain, .clear = dark);
    cli_check((raster.pixels[(2u * 40u) + 20u] == top) && (raster.pixels[(3u * 40u) + 20u] == top), "the head's line is two pixels of the signal");
    cli_check(raster.pixels[(4u * 40u) + 20u] == 0xffffffffu, "the work's inside stands under the head's line");
    cli_check((raster.pixels[(27u * 40u) + 20u] == side) && (raster.pixels[(26u * 40u) + 20u] == 0xffffffffu), "the foot's line is one pixel of the darkened link");
    cli_check((raster.pixels[(15u * 40u) + 36u] == side) && (raster.pixels[(15u * 40u) + 37u] == side), "the right's line is two pixels of it");
    cli_check((raster.pixels[(15u * 40u) + 2u] != 0xffffffffu) && (raster.pixels[(15u * 40u) + 2u] != top), "the left's line is the signal at a share over the ground");
    cli_check(raster.pixels[(2u * 40u) + 2u] == 0xff000000u, "the work's corner pixel is the table");
    cli_check(raster.pixels[(15u * 40u) + 20u] == 0xffffffffu, "the work's middle stands");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 40, 30}, .color = white);
    const GuiRect notch = {12, 2, 16, 10};
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = &raster, .work = work, .inside = inside, .radius = 4u, .lines = &lines, .holes = &notch, .count = 1u, .table = plain, .clear = dark);
    cli_check(raster.pixels[(5u * 40u) + 20u] == 0xffffffffu, "a hole's middle is left to the base");
    cli_check(red_of(raster.pixels[(11u * 40u) + 12u]) < 0x20u, "inside a fillet's arc the hole is the table");
    cli_check(raster.pixels[(2u * 40u) + 7u] == top, "the head's line runs up to the hole's turn");
    cli_check((raster.pixels[(7u * 40u) + 11u] == top) && (raster.pixels[(7u * 40u) + 10u] == top), "the hole's side inside the work carries the head's line down");
    cli_check((raster.pixels[(12u * 40u) + 20u] == top) && (raster.pixels[(13u * 40u) + 20u] == top), "and its foot carries it across");
    cli_check(raster.pixels[(20u * 40u) + 20u] == 0xffffffffu, "the work below the hole stands");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    cli_check(raster.pixels == NULL, "a raster given back holds no pixels");
    rows_checked();
    pictures_checked();
    // the arcs read from their tables are the distance from each pixel's middle to the arc, at every reach a frame uses
    int arcs_right = 1;
    for (unsigned int reach = 0u; reach <= 36u; reach += 1u)
    {
        for (unsigned int down = 0u; down <= reach + 1u; down += 1u)
        {
            for (unsigned int across = 0u; across <= reach + 1u; across += 1u)
            {
                const float inside = (float)reach + 0.5f - hypotf((float)across + 0.5f, (float)down + 0.5f);
                const float saved = (inside < 0.0f) ? 0.0f : ((inside > 1.0f) ? 1.0f : inside);
                const unsigned int wanted = (unsigned int)((255.0f * saved) + 0.5f);
                arcs_right = arcs_right && (EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = reach, .across = across, .down = down) == wanted);
            }
        }
    }
    cli_check(arcs_right, "an arc's table holds each pixel's share inside it, at every reach");
    cli_check((EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = 7u, .across = 0u, .down = 0u) == 255u) && (EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = 7u, .across = 7u, .down = 7u) == 0u), "a pixel by the center is whole inside and one past the arc is out");
    cli_check(EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = 90u, .across = 0u, .down = 0u) == 255u, "a reach past the tables is worked out");
    return cli_on_check();
}
