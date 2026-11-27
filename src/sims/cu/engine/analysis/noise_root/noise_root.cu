// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// The root noise of an object's box. Each term the noise vector table names as shared is planted in turn at three
// strengths over a scene containing one located body; the noise detector prices the body's box and every term's yank
// through the tower and compression's coder and names the root, the term whose yank saves the max bits. The null
// names none; a planted term is the root or there is none, never another term; at the strongest each is the root; and
// every yank returns the box exactly.
#include "../../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "sim_camera.h"

#include "../../../../../cu/engine/analysis/compression/compression.h"
#include "../../../../../cu/engine/analysis/noise_detector/noise_detector.h"
#include "../../../../../cu/engine/analysis/tower/tower.h"

#define ROOT_KEY 0x524F4F544E4FULL

#define ROOT_PIXEL_PURPOSE 0x504958454CULL

#define ROOT_STACK_PURPOSE 0x535441434BULL

// 16 frames of 16 x 64 x 64
#define ROOT_FRAMES 16ULL

#define ROOT_DEPTH 16ULL

#define ROOT_HEIGHT 64ULL

#define ROOT_WIDTH 64ULL

// an offset of 300 and 100 electrons at gain 1 with a read variance of 4: a background voxel's independent variance is
// 104 and a body voxel's 404, and the offset confines the strongest planted term to the lane
#define ROOT_OFFSET 300ULL

#define ROOT_BACKGROUND 100ULL

#define ROOT_READ_SQUARE 4ULL

// each term's planted variances, the strongest last, where the noise terms sim plants 4
#define ROOT_STRENGTHS 3u

static const unsigned long long ROOT_SQUARES[ROOT_STRENGTHS] = {64ULL, 256ULL, 1024ULL};

// the object: one body located at the view's middle, 300 electrons over the radii 3, 7 and 7
#define ROOT_BRIGHTNESS 300ULL

// the null, then each term planted alone at each strength
#define ROOT_RUNS ((NOISE_ROOT_TERMS * ROOT_STRENGTHS) + 1u)

static const unsigned long long ROOT_EXTENT[4] = {ROOT_FRAMES, ROOT_DEPTH, ROOT_HEIGHT, ROOT_WIDTH};

// the object's box: every frame, z 2 to 13, y and x 16 to 47
static const unsigned long long ROOT_LOW[4] = {0ULL, 2ULL, 16ULL, 16ULL};

static const unsigned long long ROOT_HIGH[4] = {ROOT_FRAMES, 14ULL, 48ULL, 48ULL};

// the terms by NOISE_ROOT_*, then none
static const char *const ROOT_NAMES[NOISE_ROOT_TERMS + 1u] = {"rows", "columns", "planes", "pixels", "stacks", "none"};

// The crystal's price of a lattice of ints: lifted through the tower and coded, the coder's bits. Each lattice is
// copied to the device on its own; the tower's and the coder's pools are sized to the box's, the largest.
static long root_cost(const int *values, const unsigned long long extent[4], unsigned long long *bits,
                      EngineError *error)
{
    const unsigned long long lanes = extent[0] * extent[1] * extent[2] * extent[3];
    int *device_values = NULL;
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    int ok = (cudaMalloc((void **)&device_values, (size_t)lanes * sizeof(int)) == cudaSuccess) &&
             (cudaMemcpy(device_values, values, (size_t)lanes * sizeof(int), cudaMemcpyHostToDevice) == cudaSuccess);
    if (ok)
    {
        TowerLiftRequest lift;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &lift, .bytes = sizeof(lift), .val = 0);
        lift.device_values = device_values;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = lift.extent, .src = extent, .bytes = sizeof(lift.extent));
        lift.coefficients = &coefficients;
        lift.scratch = &scratch;
        lift.floors = &floors;
        lift.error = error;
        ok = tower_lift(&lift) == 0L;
    }
    if (ok)
    {
        unsigned long long chunks = 0ULL;
        const unsigned long long *offsets = NULL;
        const unsigned int *stream = NULL;
        CompressionEncodeRequest code;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &code, .bytes = sizeof(code), .val = 0);
        code.device_coefficients = coefficients;
        code.count = lanes;
        code.device_scratch = scratch;
        code.chunks = &chunks;
        code.bits = bits;
        code.offsets = &offsets;
        code.stream = &stream;
        code.error = error;
        ok = compression_encode(&code) == 0L;
    }
    cudaFree(device_values);
    return ok ? 0L : -1L;
}

