// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../src/cu/engine/nbody/contact_side/contact_side.h"
#include "crc.h"
#include "../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../src/cu/engine/runtime/device_pool/device_pool.h"
#include "../../../../src/cu/engine/nbody/division/division.h"
#include "../../../../src/cu/engine/engine.h"
#include "faces.h"
#include "../../../../src/cu/engine/nbody/fingerprint/fingerprint.h"
#include "../../../../src/cu/engine/nbody/flatten/flatten.h"
#include "link_objects.h"
#include "../../../../src/cu/engine/analysis/noise_detector/noise_detector.h"
#include "../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "output.h"
#include "../../../../src/cu/engine/nbody/print_pair/print_pair.h"
#include "../../../../src/cu/engine/analysis/residual_survey/residual_survey.h"
#include "run_cfg.h"
#include "run_log.h"
#include "scan.h"
#include "../../../../src/cu/engine/runtime/schedule/schedule.h"
#include "score_sample.h"
#include "../../../../src/cu/engine/analysis/shift_agreement/shift_agreement.h"
#include "sort.h"
#include "../../../../src/cu/engine/runtime/daemon/tessera.h"
#include "../../../../src/cu/engine/analysis/tower/tower.h"
#include "track.h"
#include "../../../../src/cu/engine/nbody/velocity/velocity.h"
#include "../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned int s_ingest = 0u;

typedef enum
{
    RUN_SCHEDULE = 0,
    RUN_IAPX_PROVE = 1,
    RUN_ENTROPY = 2,
    RUN_FLOOR = 3,
    RUN_FLATTEN = 4,
    RUN_TRACK = 5,
    RUN_FINGERPRINT = 6,
    RUN_SCAN = 7,
    RUN_FLICKER = 8,
    RUN_LINES = 9,
    RUN_NEIGHBORS = 10,
    RUN_CLIPS = 11,
    RUN_PIXELS = 12,
    RUN_MOMENTS = 13,
    RUN_SORT = 14,
    RUN_OUTPUT = 15,
    RUN_FACES = 16,
    RUN_PARTS = 17
} RunPart;

static const char *const RUN_PART_NAMES[RUN_PARTS] = {
    "schedule", "iapx-prove", "entropy", "floor",  "flatten", "track", "fingerprint", "scan", "flicker",
    "lines",    "neighbors",  "clips",   "pixels", "moments", "sort",  "output",      "faces"};

// a host part allocates nothing on the device and requests no job from the daemon: the output reads .points and .links
// and writes rows, and the faces read .points, .drift, .shape and .links and write each sample's .faces
static int run_part_host(RunPart part)
{
    return (part == RUN_OUTPUT) || (part == RUN_FACES);
}

// the parse errors on a part named twice, and the floor the .cfg sets is added only when it is not named. The parts
// are distinct and at max RUN_PARTS
static RunPart s_parts[RUN_PARTS];

static unsigned int s_part_count = 0u;

// the noise detector's loader: a sample's crystal from the set, its root taken and set aside
static long track_noise_load(const char *set, const char *sample, unsigned long long extent[4], unsigned short **volume,
                             EngineError *error)
{
    EngineSignum root;
    return engine_iapx_load(set, sample, extent, volume, &root, NULL, error);
}

static int run_part_named(const char *name, RunPart *part)
{
    for (unsigned int each = 0u; each < RUN_PARTS; each += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = RUN_PART_NAMES[each], .ci = EMBED_FALSE))
        {
            *part = (RunPart)each;
            return 1;
        }
    }
    return 0;
}

#define RUN_JOB_GRACE_MICROSECONDS 2000000ull
#define RUN_JOB_SWEEP_MICROSECONDS 20000ull
#define RUN_JOB_IDLE_MICROSECONDS 5000000ull

static unsigned int s_override = 0u;

// `total` is the declaration with the bytes the process already allocated on the device as it requested, its context
// among them, which the daemon measured and reported on admission
typedef struct
{
    TesseraClient *client;
    TesseraTicket ticket;
    unsigned long long declared;
    unsigned long long total;
} RunJob;

// what a part allocates on the device for its largest lattice of `lanes`: the lattice in 16-bit lanes, as one
// allocation rounded to the page, where the part allocates it there (`lattice`); and where it lifts or lowers its
// lattices (`tower`), the tower's pool and compression's chunk pool, each retained for the largest lattice the job has
// requested. The stream, sized by the values, and the seal's passing buffers are left to the peak the daemon tracks.
static unsigned long long run_job_lattice_bytes(unsigned long long lanes, int lattice, int tower)
{
    DevicePoolPlan plan = {0ull, 0ull, 0};
    device_pool_plan_slice(&plan, (lattice != 0) ? (lanes * sizeof(unsigned short)) : 0ull);
    const unsigned long long lowered =
        (tower != 0) ? (tower_reserve_bytes(lanes) + compression_reserve_bytes(lanes)) : 0ull;
    return device_pool_plan_bytes(&plan) + lowered;
}

static unsigned long long run_job_declared(const RunInputs *inputs, const char *name, int lattice, int tower, int drift,
                                           int gate)
{
    // the largest sample's holdings; the daemon measures the rest and tracks the peak under the signum. A part that
    // drifts (`drift`) also allocates what shift_agreement retains for a sample's frame, its extents in the order the
    // drift passes them. Those bytes step with the padded lengths and not the voxels. The max over the samples is
    // taken. A part that gates (`gate`) allocates the gate's pool for the max points a frame over the samples' .points
    unsigned long long max = 0ull;
    unsigned long long retained = 0ull;
    for (unsigned int at = 0u; at < inputs->count; at += 1u)
    {
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        unsigned long long lanes = 0ull;
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        if (s_ingest != 0u)
        {
            if (engine_source_lanes(inputs->source, inputs->samples[at], &lanes, &error) != 0L)
            {
                fprintf(stderr, "  tessera: %s: the source of %s could not be described\n", name, inputs->samples[at]);
                return 0ull;
            }
        }
        else if (engine_iapx_head(inputs->set, inputs->samples[at], extent, &error) == 0L)
        {
            lanes = extent[0] * extent[1] * extent[2] * extent[3];
            // an extent past 32 bits reads as 0, which shift_agreement errors and retains nothing for
            unsigned int extents[ENGINE_AXES];
            for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
            {
                extents[axis] = (extent[axis + 1u] <= 0xFFFFFFFFull) ? (unsigned int)extent[axis + 1u] : 0u;
            }
            const unsigned long long reserve_bytes =
                (drift != 0) ? shift_agreement_reserve_bytes(ENGINE_AXES, extents) : 0ull;
            retained = (reserve_bytes > retained) ? reserve_bytes : retained;
        }
        max = (lanes > max) ? lanes : max;
    }
    const unsigned long long lattice_bytes = run_job_lattice_bytes(max, lattice, tower);
    unsigned int points = 0u;
    if ((gate != 0) && (sort_points_max(inputs->set, inputs->samples, inputs->count, &points) == 0))
    {
        fprintf(stderr, "  tessera: %s: the max points a frame could not be read\n", name);
        return 0ull;
    }
    const unsigned long long gated = (gate != 0) ? sort_reserve_bytes(points) : 0ull;
    if (drift != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " declares ");
        mmgr_line_decimal(output_line, lattice_bytes + retained, 1u);
        mmgr_line_put(output_line, " bytes: ");
        mmgr_line_decimal(output_line, lattice_bytes, 1u);
        mmgr_line_put(output_line, " for its lattice and tower, ");
        mmgr_line_decimal(output_line, retained, 1u);
        mmgr_line_put(output_line, " the drift retains");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (gate != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " declares ");
        mmgr_line_decimal(output_line, lattice_bytes + gated, 1u);
        mmgr_line_put(output_line, " bytes: ");
        mmgr_line_decimal(output_line, lattice_bytes, 1u);
        mmgr_line_put(output_line, " for its lattice and tower, ");
        mmgr_line_decimal(output_line, gated, 1u);
        mmgr_line_put(output_line, " the gate allocates for ");
        mmgr_line_decimal(output_line, points, 1u);
        mmgr_line_put(output_line, " points a frame");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return lattice_bytes + retained + gated;
}

static int run_job_submit(const char *name, const CfgText *effective, const RunInputs *inputs, int lattice, int tower,
                          int drift, int gate, RunJob *job)
{
    // every part is one job on the device's tessera daemon, keyed by the part and the whole effective request
    EMBED_CALL(memor.set, MemoriaCfg, .dst = job, .bytes = sizeof(*job), .val = 0);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    int device = 0;
    cudaDeviceProp properties;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaGetDeviceProperties(&properties, device) != cudaSuccess))
    {
        fprintf(stderr, "  tessera: %s: the device did not answer who it is\n", name);
        return 0;
    }
    TesseraJobSubmit submit;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &submit, .bytes = sizeof(submit), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = submit.device, .src = properties.uuid.bytes,
               .bytes = TESSERA_DEVICE_BYTES);
#if defined(_WIN32)
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &submit.luid, .src = properties.luid, .bytes = sizeof(submit.luid));
#endif
    const size_t named = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u;
    const size_t request_bytes = named + effective->length;
    unsigned char *const request =
        (unsigned char *)platform_cell_taken(request_bytes, platform_place_g_program_owner, 0);
    if (request == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request, .src = name, .bytes = named);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request + named, .src = effective->bytes, .bytes = effective->length);
    const ObsignatioSignumRequest signum = {
        request, request_bytes, NULL, OBSIGNATIO_MODE_HASH, submit.signum.bytes, ENGINE_SIGNUM_BYTES, &error};
    const long hashed = obsignatio_signum(&signum);
    platform_cell_released(request, platform_place_g_program_owner);
    job->declared = run_job_declared(inputs, name, lattice, tower, drift, gate);
    char daemon[ENGINE_PATH_CAPACITY];
    const int placed = engine_program_directory(daemon, sizeof(daemon)) &&
                       (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = daemon, .cap = PLATFORM_TEXT_TO_TERMINATOR) +
                            sizeof("/tessera_daemon.exe") <=
                        sizeof(daemon));
    if ((hashed != 0L) || (job->declared == 0ull) || !placed)
    {
        fprintf(stderr, "  tessera: %s: no job could be requested (signum, declaration or the daemon's place)\n", name);
        return 0;
    }
    // the name of the daemon appended where the directory ends, within the room checked above
    const size_t held = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = daemon, .cap = PLATFORM_TEXT_TO_TERMINATOR);
