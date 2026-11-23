// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_rings.cu: rings, turns and spirals
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

// ring k contains floor(pi 2^k) bits, floor(pi 2^(k + 1)) = 2 floor(pi 2^k) + pi's bit k + 1; the rings that fit max
void plane_ring_sizes(const unsigned char *bits, size_t count, unsigned long long max, PlaneRings *rings)
{
    rings->count = 0u;
    unsigned long long size = 3ULL;
    unsigned long long total = 0ULL;
    for (size_t level = 0u; (level < count) && (total + size <= max); level += 1u)
    {
        if (rings->count == PLANE_RINGS_MAX)
        {
            plane_refused("the rings pass their table");
        }
        rings->size[rings->count] = size;
        rings->count += 1u;
        total += size;
        size = (2ULL * size) + (unsigned long long)bits[level];
    }
}

unsigned long long plane_ring_total(const PlaneRings *rings)
{
    unsigned long long total = 0ULL;
    for (size_t ring = 0u; ring < rings->count; ring += 1u)
    {
        total += rings->size[ring];
    }
    return total;
}

// the largest ring
static unsigned long long plane_ring_largest(const PlaneRings *rings)
{
    unsigned long long largest = 0ULL;
    for (size_t ring = 0u; ring < rings->count; ring += 1u)
    {
        largest = (rings->size[ring] > largest) ? rings->size[ring] : largest;
    }
    return largest;
}

// Each bit against its parent, the bit at the same angle one ring in, place floor(j c_(k - 1) / c_k). The answer is
// how many agree, and the longest chain of equal bits that runs outward
PlaneRingMeasurement plane_ring_read(const PlaneRings *rings, const unsigned char *bits)
{
    PlaneRingMeasurement measurement = {0ULL, 0ULL, 1ULL, 0ULL, 0ULL};
    // the chains that end on one ring and on the ring before it, in two cells of the temporary tier at the largest
    const unsigned long long largest = plane_ring_largest(rings);
    unsigned long long *previous_run = (unsigned long long *)plane_taken(largest, sizeof(unsigned long long), 1);
    unsigned long long *run = (unsigned long long *)plane_taken(largest, sizeof(unsigned long long), 1);
    unsigned long long start = 0ULL;
    unsigned long long previous_start = 0ULL;
    for (size_t ring = 0u; ring < rings->count; ring += 1u)
    {
        const unsigned long long size = rings->size[ring];
        for (unsigned long long place = 0ULL; place < size; place += 1ULL)
        {
            run[(size_t)place] = 1ULL;
        }
        for (unsigned long long place = 0ULL; (ring > 0u) && (place < size); place += 1ULL)
        {
            const unsigned long long parent = (place * rings->size[ring - 1u]) / size;
            const int same = bits[start + place] == bits[previous_start + parent];
            measurement.pairs += 1ULL;
            measurement.agree += same ? 1ULL : 0ULL;
            run[(size_t)place] = same ? (previous_run[(size_t)parent] + 1ULL) : 1ULL;
            if (run[(size_t)place] > measurement.run)
            {
                measurement.run = run[(size_t)place];
                measurement.run_ring = ring;
                measurement.run_place = place;
            }
        }
        unsigned long long *const swapped = previous_run;
        previous_run = run;
        run = swapped;
        previous_start = start;
        start += size;
    }
    plane_given(run);
    plane_given(previous_run);
    return measurement;
}

