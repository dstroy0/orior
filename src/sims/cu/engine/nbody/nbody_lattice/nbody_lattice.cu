// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "sim_camera.h"

#define LATTICE_KEY 0x4E424F4459ull

#define LATTICE_FRAMES 24ull

#define LATTICE_DEPTH 20ull

#define LATTICE_ROWS 96ull

#define LATTICE_COLUMNS 96ull

#define LATTICE_FOUNDERS 10u

#define LATTICE_DIVISIONS 3u

#define LATTICE_BODIES (LATTICE_FOUNDERS + (2u * LATTICE_DIVISIONS))

#define LATTICE_SIGMAS_SQUARED 25ull

#define LATTICE_SECOND_MOMENT_PARTS 100ull

static void lattice_scene(SimScene *scene, SimBody *body)
{
    memset(scene, 0, sizeof(*scene));
    scene->frames = LATTICE_FRAMES;
    scene->extent[0] = LATTICE_DEPTH;
    scene->extent[1] = LATTICE_ROWS;
    scene->extent[2] = LATTICE_COLUMNS;
    scene->background = 40ull;
    scene->ramp = 2ull;
    scene->bodies = LATTICE_BODIES;
    scene->body = body;
    SimDraws draws;
    draws.key = LATTICE_KEY;
    draws.counter = 0ull;
    sim_founders_draw(&draws, scene, body, LATTICE_FOUNDERS);
    for (unsigned int division = 0u; division < LATTICE_DIVISIONS; division += 1u)
    {
        SimBody *const parent = &body[division];
        const unsigned long long frame = 8ull + (4ull * division);
        parent->ended = frame;
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            SimBody *const daughter = &body[LATTICE_FOUNDERS + (2u * division) + side];
            const long long sign = (side == 0u) ? -1ll : 1ll;
            *daughter = *parent;
            for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
            {
                daughter->center[axis] = sim_body_at(parent, frame, axis);
            }
            daughter->center[1] += sign * (parent->range[1] / 2ll);
            daughter->velocity[1] = parent->velocity[1] + sign;
            daughter->range[1] = (parent->range[1] + 1ll) / 2ll + 1ll;
            daughter->born = frame;
            daughter->ended = LATTICE_FRAMES;
            daughter->parent = division;
        }
    }
}

static void lattice_camera(SimCamera *camera)
{
    memset(camera, 0, sizeof(*camera));
    camera->key = LATTICE_KEY;
    camera->offset = 100ull;
    camera->gain = 1ull;
    camera->read_square = 3ull;
    camera->pattern_range = 8ull;
    camera->shot = 1ull;
}

static void lattice_print_rational(VerbaLinea *line, unsigned long long numerator, unsigned long long denominator)
{
    if (denominator == 0ull)
    {
        mmgr_line_put(line, "   gone");
        return;
    }
    sim_fraction_print(line, numerator, denominator, 3u);
}

static void lattice_print_scene(SimResults *results, const SimScene *scene, const unsigned long long *truth)
{
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  a synthetic n-body lattice under the camera law\n");
    mmgr_line_put(line, "  ");
    mmgr_line_decimal(line, scene->frames, 1u);
    mmgr_line_put(line, " frames of ");
    mmgr_line_decimal(line, scene->extent[0], 1u);
    mmgr_line_put(line, " x ");
    mmgr_line_decimal(line, scene->extent[1], 1u);
    mmgr_line_put(line, " x ");
    mmgr_line_decimal(line, scene->extent[2], 1u);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, scene->bodies, 1u);
    mmgr_line_put(line, " bodies (");
    mmgr_line_decimal(line, LATTICE_FOUNDERS, 1u);
    mmgr_line_put(line, " founders, ");
    mmgr_line_decimal(line, LATTICE_DIVISIONS, 1u);
    mmgr_line_put(line, " divisions)\n");
    mmgr_line_put(line, "  body parent born ended  first centroid z, y, x (exact, truncated to 3 places)   mass\n");
    for (unsigned int index = 0u; index < scene->bodies; index += 1u)
    {
        const SimBody *const body = &scene->body[index];
        const unsigned long long *const field = &truth[((body->born * scene->bodies) + index) * SIM_TRUTH_FIELDS];
        mmgr_line_put(line, "  ");
        mmgr_line_decimal_columns(line, index, 4u);
        mmgr_line_ch(line, ' ');
        if (body->parent < 0ll)
        {
            mmgr_line_put(line, "     -");
        }
        else
        {
            // a parent index is non-negative here
            mmgr_line_decimal_columns(line, (unsigned long long)body->parent, 6u);
        }
        mmgr_line_ch(line, ' ');
        mmgr_line_decimal_columns(line, body->born, 4u);
        mmgr_line_ch(line, ' ');
        mmgr_line_decimal_columns(line, body->ended, 5u);
        mmgr_line_put(line, "  ");
        for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
        {
            lattice_print_rational(line, field[1u + axis], field[0]);
            mmgr_line_put(line, (axis + 1u < SIM_AXES) ? ", " : "");
        }
        mmgr_line_put(line, "   ");
        mmgr_line_decimal(line, field[0], 1u);
        mmgr_line_ch(line, '\n');
    }
}

