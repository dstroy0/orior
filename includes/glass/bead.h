// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bead.h: a bead of water on glass over a button, lit by lights its caller places. A press on a button sinks and
// flattens its bead. The looks are baked once for each button into an atlas of frames, one frame a step of the press,
// lit from the lights at that button's place so the glint, the caustic and the shadow move as the bead sinks. A frame
// is a cell of two planes. The frame's blit reads the planes with integer work only, and warps what lies under the
// bead through its refraction. The atlas's pixels, the lights and the four colors are the caller's (bead.c)
#ifndef GUI_GLASS_BEAD_H
#define GUI_GLASS_BEAD_H
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"

typedef struct GuiBeadCfg GuiBeadCfg;
typedef struct GuiBeadBakeCfg GuiBeadBakeCfg;
typedef struct GuiBeadBlitCfg GuiBeadBlitCfg;

// The frames of the atlas. The first frame is the bead's wobble past rest, GUI_BEAD_LOW of the press, which a
// release's overshoot of about 5 percent reaches as its own frame. The last frame is the bead pressed whole, 1.0 of
// the press. The idle frame is at 0.0
#define GUI_BEAD_FRAMES 12u
#define GUI_BEAD_LOW (-0.08)
// the max buttons an atlas holds frames for, and the max lights
#define GUI_BEAD_TOOLS_MAX 32u
#define GUI_LIGHTS_MAX 4u
// The cell's margin round the button in pixels, for the shadow and the frost. The reach is the max a pixel reads
// away from itself, in pixels, for the refraction
#define GUI_BEAD_PAD 7
#define GUI_BEAD_REACH 6
// The bead's outline: a superellipse of this power over the button, round at every point, no straight run. The
// dome's height as a share of the button's side; the share of the height the press takes away; the share of the
// side the press sinks the bead; the share the press squashes the bead across and down; and water's refractive index
#define GUI_BEAD_SHAPE_POWER 3.0
#define GUI_BEAD_HEIGHT 0.36
#define GUI_BEAD_FLATTEN 0.4
#define GUI_BEAD_SINK 0.06
#define GUI_BEAD_SQUISH 0.03
#define GUI_BEAD_INDEX 1.333
// The bead's outline over a button: a dome, the superellipse, round at every point; or a tab, a rectangle with its top
// corners rounded by the atlas's radius and its foot flat on the row's line, the dome's profile rising from every
// edge to a flat top
#define GUI_BEAD_SHAPE_DOME 0u
#define GUI_BEAD_SHAPE_TAB 1u
// the four colors a bead is lit with, their places in `colors`
#define GUI_BEAD_COLOR_GLINT 0u
#define GUI_BEAD_COLOR_CAUSTIC 1u
#define GUI_BEAD_COLOR_RIM 2u
#define GUI_BEAD_COLOR_SHADOW 3u
#define GUI_BEAD_COLORS 4u

// Plane A packs a pixel as [mask:8 | mix:8 | dy:8 | dx:8]. dx and dy are the signed offsets the pixel reads from. mix
// is the share of 255 of the frosted ground in the pixel, 255 outside the bead and 0 inside it. mask is 0 where the
// pixel is untouched, and the blit skips it. Plane B holds the light as premultiplied 0xAARRGGBB
#define GUI_BEAD_DX_OF(word_) ((int)(((word_) & 0xffu) ^ 0x80u) - 0x80)
#define GUI_BEAD_DY_OF(word_) ((int)((((word_) >> 8u) & 0xffu) ^ 0x80u) - 0x80)
#define GUI_BEAD_MIX_OF(word_) (((word_) >> 16u) & 0xffu)
#define GUI_BEAD_MASK_OF(word_) (((word_) >> 24u) & 0xffu)

// A light above the glass: its place across and down in the caller's pixels, its height above the glass in pixels,
// and its strength, 1.0 for the key light
typedef struct
{
    double x;
    double y;
    double z;
    double strength;
} GuiLight;

// The atlas: its cells raster; the largest button's width and height it is baked for; its margin round the button
// and its reach, each in pixels; the beads' shape and, for tabs, their top corners' radius in pixels; the cell's
// width and height; the row stride of every plane; how many buttons it
// holds frames for, each button's cell top left in the caller's pixels with the button's own width and height, 0 for
// the largest's, its bead positioned from the cell's top left corner inside the margin; the lights and the bead's four
// colors they are lit with; and a bit a button, set once its frames are baked. The raster's rows hold, in order: each
// button's frames, each frame's plane A then plane B; three patches with the reach as a margin; and three planes of
// float work for the bake. The patches are the blit's own scratch. A button's frames bake the first time the bead is
// drawn on it
typedef struct
{
    GuiRaster cells;
    unsigned int width;
    unsigned int height;
    unsigned int pad;
    unsigned int reach;
    unsigned int shape;
    unsigned int radius;
    unsigned int cell_width;
    unsigned int cell_height;
    unsigned int stride;
    unsigned int tools;
    GuiRect origin[GUI_BEAD_TOOLS_MAX];
    GuiLight light[GUI_LIGHTS_MAX];
    unsigned int lights;
    GuiColor color[GUI_BEAD_COLORS];
    unsigned int made;
    unsigned int baked;
} GuiBeadAtlas;