static void root_camera(SimCamera *camera, unsigned int planted, unsigned long long square)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = camera, .bytes = sizeof(*camera), .val = 0);
    camera->key = ROOT_KEY;
    camera->offset = ROOT_OFFSET;
    camera->gain = 1ULL;
    camera->read_square = ROOT_READ_SQUARE;
    camera->shot = SIM_SHOT_POISSON;
    camera->row_square = (planted == NOISE_ROOT_ROWS) ? square : 0ULL;
    camera->column_square = (planted == NOISE_ROOT_COLUMNS) ? square : 0ULL;
    camera->plane_square = (planted == NOISE_ROOT_PLANES) ? square : 0ULL;
}

// The terms the camera law does not draw, added on the host: a value for each camera pixel, (y, x), shared by every
// frame and z plane, or a value for each frame and pixel, (t, y, x), shared by every z plane of the stack. Returns the
// voxel-frames the sum left outside the lane, clamped at its ends.
static unsigned long long root_plant(unsigned short *lanes, unsigned int planted, unsigned long long square)
{
    const unsigned long long plane = ROOT_HEIGHT * ROOT_WIDTH;
    const unsigned long long purpose = (planted == NOISE_ROOT_PIXELS) ? ROOT_PIXEL_PURPOSE : ROOT_STACK_PURPOSE;
    unsigned long long clipped = 0ULL;
    unsigned long long at = 0ULL;
    for (unsigned long long frame = 0ULL; frame < ROOT_FRAMES; frame += 1ULL)
    {
        for (unsigned long long z = 0ULL; z < ROOT_DEPTH; z += 1ULL)
        {
            for (unsigned long long pixel = 0ULL; pixel < plane; pixel += 1ULL)
            {
                const unsigned long long counter = (planted == NOISE_ROOT_PIXELS) ? pixel : ((frame * plane) + pixel);
                const long long value = (long long)lanes[at] + sim_centerd(ROOT_KEY ^ purpose, counter, square);
                clipped += ((value < 0LL) || (value > SIM_LANE_MAX)) ? 1ULL : 0ULL;
                const long long clamped = (value < 0LL) ? 0LL : ((value > SIM_LANE_MAX) ? SIM_LANE_MAX : value);
                // the value is clamped to the u16 lane's range just above
                lanes[at] = (unsigned short)clamped;
                at += 1ULL;
            }
        }
    }
    return clipped;
}

// the box's voxels read from the scene into ints, frames, z, y and x, as the detector reads them
static void root_box_take(const unsigned short *lanes, int *values)
{
    unsigned long long at = 0ULL;
    for (unsigned long long frame = ROOT_LOW[0]; frame < ROOT_HIGH[0]; frame += 1ULL)
    {
        for (unsigned long long z = ROOT_LOW[1]; z < ROOT_HIGH[1]; z += 1ULL)
        {
            for (unsigned long long y = ROOT_LOW[2]; y < ROOT_HIGH[2]; y += 1ULL)
            {
                for (unsigned long long x = ROOT_LOW[3]; x < ROOT_HIGH[3]; x += 1ULL)
                {
                    values[at] = lanes[(((((frame * ROOT_DEPTH) + z) * ROOT_HEIGHT) + y) * ROOT_WIDTH) + x];
                    at += 1ULL;
                }
            }
        }
    }
}

