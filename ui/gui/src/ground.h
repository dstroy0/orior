// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground.h: the window's background: the lattice drawn into a raster every frame starts from. It is redrawn only when
// the lattice next changes on screen, and its clock stops while the window is off screen (ground.c)
#ifndef GUI_GROUND_H
#define GUI_GROUND_H
typedef struct GuiGroundRasterCfg GuiGroundRasterCfg;
typedef struct GuiGroundCfg GuiGroundCfg;
typedef struct GuiGroundClockCfg GuiGroundClockCfg;
#include "embed_dispatch_layout.h"
#include "lattice.h"
#include "palette.h"

// How a dot is shaded by its order: ink from the least ordered to the most, and radius in page pixels
#define GUI_GROUND_DIM 0.85
#define GUI_GROUND_FAINTEST (0.426 * GUI_GROUND_DIM)
#define GUI_GROUND_BRIGHTEST 0.42
#define GUI_GROUND_RADIUS 0.59
#define GUI_GROUND_RADIUS_SPREAD 0.16

// The clock is `now - start_ms`, or `paused_ms - start_ms` while paused. `paused_ms` is 0 while it runs. `next_draw_ms`
// is on that clock.
typedef struct
{
    GuiRaster raster;
    uint32_t lattice_index;
    unsigned int width;
    unsigned int height;
    int scale;
    int laid;
    LatticeShades shades;
    unsigned long long start_ms;
    unsigned long long paused_ms;
    unsigned long long next_draw_ms;
    unsigned int frames;
} GuiGround;

// Takes a lattice instance and starts the clock at `now`. Returns 0 when every instance is taken.
int gui_ground_init(const GuiGroundCfg *args);
// Lets go of the raster, which the caller owns, and of the lattice instance.
void gui_ground_free(const GuiGroundCfg *args);
// Stops the clock at `now` when `paused` is 1, and restarts it where it stopped when 0.
void gui_ground_pause(const GuiGroundCfg *args);
// Lays the lattice over a `width` by `height` window at `scale` and draws into `raster`, which the caller owns. Returns
// 0 when the lattice could not be laid; the raster then holds the deep color alone.
int gui_ground_layout(const GuiGroundRasterCfg *args);
// Draws the deep color and every dot at the clock's time.
void gui_ground_draw(const GuiGroundRasterCfg *args);
// Redraws when the clock has reached `next_draw_ms`. Returns 1 when it drew.
int gui_ground_tick(const GuiGroundRasterCfg *args);
// Milliseconds until the next draw, at least 1, or 0 for never while paused or not laid.
unsigned long long gui_ground_next_deadline_ms(const GuiGroundClockCfg *args);
// Milliseconds the clock has run at `now`.
unsigned long long gui_ground_elapsed_ms(const GuiGroundClockCfg *args);
struct GuiGroundCfg
{
    GuiGround *ground;
    int paused;
};

struct GuiGroundRasterCfg
{
    GuiGround *ground;
    const GuiRaster *raster;
    unsigned int width;
    unsigned int height;
    int scale;
    const GuiPalette *palette;
};

struct GuiGroundClockCfg
{
    const GuiGround *ground;
};

typedef struct
{
    int (*init)(const GuiGroundCfg *args);
    void (*free)(const GuiGroundCfg *args);
    void (*pause)(const GuiGroundCfg *args);
} GuiGroundNs;
EMBED_TABLE_LAYOUT(GuiGroundNs, init, free, pause);
EMBED_TABLE_STORAGE GuiGroundNs gui_ground EMBED_UNUSED = {
    .init = gui_ground_init,
    .free = gui_ground_free,
    .pause = gui_ground_pause,
};

typedef struct
{
    int (*layout)(const GuiGroundRasterCfg *args);
    void (*draw)(const GuiGroundRasterCfg *args);
    int (*tick)(const GuiGroundRasterCfg *args);
} GuiGroundRasterNs;
EMBED_TABLE_LAYOUT(GuiGroundRasterNs, layout, draw, tick);
EMBED_TABLE_STORAGE GuiGroundRasterNs gui_ground_raster EMBED_UNUSED = {
    .layout = gui_ground_layout,
    .draw = gui_ground_draw,
    .tick = gui_ground_tick,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiGroundClockCfg *args);
    unsigned long long (*elapsed_ms)(const GuiGroundClockCfg *args);
} GuiGroundClockNs;
EMBED_TABLE_LAYOUT(GuiGroundClockNs, next_deadline_ms, elapsed_ms);
EMBED_TABLE_STORAGE GuiGroundClockNs gui_ground_clock EMBED_UNUSED = {
    .next_deadline_ms = gui_ground_next_deadline_ms,
    .elapsed_ms = gui_ground_elapsed_ms,
};

#endif
