// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice.h: the lattice's computing for the window: its numbers, the stage of its loop, each slot's lattice, the pens
// its dots are shaded from and the dots themselves. The lattice (lattice.c) lays out, moves, shades and judges the
// points; the window draws the dots into its ground (ground.c)
#ifndef LATTICE_H
#define LATTICE_H

#include <stdint.h>

// how far the most disordered point goes from its place, in pixels, and the seed of every lattice's points
#define LATTICE_REACH (22.0 * 2.4)
#define LATTICE_SEED 1729u
// where one side is ordered: the share of the lattice before its points start to move, and the share over which they
// go from in place to as far as they go
#define LATTICE_ORDERED 0.1
#define LATTICE_RAMP 0.54
#define LATTICE_PI 3.141592653589793
// the lattices kept at once, the window's in the first and the checks' in the others; the levels of pen the dots are
// shaded from; and the share of a pen's radius a dot is drawn at
#define LATTICE_SLOTS 8u
#define LATTICE_LEVELS 24
#define LATTICE_FITS 0.955

// the types of stage: one side ordered at an angle, the order spreading from a front, the whole lattice at one
// disorder, and the order gathering from the top; the shader compares them as 0.0 to 3.0
enum LatticeStageType
{
    LATTICE_SIDED = 0,
    LATTICE_SPREAD = 1,
    LATTICE_WHOLE = 2,
    LATTICE_GATHER = 3,
};

// where the loop stands: its stage's type, the angle of a side, the front of a spread and the share of the stage
// done
typedef struct
{
    double type;
    double angle;
    double front;
    double part;
} LatticeStageAt;

// one dot as it is drawn: its place across and down and its radius, in page pixels, and its color, red, green,
// blue and its ink a byte each in that order
typedef struct
{
    float across;
    float down;
    float radius;
    uint32_t color;
} LatticeDot;

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "LatticeDot: a color's bytes stand red first in memory");

// the pens the dots are shaded from, one a level of disorder from ordered to the least ordered: each color's red,
// green, blue and alpha from 0 to 1, and its radius in pixels
typedef struct
{
    float color[LATTICE_LEVELS * 4];
    float radius[LATTICE_LEVELS];
} LatticePens;

// the shades a dot takes, one a step of its disorder from ordered to the least ordered: its color, as LatticeDot's,
// and its radius in pixels
#define LATTICE_SHADES 256u
typedef struct
{
    uint32_t color[LATTICE_SHADES];
    float radius[LATTICE_SHADES];
} LatticeShades;

// how far a point's offset reaches as a signed byte: the radius of its circle of movement
#define LATTICE_OFFSET_MOST 127.0

// one lattice: its points stand on a grid, `columns` across and `rows` down, the first `half` a step in from the top
// left and the rest a `step` apart: a point's origin is its row and column, and it stays where it is whatever the box
// it is laid out over; each keeps the place it moves toward on its circle around its origin, drawn from a seed of its
// row and column, as its offset across and down, a signed byte each, the circle's radius LATTICE_OFFSET_MOST, two bytes
// a point for as many as `capacity` points. Made once the window draws with them, as many as `dots_capacity` hold:
// each point moved and shaded as a dot
typedef struct
{
    int8_t *offsets;
    LatticeDot *dots;
    uint32_t capacity;
    uint32_t dots_capacity;
    uint32_t count;
    uint32_t columns;
    uint32_t rows;
    double half;
    double step;
} LatticeSlot;

// the lattice of `slot`, which is below LATTICE_SLOTS
LatticeSlot *lattice_slot(uint32_t slot);

// lays out the lattice of `slot` over a box `width` by `height` pixels: the points a step apart from half a step in
// from the top left, as many as stand inside the box, at least one each way; each point's place on its circle drawn
// from its own seed, its way round and its share of the radius. The count of points, 0 where `slot` is past the last
// or the memory cannot hold them
uint32_t lattice_lay(uint32_t slot, double width, double height);

// where the loop stands at `seconds`
LatticeStageAt *lattice_stage(double seconds);

// the shades made from `pens`: each the two pens either side of its disorder blended by where it stands between them,
// its radius at LATTICE_FITS of theirs
void lattice_shaded(const LatticePens *pens, LatticeShades *shades);

// every point of `lattice`, `width` by `height` pixels, moved where the loop at `seconds` puts it, then shaded from
// `shades` by its disorder there, into its dots: 1, or 0 where the memory cannot hold them
uint32_t lattice_dots_moved(LatticeSlot *lattice, double width, double height, double seconds,
                            const LatticeShades *shades);

// 1 where the loop moved, from `then` to `now` in seconds, enough to be seen; 0 where no point moved that far
uint32_t lattice_shows(double then, double now);

// the soonest moment from `from` on at which the loop has moved since `then` enough to be seen: the moment a window
// that drew the loop at `then` next draws it; `from` and a loop past it where none shows
double lattice_shown_after(double then, double from);

// the cells of `slot` given back, its lattice laid out over nothing
void lattice_given(uint32_t slot);

#endif