// a signed value right-aligned in `columns`
static void root_signed_columns(VerbaLinea *line, long long value, unsigned int columns)
{
    // the magnitude of a negative value is its two's complement negation, taken unsigned
    unsigned long long magnitude = (value < 0LL) ? (0ULL - (unsigned long long)value) : (unsigned long long)value;
    unsigned int width = (value < 0LL) ? 2u : 1u;
    while (magnitude >= 10ULL)
    {
        magnitude /= 10ULL;
        width += 1u;
    }
    for (unsigned int pad = width; pad < columns; pad += 1u)
    {
        mmgr_line_ch(line, ' ');
    }
    mmgr_line_decimal_signed(line, value);
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    const unsigned long long count = ROOT_FRAMES * ROOT_DEPTH * ROOT_HEIGHT * ROOT_WIDTH;
    const unsigned long long lane_bytes = count * sizeof(unsigned short);
    unsigned long long box[4];
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        box[axis] = ROOT_HIGH[axis] - ROOT_LOW[axis];
    }
    const unsigned long long voxels = box[0] * box[1] * box[2] * box[3];
    // the scene's lanes, a priced lattice's copy, and the tower's and the coder's pools for the box
    const unsigned long long declared =
        lane_bytes + (voxels * sizeof(int)) + tower_reserve_bytes(voxels) + compression_reserve_bytes(voxels);
    int ok = sim_job_submit(&results, "noise_root", 0, NULL, declared);
    unsigned short *const lanes =
        (unsigned short *)platform_cell_taken((size_t)lane_bytes, platform_place_g_program_owner, 0);
    int *const values = (int *)platform_cell_taken((unsigned long long)((size_t)voxels * sizeof(int)),
                                                   (unsigned int)platform_place_g_program_owner, 0);
    int *const residual = (int *)platform_cell_taken((size_t)voxels * sizeof(int), platform_place_g_program_owner, 0);
    int *const pattern = (int *)platform_cell_taken((size_t)voxels * sizeof(int), platform_place_g_program_owner, 0);
    int *const returned = (int *)platform_cell_taken((size_t)voxels * sizeof(int), platform_place_g_program_owner, 0);
    unsigned short *device_lanes = NULL;
    ok = ok && (lanes != NULL) && (values != NULL) && (residual != NULL) && (pattern != NULL) && (returned != NULL) &&
         sim_status_check(&results, cudaMalloc((void **)&device_lanes, (size_t)lane_bytes), "lanes");
    sim_check(&results, ok, "the scene's buffers");

    SimBody body;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &body, .bytes = sizeof(body), .val = 0);
    body.center[0] = 8LL;
    body.center[1] = 32LL;
    body.center[2] = 32LL;
    body.range[0] = 3LL;
    body.range[1] = 7LL;
    body.range[2] = 7LL;
    body.brightness = ROOT_BRIGHTNESS;
    body.ended = ROOT_FRAMES;
    body.parent = -1LL;
    SimScene scene;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &scene, .bytes = sizeof(scene), .val = 0);
    scene.frames = ROOT_FRAMES;
    scene.extent[0] = ROOT_DEPTH;
    scene.extent[1] = ROOT_HEIGHT;
    scene.extent[2] = ROOT_WIDTH;
    scene.background = ROOT_BACKGROUND;
    scene.bodies = 1u;
    scene.body = &body;

    mmgr_line_put(line,
                  "  the root noise of an object's box: 16 frames x z 2 to 13 x y 16 to 47 x x 16 to 47 around one"
                  " located body of 300 e, over 100 e at offset 300, read variance 4, a Poisson shot\n");
    mmgr_line_put(line,
                  "  each term planted alone at variances 64, 256 and 1024, against a background voxel's independent"
                  " 104 and a body voxel's 404; a term's saving is the box's coded bits less its residual's and"
                  " its pattern's, through the tower and compression's coder\n");
    mmgr_line_put(line,
                  "  planted  variance    box bits      rows   columns    planes    pixels    stacks  root     per"
                  " mille  return\n");
    sim_flush(&results);
    for (unsigned int run = 0u; ok && (run < ROOT_RUNS); run += 1u)
    {
        const unsigned int planted = (run == 0u) ? NOISE_ROOT_TERMS : ((run - 1u) / ROOT_STRENGTHS);
        const unsigned int strength = (run == 0u) ? 0u : ((run - 1u) % ROOT_STRENGTHS);
        const unsigned long long square = (run == 0u) ? 0ULL : ROOT_SQUARES[strength];
        SimCamera camera;
        root_camera(&camera, planted, square);
        unsigned long long clipped = 0ULL;
        ok = sim_render(&results, &scene, &camera, device_lanes, NULL, NULL, &clipped) &&
             sim_status_check(&results, cudaMemcpy(lanes, device_lanes, (size_t)lane_bytes, cudaMemcpyDeviceToHost),
                              "lanes read");
        if (ok && ((planted == NOISE_ROOT_PIXELS) || (planted == NOISE_ROOT_STACKS)))
        {
            clipped += root_plant(lanes, planted, square);
        }
        sim_check(&results, ok && (clipped == 0ULL), "the scene rendered with no lane clipped");
        if (ok == 0)
        {
            break;
        }
        root_box_take(lanes, values);
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        NoiseRootMeasurement measurement;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &measurement, .bytes = sizeof(measurement), .val = 0);
        NoiseRootRequest request;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
        request.volume = lanes;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.extent, .src = ROOT_EXTENT, .bytes = sizeof(request.extent));
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.low, .src = ROOT_LOW, .bytes = sizeof(request.low));
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.high, .src = ROOT_HIGH, .bytes = sizeof(request.high));
        request.cost = root_cost;
        request.measurement = &measurement;
        request.residual = residual;
        request.pattern = pattern;
        request.error = &error;
        ok = noise_root_box(&request) == 0L;
        sim_check(&results, ok, "the detector priced the box and every term's yank");
        if (ok == 0)
        {
            break;
        }
        int exact = 0;
        if (measurement.root < NOISE_ROOT_TERMS)
        {
            NoiseReturnRequest back;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &back, .bytes = sizeof(back), .val = 0);
            back.residual = residual;
            back.pattern = pattern;
            back.term = measurement.root;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = back.box, .src = box, .bytes = sizeof(back.box));
            back.values = returned;
            back.error = &error;
            exact =
                (noise_root_return(&back) == 0L) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = returned, .other = values,
                                                                .bytes = (size_t)voxels * sizeof(int)) == 0);
        }
        else
        {
            exact = EMBED_CALL(memor.cmp, MemoriaCfg, .src = residual, .other = values,
                               .bytes = (size_t)voxels * sizeof(int)) == 0;
        }
        mmgr_line_put(line, "  ");
        mmgr_line_put_columns(line, ROOT_NAMES[planted], 7u);
        mmgr_line_decimal_columns(line, square, 10u);
        mmgr_line_decimal_columns(line, measurement.box_bits, 12u);
        for (unsigned int term = 0u; term < NOISE_ROOT_TERMS; term += 1u)
        {
            root_signed_columns(line, measurement.saved[term], 10u);
        }
        mmgr_line_put(line, "  ");
        mmgr_line_put_columns(line, ROOT_NAMES[measurement.root], 7u);
        // a root saves more than 0 and less than the box's bits, which are a few million here: the box's bits
        // convert to long long exactly and 1000 times the saving is far below 2^63
        const long long per_mille =
            (measurement.root < NOISE_ROOT_TERMS)
                ? ((1000LL * measurement.saved[measurement.root]) / (long long)measurement.box_bits)
                : 0LL;
        root_signed_columns(line, per_mille, 11u);
        mmgr_line_put(line, (exact != 0) ? "  exact\n" : "  differs\n");
        sim_flush(&results);
        if (planted == NOISE_ROOT_TERMS)
        {
            sim_check(&results, measurement.root == NOISE_ROOT_TERMS, "the null names no root");
        }
        else
        {
            sim_check(&results, (measurement.root == planted) || (measurement.root == NOISE_ROOT_TERMS),
                      "the root is the planted term or none, never another term");
        }
        if ((planted != NOISE_ROOT_TERMS) && (strength == (ROOT_STRENGTHS - 1u)))
        {
            sim_check(&results, measurement.root == planted, "at the strongest, the root is the planted term");
        }
        // the residual is the box less the pattern in ints: the sum is the box by construction. What this checks
        // is that noise_root_return spreads the pattern over the places the yank took it from
        sim_check(&results, exact != 0, "the root's residual and pattern return the box exactly");
    }
    cudaFree(device_lanes);
    platform_cell_released(lanes, platform_place_g_program_owner);
    platform_cell_released(values, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(residual, platform_place_g_program_owner);
    platform_cell_released(pattern, platform_place_g_program_owner);
    platform_cell_released(returned, platform_place_g_program_owner);
    return sim_close(&results, "noise root");
}
