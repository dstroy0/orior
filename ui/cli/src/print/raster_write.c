// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_write.c: pages of pixels written as PWG raster, which every IPP Everywhere printer takes, or Apple's raster,
// which AirPrint printers take. Each line of pixels is written with the count of the lines like it that follow, and
// each run of one pixel or of different ones with its length
#include "../../../../includes/arith/arith.h"
#include "print.h"

// the bytes of a PWG page's head
#define CLI_PWG_HEAD 1796u

typedef struct
{
    const unsigned char *row;
    unsigned long long a;
    unsigned long long b;
    unsigned long long pixel;
    unsigned char *head;
    unsigned int at;
    unsigned long long value;
    unsigned long long width;
    VerbaLinea *out;
} RasterWriteCtx;
// whether the pixels at `a` and `b` of a row are the same
EMBED_INLINE int cli_same_pixel(const RasterWriteCtx *args)
{
    return memor.cmp(&(MemoriaCfg){.src = args->row + (args->a * args->pixel),
                                   .other = args->row + (args->b * args->pixel),
                                   .bytes = args->pixel}) == 0;
}

// a row of `width` pixels of `pixel` bytes written as its runs: a run of one pixel as its count less one and the pixel,
// a run of different pixels as 257 less its length and the pixels
EMBED_INLINE void cli_row_compress(const RasterWriteCtx *args)
{
    const unsigned char *const row = args->row;
    const unsigned long long pixel = args->pixel;
    const unsigned long long width = args->width;
    unsigned long long x = 0ULL;
    while (x < width)
    {
        if (((x + 1ULL) < width) &&
            EMBED_CALL(cli_same_pixel, RasterWriteCtx, .row = row, .a = x, .b = x + 1ULL, .pixel = pixel))
        {
            unsigned long long run = 2ULL;
            while (((x + run) < width) && (run < 128ULL) &&
                   EMBED_CALL(cli_same_pixel, RasterWriteCtx, .row = row, .a = x + run, .b = x, .pixel = pixel))
            {
                run += 1ULL;
            }
            mmgr_line_ch(args->out, (char)(unsigned char)(run - 1ULL));
            mmgr_line_put_n(args->out, (const char *)(row + (x * pixel)), pixel);
            x += run;
            continue;
        }
        unsigned long long run = 1ULL;
        while (((x + run) < width) && (run < 128ULL) &&
               !(((x + run + 1ULL) < width) && EMBED_CALL(cli_same_pixel, RasterWriteCtx, .row = row, .a = x + run,
                                                          .b = x + run + 1ULL, .pixel = pixel)))
        {
            run += 1ULL;
        }
        // a run of different pixels written as 257 less its length, a byte
        mmgr_line_ch(args->out, (char)(unsigned char)((run == 1ULL) ? 0ULL : (257ULL - run)));
        mmgr_line_put_n(args->out, (const char *)(row + (x * pixel)), run * pixel);
        x += run;
    }
}

void cli_print_compress(const CliPrintLayoutCfg *args)
{
    const unsigned long long line = args->page->width * args->page->channels;
    const unsigned long long pixel = args->page->channels;
    unsigned long long y = 0ULL;
    while (y < args->page->height)
    {
        const unsigned char *const row = args->page->data + (y * line);
        unsigned long long repeat = 1ULL;
        while (((y + repeat) < args->page->height) && (repeat < 256ULL) &&
               (memor.cmp(
                    &(MemoriaCfg){.src = args->page->data + ((y + repeat) * line), .other = row, .bytes = line}) == 0))
        {
            repeat += 1ULL;
        }
        // a count of at max 256 written less one as a byte
        mmgr_line_ch(args->out, (char)(unsigned char)(repeat - 1ULL));
        EMBED_CALL(cli_row_compress, RasterWriteCtx, .row = row, .pixel = pixel, .width = args->page->width,
                   .out = args->out);
        y += repeat;
    }
}

void cli_print_pwg_begin(const CliPrintLayoutListingOutCfg *args)
{
    mmgr_line_put(args->out, "RaS2");
}

// a number put in a head at `at` as its four bytes, the high first
EMBED_INLINE void cli_append(const RasterWriteCtx *args)
{
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // each byte of the low 32 bits in turn
        args->head[args->at + byte] = (unsigned char)((args->value >> (8u * (3u - byte))) & 0xffu);
    }
}

void cli_print_pwg_page(const CliPrintLayoutCfg *args)
{
    unsigned char head[CLI_PWG_HEAD];
    memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(head)});
    CliPrintLayout upright = *args->layout;
    upright.landscape = 0;
    float width_points = 0.0f;
    float height_points = 0.0f;
    EMBED_CALL(cli_print_layout_width.size, CliPrintLayoutWidthCfg, .layout = &upright, .width = &width_points,
               .height = &height_points);
    memor.cpy(&(MemoriaCfg){.dst = head, .src = "PwgRaster", .bytes = 9u});
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 276u, .value = args->dpi);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 280u, .value = args->dpi);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 340u,
               .value = (args->layout->copies > 1u) ? args->layout->copies : 1u);
    // a paper's size in points, rounded, is whole and small
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 352u,
               .value = (unsigned long long)arith_round_f32(width_points));
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 356u,
               .value = (unsigned long long)arith_round_f32(height_points));
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 372u, .value = args->page->width);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 376u, .value = args->page->height);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 384u, .value = 8u);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 388u, .value = 8u * args->page->channels);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 392u, .value = args->page->width * args->page->channels);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 400u, .value = (args->page->channels == 1u) ? 18u : 19u);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 420u, .value = args->page->channels);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 452u, .value = args->count);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 456u, .value = 1u);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 460u, .value = 1u);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 480u, .value = 0x00ffffffu);
    memor.cpy(&(MemoriaCfg){.dst = head + 1668u, .src = "perceptual", .bytes = 10u});
    const char *const media = cli_print_layout.media(args);
    memor.cpy(&(MemoriaCfg){.dst = head + 1732u,
                            .src = media,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = media, .cap = CLI_POOL_BYTES})});
    mmgr_line_put_n(args->out, (const char *)head, sizeof(head));
    cli_print_layout.compress(args);
}

void cli_print_urf_begin(const CliPrintLayoutCfg *args)
{
    unsigned char head[4];
    mmgr_line_put_n(args->out, "UNIRAST", 8u);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 0u, .value = args->count);
    mmgr_line_put_n(args->out, (const char *)head, 4u);
}

void cli_print_urf_page(const CliPrintLayoutCfg *args)
{
    unsigned char head[32];
    memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(head)});
    // the bits a pixel takes, its color space and its quality are each a byte
    head[0] = (unsigned char)(8u * args->page->channels);
    head[1] = (args->page->channels == 1u) ? 0u : 1u;
    head[2] = 1u;
    head[3] = 4u;
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 12u, .value = args->page->width);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 16u, .value = args->page->height);
    EMBED_CALL(cli_append, RasterWriteCtx, .head = head, .at = 20u, .value = args->dpi);
    mmgr_line_put_n(args->out, (const char *)head, sizeof(head));
    cli_print_layout.compress(args);
}
