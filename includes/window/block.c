// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// block.c: the window's block: the system's pages reserved once at the largest monitor's size, each part of them a
// ring of MMgr's positioned over its span, and a part's pages committed as far as a raster first drawn in it reaches
#include "block.h"

#include "memoria_operor/memoria_operor.h"

#include "../raster/raster.h"

// each part's ring, its record settled by the linker over a pool of its own
#define GUI_BLOCK_RING(name_, segments_)                                                                               \
    ParsMemoriaeHospes(gui_block_##name_##_pool);                                                                      \
    MemoriaAnularis(s_ring_##name_, gui_block_##name_##_pool);
GUI_BLOCK_PARTS(GUI_BLOCK_RING)
#define GUI_BLOCK_RING_AT(name_, segments_) &s_ring_##name_,
static mmgr_ring *const s_rings[GUI_BLOCK_COUNT] = {GUI_BLOCK_PARTS(GUI_BLOCK_RING_AT)};
#define GUI_BLOCK_SEGMENTS_AT(name_, segments_) segments_,
static const unsigned int s_segments[GUI_BLOCK_COUNT] = {GUI_BLOCK_PARTS(GUI_BLOCK_SEGMENTS_AT)};

// The block: its pages and their bytes, a segment's bytes, each part's place from the pages' start, and how far each
// segment's pages are committed from the segment's start
typedef struct
{
    unsigned char *pages;
    unsigned long long bytes;
    unsigned long long segment;
    unsigned long long at[GUI_BLOCK_COUNT];
    unsigned long long committed[GUI_BLOCK_COUNT][GUI_BLOCK_SEGMENTS_MAX];
    const GuiBlockPages *system;
} GuiBlock;
static GuiBlock s_block;

// `bytes` rounded up to the next boundary
EMBED_INLINE unsigned long long block_positioned(unsigned long long bytes)
{
    return (bytes + GUI_BLOCK_POSITION - 1ULL) & ~(GUI_BLOCK_POSITION - 1ULL);
}

int gui_block_init(const GuiBlockSizeCfg *args)
{
    if (s_block.pages != NULL)
    {
        return 1;
    }
    const unsigned long long pitch = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = args->width);
    const unsigned long long segment = block_positioned(pitch * args->height * sizeof(GuiColor));
    if ((args->height == 0u) || (segment == 0ULL))
    {
        return 0;
    }
    unsigned long long bytes = 0ULL;
    for (unsigned int part = 0u; part < GUI_BLOCK_COUNT; part += 1u)
    {
        s_block.at[part] = bytes;
        bytes += segment * s_segments[part];
    }
    s_block.system = args->pages;
    s_block.pages = (args->pages != NULL) ? args->pages->reserve(bytes) : NULL;
    if (s_block.pages == NULL)
    {
        return 0;
    }
    s_block.bytes = bytes;
    s_block.segment = segment;
    memor.set(&(MemoriaCfg){.dst = s_block.committed, .val = 0, .bytes = sizeof(s_block.committed)});
    return 1;
}

void gui_block_free(const GuiBlockSizeCfg *args)
{
    (void)args;
    if (s_block.pages != NULL)
    {
        s_block.system->free(s_block.pages, s_block.bytes);
    }
    memor.set(&(MemoriaCfg){.dst = &s_block, .val = 0, .bytes = sizeof(s_block)});
}

unsigned long long gui_block_raster_bytes(const GuiBlockSizeCfg *args)
{
    (void)args;
    return s_block.segment;
}

// The range of a part: the part, where the range starts from the part's start, and its bytes
typedef struct
{
    unsigned int part;
    unsigned long long offset;
    unsigned long long bytes;
} BlockRange;
// the pages of part `part` from `offset` for `bytes` committed, each segment the range reaches committed from its start
// as far as the range reaches in it, to the next boundary: 1, or 0 where the block has no pages, the part is none, or
// the range passes the part
EMBED_INLINE int block_range_committed(const BlockRange *args)
{
    if ((s_block.pages == NULL) || (args->part >= GUI_BLOCK_COUNT))
    {
        return 0;
    }
    const unsigned long long capacity = s_block.segment * s_segments[args->part];
    if ((args->offset > capacity) || (args->bytes > capacity - args->offset))
    {
        return 0;
    }
    const unsigned long long end = args->offset + args->bytes;
    for (unsigned long long segment = args->offset / s_block.segment; (segment * s_block.segment) < end; segment += 1ULL)
    {
        const unsigned long long segment_start = segment * s_block.segment;
        const unsigned long long inside = ((end - segment_start) < s_block.segment) ? (end - segment_start) : s_block.segment;
        const unsigned long long reached = block_positioned(inside);
        unsigned long long *const committed = &s_block.committed[args->part][segment];
        if (reached <= *committed)
        {
            continue;
        }
        if (!s_block.system->commit(s_block.pages + s_block.at[args->part] + segment_start + *committed,
                                    reached - *committed))
        {
            return 0;
        }
        *committed = reached;
    }
    return 1;
}

// the pages of segment `segment` of part `part` committed from the segment's start as far as `bytes` reach: 1, or 0
// where the block has no pages, the part or segment is none, or `bytes` pass the segment
EMBED_INLINE int block_committed(const GuiBlockPartCfg *args)
{
    if ((args->part >= GUI_BLOCK_COUNT) || (args->segment >= s_segments[args->part]) || (args->bytes > s_block.segment))
    {
        return 0;
    }
    return EMBED_CALL(block_range_committed, BlockRange, .part = args->part,
                      .offset = (unsigned long long)args->segment * s_block.segment, .bytes = args->bytes);
}

unsigned char *gui_block_take(const GuiBlockPartCfg *args)
{
    if (!EMBED_CALL(block_range_committed, BlockRange, .part = args->part, .offset = *args->cursor, .bytes = args->bytes))
    {
        return NULL;
    }
    unsigned char *const span = s_block.pages + s_block.at[args->part] + *args->cursor;
    *args->cursor += (args->bytes + 7ULL) & ~7ULL;
    return span;
}

mmgr_ring *gui_block_ring(const GuiBlockPartCfg *args)
{
    if ((args->part >= GUI_BLOCK_COUNT) || (s_block.pages == NULL))
    {
        return NULL;
    }
    for (unsigned int segment = 0u; segment < s_segments[args->part]; segment += 1u)
    {
        if (!EMBED_CALL(block_committed, GuiBlockPartCfg, .part = args->part, .segment = segment, .bytes = args->bytes))
        {
            return NULL;
        }
    }
    // the segments' sizes are the block's, each a whole number of boundaries, so no size needs to be a power of two
    return anularis.init(&(AnularisCfg){.ring = s_rings[args->part],
                                        .buf = s_block.pages + s_block.at[args->part],
                                        .capacity = (embed_word)(s_block.segment * s_segments[args->part]),
                                        .segment_count = s_segments[args->part],
                                        .segment_bytes = (embed_word)s_block.segment})
               ? s_rings[args->part]
               : NULL;
}

mmgr_ring *gui_block_ring_read(const GuiBlockPartCfg *args)
{
    return ((args->part < GUI_BLOCK_COUNT) && (s_block.pages != NULL)) ? s_rings[args->part] : NULL;
}

unsigned char *gui_block_span(const GuiBlockPartCfg *args)
{
    if (!block_committed(args))
    {
        return NULL;
    }
    return s_block.pages + s_block.at[args->part] + ((unsigned long long)args->segment * s_block.segment);
}

int gui_block_raster_init(const GuiBlockRasterCfg *args)
{
    const unsigned long long bytes = (unsigned long long)args->width * args->height * sizeof(GuiColor);
    unsigned char *const span =
        EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = args->part, .segment = args->segment, .bytes = bytes);
    return EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = args->raster, .width = args->width,
                      .height = args->height, .pixels = (GuiColor *)(void *)span);
}

