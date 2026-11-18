// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer_test.c: the pointer's check: every sprite's tables whole, the cell's pixels at each scale, the rectangle at
// the hot point, the line, fill and shadow drawn in the palette's colors, the pixels under it put back, a move drawn
// again, the two rasters kept apart, a clip that stops the writes outside it, and the sheet baked once and baked again
// only for a new scale or color
#include "../../../../../includes/window/palette.h"
#include "../../../../../includes/window/pointer.h"
#include "../cli/cli_check.h"

#define POINTER_SIDE 96u
#define POINTER_GROUND GUI_RGB(0x20u, 0x40u, 0x60u)

static GuiColor s_first[POINTER_SIDE * POINTER_SIDE];
static GuiColor s_second[POINTER_SIDE * POINTER_SIDE];
static GuiPointer s_pointer;
static GuiPalette s_changed;

// every pixel of `raster` the ground
static void ground_fill(GuiRaster *raster)
{
    for (unsigned int at = 0u; at < (raster->width * raster->height); at += 1u)
    {
        raster->pixels[at] = POINTER_GROUND;
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

// whether each layer of each sprite is a table of whole rows of ' ', '#' and 'o', the end of its text after each row,
// that lies inside the sprite as it is turned, and the hot point inside the sprite
static int sprites_whole(void)
{
    int whole = 1;
    for (unsigned int shape = 0u; shape < GUI_POINTER_SHAPES; shape += 1u)
    {
        const GuiPointerSprite *const sprite =
            EMBED_CALL(gui_pointer_shape.sprite, GuiPointerShapeCfg, .shape = shape);
        whole &= (sprite->layers >= 1u) && (sprite->layers <= GUI_POINTER_LAYERS) &&
                 (sprite->hot_x < sprite->width) && (sprite->hot_y < sprite->height) &&
                 ((sprite->width + 1u) <= GUI_POINTER_SIDE_MAX) && ((sprite->height + 1u) <= GUI_POINTER_SIDE_MAX);
        for (unsigned int at = 0u; at < sprite->layers; at += 1u)
        {
            const GuiPointerLayer *const layer = &sprite->layer[at];
            const unsigned int across = layer->turned ? layer->height : layer->width;
            const unsigned int down = layer->turned ? layer->width : layer->height;
            whole &= ((layer->x + across) <= sprite->width) && ((layer->y + down) <= sprite->height);
            for (unsigned int row = 0u; row < layer->height; row += 1u)
            {
                const char *const text = layer->cells + (row * (layer->width + 1u));
                whole &= text[layer->width] == '\0';
                for (unsigned int column = 0u; column < layer->width; column += 1u)
                {
                    whole &= (text[column] == ' ') || (text[column] == '#') || (text[column] == 'o');
                }
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
    ground_fill(&first);
    ground_fill(&second);

    cli_check(sprites_whole(), "every sprite's tables are whole rows of no cell, line and fill, inside the sprite");
    cli_check((EMBED_CALL(gui_pointer_read.cell, GuiPointerReadCfg, .scale = 100) == 1u) &&
                  (EMBED_CALL(gui_pointer_read.cell, GuiPointerReadCfg, .scale = 50) == 1u) &&
                  (EMBED_CALL(gui_pointer_read.cell, GuiPointerReadCfg, .scale = 150) == 2u) &&
                  (EMBED_CALL(gui_pointer_read.cell, GuiPointerReadCfg, .scale = 300) == 3u) &&
                  (EMBED_CALL(gui_pointer_read.cell, GuiPointerReadCfg, .scale = 500) == GUI_POINTER_SCALE_MAX),
              "a cell is the scale rounded to whole pixels, one to the most");

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
    cli_check((arrow.x == 10) && (arrow.y == 10) && (arrow.width == 12) && (arrow.height == 20),
              "the arrow's rectangle starts at its tip and holds its shadow");
    const GuiRect drawn = EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer,
                                     .raster = &first, .palette = palette, .scale = 100);
    cli_check((drawn.x == arrow.x) && (drawn.width == arrow.width) && (pixel(&first, 10, 10) == palette->pointer_line) &&
                  (pixel(&first, 11, 12) == palette->pointer_fill),
              "the arrow's tip is its line and its inside its fill");
    const GuiColor shadow = pixel(&first, 18, 29);
    cli_check((shadow != POINTER_GROUND) && (shadow != palette->pointer_line) && (pixel(&first, 30, 30) == POINTER_GROUND),
              "the shadow under the arrow's foot is blended over the ground, and past the sprite is the ground");
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check(drawn_count(&first) == 0u, "the pixels under the sprite are put back");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                         .rect = EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer,
                                            .raster = &first)),
              "with nothing saved nothing is put back");

    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 60, .y = 50,
                     .shape = GUI_POINTER_TEXT, .shown = 1);
    const GuiRect moved = EMBED_CALL(gui_pointer_raster.redrawn, GuiPointerRasterCfg, .pointer = &s_pointer,
                                     .raster = &first, .palette = palette, .scale = 100);
    cli_check((pixel(&first, 10, 10) == POINTER_GROUND) && (pixel(&first, 60, 50) == palette->pointer_fill) &&
                  (moved.x == 10) && (moved.y == 10) && ((moved.x + moved.width) >= 61) &&
                  ((moved.y + moved.height) >= 51),
              "a move puts back the old place, draws the beam at its middle, and gives the rectangle of both");

    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &second,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check((drawn_count(&first) == 0u) && (pixel(&second, 60, 50) == palette->pointer_fill),
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
        if ((count == 0u) || (drawn_count(&first) != 0u))
        {
            cli_check(0, "a shape draws at twice the scale and its pixels are put back");
        }
    }
    cli_check(1, "every shape draws at twice the scale and its pixels are put back");
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 30, .y = 30,
                     .shape = GUI_POINTER_SHAPES + 4u, .shown = 0);
    cli_check((s_pointer.shape == GUI_POINTER_ARROW) && !s_pointer.shown,
              "a shape not known is the arrow, and a pointer put off the window is off it");

    // the arrow's tip in the sheet marked: a draw that bakes again writes the line there, a draw that does not shows
    // the mark
    const GuiColor mark = GUI_RGB(0x12u, 0x34u, 0x56u);
    (void)EMBED_CALL(gui_pointer_place_table.place, GuiPointerPlaceCfg, .pointer = &s_pointer, .x = 10, .y = 10,
                     .shape = GUI_POINTER_ARROW, .shown = 1);
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = palette, .scale = 100);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    s_pointer.sheet.pixels[GUI_POINTER_ARROW][0] = mark;
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
    cli_check(baked == s_changed.pointer_line, "a pointer color changed bakes the sheet again in the new color");
    s_pointer.sheet.pixels[GUI_POINTER_ARROW][0] = mark;
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first,
                     .palette = &s_changed, .scale = 200);
    const GuiColor scaled = pixel(&first, 10, 10);
    (void)EMBED_CALL(gui_pointer_raster.restore, GuiPointerRasterCfg, .pointer = &s_pointer, .raster = &first);
    cli_check(scaled == s_changed.pointer_line, "a scale changed bakes the sheet again at the new cell size");
    return cli_on_check();
}