static int lattice_write_npy(const char *path, const SimScene *scene, const unsigned short *lanes)
{
    char head[128];
    VerbaLinea line;
    line.out = head;
    line.capacity = sizeof(head);
    line.at = 0ull;
    mmgr_line_put(&line, "{'descr': '<u2', 'fortran_order': False, 'shape': (");
    mmgr_line_decimal(&line, scene->frames, 1u);
    for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
    {
        mmgr_line_put(&line, ", ");
        mmgr_line_decimal(&line, scene->extent[axis], 1u);
    }
    mmgr_line_put(&line, "), }");
    while (((10ull + line.at + 1ull) % 64ull) != 0ull)
    {
        mmgr_line_ch(&line, ' ');
    }
    mmgr_line_ch(&line, '\n');
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const unsigned char magic[10] = {0x93u, 'N', 'U', 'M', 'P', 'Y', 1u, 0u,
                                     // the header length is below 128, one byte, stored little endian
                                     (unsigned char)(line.at & 0xFFull), (unsigned char)(line.at >> 8u)};
    const unsigned long long lanes_count = scene->frames * sim_scene_voxels(scene);
    int ok = fwrite(magic, 1u, sizeof(magic), file) == sizeof(magic);
    ok = ok && (fwrite(head, 1u, (size_t)line.at, file) == (size_t)line.at);
    ok = ok && (fwrite(lanes, sizeof(unsigned short), (size_t)lanes_count, file) == (size_t)lanes_count);
    ok = (fclose(file) == 0) && ok;
    return ok;
}

static int lattice_write_truth(const char *path, const SimScene *scene, const unsigned long long *truth)
{
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    char text_buffer[256];
    VerbaLinea line;
    line.out = text_buffer;
    line.capacity = sizeof(text_buffer);
    line.at = 0ull;
    mmgr_line_put(&line, "frame\tbody\tparent\tmass\tsum_z\tsum_y\tsum_x\n");
    int ok = (mmgr_line_ok(&line) && (fwrite(line.out, 1u, mmgr_line_finish(&line), file) == line.at)) != 0;
    for (unsigned long long frame = 0ull; ok && (frame < scene->frames); frame += 1ull)
    {
        for (unsigned int index = 0u; ok && (index < scene->bodies); index += 1u)
        {
            const unsigned long long *const field = &truth[((frame * scene->bodies) + index) * SIM_TRUTH_FIELDS];
            if (field[0] == 0ull)
            {
                continue;
            }
            line.at = 0ull;
            mmgr_line_decimal(&line, frame, 1u);
            mmgr_line_ch(&line, '\t');
            mmgr_line_decimal(&line, index, 1u);
            mmgr_line_ch(&line, '\t');
            mmgr_line_decimal_signed(&line, scene->body[index].parent);
            for (unsigned int word = 0u; word < SIM_TRUTH_FIELDS; word += 1u)
            {
                mmgr_line_ch(&line, '\t');
                mmgr_line_decimal(&line, field[word], 1u);
            }
            mmgr_line_ch(&line, '\n');
            ok = (mmgr_line_ok(&line) && (fwrite(line.out, 1u, mmgr_line_finish(&line), file) == line.at)) != 0;
        }
    }
    ok = (fclose(file) == 0) && ok;
    return ok;
}

