// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the pi_plane_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef PI_PLANE_INTERNAL_H
#define PI_PLANE_INTERNAL_H
//
// pi in the plane. Machin's bracket on the exact integer certifies pi's bits after the point, and the sim arranges
// them as shapes. It draws each shape as a PNG and reads it for its longest straight line of equal bits. It reads the
// same line on keyed shuffles of the same bits, arranged the same way.
//
// 1. The bits: floor((pi - 3) 2^N) is one integer at both ends of Machin's bracket, and it begins 0x243F6A8885A308D3.
// 2. Every width at once. In rows of width W, a line of step (dy, dx) is the progression of difference
//    dy W + dx. The longest run of equal bits at every difference L below N reads every line in every width.
//    That includes a line that wraps the end of a row. The device reads it, one thread a difference, and the host
//    reads it again for pi.
// 3. The shapes: rows of width 7, 16, 32, 64, 106, 113, 128 and 256. Here 7, 106 and 113 are the denominators of
//    pi's convergents 22/7, 333/106 and 355/113, and its floors, the partial quotients, are 3, 7, 15, 1, 292. Next,
//    the tower whose row k contains k bits. Next, a disc filled by horizontal lines and filled ring by ring. Then the
//    square spiral. Last, the twindragon: bit n at the Gaussian integer that n's binary digits name in base -1 + i.
//    Each arranges every bit on its own pixel. The sim reads a shape's line along every step (dy, dx) with coprime
//    parts of at max 6.
// 4. Against chance: the sim takes every reading again on keyed Fisher-Yates shuffles of the bits. The shuffles keep
//    the count of ones. The sim ranks pi's reading among theirs.

#include "sim.h"

#define PLANE_BITS 16384u

// the lag scan stores the bits in one block's shared memory, one byte a bit
#define PLANE_BITS_MAX 32768u

// the guard bits of Machin's bracket past the bits read
#define PLANE_GUARD 32u

#define PLANE_PUBLISHED_BITS 0x243F6A8885A308D3ULL

#define PLANE_DRAWS 99u

#define PLANE_KEY 0x504C414E45ULL

#define PLANE_THREADS 256u

// the widest part of a line's step on a shape
#define PLANE_STEP_MAX 6LL

// the steps of a line at max: one of each pair of opposites with parts of at max PLANE_STEP_MAX
#define PLANE_STEPS_MAX 128u

// the sim scales a picture's longer side up to at max this many pixels
#define PLANE_PICTURE_MAX 1024ULL

// the shade of a pixel no bit lands on, midway. A one and a zero are equally far from it
#define PLANE_EMPTY_SHADE 128u

// the twindragon's pixels by their depth from its edge, 1 to this, the deepest class with the rest
#define PLANE_DEPTHS 8u

// the golden sphere's spirals join point i to i + F_k: the Fibonacci differences below the bits
#define PLANE_FIBONACCI 20u

// the empty columns between the sphere's four views in its picture
#define PLANE_SPHERE_GAP 3LL

// the rings drawn unrolled at this many pixels across, each ring a row this tall
#define PLANE_RING_UNROLLED_WIDTH 2048ULL

#define PLANE_RING_ROW 24ULL

// the fixed point, 2^this, of the rings' turn
#define PLANE_FIXED 40u

// the rings drawn enlarged about the center, and how much
#define PLANE_RING_CENTER 10u

#define PLANE_RING_CENTER_SCALE 2LL

// the rings at max: ring k holds floor(pi 2^k) bits, and the rings that fit PLANE_BITS_MAX are far fewer
#define PLANE_RINGS_MAX 32u

// the bytes of a shape's name, its terminator among them
#define PLANE_NAME_BYTES 32u

typedef struct
{
    unsigned long long agree;
    unsigned long long pairs;
    unsigned long long run;
    unsigned long long run_ring;
    unsigned long long run_place;
} PlaneRingMeasurement;

typedef struct
{
    long long x;
    long long y;
    long long z;
} PlaneSpherePoint;

#define PLANE_WIDTHS 8u

#define PLANE_TOP_LAGS 8u

// the density of ones over 2^m classes of the bits' index for m = 1 to this, each class two bits or more
#define PLANE_SCALES 13u

// A shape: its name, its extents, and the bit on each of its cells, -1 where no bit lands. The cells are one cell of
// the persistent tier, taken at the extents, as the sim reads each shape to its end
typedef struct
{
    char name[PLANE_NAME_BYTES];
    unsigned long long height;
    unsigned long long width;
    long long *cell;
    size_t cell_count;
} PlaneExtent;

typedef struct
{
    long long row;
    long long column;
} PlaneStep;

// the steps of a line, in a table at a fixed bound with their count
typedef struct
{
    PlaneStep step[PLANE_STEPS_MAX];
    size_t count;
} PlaneSteps;

typedef struct
{
    unsigned long long length;
    PlaneStep step;
    long long start;
} PlaneLine;

typedef struct
{
    long long x;
    long long y;
} PlanePoint;

// the sizes of the rings, in a table at a fixed bound with their count
typedef struct
{
    unsigned long long size[PLANE_RINGS_MAX];
    size_t count;
} PlaneRings;

void plane_refused(const char *why);

void *plane_taken(unsigned long long count, unsigned long long size, int ephemeral);

void plane_given(void *cell);

void plane_extent_given(PlaneExtent *extent);

void plane_name_set(PlaneExtent *extent, const char *name);

