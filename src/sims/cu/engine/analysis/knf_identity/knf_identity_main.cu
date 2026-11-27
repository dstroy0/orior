// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// knf_identity_main.cu: departure, controls and main
#include "../../../../../../includes/platform/platform.h"
#include "knf_identity_internal.h"
#include "memoria_operor/memoria_operor.h"

// The pool of the sim. The two lane sets of the 64^3 cube over 177 frames hold about 186 MB. The two histories and
// their centered sums hold about 134 MB more
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

static OriorExactInteger s_knf_body_departure[KNF_SCALES][KNF_BODIES];

static void knf_print_share(VerbaLinea *line, const OriorExactInteger *part, const OriorExactInteger *whole)
{
    mmgr_line_ch(line, ' ');
    char text_buffer[32];
    VerbaLinea cell;
    cell.out = text_buffer;
    cell.capacity = sizeof(text_buffer);
    cell.at = 0ULL;
    sim_ratio_print(&cell, part, whole, 3u);
    mmgr_line_finish(&cell);
    mmgr_line_put_columns(line, text_buffer, 9u);
}

static int knf_departure(SimResults *results, const KnfRecord *record, const unsigned short *boxes,
                         const SimScene *scene, const KnfBox *box, unsigned long long tag, unsigned int *map,
                         unsigned int *cells, const char *name)
{
    static const unsigned long long s_knf_tiles[KNF_SCALES] = {1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
    const unsigned long long side = record->side;
    KnfEntangled whole;
    knf_entangle(record, NULL, side, boxes, &whole);
    OriorExactInteger truth;
    knf_exact_total(&whole, &truth);
    OriorExactInteger inside_departure[KNF_SCALES];
    OriorExactInteger between_departure[KNF_SCALES];
    int inside_identified[KNF_SCALES];
    int between_identified[KNF_SCALES];
    OriorExactInteger free_sum;
    unsigned long long split_matches = 0ULL;
    unsigned long long stored_count = 0ULL;
    for (unsigned int scale = 0u; scale < KNF_SCALES; scale += 1u)
    {
        const unsigned long long tile = s_knf_tiles[scale];
        KnfEntangled split;
        knf_entangle(record, NULL, tile, NULL, &split);
        OriorExactInteger split_total;
        knf_exact_total(&split, &split_total);
        split_matches += (orior_exact_compare(&split_total, &truth) == 0) ? 1ULL : 0ULL;
        OriorExactInteger inside_sum;
        OriorExactInteger between_sum;
        OriorExactInteger body_sum[KNF_BODIES];
        orior_exact_zero(&inside_sum);
        orior_exact_zero(&between_sum);
        for (unsigned int body = 0u; body < KNF_BODIES; body += 1u)
        {
            orior_exact_zero(&body_sum[body]);
        }
        inside_identified[scale] = 1;
        between_identified[scale] = 1;
        for (unsigned int draw = 0u; draw < KNF_DRAWS; draw += 1u)
        {
            const unsigned long long counter = (((((tag * KNF_SCALES) + scale) * 2ULL) * KNF_DRAWS) + draw);
            KnfEntangled drawn;
            OriorExactInteger total;
            OriorExactInteger running;
            knf_inside_draw(map, cells, side, tile, sim_draw(KNF_KEY ^ KNF_NULL_PURPOSE, counter));
            knf_entangle(record, map, tile, boxes, &drawn);
            knf_exact_total(&drawn, &total);
            inside_identified[scale] = inside_identified[scale] && (orior_exact_compare(&total, &truth) < 0);
            sim_exact_sum(&inside_sum, &total, &running);
            inside_sum = running;
            for (unsigned int body = 0u; body < KNF_BODIES; body += 1u)
            {
                OriorExactInteger share;
                knf_wide_exact(&drawn.body[body], &share);
                sim_exact_sum(&body_sum[body], &share, &running);
                body_sum[body] = running;
            }
            knf_between_draw(map, cells, side, tile, sim_draw(KNF_KEY ^ KNF_NULL_PURPOSE, counter + KNF_DRAWS));
            knf_entangle(record, map, tile, NULL, &drawn);
            stored_count += knf_wide_equal(&drawn.inside, &split.inside) ? 1ULL : 0ULL;
            knf_exact_total(&drawn, &total);
            between_identified[scale] = between_identified[scale] && (orior_exact_compare(&total, &truth) < 0);
            sim_exact_sum(&between_sum, &total, &running);
            between_sum = running;
        }
        knf_departure_of(&truth, &inside_sum, &inside_departure[scale]);
        knf_departure_of(&truth, &between_sum, &between_departure[scale]);
        free_sum = inside_sum;
        for (unsigned int body = 0u; body < KNF_BODIES; body += 1u)
        {
            OriorExactInteger body_truth;
            knf_wide_exact(&whole.body[body], &body_truth);
            knf_departure_of(&body_truth, &body_sum[body], &s_knf_body_departure[scale][body]);
        }
    }
    const OriorExactInteger *const universal = &inside_departure[KNF_SCALES - 1u];
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  ");
    mmgr_line_put(line, name);
    mmgr_line_put(line, ": the entangled entropy against the free null's mean ");
    OriorExactInteger scaled_truth;
    sim_exact_scaled(&truth, KNF_DRAWS, &scaled_truth);
    sim_ratio_print(line, &scaled_truth, &free_sum, 3u);
    mmgr_line_put(line, ", identified ");
    mmgr_line_put(line, (inside_identified[KNF_SCALES - 1u] != 0) ? "yes" : "no");
    mmgr_line_put(line, "\n    the departure curve, as a share of the free null's departure (");
    mmgr_line_decimal(line, KNF_DRAWS, 1u);
    mmgr_line_put(line, " draws a tile size, E past every draw marked *)\n");
    mmgr_line_put(line, "      tile    inside   between       sum\n");
    for (unsigned int scale = 0u; scale < KNF_SCALES; scale += 1u)
    {
        OriorExactInteger both;
        sim_exact_sum(&inside_departure[scale], &between_departure[scale], &both);
        mmgr_line_put(line, "    ");
        mmgr_line_decimal_columns(line, s_knf_tiles[scale], 6u);
        knf_print_share(line, &inside_departure[scale], universal);
        mmgr_line_ch(line, (inside_identified[scale] != 0) ? '*' : ' ');
        knf_print_share(line, &between_departure[scale], universal);
        mmgr_line_ch(line, (between_identified[scale] != 0) ? '*' : ' ');
        knf_print_share(line, &both, universal);
        mmgr_line_ch(line, '\n');
    }
    sim_check(results, split_matches == KNF_SCALES,
              "at every tile size the whole is its tiles plus its seams, exactly");
    sim_check(results, stored_count == (KNF_SCALES * KNF_DRAWS),
              "a rigid move of whole tiles preserves every tile's inside exactly");
    sim_check(results, inside_departure[0].sign == 0, "a tile of one voxel shuffles nothing: no departure");
    sim_check(results, between_departure[KNF_SCALES - 1u].sign == 0,
              "one tile moved whole moves nothing: no departure");
    if (box == NULL)
    {
        sim_flush(results);
        return inside_identified[KNF_SCALES - 1u];
    }
    mmgr_line_put(
        line,
        "    each body's box (zmin..zmax, ymin..ymax, xmin..xmax over its life), its own inside curve as a share of\n");
    mmgr_line_put(line, "    its free departure, and that departure as a share of the whole's\n");
    mmgr_line_put(line, "      body parent  born ended  vy vx  box z      y      x           2        4        8      "
                        " 16       32     share\n");
    for (unsigned int index = 0u; index < scene->bodies; index += 1u)
    {
        const SimBody *const body = &scene->body[index];
        mmgr_line_put(line, "    ");
        mmgr_line_decimal_columns(line, index, 6u);
        mmgr_line_ch(line, ' ');
        if (body->parent < 0LL)
        {
            mmgr_line_put(line, "     -");
        }
        else
        {
            // a parent index is non-negative here
            mmgr_line_decimal_columns(line, (unsigned long long)body->parent, 6u);
        }
        mmgr_line_decimal_columns(line, body->born, 6u);
        mmgr_line_decimal_columns(line, body->ended, 6u);
        mmgr_line_ch(line, ' ');
        mmgr_line_put(line, (body->velocity[1] < 0LL) ? " " : "  ");
        mmgr_line_decimal_signed(line, body->velocity[1]);
        mmgr_line_put(line, (body->velocity[2] < 0LL) ? " " : "  ");
        mmgr_line_decimal_signed(line, body->velocity[2]);
        mmgr_line_put(line, "  ");
        if (box[index].filled == 0)
        {
            mmgr_line_put(line, "never in view\n");
            continue;
        }
        for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
        {
            // a filled box's bounds are non-negative and below the side
            mmgr_line_decimal_columns(line, (unsigned long long)box[index].box[axis], 2u);
            mmgr_line_ch(line, '-');
            mmgr_line_decimal_columns(line, (unsigned long long)box[index].box[SIM_AXES + axis], 2u);
            mmgr_line_ch(line, ' ');
        }
        for (unsigned int scale = 1u; scale + 1u < KNF_SCALES; scale += 1u)
        {
            knf_print_share(line, &s_knf_body_departure[scale][index], &s_knf_body_departure[KNF_SCALES - 1u][index]);
        }
        knf_print_share(line, &s_knf_body_departure[KNF_SCALES - 1u][index], universal);
        mmgr_line_ch(line, '\n');
    }
    sim_flush(results);
    return inside_identified[KNF_SCALES - 1u];
}

static void knf_controls(SimResults *results, unsigned short *device_lanes, unsigned short *lanes, KnfRecord *record,
                         unsigned int *map, unsigned int *cells)
{
    SimScene scene;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &scene, .bytes = sizeof(scene), .val = 0);
    scene.frames = KNF_FRAMES;
    for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
    {
        scene.extent[axis] = KNF_CONTROL_SIDE;
    }
    scene.background = 180ULL;
    const unsigned long long voxels = sim_scene_voxels(&scene);
    const unsigned long long lanes_count = scene.frames * voxels;
    record->side = KNF_CONTROL_SIDE;
    record->voxels = voxels;
    unsigned long long projected = 0ULL;
    unsigned long long identified = 0ULL;
    for (unsigned int control = 0u; control < KNF_CONTROLS; control += 1u)
    {
        SimCamera camera;
        knf_camera(&camera, sim_draw(KNF_KEY ^ KNF_CONTROL_PURPOSE, control), 0ULL);
        unsigned long long clipped = 0ULL;
        int ok = sim_render(results, &scene, &camera, device_lanes, NULL, NULL, &clipped) && (clipped == 0ULL);
        ok = ok &&
             sim_status_check(
                 results, cudaMemcpy(lanes, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                 "control lanes read");
        ok = ok && knf_project(results, lanes, record);
        if (ok == 0)
        {
            continue;
        }
        projected += 1ULL;
        identified +=
            (knf_identified(record, map, cells, sim_draw(KNF_KEY ^ KNF_CONTROL_PURPOSE, KNF_CONTROLS + control)) != 0)
                ? 1ULL
                : 0ULL;
    }
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  the null's own rate: ");
    mmgr_line_decimal(line, KNF_CONTROLS, 1u);
    mmgr_line_put(line, " volumes of 32^3 with every voxel drawn alike (no bodies, no ramp, no fixed pattern), each\n");
    mmgr_line_put(line, "  against ");
    mmgr_line_decimal(line, KNF_DRAWS, 1u);
    mmgr_line_put(line, " free spatial draws: ");
    mmgr_line_decimal(line, identified, 1u);
    mmgr_line_put(line, " identified, where exchangeability bounds the rate by 1/");
    mmgr_line_decimal(line, KNF_DRAWS + 1u, 1u);
    mmgr_line_ch(line, '\n');
    const unsigned long long scaled = (KNF_DRAWS + 1ULL) * identified;
    const unsigned long long excess = (scaled > KNF_CONTROLS) ? (scaled - KNF_CONTROLS) : 0ULL;
    sim_check(results, projected == KNF_CONTROLS, "every control volume renders and projects");
    sim_check(results, (excess * excess) <= (KNF_RANGE_SQUARED * KNF_CONTROLS * (KNF_DRAWS + 1ULL)),
              "alike voxels are identified in at max n/(draws + 1) + 5 sqrt(n p) of the n controls");
}

int program_main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    SimBody body[KNF_BODIES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = body, .bytes = sizeof(body), .val = 0);
    SimScene scene;
    knf_scene(&scene, body, 1);
    SimCamera camera;
    knf_camera(&camera, KNF_KEY, 8ULL);
    const unsigned long long voxels = sim_scene_voxels(&scene);
    const unsigned long long lanes_count = scene.frames * voxels;
    const unsigned long long history_words = KNF_WINDOWS * voxels;
    const unsigned long long cloud_words = KNF_WINDOWS * KNF_WINDOWS;

    unsigned short *lanes =
        (unsigned short *)platform_cell_taken((unsigned long long)((size_t)lanes_count * sizeof(unsigned short)),
                                              (unsigned int)platform_place_g_program_owner, 0);
    unsigned short *moved =
        (unsigned short *)platform_cell_taken((unsigned long long)((size_t)lanes_count * sizeof(unsigned short)),
                                              (unsigned int)platform_place_g_program_owner, 0);
    unsigned int *map = (unsigned int *)platform_cell_taken((unsigned long long)((size_t)voxels * sizeof(unsigned int)),
                                                            (unsigned int)platform_place_g_program_owner, 0);
    unsigned int *cells = (unsigned int *)platform_cell_taken(
        (unsigned long long)((size_t)voxels * sizeof(unsigned int)), (unsigned int)platform_place_g_program_owner, 0);
    unsigned short *boxes = (unsigned short *)platform_cell_taken(
        (unsigned long long)((size_t)voxels * sizeof(unsigned short)), (unsigned int)platform_place_g_program_owner, 0);
    KnfRecord record;
    KnfRecord moved_record;
    record.side = KNF_SIDE;
    record.voxels = voxels;
    record.history = (unsigned long long *)platform_cell_taken((size_t)history_words * sizeof(unsigned long long),
                                                               platform_place_g_program_owner, 0);
    record.cloud = (unsigned long long *)platform_cell_taken((size_t)cloud_words * sizeof(unsigned long long),
                                                             platform_place_g_program_owner, 0);
    record.centerd =
        (long long *)platform_cell_taken((size_t)history_words * sizeof(long long), platform_place_g_program_owner, 0);
    moved_record = record;
    moved_record.history = (unsigned long long *)platform_cell_taken((size_t)history_words * sizeof(unsigned long long),
                                                                     platform_place_g_program_owner, 0);
    moved_record.cloud = (unsigned long long *)platform_cell_taken((size_t)cloud_words * sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0);
    moved_record.centerd =
        (long long *)platform_cell_taken((size_t)history_words * sizeof(long long), platform_place_g_program_owner, 0);
    unsigned short *device_lanes = NULL;
    int ok = (lanes != NULL) && (moved != NULL) && (map != NULL) && (cells != NULL) && (boxes != NULL) &&
             (record.history != NULL) && (record.cloud != NULL) && (record.centerd != NULL) &&
             (moved_record.history != NULL) && (moved_record.cloud != NULL) && (moved_record.centerd != NULL);
    sim_check(&results, ok, "host buffers");
    ok = ok && sim_job_submit(&results, "knf_identity", count, arguments,
                              (4ULL * lanes_count * sizeof(unsigned short)) +
                                  ((history_words + cloud_words + 1ULL) * sizeof(unsigned long long)));
    ok = ok &&
         sim_status_check(&results, cudaMalloc((void **)&device_lanes, lanes_count * sizeof(unsigned short)), "lanes");

    VerbaLinea *const line = &results.line;
    mmgr_line_put(line,
                  "  the knf's identity by spatial null permutation: the nbody lattice's law in a 64^3 cube over ");
    mmgr_line_decimal(line, KNF_FRAMES, 1u);
    mmgr_line_put(line, " frames,\n  ");
    mmgr_line_decimal(line, KNF_WINDOWS, 1u);
    mmgr_line_put(line, " windows of ");
    mmgr_line_decimal(line, ENGINE_HISTORY_WINDOW, 1u);
    mmgr_line_put(line, " transitions; a section is one voxel's windows, E couples each with its xyz neighbors\n");
    sim_flush(&results);

    unsigned long long clipped = 0ULL;
    ok = ok && sim_render(&results, &scene, &camera, device_lanes, NULL, NULL, &clipped);
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(lanes, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                          "lanes read");
    ok = ok && (clipped == 0ULL);
    sim_check(&results, ok, "the lattice renders, no lane clipped");
    KnfBox box[KNF_BODIES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = box, .bytes = sizeof(box), .val = 0);
    if (ok)
    {
        knf_boxes(&scene, box, boxes);
        ok = knf_project(&results, lanes, &record);
        sim_check(&results, ok, "the lattice's knf projects, its parity law satisfied at every voxel");
    }
    if (ok)
    {
        knf_one_bit(&results, lanes, &record);
        sim_flush(&results);
        knf_motions(&results, lanes, moved, &record, &moved_record, map);
        sim_flush(&results);
        const int identified = knf_departure(&results, &record, boxes, &scene, box, 0ULL, map, cells, "the lattice");
        sim_check(&results, identified, "the lattice's knf is identified past every free spatial draw");
    }

    SimScene empty;
    knf_scene(&empty, body, 0);
    ok = ok && sim_render(&results, &empty, &camera, device_lanes, NULL, NULL, &clipped);
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(lanes, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                          "empty lanes read");
    ok = ok && (clipped == 0ULL) && knf_project(&results, lanes, &record);
    sim_check(&results, ok, "the same scene with no bodies renders and projects");
    if (ok)
    {
        knf_departure(&results, &record, NULL, &empty, NULL, 1ULL, map, cells, "the same camera and ramp, no bodies");
        knf_controls(&results, device_lanes, lanes, &record, map, cells);
    }

    cudaFree(device_lanes);
    platform_cell_released(moved_record.centerd, platform_place_g_program_owner);
    platform_cell_released(moved_record.cloud, platform_place_g_program_owner);
    platform_cell_released(moved_record.history, platform_place_g_program_owner);
    platform_cell_released(record.centerd, platform_place_g_program_owner);
    platform_cell_released(record.cloud, platform_place_g_program_owner);
    platform_cell_released(record.history, platform_place_g_program_owner);
    platform_cell_released(boxes, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(cells, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(map, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(moved, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(lanes, (unsigned int)platform_place_g_program_owner);
    return sim_close(&results, "knf identity");
}
