// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_arith.cu: exact arithmetic, shapes and orders
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

static int s_plane_error = 0;

static void plane_took(OriorExactStatus status)
{
    if (status != ORIOR_EXACT_OK)
    {
        s_plane_error = 1;
    }
}

static OriorExactInteger plane_exact_unsigned(unsigned long long number)
{
    OriorExactInteger value;
    sim_exact_unsigned(&value, number);
    return value;
}

static OriorExactInteger plane_power_two(unsigned int bits)
{
    OriorExactInteger value;
    orior_exact_zero(&value);
    value.limb[bits / 32u] = 1u << (bits % 32u);
    value.sign = 1;
    return value;
}

static OriorExactInteger plane_sum(const OriorExactInteger &left, const OriorExactInteger &right)
{
    OriorExactInteger value;
    orior_exact_zero(&value);
    plane_took(orior_exact_add(&left, &right, &value));
    return value;
}

static OriorExactInteger plane_difference(const OriorExactInteger &left, const OriorExactInteger &right)
{
    OriorExactInteger value;
    orior_exact_zero(&value);
    plane_took(orior_exact_subtract(&left, &right, &value));
    return value;
}

static OriorExactInteger plane_product(const OriorExactInteger &left, const OriorExactInteger &right)
{
    OriorExactInteger value;
    orior_exact_zero(&value);
    plane_took(orior_exact_multiply(&left, &right, &value));
    return value;
}

// the floor of a quotient of two non-negative integers
static OriorExactInteger plane_quotient(const OriorExactInteger &numerator, const OriorExactInteger &divisor)
{
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    orior_exact_zero(&quotient);
    orior_exact_zero(&remainder);
    plane_took(orior_exact_divide(&numerator, &divisor, &quotient, &remainder));
    return quotient;
}

// 2^bits . arctan(1 / x) within terms + 1: the series with signs in turn, each term floored
static OriorExactInteger plane_arctan(unsigned long long x, unsigned int bits, unsigned long long *terms)
{
    const OriorExactInteger square = plane_exact_unsigned(x * x);
    OriorExactInteger power = plane_quotient(plane_power_two(bits), plane_exact_unsigned(x));
    OriorExactInteger total = plane_exact_unsigned(0ULL);
    unsigned long long index = 0ULL;
    *terms = 0ULL;
    while (power.sign != 0)
    {
        const OriorExactInteger term = plane_quotient(power, plane_exact_unsigned((2ULL * index) + 1ULL));
        total = ((index & 1ULL) == 0ULL) ? plane_sum(total, term) : plane_difference(total, term);
        *terms += 1ULL;
        power = plane_quotient(power, square);
        index += 1ULL;
    }
    return total;
}

