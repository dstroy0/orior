// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "sim_camera.h"

#include "../../../../../cu/engine/analysis/period/period.h"

#define POWER_KEY 0x504F574552ULL

#define POWER_VOLUMES 32u

#define POWER_PERIOD 5ULL

#define POWER_AMPLITUDES 9u

#define POWER_DRAW_COUNTS 2u

// a count of false periods over n chances at 1/(draws + 1) each may exceed n p by 5 sqrt(n p), this squared over n p:
// the binomial's spread is sqrt(n p (1 - p)). The tolerance is 5.30 of it at 8 draws and 5.13 at 19
#define POWER_RANGE_SQUARED 25ULL

typedef struct
{
    unsigned long long found;
    unsigned long long found_multiple;
    unsigned long long found_elsewhere;
    unsigned long long false_other_axes;
} PowerResults;

static int power_read(SimResults *results, const unsigned short *lanes, const unsigned short *device_lanes,
                      const SimScene *scene, unsigned long long draws, PeriodMargin *band,
                      PeriodMeasurement *measurement)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    PeriodRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.device_lanes = device_lanes;
    request.rank = SIM_AXES;
    for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
    {
        request.extent[axis] = scene->extent[axis];
    }
    request.draws = draws;
    request.content = sim_content(lanes, sim_scene_voxels(scene), POWER_KEY);
    request.band = band;
    request.band_capacity = draws * SIM_AXES;
    request.measurement = measurement;
    request.error = &error;
    const long long status = period_read(&request);
    sim_check(results, (status == 0LL) && (error.type == ENGINE_ERROR_NONE), "period_read accepted the volume");
    return status == 0LL;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    const unsigned long long amplitude[POWER_AMPLITUDES] = {0ULL,  2ULL,   8ULL,   16ULL,  32ULL,
                                                            64ULL, 128ULL, 256ULL, 1024ULL};
    const unsigned long long draw_count[POWER_DRAW_COUNTS] = {8ULL, 19ULL};

    SimScene scene;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &scene, .bytes = sizeof(scene), .val = 0);
    scene.frames = 1ULL;
    scene.extent[0] = 32ULL;
    scene.extent[1] = 64ULL;
    scene.extent[2] = 64ULL;
    scene.background = 200ULL;
    scene.plant_axis = 2u;
    scene.plant_period = POWER_PERIOD;
    SimCamera camera;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &camera, .bytes = sizeof(camera), .val = 0);
    camera.offset = 100ULL;
    camera.gain = 1ULL;
    camera.read_square = 3ULL;
    camera.pattern_range = 8ULL;
    camera.shot = 1ULL;

    const unsigned long long voxels = sim_scene_voxels(&scene);
    unsigned short *const lanes = (unsigned short *)platform_cell_taken(
        (unsigned long long)((size_t)voxels * sizeof(unsigned short)), (unsigned int)platform_place_g_program_owner, 0);
    PeriodMargin band[19ULL * SIM_AXES];
    unsigned short *device_lanes = NULL;
    int ok = lanes != NULL;
    ok = ok && sim_job_submit(&results, "period_power", 0, NULL, voxels * sizeof(unsigned short));
    ok = ok && sim_status_check(&results, cudaMalloc((void **)&device_lanes, voxels * sizeof(unsigned short)), "lanes");

    VerbaLinea *const line = &results.line;
    mmgr_line_put(line, "  the period reading's power: a planted period ");
    mmgr_line_decimal(line, POWER_PERIOD, 1u);
    mmgr_line_put(
        line, " along x of 32 x 64 x 64 under the camera law (signal 200 e, read variance 3, fixed pattern 0 to 8)\n");
    mmgr_line_put(line, "  each column over ");
    mmgr_line_decimal(line, POWER_VOLUMES, 1u);
    mmgr_line_put(line, " volumes; the plant adds 0 to the amplitude electrons by phase\n");
    mmgr_line_put(line, "  draws  amplitude  x reads 5  x reads 10 or 15  x reads another  z or y reads one (of 64)\n");
    for (unsigned int counts = 0u; ok && (counts < POWER_DRAW_COUNTS); counts += 1u)
    {
        const unsigned long long draws = draw_count[counts];
        for (unsigned int level = 0u; ok && (level < POWER_AMPLITUDES); level += 1u)
        {
            PowerResults result;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &result, .bytes = sizeof(result), .val = 0);
            scene.plant_amplitude = amplitude[level];
            for (unsigned int volume = 0u; ok && (volume < POWER_VOLUMES); volume += 1u)
            {
                camera.key = sim_draw(POWER_KEY, (((counts * POWER_AMPLITUDES) + level) * POWER_VOLUMES) + volume);
                scene.plant_key = camera.key ^ 0x504C414E54ULL;
                unsigned long long clipped = 0ULL;
                ok = sim_render(&results, &scene, &camera, device_lanes, NULL, NULL, &clipped);
                ok = ok && (clipped == 0ULL);
                ok = ok && sim_status_check(
                               &results,
                               cudaMemcpy(lanes, device_lanes, voxels * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                               "lanes read");
                PeriodMeasurement measurement;
                ok = ok && power_read(&results, lanes, device_lanes, &scene, draws, band, &measurement);
                if (ok)
                {
                    const unsigned long long read_x = measurement.axis[2].period;
                    const int multiple = (read_x > POWER_PERIOD) && ((read_x % POWER_PERIOD) == 0ULL);
                    result.found += (read_x == POWER_PERIOD) ? 1ULL : 0ULL;
                    result.found_multiple += multiple ? 1ULL : 0ULL;
                    result.found_elsewhere += ((read_x != 0ULL) && (read_x % POWER_PERIOD) != 0ULL) ? 1ULL : 0ULL;
                    result.false_other_axes += (measurement.axis[0].period != 0ULL) ? 1ULL : 0ULL;
                    result.false_other_axes += (measurement.axis[1].period != 0ULL) ? 1ULL : 0ULL;
                }
            }
            mmgr_line_decimal_columns(line, draws, 7u);
            mmgr_line_decimal_columns(line, amplitude[level], 11u);
            mmgr_line_decimal_columns(line, result.found, 11u);
            mmgr_line_decimal_columns(line, result.found_multiple, 16u);
            mmgr_line_decimal_columns(line, result.found_elsewhere, 17u);
            mmgr_line_decimal_columns(line, result.false_other_axes, 18u);
            mmgr_line_ch(line, '\n');
            sim_flush(&results);
            const unsigned long long axes = 2ULL * POWER_VOLUMES;
            const unsigned long long scaled = (draws + 1ULL) * result.false_other_axes;
            const unsigned long long excess = (scaled > axes) ? (scaled - axes) : 0ULL;
            sim_check(&results, (excess * excess) <= (POWER_RANGE_SQUARED * axes * (draws + 1ULL)),
                      "the unplanted axes' false periods exceed n/(draws + 1) by at max 5 sqrt(n p)");
            if (amplitude[level] == 0ULL)
            {
                const unsigned long long scaled_x =
                    (draws + 1ULL) * (result.found + result.found_multiple + result.found_elsewhere);
                const unsigned long long excess_x = (scaled_x > POWER_VOLUMES) ? (scaled_x - POWER_VOLUMES) : 0ULL;
                sim_check(&results, (excess_x * excess_x) <= (POWER_RANGE_SQUARED * POWER_VOLUMES * (draws + 1ULL)),
                          "with nothing planted, x's false periods exceed n/(draws + 1) by at max 5 sqrt(n p)");
            }
            if (level + 1u == POWER_AMPLITUDES)
            {
                sim_check(&results, (result.found + result.found_multiple) == POWER_VOLUMES,
                          "the strongest plant is read as 5 or a multiple of it on every volume");
            }
        }
    }
    sim_check(&results, ok, "every volume rendered and read");
    cudaFree(device_lanes);
    platform_cell_released(lanes, (unsigned int)platform_place_g_program_owner);
    return sim_close(&results, "period power");
}
