// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// knf_identity_sections.cu: entangling, draws, sections and motions
#include "knf_identity_internal.h"
#include "memoria_operor/memoria_operor.h"

static const unsigned int s_knf_axis_order[6][SIM_AXES] = {{0u, 1u, 2u}, {0u, 2u, 1u}, {1u, 0u, 2u},
                                                           {1u, 2u, 0u}, {2u, 0u, 1u}, {2u, 1u, 0u}};

// the entangled entropy of the knf: the centered windows of every voxel against those of its neighbor along z, y and x
// on the torus. The call splits it into the edges inside a tile of the given side and the seams between tiles. The
// share of a box is its own edges
void knf_entangle(const KnfRecord *record, const unsigned int *map, unsigned long long tile,
                  const unsigned short *boxes, KnfEntangled *result)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = result, .bytes = sizeof(*result), .val = 0);
    const unsigned long long side = record->side;
    for (unsigned long long depth = 0ULL; depth < side; depth += 1ULL)
    {
        for (unsigned long long row = 0ULL; row < side; row += 1ULL)
        {
            for (unsigned long long column = 0ULL; column < side; column += 1ULL)
            {
                const unsigned long long place[SIM_AXES] = {depth, row, column};
                const unsigned long long voxel = (((depth * side) + row) * side) + column;
                const unsigned long long section = (map != NULL) ? map[voxel] : voxel;
                const long long *const here = &record->centerd[section * KNF_WINDOWS];
                for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
                {
                    unsigned long long next[SIM_AXES] = {depth, row, column};
                    next[axis] = (place[axis] + 1ULL) % side;
                    const unsigned long long across = (((next[0] * side) + next[1]) * side) + next[2];
                    const unsigned long long other = (map != NULL) ? map[across] : across;
                    const long long term = knf_pair(here, &record->centerd[other * KNF_WINDOWS]);
                    const int within = (place[axis] / tile) == (next[axis] / tile);
                    knf_wide_add(within ? &result->inside : &result->seam, term);
                    if (boxes == NULL)
                    {
                        continue;
                    }
                    unsigned int common = (unsigned int)boxes[voxel] & (unsigned int)boxes[across];
                    for (unsigned int body = 0u; common != 0u; body += 1u)
                    {
                        if (((common >> body) & 1u) != 0u)
                        {
                            knf_wide_add(&result->body[body], term);
                            common &= ~(1u << body);
                        }
                    }
                }
            }
        }
    }
}

// a null draw that shuffles the knf's sections inside each tile of the given side, and moves nothing across a seam
void knf_inside_draw(unsigned int *map, unsigned int *cells, unsigned long long side, unsigned long long tile,
                     unsigned long long key)
{
    const unsigned long long voxels = side * side * side;
    for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
    {
        // a voxel index is below 2^18 here
        map[voxel] = (unsigned int)voxel;
    }
    const unsigned long long across = side / tile;
    unsigned long long counter = 0ULL;
    for (unsigned long long tile_depth = 0ULL; tile_depth < across; tile_depth += 1ULL)
    {
        for (unsigned long long tile_row = 0ULL; tile_row < across; tile_row += 1ULL)
        {
            for (unsigned long long tile_column = 0ULL; tile_column < across; tile_column += 1ULL)
            {
                unsigned long long cell_count = 0ULL;
                for (unsigned long long depth = tile_depth * tile; depth < (tile_depth + 1ULL) * tile; depth += 1ULL)
                {
                    for (unsigned long long row = tile_row * tile; row < (tile_row + 1ULL) * tile; row += 1ULL)
                    {
                        for (unsigned long long column = tile_column * tile; column < (tile_column + 1ULL) * tile;
                             column += 1ULL)
                        {
                            // a voxel index is below 2^18 here
                            cells[cell_count] = (unsigned int)((((depth * side) + row) * side) + column);
                            cell_count += 1ULL;
                        }
                    }
                }
                for (unsigned long long remaining = cell_count; remaining > 1ULL; remaining -= 1ULL)
                {
                    const unsigned long long pick = sim_draw_below(key, counter, remaining);
                    counter += 1ULL;
                    const unsigned int chosen = map[cells[remaining - 1ULL]];
                    map[cells[remaining - 1ULL]] = map[cells[pick]];
                    map[cells[pick]] = chosen;
                }
            }
        }
    }
}

