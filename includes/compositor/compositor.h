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

// The layers of a surface, bottom to top: the backdrop, the glass over it, the content, the glyphs, and the effects and
// overlays. The top layer is the cursor, with the drawings at the rate of the pointer
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

// What a command draws. A fill is a rectangle in a color, with each edge pixel covered by the share that the
// rectangle covers. An image goes with its top left at the place of the command
#define GUI_COMPOSITOR_FILL 1u
#define GUI_COMPOSITOR_IMAGE 2u
#define GUI_COMPOSITOR_GLYPH_RUN 3u

// The places across a pixel where the atlas holds a glyph, a quarter of a pixel apart. The place is (x >> 14) & 3 of
// its 16.16 x. Then come the contrast bands that lift the text of a run. Then come the most glyph ranges that a
// compositor reserves, and the entry that is no glyph
#define GUI_COMPOSITOR_PHASES 4u
#define GUI_COMPOSITOR_PHASE(fixed_) ((embed_u32)((fixed_) >> 14) & 3u)
#define GUI_COMPOSITOR_BANDS 4u
#define GUI_COMPOSITOR_RANGES 8u
#define GUI_COMPOSITOR_NO_GLYPH 0xffffffffu

// A command as a layer records it. Each field has a fixed width, and no field is a pointer. A list of commands is plain
// data that a caller can write out and record again.
//
// It holds the type, and the color, BGRA multiplied by its alpha. It holds the place and the size in 16.16, for a run
// the box that its glyphs cover. For an image it holds the handle of the image. For a run it holds the first of
// its glyphs in the list of the layer, and how many glyphs it has. It holds the digest of the glyphs of a run, which
// differs for a run with other glyphs. Last it holds the first and last tile that it covers across and down
typedef struct
{
    embed_u32 type;
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

// A glyph in the atlas. It holds its key, the face, size, place and glyph packed, and its page and its box on the page.
// It holds the top left of its box from the pen, left of it and above the baseline. It holds its advance in 16.16, and
// whether the atlas holds it yet
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

// A range of glyphs that the compositor reserves at init. It holds its face and size, its first glyph and how many.
// It holds the first of its entries, four a glyph, one for each place across a pixel
typedef struct
{
    embed_u32 face;
    embed_u32 size;
    embed_u32 first;
    embed_u32 count;
    embed_u32 base;
} GuiCompositorGlyphRange;

// An atlas page. It holds how many segments its skyline has, and the segments, each a left, a height and a width. It
// holds when a run last drew from it. It holds whether a reserved range keeps the compositor from a release of it
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

// The sizes of a compositor. They are the widest and tallest surface, and the most commands that a layer records. Then
// come the rectangles that the clip ring of a surface holds before it folds. Then come the retained tiles of all the surfaces
// together. Then come the side of an atlas page in pixels, a power of 2 no more than 4096, and how many pages. Last
// come the glyphs that the atlas holds, and the glyphs that the runs of a layer hold together
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

// A layer of a surface. It holds the commands that it recorded last and the commands that it records now, with how many
// of each. It holds whether it records now.
//
// It holds its pool indices: the retained tile of each cell in the pool, GUI_COMPOSITOR_NONE for none. It holds its
// dirty tiles, a bit for each, which a retained layer draws again. It holds the glyphs of its runs, shown and recorded
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

// A surface. It holds whether it is open, its size in pixels and in tiles, and the color under the backdrop. It holds
// which layers keep their pixels, a bit for each, and the tiles that its clip ring marked, a bit for each. It holds its
// clip ring and how many rectangles that holds, the words of a set of tile bits, and its layers
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

// A compositor. It holds its limits, the pool of retained tiles and its stack of free tiles, its surfaces and its
// images.
//
// It holds its atlas. That is the coverage bytes of the pages, the pages, and the glyph entries. It is the hash of the
// entries past the ranges, with its mask. It is also the ranges and how many. It is the contrast tables that lift a
// run, a byte of coverage to a byte for each band. Last is the stamp that each compose moves on
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
// the surface `surface` opened `width` by `height` over `clear`. The layers in `retained` keep their pixels, and each
// tile is dirty. The result is 1, or 0 where the size passes the limits or the retained tiles are too few. When the
// surface shuts, its tiles go back to the pool
int gui_compositor_surface_open(const GuiCompositorSurfaceCfg *args);
void gui_compositor_surface_shut(const GuiCompositorSurfaceCfg *args);
// the layer `layer` of `surface` starts to record, with an empty list. The second call ends the record. It compares
// the recorded list with the list that it records over, command by command. Each command that differs marks the
// tiles of both as dirty. The result is how many tiles it marked
void gui_compositor_record_begin(const GuiCompositorLayerCfg *args);
embed_u32 gui_compositor_record_end(const GuiCompositorLayerCfg *args);
// a rectangle `width` by `height` at x, y, all in 16.16, recorded filled in `color`: 1, or 0 where the layer does not
// record or its list is full. An empty rectangle records nothing and is 1
int gui_compositor_fill(const GuiCompositorFillCfg *args);
// the image `image` recorded with its top left at x, y in 16.16, without its fraction. The result is 1, or 0 as for a
// fill, or where the handle names no image
int gui_compositor_put(const GuiCompositorPutCfg *args);
// a run of `count` glyphs recorded in `color`, with the pens and entries of its glyphs copied into the list of the
// layer. The result is 1, or 0 where the layer does not record, its lists are full, or an entry is not in the atlas
int gui_compositor_glyphs(const GuiCompositorGlyphsCfg *args);
// `count` commands recorded as the caller gives them, a list that another layer recorded, with the `glyph_count`
// glyphs that its runs draw. The result is 1, or 0 where they do not fit. The call finds the tiles of each command
// again for this surface
int gui_compositor_replay(const GuiCompositorReplayCfg *args);
// The glyphs `first` on, `count` of them, of `face` at `size`, reserved in the table. The table finds a glyph of the
// range by its place in it, with no hash. The result is 1, or 0 where the ranges or the entries are full. The
// compositor never lets go of the pages that hold its glyphs
int gui_compositor_glyph_range(const GuiCompositorGlyphRangeCfg *args);
// The glyph put into the atlas from `coverage`, `width` by `height` bytes row after row, at the place `phase` across a
// pixel. The result is its entry. It is GUI_COMPOSITOR_NO_GLYPH where no page has room after the release of the page
// with the oldest use. The call writes over a glyph that it finds again
embed_u32 gui_compositor_glyph_bake(const GuiCompositorGlyphBakeCfg *args);
// the entry of a glyph baked at the place `phase`, GUI_COMPOSITOR_NO_GLYPH where it is not
embed_u32 gui_compositor_glyph_find(const GuiCompositorGlyphFindCfg *args);
// the rectangle `rect` pushed on the surface's clip ring: its tiles are dirty at the next compose
void gui_compositor_damage(const GuiCompositorDamageCfg *args);
// the dirty tiles of `surface` made into `raster`, of the size of the surface. The result is how many tiles the call
// made. Each tile is clean after
embed_u32 gui_compositor_compose(const GuiCompositorComposeCfg *args);
// `count` pixels of `source` put over `target`, both multiplied by their alpha. The call does four at a time in a
// vector where the target has one. The scalar form does one at a time and gives the same pixels
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
