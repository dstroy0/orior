// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_main.cu: main
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

static const unsigned int s_plane_fibonacci[PLANE_FIBONACCI] = {
    1u, 2u, 3u, 5u, 8u, 13u, 21u, 34u, 55u, 89u, 144u, 233u, 377u, 610u, 987u, 1597u, 2584u, 4181u, 6765u, 10946u};

static const unsigned long long s_plane_widths[PLANE_WIDTHS] = {7ULL,   16ULL,  32ULL,  64ULL,
                                                                106ULL, 113ULL, 128ULL, 256ULL};

// The pool. The largest picture of the rings is 4105 pixels a side at 13 rings. It takes two counts of eight bytes
// and one shade a pixel, about 300 MB, and also the file that holds it
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

// the shapes: the rows of each width, the tower, the disc by rows and by rings, the spiral and the twindragon
#define PLANE_EXTENTS (PLANE_WIDTHS + 5u)

// The bands of the draws, one for each reading, each `draws` long. They are the lag, the line of each shape, the
// spreads and the depth. Then the two spheres' lines and spreads of latitude, and the Fibonacci differences with their
// max
#define PLANE_BAND_LAG 0u
#define PLANE_BAND_LINES (PLANE_BAND_LAG + 1u)
#define PLANE_BAND_SPREADS (PLANE_BAND_LINES + PLANE_EXTENTS)
#define PLANE_BAND_DEPTH (PLANE_BAND_SPREADS + (2u * PLANE_SCALES))
#define PLANE_BAND_SPHERE_LINES (PLANE_BAND_DEPTH + 1u)
#define PLANE_BAND_SPHERE_LATITUDES (PLANE_BAND_SPHERE_LINES + 2u)
#define PLANE_BAND_FIBONACCI (PLANE_BAND_SPHERE_LATITUDES + 2u)
#define PLANE_BANDS (PLANE_BAND_FIBONACCI + PLANE_FIBONACCI + 1u)

// a difference and the longest run at it, which the ranks of the differences sort by
typedef struct
{
    unsigned int longest;
    unsigned int lag;
} PlaneLagRank;

// the longest run first, and the order of the differences where two are equal
static int plane_rank_order(const void *left, const void *right)
{
    const unsigned int one = ((const PlaneLagRank *)left)->longest;
    const unsigned int other = ((const PlaneLagRank *)right)->longest;
    return (one > other) ? -1 : ((one < other) ? 1 : 0);
}

static PlaneExtent s_extents[PLANE_EXTENTS];
static PlaneExtent s_spheres[2];
static PlaneLine s_found[PLANE_EXTENTS];
static PlaneSteps s_steps;

