// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_shapes.cu: discs, spirals, shells and spheres
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

// the points of `count` elements of `size` bytes put in the order of `order`, with room on the temporary tier
static void plane_sorted(void *points, size_t count, size_t size, int (*order)(const void *, const void *))
{
    void *const room = plane_taken(count, size, 1);
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = points, .bytes = size, .scratch = room, .count = count,
                     .order = order);
    plane_given(room);
}

// the count lattice points nearest the center, by radius then angle, filled ring by ring or by horizontal lines
int plane_disc(PlaneExtent *by_rows, PlaneExtent *by_rings, unsigned int count)
{
    long long radius = 1LL;
    unsigned long long inside = 0ULL;
    while (inside < count)
    {
        radius += 1LL;
        inside = 0ULL;
        for (long long y = -radius; y <= radius; y += 1LL)
        {
            for (long long x = -radius; x <= radius; x += 1LL)
            {
                inside += (((x * x) + (y * y)) <= (radius * radius)) ? 1ULL : 0ULL;
            }
        }
    }
    // the points inside the last radius, counted above, in a cell of the temporary tier at that count
    PlanePoint *const points = (PlanePoint *)plane_taken(inside, sizeof(PlanePoint), 1);
    size_t filled = 0u;
    for (long long y = -radius; y <= radius; y += 1LL)
    {
        for (long long x = -radius; x <= radius; x += 1LL)
        {
            if (((x * x) + (y * y)) <= (radius * radius))
            {
                points[filled].x = x;
                points[filled].y = y;
                filled += 1u;
            }
        }
    }
    plane_sorted(points, filled, sizeof(PlanePoint), plane_ring_order);
    long long *const rows = (long long *)plane_taken(count, sizeof(long long), 1);
    long long *const columns = (long long *)plane_taken(count, sizeof(long long), 1);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        rows[bit] = -points[bit].y;
        columns[bit] = points[bit].x;
    }
    int ok = plane_extent_place(by_rings, "disc_rings", rows, columns, count);
    plane_sorted(points, count, sizeof(PlanePoint), plane_raster_order);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        rows[bit] = -points[bit].y;
        columns[bit] = points[bit].x;
    }
    ok = ok && plane_extent_place(by_rows, "disc_rows", rows, columns, count);
    plane_given(columns);
    plane_given(rows);
    plane_given(points);
    return ok;
}

// the square spiral from the center, right, up, left, down, with runs 1, 1, 2, 2, 3, 3, ...
int plane_spiral(PlaneExtent *extent, unsigned int count)
{
    long long side = 1LL;
    while (side * side < (long long)count)
    {
        side += 1LL;
    }
    const long long step_row[4] = {0LL, -1LL, 0LL, 1LL};
    const long long step_column[4] = {1LL, 0LL, -1LL, 0LL};
    long long *const rows = (long long *)plane_taken(count, sizeof(long long), 1);
    long long *const columns = (long long *)plane_taken(count, sizeof(long long), 1);
    size_t placed = 0u;
    long long row = side / 2LL;
    long long column = (side - 1LL) / 2LL;
    rows[placed] = row;
    columns[placed] = column;
    placed += 1u;
    long long run = 1LL;
    unsigned int turn = 0u;
    while (placed < count)
    {
        for (long long walked = 0LL; (walked < run) && (placed < count); walked += 1LL)
        {
            row += step_row[turn % 4u];
            column += step_column[turn % 4u];
            if ((row >= 0LL) && (row < side) && (column >= 0LL) && (column < side))
            {
                rows[placed] = row;
                columns[placed] = column;
                placed += 1u;
            }
        }
        turn += 1u;
        run += ((turn % 2u) == 0u) ? 1LL : 0LL;
    }
    const int ok = plane_extent_place(extent, "spiral", rows, columns, placed);
    plane_given(columns);
    plane_given(rows);
    return ok;
}