// the bits of pi - 3 after the point, most significant first, into the `count` bytes at `bits`, where Machin's
// bracket agrees on all of them
int plane_bits(SimResults *results, unsigned int count, unsigned char *bits)
{
    const unsigned int precision = count + PLANE_GUARD;
    unsigned long long fifth_terms = 0ULL;
    unsigned long long far_terms = 0ULL;
    const OriorExactInteger fifth = plane_arctan(5ULL, precision, &fifth_terms);
    const OriorExactInteger far = plane_arctan(239ULL, precision, &far_terms);
    const OriorExactInteger middle = plane_difference(plane_product(fifth, plane_exact_unsigned(16ULL)),
                                                      plane_product(far, plane_exact_unsigned(4ULL)));
    const OriorExactInteger spread = plane_exact_unsigned((16ULL * (fifth_terms + 1ULL)) + (4ULL * (far_terms + 1ULL)));
    const OriorExactInteger guard = plane_power_two(PLANE_GUARD);
    const OriorExactInteger below = plane_quotient(plane_difference(middle, spread), guard);
    const OriorExactInteger above = plane_quotient(plane_sum(middle, spread), guard);
    const int agree = (s_plane_error == 0) && (orior_exact_compare(&below, &above) == 0);
    const OriorExactInteger turn =
        plane_difference(below, plane_product(plane_exact_unsigned(3ULL), plane_power_two(count)));
    for (unsigned int place = 1u; place <= count; place += 1u)
    {
        const unsigned int bit = count - place;
        // one bit of a 32-bit limb, taken from the bottom after the shift
        bits[place - 1u] = (unsigned char)((turn.limb[bit / 32u] >> (bit % 32u)) & 1u);
    }
    unsigned long long leading = 0ULL;
    for (unsigned int place = 0u; place < 64u; place += 1u)
    {
        leading = (leading << 1u) | (unsigned long long)bits[place];
    }
    unsigned long long ones = 0ULL;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        ones += (unsigned long long)bits[place];
    }
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  pi by Machin's formula at ");
    mmgr_line_decimal(line, precision, 1u);
    mmgr_line_put(line, " bits, ");
    mmgr_line_decimal(line, fifth_terms + far_terms, 1u);
    mmgr_line_put(line, " terms: ");
    mmgr_line_decimal(line, count, 1u);
    mmgr_line_put(line, " bits after the point, ");
    mmgr_line_decimal(line, ones, 1u);
    mmgr_line_put(line, " of them ones\n");
    sim_check(results, agree, "floor((pi - 3) 2^N) is one integer at both ends of Machin's bracket");
    sim_check(results, leading == PLANE_PUBLISHED_BITS, "pi's first 64 bits after the point are 0x243F6A8885A308D3");
    return agree && (leading == PLANE_PUBLISHED_BITS);
}

