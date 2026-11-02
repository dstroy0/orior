// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_raster.c: the compression restores each pixel, and a listing draws ink where its text is and writes both raster formats
#include "ui/cli/src/print/print.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the most bytes of a page drawn here, and of a raster written
#define RASTER_PAGE (1240ull * 1754ull * 3ull)
#define RASTER_OUT (16ull << 20u)

// What the drawing produced: the first page, copied, and how many pages there were
typedef struct
{
    CliPixels page;
    unsigned int count;
} RasterDrawn;

static unsigned char s_page[RASTER_PAGE];
static unsigned char s_back[RASTER_PAGE];
static char s_out[RASTER_OUT];
static RasterDrawn s_drawn;
static CliPrintLayout s_layout;
static const CliPrintListing *s_listing;

// the pixels decompressed into `back`: each line's repeat, then its runs; their count
static unsigned long long decompressed(const unsigned char *data, unsigned long long size, unsigned long long width, unsigned long long height, unsigned long long channels, unsigned char *back)
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

// a drawn page recorded, the first only
static void page_recorded(const CliEachPageIndexCountCfg *args)
{
    const CliPixels *page = args->page;
    unsigned int count = args->count;
    RasterDrawn *const drawn = args->context;
    drawn->count = count;
    if ((args->index == 0u) && (page->width * page->height * page->channels <= RASTER_PAGE))
    {
        drawn->page = *page;
        drawn->page.data = s_page;
        memor.cpy(&(MemoriaCfg){.dst = s_page, .src = page->data, .bytes = page->width * page->height * page->channels});
    }
}

// a small page of mostly white with one patterned line, compressed: the compressed size
static unsigned long long small_page_compressed(void)
{
    CliPixels page = {300ull, 7ull, 3ull, s_page};
    memor.set(&(MemoriaCfg){.dst = s_page, .val = 255, .bytes = 300u * 7u * 3u});
    for (unsigned int across = 0u; across < 300u; across += 1u)
    {
        // the pattern's value is below 7
        s_page[(3u * 300u + across) * 3u] = (unsigned char)(across % 7u);
    }
    s_page[5u * 900u + 10u] = 0u;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_print_layout.compress, CliPrintLayoutCfg, .page = &page, .out = &out);
    return out.at;
}

// a big-endian 32-bit number at `at` of `bytes`
static unsigned long long big_endian_32(const char *bytes, unsigned long long at)
{
    const unsigned char *const of = (const unsigned char *)bytes + at;
    return ((unsigned long long)of[0] << 24u) | ((unsigned long long)of[1] << 16u) | ((unsigned long long)of[2] << 8u) | (unsigned long long)of[3];
}

// a listing of one line and two runs, the second bold and italic, drawn on A4 at 150 dpi, once for the process, the print job open
// until the process ends; the test ignored where the system has no monospace face
static void listing_drawn(void)
{
    static const char s_text[] = "{\"title\":\"t\",\"lines\":[{\"number\":1,\"runs\":[{\"text\":\"Hello, orior\","
                                 "\"color\":\"#000000\"},{\"text\":\" bold\",\"color\":\"#c4314b\",\"bold\":true,"
                                 "\"italic\":true}]}],\"tab\":4}";
    static int s_drawn_once = 0;
    static int s_has_face = 0;
    if (s_drawn_once)
    {
        if (!s_has_face)
        {
            TEST_IGNORE_MESSAGE("the system has no monospace face");
        }
        return;
    }
    s_drawn_once = 1;
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_print_alloc();
    if (!EMBED_CALL(cli_print_json.faces, CliPrintJsonCfg, .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("the system has no monospace face");
    }
    s_has_face = 1;
    memor.set(&(MemoriaCfg){.dst = &s_layout, .val = 0, .bytes = sizeof(s_layout)});
    memor.cpy(&(MemoriaCfg){.dst = s_layout.paper, .src = "a4", .bytes = 3u});
    s_layout.copies = 1u;
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_text, sizeof(s_text) - 1u, &read);
    CliPrintListing *listing = NULL;
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_print_json.listing_read, CliPrintJsonCfg, .json = json, .node = read, .listing_pointer = &listing, .err_msg = &err_msg));
    s_listing = listing;
    EMBED_CALL(cli_print_layout_listing.render, CliPrintLayoutListingCfg, .listing = s_listing, .layout = &s_layout, .dpi_f32 = 150.0f, .each = page_recorded, .context = &s_drawn);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_compression_is_smaller_than_the_page(void)
{
    TEST_ASSERT_LESS_THAN_UINT64(300ull * 7ull * 3ull, small_page_compressed());
}

void test_the_compression_restores_each_pixel(void)
{
    const unsigned long long size = small_page_compressed();
    TEST_ASSERT_EQUAL_UINT64(300ull * 7ull * 3ull, decompressed((const unsigned char *)s_out, size, 300ull, 7ull, 3ull, s_back));
    TEST_ASSERT_EQUAL_MEMORY(s_page, s_back, 300u * 7u * 3u);
}

void test_one_line_draws_one_page(void)
{
    listing_drawn();
    TEST_ASSERT_EQUAL_UINT(1u, s_drawn.count);
}

void test_a4_at_150_dpi_is_1240_by_1754(void)
{
    listing_drawn();
    TEST_ASSERT_EQUAL_UINT64(1240ull, s_drawn.page.width);
    TEST_ASSERT_EQUAL_UINT64(1754ull, s_drawn.page.height);
}