// bit n at the Gaussian integer sum over n's set bits k of (-1 + i)^k, drawn with the imaginary axis up
int plane_twindragon(PlaneExtent *extent, unsigned int count)
{
    long long *const rows = (long long *)plane_taken(count, sizeof(long long), 1);
    long long *const columns = (long long *)plane_taken(count, sizeof(long long), 1);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        long long real = 0LL;
        long long imaginary = 0LL;
        long long power_real = 1LL;
        long long power_imaginary = 0LL;
        for (unsigned int digit = bit; digit != 0u; digit >>= 1u)
        {
            if ((digit & 1u) != 0u)
            {
                real += power_real;
                imaginary += power_imaginary;
            }
            // (a + bi)(-1 + i) = (-a - b) + (a - b)i
            const long long next_real = -power_real - power_imaginary;
            power_imaginary = power_real - power_imaginary;
            power_real = next_real;
        }
        rows[bit] = -imaginary;
        columns[bit] = real;
    }
    const int ok = plane_extent_place(extent, "twindragon", rows, columns, count);
    plane_given(columns);
    plane_given(rows);
    return ok;
}

// The integer points whose distance from the center rounds to radius: R^2 - R + 1 <= x^2 + y^2 + z^2 <= R^2 + R.
// Where `points` is NULL the call only counts them. The answer is their count
static size_t plane_shell_points(long long radius, PlaneSpherePoint *points)
{
    size_t count = 0u;
    const long long least = (radius * radius) - radius + 1LL;
    const long long max = (radius * radius) + radius;
    for (long long z = -radius - 1LL; z <= radius + 1LL; z += 1LL)
    {
        for (long long y = -radius - 1LL; y <= radius + 1LL; y += 1LL)
        {
            for (long long x = -radius - 1LL; x <= radius + 1LL; x += 1LL)
            {
                const long long square = (x * x) + (y * y) + (z * z);
                if ((square >= least) && (square <= max))
                {
                    if (points != NULL)
                    {
                        points[count].x = x;
                        points[count].y = y;
                        points[count].z = z;
                    }
                    count += 1u;
                }
            }
        }
    }
    return count;
}

// The widest shell that contains no more points than the bits. Its points go in one cell of the persistent tier at
// their count, which `points` names, as the sim reads the sphere to its end. The answer is the count [RETURNS
// OWNERSHIP]
size_t plane_shell(unsigned int count, long long *radius, PlaneSpherePoint **points)
{
    *radius = 0LL;
    for (long long trial = 1LL; plane_shell_points(trial, NULL) <= count; trial += 1LL)
    {
        *radius = trial;
    }
    const size_t retained = (*radius == 0LL) ? 0u : plane_shell_points(*radius, NULL);
    *points = (PlaneSpherePoint *)plane_taken(retained, sizeof(PlaneSpherePoint), 0);
    if (retained != 0u)
    {
        (void)plane_shell_points(*radius, *points);
    }
    return retained;
}

// by longitude, counterclockwise from the positive x axis, in integers; the poles' points, on the axis, first
static int plane_longitude_order(const PlaneSpherePoint &left, const PlaneSpherePoint &right)
{
    const int left_pole = (left.x == 0LL) && (left.y == 0LL);
    const int right_pole = (right.x == 0LL) && (right.y == 0LL);
    if ((left_pole != 0) || (right_pole != 0))
    {
        return right_pole - left_pole;
    }
    const PlanePoint left_flat = {left.x, left.y};
    const PlanePoint right_flat = {right.x, right.y};
    const int left_half = plane_half(left_flat);
    const int right_half = plane_half(right_flat);
    if (left_half != right_half)
    {
        return (left_half < right_half) ? -1 : 1;
    }
    const long long cross = (left.x * right.y) - (left.y * right.x);
    return (cross > 0LL) ? -1 : ((cross < 0LL) ? 1 : 0);
}

static long long plane_axis_square(const PlaneSpherePoint &point)
{
    return (point.x * point.x) + (point.y * point.y);
}