// the run ended with `why`, as a table passes its bound or the pool gives no cell
void plane_refused(const char *why)
{
    static const char s_head[] = "pi_plane: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// A cell of `count` elements of `size` bytes, on the temporary tier where `ephemeral` is 1 and on the persistent tier
// where it is 0. The run ends where the pool gives no cell [RETURNS OWNERSHIP]
void *plane_taken(unsigned long long count, unsigned long long size, int ephemeral)
{
    void *const cell = platform_cell_taken(count * size, (unsigned int)platform_place_g_program_owner, ephemeral);
    if ((cell == NULL) && ((count * size) != 0ULL))
    {
        plane_refused("the pool gave no cell");
    }
    return cell;
}

// a cell that plane_taken gave, given back [TAKES OWNERSHIP]
void plane_given(void *cell)
{
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
}

// the cells of a shape given back [TAKES OWNERSHIP]
void plane_extent_given(PlaneExtent *extent)
{
    plane_given(extent->cell);
    extent->cell = NULL;
    extent->cell_count = 0u;
}

// the name of a shape, terminated, which the run ends on where it passes the room
void plane_name_set(PlaneExtent *extent, const char *name)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (length >= PLANE_NAME_BYTES)
    {
        plane_refused("the name of a shape passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = extent->name, .src = name, .bytes = length + 1u);
}

// the shape's pixels from the place of each of `count` bits; 0 where two bits land on one pixel
int plane_extent_place(PlaneExtent *extent, const char *name, const long long *rows, const long long *columns,
                       size_t count)
{
    plane_name_set(extent, name);
    long long top = rows[0];
    long long bottom = rows[0];
    long long left = columns[0];
    long long right = columns[0];
    for (size_t bit = 1u; bit < count; bit += 1u)
    {
        top = (rows[bit] < top) ? rows[bit] : top;
        bottom = (rows[bit] > bottom) ? rows[bit] : bottom;
        left = (columns[bit] < left) ? columns[bit] : left;
        right = (columns[bit] > right) ? columns[bit] : right;
    }
    // both spans are at least one and below the bit count. They widen to unsigned exactly
    extent->height = (unsigned long long)(bottom - top + 1LL);
    extent->width = (unsigned long long)(right - left + 1LL);
    extent->cell_count = (size_t)(extent->height * extent->width);
    extent->cell = (long long *)plane_taken(extent->cell_count, sizeof(long long), 0);
    // every byte 0xFF makes every cell -1
    EMBED_CALL(memor.set, MemoriaCfg, .dst = extent->cell, .bytes = extent->cell_count * sizeof(long long),
               .val = 0xFFu);
    for (size_t bit = 0u; bit < count; bit += 1u)
    {
        // the offsets from the top left corner are never negative
        const size_t index =
            (size_t)((unsigned long long)(rows[bit] - top) * extent->width + (unsigned long long)(columns[bit] - left));
        if (extent->cell[index] >= 0LL)
        {
            return 0;
        }
        // a bit's index is below 2^15. It fits a signed word
        extent->cell[index] = (long long)bit;
    }
    return 1;
}

int plane_rows(PlaneExtent *extent, unsigned int count, unsigned long long width)
{
    long long *const rows = (long long *)plane_taken(count, sizeof(long long), 1);
    long long *const columns = (long long *)plane_taken(count, sizeof(long long), 1);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        // the row and column are below the bit count. They fit a signed word
        rows[bit] = (long long)(bit / width);
        columns[bit] = (long long)(bit % width);
    }
    char name[PLANE_NAME_BYTES];
    VerbaLinea line = mmgr_line_over(name, sizeof(name));
    mmgr_line_put(&line, "rows_");
    mmgr_line_decimal(&line, width, 1u);
    (void)mmgr_line_finish(&line);
    const int placed = plane_extent_place(extent, name, rows, columns, count);
    plane_given(columns);
    plane_given(rows);
    return placed;
}

// row k stores k + 1 bits, left aligned
int plane_tower(PlaneExtent *extent, unsigned int count)
{
    long long *const rows = (long long *)plane_taken(count, sizeof(long long), 1);
    long long *const columns = (long long *)plane_taken(count, sizeof(long long), 1);
    long long row = 0LL;
    long long start = 0LL;
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        if ((long long)bit >= start + row + 1LL)
        {
            start += row + 1LL;
            row += 1LL;
        }
        rows[bit] = row;
        // the bit's index is below 2^15. It fits a signed word
        columns[bit] = (long long)bit - start;
    }
    const int placed = plane_extent_place(extent, "tower", rows, columns, count);
    plane_given(columns);
    plane_given(rows);
    return placed;
}

int plane_half(const PlanePoint &point)
{
    return (point.y < 0LL) || ((point.y == 0LL) && (point.x < 0LL));
}

// By the square of the radius, then counterclockwise from the positive x axis, in integers. Two points tie only where
// they are one point
int plane_ring_order(const void *left_point, const void *right_point)
{
    const PlanePoint &left = *(const PlanePoint *)left_point;
    const PlanePoint &right = *(const PlanePoint *)right_point;
    const long long left_radius = (left.x * left.x) + (left.y * left.y);
    const long long right_radius = (right.x * right.x) + (right.y * right.y);
    if (left_radius != right_radius)
    {
        return (left_radius < right_radius) ? -1 : 1;
    }
    const int left_half = plane_half(left);
    const int right_half = plane_half(right);
    if (left_half != right_half)
    {
        return (left_half < right_half) ? -1 : 1;
    }
    const long long cross = (left.x * right.y) - (left.y * right.x);
    return (cross > 0LL) ? -1 : ((cross < 0LL) ? 1 : 0);
}

// the rows from the top, each from the left
int plane_raster_order(const void *left_point, const void *right_point)
{
    const PlanePoint &left = *(const PlanePoint *)left_point;
    const PlanePoint &right = *(const PlanePoint *)right_point;
    if (left.y != right.y)
    {
        return (left.y > right.y) ? -1 : 1;
    }
    return (left.x < right.x) ? -1 : ((left.x > right.x) ? 1 : 0);
}
