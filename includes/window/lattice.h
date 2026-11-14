// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice.h: the lattice's computing for the window: its numbers, the stage of its loop, each slot's lattice, the pens
// its dots are shaded from and the dots themselves. The lattice (lattice.c) arranges, moves, shades and judges the
// points; the window draws the dots into its ground (ground.c)
#ifndef LATTICE_H
#define LATTICE_H

#include "embed_types.h"

// how far the max disordered point goes from its place, in pixels, and the seed of every lattice's points
#define LATTICE_REACH (22.0 * 2.4)
#define LATTICE_SEED 1729u
// where one side is ordered: the share of the lattice before its points start to move, and the share over which they
// go from in place to as far as they go
#define LATTICE_ORDERED 0.1
#define LATTICE_RAMP 0.54
#define LATTICE_PI 3.141592653589793
// the lattices retained at once, the window's in the first and the checks' in the others; the levels of pen the dots are
// shaded from; and the share of a pen's radius a dot is drawn at
#define LATTICE_SLOTS 8u
#define LATTICE_LEVELS 24
#define LATTICE_FITS 0.955
// A dot's ink fades as the lattice aligns about it: its own and its four nearest neighbors' shares of their reach
// gone from their origins, taken together, run its ink from none at their origins to its shade's whole at this
// share, so an aligned stretch of the lattice reads as the ground itself
#define LATTICE_FADE_SPAN 0.25f

// the types of stage: one side ordered at an angle, the order spreading from a front, the whole lattice at one
// disorder, and the order gathering from the top; the shader compares them as 0.0 to 3.0
enum LatticeStageType
{
    LATTICE_SIDED = 0,
    LATTICE_SPREAD = 1,
    LATTICE_WHOLE = 2,
    LATTICE_GATHER = 3,
};

// where the loop is: its stage's type, the angle of a side, the front of a spread and the share of the stage
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
    embed_u32 color;
} LatticeDot;

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "LatticeDot: a color's bytes are red first in memory");

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
    embed_u32 color[LATTICE_SHADES];
    float radius[LATTICE_SHADES];
} LatticeShades;

// how far a point's offset reaches as a signed byte: the radius of its circle of movement
#define LATTICE_OFFSET_MAX 127.0

// one lattice: its points are on a grid, `columns` across and `rows` down, the first `half` a step in from the top
// left and the rest a `step` apart: a point's origin is its row and column, and it stays where it is whatever the box
// it is arranged over; each retains the place it moves toward on its circle around its origin, drawn from a seed of its
// row and column, as its offset across and down, a signed byte each, the circle's radius LATTICE_OFFSET_MAX, two bytes
// a point for as many as `capacity` points. Made once the window draws with them, as many as `dots_capacity` hold:
// each point moved and shaded as a dot
typedef struct
{
    embed_i8 *offsets;
    LatticeDot *dots;
    embed_u32 capacity;
    embed_u32 dots_capacity;
    embed_u32 count;
    embed_u32 columns;
    embed_u32 rows;
    double half;
    double step;
} LatticeSlot;

// the lattice of `slot`, which is below LATTICE_SLOTS
LatticeSlot *lattice_slot(embed_u32 slot);

// arranges the lattice of `slot` over a box `width` by `height` pixels: the points a step apart from half a step in
// from the top left, as many as are inside the box, at least one each way; each point's place on its circle drawn
// from its own seed, its way round and its share of the radius. The count of points, 0 where `slot` is past the last
// or the memory cannot hold them
embed_u32 lattice_position(embed_u32 slot, double width, double height);

// where the loop is at `seconds`
LatticeStageAt *lattice_stage(double seconds);

// the shades made from `pens`: each the two pens either side of its disorder blended by where it is between them,
// its radius at LATTICE_FITS of theirs
void lattice_shaded(const LatticePens *pens, LatticeShades *shades);

// every point of `lattice`, `width` by `height` pixels, moved where the loop at `seconds` puts it, then shaded from
// `shades` by its disorder there, into its dots: 1, or 0 where the memory cannot hold them
embed_u32 lattice_dots_moved(LatticeSlot *lattice, double width, double height, double seconds,
                             const LatticeShades *shades);

// 1 where the loop moved, from `then` to `now` in seconds, enough to be seen; 0 where no point moved that far
embed_u32 lattice_shows(double then, double now);

// the soonest moment from `from` on at which the loop has moved since `then` enough to be seen: the moment a window
// that drew the loop at `then` next draws it; `from` and a loop past it where none shows
double lattice_displayed_after(double then, double from);

// the cells of `slot` given back, its lattice arranged over nothing
void lattice_given(embed_u32 slot);

#endif
