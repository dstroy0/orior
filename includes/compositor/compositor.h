// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compositor.h: the layers a window's pixels are made from, and the one way they move. A compositor holds surfaces,
// one for each window the system shows. A surface holds six layers in a fixed order, from the backdrop at the bottom
// to the cursor at the top, over a grid of tiles 64 pixels on a side. A part of the window records what it draws into
// its layer as commands. A command is a fixed record of integers and indices, its places in 16.16 fixed point: the
// pixel is the value shifted right 16. A layer that records the same commands again changes nothing. A command that
// differs marks the tiles it covers, before and after, as dirty, and a rectangle pushed on the surface's clip ring
// marks its tiles too. A compose then makes only the dirty tiles, bottom layer to top, into the caller's raster. A
// retained layer keeps the pixels of its tiles and draws a tile again only where it is dirty. A direct layer draws
// its commands straight into the raster each time a tile is made. Text is a run of glyphs out of the compositor's
// atlas: pages of coverage bytes the caller bakes each glyph into at one of four places across a pixel, found by a
// table where the caller reserves a range at init and by a hash past it. Every table is placed at init in the caller's
// block, and a frame takes no memory. Nothing in a compositor calls back to its caller (compositor.c, atlas.c)
#ifndef GUI_COMPOSITOR_H
#define GUI_COMPOSITOR_H
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"
#include "embed_types.h"

typedef struct GuiCompositorLimitsCfg GuiCompositorLimitsCfg;
typedef struct GuiCompositorInitCfg GuiCompositorInitCfg;
typedef struct GuiCompositorImageCfg GuiCompositorImageCfg;
typedef struct GuiCompositorSurfaceCfg GuiCompositorSurfaceCfg;
typedef struct GuiCompositorLayerCfg GuiCompositorLayerCfg;
typedef struct GuiCompositorFillCfg GuiCompositorFillCfg;
typedef struct GuiCompositorPutCfg GuiCompositorPutCfg;
typedef struct GuiCompositorReplayCfg GuiCompositorReplayCfg;
typedef struct GuiCompositorDamageCfg GuiCompositorDamageCfg;
typedef struct GuiCompositorComposeCfg GuiCompositorComposeCfg;
typedef struct GuiCompositorOverCfg GuiCompositorOverCfg;
typedef struct GuiCompositorGlyphsCfg GuiCompositorGlyphsCfg;
typedef struct GuiCompositorGlyphRangeCfg GuiCompositorGlyphRangeCfg;
typedef struct GuiCompositorGlyphBakeCfg GuiCompositorGlyphBakeCfg;
typedef struct GuiCompositorGlyphFindCfg GuiCompositorGlyphFindCfg;

// The layers of a surface, bottom to top: the backdrop, the glass over it, the content, the glyphs, the effects and
// overlays, and the cursor with what is drawn at the pointer's rate
#define GUI_COMPOSITOR_BACKDROP 0u
#define GUI_COMPOSITOR_GLASS 1u
#define GUI_COMPOSITOR_CONTENT 2u
#define GUI_COMPOSITOR_GLYPHS 3u
#define GUI_COMPOSITOR_OVERLAY 4u
#define GUI_COMPOSITOR_CURSOR 5u
#define GUI_COMPOSITOR_LAYERS 6u

// The pixels on a side of a tile and the shift that divides by them. The surfaces and the images a compositor holds
// at most; and the one fixed point: 16.16, the pixel the place shifted right 16
#define GUI_COMPOSITOR_TILE 64u
#define GUI_COMPOSITOR_TILE_SHIFT 6u
#define GUI_COMPOSITOR_SURFACES 4u
#define GUI_COMPOSITOR_IMAGES 32u
#define GUI_COMPOSITOR_FIXED_SHIFT 16
#define GUI_COMPOSITOR_FIXED(pixels_) ((embed_i32)(pixels_) * (embed_i32)65536)
#define GUI_COMPOSITOR_PIXEL(fixed_) ((fixed_) >> GUI_COMPOSITOR_FIXED_SHIFT)
#define GUI_COMPOSITOR_NONE 0xffffu
_Static_assert((1u << GUI_COMPOSITOR_TILE_SHIFT) == GUI_COMPOSITOR_TILE, "the tile shift divides by the tile's side");

