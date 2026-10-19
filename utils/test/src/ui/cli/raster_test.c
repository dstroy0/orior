// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_test.c: the drawing's check: the compression gives back each pixel, and a listing draws ink where its text
// stands and writes both rasters
#include "../../../../../ui/cli/src/print/print.h"
#include "cli_check.h"

// the most bytes of a page drawn here, and of a raster written
#define RASTER_PAGE (1240ull * 1754ull * 3ull)
#define RASTER_OUT (16ull << 20u)

static unsigned char s_page[RASTER_PAGE];
static unsigned char s_back[RASTER_PAGE];
static char s_out[RASTER_OUT];

// the pixels back from the compression into `back`: each line's repeat, then its runs; their count
static unsigned long long raster_decompressed(const unsigned char *data, unsigned long long size,
                                              unsigned long long width, unsigned long long height,
                                              unsigned long long channels, unsigned char *back)
{
    unsigned long long out = 0ull;
    unsigned long long at = 0ull;
    const unsigned long long line_size = width * channels;
    while ((out < width * height * channels) && (at < size))
    {
        const unsigned long long repeat = (unsigned long long)data[at] + 1ull;
        at += 1ull;
        const unsigned long long start = out;
        while ((out - start < line_size) && (at < size))
        {
            const unsigned int control = data[at];
            at += 1ull;
            if (control < 128u)
            {
                for (unsigned int each = 0u; each <= control; each += 1u)
                {
                    memor.cpy(&(MemoriaCfg){.dst = back + out, .src = data + at, .bytes = channels});
                    out += channels;
                }
                at += channels;
            }
            else
            {
                const unsigned long long count = 257ull - control;
                memor.cpy(&(MemoriaCfg){.dst = back + out, .src = data + at, .bytes = count * channels});
                out += count * channels;
                at += count * channels;
            }
        }
        for (unsigned long long again = 1ull; again < repeat; again += 1ull)
        {
            memor.cpy(&(MemoriaCfg){.dst = back + out, .src = back + start, .bytes = line_size});
            out += line_size;
        }
    }
    return out;
}

// What the drawing gave: the first page, copied, and how many pages there were
typedef struct
{
    CliPixels page;
    unsigned int count;
} RasterDrawn;

// a page drawn kept, the first alone
static void raster_kept(void *context, const CliPixels *page, unsigned int index, unsigned int count)
{
    RasterDrawn *const drawn = context;
    drawn->count = count;
    if ((index == 0u) && (page->width * page->height * page->channels <= RASTER_PAGE))
    {
        drawn->page = *page;
        drawn->page.data = s_page;
        memor.cpy(
            &(MemoriaCfg){.dst = s_page, .src = page->data, .bytes = page->width * page->height * page->channels});
    }
}

// the compression gives back each pixel
static void raster_compression(void)
{
    CliPixels page = {300ull, 7ull, 3ull, s_page};
    memor.set(&(MemoriaCfg){.dst = s_page, .val = 255, .bytes = 300u * 7u * 3u});
    for (unsigned int x = 0u; x < 300u; x += 1u)
    {
        s_page[(3u * 300u + x) * 3u] = (unsigned char)(x % 7u);
    }
    s_page[5u * 900u + 10u] = 0u;
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_print_compress(&page, &out);
    cli_check(out.at < 300ull * 7ull * 3ull, "the compression is smaller than the page");
    const unsigned long long back =
        raster_decompressed((const unsigned char *)s_out, out.at, 300ull, 7ull, 3ull, s_back);
    cli_check((back == 300ull * 7ull * 3ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_back, .other = s_page, .bytes = back}) == 0),
              "it gives back each pixel");
}

// the listing of one line, two runs, the second bold and italic, read
static const CliPrintListing *raster_listing(void)
{
    static const char s_text[] = "{\"title\":\"t\",\"lines\":[{\"number\":1,\"runs\":[{\"text\":\"Hello, orior\","
                                 "\"color\":\"#000000\"},{\"text\":\" bold\",\"color\":\"#c4314b\",\"bold\":true,"
                                 "\"italic\":true}]}],\"tab\":4}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_text, sizeof(s_text) - 1u, &read);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliPrintListing *listing = NULL;
    return cli_print_listing_read(json, read, &listing, &said) ? listing : NULL;
}

