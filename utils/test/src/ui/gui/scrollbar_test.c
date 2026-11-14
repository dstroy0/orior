// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scrollbar_test.c: the scrollbar's check, down a side and across a foot: nothing stands where the whole is in view;
// the thumb's length is the share in view and its place the share before it, at the start for the first line and at
// the end for the last; a press lands on an arrow, the thumb or the track before or after it; a thumb dragged to a
// place puts the line there in view; the thumb is drawn, round at its ends, and the arrows with it; and a layout at
// the same scale and direction keeps the masks it made
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/scrollbar.h"
#include "../cli/cli_check.h"

// the color the raster is filled with before the scrollbar is drawn, which no part of the palette is
#define SCROLLBAR_TEST_UNDER 0xff123456u

// whether any pixel of `rect` differs from the ground the check laid
static int painted(const GuiRaster *raster, GuiRect rect)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != SCROLLBAR_TEST_UNDER)
            {
                return 1;
            }
        }
    }
    return 0;
}

// the scrollbar `bar` laid out at `rect`, `across` or down, checked through, `label` naming its direction
static void scrollbar_checked(GuiRaster *raster, const GuiPalette *palette, GuiScrollbar *bar, GuiRect rect, int across)
{
    EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = bar, .rect = rect, .across = across, .scale = 100);
    const int track_start = across ? rect.x + GUI_SCROLLBAR_ARROW : rect.y + GUI_SCROLLBAR_ARROW;
    const int track_length = (across ? rect.width : rect.height) - (2 * GUI_SCROLLBAR_ARROW);
    // a place on the scrollbar's own way, x across and y down, the other at its middle
    const int middle_x = rect.x + (rect.width / 2);
    const int middle_y = rect.y + (rect.height / 2);
    unsigned long long first = 0ull;

    // the whole in view: no scrollbar
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = (GuiRect){0, 0, (int)raster->width, (int)raster->height},
               .color = SCROLLBAR_TEST_UNDER);
    cli_check(!EMBED_CALL(gui_scrollbar.needed, GuiScrollbarCfg, .total = 20ull, .shown = 20ull), "a whole in view needs no scrollbar");
    EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg, .bar_read = bar, .raster = raster, .palette = palette, .total = 20ull, .shown = 30ull);
    cli_check(!painted(raster, rect), "and none is drawn");
    cli_check(EMBED_CALL(gui_scrollbar.hit, GuiScrollbarCfg, .bar_read = bar, .total = 20ull, .shown = 20ull, .x = middle_x, .y = middle_y) == GUI_SCROLLBAR_NONE,
              "and a press there lands on none");

    // a quarter in view: the thumb a quarter of the track, at its start for the first line and its end for the last
    const GuiRect start = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull, .first = 0ull);
    cli_check(((across ? start.x : start.y) == track_start) && ((across ? start.width : start.height) == track_length / 4),
              "a quarter in view makes a quarter of the track, at its start");
    const GuiRect end = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull, .first = 300ull);
    cli_check((across ? end.x + end.width : end.y + end.height) == track_start + track_length, "the last lines put it at the track's end");
    const GuiRect least = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = bar, .total = 1000000ull, .shown = 10ull);
    cli_check((across ? least.width : least.height) == GUI_SCROLLBAR_THUMB_MIN, "a thumb is never shorter than its least");

    // presses, at a place along the scrollbar's own way
    const GuiRect thumb = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull, .first = 150ull);
    const int thumb_start = across ? thumb.x : thumb.y;
    const int thumb_length = across ? thumb.width : thumb.height;
    const int places[5] = {(across ? rect.x : rect.y) + 2, (across ? rect.x + rect.width : rect.y + rect.height) - 2, thumb_start + 2, thumb_start - 2,
                           thumb_start + thumb_length + 2};
    const unsigned int lands[5] = {GUI_SCROLLBAR_UP, GUI_SCROLLBAR_DOWN, GUI_SCROLLBAR_THUMB, GUI_SCROLLBAR_ABOVE, GUI_SCROLLBAR_BELOW};
    unsigned int landed = 0u;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        landed += (EMBED_CALL(gui_scrollbar.hit, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull, .first = 150ull,
                              .x = across ? places[at] : middle_x, .y = across ? middle_y : places[at]) == lands[at])
                      ? 1u
                      : 0u;
    }
    cli_check(landed == 5u, "a press lands on the arrow before, the arrow after, the thumb, and the track before and after it");

    // a thumb dragged
    EMBED_CALL(gui_scrollbar.first_at, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull,
               .x = across ? track_start : middle_x, .y = across ? middle_y : track_start, .first_out = &first);
    cli_check(first == 0ull, "a thumb dragged to the track's start shows the first line");
    EMBED_CALL(gui_scrollbar.first_at, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull,
               .x = across ? track_start + track_length : middle_x, .y = across ? middle_y : track_start + track_length, .first_out = &first);
    cli_check(first == 300ull, "and past its end the last lines");
    EMBED_CALL(gui_scrollbar.first_at, GuiScrollbarCfg, .bar_read = bar, .total = 400ull, .shown = 100ull,
               .x = across ? thumb_start : middle_x, .y = across ? middle_y : thumb_start, .first_out = &first);
    // a pixel of the thumb's way stands for the lines that can stand first over the room the thumb moves in
    const unsigned long long pixel_lines = (300ull + (unsigned long long)(track_length - thumb_length) - 1ull) /
                                           (unsigned long long)(track_length - thumb_length);
    cli_check((first + pixel_lines >= 150ull) && (first <= 150ull + pixel_lines), "and where it stands, the line it stands for within a pixel's lines");

    // drawn
    EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg, .bar_read = bar, .raster = raster, .palette = palette, .total = 400ull, .shown = 100ull, .first = 150ull);
    cli_check(painted(raster, across ? (GuiRect){thumb.x + (thumb.width / 2), thumb.y, 1, thumb.height} : (GuiRect){thumb.x, thumb.y + (thumb.height / 2), thumb.width, 1}),
              "the thumb is drawn");
    cli_check(!painted(raster, (GuiRect){thumb.x, thumb.y, 1, 1}), "round at its ends, its corner clear");
    const GuiRect before = across ? (GuiRect){rect.x, rect.y, GUI_SCROLLBAR_ARROW, rect.height} : (GuiRect){rect.x, rect.y, rect.width, GUI_SCROLLBAR_ARROW};
    const GuiRect after = across ? (GuiRect){rect.x + rect.width - GUI_SCROLLBAR_ARROW, rect.y, GUI_SCROLLBAR_ARROW, rect.height}
                                 : (GuiRect){rect.x, rect.y + rect.height - GUI_SCROLLBAR_ARROW, rect.width, GUI_SCROLLBAR_ARROW};
    cli_check(painted(raster, before) && painted(raster, after), "and both arrows with it");
    const GuiRect clear = across ? (GuiRect){track_start, rect.y, thumb_start - track_start - 1, rect.height}
                                 : (GuiRect){rect.x, track_start, rect.width, thumb_start - track_start - 1};
    cli_check(!painted(raster, clear), "the track itself is left clear");

    // laid out again at the same scale and direction, the masks it made are kept
    const unsigned char *const made = bar->arrow_before.cover;
    bar->arrow_cover[0][0] = 77u;
    EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = bar, .rect = rect, .across = across, .scale = 100);
    cli_check((bar->arrow_before.cover == made) && (bar->arrow_cover[0][0] == 77u), "a layout at the same scale and direction makes no mask again");
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 240u, .height = 240u),
              "a raster to draw on");
    static GuiScrollbar down;
    static GuiScrollbar across;
    scrollbar_checked(&raster, palette, &down, (GuiRect){10, 10, GUI_SCROLLBAR_WIDTH, 200}, 0);
    scrollbar_checked(&raster, palette, &across, (GuiRect){10, 220, 200, GUI_SCROLLBAR_WIDTH}, 1);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