// an atlas with nothing arranged or baked
void gui_bead_init(const GuiBeadBakeCfg *args);
// the bytes the atlas's raster takes for `tools` buttons `width` by `height` pixels with the margin `pad` and the
// reach `reach`, 0 where the sizes are refused as the bake refuses them
unsigned long long gui_bead_bytes(const GuiBeadBakeCfg *args);
// the atlas arranged for `tools` buttons `width` by `height` pixels at max, the margin `pad` round each and the
// reach `reach`, their beads of `shape` with the corner `radius`, over `pixels`, which hold `gui_bead_bytes` of the
// same sizes; their cells' top lefts `cells` with each button's own width and height, 0 for the max, lit by
// `light_count` of `lights` in `colors`, GUI_BEAD_COLORS of them: 1, or 0 where there are no pixels, a size is 0,
// the cell is too tall, the reach is below 3 or there are too many tools. A button whose frames are baked for the
// same sizes, shape, place, lights and colors retains them; any other bakes again when the bead is next drawn on it
int gui_bead_bake(const GuiBeadBakeCfg *args);
// the frames of the button `tool` baked where they are not: 1, or 0 where the atlas is not arranged
int gui_bead_tool_bake(const GuiBeadBlitCfg *args);
// the frame of the press share `press`, in `one` from rest: 0 at rest, `one` pressed whole, below 0 in the wobble
unsigned int gui_bead_frame(const GuiBeadCfg *args);
// the frame `frame` of the button `tool` blitted with the cell's top left at x, y on `raster`, inside `clip` where
// that is not empty, at `opacity` of 255: the button's frames baked where they are not, the patch under the cell
// copied into the atlas's scratch, frosted, read through the refraction, mixed and lit
void gui_bead_blit(const GuiBeadBlitCfg *args);

struct GuiBeadCfg
{
    long long press;
    long long one;
};
struct GuiBeadBakeCfg
{
    GuiBeadAtlas *atlas;
    GuiColor *pixels;
    const GuiColor *colors;
    unsigned int width;
    unsigned int height;
    unsigned int pad;
    unsigned int reach;
    unsigned int shape;
    unsigned int radius;
    const GuiRect *cells;
    unsigned int tools;
    const GuiLight *lights;
    unsigned int light_count;
};
struct GuiBeadBlitCfg
{
    GuiBeadAtlas *atlas;
    GuiRaster *raster;
    int x;
    int y;
    unsigned int tool;
    unsigned int frame;
    unsigned int opacity;
    GuiRect clip;
};
typedef struct
{
    unsigned int (*frame)(const GuiBeadCfg *args);
} GuiBeadNs;
EMBED_TABLE_LAYOUT(GuiBeadNs, frame);
EMBED_TABLE_STORAGE GuiBeadNs gui_bead EMBED_UNUSED = {
    .frame = gui_bead_frame,
};
typedef struct
{
    void (*init)(const GuiBeadBakeCfg *args);
    unsigned long long (*bytes)(const GuiBeadBakeCfg *args);
    int (*bake)(const GuiBeadBakeCfg *args);
} GuiBeadBakeNs;
EMBED_TABLE_LAYOUT(GuiBeadBakeNs, init, bytes, bake);
EMBED_TABLE_STORAGE GuiBeadBakeNs gui_bead_bake_table EMBED_UNUSED = {
    .init = gui_bead_init,
    .bytes = gui_bead_bytes,
    .bake = gui_bead_bake,
};
typedef struct
{
    int (*tool_bake)(const GuiBeadBlitCfg *args);
    void (*blit)(const GuiBeadBlitCfg *args);
} GuiBeadBlitNs;
EMBED_TABLE_LAYOUT(GuiBeadBlitNs, tool_bake, blit);
EMBED_TABLE_STORAGE GuiBeadBlitNs gui_bead_blit_table EMBED_UNUSED = {
    .tool_bake = gui_bead_tool_bake,
    .blit = gui_bead_blit,
};
#endif
