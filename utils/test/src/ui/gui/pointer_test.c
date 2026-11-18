// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer_test.c: the pointer's check: every sprite's figures whole, the scale a sprite is baked at, the rectangle at
// the hot point, the glass drawn over the ground with the ground seen through its body, its shadow below it, the
// pixels under it put back, a move drawn again, the two rasters kept apart, a clip that stops the writes outside it,
// and the sheet baked once and baked again only for a new scale or color
#include "../../../../../includes/window/palette.h"
#include "../../../../../includes/window/pointer.h"
#include "../cli/cli_check.h"

#define POINTER_SIDE 96u
#define POINTER_GROUND GUI_RGB(0x20u, 0x40u, 0x60u)
#define POINTER_OTHER_GROUND GUI_RGB(0xe0u, 0xd0u, 0x30u)

static GuiColor s_first[POINTER_SIDE * POINTER_SIDE];
static GuiColor s_second[POINTER_SIDE * POINTER_SIDE];
static GuiPointer s_pointer;
static GuiPalette s_changed;

// every pixel of `raster` the color `color`
static void ground_fill(GuiRaster *raster, GuiColor color)
{
    for (unsigned int at = 0u; at < (raster->width * raster->height); at += 1u)
    {
        raster->pixels[at] = color;
    }
}

// the pixel at x, y of `raster`, which the check keeps inside it
static GuiColor pixel(const GuiRaster *raster, int x, int y)
{
    // the places the check reads lie on the raster
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

// how many pixels of `raster` are not the ground
static unsigned int drawn_count(const GuiRaster *raster)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < (raster->width * raster->height); at += 1u)
    {
        count += (raster->pixels[at] != POINTER_GROUND) ? 1u : 0u;
    }
    return count;
}

// the byte at `shift` of `color`
static unsigned int channel(GuiColor color, unsigned int shift)
{
    return (color >> shift) & 0xffu;
}

