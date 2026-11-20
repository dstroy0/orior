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
// where one side is in order. The first value is the share of the lattice before its points start to move. The second
// is the share over which they go from in place to as far as they go
#define LATTICE_ORDERED 0.1
#define LATTICE_RAMP 0.54
#define LATTICE_PI 3.141592653589793
// the lattices that the code keeps at once, the lattice of the window in the first and those of the checks in the
// others. Then the levels of pen that shade the dots, and the share of the radius of a pen that a dot draws at
#define LATTICE_SLOTS 8u
#define LATTICE_LEVELS 24
#define LATTICE_FITS 0.955
// The ink of a dot fades as the lattice aligns about it. Take its own share of its reach gone from its origin, and those
// of its four nearest neighbors, together. That share runs its ink from none at their origins to the whole of its shade
// at this value. An aligned stretch of the lattice then reads as the ground itself
#define LATTICE_FADE_SPAN 0.25f

// the types of stage. In the first, one side is in order at an angle. In the second, the order spreads from a front.
// In the third, the whole lattice is at one disorder. In the fourth, the order comes in from the top. The shader
// compares them as 0.0 to 3.0
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

// one dot as the code draws it. Its place across and down and its radius are in page pixels. Its color holds red, green,
// blue and its ink, a byte each in that order
typedef struct
{
    float across;
    float down;
    float radius;
    embed_u32 color;
} LatticeDot;

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "LatticeDot: a color's bytes are red first in memory");

// the pens that shade the dots, one a level of disorder from the most order to the least. Each holds the red, green,
// blue and alpha of its color from 0 to 1, and its radius in pixels
typedef struct
{
    float color[LATTICE_LEVELS * 4];
    float radius[LATTICE_LEVELS];
} LatticePens;

// the shades of a dot, one a step of its disorder from the most order to the least. Each holds its color, as
// LatticeDot holds it, and its radius in pixels
#define LATTICE_SHADES 256u
typedef struct
{
    embed_u32 color[LATTICE_SHADES];
    float radius[LATTICE_SHADES];
} LatticeShades;

// how far a point's offset reaches as a signed byte: the radius of its circle of movement
#define LATTICE_OFFSET_MAX 127.0

// one lattice. Its points are on a grid, `columns` across and `rows` down. The first is `half` a step in from the top
// left, and the rest are a `step` apart.
//
// The origin of a point is its row and column. It stays there, whatever box the lattice covers.
//
// Each point keeps the place that it moves toward on its circle round its origin. A seed of its row and column draws
// that place. The lattice keeps it as its offset across and down, a signed byte each. The radius of the circle is
// LATTICE_OFFSET_MAX. That is two bytes a point, for as many as `capacity` points.
//
// The window makes the dots, as many as `dots_capacity` holds, once it draws with them. Each is a point, moved and
// shaded
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

// arranges the lattice of `slot` over a box `width` by `height` pixels. The points are a step apart, from half a step
// in from the top left. There are as many as fit in the box, at least one each way.
//
// The own seed of each point draws its place on its circle, its way round and its share of the radius. The result is
// the count of points, 0 where `slot` is past the last or the memory cannot hold them
embed_u32 lattice_position(embed_u32 slot, double width, double height);

// where the loop is at `seconds`
LatticeStageAt *lattice_stage(double seconds);

// the shades made from `pens`. Each blends the two pens on either side of its disorder, by where it is between them. Its
// radius is LATTICE_FITS of theirs
void lattice_shaded(const LatticePens *pens, LatticeShades *shades);

// each point of `lattice`, `width` by `height` pixels, moved to where the loop at `seconds` puts it. The code then
// shades it from `shades` by its disorder there, into its dots. The result is 1, or 0 where the memory cannot hold them
embed_u32 lattice_dots_moved(LatticeSlot *lattice, double width, double height, double seconds,
                             const LatticeShades *shades);

// 1 where the loop moved enough to see, from `then` to `now` in seconds. 0 where no point moved that far
embed_u32 lattice_shows(double then, double now);

// the soonest moment from `from` on at which the loop moved enough to see since `then`. That is the moment when a window
// that drew the loop at `then` next draws it. Where none shows, the result is `from` and a loop past it
double lattice_displayed_after(double then, double from);

// the cells of `slot` given back, its lattice arranged over nothing
void lattice_given(embed_u32 slot);

#endif
