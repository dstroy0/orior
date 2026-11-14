// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// block.h: the window's block: one span of the system's pages, reserved once as the window starts at the size the
// largest monitor takes and given back whole as it ends. Every ring of pixels the window draws is a part of it: a ring
// of MMgr's whose record the linker settles, positioned over its span of the block on a GUI_BLOCK_POSITION boundary, where
// the system's graphics device can place a buffer, its pages committed as far as the part is first drawn. Nothing of
// the window's pixels comes from the shared cells (block.c)
#ifndef GUI_BLOCK_H
#define GUI_BLOCK_H
#include "../frame_buffer/framebuffer.h"
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"

typedef struct GuiBlockSizeCfg GuiBlockSizeCfg;
typedef struct GuiBlockPartCfg GuiBlockPartCfg;
typedef struct GuiBlockRasterCfg GuiBlockRasterCfg;
typedef struct GuiBlockFramebufferCfg GuiBlockFramebufferCfg;

// the boundary each part and each of its segments starts on in the block, the placement a graphics device takes a
// buffer at
#define GUI_BLOCK_POSITION (64ull << 10u)

// The parts, each named with its segments; a segment holds one raster of the largest monitor. The rasters hold one
// each; the glyph cells, the busy indicator and the faces span theirs; the lattice has a segment a slot and the faces one
// a face; the work parts hold the float coverage a glyph, an icon or the wordmark's cut is drawn in before its bytes,
// and the row counts a polygon fill retains; the bead part holds the toolkit's own three groups of beads, the side menu's,
// the top menu's and the controls', a segment each. A program adds parts of its own after them through
// GUI_BLOCK_PARTS_OWN, which its build defines as part_(NAME, segments) lines, none where it defines none
#ifndef GUI_BLOCK_PARTS_OWN
#define GUI_BLOCK_PARTS_OWN(part_)
#endif
#define GUI_BLOCK_PARTS(part_)                                                                                        \
    part_(FRAMES, 2u)                                                                                                  \
    part_(BACKGROUND, 1u)                                                                                              \
    part_(PREFERENCES, 1u)                                                                                             \
    part_(PREFERENCES_GROUND, 1u)                                                                                      \
    part_(DIALOG, 1u)                                                                                                  \
    part_(DIALOG_GROUND, 1u)                                                                                           \
    part_(BASE_RETAINED, 1u)                                                                                               \
    part_(WORDMARK_UNDER, 1u)                                                                                          \
    part_(WORDMARK_NAME, 1u)                                                                                           \
    part_(WORDMARK_FLIGHT, 1u)                                                                                         \
    part_(WORDMARK_WORK, 1u)                                                                                           \
    part_(ICONS, 2u)                                                                                                   \
    part_(ICON_COVERS, 1u)                                                                                             \
    part_(ICON_WORK, 1u)                                                                                               \
    part_(FILL_WORK, 1u)                                                                                               \
    part_(GLYPH_TABLES, 1u)                                                                                            \
    part_(GLYPH_CELLS, 3u)                                                                                             \
    part_(GLYPH_MASKS, 1u)                                                                                             \
    part_(GLYPH_WORK, 1u)                                                                                              \
    part_(FACES, 8u)                                                                                                   \
    part_(CONTROLS, 1u)                                                                                                \
    part_(BEAD, 3u)                                                                                                    \
    part_(BUSY, 2u)                                                                                                    \
    part_(STAGES, 1u)                                                                                                  \
    part_(LATTICE, 8u)                                                                                                 \
    part_(BASE_TILES, 1u)                                                                                              \
    GUI_BLOCK_PARTS_OWN(part_)
#define GUI_BLOCK_PART_NAMED(name_, segments_) GUI_BLOCK_##name_,
enum
{
    GUI_BLOCK_PARTS(GUI_BLOCK_PART_NAMED) GUI_BLOCK_COUNT
};
// the max segments a part has
#define GUI_BLOCK_SEGMENTS_MAX 8u

