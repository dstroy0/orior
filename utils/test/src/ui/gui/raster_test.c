// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_test.c: the raster's check: a raster made is clear, a fill colors its rectangle and nothing about it, is
// clipped to the raster and blends by its alpha, a mask blends by its coverage inside the clip, two rectangles meet in
// their overlap, a rounded fill leaves its corners short, a line colors its ends and a dot is soft-edged
#include "../../../../../ui/gui/src/raster.h"
#include "../cli/cli_check.h"

// the red channel of a pixel
static unsigned int red_of(GuiColor color)
{
    return (color >> 16u) & 0xffu;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 8u, 4u), "a raster of eight by four is made");
    cli_check((raster.width == 8u) && (raster.height == 4u), "a raster made keeps its size");
    cli_check((raster.pixels[0] == 0u) && (raster.pixels[31] == 0u), "a raster made is clear");

    const GuiColor ink = GUI_RGB(0x10u, 0x20u, 0x30u);
    gui_raster_filled(&raster, (GuiRect){2, 1, 3, 2}, ink);
    cli_check(raster.pixels[(1u * 8u) + 2u] == 0xff102030u, "a fill colors its top left pixel, opaque");
    cli_check(raster.pixels[(2u * 8u) + 4u] == 0xff102030u, "a fill colors its bottom right pixel");
    cli_check((raster.pixels[(1u * 8u) + 1u] == 0u) && (raster.pixels[(1u * 8u) + 5u] == 0u) &&
                  (raster.pixels[2u] == 0u) && (raster.pixels[(3u * 8u) + 2u] == 0u),
              "a fill leaves the pixels about its rectangle");

    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    gui_raster_filled(&raster, (GuiRect){-2, -2, 100, 100}, black);
    cli_check((raster.pixels[0] == 0xff000000u) && (raster.pixels[31] == 0xff000000u),
              "a fill past the raster's edges is clipped to the raster");
    gui_raster_filled(&raster, (GuiRect){0, 0, 8, 4}, GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u));
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
                gui_raster_filled(&raster, (GuiRect){0, 0, 1, 1}, GUI_RGBA(color_red, color_green, color_blue, share));
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
    gui_raster_filled(&raster, whole, black);
    gui_raster_masked(&raster, &mask, 3, 1, white, whole);
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a pixel a mask covers whole takes the color whole");
    cli_check(raster.pixels[(1u * 8u) + 4u] == 0xff000000u, "a pixel a mask leaves bare keeps its ground");
    cli_check(red_of(raster.pixels[(2u * 8u) + 3u]) == 0x80u, "a pixel a mask covers by half takes half the color");
    gui_raster_filled(&raster, whole, black);
    gui_raster_masked(&raster, &mask, 3, 1, white, (GuiRect){4, 0, 4, 4});
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xff000000u, "a mask draws nothing outside the clip");
    cli_check(raster.pixels[(1u * 8u) + 4u] == 0xff000000u, "a mask's bare pixel inside the clip stays bare");
    gui_raster_masked(&raster, &mask, -1, -1, white, whole);
    cli_check(red_of(raster.pixels[0]) == 0x40u, "a mask partly off the raster draws the pixel of it that is on");

    GuiRect met = gui_rect_met(whole, (GuiRect){5, 2, 10, 10});
    cli_check((met.x == 5) && (met.y == 2) && (met.width == 3) && (met.height == 2),
              "two rectangles meet in their overlap");
    met = gui_rect_met((GuiRect){0, 0, 2, 2}, (GuiRect){5, 5, 1, 1});
    cli_check(gui_rect_empty(met), "rectangles apart meet in none");
    cli_check(gui_rect_holds(whole, 7, 3) && !gui_rect_holds(whole, 8, 3), "a rectangle holds its last pixel, not the one after");

    gui_raster_filled(&raster, whole, black);
    gui_raster_rounded(&raster, whole, 2u, white);
    cli_check(raster.pixels[0] != 0xffffffffu, "a rounded fill leaves its corner pixel short of the color");
    cli_check(raster.pixels[0] != 0xff000000u, "a rounded fill covers its corner pixel in part");
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a rounded fill colors its middle whole");
    cli_check(raster.pixels[3u] == 0xffffffffu, "a rounded fill colors its top edge between the corners whole");

    gui_raster_filled(&raster, whole, white);
    const GuiUnder plain = {NULL, black};
    gui_raster_corners_cut(&raster, whole, 2u, plain);
    cli_check(red_of(raster.pixels[0]) < red_of(raster.pixels[1u]), "a corner cut takes more ground at the corner pixel than beside it");
    cli_check(raster.pixels[(1u * 8u) + 3u] == 0xffffffffu, "a corner cut leaves the middle");
    cli_check((raster.pixels[1u] != 0xffffffffu) && (raster.pixels[1u] != 0xff000000u), "a corner cut blends along the arc");

    // a corner cut to a ground raster takes that raster's pixel, with a color over it where one is given
    GuiRaster under;
    cli_check(gui_raster_made(&under, 8u, 4u), "a ground raster is made");
    gui_raster_filled(&under, whole, GUI_RGB(0x00u, 0x80u, 0x00u));
    gui_raster_filled(&raster, whole, white);
    gui_raster_corners_cut(&raster, whole, 3u, (GuiUnder){&under, 0u});
    cli_check((red_of(raster.pixels[0]) < 0x40u) && (((raster.pixels[0] >> 8u) & 0xffu) > 0x40u), "a corner cut to a ground raster lays its pixel bare");
    gui_raster_filled(&raster, whole, white);
    gui_raster_corners_cut(&raster, whole, 3u, (GuiUnder){&under, GUI_RGBA(0xffu, 0x00u, 0x00u, 0x80u)});
    cli_check((red_of(raster.pixels[0]) > 0x40u) && (((raster.pixels[0] >> 8u) & 0xffu) > 0x20u), "a color over the ground raster is blended into the corner");
    gui_raster_given(&under);

    // the grain: a tile of alphas laid from the raster's top left, repeated, inside the corners
    const unsigned char grain[4] = {0u, 255u, 0u, 0u};
    gui_raster_filled(&raster, whole, black);
    gui_raster_grained(&raster, whole, 0u, grain, 2u);
    cli_check((raster.pixels[1u] == 0xffffffffu) && (raster.pixels[0] == 0xff000000u) && (raster.pixels[3u] == 0xffffffffu) &&
                  (raster.pixels[(1u * 8u) + 1u] == 0xff000000u),
              "the grain lays its tile's alphas over the raster, repeated");
    gui_raster_filled(&raster, whole, black);
    gui_raster_grained(&raster, whole, 3u, grain, 2u);
    cli_check(red_of(raster.pixels[1u]) < 0xffu, "the grain is short at a rounded corner");

    gui_raster_given(&raster);
    cli_check(gui_raster_made(&raster, 40u, 30u), "a raster of forty by thirty is made for a notch");
    const GuiRect big = {0, 0, 40, 30};
    gui_raster_filled(&raster, big, white);
    gui_raster_notched(&raster, big, (GuiRect){10, 0, 20, 12}, 4u, plain);
    cli_check(raster.pixels[(5u * 40u) + 20u] == 0xff000000u, "a notch at the top is cut to the ground");
    cli_check(raster.pixels[(5u * 40u) + 5u] == 0xffffffffu, "the work beside the notch stands");
    cli_check(raster.pixels[(20u * 40u) + 20u] == 0xffffffffu, "the work below the notch stands");
    cli_check(red_of(raster.pixels[(11u * 40u) + 10u]) > 0x80u, "the turn into the notch's corner inside the work is filleted, the work's color left at the corner");
    cli_check(red_of(raster.pixels[(0u * 40u) + 9u]) < 0x80u, "the turn at the work's edge beside the notch is cut round");
    cli_check(raster.pixels[(8u * 40u) + 20u] == 0xff000000u, "the notch's middle is the ground whole");
    gui_raster_given(&raster);
    cli_check(gui_raster_made(&raster, 8u, 4u), "the small raster is made again");

    gui_raster_filled(&raster, whole, black);
    gui_raster_line(&raster, 0, 3, 7, 3, ink);
    cli_check((raster.pixels[(3u * 8u) + 0u] == 0xff102030u) && (raster.pixels[(3u * 8u) + 7u] == 0xff102030u),
              "a line colors both its ends");
    cli_check(raster.pixels[(2u * 8u) + 3u] == 0xff000000u, "a line colors nothing off its path");
    gui_raster_line(&raster, 0, 0, 3, 3, ink);
    cli_check(raster.pixels[(2u * 8u) + 2u] == 0xff102030u, "a diagonal colors the pixels it crosses");

    const unsigned char stencil_cover[32] = {0u, 255u, 128u, 0u};
    const GuiMask stencil = {8u, 4u, 0, 0, (unsigned char *)stencil_cover};
    gui_raster_stenciled(&raster, &stencil, GUI_RGB(0x10u, 0x20u, 0x30u));
    cli_check((raster.pixels[0] == 0x00102030u) && (raster.pixels[1u] == 0xff102030u) && (raster.pixels[2u] == 0x80102030u),
              "a stencil lays the color with the mask's cover as each pixel's alpha");

    gui_raster_filled(&raster, whole, black);
    gui_raster_dotted(&raster, 4.0f, 2.0f, 1.5f, white);
    cli_check(red_of(raster.pixels[(2u * 8u) + 4u]) >= 254u, "a dot's middle takes the whole color, within the dither");
    cli_check((red_of(raster.pixels[(1u * 8u) + 2u]) > 80u) && (red_of(raster.pixels[(1u * 8u) + 2u]) < 140u),
              "a pixel on the dot's edge takes its share of the color");
    cli_check((raster.pixels[0] == 0xff000000u) && (raster.pixels[(2u * 8u) + 7u] == 0xff000000u), "a pixel a pixel out of the dot keeps its ground");
    gui_raster_filled(&raster, whole, black);
    gui_raster_dotted(&raster, 4.0f, 2.0f, 1.5f, GUI_RGBA(0xffu, 0xffu, 0xffu, 0x80u));
    cli_check((red_of(raster.pixels[(2u * 8u) + 4u]) > 120u) && (red_of(raster.pixels[(2u * 8u) + 4u]) < 136u),
              "a dot's ink is its color's alpha");
    gui_raster_dotted(&raster, -3.0f, 20.0f, 1.0f, white);
    cli_check(raster.pixels[0] == 0xff000000u, "a dot off the raster draws nothing");

    gui_raster_given(&raster);
    cli_check(raster.pixels == NULL, "a raster given back holds no pixels");
    return cli_checked();
}