#if defined(_WIN32)
    (void)EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = "\\tessera_daemon.exe", .cap = sizeof(daemon) - held,
                     .dst = &daemon[held]);
#else
    (void)EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = "/tessera_daemon", .cap = sizeof(daemon) - held,
                     .dst = &daemon[held]);
#endif
    submit.declared = job->declared;
    submit.grace_microseconds = RUN_JOB_GRACE_MICROSECONDS;
    submit.sweep_microseconds = RUN_JOB_SWEEP_MICROSECONDS;
    submit.idle_microseconds = RUN_JOB_IDLE_MICROSECONDS;
    submit.daemon_path = daemon;
    submit.error = &error;
    if (tessera_job_submit(&submit, &job->client, &job->ticket) != 0L)
    {
        fprintf(stderr, "  tessera: %s: the daemon (%s) did not take the job\n", name, daemon);
        return 0;
    }
    if (job->ticket.over_budget != 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  tessera: ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, " declares ");
            mmgr_line_decimal(output_line, job->declared, 1u);
            mmgr_line_put(output_line, " bytes over its tracked peak of ");
            mmgr_line_decimal(output_line, job->ticket.last_peak, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        const long answered = (s_override != 0u) ? tessera_job_override(job->client, &job->ticket, &error)
                                                 : tessera_job_wait(job->client, &job->ticket, &error);
        if ((answered != 0L) || (job->ticket.lost != 0u))
        {
            fprintf(stderr,
                    "  tessera: %s was reserved past its grace time and lost; --override admits it on its"
                    " declaration (ticket in %s)\n",
                    name, job->ticket.lost_path);
            if (answered == 0L)
            {
                tessera_job_precalc_saved(job->client, &error);
            }
            job->client = NULL;
            return 0;
        }
    }
    job->total = job->declared + job->ticket.located;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " admitted, ");
        mmgr_line_decimal(output_line, job->ticket.granted, 1u);
        mmgr_line_put(output_line, " bytes reserved; it declared ");
        mmgr_line_decimal(output_line, job->declared, 1u);
        mmgr_line_put(output_line, " over the ");
        mmgr_line_decimal(output_line, job->ticket.located, 1u);
        mmgr_line_put(output_line, " its process allocated as it requested");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

static void run_job_release(const char *name, RunJob *job)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if ((job->client == NULL) || (tessera_job_release(job->client, &job->ticket, &error) != 0L))
    {
        fprintf(stderr, "  tessera: %s did not release\n", name);
        return;
    }
    job->client = NULL;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " released, peak ");
        mmgr_line_decimal(output_line, job->ticket.last_peak, 1u);
        mmgr_line_put(output_line, " bytes");
        mmgr_line_put(output_line, (job->ticket.last_peak > job->total) ? ", more than it declared" : "");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

static int run_part_named_already(RunPart part)
{
    for (unsigned int each = 0u; each < s_part_count; each += 1u)
    {
        if (s_parts[each] == part)
        {
            return 1;
        }
    }
    return 0;
}

#define RUN_VOCABULARY_STEPS 12u

#define RUN_VOCABULARY_OUTPUTS 3u

static const EngineRecordStep RUN_VOCABULARY_PROGRAM[RUN_VOCABULARY_STEPS] = {
    {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_MASS, 0u, 0u},
    {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_SUM_Z, 0u, 0u},
    {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_SUM_Y, 0u, 0u},
    {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_MOMENT_ZY, 0u, 0u},
    {ENGINE_RECORD_PRODUCT, 0u, 3u, 0u},
    {ENGINE_RECORD_PRODUCT, 1u, 2u, 0u},
    {ENGINE_RECORD_DIFFERENCE, 4u, 5u, 0u},
    {ENGINE_RECORD_CONSTANT, 0u, 0u, 0u},
    {ENGINE_RECORD_ABSOLUTE, 6u, 0u, 0u},
    {ENGINE_RECORD_COMPARE, 6u, 7u, 0u},
    {ENGINE_RECORD_COMPARE, 8u, 6u, 0u},
    {ENGINE_RECORD_COMPARE, 6u, 8u, 0u}};

static const unsigned int RUN_VOCABULARY_OUTPUT[RUN_VOCABULARY_OUTPUTS] = {9u, 10u, 11u};

static_assert(PRINT_PAIR_BANDS == FINGERPRINT_OUTPUTS,
              "track_driver: the pair program reads every band the print writes");

typedef struct
{
    unsigned int offset[FINGERPRINT_OUTPUTS];
    unsigned int bits[FINGERPRINT_OUTPUTS];
    unsigned int limbs;
} RunPrintFields;

static void run_print_request(const RunInputs *inputs, FingerprintRequest *print)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = print, .bytes = sizeof(*print), .val = 0);
    print->mass_field = MAX_TREE_FIELD_MASS;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        print->sum_field[axis] = MAX_TREE_FIELD_SUM_Z + axis;
        print->voxel_pm[axis] = inputs->voxel_pm[axis];
    }
    for (unsigned int moment = 0u; moment < FINGERPRINT_MOMENTS; moment += 1u)
    {
        print->moment_field[moment] = MAX_TREE_FIELD_MOMENT_ZZ + moment;
    }
}

static int run_pair_request(const RunPrintFields *fields, EngineRecordStep program[PRINT_PAIR_STEPS],
                            unsigned int outputs[PRINT_PAIR_OUTPUTS], EngineRecordRequest *encode_request)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = encode_request, .bytes = sizeof(*encode_request), .val = 0);
    encode_request->steps = program;
    encode_request->count = PRINT_PAIR_STEPS;
    encode_request->field_bits = fields->bits;
    encode_request->field_offset = fields->offset;
    encode_request->fields = PRINT_PAIR_BANDS;
    encode_request->in_limbs[0] = fields->limbs;
    encode_request->in_limbs[1] = fields->limbs;
    encode_request->members = 2u;
    encode_request->outputs = outputs;
    encode_request->output_count = PRINT_PAIR_OUTPUTS;
    return print_pair_program(program, outputs) != PRINT_PAIR_ERROR;
}

static long run_pair_encode(const RunPrintFields *fields, CycleRecord **record, EngineError *error)
{
    EngineRecordStep program[PRINT_PAIR_STEPS];
    unsigned int outputs[PRINT_PAIR_OUTPUTS];
    EngineRecordRequest encode_request;
    *record = NULL;
    const long out_bits = (run_pair_request(fields, program, outputs, &encode_request) == 0)
                              ? ENGINE_ERROR
                              : engine_record_encode(&encode_request, record, error);
    return ((out_bits == ENGINE_ERROR) || (cycle_record_out_limbs(*record) != 1u)) ? ENGINE_ERROR : out_bits;
}

static int run_pair_proof(const FlattenResident *flattened, const unsigned int *prints, const RunPrintFields *fields)
{
    CycleRecord *record = NULL;
    const size_t lanes = (size_t)flattened->bodies;
    unsigned int *const index = (unsigned int *)platform_cell_taken((2u * lanes + 1u) * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 0);
    unsigned int *const weight = (unsigned int *)platform_cell_taken((2u * lanes + 1u) * sizeof(unsigned int),
                                                                     platform_place_g_program_owner, 0);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    int ok = (run_pair_encode(fields, &record, &error) != ENGINE_ERROR) && (index != NULL) && (weight != NULL) &&
             (flattened->bodies <= 0xFFFFFFFFull);
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        index[2u * lane] = (unsigned int)lane;
        index[(2u * lane) + 1u] = (unsigned int)lane;
    }
    unsigned long long sweep = 0ull;
    const EngineRecordSweep itself = {
        record, {prints, prints}, {flattened->bodies, flattened->bodies}, index, flattened->bodies, weight, &sweep,
        &error};
    ok = ok && (engine_record_sweep(&itself) == (long)flattened->bodies);
    unsigned long long short_of_ceiling = 0ull;
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        short_of_ceiling += ((unsigned long long)weight[lane] != PRINT_PAIR_CEILING) ? 1ull : 0ull;
    }
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        index[(2u * lane) + 1u] = (unsigned int)(lanes - 1u - lane);
    }
    unsigned int *const host =
        ok ? (unsigned int *)platform_cell_taken((lanes + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
           : NULL;
    EngineRecordStep program[PRINT_PAIR_STEPS];
    unsigned int outputs[PRINT_PAIR_OUTPUTS];
    EngineRecordRequest encode_request;
    ok = ok && (run_pair_request(fields, program, outputs, &encode_request) != 0);
    unsigned long long across_sweep = 0ull;
    const EngineRecordSweep across = {record,
                                      {prints, prints},
                                      {flattened->bodies, flattened->bodies},
                                      index,
                                      flattened->bodies,
                                      weight,
                                      &across_sweep,
                                      &error};
    const EngineRecordSweep across_host = {
        NULL, {prints, prints}, {flattened->bodies, flattened->bodies}, index, flattened->bodies, host, NULL, &error};
    ok = ok && (host != NULL) && (engine_record_sweep(&across) == (long)flattened->bodies) &&
         (engine_record_host(&encode_request, &across_host) == (long)flattened->bodies);
    unsigned long long differ = 0ull;
    unsigned long long lightest = PRINT_PAIR_CEILING;
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        differ += (weight[lane] != host[lane]) ? 1ull : 0ull;
        lightest = ((unsigned long long)weight[lane] < lightest) ? (unsigned long long)weight[lane] : lightest;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  print pairs: every body against itself weighs the ceiling ");
        mmgr_line_decimal(output_line, PRINT_PAIR_CEILING, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, short_of_ceiling, 1u);
        mmgr_line_put(output_line, " short of it, in ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us; against the body across from it ");
        mmgr_line_decimal(output_line, across_sweep, 1u);
        mmgr_line_put(output_line, " us, lightest ");
        mmgr_line_decimal(output_line, lightest, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " differ from the host");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  run fingerprint: the print pairs errored\n");
        track_error_report("print pairs", &error);
    }
    platform_cell_released(index, platform_place_g_program_owner);
    platform_cell_released(weight, platform_place_g_program_owner);
    platform_cell_released(host, platform_place_g_program_owner);
    cycle_record_release(record);
    return (ok && (short_of_ceiling == 0ull) && (differ == 0ull)) ? 0 : 1;
}