// the rings unrolled, ring k row k, bit j across [j W / c_k, (j + 1) W / c_k), each pixel the share of ones on it
int plane_ring_unrolled(const PlaneRings *rings, const unsigned char *bits, const char *path)
{
    const unsigned long long width = PLANE_RING_UNROLLED_WIDTH;
    const unsigned long long height = rings->count * PLANE_RING_ROW;
    unsigned char *const pixels = (unsigned char *)plane_taken(width * height, 1u, 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pixels, .bytes = (size_t)(width * height), .val = PLANE_EMPTY_SHADE);
    unsigned long long *const ones = (unsigned long long *)plane_taken(width, sizeof(unsigned long long), 1);
    unsigned long long *const counted = (unsigned long long *)plane_taken(width, sizeof(unsigned long long), 1);
    unsigned long long start = 0ULL;
    for (size_t ring = 0u; ring < rings->count; ring += 1u)
    {
        const unsigned long long size = rings->size[ring];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = ones, .bytes = (size_t)width * sizeof(unsigned long long), .val = 0u);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = counted, .bytes = (size_t)width * sizeof(unsigned long long),
                   .val = 0u);
        for (unsigned long long place = 0ULL; place < size; place += 1ULL)
        {
            const unsigned long long from = (place * width) / size;
            const unsigned long long reach = ((place + 1ULL) * width) / size;
            const unsigned long long to = (reach > (from + 1ULL)) ? reach : (from + 1ULL);
            for (unsigned long long column = from; column < to; column += 1ULL)
            {
                ones[(size_t)column] += (unsigned long long)bits[start + place];
                counted[(size_t)column] += 1ULL;
            }
        }
        for (unsigned long long row = 1ULL; row + 1ULL < PLANE_RING_ROW; row += 1ULL)
        {
            for (unsigned long long column = 0ULL; column < width; column += 1ULL)
            {
                // the share of ones, scaled to 255, is at max 255
                pixels[(size_t)((((ring * PLANE_RING_ROW) + row) * width) + column)] =
                    (unsigned char)((255ULL * ones[(size_t)column]) / counted[(size_t)column]);
            }
        }
        start += size;
    }
    const int written = plane_png(path, pixels, width, height, 1u);
    plane_given(counted);
    plane_given(ones);
    plane_given(pixels);
    return written;
}

// a signed quotient by 2^shift, rounded toward zero, as CORDIC takes it
static long long plane_halve(long long value, unsigned int shift)
{
    return (value >= 0LL) ? (value >> shift) : -((-value) >> shift);
}

// a signed quotient rounded to the nearest, halves away from zero
static long long plane_round(long long numerator, long long denominator)
{
    return (numerator >= 0LL) ? ((numerator + (denominator / 2LL)) / denominator)
                              : -(((-numerator) + (denominator / 2LL)) / denominator);
}

PlaneTurn plane_turn(const unsigned char *bits)
{
    PlaneTurn turn;
    const long long one = 1LL << PLANE_FIXED;
    turn.pi = 3LL * one;
    for (unsigned int place = 0u; place < PLANE_FIXED; place += 1u)
    {
        turn.pi += (long long)bits[place] << (PLANE_FIXED - 1u - place);
    }
    turn.arctangent[0] = turn.pi / 4LL;
    for (unsigned int shift = 1u; shift < PLANE_FIXED; shift += 1u)
    {
        long long total = 0LL;
        for (unsigned int term = 0u; (shift * ((2u * term) + 1u)) < PLANE_FIXED; term += 1u)
        {
            const long long part = (one >> (shift * ((2u * term) + 1u))) / (long long)((2u * term) + 1u);
            total += ((term & 1u) == 0u) ? part : -part;
        }
        turn.arctangent[shift] = total;
    }
    // the rotation by every arctangent in turn scales every vector by the same gain: read it off the turn by 0 of (1,
    // 0)
    long long gain_x = one;
    long long gain_y = 0LL;
    long long angle = 0LL;
    for (unsigned int shift = 0u; shift < PLANE_FIXED; shift += 1u)
    {
        const int up = angle >= 0LL;
        const long long next_x = up ? (gain_x - plane_halve(gain_y, shift)) : (gain_x + plane_halve(gain_y, shift));
        gain_y = up ? (gain_y + plane_halve(gain_x, shift)) : (gain_y - plane_halve(gain_x, shift));
        gain_x = next_x;
        angle += up ? -turn.arctangent[shift] : turn.arctangent[shift];
    }
    turn.gain = gain_x;
    return turn;
}

// the point at the fraction place / size of a turn, at radius, rounded to the pixel
static void plane_turn_point(const PlaneTurn &turn, unsigned long long place, unsigned long long size, long long radius,
                             long long *column, long long *row)
{
    // 2 pi 2^40 times a place below 2^15 is below 2^58, and the size is below 2^15
    long long angle = (2LL * turn.pi * (long long)place) / (long long)size;
    long long x = 1LL << PLANE_FIXED;
    long long y = 0LL;
    if (angle > turn.pi)
    {
        angle -= 2LL * turn.pi;
    }
    if (angle > turn.pi / 2LL)
    {
        x = 0LL;
        y = 1LL << PLANE_FIXED;
        angle -= turn.pi / 2LL;
    }
    else if (angle < -(turn.pi / 2LL))
    {
        x = 0LL;
        y = -(1LL << PLANE_FIXED);
        angle += turn.pi / 2LL;
    }
    for (unsigned int shift = 0u; shift < PLANE_FIXED; shift += 1u)
    {
        const int up = angle >= 0LL;
        const long long next_x = up ? (x - plane_halve(y, shift)) : (x + plane_halve(y, shift));
        y = up ? (y + plane_halve(x, shift)) : (y - plane_halve(x, shift));
        x = next_x;
        angle += up ? -turn.arctangent[shift] : turn.arctangent[shift];
    }
    // the radius is at max 2^12 and each part at max 2^41. The products stay below 2^53
    *column = plane_round(radius * x, turn.gain);
    *row = -plane_round(radius * y, turn.gain);
}