// -1, 0 or 1 as the left square from the axis is below, equal to or above the right
static int plane_axis_order(const PlaneSpherePoint &left, const PlaneSpherePoint &right)
{
    const long long left_square = plane_axis_square(left);
    const long long right_square = plane_axis_square(right);
    return (left_square < right_square) ? -1 : ((left_square > right_square) ? 1 : 0);
}

// Latitude circles from the north pole down, each by longitude. Two points tie only where they are one point
int plane_latitude_order(const void *left_point, const void *right_point)
{
    const PlaneSpherePoint &left = *(const PlaneSpherePoint *)left_point;
    const PlaneSpherePoint &right = *(const PlaneSpherePoint *)right_point;
    if (left.z != right.z)
    {
        return (left.z > right.z) ? -1 : 1;
    }
    const int order = plane_longitude_order(left, right);
    return (order != 0) ? order : plane_axis_order(left, right);
}

// Meridians by longitude, each from the north pole down. Two points tie only where they are one point
int plane_meridian_order(const void *left_point, const void *right_point)
{
    const PlaneSpherePoint &left = *(const PlaneSpherePoint *)left_point;
    const PlaneSpherePoint &right = *(const PlaneSpherePoint *)right_point;
    const int order = plane_longitude_order(left, right);
    if (order != 0)
    {
        return order;
    }
    if (left.z != right.z)
    {
        return (left.z > right.z) ? -1 : 1;
    }
    return plane_axis_order(left, right);
}

// the sphere seen from +z, -z, +x and -x, side by side, each pixel the bit of the point nearest the eye
void plane_sphere_views(PlaneExtent *extent, const char *name, const PlaneSpherePoint *points, size_t count,
                        long long radius)
{
    const long long side = (2LL * radius) + 3LL;
    plane_name_set(extent, name);
    // both extents are small and positive
    extent->height = (unsigned long long)side;
    extent->width = (unsigned long long)((4LL * side) + (3LL * PLANE_SPHERE_GAP));
    extent->cell_count = (size_t)(extent->height * extent->width);
    extent->cell = (long long *)plane_taken(extent->cell_count, sizeof(long long), 0);
    // every byte 0xFF makes every cell -1
    EMBED_CALL(memor.set, MemoriaCfg, .dst = extent->cell, .bytes = extent->cell_count * sizeof(long long),
               .val = 0xFFu);
    long long *const nearest = (long long *)plane_taken(extent->cell_count, sizeof(long long), 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = nearest, .bytes = extent->cell_count * sizeof(long long), .val = 0u);
    for (size_t bit = 0u; bit < count; bit += 1u)
    {
        const PlaneSpherePoint &point = points[bit];
        // each view: its row, its column, and how near the eye the point is
        const long long row[4] = {-point.y, -point.y, -point.z, -point.z};
        const long long column[4] = {point.x, -point.x, -point.y, point.y};
        const long long toward[4] = {point.z, -point.z, point.x, -point.x};
        for (long long view = 0LL; view < 4LL; view += 1LL)
        {
            const long long at_row = row[view] + radius + 1LL;
            const long long at_column = column[view] + radius + 1LL + (view * (side + PLANE_SPHERE_GAP));
            // the offsets lie inside the picture. The index is never negative
            const size_t index = (size_t)((at_row * (long long)extent->width) + at_column);
            if ((extent->cell[index] < 0LL) || (toward[view] > nearest[index]))
            {
                // a bit's index is below 2^15. It fits a signed word
                extent->cell[index] = (long long)bit;
                nearest[index] = toward[view];
            }
        }
    }
    plane_given(nearest);
}