// the inverse draw: whole tiles move rigidly to shuffled places, and nothing inside a tile moves against its own
void knf_between_draw(unsigned int *map, unsigned int *cells, unsigned long long side, unsigned long long tile,
                      unsigned long long key)
{
    const unsigned long long across = side / tile;
    const unsigned long long tiles = across * across * across;
    for (unsigned long long index = 0ULL; index < tiles; index += 1ULL)
    {
        // a tile index is below 2^18 here
        cells[index] = (unsigned int)index;
    }
    for (unsigned long long remaining = tiles; remaining > 1ULL; remaining -= 1ULL)
    {
        const unsigned long long pick = sim_draw_below(key, remaining, remaining);
        const unsigned int chosen = cells[remaining - 1ULL];
        cells[remaining - 1ULL] = cells[pick];
        cells[pick] = chosen;
    }
    for (unsigned long long depth = 0ULL; depth < side; depth += 1ULL)
    {
        for (unsigned long long row = 0ULL; row < side; row += 1ULL)
        {
            for (unsigned long long column = 0ULL; column < side; column += 1ULL)
            {
                const unsigned long long place =
                    ((((depth / tile) * across) + (row / tile)) * across) + (column / tile);
                const unsigned long long source = cells[place];
                const unsigned long long source_depth = ((source / (across * across)) * tile) + (depth % tile);
                const unsigned long long source_row = (((source / across) % across) * tile) + (row % tile);
                const unsigned long long source_column = ((source % across) * tile) + (column % tile);
                // a voxel index is below 2^18 here
                map[(((depth * side) + row) * side) + column] =
                    (unsigned int)((((source_depth * side) + source_row) * side) + source_column);
            }
        }
    }
}

int knf_identified(const KnfRecord *record, unsigned int *map, unsigned int *cells, unsigned long long key_base)
{
    KnfEntangled whole;
    knf_entangle(record, NULL, record->side, NULL, &whole);
    OriorExactInteger truth;
    knf_exact_total(&whole, &truth);
    int wins = 1;
    for (unsigned int draw = 0u; draw < KNF_DRAWS; draw += 1u)
    {
        knf_inside_draw(map, cells, record->side, record->side, sim_draw(key_base, draw));
        KnfEntangled drawn;
        knf_entangle(record, map, record->side, NULL, &drawn);
        OriorExactInteger total;
        knf_exact_total(&drawn, &total);
        wins = wins && (orior_exact_compare(&total, &truth) < 0);
    }
    return wins;
}

static void knf_host_section(const unsigned short *series, unsigned long long *words)
{
    unsigned int counts[ENGINE_HISTORY_BITS];
    for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
    {
        counts[bit] = 0u;
    }
    unsigned long long window = 0ULL;
    unsigned int in_window = 0u;
    for (unsigned long long frame = 1ULL; frame < KNF_FRAMES; frame += 1ULL)
    {
        const unsigned int flipped = (unsigned int)series[frame] ^ (unsigned int)series[frame - 1ULL];
        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
        {
            counts[bit] += (flipped >> bit) & 1u;
        }
        in_window += 1u;
        if ((in_window == ENGINE_HISTORY_WINDOW) || (frame == (KNF_FRAMES - 1ULL)))
        {
            unsigned long long packed = 0ULL;
            for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
            {
                packed |= (unsigned long long)counts[bit] << (4u * bit);
                counts[bit] = 0u;
            }
            words[window] = packed;
            window += 1ULL;
            in_window = 0u;
        }
    }
}

static int knf_section_same(const KnfRecord *record, unsigned long long voxel, const unsigned long long *words)
{
    int same = 1;
    for (unsigned long long window = 0ULL; window < KNF_WINDOWS; window += 1ULL)
    {
        same = same && (record->history[(window * record->voxels) + voxel] == words[window]);
    }
    return same;
}

void knf_one_bit(SimResults *results, const unsigned short *lanes, const KnfRecord *record)
{
    const unsigned long long voxels = record->voxels;
    unsigned short series[KNF_FRAMES];
    unsigned long long words[KNF_WINDOWS];
    unsigned long long mirror_differ = 0ULL;
    for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
    {
        for (unsigned long long frame = 0ULL; frame < KNF_FRAMES; frame += 1ULL)
        {
            series[frame] = lanes[(frame * voxels) + voxel];
        }
        knf_host_section(series, words);
        mirror_differ += (knf_section_same(record, voxel, words) != 0) ? 0ULL : 1ULL;
    }
    sim_check(results, mirror_differ == 0ULL, "the host's walk of every voxel equals the engine's knf");
    unsigned long long agree = 0ULL;
    unsigned long long unchanged = 0ULL;
    for (unsigned int flip = 0u; flip < KNF_FLIPS; flip += 1u)
    {
        const unsigned long long key = KNF_KEY ^ KNF_FLIP_PURPOSE;
        const unsigned long long voxel = sim_draw_below(key, 3ULL * flip, voxels);
        const unsigned long long frame = sim_draw_below(key, (3ULL * flip) + 1ULL, KNF_FRAMES);
        // a draw below 16 fits any unsigned int
        const unsigned int bit = (unsigned int)sim_draw_below(key, (3ULL * flip) + 2ULL, ENGINE_HISTORY_BITS);
        for (unsigned long long at = 0ULL; at < KNF_FRAMES; at += 1ULL)
        {
            series[at] = lanes[(at * voxels) + voxel];
        }
        // one bit of a 16-bit lane flipped, which stays in the lane's range
        series[frame] = (unsigned short)((unsigned int)series[frame] ^ (1u << bit));
        knf_host_section(series, words);
        const int same = knf_section_same(record, voxel, words);
        // a flip inside a window toggles two of its transitions, which cancel when exactly one of them flipped before
        const int predicted =
            (frame >= 1ULL) && ((frame + 1ULL) < KNF_FRAMES) && ((frame % ENGINE_HISTORY_WINDOW) != 0ULL) &&
            (((((unsigned int)series[frame - 1ULL] ^ (unsigned int)series[frame + 1ULL]) >> bit) & 1u) != 0u);
        agree += (same == predicted) ? 1ULL : 0ULL;
        unchanged += (same != 0) ? 1ULL : 0ULL;
    }
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  one bit: the host's walk equals the knf at every voxel; of ");
    mmgr_line_decimal(line, KNF_FLIPS, 1u);
    mmgr_line_put(line, " single flipped bits, ");
    mmgr_line_decimal(line, unchanged, 1u);
    mmgr_line_put(line, " leave the knf unchanged, and the window rule names ");
    mmgr_line_decimal(line, agree, 1u);
    mmgr_line_put(line, " of them exactly\n");
    sim_check(results, agree == KNF_FLIPS,
              "a flipped bit leaves the knf unchanged exactly when the window rule specifies");
}