static void lattice_moments(SimResults *results, const SimScene *scene, const SimCamera *camera,
                            const unsigned short *lanes, const unsigned int *signal)
{
    const unsigned long long voxels = sim_scene_voxels(scene);
    const unsigned long long lanes_count = scene->frames * voxels;
    long long first = 0ll;
    unsigned long long second = 0ull;
    unsigned long long expected = 0ull;
    for (unsigned long long index = 0ull; index < lanes_count; index += 1ull)
    {
        const unsigned long long voxel = index % voxels;
        // a lane, its pattern, offset and gained signal are each far below 2^31
        const long long residual = (long long)lanes[index] - (long long)camera->offset -
                                   (long long)sim_pattern(camera, voxel) -
                                   ((long long)camera->gain * (long long)signal[index]);
        first += residual;
        // a residual's magnitude is far below 2^31: its square fits 64 bits
        second += (unsigned long long)(residual * residual);
        expected += (camera->gain * camera->gain * signal[index]) + camera->read_square;
    }
    // the first moment's magnitude is below 2^31 in this scene: its square fits 64 bits
    const unsigned long long first_square = (unsigned long long)(first * first);
    const unsigned long long apart = (second > expected) ? (second - expected) : (expected - second);
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "  camera law over ");
    mmgr_line_decimal(line, lanes_count, 1u);
    mmgr_line_put(line, " lanes: gain ");
    mmgr_line_decimal(line, camera->gain, 1u);
    mmgr_line_put(line, ", read variance ");
    mmgr_line_decimal(line, camera->read_square, 1u);
    mmgr_line_put(line, ", offset ");
    mmgr_line_decimal(line, camera->offset, 1u);
    mmgr_line_put(line, ", fixed pattern 0 to ");
    mmgr_line_decimal(line, camera->pattern_range, 1u);
    mmgr_line_ch(line, '\n');
    mmgr_line_put(line, "    sum of residuals ");
    mmgr_line_decimal_signed(line, first);
    mmgr_line_put(line, " against a spread of sqrt(");
    mmgr_line_decimal(line, expected, 1u);
    mmgr_line_put(line, ")\n    sum of squared residuals ");
    mmgr_line_decimal(line, second, 1u);
    mmgr_line_put(line, " against the law's ");
    mmgr_line_decimal(line, expected, 1u);
    mmgr_line_put(line, " (ratio ");
    sim_fraction_print(line, second, expected, 5u);
    mmgr_line_put(line, ")\n");
    sim_check(results, first_square <= (LATTICE_SIGMAS_SQUARED * expected),
              "the first moment lies within 5 sigma of 0");
    sim_check(results, (LATTICE_SECOND_MOMENT_PARTS * apart) <= expected,
              "the second moment lies within 1% of the law");
}