// What a command draws: a rectangle filled in a color, its edges covered by the share of each edge pixel the
// rectangle covers; and an image put with its top left at the command's place
#define GUI_COMPOSITOR_FILL 1u
#define GUI_COMPOSITOR_IMAGE 2u
#define GUI_COMPOSITOR_GLYPH_RUN 3u

// The places a glyph is baked at across a pixel, a quarter of a pixel apart: the place is (x >> 14) & 3 of its 16.16 x.
// The contrast bands a run's text is lifted through. The glyph ranges a compositor reserves at most. And the entry
// that is no glyph
#define GUI_COMPOSITOR_PHASES 4u
#define GUI_COMPOSITOR_PHASE(fixed_) ((embed_u32)((fixed_) >> 14) & 3u)
#define GUI_COMPOSITOR_BANDS 4u
#define GUI_COMPOSITOR_RANGES 8u
#define GUI_COMPOSITOR_NO_GLYPH 0xffffffffu

// A command as a layer records it. Every field has a fixed width, and none is a pointer: a list of commands is plain
// data a caller can write out and record again. The kind; the color, BGRA multiplied by its alpha; the place and the
// size in 16.16, a run's the box its glyphs cover; the image's handle for an image, the first of a run's glyphs in
// the layer's list for a run; how many glyphs a run has; the digest of a run's glyphs: a run with other glyphs
// differs; and the first and last tile it covers across and down
typedef struct
{
    embed_u32 kind;
    embed_u32 color;
    embed_i32 x;
    embed_i32 y;
    embed_i32 width;
    embed_i32 height;
    embed_u32 image;
    embed_u32 count;
    embed_u32 digest;
    embed_u16 tile_left;
    embed_u16 tile_top;
    embed_u16 tile_right;
    embed_u16 tile_bottom;
} GuiCompositorCommand;
_Static_assert(sizeof(GuiCompositorCommand) == 44u, "a command is 44 bytes on every target");

// A glyph of a run: its pen on the baseline in 16.16, and its entry in the atlas, baked at the place its x falls on
typedef struct
{
    embed_i32 x;
    embed_i32 y;
    embed_u32 entry;
} GuiCompositorGlyph;
_Static_assert(sizeof(GuiCompositorGlyph) == 12u, "a run's glyph is 12 bytes on every target");

// A glyph baked in the atlas: its key, the face, size, place and glyph packed; its page and its box on the page. Where
// its box's top left is from the pen, left of it and above the baseline; its advance in 16.16; and whether it is baked
typedef struct
{
    embed_u64 key;
    embed_u16 page;
    embed_u16 x;
    embed_u16 y;
    embed_u16 width;
    embed_u16 height;
    embed_i16 left;
    embed_i16 top;
    embed_i32 advance;
    embed_u32 baked;
} GuiCompositorGlyphEntry;

// A range of glyphs reserved at init: its face and size, its first glyph and how many, and the first of its entries,
// four a glyph, one for each place across a pixel
typedef struct
{
    embed_u32 face;
    embed_u32 size;
    embed_u32 first;
    embed_u32 count;
    embed_u32 base;
} GuiCompositorGlyphRange;

// An atlas page: how many segments its skyline has, and the skyline's segments, each a left, a height and a width;
// when a run last drew from it; and whether a reserved range keeps it from being let go
typedef struct
{
    embed_u32 segments;
    embed_u16 *segment_x;
    embed_u16 *segment_y;
    embed_u16 *segment_width;
    embed_u32 stamp;
    int pinned;
} GuiCompositorPage;

// A rectangle in pixels on the clip ring
typedef struct
{
    embed_i32 x;
    embed_i32 y;
    embed_i32 width;
    embed_i32 height;
} GuiCompositorRect;

// The sizes a compositor is placed for: a surface's widest and tallest; the commands a layer records at most; the
// rectangles a surface's clip ring holds before it folds; the retained tiles of all the surfaces together; the side
// of an atlas page in pixels, a power of 2 no more than 4096, and how many pages; the glyphs the atlas holds; and the
// glyphs a layer's runs hold together
struct GuiCompositorLimitsCfg
{
    embed_u32 width_max;
    embed_u32 height_max;
    embed_u32 commands_max;
    embed_u32 ring_max;
    embed_u32 tiles_max;
    embed_u32 atlas_side;
    embed_u32 atlas_pages;
    embed_u32 glyphs_max;
    embed_u32 items_max;
};