int program_main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    const unsigned int bits_count = plane_number(count, arguments, "--bits", PLANE_BITS);
    const unsigned int draws = plane_number(count, arguments, "--draws", PLANE_DRAWS);
    const char *const directory = plane_directory(count, arguments);
    const unsigned long long needed = (unsigned long long)bits_count + PLANE_GUARD + 64ULL;
    if ((bits_count < 64u) || (bits_count > PLANE_BITS_MAX) || (needed > ORIOR_EXACT_BITS) || (draws == 0u))
    {
        unsigned long long limbs = 1ULL;
        while (limbs * 32ULL < needed)
        {
            limbs *= 2ULL;
        }
        mmgr_line_put(line, "  errored: the draws are at least 1, the bits are 64 to ");
        mmgr_line_decimal(line, PLANE_BITS_MAX, 1u);
        mmgr_line_put(line, ", and the bracket needs an exact width of ");
        mmgr_line_decimal(line, needed, 1u);
        mmgr_line_put(line, " bits: run with SIM_EXACT_LIMBS=");
        mmgr_line_decimal(line, limbs, 1u);
        mmgr_line_ch(line, '\n');
        sim_check(&results, 0, "the request fits the exact width");
        return sim_close(&results, "pi plane");
    }

    // pi's bits, the bits of a draw, and the longest runs. Each is one cell of the persistent tier at the bits, as the
    // run reads them to its end
    unsigned char *const bits = (unsigned char *)plane_taken(bits_count, 1u, 0);
    unsigned char *const drawn = (unsigned char *)plane_taken(bits_count, 1u, 0);
    unsigned int *const longest = (unsigned int *)plane_taken(bits_count, sizeof(unsigned int), 0);
    unsigned int *const host_longest = (unsigned int *)plane_taken(bits_count, sizeof(unsigned int), 0);
    unsigned int *const drawn_longest = (unsigned int *)plane_taken(bits_count, sizeof(unsigned int), 0);
    unsigned long long *const bands =
        (unsigned long long *)plane_taken((unsigned long long)PLANE_BANDS * draws, sizeof(unsigned long long), 0);
    int ok = plane_bits(&results, bits_count, bits);
    sim_flush(&results);

    PlaneExtent *const extents = s_extents;
    int built = ok;
    for (unsigned int width = 0u; built && (width < PLANE_WIDTHS); width += 1u)
    {
        built = plane_rows(&extents[width], bits_count, s_plane_widths[width]);
    }
    built = built && plane_tower(&extents[PLANE_WIDTHS], bits_count);
    built = built && plane_disc(&extents[PLANE_WIDTHS + 1u], &extents[PLANE_WIDTHS + 2u], bits_count);
    built = built && plane_spiral(&extents[PLANE_WIDTHS + 3u], bits_count);
    built = built && plane_twindragon(&extents[PLANE_WIDTHS + 4u], bits_count);
    sim_check(&results, built, "every shape arranges each bit on its own pixel");
    ok = ok && built;

    plane_shuffle(bits, bits_count, 0u, drawn);
    int drew = ok;
    char path[PLATFORM_PATH_BYTES];
    for (size_t extent = 0u; drew && (extent < PLANE_EXTENTS); extent += 1u)
    {
        plane_path(path, directory, "/pi_", extents[extent].name, ".png");
        drew = plane_picture(extents[extent], bits, path);
        plane_path(path, directory, "/shuffled_", extents[extent].name, ".png");
        drew = drew && plane_picture(extents[extent], drawn, path);
    }
    sim_check(&results, drew, "every shape is drawn for pi and for one shuffle of its bits");
    if (drew)
    {
        mmgr_line_put(line, "  the pictures, pi_<shape>.png and shuffled_<shape>.png, are in ");
        mmgr_line_put(line, directory);
        mmgr_line_ch(line, '\n');
    }

    unsigned char *device_bits = NULL;
    unsigned int *device_longest = NULL;
    ok = ok && sim_job_submit(&results, "pi_plane", count, arguments,
                              (unsigned long long)bits_count * (1ULL + sizeof(unsigned int)));
    ok = ok && sim_status_check(&results, cudaMalloc((void **)&device_bits, bits_count), "bits");
    ok = ok &&
         sim_status_check(&results, cudaMalloc((void **)&device_longest, (size_t)bits_count * sizeof(unsigned int)),
                          "the lags' longest runs");
    ok = ok && plane_lag_device(&results, bits, bits_count, device_bits, device_longest, longest);
    if (ok)
    {
        plane_lag_host(bits, bits_count, host_longest);
        longest[0] = 0u;
        sim_check(&results,
                  EMBED_CALL(memor.cmp, MemoriaCfg, .src = longest, .other = host_longest,
                             .bytes = (size_t)bits_count * sizeof(unsigned int)) == 0,
                  "the device's longest run at every difference equals the host's, for pi");
    }

    PlaneSteps *const steps = &s_steps;
    plane_steps(steps);
    PlaneLine *const found = s_found;
    for (size_t extent = 0u; ok && (extent < PLANE_EXTENTS); extent += 1u)
    {
        found[extent] = plane_longest_line(extents[extent], bits, steps);
    }
    if (ok)
    {
        const PlaneExtent &rings = extents[PLANE_WIDTHS + 2u];
        const PlaneLine &radial = found[PLANE_WIDTHS + 2u];
        unsigned char *const first_drawn = (unsigned char *)plane_taken(bits_count, 1u, 1);
        plane_shuffle(bits, bits_count, 0u, first_drawn);
        const PlaneLine drawn_line = plane_longest_line(rings, first_drawn, steps);
        plane_path(path, directory, "/pi_disc_rings_sparse.png", "", "");
        int sparse = plane_sparse_picture(rings, bits, radial, path);
        plane_path(path, directory, "/shuffled_disc_rings_sparse.png", "", "");
        sparse = sparse && plane_sparse_picture(rings, first_drawn, drawn_line, path);
        plane_given(first_drawn);
        sim_check(&results, sparse, "the ring-filled disc is drawn sparse, its longest line in red");
        mmgr_line_put(line, "  the sparse ring: pi's longest line on the ring-filled disc is ");
        plane_line_print(line, radial);
        mmgr_line_put(line, ", every bit of it a ");
        mmgr_line_decimal(line, bits[(size_t)radial.start], 1u);
        mmgr_line_put(line, "; the shuffle's is ");
        plane_line_print(line, drawn_line);
        mmgr_line_ch(line, '\n');
        sim_flush(&results);
    }
    unsigned int pi_lag = 0u;
    const unsigned int pi_lag_length = ok ? plane_lag_max(longest, bits_count, &pi_lag) : 0u;

    // the draws read so far, the length of every band
    size_t band_count = 0u;
    const PlaneExtent &dragon = extents[PLANE_WIDTHS + 4u];
    unsigned int *dragon_depth = NULL;
    if (ok)
    {
        dragon_depth = (unsigned int *)plane_taken(dragon.cell_count, sizeof(unsigned int), 0);
        plane_depth(dragon, dragon_depth);
    }
    unsigned long long depth_ones[PLANE_DEPTHS + 1u];
    unsigned long long depth_sizes[PLANE_DEPTHS + 1u];

    // the sphere: the widest integer shell the bits fill, bit n at the n-th point by latitude circles or by meridians
    long long sphere_radius = 0LL;
    PlaneSpherePoint *sphere_orders[2] = {NULL, NULL};
    PlaneExtent *const spheres = s_spheres;
    size_t sphere_count = 0u;
    if (ok)
    {
        sphere_count = plane_shell(bits_count, &sphere_radius, &sphere_orders[0]);
        sphere_orders[1] = (PlaneSpherePoint *)plane_taken(sphere_count, sizeof(PlaneSpherePoint), 0);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = sphere_orders[1], .src = sphere_orders[0],
                   .bytes = sphere_count * sizeof(PlaneSpherePoint));
        PlaneSpherePoint *const room = (PlaneSpherePoint *)plane_taken(sphere_count, sizeof(PlaneSpherePoint), 1);
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = sphere_orders[0], .bytes = sizeof(PlaneSpherePoint),
                         .scratch = room, .count = sphere_count, .order = plane_latitude_order);
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = sphere_orders[1], .bytes = sizeof(PlaneSpherePoint),
                         .scratch = room, .count = sphere_count, .order = plane_meridian_order);
        plane_given(room);
        plane_sphere_views(&spheres[0], "sphere_latitudes", sphere_orders[0], sphere_count, sphere_radius);
        plane_sphere_views(&spheres[1], "sphere_meridians", sphere_orders[1], sphere_count, sphere_radius);
    }
    // the sphere's bits are pi's first, as many as its points
    const unsigned char *const sphere_bits = bits;
    unsigned char *const sphere_drawn = (unsigned char *)plane_taken(bits_count, 1u, 0);
    if (sphere_count != 0u)
    {
        plane_shuffle(sphere_bits, sphere_count, 0u, sphere_drawn);
    }
    int sphere_drew = ok && (sphere_count > 0u);
    for (size_t fill = 0u; sphere_drew && (fill < 2u); fill += 1u)
    {
        plane_path(path, directory, "/pi_", spheres[fill].name, ".png");
        sphere_drew = plane_picture(spheres[fill], sphere_bits, path);
        plane_path(path, directory, "/shuffled_", spheres[fill].name, ".png");
        sphere_drew = sphere_drew && plane_picture(spheres[fill], sphere_drawn, path);
    }
    sim_check(&results, sphere_drew, "the sphere is arranged and drawn for pi and for one shuffle of its bits");
    ok = ok && sphere_drew;
    PlaneLine sphere_found[2];
    unsigned long long sphere_latitude[2] = {0ULL, 0ULL};
    for (size_t fill = 0u; ok && (fill < 2u); fill += 1u)
    {
        sphere_found[fill] = plane_longest_line(spheres[fill], sphere_bits, steps);
        sphere_latitude[fill] = plane_latitude_spread(sphere_orders[fill], sphere_count, sphere_radius, sphere_bits);
    }
    int shuffled = ok;
    for (unsigned int draw = 0u; shuffled && (draw < draws); draw += 1u)
    {
        plane_shuffle(bits, bits_count, draw, drawn);
        shuffled = plane_lag_device(&results, drawn, bits_count, device_bits, device_longest, drawn_longest);
        unsigned int drawn_lag = 0u;
        bands[(PLANE_BAND_LAG * draws) + band_count] = plane_lag_max(drawn_longest, bits_count, &drawn_lag);
        for (size_t extent = 0u; extent < PLANE_EXTENTS; extent += 1u)
        {
            // a draw that the device failed on reads no line, and its value stays out of the bands
            bands[((PLANE_BAND_LINES + extent) * draws) + band_count] =
                shuffled ? plane_longest_line(extents[extent], drawn, steps).length : 0ULL;
        }
        for (unsigned int type = 0u; type < 2u * PLANE_SCALES; type += 1u)
        {
            // the type's low bit chooses residues or blocks
            bands[((PLANE_BAND_SPREADS + type) * draws) + band_count] =
                plane_spread(drawn, bits_count, (type / 2u) + 1u, (int)(type % 2u));
        }
        bands[(PLANE_BAND_DEPTH * draws) + band_count] =
            plane_depth_spread(dragon, dragon_depth, drawn, depth_ones, depth_sizes);
        plane_shuffle(sphere_bits, sphere_count, draw, sphere_drawn);
        for (size_t fill = 0u; fill < 2u; fill += 1u)
        {
            bands[((PLANE_BAND_SPHERE_LINES + fill) * draws) + band_count] =
                plane_longest_line(spheres[fill], sphere_drawn, steps).length;
            bands[((PLANE_BAND_SPHERE_LATITUDES + fill) * draws) + band_count] =
                plane_latitude_spread(sphere_orders[fill], sphere_count, sphere_radius, sphere_drawn);
        }
        unsigned long long golden_max = 0ULL;
        for (unsigned int fibonacci = 0u; fibonacci < PLANE_FIBONACCI; fibonacci += 1u)
        {
            const unsigned long long run =
                (s_plane_fibonacci[fibonacci] < bits_count) ? drawn_longest[s_plane_fibonacci[fibonacci]] : 0ULL;
            bands[((PLANE_BAND_FIBONACCI + fibonacci) * draws) + band_count] = run;
            golden_max = (run > golden_max) ? run : golden_max;
        }
        bands[((PLANE_BAND_FIBONACCI + PLANE_FIBONACCI) * draws) + band_count] = golden_max;
        band_count += 1u;
    }
    sim_check(&results, shuffled, "every draw is shuffled and read");

    if (ok && shuffled)
    {
        mmgr_line_put(line, "  every width at once, the longest run of equal bits at any difference 1 to ");
        mmgr_line_decimal(line, bits_count - 1u, 1u);
        mmgr_line_put(line, ": ");
        mmgr_line_decimal(line, pi_lag_length, 1u);
        mmgr_line_put(line, " at difference ");
        mmgr_line_decimal(line, pi_lag, 1u);
        plane_band_print(line, &bands[PLANE_BAND_LAG * draws], band_count, pi_lag_length);
        // the differences ranked by their longest run, a stable sort that keeps equal runs in the order of their lag
        PlaneLagRank *const order = (PlaneLagRank *)plane_taken(bits_count - 1u, sizeof(PlaneLagRank), 1);
        PlaneLagRank *const order_room = (PlaneLagRank *)plane_taken(bits_count - 1u, sizeof(PlaneLagRank), 1);
        for (unsigned int lag = 1u; lag < bits_count; lag += 1u)
        {
            order[lag - 1u].longest = longest[lag];
            order[lag - 1u].lag = lag;
        }
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = order, .bytes = sizeof(PlaneLagRank), .scratch = order_room,
                         .count = bits_count - 1u, .order = plane_rank_order);
        mmgr_line_put(line, "  pi's longest runs by difference:");
        for (unsigned int rank = 0u; rank < PLANE_TOP_LAGS; rank += 1u)
        {
            mmgr_line_put(line, "  ");
            mmgr_line_decimal(line, order[rank].longest, 1u);
            mmgr_line_put(line, " at ");
            mmgr_line_decimal(line, order[rank].lag, 1u);
        }
        plane_given(order_room);
        plane_given(order);
        mmgr_line_ch(line, '\n');
        mmgr_line_put(line, "  pi's convergent denominators as differences:");
        const unsigned int denominators[3] = {7u, 106u, 113u};
        for (unsigned int convergent = 0u; convergent < 3u; convergent += 1u)
        {
            mmgr_line_put(line, "  ");
            mmgr_line_decimal(line, longest[denominators[convergent]], 1u);
            mmgr_line_put(line, " at ");
            mmgr_line_decimal(line, denominators[convergent], 1u);
        }
        mmgr_line_ch(line, '\n');
        sim_flush(&results);
        for (size_t extent = 0u; extent < PLANE_EXTENTS; extent += 1u)
        {
            mmgr_line_put(line, "  ");
            mmgr_line_put(line, extents[extent].name);
            mmgr_line_put(line, " (");
            mmgr_line_decimal(line, extents[extent].width, 1u);
            mmgr_line_put(line, " x ");
            mmgr_line_decimal(line, extents[extent].height, 1u);
            mmgr_line_put(line, "): pi's longest line ");
            plane_line_print(line, found[extent]);
            plane_band_print(line, &bands[(PLANE_BAND_LINES + extent) * draws], band_count, found[extent].length);
            sim_flush(&results);
        }
        mmgr_line_put(line,
                      "  the ones' spread, sum over the classes of (2 ones - size)^2; a residue is one place on every");
        mmgr_line_put(line, " arm of the twindragon, a block one whole sub-dragon\n");
        for (unsigned int type = 0u; type < 2u * PLANE_SCALES; type += 1u)
        {
            const unsigned int scale = (type / 2u) + 1u;
            // the type's low bit chooses residues or blocks
            const unsigned long long measurement = plane_spread(bits, bits_count, scale, (int)(type % 2u));
            mmgr_line_put(line, ((type % 2u) == 0u) ? "  residues mod 2^" : "  blocks, 2^");
            mmgr_line_decimal(line, scale, 1u);
            mmgr_line_put(line, ((type % 2u) == 0u) ? ": pi's spread " : " of them: pi's spread ");
            mmgr_line_decimal(line, measurement, 1u);
            plane_band_print(line, &bands[(PLANE_BAND_SPREADS + type) * draws], band_count, measurement);
            sim_flush(&results);
        }
        const unsigned long long depth_measurement =
            plane_depth_spread(dragon, dragon_depth, bits, depth_ones, depth_sizes);
        mmgr_line_put(line, "  the twindragon by depth from its edge, 1 the arm points and the lace to ");
        mmgr_line_decimal(line, PLANE_DEPTHS, 1u);
        mmgr_line_put(line, " and deeper the middle; pi's ones of the pixels at each depth:");
        for (unsigned int level = 1u; level <= PLANE_DEPTHS; level += 1u)
        {
            mmgr_line_put(line, "  ");
            mmgr_line_decimal(line, depth_ones[level], 1u);
            mmgr_line_ch(line, '/');
            mmgr_line_decimal(line, depth_sizes[level], 1u);
        }
        mmgr_line_put(line, "\n  pi's spread over the depths ");
        mmgr_line_decimal(line, depth_measurement, 1u);
        plane_band_print(line, &bands[PLANE_BAND_DEPTH * draws], band_count, depth_measurement);
        sim_flush(&results);

        mmgr_line_put(line, "  the sphere: the integer shell of radius ");
        mmgr_line_decimal_signed(line, sphere_radius);
        mmgr_line_put(line, ", ");
        mmgr_line_decimal(line, sphere_count, 1u);
        mmgr_line_put(line, " points, containing pi's first ");
        mmgr_line_decimal(line, sphere_count, 1u);
        mmgr_line_put(line, " bits, seen from +z, -z, +x and -x\n");
        for (size_t fill = 0u; fill < 2u; fill += 1u)
        {
            mmgr_line_put(line, "  ");
            mmgr_line_put(line, spheres[fill].name);
            mmgr_line_put(line, ": pi's longest line on the views ");
            plane_line_print(line, sphere_found[fill]);
            plane_band_print(line, &bands[(PLANE_BAND_SPHERE_LINES + fill) * draws], band_count,
                             sphere_found[fill].length);
            mmgr_line_put(line, "  ");
            mmgr_line_put(line, spheres[fill].name);
            mmgr_line_put(line, ": pi's spread over the circles of latitude ");
            mmgr_line_decimal(line, sphere_latitude[fill], 1u);
            plane_band_print(line, &bands[(PLANE_BAND_SPHERE_LATITUDES + fill) * draws], band_count,
                             sphere_latitude[fill]);
            sim_flush(&results);
        }
        mmgr_line_put(line, "  the golden sphere's spirals, pi's longest run at each Fibonacci difference:\n");
        unsigned long long golden_max = 0ULL;
        for (unsigned int fibonacci = 0u; fibonacci < PLANE_FIBONACCI; fibonacci += 1u)
        {
            if (s_plane_fibonacci[fibonacci] >= bits_count)
            {
                continue;
            }
            const unsigned long long run = longest[s_plane_fibonacci[fibonacci]];
            golden_max = (run > golden_max) ? run : golden_max;
            mmgr_line_put(line, "    F = ");
            mmgr_line_decimal(line, s_plane_fibonacci[fibonacci], 1u);
            mmgr_line_put(line, ": ");
            mmgr_line_decimal(line, run, 1u);
            plane_band_print(line, &bands[(PLANE_BAND_FIBONACCI + fibonacci) * draws], band_count, run);
        }
        mmgr_line_put(line, "  the longest over every Fibonacci difference ");
        mmgr_line_decimal(line, golden_max, 1u);
        plane_band_print(line, &bands[(PLANE_BAND_FIBONACCI + PLANE_FIBONACCI) * draws], band_count, golden_max);
        sim_flush(&results);
    }

    if (ok)
    {
        plane_rings(&results, bits, bits_count, draws, plane_number(count, arguments, "--blind", 0u),
                    plane_number(count, arguments, "--blind-from", 0u),
                    plane_number(count, arguments, "--balanced", 0u) != 0u,
                    plane_number(count, arguments, "--funnels", 0u), directory);
    }

    // a path drawn over the ring-filled disc: pi's ones under it against the same cells in each shuffle
    const char *const cells_path = plane_text(count, arguments, "--cells");
    if (ok && (cells_path != NULL))
    {
        long long *under = NULL;
        const size_t under_count = plane_path_cells(extents[PLANE_WIDTHS + 2u], cells_path, &under);
        const unsigned long long ones = plane_ones_under(under, under_count, bits);
        unsigned long long *const path_band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
        unsigned long long at_or_below = 0ULL;
        for (unsigned int draw = 0u; draw < draws; draw += 1u)
        {
            plane_shuffle(bits, bits_count, draw, drawn);
            const unsigned long long drawn_ones = plane_ones_under(under, under_count, drawn);
            path_band[draw] = drawn_ones;
            at_or_below += (drawn_ones <= ones) ? 1ULL : 0ULL;
        }
        mmgr_line_put(line, "  the drawn path: ");
        mmgr_line_decimal(line, under_count, 1u);
        mmgr_line_put(line, " cells of the ring-filled disc, pi's ones on them ");
        mmgr_line_decimal(line, ones, 1u);
        mmgr_line_put(line, ", its zeros ");
        mmgr_line_decimal(line, under_count - ones, 1u);
        mmgr_line_put(line, "; ");
        mmgr_line_decimal(line, at_or_below, 1u);
        mmgr_line_put(line, " of the draws at or below pi's ones");
        if (under_count != 0u)
        {
            plane_band_print(line, path_band, draws, ones);
        }
        else
        {
            mmgr_line_ch(line, '\n');
        }
        sim_check(&results, under_count != 0u, "the drawn path lies on the disc");
        sim_flush(&results);
        plane_given(path_band);
        plane_given(under);
    }

    cudaFree(device_bits);
    cudaFree(device_longest);
    plane_given(sphere_drawn);
    plane_given(sphere_orders[1]);
    plane_given(sphere_orders[0]);
    plane_given(dragon_depth);
    for (size_t fill = 0u; fill < 2u; fill += 1u)
    {
        plane_extent_given(&spheres[fill]);
    }
    for (size_t extent = 0u; extent < PLANE_EXTENTS; extent += 1u)
    {
        plane_extent_given(&extents[extent]);
    }
    plane_given(bands);
    plane_given(drawn_longest);
    plane_given(host_longest);
    plane_given(longest);
    plane_given(drawn);
    plane_given(bits);
    return sim_close(&results, "pi plane");
}