void test_the_text_draws_ink(void)
{
    listing_drawn();
    unsigned long long dark = 0ull;
    for (unsigned long long at = 0ull; at + 2ull < s_drawn.page.width * s_drawn.page.height * 3ull; at += 3ull)
    {
        dark += (s_page[at] < 128u) ? 1ull : 0ull;
    }
    TEST_ASSERT_GREATER_THAN_UINT64(300ull, dark);
}

void test_the_red_run_draws_red(void)
{
    listing_drawn();
    unsigned long long red = 0ull;
    for (unsigned long long at = 0ull; at + 2ull < s_drawn.page.width * s_drawn.page.height * 3ull; at += 3ull)
    {
        red += ((s_page[at] > 150u) && (s_page[at + 1ull] < 120u)) ? 1ull : 0ull;
    }
    TEST_ASSERT_GREATER_THAN_UINT64(20ull, red);
}

void test_the_text_is_within_the_margins_adjacent_the_top(void)
{
    listing_drawn();
    int top = 0;
    // 40 and 60 points at 150 dots an inch are whole rows of the page
    const unsigned long long first = (unsigned long long)(40.0 * 150.0 / 72.0);
    const unsigned long long last = (unsigned long long)(60.0 * 150.0 / 72.0);
    for (unsigned long long row = first; !top && (row < last); row += 1ull)
    {
        for (unsigned long long across = 0ull; !top && (across < s_drawn.page.width * 3ull); across += 1ull)
        {
            top = s_page[row * s_drawn.page.width * 3ull + across] < 100u;
        }
    }
    TEST_ASSERT_TRUE(top);
}

void test_the_pwg_rasters_header_names_its_kind_and_the_pages_width(void)
{
    listing_drawn();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_print_layout_listing_out.pwg_begin, CliPrintLayoutListingOutCfg, .out = &out);
    EMBED_CALL(cli_print_layout.pwg_page, CliPrintLayoutCfg, .out = &out, .page = &s_drawn.page, .layout = &s_layout, .dpi = 150u, .count = 1u);
    TEST_ASSERT_GREATER_THAN_UINT64(4ull + 1796ull, out.at);
    TEST_ASSERT_EQUAL_MEMORY("RaS2", s_out, 4u);
    TEST_ASSERT_EQUAL_MEMORY("PwgRaster", s_out + 4, 9u);
    TEST_ASSERT_EQUAL_UINT64(1240ull, big_endian_32(s_out, 4u + 372u));
}

void test_the_pwg_raster_contains_the_page(void)
{
    listing_drawn();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_print_layout_listing_out.pwg_begin, CliPrintLayoutListingOutCfg, .out = &out);
    EMBED_CALL(cli_print_layout.pwg_page, CliPrintLayoutCfg, .out = &out, .page = &s_drawn.page, .layout = &s_layout, .dpi = 150u, .count = 1u);
    TEST_ASSERT_EQUAL_UINT64(RASTER_PAGE, decompressed((const unsigned char *)s_out + 4u + 1796u, out.at - 4u - 1796u, 1240ull, 1754ull, 3ull, s_back));
    TEST_ASSERT_EQUAL_MEMORY(s_page, s_back, RASTER_PAGE);
}

void test_apples_raster_names_its_kind(void)
{
    listing_drawn();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_print_layout.urf_begin, CliPrintLayoutCfg, .out = &out, .count = 1u);
    EMBED_CALL(cli_print_layout.urf_page, CliPrintLayoutCfg, .out = &out, .page = &s_drawn.page, .dpi = 150u);
    TEST_ASSERT_GREATER_THAN_UINT64(44ull, out.at);
    TEST_ASSERT_EQUAL_MEMORY("UNIRAST\0", s_out, 8u);
}

void test_apples_raster_contains_the_page(void)
{
    listing_drawn();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_print_layout.urf_begin, CliPrintLayoutCfg, .out = &out, .count = 1u);
    EMBED_CALL(cli_print_layout.urf_page, CliPrintLayoutCfg, .out = &out, .page = &s_drawn.page, .dpi = 150u);
    TEST_ASSERT_EQUAL_UINT64(RASTER_PAGE, decompressed((const unsigned char *)s_out + 12u + 32u, out.at - 44u, 1240ull, 1754ull, 3ull, s_back));
    TEST_ASSERT_EQUAL_MEMORY(s_page, s_back, RASTER_PAGE);
}

void test_a_landscape_page_is_wider_than_tall(void)
{
    listing_drawn();
    s_layout.landscape = 1;
    s_layout.gray = 1;
    EMBED_CALL(cli_print_layout_listing.render, CliPrintLayoutListingCfg, .listing = s_listing, .layout = &s_layout, .dpi_f32 = 100.0f, .each = page_recorded, .context = &s_drawn);
    TEST_ASSERT_GREATER_THAN_UINT64(s_drawn.page.height, s_drawn.page.width);
}

void test_a_turned_landscape_page_is_portrait(void)
{
    listing_drawn();
    s_layout.landscape = 1;
    s_layout.gray = 1;
    EMBED_CALL(cli_print_layout_listing.render, CliPrintLayoutListingCfg, .listing = s_listing, .layout = &s_layout, .dpi_f32 = 100.0f, .each = page_recorded, .context = &s_drawn);
    const CliPixels side = s_drawn.page;
    CliPixels upright = {0ull, 0ull, side.channels, s_back};
    EMBED_CALL(cli_print_page.turn, CliPrintPageCfg, .page = &side, .out_pixels_pointer = &upright);
    TEST_ASSERT_EQUAL_UINT64(side.height, upright.width);
    TEST_ASSERT_EQUAL_UINT64(side.width, upright.height);
}