// A layer of a surface: the commands it recorded last, the ones it records now, and how many of each; whether it
// records now; its pool indices, each cell's retained tile in the pool, GUI_COMPOSITOR_NONE for none; and its own dirty
// tiles, a bit for each, which a retained layer draws again; and the glyphs of its runs, shown and recorded
typedef struct
{
    GuiCompositorCommand *shown;
    GuiCompositorCommand *recorded;
    embed_u32 shown_count;
    embed_u32 recorded_count;
    int recording;
    embed_u16 *slots;
    embed_u64 *dirty;
    GuiCompositorGlyph *glyphs_shown;
    GuiCompositorGlyph *glyphs_recorded;
    embed_u32 glyphs_shown_count;
    embed_u32 glyphs_recorded_count;
} GuiCompositorLayer;

// A surface: whether it is open; its size in pixels and in tiles; the color under the backdrop; which layers keep their
// pixels, a bit for each; the tiles its clip ring marked, a bit for each; its clip ring and how many rectangles it
// holds; the words of a set of tile bits; and its layers
typedef struct
{
    int open;
    embed_u32 width;
    embed_u32 height;
    embed_u32 columns;
    embed_u32 rows;
    embed_u32 clear;
    embed_u32 retained;
    embed_u64 *dirty;
    GuiCompositorRect *ring;
    embed_u32 ring_count;
    embed_u32 words;
    GuiCompositorLayer layer[GUI_COMPOSITOR_LAYERS];
} GuiCompositorSurface;

// An image a command puts: its pixels, BGRA multiplied by their alpha, row after row of its width
typedef struct
{
    const embed_u32 *pixels;
    embed_u32 width;
    embed_u32 height;
} GuiCompositorImage;

// A compositor: the limits it was placed for; the pool of retained tiles and its stack of free tiles; its surfaces;
// its images; and its atlas: the pages' coverage bytes and the pages, the glyph entries, the hash of the entries past
// the ranges and its mask, the ranges and how many, the contrast tables a run is lifted through, a byte of coverage to a
// byte for each band, and the stamp each compose moves on
typedef struct
{
    GuiCompositorLimitsCfg limits;
    embed_u32 *pool;
    embed_u16 *free_tiles;
    embed_u32 free_count;
    GuiCompositorSurface surface[GUI_COMPOSITOR_SURFACES];
    GuiCompositorImage image[GUI_COMPOSITOR_IMAGES];
    unsigned char *coverage;
    GuiCompositorPage *page;
    GuiCompositorGlyphEntry *entry;
    embed_u32 *hash;
    embed_u32 hash_mask;
    GuiCompositorGlyphRange range[GUI_COMPOSITOR_RANGES];
    embed_u32 ranges;
    embed_u32 reserved;
    unsigned char contrast[GUI_COMPOSITOR_BANDS][256];
    embed_u32 stamp;
} GuiCompositor;