#define RUN_STEER_STEPS 7u

static int run_steer_proof(const FlattenResident *flattened, const unsigned int *prints, const RunPrintFields *fields)
{
    unsigned int bits[MAX_TREE_FIELDS + FINGERPRINT_OUTPUTS];
    unsigned int offset[MAX_TREE_FIELDS + FINGERPRINT_OUTPUTS];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = bits, .src = flattened->layout.bits,
               .bytes = MAX_TREE_FIELDS * sizeof(unsigned int));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = offset, .src = flattened->layout.offset,
               .bytes = MAX_TREE_FIELDS * sizeof(unsigned int));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &bits[MAX_TREE_FIELDS], .src = fields->bits,
               .bytes = FINGERPRINT_OUTPUTS * sizeof(unsigned int));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &offset[MAX_TREE_FIELDS], .src = fields->offset,
               .bytes = FINGERPRINT_OUTPUTS * sizeof(unsigned int));
    const EngineRecordStep program[RUN_STEER_STEPS] = {{ENGINE_RECORD_FIELD, MAX_TREE_FIELD_MASS, 0u, 0u},
                                                       {ENGINE_RECORD_CONSTANT, 1u, 0u, 0u},
                                                       {ENGINE_RECORD_LADDER, 0u, 1u, 0u},
                                                       {ENGINE_RECORD_FIELD_SIGNED, MAX_TREE_FIELDS, 0u, 1u},
                                                       {ENGINE_RECORD_COMPARE, 2u, 3u, 0u},
                                                       {ENGINE_RECORD_ABSOLUTE, 4u, 0u, 0u},
                                                       {ENGINE_RECORD_DIFFERENCE, 1u, 5u, 0u}};
    const unsigned int output = RUN_STEER_STEPS - 1u;
    const EngineRecordRequest encode_request = {program,
                                                RUN_STEER_STEPS,
                                                bits,
                                                offset,
                                                MAX_TREE_FIELDS + FINGERPRINT_OUTPUTS,
                                                {flattened->layout.limbs, fields->limbs},
                                                2u,
                                                &output,
                                                1u};
    CycleRecord *record = NULL;
    const size_t lanes = (size_t)flattened->bodies;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    int ok = (engine_record_encode(&encode_request, &record, &error) != ENGINE_ERROR) &&
             (cycle_record_out_limbs(record) == 1u);
    unsigned int *const verdict =
        ok ? (unsigned int *)platform_cell_taken((lanes + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const host =
        ok ? (unsigned int *)platform_cell_taken((lanes + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
           : NULL;
    unsigned long long sweep = 0ull;
    const EngineRecordSweep run = {record,
                                   {flattened->magnitudes, prints},
                                   {flattened->bodies, flattened->bodies},
                                   NULL,
                                   flattened->bodies,
                                   verdict,
                                   &sweep,
                                   &error};
    const EngineRecordSweep host_run = {NULL,
                                        {flattened->magnitudes, prints},
                                        {flattened->bodies, flattened->bodies},
                                        NULL,
                                        flattened->bodies,
                                        host,
                                        NULL,
                                        &error};
    ok = ok && (verdict != NULL) && (host != NULL) && (engine_record_sweep(&run) == (long)flattened->bodies) &&
         (engine_record_host(&encode_request, &host_run) == (long)flattened->bodies);
    unsigned long long truthy = 0ull;
    unsigned long long differ = 0ull;
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        truthy += (verdict[lane] != 0u) ? 1ull : 0ull;
        differ += (verdict[lane] != host[lane]) ? 1ull : 0ull;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "  the body beside its own print, no index: the mass band read again agrees with the print's on ");
        mmgr_line_decimal(output_line, truthy, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, flattened->bodies, 1u);
        mmgr_line_put(output_line, " bodies, truthy; the sweep ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " differ from the host");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  run fingerprint: the body beside its print errored\n");
        track_error_report("body beside its print", &error);
    }
    platform_cell_released(verdict, platform_place_g_program_owner);
    platform_cell_released(host, platform_place_g_program_owner);
    cycle_record_release(record);
    return (ok && (truthy == flattened->bodies) && (differ == 0ull)) ? 0 : 1;
}

static int run_vocabulary_proof(const FlattenResident *flattened, const unsigned int *prints,
                                const RunPrintFields *fields)
{
    const unsigned int print_limbs = fields->limbs;
    CycleRecord *record = NULL;
    const EngineRecordRequest encode_request = {RUN_VOCABULARY_PROGRAM,
                                                RUN_VOCABULARY_STEPS,
                                                flattened->layout.bits,
                                                flattened->layout.offset,
                                                MAX_TREE_FIELDS,
                                                {flattened->layout.limbs},
                                                1u,
                                                RUN_VOCABULARY_OUTPUT,
                                                RUN_VOCABULARY_OUTPUTS};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if (engine_record_encode(&encode_request, &record, &error) == ENGINE_ERROR)
    {
        fprintf(stderr, "  run fingerprint: absolute and compare did not encode\n");
        track_error_report("absolute and compare", &error);
        return 1;
    }
    const unsigned int out_limbs = cycle_record_out_limbs(record);
    const size_t words = (size_t)flattened->bodies * out_limbs;
    unsigned int *const device =
        (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const host =
        (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned long long sweep = 0ull;
    const EngineRecordSweep run = {
        record, {flattened->magnitudes}, {flattened->bodies}, NULL, flattened->bodies, device, &sweep, &error};
    const EngineRecordSweep host_run = {
        NULL, {flattened->magnitudes}, {flattened->bodies}, NULL, flattened->bodies, host, NULL, &error};
    const int ok = (device != NULL) && (host != NULL) && (engine_record_sweep(&run) == (long)flattened->bodies) &&
                   (engine_record_host(&encode_request, &host_run) == (long)flattened->bodies);
    const unsigned int band_offset = fields->offset[1u + MAX_TREE_FIELD_MOMENT_ZY];
    const unsigned int band_bits = fields->bits[1u + MAX_TREE_FIELD_MOMENT_ZY];
    unsigned long long order[3] = {0ull, 0ull, 0ull};
    unsigned long long broken = 0ull;
    unsigned long long against_print = 0ull;
    unsigned long long differ = 0ull;
    for (unsigned long long body = 0ull; ok && (body < flattened->bodies); body += 1ull)
    {
        const unsigned int *const lane = &device[body * out_limbs];
        const int sign = engine_packed_signed(lane, 0u, 2u);
        const int above = engine_packed_signed(lane, 2u, 2u);
        const int below = engine_packed_signed(lane, 4u, 2u);
        const int band = engine_packed_signed(&prints[body * print_limbs], band_offset, band_bits);
        order[sign + 1] += 1ull;
        broken += ((above != ((sign < 0) ? 1 : 0)) || (below != -above)) ? 1ull : 0ull;
        against_print += ((band != 0) && (((band < 0) ? -1 : 1) != sign)) ? 1ull : 0ull;
        differ += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = lane, .other = &host[body * out_limbs],
                              .bytes = out_limbs * sizeof(unsigned int)) != 0)
                      ? 1ull
                      : 0ull;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  absolute and compare on K_zy of ");
        mmgr_line_decimal(output_line, flattened->bodies, 1u);
        mmgr_line_put(output_line, " bodies: ");
        mmgr_line_decimal(output_line, order[0], 1u);
        mmgr_line_put(output_line, " below zero, ");
        mmgr_line_decimal(output_line, order[1], 1u);
        mmgr_line_put(output_line, " at zero, ");
        mmgr_line_decimal(output_line, order[2], 1u);
        mmgr_line_put(output_line, " above; the sweep ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, broken, 1u);
        mmgr_line_put(output_line, " break |K| against K, ");
        mmgr_line_decimal(output_line, against_print, 1u);
        mmgr_line_put(output_line, " disagree with the print's band, ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " differ from the host");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  run fingerprint: absolute and compare errored\n");
        track_error_report("absolute and compare", &error);
    }
    platform_cell_released(device, platform_place_g_program_owner);
    platform_cell_released(host, platform_place_g_program_owner);
    cycle_record_release(record);
    return (ok && (broken == 0ull) && (against_print == 0ull) && (differ == 0ull)) ? 0 : 1;
}

static int run_fingerprint(const RunInputs *inputs)
{
    FlattenResident flattened;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if (flatten_read(inputs->set, &flattened, &error) == 0)
    {
        track_error_report("run fingerprint: flattened.iapx", &error);
        return 1;
    }
    FingerprintRequest print;
    run_print_request(inputs, &print);
    EngineRecordStep program[FINGERPRINT_STEPS];
    unsigned int outputs[FINGERPRINT_OUTPUTS];
    CycleRecord *record = NULL;
    RunPrintFields fields;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &fields, .bytes = sizeof(fields), .val = 0);
    const EngineRecordRequest encode_request = {program,
                                                FINGERPRINT_STEPS,
                                                flattened.layout.bits,
                                                flattened.layout.offset,
                                                MAX_TREE_FIELDS,
                                                {flattened.layout.limbs},
                                                1u,
                                                outputs,
                                                FINGERPRINT_OUTPUTS,
                                                fields.offset,
                                                fields.bits};
    const long out_bits = (fingerprint_program(&print, program, outputs) == FINGERPRINT_ERROR)
                              ? ENGINE_ERROR
                              : engine_record_encode(&encode_request, &record, &error);
    if (out_bits == ENGINE_ERROR)
    {
        fprintf(stderr, "  run fingerprint: the program did not encode; voxel_pm must name all three axes\n");
        track_error_report("fingerprint", &error);
        flatten_release(&flattened);
        return 1;
    }
    const unsigned int out_limbs = cycle_record_out_limbs(record);
    fields.limbs = out_limbs;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the print's record as a vector magnitude, field at offset:bits:");
        platform_line_out_ended();
    }
    for (unsigned int output = 0u; output < FINGERPRINT_OUTPUTS; output += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, fields.offset[output], 1u);
        mmgr_line_put(output_line, ":");
        mmgr_line_decimal(output_line, fields.bits[output], 1u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int *const records = (unsigned int *)platform_cell_taken(
        ((size_t)flattened.bodies * out_limbs + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned long long sweep = 0ull;
    const unsigned long long started = engine_clock_microseconds();
    const EngineRecordSweep run = {
        record, {flattened.magnitudes}, {flattened.bodies}, NULL, flattened.bodies, records, &sweep, &error};
    const int ok = (records != NULL) && (engine_record_sweep(&run) == (long)flattened.bodies);
    const unsigned long long elapsed = engine_clock_microseconds() - started;
    unsigned long long crc = ~0ull;
    for (size_t limb = 0u; ok && (limb < (size_t)flattened.bodies * out_limbs); limb += 1u)
    {
        for (unsigned int place = 0u; place < 4u; place += 1u)
        {
            crc = crc_step(CRC_TABLE, crc, (records[limb] >> (8u * place)) & 0xFFu);
        }
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  fingerprint: ");
        mmgr_line_decimal(output_line, flattened.bodies, 1u);
        mmgr_line_put(output_line, " bodies, ");
        mmgr_line_decimal(output_line, FINGERPRINT_STEPS, 1u);
        mmgr_line_put(output_line, " steps, ");
        mmgr_line_decimal_signed(output_line, out_bits);
        mmgr_line_put(output_line, " bits a print in ");
        mmgr_line_decimal(output_line, out_limbs, 1u);
        mmgr_line_put(output_line, " limbs; the sweep ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us, with the transfers ");
        mmgr_line_decimal(output_line, elapsed, 1u);
        mmgr_line_put(output_line, " us; CRC-64 of the prints ");
        mmgr_line_hex(output_line, ~crc, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  run fingerprint: the sweep errored\n");
        track_error_report("fingerprint sweep", &error);
    }
    const int indexed = ok && (flattened.bodies <= 0xFFFFFFFFull);
    unsigned int *const reversed =
        indexed ? (unsigned int *)platform_cell_taken(((size_t)flattened.bodies + 1u) * sizeof(unsigned int),
                                                      platform_place_g_program_owner, 0)
                : NULL;
    unsigned int *const gathered =
        indexed
            ? (unsigned int *)platform_cell_taken(((size_t)flattened.bodies * out_limbs + 1u) * sizeof(unsigned int),
                                                  platform_place_g_program_owner, 0)
            : NULL;
    for (unsigned long long lane = 0ull; (reversed != NULL) && (lane < flattened.bodies); lane += 1ull)
    {
        reversed[lane] = (unsigned int)(flattened.bodies - 1ull - lane);
    }
    unsigned long long gather_sweep = 0ull;
    const EngineRecordSweep through = {
        record, {flattened.magnitudes}, {flattened.bodies}, reversed, flattened.bodies, gathered, &gather_sweep,
        &error};
    const int gather =
        (reversed != NULL) && (gathered != NULL) && (engine_record_sweep(&through) == (long)flattened.bodies);
    unsigned long long misplaced = 0ull;
    for (unsigned long long lane = 0ull; gather && (lane < flattened.bodies); lane += 1ull)
    {
        misplaced += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &gathered[lane * out_limbs],
                                 .other = &records[(flattened.bodies - 1ull - lane) * out_limbs],
                                 .bytes = out_limbs * sizeof(unsigned int)) != 0)
                         ? 1ull
                         : 0ull;
    }
    if (gather)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  fingerprint through the index, every body read by the lane across from it: the sweep ");
        mmgr_line_decimal(output_line, gather_sweep, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, misplaced, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, flattened.bodies, 1u);
        mmgr_line_put(output_line, " prints land away from their body");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else if (ok)
    {
        fprintf(stderr, "  run fingerprint: the sweep through the index errored\n");
        track_error_report("fingerprint through the index", &error);
    }
    platform_cell_released(reversed, platform_place_g_program_owner);
    platform_cell_released(gathered, platform_place_g_program_owner);
    unsigned int *const host =
        ok ? (unsigned int *)platform_cell_taken(((size_t)flattened.bodies * out_limbs + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    const unsigned long long host_started = engine_clock_microseconds();
    const EngineRecordSweep host_run = {
        NULL, {flattened.magnitudes}, {flattened.bodies}, NULL, flattened.bodies, host, NULL, &error};
    const int ported = (host != NULL) && (engine_record_host(&encode_request, &host_run) == (long)flattened.bodies);
    const unsigned long long host_elapsed = engine_clock_microseconds() - host_started;
    unsigned long long differ = 0ull;
    for (unsigned long long body = 0ull; ported && (body < flattened.bodies); body += 1ull)
    {
        differ += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &host[body * out_limbs],
                              .other = &records[body * out_limbs], .bytes = out_limbs * sizeof(unsigned int)) != 0)
                      ? 1ull
                      : 0ull;
    }
    if (ported)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  fingerprint on the host, orior's exact integer: ");
        mmgr_line_decimal(output_line, host_elapsed, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, flattened.bodies, 1u);
        mmgr_line_put(output_line, " prints differ from the device");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else if (ok)
    {
        fprintf(stderr, "  run fingerprint: the host port errored\n");
        track_error_report("fingerprint host port", &error);
    }
    const int vocabulary = ok ? run_vocabulary_proof(&flattened, records, &fields) : 1;
    const int paired = ok ? run_pair_proof(&flattened, records, &fields) : 1;
    const int steered = ok ? run_steer_proof(&flattened, records, &fields) : 1;
    platform_cell_released(host, platform_place_g_program_owner);
    platform_cell_released(records, platform_place_g_program_owner);
    cycle_record_release(record);
    flatten_release(&flattened);
    return (ok && gather && (misplaced == 0ull) && ported && (differ == 0ull) && (vocabulary == 0) && (paired == 0) &&
            (steered == 0))
               ? 0
               : 1;
}

static int run_flattened_prepare(const RunInputs *inputs, FlattenResident *flattened_set, unsigned long long **starts,
                                 TreeRules *rules)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if (flatten_read(inputs->set, flattened_set, &error) == 0)
    {
        track_error_report("flattened.iapx", &error);
        return 0;
    }
    const unsigned int sample_bits = flattened_set->layout.bits[MAX_TREE_FIELD_SAMPLE];
    const unsigned int sample_offset = flattened_set->layout.offset[MAX_TREE_FIELD_SAMPLE];
    *starts = (unsigned long long *)platform_cell_cleared(
        (size_t)flattened_set->samples + 2u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    int ok = (*starts != NULL) && (sample_bits <= 32u);
    unsigned int previous = 0u;
    for (unsigned long long body = 0ull; ok && (body < flattened_set->bodies); body += 1ull)
    {
        const unsigned int sample = engine_packed_unsigned(
            &flattened_set->magnitudes[body * flattened_set->layout.limbs], sample_offset, sample_bits);
        ok = (sample < flattened_set->samples) && (sample >= previous);
        previous = sample;
        (*starts)[ok ? (sample + 1u) : 0u] += ok ? 1ull : 0ull;
    }
    for (unsigned int sample = 0u; ok && (sample < flattened_set->samples); sample += 1u)
    {
        (*starts)[sample + 1u] += (*starts)[sample];
    }
    if (ok == 0)
    {
        fprintf(stderr, "  flattened.iapx in %s does not store its samples in order\n", inputs->set);
        return 0;
    }
    rules->flattened = flattened_set->magnitudes;
    rules->flattened_limbs = flattened_set->layout.limbs;
    rules->flattened_samples = flattened_set->samples;
    rules->flattened_names = flattened_set->names;
    rules->flattened_start = *starts;
    return 1;
}

typedef struct
{
    EngineRecordStep program[VELOCITY_STEPS];
    unsigned int outputs[VELOCITY_OUTPUTS];
    unsigned int bits[MAX_TREE_FIELDS + ENGINE_AXES];
    unsigned int offset[MAX_TREE_FIELDS + ENGINE_AXES];
    unsigned int output_offset[VELOCITY_OUTPUTS];
    unsigned int output_bits[VELOCITY_OUTPUTS];
    EngineRecordRequest encode_request;
} RunVelocity;

static int run_velocity_prepare(const FlattenResident *flattened, RunVelocity *velocity, CycleRecord **record,
                                TreeRules *rules)
{
    VelocityRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.mass_field = MAX_TREE_FIELD_MASS;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = velocity->bits, .src = flattened->layout.bits,
               .bytes = MAX_TREE_FIELDS * sizeof(unsigned int));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = velocity->offset, .src = flattened->layout.offset,
               .bytes = MAX_TREE_FIELDS * sizeof(unsigned int));
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        request.sum_field[axis] = MAX_TREE_FIELD_SUM_Z + axis;
        request.lag_field[axis] = MAX_TREE_FIELDS + axis;
        velocity->bits[MAX_TREE_FIELDS + axis] = 32u;
        velocity->offset[MAX_TREE_FIELDS + axis] = 32u * axis;
    }
    EngineRecordRequest *const encode_request = &velocity->encode_request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = encode_request, .bytes = sizeof(*encode_request), .val = 0);
    encode_request->steps = velocity->program;
    encode_request->count = VELOCITY_STEPS;
    encode_request->field_bits = velocity->bits;
    encode_request->field_offset = velocity->offset;
    encode_request->fields = MAX_TREE_FIELDS + ENGINE_AXES;
    encode_request->in_limbs[VELOCITY_EARLIER] = flattened->layout.limbs;
    encode_request->in_limbs[VELOCITY_LATER] = flattened->layout.limbs;
    encode_request->in_limbs[VELOCITY_LAG] = ENGINE_AXES;
    encode_request->members = VELOCITY_MEMBERS;
    encode_request->outputs = velocity->outputs;
    encode_request->output_count = VELOCITY_OUTPUTS;
    encode_request->output_offset = velocity->output_offset;
    encode_request->output_bits = velocity->output_bits;
    *record = NULL;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const long out_bits = (velocity_program(&request, velocity->program, velocity->outputs) == VELOCITY_ERROR)
                              ? ENGINE_ERROR
                              : engine_record_encode(encode_request, record, &error);
    if (out_bits == ENGINE_ERROR)
    {
        fprintf(stderr, "  velocity: the program did not encode\n");
        track_error_report("velocity", &error);
        return 0;
    }
    rules->velocity_record = *record;
    rules->velocity_encode_request = encode_request;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  velocity: ");
        mmgr_line_decimal(output_line, VELOCITY_STEPS, 1u);
        mmgr_line_put(output_line, " steps, a record of ");
        mmgr_line_decimal_signed(output_line, out_bits);
        mmgr_line_put(output_line, " bits in ");
        mmgr_line_decimal(output_line, cycle_record_out_limbs(*record), 1u);
        mmgr_line_put(output_line, " limbs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

typedef struct
{
    EngineRecordStep program[DIVISION_STEPS];
    unsigned int outputs[DIVISION_OUTPUTS];
    unsigned int output_offset[DIVISION_OUTPUTS];
    unsigned int output_bits[DIVISION_OUTPUTS];
    EngineRecordRequest encode_request;
} RunDivision;

static int run_division_prepare(const RunInputs *inputs, const FlattenResident *flattened, RunDivision *division,
                                CycleRecord **record, TreeRules *rules)
{
    DivisionRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.mass_field = MAX_TREE_FIELD_MASS;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        request.sum_field[axis] = MAX_TREE_FIELD_SUM_Z + axis;
        request.voxel_pm[axis] = inputs->voxel_pm[axis];
    }
    for (unsigned int moment = 0u; moment < DIVISION_MOMENTS; moment += 1u)
    {
        request.moment_field[moment] = MAX_TREE_FIELD_MOMENT_ZZ + moment;
    }
    EngineRecordRequest *const encode_request = &division->encode_request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = encode_request, .bytes = sizeof(*encode_request), .val = 0);
    encode_request->steps = division->program;
    encode_request->count = DIVISION_STEPS;
    encode_request->field_bits = flattened->layout.bits;
    encode_request->field_offset = flattened->layout.offset;
    encode_request->fields = MAX_TREE_FIELDS;
    for (unsigned int member = 0u; member < DIVISION_MEMBERS; member += 1u)
    {
        encode_request->in_limbs[member] = flattened->layout.limbs;
    }
    encode_request->members = DIVISION_MEMBERS;
    encode_request->outputs = division->outputs;
    encode_request->output_count = DIVISION_OUTPUTS;
    encode_request->output_offset = division->output_offset;
    encode_request->output_bits = division->output_bits;
    *record = NULL;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const long out_bits = (division_program(&request, division->program, division->outputs) == DIVISION_ERROR)
                              ? ENGINE_ERROR
                              : engine_record_encode(encode_request, record, &error);
    if (out_bits == ENGINE_ERROR)
    {
        fprintf(stderr, "  division: the program did not encode; voxel_pm must name all three axes\n");
        track_error_report("division", &error);
        return 0;
    }
    rules->division_record = *record;
    rules->division_encode_request = encode_request;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  division: ");
        mmgr_line_decimal(output_line, DIVISION_STEPS, 1u);
        mmgr_line_put(output_line, " steps, a record of ");
        mmgr_line_decimal_signed(output_line, out_bits);
        mmgr_line_put(output_line, " bits in ");
        mmgr_line_decimal(output_line, cycle_record_out_limbs(*record), 1u);
        mmgr_line_put(output_line, " limbs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

typedef struct
{
    EngineRecordStep difference_program[CONTACT_SIDE_DIFFERENCE_STEPS];
    unsigned int difference_outputs[CONTACT_SIDE_DIFFERENCE_OUTPUTS];
    unsigned int difference_offset[CONTACT_SIDE_DIFFERENCE_OUTPUTS];
    unsigned int difference_bits[CONTACT_SIDE_DIFFERENCE_OUTPUTS];
    EngineRecordRequest difference_encode_request;
    EngineRecordStep retained_program[CONTACT_SIDE_SIGN_STEPS];
    unsigned int retained_outputs[CONTACT_SIDE_SIGN_OUTPUTS];
    unsigned int retained_offset[CONTACT_SIDE_SIGN_OUTPUTS];
    unsigned int retained_bits[CONTACT_SIDE_SIGN_OUTPUTS];
    EngineRecordRequest retained_encode_request;
} RunContactSide;

static int run_contact_side_prepare(const RunInputs *inputs, const FlattenResident *flattened, RunContactSide *side,
                                    CycleRecord **difference_record, CycleRecord **retained_record, TreeRules *rules)
{
    ContactSideDifferenceRequest difference;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &difference, .bytes = sizeof(difference), .val = 0);
    difference.mass_field = MAX_TREE_FIELD_MASS;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        difference.sum_field[axis] = MAX_TREE_FIELD_SUM_Z + axis;
        difference.voxel_pm[axis] = inputs->voxel_pm[axis];
    }
    EngineRecordRequest *const apart = &side->difference_encode_request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = apart, .bytes = sizeof(*apart), .val = 0);
    apart->steps = side->difference_program;
    apart->count = CONTACT_SIDE_DIFFERENCE_STEPS;
    apart->field_bits = flattened->layout.bits;
    apart->field_offset = flattened->layout.offset;
    apart->fields = MAX_TREE_FIELDS;
    apart->in_limbs[CONTACT_SIDE_ONE] = flattened->layout.limbs;
    apart->in_limbs[CONTACT_SIDE_OTHER] = flattened->layout.limbs;
    apart->members = CONTACT_SIDE_MEMBERS;
    apart->outputs = side->difference_outputs;
    apart->output_count = CONTACT_SIDE_DIFFERENCE_OUTPUTS;
    apart->output_offset = side->difference_offset;
    apart->output_bits = side->difference_bits;
    *difference_record = NULL;
    *retained_record = NULL;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const long apart_bits = (contact_side_difference_program(&difference, side->difference_program,
                                                             side->difference_outputs) == CONTACT_SIDE_ERROR)
                                ? ENGINE_ERROR
                                : engine_record_encode(apart, difference_record, &error);
    if (apart_bits == ENGINE_ERROR)
    {
        fprintf(stderr, "  contact side: the difference program did not encode; voxel_pm must name all three axes\n");
        track_error_report("contact side difference", &error);
        return 0;
    }
    ContactSideSignRequest retained;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &retained, .bytes = sizeof(retained), .val = 0);
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        retained.difference_field[axis] = axis;
    }
    const unsigned int apart_limbs = cycle_record_out_limbs(*difference_record);
    EngineRecordRequest *const verdict = &side->retained_encode_request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = verdict, .bytes = sizeof(*verdict), .val = 0);
    verdict->steps = side->retained_program;
    verdict->count = CONTACT_SIDE_SIGN_STEPS;
    verdict->field_bits = side->difference_bits;
    verdict->field_offset = side->difference_offset;
    verdict->fields = CONTACT_SIDE_DIFFERENCE_OUTPUTS;
    verdict->in_limbs[CONTACT_SIDE_BEFORE] = apart_limbs;
    verdict->in_limbs[CONTACT_SIDE_AFTER] = apart_limbs;
    verdict->members = CONTACT_SIDE_MEMBERS;
    verdict->outputs = side->retained_outputs;
    verdict->output_count = CONTACT_SIDE_SIGN_OUTPUTS;
    verdict->output_offset = side->retained_offset;
    verdict->output_bits = side->retained_bits;
    const long retained_bit_count =
        (contact_side_sign_program(&retained, side->retained_program, side->retained_outputs) == CONTACT_SIDE_ERROR)
            ? ENGINE_ERROR
            : engine_record_encode(verdict, retained_record, &error);
    if (retained_bit_count == ENGINE_ERROR)
    {
        fprintf(stderr, "  contact side: the verdict program did not encode\n");
        track_error_report("contact side verdict", &error);
        return 0;
    }
    rules->contact_difference_record = *difference_record;
    rules->contact_difference_encode_request = apart;
    rules->contact_retained_record = *retained_record;
    rules->contact_retained_encode_request = verdict;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  contact side: the difference, ");
        mmgr_line_decimal(output_line, CONTACT_SIDE_DIFFERENCE_STEPS, 1u);
        mmgr_line_put(output_line, " steps, a record of ");
        mmgr_line_decimal_signed(output_line, apart_bits);
        mmgr_line_put(output_line, " bits in ");
        mmgr_line_decimal(output_line, apart_limbs, 1u);
        mmgr_line_put(output_line, " limbs; the verdict, ");
        mmgr_line_decimal(output_line, CONTACT_SIDE_SIGN_STEPS, 1u);
        mmgr_line_put(output_line, " steps, ");
        mmgr_line_decimal_signed(output_line, retained_bit_count);
        mmgr_line_put(output_line, " bits in ");
        mmgr_line_decimal(output_line, cycle_record_out_limbs(*retained_record), 1u);
        mmgr_line_put(output_line, " limbs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

static int run_print_match_prepare(const RunInputs *inputs, const FlattenResident *flattened, unsigned int **prints,
                                   CycleRecord **pair, TreeRules *rules)
{
    FingerprintRequest print;
    run_print_request(inputs, &print);
    EngineRecordStep program[FINGERPRINT_STEPS];
    unsigned int outputs[FINGERPRINT_OUTPUTS];
    CycleRecord *record = NULL;
    RunPrintFields fields;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &fields, .bytes = sizeof(fields), .val = 0);
    const EngineRecordRequest encode_request = {program,
                                                FINGERPRINT_STEPS,
                                                flattened->layout.bits,
                                                flattened->layout.offset,
                                                MAX_TREE_FIELDS,
                                                {flattened->layout.limbs},
                                                1u,
                                                outputs,
                                                FINGERPRINT_OUTPUTS,
                                                fields.offset,
                                                fields.bits};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    int ok = (fingerprint_program(&print, program, outputs) != FINGERPRINT_ERROR) &&
             (engine_record_encode(&encode_request, &record, &error) != ENGINE_ERROR);
    const unsigned int limbs = ok ? cycle_record_out_limbs(record) : 0u;
    fields.limbs = limbs;
    *prints = ok ? (unsigned int *)platform_cell_taken(((size_t)flattened->bodies * limbs + 1u) * sizeof(unsigned int),
                                                       platform_place_g_program_owner, 0)
                 : NULL;
    unsigned long long sweep = 0ull;
    const EngineRecordSweep run = {
        record, {flattened->magnitudes}, {flattened->bodies}, NULL, flattened->bodies, *prints, &sweep, &error};
    ok = ok && (*prints != NULL) && (engine_record_sweep(&run) == (long)flattened->bodies);
    cycle_record_release(record);
    ok = ok && (run_pair_encode(&fields, pair, &error) != ENGINE_ERROR);
    if (ok == 0)
    {
        fprintf(stderr, "  print match: the prints or the pair program errored\n");
        track_error_report("print match", &error);
        return 0;
    }
    rules->print_pair = *pair;
    rules->prints = *prints;
    rules->print_limbs = limbs;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  print match: ");
        mmgr_line_decimal(output_line, flattened->bodies, 1u);
        mmgr_line_put(output_line, " bodies of ");
        mmgr_line_decimal(output_line, flattened->samples, 1u);
        mmgr_line_put(output_line, " samples printed in one sweep of ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

static int run_track(const TreeRules *rules, const RunInputs *inputs)
{
    TreeRules matched = *rules;
    FlattenResident flattened;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &flattened, .bytes = sizeof(flattened), .val = 0);
    unsigned int *prints = NULL;
    unsigned long long *starts = NULL;
    CycleRecord *pair = NULL;
    CycleRecord *velocity_record = NULL;
    CycleRecord *division_record = NULL;
    RunVelocity *const velocity =
        (RunVelocity *)platform_cell_cleared(1u, sizeof(RunVelocity), platform_place_g_program_owner, 0);
    RunDivision *const division =
        (RunDivision *)platform_cell_cleared(1u, sizeof(RunDivision), platform_place_g_program_owner, 0);
    CycleRecord *difference_record = NULL;
    CycleRecord *retained_record = NULL;
    RunContactSide *const side =
        (RunContactSide *)platform_cell_cleared(1u, sizeof(RunContactSide), platform_place_g_program_owner, 0);
    int ready = (velocity != NULL) && (division != NULL) && (side != NULL) &&
                (((rules->print_match == 0) && (rules->velocity == 0) && (rules->division == 0) &&
                  (rules->contact_side == 0)) ||
                 (run_flattened_prepare(inputs, &flattened, &starts, &matched) != 0));
    ready = ready &&
            ((rules->print_match == 0) || (run_print_match_prepare(inputs, &flattened, &prints, &pair, &matched) != 0));
    ready = ready &&
            ((rules->velocity == 0) || (run_velocity_prepare(&flattened, velocity, &velocity_record, &matched) != 0));
    ready = ready && ((rules->division == 0) ||
                      (run_division_prepare(inputs, &flattened, division, &division_record, &matched) != 0));
    ready = ready &&
            ((rules->contact_side == 0) ||
             (run_contact_side_prepare(inputs, &flattened, side, &difference_record, &retained_record, &matched) != 0));
    if (ready == 0)
    {
        platform_cell_released(prints, platform_place_g_program_owner);
        platform_cell_released(starts, platform_place_g_program_owner);
        cycle_record_release(pair);
        cycle_record_release(velocity_record);
        cycle_record_release(division_record);
        cycle_record_release(difference_record);
        cycle_record_release(retained_record);
        platform_cell_released(velocity, platform_place_g_program_owner);
        platform_cell_released(division, platform_place_g_program_owner);
        platform_cell_released(side, platform_place_g_program_owner);
        flatten_release(&flattened);
        return 1;
    }
    rules_line(rules, g_rules_line, sizeof(g_rules_line));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  rules:");
        mmgr_line_put(output_line, g_rules_line);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "sample", 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_columns(output_line, "edges", 6u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, "correct/branched/wrong/no link/missed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EdgeResults pooled;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pooled, .bytes = sizeof(pooled), .val = 0);
    const unsigned long long started = engine_clock_microseconds() / 1000ull;
    int failures = 0;
    for (unsigned int sample = 0u; sample < inputs->count; sample += 1u)
    {
        EdgeResults results;
        if (score_sample(inputs->set, inputs->source, inputs->samples[sample], &matched, &results) == 0)
        {
            fprintf(stderr, "  %s failed\n", inputs->samples[sample]);
            failures += 1;
            continue;
        }
        pooled.correct += results.correct;
        pooled.branched += results.branched;
        pooled.wrong += results.wrong;
        pooled.unlinked += results.unlinked;
        pooled.missed += results.missed;
    }
    const unsigned long long total = pooled.correct + pooled.branched + pooled.wrong + pooled.unlinked + pooled.missed;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  POOLED over ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " ground truth edges:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (total > 0ULL)
    {
        const char *const names[5] = {"correct link", "target among branches", "wrong link", "no link made",
                                      "endpoint undetected"};
        const unsigned long long values[5] = {pooled.correct, pooled.branched, pooled.wrong, pooled.unlinked,
                                              pooled.missed};
        for (unsigned int row = 0u; row < 5u; row += 1u)
        {
            unsigned long long percent = 0ULL;
            unsigned long long tenth = 0ULL;
            engine_percent_of(values[row], total, &percent, &tenth);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    ");
                mmgr_line_put_columns(output_line, names[row], 22u);
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal_columns(output_line, values[row], 5u);
                mmgr_line_put(output_line, "   ");
                mmgr_line_decimal(output_line, percent, 1u);
                mmgr_line_put(output_line, ".");
                mmgr_line_decimal(output_line, tenth, 1u);
                mmgr_line_ch(output_line, '%');
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    if ((g_web_queried + g_web_capped) > 0ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    web: queried ");
        mmgr_line_decimal(output_line, g_web_queried, 1u);
        mmgr_line_put(output_line, " objects, moved ");
        mmgr_line_decimal(output_line, g_web_moved, 1u);
        mmgr_line_put(output_line, ", capped ");
        mmgr_line_decimal(output_line, g_web_capped, 1u);
        mmgr_line_put(output_line, " over ");
        mmgr_line_decimal(output_line, WEB_MEMBERS, 1u);
        mmgr_line_put(output_line, " bodies");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if ((g_mutual_alone + g_mutual_split + g_mutual_empty) > 0ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    mutual: ");
        mmgr_line_decimal(output_line, g_mutual_alone + g_mutual_split + g_mutual_empty, 1u);
        mmgr_line_put(output_line, " cells, one survivor ");
        mmgr_line_decimal(output_line, g_mutual_alone, 1u);
        mmgr_line_put(output_line, ", several ");
        mmgr_line_decimal(output_line, g_mutual_split, 1u);
        mmgr_line_put(output_line, ", none ");
        mmgr_line_decimal(output_line, g_mutual_empty, 1u);
        mmgr_line_put(output_line, ", moved ");
        mmgr_line_decimal(output_line, g_mutual_moved, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    survey_report();
    EngineBodiesResults results;
    engine_bodies_results(&results);
    if (results.frames != 0ull)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    bodies: proved on ");
        mmgr_line_decimal(output_line, results.proven, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, results.frames, 1u);
        mmgr_line_put(output_line, " frames, ");
        mmgr_line_decimal(output_line, results.bodies, 1u);
        mmgr_line_put(output_line, " bodies, ");
        mmgr_line_decimal(output_line, results.bodies / results.frames, 1u);
        mmgr_line_put(output_line, " per frame, mean level index ");
        mmgr_line_decimal(output_line, results.levels / results.frames, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EngineResidualResults residual;
    engine_residual_results(&residual);
    if (residual.frames != 0ull)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    residual: ");
        mmgr_line_decimal(output_line, residual.frames, 1u);
        mmgr_line_put(output_line, " frames, the key ");
        mmgr_line_decimal(output_line, residual.key_microseconds, 1u);
        mmgr_line_put(output_line, " us, the unit sweeps ");
        mmgr_line_decimal(output_line, residual.sweep_microseconds, 1u);
        mmgr_line_put(output_line, " us; proved on ");
        mmgr_line_decimal(output_line, residual.proved_frames, 1u);
        mmgr_line_put(output_line, " frames, ");
        mmgr_line_decimal(output_line, residual.differing_frames, 1u);
        mmgr_line_put(output_line, " differ (");
        mmgr_line_decimal(output_line, residual.differing_lanes, 1u);
        mmgr_line_put(output_line, " lanes)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long wall = (engine_clock_microseconds() / 1000ull) - started;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  ");
        mmgr_line_decimal(output_line, wall, 1u);
        mmgr_line_put(output_line, " ms");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    FILE *const log = log_open();
    if (log != NULL)
    {
        char when[32];
        log_when(when, sizeof(when));
        fprintf(log,
                "%s\trun\t%s\t%u samples, %d failed\t0\t0\t0\t0\t0\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu"
                "\t0\t0\t0\t0\t0\t0\t0\t0\t%llu\t0\t%llu\t%llu\t%llu\t%u\t%llu\t%llu\t%llu\n",
                when, log_rules(), inputs->count, failures, total, pooled.correct, pooled.branched, pooled.wrong,
                pooled.unlinked, pooled.missed, wall, g_web_queried, g_web_moved, g_web_capped, WEB_MEMBERS,
                g_damp_leaves, g_damp_landings, (unsigned long long)DAMP_DEVIATIONS);
        fclose(log);
    }
    platform_cell_released(prints, platform_place_g_program_owner);
    platform_cell_released(starts, platform_place_g_program_owner);
    cycle_record_release(pair);
    cycle_record_release(velocity_record);
    cycle_record_release(division_record);
    cycle_record_release(difference_record);
    cycle_record_release(retained_record);
    platform_cell_released(velocity, platform_place_g_program_owner);
    platform_cell_released(division, platform_place_g_program_owner);
    platform_cell_released(side, platform_place_g_program_owner);
    flatten_release(&flattened);
    return failures;
}

int program_main(int argc, char **argv)
{
    TreeRules rules;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &rules, .bytes = sizeof(rules), .val = 0);
    rules.resolve = 1;
    RunInputs inputs;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &inputs, .bytes = sizeof(inputs), .val = 0);
    const char *cfg_out = NULL;
    // --gate-still: the sort's gate also takes each source's still place; the sort alone reads it
    unsigned int gate_still = 0u;
    // the sort's costs are O7's, each distance's level in the sample's pooled count of first-rank links, unless
    // --cost-distance retains the first pass's weighted squared distances; the sort alone reads it
    unsigned int cost_first = 1u;
    int argument = 1;
    while ((argument < argc) && (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = argv[argument],
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--", .ci = EMBED_FALSE)))
    {
        const char *const flag = argv[argument];
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--pick",
                       .ci = EMBED_FALSE))
        {
            rules.pick = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--share", .ci = EMBED_FALSE))
        {
            rules.share = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--agree", .ci = EMBED_FALSE))
        {
            rules.agree = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--unbound", .ci = EMBED_FALSE))
        {
            rules.unbound = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--cast", .ci = EMBED_FALSE))
        {
            rules.cast = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--parallax", .ci = EMBED_FALSE))
        {
            rules.parallax = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--arc", .ci = EMBED_FALSE))
        {
            rules.arc = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--settle", .ci = EMBED_FALSE))
        {
            rules.settle = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--web", .ci = EMBED_FALSE))
        {
            rules.web = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--damp", .ci = EMBED_FALSE))
        {
            rules.damp = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--dish", .ci = EMBED_FALSE))
        {
            rules.dish = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--vote", .ci = EMBED_FALSE))
        {
            rules.vote = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--mutual", .ci = EMBED_FALSE))
        {
            rules.mutual = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--tower", .ci = EMBED_FALSE))
        {
            rules.tower = 1;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--plan", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            g_schedule_path = argv[argument];
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--run", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            RunPart part = RUN_TRACK;
            if (run_part_named(argv[argument], &part) == 0)
            {
                fprintf(stderr,
                        "  --run %s: the parts are schedule, iapx-prove, entropy, floor, flatten, track,"
                        " fingerprint, scan, flicker, lines, neighbors, clips, pixels, moments, sort, output"
                        " and faces, each named once\n",
                        argv[argument]);
                return 2;
            }
            if (run_part_named_already(part) != 0)
            {
                fprintf(stderr, "  --run %s: the part is named twice; each part runs once\n", argv[argument]);
                return 2;
            }
            s_parts[s_part_count] = part;
            s_part_count += 1u;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--survey", .ci = EMBED_FALSE))
        {
            g_survey = 1u;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--ingest", .ci = EMBED_FALSE))
        {
            s_ingest = 1u;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--override", .ci = EMBED_FALSE))
        {
            s_override = 1u;
        }
        else if (((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = "--source", .ci = EMBED_FALSE)) ||
                  (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = "--set", .ci = EMBED_FALSE)) ||
                  (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = "--axes", .ci = EMBED_FALSE))) &&
                 (argument + 1 < argc))
        {
            char **const path_field =
                (flag[2] == 's') ? ((flag[3] == 'o') ? &inputs.source : &inputs.set) : &inputs.axes;
            argument += 1;
            platform_cell_released(*path_field, platform_place_g_program_owner);
            *path_field = cfg_copy(argv[argument]);
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--channel", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            unsigned int channel = 0u;
            for (const char *digit = argv[argument]; (*digit >= '0') && (*digit <= '9') && (channel < 65536u);
                 digit += 1u)
            {
                channel = (channel * 10u) + (unsigned int)(*digit - '0');
            }
            inputs.channel = channel;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--mass", .ci = EMBED_FALSE))
        {
            rules.mass = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--forest", .ci = EMBED_FALSE))
        {
            rules.forest = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--cohere", .ci = EMBED_FALSE))
        {
            rules.cohere = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--accrue", .ci = EMBED_FALSE))
        {
            rules.accrue = 1;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--spiral", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            rules.spiral = (unsigned int)strtoul(argv[argument], NULL, 10);
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--nodes", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 6u, argv[argument]))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--log", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            g_log_path = argv[argument];
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--focus", .ci = EMBED_FALSE))
        {
            rules.focus = 1;
            rules.arms = (rules.arms == 0u) ? 1u : rules.arms;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--merge-split", .ci = EMBED_FALSE))
        {
            rules.merge_split = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--merge-target", .ci = EMBED_FALSE))
        {
            rules.merge_target = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--forward-only", .ci = EMBED_FALSE))
        {
            rules.forward_only = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--retain-view", .ci = EMBED_FALSE))
        {
            rules.retain_view = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--no-resolve", .ci = EMBED_FALSE))
        {
            rules.resolve = 0;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--climb", .ci = EMBED_FALSE))
        {
            rules.climb = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--print-match", .ci = EMBED_FALSE))
        {
            rules.print_match = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--velocity", .ci = EMBED_FALSE))
        {
            rules.velocity = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--mass-band", .ci = EMBED_FALSE))
        {
            rules.mass_band = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--division", .ci = EMBED_FALSE))
        {
            rules.division = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--contact-side", .ci = EMBED_FALSE))
        {
            rules.contact_side = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--box", .ci = EMBED_FALSE))
        {
            rules.box = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--core", .ci = EMBED_FALSE))
        {
            rules.core = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--residual-key", .ci = EMBED_FALSE))
        {
            rules.unit_sweep = ENGINE_RESIDUAL_BY_KEY;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--unit-sweep-prove", .ci = EMBED_FALSE))
        {
            rules.unit_sweep = ENGINE_RESIDUAL_BOTH_PROVED;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--box-history", .ci = EMBED_FALSE))
        {
            rules.box_history = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--marginal", .ci = EMBED_FALSE))
        {
            rules.marginal = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--cfg", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!apply_cfg(argv[argument], &rules, &inputs))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--cfg-out", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            cfg_out = argv[argument];
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--edges", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 0u, argv[argument]))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--pool", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 5u, argv[argument]))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--export", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 2u, argv[argument]))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--object", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 3u, argv[argument]))
            {
                return 2;
            }
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--vis", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 4u, argv[argument]))
            {
                return 2;
            }
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--sticky", .ci = EMBED_FALSE))
        {
            rules.sticky = 1;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--motion-check", .ci = EMBED_FALSE))
        {
            rules.motion_check = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--gate-still", .ci = EMBED_FALSE))
        {
            gate_still = 1u;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--cost-distance", .ci = EMBED_FALSE))
        {
            cost_first = 0u;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--null", .ci = EMBED_FALSE)) &&
                 ((argument + 1) < argc))
        {
            argument += 1;
            unsigned int draws = 0u;
            for (const char *digit = argv[argument]; (*digit >= '0') && (*digit <= '9') && (draws < 4096u); digit += 1u)
            {
                draws = (draws * 10u) + (unsigned int)(*digit - '0');
            }
            rules.null_draws = draws;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--arms", .ci = EMBED_FALSE)) &&
                 ((argument + 1) < argc))
        {
            argument += 1;
            unsigned int arms = 0u;
            for (const char *digit = argv[argument]; (*digit >= '0') && (*digit <= '9') && (arms < 64u); digit += 1u)
            {
                arms = (arms * 10u) + (unsigned int)(*digit - '0');
            }
            rules.arms = arms;
            rules.climb = (rules.climb == 0) ? 1 : rules.climb;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--climb-host", .ci = EMBED_FALSE))
        {
            rules.climb = 2;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--climb-check", .ci = EMBED_FALSE))
        {
            rules.climb = 3;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = flag, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--coherence", .ci = EMBED_FALSE)) &&
                 (argument + 1 < argc))
        {
            argument += 1;
            if (!name_output(&rules, &inputs, 1u, argv[argument]))
            {
                return 2;
            }
        }
        else
        {
            fprintf(stderr, "  unknown flag %s\n", flag);
            return 2;
        }
        argument += 1;
    }
    if (argument < argc)
    {
        platform_cell_released(inputs.set, platform_place_g_program_owner);
        inputs.set = cfg_copy(argv[argument]);
        argument += 1;
    }
    if (argument < argc)
    {
        for (unsigned int index = 0u; index < inputs.count; index += 1u)
        {
            platform_cell_released(inputs.samples[index], platform_place_g_program_owner);
        }
        platform_cell_released(inputs.samples, platform_place_g_program_owner);
        inputs.count = (unsigned int)(argc - argument);
        inputs.samples = (char **)platform_cell_cleared((size_t)inputs.count + 1u, sizeof(char *),
                                                        platform_place_g_program_owner, 0);
        for (unsigned int index = 0u; inputs.samples && (index < inputs.count); index += 1u)
        {
            inputs.samples[index] = cfg_copy(argv[argument + (int)index]);
        }
    }
    if (!inputs.count && inputs.first)
    {
        first_samples(&inputs, s_ingest != 0u);
    }
    if ((s_ingest != 0u) && (s_part_count != 0u))
    {
        fprintf(stderr, "  --ingest is its own step: it prepares the set and runs nothing, so no --run part goes"
                        " with it\n");
        return 2;
    }
    if (!inputs.set || !inputs.count || ((s_ingest != 0u) && !inputs.source))
    {
        fprintf(stderr,
                "usage: track_driver --ingest --source directory [--axes tzyx] [--channel c] <set> [<sample> ...]\n"
                "       track_driver [--cfg run.cfg] [--cfg-out effective.cfg] [--run part ...] [--plan path]"
                " [--pick] [--merge-split] [--merge-target] [--forward-only] [--retain-view] [--no-resolve] [--climb]"
                " [--edges path] [--object directory] [<set> [<sample> ...]]\n"
                "  the source is the dataset as it came; the set contains one <sample>/<sample>.iapx per sample,"
                " made from the source by --ingest and by nothing else\n"
                "  --run names one part; each part once, the parts running in the order given: schedule, iapx-prove,"
                " entropy, floor, flatten, track, fingerprint, scan, flicker, lines, neighbors, clips,"
                " pixels, moments, sort, output, faces\n"
                "  a .cfg names the source, the set and the samples in its input section; positional words"
                " override it\n"
                "  every part but output and faces, and --ingest, is one job on the device's tessera daemon"
                " (tessera_daemon beside this program); --override admits a job that declares more than its"
                " request's tracked peak. output and faces are host parts and request no job\n");
        return 2;
    }
    if ((s_ingest == 0u) && (s_part_count == 0u))
    {
        s_parts[0] = RUN_TRACK;
        s_part_count = 1u;
    }
    if (run_part_named_already(RUN_FLOOR) != 0)
    {
        inputs.floor_entropy = true;
    }
    // the floor is not among the parts named here, and they are distinct. With it they are at max RUN_PARTS
    if ((s_ingest == 0u) && inputs.floor_entropy && (run_part_named_already(RUN_FLOOR) == 0))
    {
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = &s_parts[1], .src = &s_parts[0],
                   .bytes = (size_t)s_part_count * sizeof(s_parts[0]));
        s_parts[0] = RUN_FLOOR;
        s_part_count += 1u;
    }
    // the outputs were only named at parse (name_output). A run errored there changed no file. Now that every
    // error of the command line has had its chance, each named output is probed before anything is written
    // (probe_output)
    for (unsigned int output = 0u; output < RUN_OUTPUTS; output += 1u)
    {
        if (!probe_output(&inputs, output))
        {
            return 2;
        }
    }
    CfgText effective;
    if (!write_cfg(&rules, &inputs, &effective))
    {
        fprintf(stderr, "  could not write the effective .cfg\n");
        return 2;
    }
    rules.cfg_text = effective.bytes;
    rules.cfg_length = effective.length;
    rules.floor_entropy = inputs.floor_entropy;
    FILE *const cfg_file = cfg_out ? fopen(cfg_out, "wb") : NULL;
    if (cfg_out && (!cfg_file || (fwrite(effective.bytes, 1u, effective.length, cfg_file) != effective.length)))
    {
        fprintf(stderr, "  could not write %s\n", cfg_out);
        return 2;
    }
    cfg_file ? (void)fclose(cfg_file) : (void)0;
    // then the outputs open (open_output) in their order, the nodes and the submission last
    for (unsigned int output = 0u; output < RUN_OUTPUTS; output += 1u)
    {
        if (!open_output(&rules, &inputs, output))
        {
            return 2;
        }
    }
    const char *const directory = inputs.set;
    // the device's context is made before any job is requested, and only when some part requests one
    unsigned int device_parts = 0u;
    for (unsigned int at = 0u; at < s_part_count; at += 1u)
    {
        device_parts += (run_part_host(s_parts[at]) == 0) ? 1u : 0u;
    }
    if ((s_ingest != 0u) || (device_parts != 0u))
    {
        (void)cudaFree(NULL);
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  every part named is a host part: no device job is requested, and no device context is made");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    if (s_ingest != 0u)
    {
        RunJob job;
        if (!run_job_submit("ingest", &effective, &inputs, 1, 1, 0, 0, &job))
        {
            return 2;
        }
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        EngineSetReport report;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &report, .bytes = sizeof(report), .val = 0);
        report.samples = (EngineSampleRecord *)platform_cell_cleared(
            (size_t)inputs.count + 1u, sizeof(EngineSampleRecord), platform_place_g_program_owner, 0);
        const EngineIngestRequest ingest = {inputs.source, inputs.set,     inputs.samples, inputs.count,
                                            inputs.axes,   inputs.channel, &error,         &report};
        const long ingested = engine_ingest_set(&ingest);
        run_job_release("ingest", &job);
        engine_ingest_print(&ingest, platform_line_out());
        platform_line_out_ended();
        platform_cell_released(report.samples, platform_place_g_program_owner);
        if (ingested != 0L)
        {
            track_error_report("ingest", &error);
            return 2;
        }
        return 0;
    }
    const EngineSetRequest iapx = {directory, inputs.samples, inputs.count};
    int failures = 0;
    for (unsigned int at = 0u; (failures == 0) && (at < s_part_count); at += 1u)
    {
        const RunPart part = s_parts[at];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  run ");
            mmgr_line_put(output_line, RUN_PART_NAMES[part]);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        RunJob job;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
        // iapx-prove lowers every lattice to prove it and allocates none of its own on the device. Schedule reads only
        // the heads, and fingerprint only flattened.iapx. Neither lowers one; the sort reads only .points and .drift,
        // and allocates no lattice and lowers none
        const int lattice = (part != RUN_IAPX_PROVE) && (part != RUN_SORT);
        const int tower = (part != RUN_SCHEDULE) && (part != RUN_FINGERPRINT) && (part != RUN_SORT);
        // the scan alone drifts its frames through shift_agreement, and the sort alone gates its frame pairs
        const int drift = part == RUN_SCAN;
        const int gate = part == RUN_SORT;
        const int host = run_part_host(part);
        if ((host == 0) &&
            !run_job_submit(RUN_PART_NAMES[part], &effective, &inputs, lattice, tower, drift, gate, &job))
        {
            failures = 1;
        }
        else if (part == RUN_SCHEDULE)
        {
            failures =
                ((g_schedule_path != NULL) && (schedule_program(directory, inputs.samples, inputs.count) != 0)) ? 0 : 1;
            if (g_schedule_path == NULL)
            {
                fprintf(stderr, "  run schedule: --plan names where the plan is written\n");
            }
        }
        else if (part == RUN_IAPX_PROVE)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            EngineSetReport report;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &report, .bytes = sizeof(report), .val = 0);
            report.samples = (EngineSampleRecord *)platform_cell_cleared(
                (size_t)inputs.count + 1u, sizeof(EngineSampleRecord), platform_place_g_program_owner, 0);
            EngineSetRequest prove = iapx;
            prove.error = &error;
            prove.report = &report;
            failures = (engine_iapx_prove_set(&prove) == 0L) ? 0 : 1;
            engine_prove_print(&prove, platform_line_out());
            platform_line_out_ended();
            platform_cell_released(report.samples, platform_place_g_program_owner);
            if (failures != 0)
            {
                track_error_report("iapx prove", &error);
            }
        }
        else if ((part == RUN_ENTROPY) || (part == RUN_FLOOR))
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            EngineEntropySetRequest entropy = {iapx, (part == RUN_FLOOR) ? 1u : 0u};
            entropy.set.error = &error;
            failures = (engine_entropy_set(&entropy) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("entropy", &error);
            }
        }
        else if (part == RUN_FLATTEN)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            FlattenSetRequest flatten;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &flatten, .bytes = sizeof(flatten), .val = 0);
            flatten.set = directory;
            flatten.names = inputs.samples;
            flatten.count = inputs.count;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = flatten.smooth_orders, .src = SMOOTH_ORDERS,
                       .bytes = sizeof(flatten.smooth_orders));
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = flatten.background_orders, .src = BACKGROUND_ORDERS,
                       .bytes = sizeof(flatten.background_orders));
            flatten.error = &error;
            failures = (flatten_set(&flatten) != 0) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("flatten", &error);
            }
        }
        else if (part == RUN_FINGERPRINT)
        {
            failures = run_fingerprint(&inputs);
        }
        else if (part == RUN_SCAN)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            ScanRequest scan;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &scan, .bytes = sizeof(scan), .val = 0);
            scan.set = directory;
            scan.samples = inputs.samples;
            scan.count = inputs.count;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = scan.smooth_orders, .src = SMOOTH_ORDERS,
                       .bytes = sizeof(scan.smooth_orders));
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = scan.background_orders, .src = BACKGROUND_ORDERS,
                       .bytes = sizeof(scan.background_orders));
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = scan.voxel_pm, .src = inputs.voxel_pm,
                       .bytes = sizeof(scan.voxel_pm));
            scan.error = &error;
            failures = (scan_set(&scan) == 0L) ? 0 : 1;
            if (error.type != ENGINE_ERROR_NONE)
            {
                track_error_report("scan", &error);
            }
        }
        else if (part == RUN_FLICKER)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_flicker_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("flicker", &error);
            }
        }
        else if (part == RUN_LINES)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_lines_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("lines", &error);
            }
        }
        else if (part == RUN_NEIGHBORS)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_neighbors_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("neighbors", &error);
            }
        }
        else if (part == RUN_CLIPS)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_clips_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("clips", &error);
            }
        }
        else if (part == RUN_PIXELS)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_pixels_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("pixels", &error);
            }
        }
        else if (part == RUN_MOMENTS)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            const NoiseSetRequest noise = {directory, inputs.samples, inputs.count, &error, track_noise_load};
            failures = (noise_moments_set(&noise) == 0L) ? 0 : 1;
            if (failures != 0)
            {
                track_error_report("moments", &error);
            }
        }
        else if (part == RUN_SORT)
        {
            EngineError error;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
            SortRequest links;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &links, .bytes = sizeof(links), .val = 0);
            links.set = directory;
            links.samples = inputs.samples;
            links.count = inputs.count;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = links.voxel_pm, .src = inputs.voxel_pm,
                       .bytes = sizeof(links.voxel_pm));
            links.error = &error;
            links.still = gate_still;
            links.first = cost_first;
            failures = (sort_link_set(&links) == 0L) ? 0 : 1;
            if (error.type != ENGINE_ERROR_NONE)
            {
                track_error_report("sort", &error);
            }
        }
        else if (part == RUN_OUTPUT)
        {
            const OutputRequest output = {directory,   inputs.samples,   inputs.count,
                                          rules.nodes, rules.submission, stdout};
            failures = (output_graph_set(&output) == 0L) ? 0 : 1;
        }
        else if (part == RUN_FACES)
        {
            const FacesRequest faces = {directory, inputs.samples, inputs.count, stdout};
            failures = (faces_find_set(&faces) == 0L) ? 0 : 1;
        }
        else
        {
            failures = run_track(&rules, &inputs);
        }
        if (job.client != NULL)
        {
            run_job_release(RUN_PART_NAMES[part], &job);
        }
        if (failures != 0)
        {
            fprintf(stderr, "  run %s failed; the parts after it do not run\n", RUN_PART_NAMES[part]);
        }
    }
    if (rules.coherence != NULL)
    {
        fclose(rules.coherence);
    }
    if (rules.vis_index != NULL)
    {
        fclose(rules.vis_index);
    }
    if (rules.edges != NULL)
    {
        fclose(rules.edges);
    }
    // the nodes and the submission close checked (output_close): a write that failed on either, or a close that fails,
    // has lost rows, and fails the run
    FILE *const closing[2] = {rules.nodes, rules.submission};
    const unsigned int closing_outputs[2] = {6u, 7u};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (output_close(closing[at], inputs.outputs[closing_outputs[at]]) == 0)
        {
            failures = 1;
        }
    }
    for (unsigned int index = 0u; index < inputs.count; index += 1u)
    {
        platform_cell_released(inputs.samples[index], platform_place_g_program_owner);
    }
    for (unsigned int index = 0u; index < RUN_OUTPUTS; index += 1u)
    {
        platform_cell_released(inputs.outputs[index], platform_place_g_program_owner);
    }
    platform_cell_released(inputs.samples, platform_place_g_program_owner);
    platform_cell_released(inputs.source, platform_place_g_program_owner);
    platform_cell_released(inputs.set, platform_place_g_program_owner);
    platform_cell_released(inputs.axes, platform_place_g_program_owner);
    platform_cell_released(inputs.species, platform_place_g_program_owner);
    platform_cell_released(inputs.view, platform_place_g_program_owner);
    platform_cell_released(effective.bytes, platform_place_g_program_owner);
    return (failures == 0) ? 0 : 1;
}