// The rings as circles, ring k at radius 2^(k - 1) times the scale, bit j at the fraction j / c_k of the turn. Each
// pixel is the share of ones on it, and the ground is gray
int plane_ring_circle(const PlaneRings *rings, const unsigned char *bits, const PlaneTurn &turn, size_t ring_count,
                      long long scale, const char *path)
{
    const long long range = ((1LL << (ring_count - 2u)) * scale) + 4LL;
    const unsigned long long side = (unsigned long long)((2LL * range) + 1LL);
    const size_t area = (size_t)(side * side);
    unsigned long long *const ones = (unsigned long long *)plane_taken(area, sizeof(unsigned long long), 1);
    unsigned long long *const counted = (unsigned long long *)plane_taken(area, sizeof(unsigned long long), 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = ones, .bytes = area * sizeof(unsigned long long), .val = 0u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = counted, .bytes = area * sizeof(unsigned long long), .val = 0u);
    unsigned long long start = 0ULL;
    for (size_t ring = 0u; ring < ring_count; ring += 1u)
    {
        // the sim draws ring 0, radius a half, at the center
        const long long radius = (ring == 0u) ? 0LL : ((1LL << (ring - 1u)) * scale);
        for (unsigned long long place = 0ULL; place < rings->size[ring]; place += 1ULL)
        {
            long long column = 0LL;
            long long row = 0LL;
            plane_turn_point(turn, place, rings->size[ring], radius, &column, &row);
            // the point lies within the radius of the center. The index is never negative
            const size_t index =
                (size_t)(((unsigned long long)(row + range) * side) + (unsigned long long)(column + range));
            ones[index] += (unsigned long long)bits[start + place];
            counted[index] += 1ULL;
        }
        start += rings->size[ring];
    }
    unsigned char *const pixels = (unsigned char *)plane_taken(area, 1u, 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pixels, .bytes = area, .val = PLANE_EMPTY_SHADE);
    for (size_t index = 0u; index < area; index += 1u)
    {
        if (counted[index] != 0ULL)
        {
            // the share of ones, scaled to 255, is at max 255
            pixels[index] = (unsigned char)((255ULL * ones[index]) / counted[index]);
        }
    }
    const int written = plane_png(path, pixels, side, side, 1u);
    plane_given(pixels);
    plane_given(counted);
    plane_given(ones);
    return written;
}

// the `count` bits into `drawn`, then a keyed Fisher-Yates shuffle of them
void plane_shuffle(const unsigned char *bits, size_t count, unsigned int draw, unsigned char *drawn)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = drawn, .src = bits, .bytes = count);
    for (size_t place = count - 1u; place > 0u; place -= 1u)
    {
        // the draw is below place + 1, a bit's index. It narrows to size_t exactly
        const size_t other = (size_t)sim_draw_below(PLANE_KEY ^ ((unsigned long long)draw << 32u), place, place + 1u);
        const unsigned char held = drawn[place];
        drawn[place] = drawn[other];
        drawn[other] = held;
    }
}

// the sim reads the spirals at every slope from -this to this, in 1 / PLANE_RING_UNROLLED_WIDTH of a turn a ring
#define PLANE_RING_SLOPES 32LL

static long long plane_floor_quotient(long long numerator, long long denominator)
{
    const long long quotient = numerator / denominator;
    return (((numerator % denominator) != 0LL) && (numerator < 0LL)) ? (quotient - 1LL) : quotient;
}