// the bytes of the block a compositor needs for `limits`, 0 where a limit is 0 or the tiles pass GUI_COMPOSITOR_NONE
embed_u64 gui_compositor_bytes(const GuiCompositorLimitsCfg *args);
// the compositor placed in `block` of `bytes` for `limits`, every surface shut: 1, or 0 where the block is short
int gui_compositor_init(const GuiCompositorInitCfg *args);
// the image `image` named for the pixels given, or forgotten where they are NULL: 1, or 0 for a handle past the table
int gui_compositor_image(const GuiCompositorImageCfg *args);
// the surface `surface` opened `width` by `height` over `clear`, the layers in `retained` keeping their pixels, every
// tile dirty: 1, or 0 where the size passes the limits or the retained tiles are short. Shut, its tiles go back to
// the pool
int gui_compositor_surface_open(const GuiCompositorSurfaceCfg *args);
void gui_compositor_surface_shut(const GuiCompositorSurfaceCfg *args);
// the layer `layer` of `surface` begins to record, its list empty; and ends: the recorded list is compared with the
// one it records over, command by command, and each command that differs marks the tiles of both as dirty. The
// result is how many tiles it marked
void gui_compositor_record_begin(const GuiCompositorLayerCfg *args);
embed_u32 gui_compositor_record_end(const GuiCompositorLayerCfg *args);
// a rectangle `width` by `height` at x, y, all in 16.16, recorded filled in `color`: 1, or 0 where the layer does not
// record or its list is full. An empty rectangle records nothing and is 1
int gui_compositor_fill(const GuiCompositorFillCfg *args);
// the image `image` recorded with its top left at x, y in 16.16, its fraction dropped: 1, or 0 as for a fill or where
// the handle names no image
int gui_compositor_put(const GuiCompositorPutCfg *args);
// a run of `count` glyphs recorded in `color`, its glyphs' pens and entries copied into the layer's list: 1, or 0
// where the layer does not record, its lists are full or an entry is no baked glyph
int gui_compositor_glyphs(const GuiCompositorGlyphsCfg *args);
// `count` commands recorded as they are given, a list another layer recorded, with the `glyph_count` glyphs its runs
// draw: 1, or 0 where they do not fit. Each command's tiles are worked out again for this surface
int gui_compositor_replay(const GuiCompositorReplayCfg *args);
// The glyphs `first` on, `count` of them, of `face` at `size`, reserved in the table: a glyph of the range is found by
// its place in it, with no hash. 1, or 0 where the ranges or the entries are full. The pages its glyphs are baked on
// are never let go
int gui_compositor_glyph_range(const GuiCompositorGlyphRangeCfg *args);
// The glyph baked into the atlas from `coverage`, `width` by `height` bytes row after row, at the place `phase` across
// a pixel: its entry, or GUI_COMPOSITOR_NO_GLYPH where no page has room once the page used least long ago is let go.
// A glyph found again is baked over
embed_u32 gui_compositor_glyph_bake(const GuiCompositorGlyphBakeCfg *args);
// the entry of a glyph baked at the place `phase`, GUI_COMPOSITOR_NO_GLYPH where it is not
embed_u32 gui_compositor_glyph_find(const GuiCompositorGlyphFindCfg *args);
// the rectangle `rect` pushed on the surface's clip ring: its tiles are dirty at the next compose
void gui_compositor_damage(const GuiCompositorDamageCfg *args);
// the dirty tiles of `surface` made into `raster` of the surface's size: how many tiles were made, every tile
// clean after
embed_u32 gui_compositor_compose(const GuiCompositorComposeCfg *args);
// `count` pixels of `source` put over `target`, both multiplied by their alpha: four at a time in a vector where the
// target has one, and one at a time in the scalar form, which gives the same pixels
void gui_compositor_over(const GuiCompositorOverCfg *args);
void gui_compositor_over_scalar(const GuiCompositorOverCfg *args);

struct GuiCompositorInitCfg
{
    GuiCompositor *compositor;
    GuiCompositorLimitsCfg limits;
    void *block;
    embed_u64 bytes;
};

struct GuiCompositorImageCfg
{
    GuiCompositor *compositor;
    embed_u32 image;
    const embed_u32 *pixels;
    embed_u32 width;
    embed_u32 height;
};

struct GuiCompositorSurfaceCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 width;
    embed_u32 height;
    embed_u32 clear;
    embed_u32 retained;
};

struct GuiCompositorLayerCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 layer;
};

struct GuiCompositorFillCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 layer;
    embed_i32 x;
    embed_i32 y;
    embed_i32 width;
    embed_i32 height;
    embed_u32 color;
};

struct GuiCompositorPutCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 layer;
    embed_i32 x;
    embed_i32 y;
    embed_u32 image;
};

struct GuiCompositorGlyphsCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 layer;
    embed_u32 color;
    const GuiCompositorGlyph *glyphs;
    embed_u32 count;
};

struct GuiCompositorGlyphRangeCfg
{
    GuiCompositor *compositor;
    embed_u32 face;
    embed_u32 size;
    embed_u32 first;
    embed_u32 count;
};

struct GuiCompositorGlyphBakeCfg
{
    GuiCompositor *compositor;
    embed_u32 face;
    embed_u32 size;
    embed_u32 glyph;
    embed_u32 phase;
    const unsigned char *coverage;
    embed_u32 width;
    embed_u32 height;
    embed_i32 left;
    embed_i32 top;
    embed_i32 advance;
};