// the block reserved for a monitor `width` by `height`, nothing of it committed: 1, or 0 where there is no size or the
// system gave no pages; the block given back whole, every part with it; and the bytes one raster of the largest monitor
// takes, which is a segment's capacity, 0 where the block has no pages
int gui_block_init(const GuiBlockSizeCfg *args);
void gui_block_free(const GuiBlockSizeCfg *args);
unsigned long long gui_block_raster_bytes(const GuiBlockSizeCfg *args);
// the ring of part `part` positioned anew over the part's span, its segments the part's, each segment's pages committed as
// far as `bytes` reach: the ring, or NULL where the block has no pages or `bytes` pass a segment; the ring as it
// is, NULL where the block has no pages; `bytes` from the start of segment `segment` of part `part`, committed:
// the span, or NULL where the block has no pages or `bytes` pass a segment; and `bytes` taken at `*cursor` from the
// start of part `part`, across its segments, committed, the cursor moved past them to a word's boundary: the span, or
// NULL where the block has no pages or the take passes the part, the cursor as it was
mmgr_ring *gui_block_ring(const GuiBlockPartCfg *args);
mmgr_ring *gui_block_ring_read(const GuiBlockPartCfg *args);
unsigned char *gui_block_span(const GuiBlockPartCfg *args);
unsigned char *gui_block_take(const GuiBlockPartCfg *args);
// a raster of `width` by `height` made clear over segment `segment` of part `part`, its pixels committed: 1, or 0
// where the block has no pages or the raster passes the segment
int gui_block_raster_init(const GuiBlockRasterCfg *args);
// the framebuffers positioned over the frames and background parts for a window `width` by `height`, a segment's capacity
// one raster of the largest monitor: 1, or 0 where the block has no pages or the window passes the capacity; and the
// framebuffers sized to a window `width` by `height`, each segment's pages committed as far as the window draws: 1, or
// 0 with nothing changed where the window passes the capacity
int gui_block_framebuffer_init(const GuiBlockFramebufferCfg *args);
int gui_block_framebuffer_fit(const GuiBlockFramebufferCfg *args);

// The system's pages the block positions in, as the caller's system does them: `bytes` of pages reserved, NULL where the
// system gives none; `bytes` of pages at `at` committed, 1 or 0; and `bytes` of pages at `at` given back
typedef struct
{
    void *(*reserve)(unsigned long long bytes);
    int (*commit)(void *at, unsigned long long bytes);
    void (*free)(void *at, unsigned long long bytes);
} GuiBlockPages;

struct GuiBlockSizeCfg
{
    unsigned int width;
    unsigned int height;
    const GuiBlockPages *pages;
};

struct GuiBlockPartCfg
{
    unsigned int part;
    unsigned int segment;
    unsigned long long bytes;
    unsigned long long *cursor;
};

struct GuiBlockRasterCfg
{
    GuiRaster *raster;
    unsigned int width;
    unsigned int height;
    unsigned int part;
    unsigned int segment;
};

struct GuiBlockFramebufferCfg
{
    GuiFramebuffer *framebuffer;
    unsigned int width;
    unsigned int height;
};

typedef struct
{
    int (*init)(const GuiBlockSizeCfg *args);
    void (*free)(const GuiBlockSizeCfg *args);
    unsigned long long (*raster_bytes)(const GuiBlockSizeCfg *args);
} GuiBlockSizeNs;
EMBED_TABLE_LAYOUT(GuiBlockSizeNs, init, free, raster_bytes);
EMBED_TABLE_STORAGE GuiBlockSizeNs gui_block_size EMBED_UNUSED = {
    .init = gui_block_init,
    .free = gui_block_free,
    .raster_bytes = gui_block_raster_bytes,
};

typedef struct
{
    mmgr_ring *(*ring)(const GuiBlockPartCfg *args);
    mmgr_ring *(*ring_read)(const GuiBlockPartCfg *args);
    unsigned char *(*span)(const GuiBlockPartCfg *args);
    unsigned char *(*take)(const GuiBlockPartCfg *args);
} GuiBlockPartNs;
EMBED_TABLE_LAYOUT(GuiBlockPartNs, ring, ring_read, span, take);
EMBED_TABLE_STORAGE GuiBlockPartNs gui_block_part EMBED_UNUSED = {
    .ring = gui_block_ring,
    .ring_read = gui_block_ring_read,
    .span = gui_block_span,
    .take = gui_block_take,
};

typedef struct
{
    int (*init)(const GuiBlockRasterCfg *args);
} GuiBlockRasterNs;
EMBED_TABLE_LAYOUT(GuiBlockRasterNs, init);
EMBED_TABLE_STORAGE GuiBlockRasterNs gui_block_raster EMBED_UNUSED = {
    .init = gui_block_raster_init,
};

typedef struct
{
    int (*init)(const GuiBlockFramebufferCfg *args);
    int (*fit)(const GuiBlockFramebufferCfg *args);
} GuiBlockFramebufferNs;
EMBED_TABLE_LAYOUT(GuiBlockFramebufferNs, init, fit);
EMBED_TABLE_STORAGE GuiBlockFramebufferNs gui_block_framebuffer EMBED_UNUSED = {
    .init = gui_block_framebuffer_init,
    .fit = gui_block_framebuffer_fit,
};

#endif
