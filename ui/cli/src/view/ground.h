// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground.h: the window's background: the lattice drawn into a raster every frame starts from. It is redrawn only when
// the lattice next changes on screen, and its clock stops while the window is off screen (ground.c)
#ifndef GUI_BACKGROUND_H
#define GUI_BACKGROUND_H
#include "embed_dispatch_layout.h"
#include "lattice.h"
#include "palette.h"

typedef struct GuiBackgroundRasterCfg GuiBackgroundRasterCfg;
typedef struct GuiBackgroundCfg GuiBackgroundCfg;
typedef struct GuiBackgroundClockCfg GuiBackgroundClockCfg;

// How a dot is shaded by its order: ink from the least ordered to the most, and radius in page pixels
#define GUI_BACKGROUND_DIM 0.85
#define GUI_BACKGROUND_FAINTEST (0.426 * GUI_BACKGROUND_DIM)
#define GUI_BACKGROUND_BRIGHTEST 0.42
#define GUI_BACKGROUND_RADIUS 0.59
#define GUI_BACKGROUND_RADIUS_SPREAD 0.16
// how many times slower than the window's clock the lattice moves
#define GUI_LATTICE_SLOWING 2.0

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
} GuiBackground;

// Takes a lattice instance and starts the clock. Returns 0 when every instance is taken.
int gui_background_init(const GuiBackgroundCfg *args);
// Lets go of the raster, which the caller owns, and of the lattice instance.
void gui_background_free(const GuiBackgroundCfg *args);
// Stops the clock when `paused` is 1, and restarts it where it stopped when 0.
void gui_background_pause(const GuiBackgroundCfg *args);
// Lays the lattice over a `width` by `height` window at `scale` and draws into `raster`, which the caller owns. Returns
// 0 when the lattice could not be laid; the raster then holds the deep color alone.
int gui_background_layout(const GuiBackgroundRasterCfg *args);
// Draws the deep color and every dot at the clock's time.
void gui_background_draw(const GuiBackgroundRasterCfg *args);
// Redraws when the clock has reached `next_draw_ms`. Returns 1 when it drew.
int gui_background_tick(const GuiBackgroundRasterCfg *args);
// The next draw's deadline, or 0 for none while paused or not laid.
unsigned long long gui_background_next_deadline_ms(const GuiBackgroundClockCfg *args);
// Milliseconds the clock has run.
unsigned long long gui_background_elapsed_ms(const GuiBackgroundClockCfg *args);
struct GuiBackgroundCfg
{
    GuiBackground *background_fill;
    int paused;
};

struct GuiBackgroundRasterCfg
{
    GuiBackground *background_fill;
    const GuiRaster *raster;
    unsigned int width;
    unsigned int height;
    int scale;
    const GuiPalette *palette;
};

struct GuiBackgroundClockCfg
{
    const GuiBackground *background_fill;
};

typedef struct
{
    int (*init)(const GuiBackgroundCfg *args);
    void (*free)(const GuiBackgroundCfg *args);
    void (*pause)(const GuiBackgroundCfg *args);
} GuiBackgroundNs;
EMBED_TABLE_LAYOUT(GuiBackgroundNs, init, free, pause);
EMBED_TABLE_STORAGE GuiBackgroundNs gui_background EMBED_UNUSED = {
    .init = gui_background_init,
    .free = gui_background_free,
    .pause = gui_background_pause,
};

typedef struct
{
    int (*layout)(const GuiBackgroundRasterCfg *args);
    void (*draw)(const GuiBackgroundRasterCfg *args);
    int (*tick)(const GuiBackgroundRasterCfg *args);
} GuiBackgroundRasterNs;
EMBED_TABLE_LAYOUT(GuiBackgroundRasterNs, layout, draw, tick);
EMBED_TABLE_STORAGE GuiBackgroundRasterNs gui_background_raster EMBED_UNUSED = {
    .layout = gui_background_layout,
    .draw = gui_background_draw,
    .tick = gui_background_tick,
};

typedef struct
{
    unsigned long long (*next_deadline_ms)(const GuiBackgroundClockCfg *args);
    unsigned long long (*elapsed_ms)(const GuiBackgroundClockCfg *args);
} GuiBackgroundClockNs;
EMBED_TABLE_LAYOUT(GuiBackgroundClockNs, next_deadline_ms, elapsed_ms);
EMBED_TABLE_STORAGE GuiBackgroundClockNs gui_background_clock EMBED_UNUSED = {
    .next_deadline_ms = gui_background_next_deadline_ms,
    .elapsed_ms = gui_background_elapsed_ms,
};

#endif