int plane_bits(SimResults *results, unsigned int count, unsigned char *bits);

int plane_extent_place(PlaneExtent *extent, const char *name, const long long *rows, const long long *columns,
                       size_t count);

int plane_rows(PlaneExtent *extent, unsigned int count, unsigned long long width);

int plane_tower(PlaneExtent *extent, unsigned int count);

int plane_half(const PlanePoint &point);

int plane_ring_order(const void *left, const void *right);

int plane_raster_order(const void *left, const void *right);

int plane_disc(PlaneExtent *by_rows, PlaneExtent *by_rings, unsigned int count);

int plane_spiral(PlaneExtent *extent, unsigned int count);

int plane_twindragon(PlaneExtent *extent, unsigned int count);

size_t plane_shell(unsigned int count, long long *radius, PlaneSpherePoint **points);

int plane_latitude_order(const void *left, const void *right);

int plane_meridian_order(const void *left, const void *right);

void plane_sphere_views(PlaneExtent *extent, const char *name, const PlaneSpherePoint *points, size_t count,
                        long long radius);

unsigned long long plane_latitude_spread(const PlaneSpherePoint *points, size_t count, long long radius,
                                         const unsigned char *bits);

void plane_steps(PlaneSteps *steps);

long long plane_cell_at(const PlaneExtent &extent, long long row, long long column);

PlaneLine plane_longest_line(const PlaneExtent &extent, const unsigned char *bits, const PlaneSteps *steps);

void plane_lag_host(const unsigned char *bits, unsigned int count, unsigned int *longest);

int plane_lag_device(SimResults *results, const unsigned char *bits, unsigned int count, unsigned char *device_bits,
                     unsigned int *device_longest, unsigned int *longest);

unsigned int plane_lag_max(const unsigned int *longest, unsigned int count, unsigned int *lag);

unsigned long long plane_spread(const unsigned char *bits, unsigned int count, unsigned int scale, int by_block);

void plane_depth(const PlaneExtent &extent, unsigned int *depth);

unsigned long long plane_depth_spread(const PlaneExtent &extent, const unsigned int *depth, const unsigned char *bits,
                                      unsigned long long *ones, unsigned long long *sizes);

int plane_png(const char *path, const unsigned char *pixels, unsigned long long width, unsigned long long height,
              unsigned int channels);

int plane_picture(const PlaneExtent &extent, const unsigned char *bits, const char *path);

int plane_sparse_picture(const PlaneExtent &extent, const unsigned char *bits, const PlaneLine &found,
                         const char *path);

void plane_ring_sizes(const unsigned char *bits, size_t count, unsigned long long max, PlaneRings *rings);

unsigned long long plane_ring_total(const PlaneRings *rings);

PlaneRingMeasurement plane_ring_read(const PlaneRings *rings, const unsigned char *bits);

int plane_ring_unrolled(const PlaneRings *rings, const unsigned char *bits, const char *path);

// the turn in integers: pi from its certified bits in 2^PLANE_FIXED, atan(2^-i) by its series, and CORDIC
typedef struct
{
    long long pi;
    long long gain;
    long long arctangent[PLANE_FIXED];
} PlaneTurn;

PlaneTurn plane_turn(const unsigned char *bits);

int plane_ring_circle(const PlaneRings *rings, const unsigned char *bits, const PlaneTurn &turn, size_t ring_count,
                      long long scale, const char *path);

void plane_shuffle(const unsigned char *bits, size_t count, unsigned int draw, unsigned char *drawn);

void plane_ring_spiral_read(SimResults *results, const PlaneRings *rings, const unsigned char *block, size_t count,
                            unsigned int draws, const char *label);

// the blind pairs contain rings 0 to this less one, and lie in the first this many blocks past the seen bits
#define PLANE_BLIND_RINGS 12u

#define PLANE_BLIND_MAX 16u

// the first round of blind pairs, which the eye scores 6 of 6
#define PLANE_BLIND_SCORED 6u

int plane_blind(SimResults *results, const PlaneRings *rings, unsigned long long seen, unsigned int first,
                unsigned int pairs, int balanced, const char *directory);

#define PLANE_FUNNEL_READINGS 3u

typedef struct
{
    size_t ring;
    unsigned long long first;
    unsigned long long last;
} PlaneBar;

// the funnels' depth, their bars' fit to one ratio, and the bits that agree with their parent
typedef struct
{
    unsigned long long value[PLANE_FUNNEL_READINGS];
} PlaneFunnelMeasurement;

int plane_funnels(SimResults *results, const PlaneRings *rings, unsigned long long seen, unsigned int fresh,
                  unsigned int draws, const char *directory);

int plane_rings(SimResults *results, const unsigned char *bits, unsigned int bits_count, unsigned int draws,
                unsigned int blind, unsigned int blind_first, int balanced, unsigned int funnels,
                const char *directory);

size_t plane_path_cells(const PlaneExtent &extent, const char *path, long long **under);

unsigned long long plane_ones_under(const long long *under, size_t count, const unsigned char *bits);

const char *plane_text(int count, char **arguments, const char *name);

const char *plane_directory(int count, char **arguments);

void plane_path(char *path, const char *directory, const char *stem, const char *name, const char *tail);

unsigned int plane_number(int count, char **arguments, const char *name, unsigned int fallback);

void plane_line_print(VerbaLinea *line, const PlaneLine &found);

void plane_band_print(VerbaLinea *line, const unsigned long long *band, size_t count, unsigned long long measurement);

#endif
