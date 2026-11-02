// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer_test.c: the framebuffer's check: the rasters are made clear on their boundaries with room for the largest
// monitor and no more, resized where they stand, refused past their room, flipped between the two while the ground
// stays, and counted to their owner until they are given back
#include "../../../../../ui/gui/src/framebuffer.h"
#include "../cli/cli_check.h"

// the largest monitor the check names, and the rows past it that clear the room's rounding to its boundary
#define MONITOR_WIDTH 3440u
#define MONITOR_HEIGHT 1440u
#define ROWS_PAST 5u

// the bytes an owner named `gui_framebuffer` counts now
static unsigned long long owner_bytes(void)
{
    const size_t listed = mmgr_owner_count();
    for (size_t at = 0u; at < listed; at += 1u)
    {
        const CellOwner *const owner = mmgr_owner_at(at);
        if ((owner != NULL) &&
            cellul.eq(&(CatenaFinitaCfg){.src = owner->name, .other = "gui_framebuffer", .cap = 64u}))
        {
            return (unsigned long long)mmgr_owner_in_use(owner);
        }
    }
    return 0ull;
}

// the bytes a window `width` by `height` draws in one raster
static unsigned long long window_bytes(unsigned int width, unsigned int height)
{
    return (unsigned long long)EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width) * height * sizeof(GuiColor);
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    GuiFramebuffer framebuffer;
    cli_check(EMBED_CALL(gui_framebuffer_most_width.init, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = 1920u, .height = 1080u, .most_width = MONITOR_WIDTH, .most_height = MONITOR_HEIGHT),
              "the rasters are made for a window with room for the largest monitor");
    const unsigned long long room = framebuffer.room;
    const unsigned long long monitor = window_bytes(MONITOR_WIDTH, MONITOR_HEIGHT);
    cli_check((room >= monitor) && (room < (monitor + GUI_FRAMEBUFFER_PLACE)),
              "the room is the monitor's, to the next place boundary and no more");
    cli_check((room % GUI_FRAMEBUFFER_PLACE) == 0ull, "and lies on a place boundary");
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        const GuiRaster *const raster = &framebuffer.raster[at];
        cli_check((raster->width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 1920u)) && (raster->height == 1080u), "each is the window's size");
        cli_check((raster->placed % GUI_FRAMEBUFFER_PLACE) == 0ull, "each starts on a place boundary of its pages");
        cli_check((raster->pixels[0] == 0u) && (((const unsigned char *)raster->pixels)[room - 1ull] == 0u),
                  "and all of its room is clear");
    }
    cli_check(framebuffer.raster[1].placed == room, "the second framebuffer is the second segment");
    cli_check(framebuffer.raster[0].pages_bytes == (2ull * room), "the two lie in pages of twice the room");
    cli_check((framebuffer.raster[GUI_FRAMEBUFFER_GROUND].pages == framebuffer.ground_pages) &&
                  (framebuffer.raster[GUI_FRAMEBUFFER_GROUND].pages_bytes == room),
              "the ground lies in pages of its own, of the room");
    cli_check(owner_bytes() == (3ull * room), "the pages count to the owner in orior's total");

    GuiRaster *const first = &framebuffer.raster[0];
    GuiRaster *const second = &framebuffer.raster[1];
    GuiRaster *drawn = EMBED_CALL(gui_framebuffer_most_width.flip, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer);
    cli_check(drawn == second, "the first flip draws the second framebuffer");
    for (unsigned int round = 0u; round < 8u; round += 1u)
    {
        GuiRaster *const next = EMBED_CALL(gui_framebuffer_most_width.flip, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer);
        cli_check(next != drawn, "each flip draws the framebuffer that is not being shown");
        cli_check((next == first) || (next == second), "and it is one of the two, never the ground");
        drawn = next;
    }

    GuiColor *const pixels = framebuffer.raster[0].pixels;
    cli_check(EMBED_CALL(gui_framebuffer_most_width.fit, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = 1280u, .height = 720u), "a smaller window fits");
    cli_check((framebuffer.raster[0].pixels == pixels) && (framebuffer.raster[0].height == 720u),
              "and is sized where the raster stands");
    cli_check(EMBED_CALL(gui_framebuffer_most_width.fit, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT), "a window the monitor's size fits");
    cli_check(framebuffer.raster[0].pixels == pixels, "where the raster stands");
    cli_check(!EMBED_CALL(cli_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT + ROWS_PAST),
              "a window past the room does not");
    cli_check(!EMBED_CALL(gui_framebuffer_most_width.fit, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT + ROWS_PAST), "and is refused");
    cli_check(framebuffer.raster[0].height == MONITOR_HEIGHT, "with nothing changed");

    EMBED_CALL(gui_framebuffer_most_width.free, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer);
    cli_check((framebuffer.pages == NULL) && (framebuffer.ground_pages == NULL), "the pages are given back");
    cli_check(owner_bytes() == 0ull, "and come off the owner");
    cli_check(!EMBED_CALL(cli_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &framebuffer, .width = 8u, .height = 8u), "with no pages nothing fits");

    cli_check(EMBED_CALL(gui_framebuffer_most_width.init, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = 1000u, .height = 1000u, .most_width = 800u, .most_height = 600u), "a window larger than the monitor is made");
    cli_check(EMBED_CALL(cli_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &framebuffer, .width = 1000u, .height = 1000u), "the room following the window");
    cli_check(framebuffer.room < (window_bytes(1000u, 1000u) + GUI_FRAMEBUFFER_PLACE), "and no further");
    EMBED_CALL(gui_framebuffer_most_width.free, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer);

    cli_check(EMBED_CALL(gui_framebuffer_most_width.init, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer, .width = 800u, .height = 600u, .most_width = 0u, .most_height = 0u),
              "with no monitor known the window's size is the room");
    cli_check(EMBED_CALL(gui_framebuffer_most_width.flip, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer) == &framebuffer.raster[1], "the rings starting anew");
    EMBED_CALL(gui_framebuffer_most_width.free, GuiFramebufferMostWidthCfg, .framebuffer = &framebuffer);
    cli_check(owner_bytes() == 0ull, "and come off the owner again");

    GuiFramebuffer none;
    cli_check(!EMBED_CALL(gui_framebuffer_most_width.init, GuiFramebufferMostWidthCfg, .framebuffer = &none, .width = 0u, .height = 0u, .most_width = 0u, .most_height = 0u), "no size at all makes nothing");
    cli_check((none.pages == NULL) && (owner_bytes() == 0ull), "and nothing is kept or counted");
    return cli_on_check();
}