// the ones on each circle of latitude, and their spread over the circles, sum over them of (2 ones - size)^2
unsigned long long plane_latitude_spread(const PlaneSpherePoint *points, size_t count, long long radius,
                                         const unsigned char *bits)
{
    const size_t levels = (size_t)((2LL * radius) + 3LL);
    long long *const ones = (long long *)plane_taken(levels, sizeof(long long), 1);
    long long *const sizes = (long long *)plane_taken(levels, sizeof(long long), 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = ones, .bytes = levels * sizeof(long long), .val = 0u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = sizes, .bytes = levels * sizeof(long long), .val = 0u);
    for (size_t bit = 0u; bit < count; bit += 1u)
    {
        // the height lies within the radius plus one. The offset is never negative
        const size_t level = (size_t)(points[bit].z + radius + 1LL);
        ones[level] += (long long)bits[bit];
        sizes[level] += 1LL;
    }
    unsigned long long spread = 0ULL;
    for (size_t level = 0u; level < levels; level += 1u)
    {
        const long long excess = (2LL * ones[level]) - sizes[level];
        // a square is never negative
        spread += (unsigned long long)(excess * excess);
    }
    plane_given(sizes);
    plane_given(ones);
    return spread;
}

static long long plane_gcd(long long left, long long right)
{
    while (right != 0LL)
    {
        const long long rest = left % right;
        left = right;
        right = rest;
    }
    return left;
}

// the steps (dy, dx) with coprime parts of at max PLANE_STEP_MAX, one of each pair of opposites
void plane_steps(PlaneSteps *steps)
{
    steps->count = 0u;
    for (long long row = 0LL; row <= PLANE_STEP_MAX; row += 1LL)
    {
        for (long long column = -PLANE_STEP_MAX; column <= PLANE_STEP_MAX; column += 1LL)
        {
            const int forward = (row > 0LL) || (column > 0LL);
            const long long magnitude = (column < 0LL) ? -column : column;
            if (forward && (plane_gcd(row, magnitude) == 1LL))
            {
                if (steps->count == PLANE_STEPS_MAX)
                {
                    plane_refused("the steps of a line pass their table");
                }
                steps->step[steps->count].row = row;
                steps->step[steps->count].column = column;
                steps->count += 1u;
            }
        }
    }
}

long long plane_cell_at(const PlaneExtent &extent, long long row, long long column)
{
    // the extents are below 2^16. They fit a signed word
    const long long height = (long long)extent.height;
    const long long width = (long long)extent.width;
    if ((row < 0LL) || (row >= height) || (column < 0LL) || (column >= width))
    {
        return -1LL;
    }
    // the row and column are inside the shape. The index is never negative
    return extent.cell[(size_t)((row * width) + column)];
}

// the longest straight run of equal bits on the shape, along any of the steps
PlaneLine plane_longest_line(const PlaneExtent &extent, const unsigned char *bits, const PlaneSteps *steps)
{
    PlaneLine best = {0ULL, {0LL, 0LL}, -1LL};
    // the extents are below 2^16. They fit a signed word
    const long long height = (long long)extent.height;
    const long long width = (long long)extent.width;
    for (size_t at = 0u; at < steps->count; at += 1u)
    {
        const PlaneStep &step = steps->step[at];
        for (long long row = 0LL; row < height; row += 1LL)
        {
            for (long long column = 0LL; column < width; column += 1LL)
            {
                const long long here = extent.cell[(size_t)((row * width) + column)];
                if (here < 0LL)
                {
                    continue;
                }
                const long long before = plane_cell_at(extent, row - step.row, column - step.column);
                if ((before >= 0LL) && (bits[before] == bits[here]))
                {
                    continue;
                }
                unsigned long long length = 1ULL;
                long long next = plane_cell_at(extent, row + step.row, column + step.column);
                long long walked = 1LL;
                while ((next >= 0LL) && (bits[next] == bits[here]))
                {
                    length += 1ULL;
                    walked += 1LL;
                    next = plane_cell_at(extent, row + (walked * step.row), column + (walked * step.column));
                }
                if (length > best.length)
                {
                    best.length = length;
                    best.step = step;
                    best.start = here;
                }
            }
        }
    }
    return best;
}
