// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_write.c: pages of pixels written as PWG raster, which every IPP Everywhere printer takes, or Apple's raster,
// which AirPrint printers take. Each line of pixels is written with the count of the lines like it that follow, and
// each run of one pixel or of different ones with its length
#include "print.h"

#include <math.h>

// the bytes of a PWG page's head
#define CLI_PWG_HEAD 1796u

// whether the pixels at `a` and `b` of a row are the same
static int cli_same_pixel(const unsigned char *row, unsigned long long a, unsigned long long b,
                          unsigned long long pixel)
{
    return memor.cmp(&(MemoriaCfg){.src = row + (a * pixel), .other = row + (b * pixel), .bytes = pixel}) == 0;
}

void cli_print_compress(const CliPixels *page, CliLine *out)
{
    const unsigned long long line = page->width * page->channels;
    const unsigned long long pixel = page->channels;
    unsigned long long y = 0ull;
    while (y < page->height)
    {
        const unsigned char *const row = page->data + (y * line);
        unsigned long long repeat = 1ull;
        while (((y + repeat) < page->height) && (repeat < 256ull) &&
               (memor.cmp(&(MemoriaCfg){.src = page->data + ((y + repeat) * line), .other = row, .bytes = line}) == 0))
        {
            repeat += 1ull;
        }
        // a count of at most 256 written less one as a byte
        cli_line_character(out, (char)(unsigned char)(repeat - 1ull));
        unsigned long long x = 0ull;
        while (x < page->width)
        {
            if (((x + 1ull) < page->width) && cli_same_pixel(row, x, x + 1ull, pixel))
            {
                unsigned long long run = 2ull;
                while (((x + run) < page->width) && (run < 128ull) && cli_same_pixel(row, x + run, x, pixel))
                {
                    run += 1ull;
                }
                cli_line_character(out, (char)(unsigned char)(run - 1ull));
                cli_line_counted(out, (const char *)(row + (x * pixel)), pixel);
                x += run;
            }
            else
            {
                unsigned long long run = 1ull;
                while (((x + run) < page->width) && (run < 128ull) &&
                       !(((x + run + 1ull) < page->width) && cli_same_pixel(row, x + run, x + run + 1ull, pixel)))
                {
                    run += 1ull;
                }
                // a run of different pixels written as 257 less its length, a byte
                cli_line_character(out, (char)(unsigned char)((run == 1ull) ? 0ull : (257ull - run)));
                cli_line_counted(out, (const char *)(row + (x * pixel)), run * pixel);
                x += run;
            }
        }
        y += repeat;
    }
}

void cli_print_pwg_begin(CliLine *out)
{
    cli_line_text(out, "RaS2");
}

// a number put in a head at `at` as its four bytes, the high first
static void cli_put(unsigned char *head, unsigned int at, unsigned long long value)
{
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // each byte of the low 32 bits in turn
        head[at + byte] = (unsigned char)((value >> (8u * (3u - byte))) & 0xffu);
    }
}

void cli_print_pwg_page(CliLine *out, const CliPixels *page, const CliPrintLayout *layout, unsigned int dpi,
                        unsigned int count)
{
    unsigned char head[CLI_PWG_HEAD];
    memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(head)});
    CliPrintLayout upright = *layout;
    upright.landscape = 0;
    float width_points = 0.0f;
    float height_points = 0.0f;
    cli_print_size(&upright, &width_points, &height_points);
    memor.cpy(&(MemoriaCfg){.dst = head, .src = "PwgRaster", .bytes = 9u});
    cli_put(head, 276u, dpi);
    cli_put(head, 280u, dpi);
    cli_put(head, 340u, (layout->copies > 1u) ? layout->copies : 1u);
    // a paper's size in points, rounded, is whole and small
    cli_put(head, 352u, (unsigned long long)roundf(width_points));
    cli_put(head, 356u, (unsigned long long)roundf(height_points));
    cli_put(head, 372u, page->width);
    cli_put(head, 376u, page->height);
    cli_put(head, 384u, 8u);
    cli_put(head, 388u, 8u * page->channels);
    cli_put(head, 392u, page->width * page->channels);
    cli_put(head, 400u, (page->channels == 1u) ? 18u : 19u);
    cli_put(head, 420u, page->channels);
    cli_put(head, 452u, count);
    cli_put(head, 456u, 1u);
    cli_put(head, 460u, 1u);
    cli_put(head, 480u, 0x00ffffffu);
    memor.cpy(&(MemoriaCfg){.dst = head + 1668u, .src = "perceptual", .bytes = 10u});
    const char *const media = cli_print_media(layout);
    memor.cpy(&(MemoriaCfg){.dst = head + 1732u,
                            .src = media,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = media, .cap = CLI_CELLS_POOL})});
    cli_line_counted(out, (const char *)head, sizeof(head));
    cli_print_compress(page, out);
}

void cli_print_urf_begin(CliLine *out, unsigned int count)
{
    unsigned char head[4];
    cli_line_counted(out, "UNIRAST", 8u);
    cli_put(head, 0u, count);
    cli_line_counted(out, (const char *)head, 4u);
}

void cli_print_urf_page(CliLine *out, const CliPixels *page, unsigned int dpi)
{
    unsigned char head[32];
    memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(head)});
    // the bits a pixel takes, its color space and its quality are each a byte
    head[0] = (unsigned char)(8u * page->channels);
    head[1] = (page->channels == 1u) ? 0u : 1u;
    head[2] = 1u;
    head[3] = 4u;
    cli_put(head, 12u, page->width);
    cli_put(head, 16u, page->height);
    cli_put(head, 20u, dpi);
    cli_line_counted(out, (const char *)head, sizeof(head));
    cli_print_compress(page, out);
}
