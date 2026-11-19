// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_shapes.cu: discs, spirals, shells and spheres
#include "pi_plane_internal.h"

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
    std::vector<PlanePoint> points;
    for (long long y = -radius; y <= radius; y += 1LL)
    {
        for (long long x = -radius; x <= radius; x += 1LL)
        {
            if (((x * x) + (y * y)) <= (radius * radius))
            {
                points.push_back(PlanePoint{x, y});
            }
        }
    }
    std::sort(points.begin(), points.end(), plane_ring_before);
    points.resize(count);
    std::vector<long long> rows(count);
    std::vector<long long> columns(count);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        rows[bit] = -points[bit].y;
        columns[bit] = points[bit].x;
    }
    int ok = plane_extent_place(by_rings, "disc_rings", rows, columns);
    std::sort(points.begin(), points.end(), plane_raster_before);
    for (unsigned int bit = 0u; bit < count; bit += 1u)
    {
        rows[bit] = -points[bit].y;
        columns[bit] = points[bit].x;
    }
    ok = ok && plane_extent_place(by_rows, "disc_rows", rows, columns);
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
    std::vector<long long> rows;
    std::vector<long long> columns;
    long long row = side / 2LL;
    long long column = (side - 1LL) / 2LL;
    rows.push_back(row);
    columns.push_back(column);
    long long run = 1LL;
    unsigned int turn = 0u;
    while (rows.size() < count)
    {
        for (long long walked = 0LL; (walked < run) && (rows.size() < count); walked += 1LL)
        {
            row += step_row[turn % 4u];
            column += step_column[turn % 4u];
            if ((row >= 0LL) && (row < side) && (column >= 0LL) && (column < side))
            {
                rows.push_back(row);
                columns.push_back(column);
            }
        }
        turn += 1u;
        run += ((turn % 2u) == 0u) ? 1LL : 0LL;
    }
    return plane_extent_place(extent, "spiral", rows, columns);
}

// bit n at the Gaussian integer sum over n's set bits k of (-1 + i)^k, drawn with the imaginary axis up
int plane_twindragon(PlaneExtent *extent, unsigned int count)
{
    std::vector<long long> rows(count);
    std::vector<long long> columns(count);
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
    return plane_extent_place(extent, "twindragon", rows, columns);
}

// the integer points whose distance from the center rounds to radius: R^2 - R + 1 <= x^2 + y^2 + z^2 <= R^2 + R
static std::vector<PlaneSpherePoint> plane_shell_points(long long radius)
{
    std::vector<PlaneSpherePoint> points;
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
                    points.push_back(PlaneSpherePoint{x, y, z});
                }
            }
        }
    }
    return points;
}

// the widest shell that contains no more points than the bits
std::vector<PlaneSpherePoint> plane_shell(unsigned int count, long long *radius)
{
    std::vector<PlaneSpherePoint> retained;
    *radius = 0LL;
    for (long long trial = 1LL;; trial += 1LL)
    {
        std::vector<PlaneSpherePoint> points = plane_shell_points(trial);
        if (points.size() > count)
        {
            return retained;
        }
        retained.swap(points);
        *radius = trial;
    }
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

// latitude circles from the north pole down, each by longitude
bool plane_latitude_before(const PlaneSpherePoint &left, const PlaneSpherePoint &right)
{
    if (left.z != right.z)
    {
        return left.z > right.z;
    }
    const int order = plane_longitude_order(left, right);
    if (order != 0)
    {
        return order < 0;
    }
    return plane_axis_square(left) < plane_axis_square(right);
}

// meridians by longitude, each from the north pole down
bool plane_meridian_before(const PlaneSpherePoint &left, const PlaneSpherePoint &right)
{
    const int order = plane_longitude_order(left, right);
    if (order != 0)
    {
        return order < 0;
    }
    if (left.z != right.z)
    {
        return left.z > right.z;
    }
    return plane_axis_square(left) < plane_axis_square(right);
}

// the sphere seen from +z, -z, +x and -x, side by side, each pixel the bit of the point nearest the eye
void plane_sphere_views(PlaneExtent *extent, const std::string &name, const std::vector<PlaneSpherePoint> &points,
                        long long radius)
{
    const long long side = (2LL * radius) + 3LL;
    extent->name = name;
    // both extents are small and positive
    extent->height = (unsigned long long)side;
    extent->width = (unsigned long long)((4LL * side) + (3LL * PLANE_SPHERE_GAP));
    extent->cell.assign((size_t)(extent->height * extent->width), -1LL);
    std::vector<long long> nearest(extent->cell.size(), 0LL);
    for (size_t bit = 0u; bit < points.size(); bit += 1u)
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
}

// the ones on each circle of latitude, and their spread over the circles, sum over them of (2 ones - size)^2
unsigned long long plane_latitude_spread(const std::vector<PlaneSpherePoint> &points, long long radius,
                                         const unsigned char *bits)
{
    const size_t levels = (size_t)((2LL * radius) + 3LL);
    std::vector<long long> ones(levels, 0LL);
    std::vector<long long> sizes(levels, 0LL);
    for (size_t bit = 0u; bit < points.size(); bit += 1u)
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
std::vector<PlaneStep> plane_steps(void)
{
    std::vector<PlaneStep> steps;
    for (long long row = 0LL; row <= PLANE_STEP_MAX; row += 1LL)
    {
        for (long long column = -PLANE_STEP_MAX; column <= PLANE_STEP_MAX; column += 1LL)
        {
            const int forward = (row > 0LL) || (column > 0LL);
            const long long magnitude = (column < 0LL) ? -column : column;
            if (forward && (plane_gcd(row, magnitude) == 1LL))
            {
                steps.push_back(PlaneStep{row, column});
            }
        }
    }
    return steps;
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
PlaneLine plane_longest_line(const PlaneExtent &extent, const unsigned char *bits, const std::vector<PlaneStep> &steps)
{
    PlaneLine best = {0ULL, {0LL, 0LL}, -1LL};
    // the extents are below 2^16. They fit a signed word
    const long long height = (long long)extent.height;
    const long long width = (long long)extent.width;
    for (const PlaneStep &step : steps)
    {
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