struct GuiCompositorGlyphFindCfg
{
    const GuiCompositor *compositor_read;
    embed_u32 face;
    embed_u32 size;
    embed_u32 glyph;
    embed_u32 phase;
};

struct GuiCompositorReplayCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    embed_u32 layer;
    const GuiCompositorCommand *commands;
    embed_u32 count;
    const GuiCompositorGlyph *glyphs;
    embed_u32 glyph_count;
};

struct GuiCompositorDamageCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    GuiCompositorRect rect;
};

struct GuiCompositorComposeCfg
{
    GuiCompositor *compositor;
    embed_u32 surface;
    GuiRaster *raster;
};

struct GuiCompositorOverCfg
{
    embed_u32 *target;
    const embed_u32 *source;
    embed_u32 count;
};

typedef struct
{
    embed_u64 (*bytes)(const GuiCompositorLimitsCfg *args);
    int (*init)(const GuiCompositorInitCfg *args);
    int (*image)(const GuiCompositorImageCfg *args);
    embed_u32 (*compose)(const GuiCompositorComposeCfg *args);
    void (*damage)(const GuiCompositorDamageCfg *args);
} GuiCompositorNs;
EMBED_TABLE_LAYOUT(GuiCompositorNs, bytes, init, image, compose, damage);
EMBED_TABLE_STORAGE GuiCompositorNs gui_compositor EMBED_UNUSED = {
    .bytes = gui_compositor_bytes,
    .init = gui_compositor_init,
    .image = gui_compositor_image,
    .compose = gui_compositor_compose,
    .damage = gui_compositor_damage,
};

typedef struct
{
    int (*open)(const GuiCompositorSurfaceCfg *args);
    void (*shut)(const GuiCompositorSurfaceCfg *args);
} GuiCompositorSurfaceNs;
EMBED_TABLE_LAYOUT(GuiCompositorSurfaceNs, open, shut);
EMBED_TABLE_STORAGE GuiCompositorSurfaceNs gui_compositor_surface EMBED_UNUSED = {
    .open = gui_compositor_surface_open,
    .shut = gui_compositor_surface_shut,
};

typedef struct
{
    void (*begin)(const GuiCompositorLayerCfg *args);
    embed_u32 (*end)(const GuiCompositorLayerCfg *args);
    int (*fill)(const GuiCompositorFillCfg *args);
    int (*put)(const GuiCompositorPutCfg *args);
    int (*replay)(const GuiCompositorReplayCfg *args);
    int (*glyphs)(const GuiCompositorGlyphsCfg *args);
} GuiCompositorRecordNs;
EMBED_TABLE_LAYOUT(GuiCompositorRecordNs, begin, end, fill, put, replay, glyphs);
EMBED_TABLE_STORAGE GuiCompositorRecordNs gui_compositor_record EMBED_UNUSED = {
    .begin = gui_compositor_record_begin,
    .end = gui_compositor_record_end,
    .fill = gui_compositor_fill,
    .put = gui_compositor_put,
    .replay = gui_compositor_replay,
    .glyphs = gui_compositor_glyphs,
};

typedef struct
{
    int (*range)(const GuiCompositorGlyphRangeCfg *args);
    embed_u32 (*bake)(const GuiCompositorGlyphBakeCfg *args);
    embed_u32 (*find)(const GuiCompositorGlyphFindCfg *args);
} GuiCompositorAtlasNs;
EMBED_TABLE_LAYOUT(GuiCompositorAtlasNs, range, bake, find);
EMBED_TABLE_STORAGE GuiCompositorAtlasNs gui_compositor_atlas EMBED_UNUSED = {
    .range = gui_compositor_glyph_range,
    .bake = gui_compositor_glyph_bake,
    .find = gui_compositor_glyph_find,
};

typedef struct
{
    void (*over)(const GuiCompositorOverCfg *args);
    void (*over_scalar)(const GuiCompositorOverCfg *args);
} GuiCompositorBlendNs;
EMBED_TABLE_LAYOUT(GuiCompositorBlendNs, over, over_scalar);
EMBED_TABLE_STORAGE GuiCompositorBlendNs gui_compositor_blend EMBED_UNUSED = {
    .over = gui_compositor_over,
    .over_scalar = gui_compositor_over_scalar,
};

#endif