// whether each sprite lies inside the largest side with its hot point inside it, and has one to the most figures,
// each a disc of one point or three to the most points, inside the largest side, with a corner above 0
static int sprites_whole(void)
{
    int whole = 1;
    for (unsigned int shape = 0u; shape < GUI_POINTER_SHAPES; shape += 1u)
    {
        const GuiPointerSprite *const sprite =
            EMBED_CALL(gui_pointer_shape.sprite, GuiPointerShapeCfg, .shape = shape);
        whole &= (sprite->figures >= 1u) && (sprite->figures <= GUI_POINTER_FIGURES) && (sprite->hot_x >= 0.0) &&
                 (sprite->hot_y >= 0.0) && (sprite->hot_x < sprite->width) && (sprite->hot_y < sprite->height) &&
                 (sprite->width <= (double)GUI_POINTER_SIDE_MAX) && (sprite->height <= (double)GUI_POINTER_SIDE_MAX);
        for (unsigned int at = 0u; at < sprite->figures; at += 1u)
        {
            const GuiPointerFigure *const figure = &sprite->figure[at];
            whole &= ((figure->points == 1u) || (figure->points >= 3u)) && (figure->points <= GUI_POINTER_POINTS) &&
                     (figure->corner > 0.0);
            for (unsigned int point = 0u; point < figure->points; point += 1u)
            {
                whole &= (figure->point[point][0] >= 0.0) && (figure->point[point][1] >= 0.0) &&
                         (figure->point[point][0] <= (double)GUI_POINTER_SIDE_MAX) &&
                         (figure->point[point][1] <= (double)GUI_POINTER_SIDE_MAX);
            }
        }
    }
    return whole;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiRaster first = {POINTER_SIDE, POINTER_SIDE, s_first, NULL, 0ULL, 0ULL, {0, 0, 0, 0}};
    GuiRaster second = {POINTER_SIDE, POINTER_SIDE, s_second, NULL, 0ULL, 0ULL, {0, 0, 0, 0}};
    ground_fill(&first, POINTER_GROUND);
    ground_fill(&second, POINTER_GROUND);

    cli_check(sprites_whole(), "every sprite's figures are whole and lie inside the largest side");
    cli_check((EMBED_CALL(gui_pointer_read.scale, GuiPointerReadCfg, .scale = 100) == 100) &&
                  (EMBED_CALL(gui_pointer_read.scale, GuiPointerReadCfg, .scale = 50) == GUI_POINTER_SCALE_LEAST) &&
                  (EMBED_CALL(gui_pointer_read.scale, GuiPointerReadCfg, .scale = 150) == 150) &&
                  (EMBED_CALL(gui_pointer_read.scale, GuiPointerReadCfg, .scale = 500) == GUI_POINTER_SCALE_MOST),
              "a sprite is baked at the window's scale, clamped to the least and the most");

    EMBED_CALL(gui_pointer.init, GuiPointerCfg, .pointer = &s_pointer);
    const GuiRect off = EMBED_CALL(gui_pointer_read.rect, GuiPointerReadCfg, .pointer_read = &s_pointer, .scale = 100);
    cli_check(!s_pointer.shown && (off.width == 0) &&
                  EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                             .rect = EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer,
                                                .raster = &first, .palette = palette, .scale = 100)) &&
                  (drawn_count(&first) == 0u),
              "a pointer made is off the window and draws nothing");
    cli_check(EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 10, .y = 10,
                         .shape = GUI_POINTER_ARROW, .shown = 1) &&
                  !EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 10,
                              .y = 10, .shape = GUI_POINTER_ARROW, .shown = 1),
              "a pointer put somewhere new changes, and put there again does not");
    const GuiRect arrow = EMBED_CALL(gui_pointer_read.rect, GuiPointerReadCfg, .pointer_read = &s_pointer, .scale = 100);
    cli_check((arrow.x == 5) && (arrow.y == 5) && (arrow.width == 26) && (arrow.height == 28),
              "the arrow's rectangle puts its tip on the hot point and holds its margin");
    const GuiRect drawn = EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer,
                                     .raster = &first, .palette = palette, .scale = 100);
    const GuiColor tip = pixel(&first, 10, 10);
    const GuiColor body = pixel(&first, 14, 15);
    const GuiColor shadow = pixel(&first, 16, 30);
    cli_check((drawn.x == arrow.x) && (drawn.width == arrow.width) && (tip != POINTER_GROUND) &&
                  (body != POINTER_GROUND) && (channel(body, 0u) > channel(POINTER_GROUND, 0u)),
              "the arrow's tip is drawn and its body is tinted toward the glass's violet");
    cli_check((shadow != POINTER_GROUND) && (channel(shadow, 16u) <= channel(POINTER_GROUND, 16u)) &&
                  (channel(shadow, 8u) <= channel(POINTER_GROUND, 8u)) &&
                  (channel(shadow, 0u) <= channel(POINTER_GROUND, 0u)) && (pixel(&first, 40, 40) == POINTER_GROUND),
              "the shadow below the arrow's foot darkens the ground, and past the sprite is the ground");
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check(drawn_count(&first) == 0u, "the pixels under the sprite are put back");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                         .rect = EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer,
                                            .raster = &first)),
              "with nothing saved nothing is put back");
    ground_fill(&second, POINTER_OTHER_GROUND);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &second,
                     .palette = palette, .scale = 100);
    cli_check(pixel(&second, 14, 15) != body, "the ground shows through the glass's body: on another ground it differs");
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &second);
    ground_fill(&second, POINTER_GROUND);

    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 60, .y = 50,
                     .shape = GUI_POINTER_TEXT, .shown = 1);
    const GuiRect moved = EMBED_CALL(gui_pointer_raster.redrawn, GuiPointerRasterCfg, .pointer = &s_pointer,
                                     .raster = &first, .palette = palette, .scale = 100);
    cli_check((pixel(&first, 10, 10) == POINTER_GROUND) && (pixel(&first, 60, 50) != POINTER_GROUND) &&
                  (moved.x <= 5) && ((moved.y + moved.height) >= 51),
              "a move puts back the old place, draws the beam at its middle, and gives the rectangle of both");

    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &second,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check((drawn_count(&first) == 0u) && (pixel(&second, 60, 50) != POINTER_GROUND),
              "each raster keeps its own pixels saved, and putting back one leaves the other drawn");
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &second);

    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 40, .y = 40,
                     .shape = GUI_POINTER_MOVE, .shown = 1);
    first.clip = (GuiRect){40, 40, 20, 20};
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    first.clip = (GuiRect){0, 0, 0, 0};
    cli_check((pixel(&first, 33, 40) == POINTER_GROUND) && (pixel(&first, 44, 40) != POINTER_GROUND),
              "a clip stops the writes outside it");
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);

    for (unsigned int shape = 0u; shape < GUI_POINTER_SHAPES; shape += 1u)
    {
        (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 30, .y = 30,
                         .shape = shape, .shown = 1);
        (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                         .palette = palette, .scale = 200);
        const unsigned int count = drawn_count(&first);
        (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
        if ((count == 0u) || (drawn_count(&first) != 0u) || (pixel(&first, 30, 30) != POINTER_GROUND))
        {
            cli_check(0, "a shape draws at twice the scale and its pixels are put back");
        }
    }
    cli_check(1, "every shape draws at twice the scale and its pixels are put back");
    // the compass at rest and with its north lit: the north arrow shows only when lit, the orb both times
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 40, .y = 40,
                     .shape = GUI_POINTER_MOVE, .shown = 1);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    const GuiColor rest_arrow = pixel(&first, 40, 32);
    const GuiColor rest_orb = pixel(&first, 40, 40);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 40, .y = 40,
                     .shape = GUI_POINTER_GLIDE_N, .shown = 1);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    const GuiColor lit_arrow = pixel(&first, 40, 32);
    const GuiColor lit_orb = pixel(&first, 40, 40);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check((rest_arrow == POINTER_GROUND) && (lit_arrow != POINTER_GROUND) && (rest_orb != POINTER_GROUND) &&
                  (lit_orb == rest_orb),
              "the compass at rest draws its orb alone, and gliding north draws the orb and the north arrow alone");
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 30, .y = 30,
                     .shape = GUI_POINTER_SHAPES + 4u, .shown = 0);
    cli_check((s_pointer.shape == GUI_POINTER_ARROW) && !s_pointer.shown,
              "a shape not known is the arrow, and a pointer put off the window is off it");

    // the arrow's hot pixel in the sheet marked: a draw that bakes again writes the glass there, a draw that does not
    // shows the mark
    const GuiColor mark = GUI_RGB(0x12u, 0x34u, 0x56u);
    const unsigned int hot = (5u * 26u) + 5u;
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 10, .y = 10,
                     .shape = GUI_POINTER_ARROW, .shown = 1);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    s_pointer.sheet.pixels[GUI_POINTER_ARROW][hot] = mark;
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    const GuiColor kept = pixel(&first, 10, 10);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check(kept == mark, "a draw at the same scale in the same colors bakes nothing, the sheet's picture put down");
    s_changed = *palette;
    s_changed.pointer_line = GUI_RGB(0xc0u, 0x10u, 0x20u);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = &s_changed, .scale = 100);
    const GuiColor baked = pixel(&first, 10, 10);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check((baked != mark) && (channel(baked, 16u) > channel(baked, 0u)),
              "a pointer color changed bakes the sheet again in the new color");
    s_pointer.sheet.pixels[GUI_POINTER_ARROW][hot] = mark;
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = &s_changed, .scale = 200);
    const GuiColor scaled = pixel(&first, 10, 10);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check((scaled != mark) && (s_pointer.sheet.scale == 200), "a scale changed bakes the sheet again at the new scale");
    return cli_on_check();
}