// the three segments the framebuffers lie in committed as far as a window `width` by `height` draws: 1, or 0 where
// the block has no pages or the window passes a segment
EMBED_INLINE int block_framebuffer_committed(const GuiBlockFramebufferCfg *args)
{
    const unsigned long long window =
        EMBED_CALL(gui_framebuffer.bytes, GuiFramebufferCfg, .width = args->width, .height = args->height);
    return (EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_FRAMES, .segment = 0u,
                       .bytes = window) != NULL) &&
           (EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_FRAMES, .segment = 1u,
                       .bytes = window) != NULL) &&
           (EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_BACKGROUND, .segment = 0u,
                       .bytes = window) != NULL);
}

int gui_block_framebuffer_init(const GuiBlockFramebufferCfg *args)
{
    const unsigned long long window =
        EMBED_CALL(gui_framebuffer.bytes, GuiFramebufferCfg, .width = args->width, .height = args->height);
    const unsigned long long capacity = EMBED_CALL(gui_block_size.raster_bytes, GuiBlockSizeCfg);
    if ((window == 0ULL) || (window > capacity))
    {
        memor.set(&(MemoriaCfg){.dst = args->framebuffer, .val = 0, .bytes = sizeof(*args->framebuffer)});
        return 0;
    }
    return EMBED_CALL(gui_framebuffer_rings.init, GuiFramebufferRingsCfg, .framebuffer = args->framebuffer,
                      .frames = EMBED_CALL(gui_block_part.ring, GuiBlockPartCfg, .part = GUI_BLOCK_FRAMES,
                                           .bytes = window),
                      .background = EMBED_CALL(gui_block_part.ring, GuiBlockPartCfg, .part = GUI_BLOCK_BACKGROUND,
                                               .bytes = window),
                      .capacity = capacity, .width = args->width, .height = args->height);
}

int gui_block_framebuffer_fit(const GuiBlockFramebufferCfg *args)
{
    if (!EMBED_CALL(gui_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = args->framebuffer,
                    .width = args->width, .height = args->height) ||
        !block_framebuffer_committed(args))
    {
        return 0;
    }
    return EMBED_CALL(gui_framebuffer_rings.fit, GuiFramebufferRingsCfg, .framebuffer = args->framebuffer,
                      .width = args->width, .height = args->height);
}