// a big-endian 32-bit number at `at` of `bytes`
static unsigned long long raster_be32(const char *bytes, unsigned long long at)
{
    const unsigned char *const of = (const unsigned char *)bytes + at;
    return ((unsigned long long)of[0] << 24u) | ((unsigned long long)of[1] << 16u) | ((unsigned long long)of[2] << 8u) |
           (unsigned long long)of[3];
}

// a listing draws ink where its text stands and writes both rasters
static void raster_listing_drawn(void)
{
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_print_faces(&said))
    {
        cli_check(1, "the system has no monospace face, and nothing is drawn");
        return;
    }
    CliPrintLayout layout;
    memor.set(&(MemoriaCfg){.dst = &layout, .val = 0, .bytes = sizeof(layout)});
    memor.cpy(&(MemoriaCfg){.dst = layout.paper,
                            .src = "a4",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "a4", .cap = CLI_CELLS_POOL}) + 1u});
    layout.copies = 1u;
    cli_print_take();
    const CliPrintListing *const listing = raster_listing();
    static RasterDrawn s_drawn;
    cli_print_render(listing, &layout, 150.0f, raster_kept, &s_drawn);
    const CliPixels *const page = &s_drawn.page;
    cli_check(s_drawn.count == 1u, "one line draws one page");
    cli_check((page->width == 1240ull) && (page->height == 1754ull), "A4 at 150 dpi is 1240 by 1754");
    unsigned long long dark = 0ull;
    unsigned long long red = 0ull;
    for (unsigned long long at = 0ull; at + 2ull < page->width * page->height * 3ull; at += 3ull)
    {
        dark += (s_page[at] < 128u) ? 1ull : 0ull;
        red += ((s_page[at] > 150u) && (s_page[at + 1ull] < 120u)) ? 1ull : 0ull;
    }
    cli_check(dark > 300ull, "the text draws ink");
    cli_check(red > 20ull, "the red run draws red");
    int top = 0;
    for (unsigned long long y = (unsigned long long)(40.0 * 150.0 / 72.0);
         !top && (y < (unsigned long long)(60.0 * 150.0 / 72.0)); y += 1ull)
    {
        for (unsigned long long x = 0ull; !top && (x < page->width * 3ull); x += 1ull)
        {
            top = s_page[y * page->width * 3ull + x] < 100u;
        }
    }
    cli_check(top, "the text stands within the margins, near the top");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_print_pwg_begin(&out);
    cli_print_pwg_page(&out, page, &layout, 150u, 1u);
    cli_check((out.at > 4u + 1796u) && (memor.cmp(&(MemoriaCfg){.src = s_out, .other = "RaS2", .bytes = 4u}) == 0) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_out + 4, .other = "PwgRaster", .bytes = 9u}) == 0) &&
                  (raster_be32(s_out, 4u + 372u) == 1240ull),
              "the PWG raster's head names its kind and the page's width");
    unsigned long long back = raster_decompressed((const unsigned char *)s_out + 4u + 1796u, out.at - 4u - 1796u,
                                                  1240ull, 1754ull, 3ull, s_back);
    cli_check((back == RASTER_PAGE) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_back, .other = s_page, .bytes = RASTER_PAGE}) == 0),
              "the PWG raster holds the page");
    out.at = 0ull;
    cli_print_urf_begin(&out, 1u);
    cli_print_urf_page(&out, page, 150u);
    cli_check((out.at > 44u) && (memor.cmp(&(MemoriaCfg){.src = s_out, .other = "UNIRAST\0", .bytes = 8u}) == 0),
              "Apple's raster names its kind");
    back = raster_decompressed((const unsigned char *)s_out + 12u + 32u, out.at - 44u, 1240ull, 1754ull, 3ull, s_back);
    cli_check((back == RASTER_PAGE) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_back, .other = s_page, .bytes = RASTER_PAGE}) == 0),
              "Apple's raster holds the page");
    layout.landscape = 1;
    layout.gray = 1;
    cli_print_render(listing, &layout, 100.0f, raster_kept, &s_drawn);
    const CliPixels side = s_drawn.page;
    cli_check(side.width > side.height, "a page on its side is wider than tall");
    CliPixels upright = {0ull, 0ull, side.channels, s_back};
    cli_print_turned(&side, &upright);
    cli_check((upright.width == side.height) && (upright.height == side.width), "turned, it stands upright");
    cli_print_give();
}

int main(void)
{
    raster_compression();
    raster_listing_drawn();
    return cli_checked();
}
