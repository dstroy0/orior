// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// root_universal_main.cu: the extent printed and main
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "root_universal_internal.h"

static void root_extent_print(VerbaLinea *line, const unsigned long long extent[4])
{
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        if (axis != 0u)
        {
            mmgr_line_put(line, " x ");
        }
        mmgr_line_decimal(line, extent[axis], 1u);
    }
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;

    SimCamera camera;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &camera, .bytes = sizeof(camera), .val = 0);
    camera.offset = 100ULL;
    camera.gain = 1ULL;
    camera.read_square = 3ULL;
    camera.pattern_range = 8ULL;
    camera.shot = 1ULL;

    const unsigned long long extent[ROOT_EXTENTS][4] = {{4ULL, 12ULL, 48ULL, 48ULL}, {3ULL, 13ULL, 37ULL, 41ULL}};
    RootVolume volume[ROOT_EXTENTS];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = volume, .bytes = sizeof(volume), .val = 0);
    // both volumes' device lanes, the tower's and the coder's pools for the larger, and the table of the widest edge;
    // the coded stream, sized by the values, is left to the retained peak
    unsigned long long declared = (unsigned long long)ROOT_TABLE_CAPACITY * sizeof(unsigned int);
    unsigned long long lanes_declared = 0ULL;
    for (unsigned int shaped = 0u; shaped < ROOT_EXTENTS; shaped += 1u)
    {
        const unsigned long long lanes = extent[shaped][0] * extent[shaped][1] * extent[shaped][2] * extent[shaped][3];
        declared += lanes * sizeof(unsigned short);
        lanes_declared = (lanes > lanes_declared) ? lanes : lanes_declared;
    }
    declared += tower_reserve_bytes(lanes_declared) + compression_reserve_bytes(lanes_declared);
    int ok = sim_job_submit(&results, "root_universal", 0, NULL, declared);
    unsigned long long lanes_max = 0ULL;
    for (unsigned int shaped = 0u; shaped < ROOT_EXTENTS; shaped += 1u)
    {
        ok = ok && root_volume_open(&results, &volume[shaped], extent[shaped]);
        lanes_max = (volume[shaped].lanes > lanes_max) ? volume[shaped].lanes : lanes_max;
    }
    RootProgram program;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &program, .bytes = sizeof(program), .val = 0);
    for (unsigned int at = 0u; at < ROOT_EDGES_MAX; at += 1u)
    {
        program.table[at] = (unsigned int *)platform_cell_taken((size_t)ROOT_TABLE_CAPACITY * sizeof(unsigned int),
                                                                platform_place_g_program_owner, 0);
        ok = ok && (program.table[at] != NULL);
    }
    int *crystal[4];
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        crystal[at] = (int *)platform_cell_taken((size_t)lanes_max * sizeof(int), platform_place_g_program_owner, 0);
        ok = ok && (crystal[at] != NULL);
    }
    sim_check(&results, ok, "the sim's tables and crystals were allocated");

    mmgr_line_put(
        line,
        "  the root universal: the tower with reversible lookup edges between its floors, on camera-law volumes\n");
    mmgr_line_put(line, "  (signal 200 e plus a ramp of 1 e a column, 6 moving bodies of 150 to 400 e, read variance "
                        "3, fixed pattern 0 to 8)\n\n");

    mmgr_line_put(
        line,
        "  1. the root stays exact: random edge programs (1 to 6 edges, random floors, widths 1 to 12 bits, and one\n");
    mmgr_line_put(line, "     20-bit edge per extent) through lift, code, wipe, decode, lower\n");
    mmgr_line_put(
        line, "  extent (t x z x y x)  floors  volumes  edges arranged  plain exact  edged exact  crystals changed\n");
    sim_flush(&results);
    for (unsigned int shaped = 0u; ok && (shaped < ROOT_EXTENTS); shaped += 1u)
    {
        RootVolume *const here = &volume[shaped];
        unsigned long long plain_exact = 0ULL;
        unsigned long long edged_exact = 0ULL;
        unsigned long long changed = 0ULL;
        unsigned long long edge_count = 0ULL;
        for (unsigned int drawn = 0u; ok && (drawn < ROOT_EXACT_VOLUMES); drawn += 1u)
        {
            camera.key = sim_draw(ROOT_KEY, ((unsigned long long)shaped * ROOT_EXACT_VOLUMES) + drawn);
            ok = root_volume_render(&results, here, &camera);
            unsigned long long bits = 0ULL;
            int exact = 0;
            ok = ok && root_crystal_trip(&results, here, NULL, 0u, crystal[0], &bits, &exact);
            plain_exact += (exact != 0) ? 1ULL : 0ULL;
            root_program_draw(&program, here->floors, sim_draw(camera.key, ROOT_PROGRAM_PURPOSE), drawn == 0u);
            ok = ok && root_crystal_trip(&results, here, program.edge, program.count, crystal[1], &bits, &exact);
            edged_exact += (exact != 0) ? 1ULL : 0ULL;
            changed += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = crystal[0], .other = crystal[1],
                                   .bytes = (size_t)here->lanes * sizeof(int)) != 0)
                           ? 1ULL
                           : 0ULL;
            edge_count += program.count;
        }
        mmgr_line_put(line, "  ");
        root_extent_print(line, here->extent);
        mmgr_line_decimal_columns(line, here->floors, 11u);
        mmgr_line_decimal_columns(line, ROOT_EXACT_VOLUMES, 9u);
        mmgr_line_decimal_columns(line, edge_count, 12u);
        mmgr_line_decimal_columns(line, plain_exact, 13u);
        mmgr_line_decimal_columns(line, edged_exact, 13u);
        mmgr_line_decimal_columns(line, changed, 18u);
        mmgr_line_ch(line, '\n');
        sim_flush(&results);
        sim_check(&results, plain_exact == ROOT_EXACT_VOLUMES, "every plain crystal rebuilt its lanes exactly");
        sim_check(&results, edged_exact == ROOT_EXACT_VOLUMES,
                  "every edged crystal rebuilt its lanes exactly through the coder");
    }

    mmgr_line_put(line, "\n  2. edges fold in order: on floor 1 of 4 x 12 x 48 x 48, 8-bit edges a then b against the "
                        "one table b(a(i))\n");
    mmgr_line_put(line, "  volumes  a,b = b(a)  b,a differs  a on 1, b on 2 differs\n");
    sim_flush(&results);
    unsigned long long folds = 0ULL;
    unsigned long long orders = 0ULL;
    unsigned long long blocks = 0ULL;
    RootVolume *const folded = &volume[0];
    for (unsigned int drawn = 0u; ok && (drawn < ROOT_FOLD_VOLUMES); drawn += 1u)
    {
        camera.key = sim_draw(ROOT_KEY ^ ROOT_FOLD_PURPOSE, drawn);
        ok = root_volume_render(&results, folded, &camera);
        unsigned int *const first = program.table[0];
        unsigned int *const second = program.table[1];
        unsigned int *const both = program.table[2];
        root_permutation(first, ROOT_FOLD_BITS, sim_draw(camera.key, 1ULL));
        root_permutation(second, ROOT_FOLD_BITS, sim_draw(camera.key, 2ULL));
        for (unsigned int index = 0u; index < (1u << ROOT_FOLD_BITS); index += 1u)
        {
            both[index] = second[first[index]];
        }
        TowerEdge pair[2];
        pair[0].floor = ROOT_FOLD_FLOOR;
        pair[0].index_bits = ROOT_FOLD_BITS;
        pair[0].forward = first;
        pair[1].floor = ROOT_FOLD_FLOOR;
        pair[1].index_bits = ROOT_FOLD_BITS;
        pair[1].forward = second;
        TowerEdge single;
        single.floor = ROOT_FOLD_FLOOR;
        single.index_bits = ROOT_FOLD_BITS;
        single.forward = both;
        TowerEdge swapped[2];
        swapped[0] = pair[1];
        swapped[1] = pair[0];
        TowerEdge split[2];
        split[0] = pair[0];
        split[1] = pair[1];
        split[1].floor = ROOT_FOLD_FLOOR + 1u;
        ok = ok && root_lift_crystal(&results, folded, pair, 2u, crystal[0]);
        ok = ok && root_lift_crystal(&results, folded, &single, 1u, crystal[1]);
        ok = ok && root_lift_crystal(&results, folded, swapped, 2u, crystal[2]);
        ok = ok && root_lift_crystal(&results, folded, split, 2u, crystal[3]);
        const size_t bytes = (size_t)folded->lanes * sizeof(int);
        folds += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = crystal[0], .other = crystal[1], .bytes = bytes) == 0)
                     ? 1ULL
                     : 0ULL;
        orders += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = crystal[0], .other = crystal[2], .bytes = bytes) != 0)
                      ? 1ULL
                      : 0ULL;
        blocks += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = crystal[0], .other = crystal[3], .bytes = bytes) != 0)
                      ? 1ULL
                      : 0ULL;
    }
    mmgr_line_decimal_columns(line, ROOT_FOLD_VOLUMES, 9u);
    mmgr_line_decimal_columns(line, folds, 12u);
    mmgr_line_decimal_columns(line, orders, 13u);
    mmgr_line_decimal_columns(line, blocks, 24u);
    mmgr_line_ch(line, '\n');
    sim_flush(&results);
    // "differs" is read on these keyed draws: tables that agree on every value the crystal contains would arrange the
    // same crystal swapped or split. The last two checks show order and blocking on these volumes, not for every table
    sim_check(&results, folds == ROOT_FOLD_VOLUMES,
              "two edges on one floor fold into the one table that composes them");
    sim_check(&results, orders == ROOT_FOLD_VOLUMES,
              "the fold preserves order: the swapped pair arranges a different crystal");
    sim_check(&results, blocks == ROOT_FOLD_VOLUMES, "a floor between two edges blocks the fold");

    const unsigned int width[ROOT_COST_WIDTHS] = {2u, 4u, 8u, 12u};
    const unsigned int floors = folded->floors;
    const unsigned int place_first[ROOT_PLACES] = {0u, floors / 2u, floors, 0u};
    const unsigned int place_last[ROOT_PLACES] = {0u, floors / 2u, floors, floors};
    unsigned long long plain_bits = 0ULL;
    unsigned long long cost[ROOT_COST_WIDTHS][ROOT_PLACES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = cost, .bytes = sizeof(cost), .val = 0);
    unsigned long long trips = 0ULL;
    unsigned long long exact_trips = 0ULL;
    for (unsigned int drawn = 0u; ok && (drawn < ROOT_COST_VOLUMES); drawn += 1u)
    {
        camera.key = sim_draw(ROOT_KEY ^ ROOT_COST_PURPOSE, drawn);
        ok = root_volume_render(&results, folded, &camera);
        unsigned long long bits = 0ULL;
        int exact = 0;
        ok = ok && root_crystal_trip(&results, folded, NULL, 0u, NULL, &bits, &exact);
        plain_bits += bits;
        trips += 1ULL;
        exact_trips += (exact != 0) ? 1ULL : 0ULL;
        for (unsigned int wide = 0u; ok && (wide < ROOT_COST_WIDTHS); wide += 1u)
        {
            for (unsigned int placed = 0u; ok && (placed < ROOT_PLACES); placed += 1u)
            {
                root_program_place(&program, width[wide], place_first[placed], place_last[placed],
                                   sim_draw(camera.key, (wide * ROOT_PLACES) + placed));
                ok = root_crystal_trip(&results, folded, program.edge, program.count, NULL, &bits, &exact);
                cost[wide][placed] += bits;
                trips += 1ULL;
                exact_trips += (exact != 0) ? 1ULL : 0ULL;
            }
        }
    }
    const unsigned long long coded = (unsigned long long)ROOT_COST_VOLUMES * folded->lanes;
    mmgr_line_put(line, "\n  3. the price: coded bits a voxel on 4 x 12 x 48 x 48 over ");
    mmgr_line_decimal(line, ROOT_COST_VOLUMES, 1u);
    mmgr_line_put(line, " volumes, one keyed random permutation a floor\n");
    mmgr_line_put(line, "  no edge: ");
    sim_fraction_print(line, plain_bits, coded, 3u);
    mmgr_line_put(line, "\n  floor (coefficients touched):  0 (");
    mmgr_line_decimal(line, root_region(folded->extent, 0u), 1u);
    mmgr_line_put(line, ")  ");
    mmgr_line_decimal(line, floors / 2u, 1u);
    mmgr_line_put(line, " (");
    mmgr_line_decimal(line, root_region(folded->extent, floors / 2u), 1u);
    mmgr_line_put(line, ")  ");
    mmgr_line_decimal(line, floors, 1u);
    mmgr_line_put(line, " collapsed (");
    mmgr_line_decimal(line, root_region(folded->extent, floors), 1u);
    mmgr_line_put(line, ")  every floor\n");
    mmgr_line_put(line, "  width    floor 0    middle    collapsed    every floor\n");
    for (unsigned int wide = 0u; wide < ROOT_COST_WIDTHS; wide += 1u)
    {
        mmgr_line_decimal_columns(line, width[wide], 7u);
        for (unsigned int placed = 0u; placed < ROOT_PLACES; placed += 1u)
        {
            mmgr_line_put(line, (placed == 0u) ? "     " : "    ");
            sim_fraction_print(line, cost[wide][placed], coded, 3u);
        }
        mmgr_line_ch(line, '\n');
    }
    sim_flush(&results);
    sim_check(&results, exact_trips == trips, "every coded crystal, edged or not, rebuilt its lanes exactly");

    sim_check(&results, ok, "every volume rendered, lifted, coded and lowered");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        platform_cell_released(crystal[at], platform_place_g_program_owner);
    }
    for (unsigned int at = 0u; at < ROOT_EDGES_MAX; at += 1u)
    {
        platform_cell_released(program.table[at], platform_place_g_program_owner);
    }
    for (unsigned int shaped = 0u; shaped < ROOT_EXTENTS; shaped += 1u)
    {
        root_volume_close(&volume[shaped]);
    }
    return sim_close(&results, "root universal");
}
