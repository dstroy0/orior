// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer_test.c: the framebuffer's check: the rasters are made clear on their boundaries with capacity for the largest
// monitor and no more, resized where they are, refused past their capacity, flipped between the two while the ground
// stays, and counted to their owner until they are given back
#include "../../../../../includes/window/block.h"
#include "../cli/cli_check.h"

// the largest monitor the check names, and the rows past it that clear the capacity's rounding to its boundary
#define MONITOR_WIDTH 3440u
#define MONITOR_HEIGHT 1440u
#define ROWS_PAST 5u

// the bytes an owner named `gui_framebuffer` counts now
static unsigned long long owner_bytes(void)
{
    return (unsigned long long)mmgr_owner_in_use(g_gui_framebuffer_owner);
}

// the bytes a window `width` by `height` draws in one raster
static unsigned long long window_bytes(unsigned int width, unsigned int height)
{
    return (unsigned long long)EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width) * height * sizeof(GuiColor);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    GuiFramebuffer framebuffer;
    cli_check(EMBED_CALL(gui_block_framebuffer.init, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = 1920u, .height = 1080u),
              "the rasters are made for a window with capacity for the largest monitor");
    const unsigned long long capacity = framebuffer.capacity;
    const unsigned long long monitor = window_bytes(MONITOR_WIDTH, MONITOR_HEIGHT);
    cli_check((capacity >= monitor) && (capacity < (monitor + GUI_BLOCK_POSITION)),
              "the capacity is the monitor's, to the next place boundary and no more");
    cli_check((capacity % GUI_BLOCK_POSITION) == 0ull, "and lies on a place boundary");
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        const GuiRaster *const raster = &framebuffer.raster[at];
        cli_check((raster->width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 1920u)) && (raster->height == 1080u), "each is the window's size");
        cli_check((raster->positioned % GUI_BLOCK_POSITION) == 0ull, "each starts on a place boundary of its pages");
        cli_check((raster->pixels[0] == 0u) &&
                      (((const unsigned char *)raster->pixels)[window_bytes(1920u, 1080u) - 1ull] == 0u),
                  "and all of the window's bytes are clear");
    }
    cli_check(framebuffer.raster[1].positioned == capacity, "the second framebuffer is the second segment");
    cli_check(framebuffer.raster[0].pages_bytes == (2ull * capacity), "the two lie in pages of twice the capacity");
    cli_check((framebuffer.raster[GUI_FRAMEBUFFER_BACKGROUND].pages == framebuffer.background_pages) &&
                  (framebuffer.raster[GUI_FRAMEBUFFER_BACKGROUND].pages_bytes == capacity),
              "the ground lies in pages of its own, of the capacity");
    cli_check(owner_bytes() == 0ull, "the pages are the block's and count to no owner");

    GuiRaster *const first = &framebuffer.raster[0];
    GuiRaster *const second = &framebuffer.raster[1];
    GuiRaster *drawn = EMBED_CALL(gui_framebuffer_rings.flip, GuiFramebufferRingsCfg, .framebuffer = &framebuffer);
    cli_check(drawn == second, "the first flip draws the second framebuffer");
    for (unsigned int round = 0u; round < 8u; round += 1u)
    {
        GuiRaster *const next = EMBED_CALL(gui_framebuffer_rings.flip, GuiFramebufferRingsCfg, .framebuffer = &framebuffer);
        cli_check(next != drawn, "each flip draws the framebuffer that is not being displayed");
        cli_check((next == first) || (next == second), "and it is one of the two, never the ground");
        drawn = next;
    }

    GuiColor *const pixels = framebuffer.raster[0].pixels;
    cli_check(EMBED_CALL(gui_block_framebuffer.fit, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = 1280u, .height = 720u), "a smaller window fits");
    cli_check((framebuffer.raster[0].pixels == pixels) && (framebuffer.raster[0].height == 720u),
              "and is sized where the raster is");
    cli_check(EMBED_CALL(gui_block_framebuffer.fit, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT), "a window the monitor's size fits");
    cli_check(framebuffer.raster[0].pixels == pixels, "where the raster is");
    cli_check(!EMBED_CALL(gui_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT + ROWS_PAST),
              "a window past the capacity does not");
    cli_check(!EMBED_CALL(gui_block_framebuffer.fit, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT + ROWS_PAST), "and is refused");
    cli_check(framebuffer.raster[0].height == MONITOR_HEIGHT, "with nothing changed");

    EMBED_CALL(gui_framebuffer_rings.free, GuiFramebufferRingsCfg, .framebuffer = &framebuffer);
    cli_check((framebuffer.pages == NULL) && (framebuffer.background_pages == NULL), "the pages are given back");
    cli_check(owner_bytes() == 0ull, "and come off the owner");
    cli_check(!EMBED_CALL(gui_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &framebuffer, .width = 8u, .height = 8u), "with no pages nothing fits");

    cli_check(!EMBED_CALL(gui_block_framebuffer.init, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = MONITOR_WIDTH, .height = MONITOR_HEIGHT + ROWS_PAST), "a window larger than the monitor is refused");
    cli_check(framebuffer.capacity < (window_bytes(MONITOR_WIDTH, MONITOR_HEIGHT) + GUI_BLOCK_POSITION), "the capacity is the monitor's raster and no further");

    cli_check(EMBED_CALL(gui_block_framebuffer.init, GuiBlockFramebufferCfg, .framebuffer = &framebuffer, .width = 800u, .height = 600u),
              "with no monitor known the window's size is the capacity");
    cli_check(EMBED_CALL(gui_framebuffer_rings.flip, GuiFramebufferRingsCfg, .framebuffer = &framebuffer) == &framebuffer.raster[1], "the rings starting anew");
    EMBED_CALL(gui_framebuffer_rings.free, GuiFramebufferRingsCfg, .framebuffer = &framebuffer);
    cli_check(owner_bytes() == 0ull, "and come off the owner again");

    GuiFramebuffer none;
    cli_check(!EMBED_CALL(gui_block_framebuffer.init, GuiBlockFramebufferCfg, .framebuffer = &none, .width = 0u, .height = 0u), "no size at all makes nothing");
    cli_check((none.pages == NULL) && (owner_bytes() == 0ull), "and nothing is retained or counted");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