static void knf_motion_place(unsigned long long side, unsigned int motion, const unsigned long long *shift,
                             const unsigned long long *place, unsigned long long *moved)
{
    const unsigned int *const order = s_knf_axis_order[motion / 8u];
    for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
    {
        const unsigned long long taken = place[order[axis]];
        const unsigned long long reflected = ((((motion & 7u) >> axis) & 1u) != 0u) ? (side - 1ULL - taken) : taken;
        moved[axis] = (reflected + shift[axis]) % side;
    }
}

void knf_motions(SimResults *results, const unsigned short *lanes, unsigned short *moved, const KnfRecord *record,
                 KnfRecord *moved_record, unsigned int *target)
{
    const unsigned long long side = record->side;
    const unsigned long long voxels = record->voxels;
    KnfEntangled whole;
    knf_entangle(record, NULL, side, NULL, &whole);
    unsigned long long projected = 0ULL;
    unsigned long long carried = 0ULL;
    unsigned long long retained = 0ULL;
    unsigned long long cloud_count = 0ULL;
    for (unsigned int motion = 0u; motion < KNF_MOTIONS; motion += 1u)
    {
        unsigned long long shift[SIM_AXES];
        for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
        {
            shift[axis] = sim_draw_below(KNF_KEY ^ KNF_MOTION_PURPOSE, (3ULL * motion) + axis, side);
        }
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
        {
            const unsigned long long place[SIM_AXES] = {voxel / (side * side), (voxel / side) % side, voxel % side};
            unsigned long long to[SIM_AXES];
            knf_motion_place(side, motion, shift, place, to);
            // a voxel index is below 2^18 here
            target[voxel] = (unsigned int)((((to[0] * side) + to[1]) * side) + to[2]);
        }
        for (unsigned long long frame = 0ULL; frame < KNF_FRAMES; frame += 1ULL)
        {
            for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
            {
                moved[(frame * voxels) + target[voxel]] = lanes[(frame * voxels) + voxel];
            }
        }
        if (knf_project(results, moved, moved_record) == 0)
        {
            continue;
        }
        projected += 1ULL;
        unsigned long long differ = 0ULL;
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
        {
            int same = 1;
            for (unsigned long long window = 0ULL; window < KNF_WINDOWS; window += 1ULL)
            {
                same = same && (moved_record->history[(window * voxels) + target[voxel]] ==
                                record->history[(window * voxels) + voxel]);
            }
            differ += (same != 0) ? 0ULL : 1ULL;
        }
        carried += (differ == 0ULL) ? 1ULL : 0ULL;
        KnfEntangled turned;
        knf_entangle(moved_record, NULL, side, NULL, &turned);
        retained += knf_wide_equal(&turned.inside, &whole.inside) ? 1ULL : 0ULL;
        cloud_count += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = moved_record->cloud, .other = record->cloud,
                                   .bytes = (size_t)(KNF_WINDOWS * KNF_WINDOWS) * sizeof(unsigned long long)) == 0)
                           ? 1ULL
                           : 0ULL;
    }
    VerbaLinea *const line = &results->line;
    mmgr_line_put(
        line,
        "  the exact motions (6 axis orders x 8 reflections, each with a keyed torus translation), the data moved\n");
    mmgr_line_put(line, "  over xyz and projected again: the knf carried as a whole on ");
    mmgr_line_decimal(line, carried, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, KNF_MOTIONS, 1u);
    mmgr_line_put(line, ", its entangled entropy unchanged on ");
    mmgr_line_decimal(line, retained, 1u);
    mmgr_line_put(line, ", its cloud unchanged on ");
    mmgr_line_decimal(line, cloud_count, 1u);
    mmgr_line_ch(line, '\n');
    sim_check(results, projected == KNF_MOTIONS, "every moved volume projects, its parity law satisfied");
    sim_check(results, carried == KNF_MOTIONS, "the knf of the moved data is the moved knf, from every exact angle");
    sim_check(results, retained == KNF_MOTIONS, "the entangled entropy reads the same from every exact angle");
    sim_check(results, cloud_count == KNF_MOTIONS, "the cloud reads the same from every exact angle");
}