// Each bit against the bit one ring in at its angle turned back by slope / W of a turn. On the unrolled rings that is
// a line that leans slope pixels a row, and on the circle a logarithmic spiral
static unsigned long long plane_ring_slope(const PlaneRings *rings, const unsigned char *bits, long long slope,
                                           unsigned long long *pairs)
{
    // the width is 2^11. It fits a signed word
    const long long width = (long long)PLANE_RING_UNROLLED_WIDTH;
    unsigned long long agree = 0ULL;
    unsigned long long start = rings->size[0];
    unsigned long long previous_start = 0ULL;
    *pairs = 0ULL;
    for (size_t ring = 1u; ring < rings->count; ring += 1u)
    {
        // the sizes are below 2^15. They fit a signed word, and every product below stays under 2^45
        const long long size = (long long)rings->size[ring];
        const long long inner = (long long)rings->size[ring - 1u];
        for (long long place = 0LL; place < size; place += 1LL)
        {
            const long long turned = plane_floor_quotient(((place * width) - (slope * size)) * inner, size * width);
            const long long parent = ((turned % inner) + inner) % inner;
            // the place and its parent are never negative
            agree += (bits[start + (unsigned long long)place] == bits[previous_start + (unsigned long long)parent])
                         ? 1ULL
                         : 0ULL;
            *pairs += 1ULL;
        }
        previous_start = start;
        start += rings->size[ring];
    }
    return agree;
}

// the spirals' excess over every slope, sum over the slopes of (2 agree - pairs)^2, and the best slope
static unsigned long long plane_ring_spirals(const PlaneRings *rings, const unsigned char *bits, long long *best_slope,
                                             unsigned long long *best_agree)
{
    unsigned long long excess = 0ULL;
    *best_slope = 0LL;
    *best_agree = 0ULL;
    for (long long slope = -PLANE_RING_SLOPES; slope <= PLANE_RING_SLOPES; slope += 1LL)
    {
        unsigned long long pairs = 0ULL;
        const unsigned long long agree = plane_ring_slope(rings, bits, slope, &pairs);
        // both counts are below 2^16. They fit a signed word
        const long long lean = (2LL * (long long)agree) - (long long)pairs;
        // a square is never negative
        excess += (unsigned long long)(lean * lean);
        if (agree > *best_agree)
        {
            *best_agree = agree;
            *best_slope = slope;
        }
    }
    return excess;
}

// one block of the rings' bits, `count` of them, read for spirals against keyed shuffles of the same block
void plane_ring_spiral_read(SimResults *results, const PlaneRings *rings, const unsigned char *block, size_t count,
                            unsigned int draws, const char *label)
{
    VerbaLinea *const line = &results->line;
    long long slope = 0LL;
    unsigned long long agree = 0ULL;
    const unsigned long long excess = plane_ring_spirals(rings, block, &slope, &agree);
    unsigned long long *const excess_band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
    unsigned long long *const agree_band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
    unsigned char *const drawn = (unsigned char *)plane_taken(count, 1u, 1);
    for (unsigned int draw = 0u; draw < draws; draw += 1u)
    {
        plane_shuffle(block, count, draw, drawn);
        long long drawn_slope = 0LL;
        unsigned long long drawn_agree = 0ULL;
        excess_band[draw] = plane_ring_spirals(rings, drawn, &drawn_slope, &drawn_agree);
        agree_band[draw] = drawn_agree;
    }
    unsigned long long pairs = 0ULL;
    const unsigned long long radial = plane_ring_slope(rings, block, 0LL, &pairs);
    mmgr_line_put(line, "  ");
    mmgr_line_put(line, label);
    mmgr_line_put(line, ": the radial agreement ");
    mmgr_line_decimal(line, radial, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, pairs, 1u);
    mmgr_line_put(line, "; the best spiral, slope ");
    mmgr_line_decimal_signed(line, slope);
    mmgr_line_put(line, ", agrees on ");
    mmgr_line_decimal(line, agree, 1u);
    plane_band_print(line, agree_band, draws, agree);
    mmgr_line_put(line, "  ");
    mmgr_line_put(line, label);
    mmgr_line_put(line, ": the spirals' excess over every slope from -");
    mmgr_line_decimal(line, PLANE_RING_SLOPES, 1u);
    mmgr_line_put(line, " to ");
    mmgr_line_decimal(line, PLANE_RING_SLOPES, 1u);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, excess, 1u);
    plane_band_print(line, excess_band, draws, excess);
    sim_flush(results);
    plane_given(drawn);
    plane_given(agree_band);
    plane_given(excess_band);
}