int main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    const char *out = NULL;
    for (int argument = 1; argument + 1 < count; argument += 1)
    {
        if (strcmp(arguments[argument], "--out") == 0)
        {
            out = arguments[argument + 1];
        }
    }

    SimBody body[LATTICE_BODIES];
    memset(body, 0, sizeof(body));
    SimScene scene;
    lattice_scene(&scene, body);
    SimCamera camera;
    lattice_camera(&camera);
    const unsigned long long voxels = sim_scene_voxels(&scene);
    const unsigned long long lanes_count = scene.frames * voxels;
    const unsigned long long truth_words = scene.frames * scene.bodies * SIM_TRUTH_FIELDS;

    unsigned short *lanes = (unsigned short *)malloc((size_t)lanes_count * sizeof(unsigned short));
    unsigned short *again = (unsigned short *)malloc((size_t)lanes_count * sizeof(unsigned short));
    unsigned int *signal = (unsigned int *)malloc((size_t)lanes_count * sizeof(unsigned int));
    unsigned long long *truth = (unsigned long long *)malloc((size_t)truth_words * sizeof(unsigned long long));
    unsigned long long *reference = (unsigned long long *)malloc((size_t)truth_words * sizeof(unsigned long long));
    unsigned short *device_lanes = NULL;
    unsigned int *device_signal = NULL;
    int ok = (lanes != NULL) && (again != NULL) && (signal != NULL) && (truth != NULL) && (reference != NULL);
    sim_check(&results, ok, "host buffers");
    ok = ok && sim_job_submit(&results, "nbody_lattice", count, arguments,
                              lanes_count * (sizeof(unsigned short) + sizeof(unsigned int)));
    ok = ok &&
         sim_status_check(&results, cudaMalloc((void **)&device_lanes, lanes_count * sizeof(unsigned short)), "lanes");
    ok = ok &&
         sim_status_check(&results, cudaMalloc((void **)&device_signal, lanes_count * sizeof(unsigned int)), "signal");

    unsigned long long clipped = 0ull;
    ok = ok && sim_render(&results, &scene, &camera, device_lanes, device_signal, truth, &clipped);
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(lanes, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                          "lanes read");
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(signal, device_signal, lanes_count * sizeof(unsigned int), cudaMemcpyDeviceToHost),
                          "signal read");
    if (ok)
    {
        sim_truth_host(&scene, reference);
        lattice_print_scene(&results, &scene, truth);
        sim_check(&results, memcmp(truth, reference, (size_t)truth_words * sizeof(unsigned long long)) == 0,
                  "every body's device truth equals the host walk of its box");
        unsigned long long signal_differ = 0ull;
        for (unsigned long long index = 0ull; index < lanes_count; index += 1ull)
        {
            const unsigned long long voxel = index % voxels;
            long long place[SIM_AXES];
            // each coordinate is below its extent
            place[0] = (long long)(voxel / (scene.extent[1] * scene.extent[2]));
            place[1] = (long long)((voxel / scene.extent[2]) % scene.extent[1]);
            place[2] = (long long)(voxel % scene.extent[2]);
            signal_differ += (sim_signal(&scene, index / voxels, place) != signal[index]) ? 1ull : 0ull;
        }
        mmgr_line_put(&results.line, "  the device signal against the host's at every lane: ");
        mmgr_line_decimal(&results.line, signal_differ, 1u);
        mmgr_line_put(&results.line, " differ\n");
        sim_check(&results, signal_differ == 0ull, "the device signal equals the host's at every lane");
        sim_check(&results, clipped == 0ull, "no lane clipped to the u16 range");
        unsigned long long born_seen = 0ull;
        for (unsigned int index = LATTICE_FOUNDERS; index < scene.bodies; index += 1u)
        {
            born_seen += (truth[((body[index].born * scene.bodies) + index) * SIM_TRUTH_FIELDS] != 0ull) ? 1ull : 0ull;
        }
        sim_check(&results, born_seen == (2ull * LATTICE_DIVISIONS), "every daughter is in view at its birth");
        lattice_moments(&results, &scene, &camera, lanes, signal);
    }

    SimCamera quiet;
    memset(&quiet, 0, sizeof(quiet));
    quiet.gain = 1ull;
    ok = ok && sim_render(&results, &scene, &quiet, device_lanes, NULL, NULL, &clipped);
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(again, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                          "quiet lanes read");
    if (ok)
    {
        unsigned long long quiet_differ = 0ull;
        for (unsigned long long index = 0ull; index < lanes_count; index += 1ull)
        {
            quiet_differ += (again[index] != signal[index]) ? 1ull : 0ull;
        }
        sim_check(&results, quiet_differ == 0ull, "a noiseless camera renders the signal exactly");
    }
    ok = ok && sim_render(&results, &scene, &camera, device_lanes, NULL, NULL, &clipped);
    ok = ok &&
         sim_status_check(&results,
                          cudaMemcpy(again, device_lanes, lanes_count * sizeof(unsigned short), cudaMemcpyDeviceToHost),
                          "second render read");
    if (ok)
    {
        sim_check(&results, memcmp(lanes, again, (size_t)lanes_count * sizeof(unsigned short)) == 0,
                  "a second render of the same key is identical");
    }
    if (ok && (out != NULL))
    {
        char path[1024];
        VerbaLinea name;
        name.out = path;
        name.capacity = sizeof(path);
        name.at = 0ull;
        mmgr_line_put(&name, out);
        mmgr_line_put(&name, "/lattice.npy");
        mmgr_line_finish(&name);
        sim_check(&results, lattice_write_npy(path, &scene, lanes), "lattice.npy written");
        name.at = 0ull;
        mmgr_line_put(&name, out);
        mmgr_line_put(&name, "/truth.tsv");
        mmgr_line_finish(&name);
        sim_check(&results, lattice_write_truth(path, &scene, truth), "truth.tsv written");
        mmgr_line_put(&results.line, "  wrote lattice.npy and truth.tsv to ");
        mmgr_line_put(&results.line, out);
        mmgr_line_ch(&results.line, '\n');
    }

    cudaFree(device_signal);
    cudaFree(device_lanes);
    free(reference);
    free(truth);
    free(signal);
    free(again);
    free(lanes);
    return sim_close(&results, "nbody lattice");
}
