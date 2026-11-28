// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// period_record.cu: the agreement and the null on the record machine
#include "../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "period_internal.h"

#include "../../../../../includes/exact_types/integers/exact_record/exact_record.h"
#include "../cycle/cycle.h"
#include "../key_schedule/key_schedule.h"
#include "../keymath/keymath.h"

// a sweep's index names a voxel by an output one bit wider than its register, 32 bits a limb
#define PERIOD_RECORD_VOXELS_MAX (1ULL << 31u)

#define PERIOD_RECORD_PLACE_BITS 31u

#define PERIOD_RECORD_KEY_BITS 32u

#define PERIOD_RECORD_VALUE_BITS 16u

// the agreement's shared record: the axis's stride, its extent and its usable length, a limb each
#define PERIOD_RECORD_AGREE_LIMBS 3u

// the key's shared record: the axis's extent, its stride and the four shuffle keys, a limb each
#define PERIOD_RECORD_KEY_LIMBS 6u

// the key's record: the voxel's place in 32 bits, then its key in 33
#define PERIOD_RECORD_KEYED_LIMBS 3u

// period_round's two multipliers
#define PERIOD_RECORD_MIX_FIRST 0x9E3779B1ULL

#define PERIOD_RECORD_MIX_SECOND 0x85EBCA77ULL

#define PERIOD_RECORD_SLICES 7u

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} PeriodProgram;

// The four programs and the cells of the device, made once a process. The agreement reads a voxel, the voxel a lag on,
// and the shared record; the key writes each voxel's place and key. The place reads a keyed record through the order
// and writes its place; the gather reads a voxel through those places. The cells are persistent cells of the pool of
// the device, from the lanes to the histogram, until period_record_release
typedef struct
{
    int made;
    PeriodProgram agree;
    PeriodProgram key;
    PeriodProgram place;
    PeriodProgram gather;
    unsigned int agree_offset;
    unsigned int agree_bits;
    unsigned int key_offset;
    unsigned int *lanes;
    unsigned int *shuffled;
    unsigned int *keyed;
    unsigned int *order;
    unsigned int *places;
    unsigned int *shared;
    unsigned int *histogram;
    unsigned long long voxels;
} PeriodRecordResident;

static PeriodRecordResident g_period_record;

static void period_program_release(PeriodProgram *program)
{
    if (program->record != NULL)
    {
        cycle_record_release(program->record);
    }
    key_schedule_record_release(&program->layout);
    keymath_record_release(&program->key);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
}

// imprints, arranges and loads a built program
static int period_program_load(const ExactRecordProgram *built, unsigned int members, const unsigned int *outputs,
                               unsigned int output_count, const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX],
                               PeriodProgram *program, EngineError *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const KeymathRecordRequest encode = {
        built->steps, built->count,  built->field_bits,  built->fields, members, outputs,
        output_count, built->tables, built->table_count, &program->key, error};
    const KeyScheduleRecordRequest layout = {
        &program->key, built->field_offset, built->fields, in_limbs, 1, &program->layout, error};
    return PERIOD_CHECK(built->failed == 0, built, error, ENGINE_ERROR_RESOURCE) &&
           (keymath_record_encode(&encode) != KEYMATH_ERROR) &&
           (key_schedule_record_layout(&layout) != KEY_SCHEDULE_ERROR) &&
           (cycle_record_load(&program->layout, &program->record, error) != CYCLE_ERROR);
}

// [the voxel equals the voxel one lag on, and the pair is where the usable length of the axis holds it]. The place of
// the lane along the axis is its quotient by the stride, modulo the extent
static int period_make_agree(PeriodRecordResident *resident, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int value = exact_record_member_field(&built, PERIOD_RECORD_VALUE_BITS, 0u);
    const unsigned int stride_field = exact_record_member_field(&built, 32u, 0u);
    const unsigned int extent_field = exact_record_member_field(&built, 32u, 32u);
    const unsigned int usable_field = exact_record_member_field(&built, 32u, 64u);
    const unsigned int here = exact_record_read_unsigned(&built, value, 0u);
    const unsigned int there = exact_record_read_unsigned(&built, value, 1u);
    const unsigned int stride = exact_record_read_unsigned(&built, stride_field, 2u);
    const unsigned int extent = exact_record_read_unsigned(&built, extent_field, 2u);
    const unsigned int usable = exact_record_read_unsigned(&built, usable_field, 2u);
    const unsigned int lane = exact_record_lane_below(&built, 32u);
    const unsigned int along = exact_record_remainder(&built, exact_record_quotient(&built, lane, stride), extent);
    const unsigned int usable_lane = exact_record_above(&built, usable, along);
    const unsigned int same = exact_record_equal(&built, here, there);
    const unsigned int agree = exact_record_product(&built, same, usable_lane);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 1u, PERIOD_RECORD_AGREE_LIMBS};
    const int ok = period_program_load(&built, 3u, &agree, 1u, in_limbs, &resident->agree, error);
    exact_record_close(&built);
    if (ok != 0)
    {
        resident->agree_offset = resident->agree.layout.step_table[agree].out_offset;
        resident->agree_bits = resident->agree.layout.step_table[agree].out_bits;
    }
    return ok;
}

// period_round: the 32-bit word `(half xor key)` times the first multiplier, xor itself shifted down 15. Then that
// times the second, xor itself shifted down 13, with each product limited to its low 32 bits
static unsigned int period_record_round(ExactRecordProgram *built, unsigned int half, unsigned int key)
{
    const unsigned int first = exact_record_constant(built, PERIOD_RECORD_MIX_FIRST);
    const unsigned int second = exact_record_constant(built, PERIOD_RECORD_MIX_SECOND);
    unsigned int mixed = exact_record_narrow(
        built, exact_record_product(built, exact_record_step(built, ENGINE_RECORD_XOR, half, key, 0u), first), 32u);
    mixed = exact_record_step(built, ENGINE_RECORD_XOR, mixed,
                              exact_record_quotient(built, mixed, exact_record_power_two(built, 15u)), 0u);
    mixed = exact_record_narrow(built, exact_record_product(built, mixed, second), 32u);
    return exact_record_step(built, ENGINE_RECORD_XOR, mixed,
                             exact_record_quotient(built, mixed, exact_record_power_two(built, 13u)), 0u);
}

// lane l of a line-major sweep is at step `j = l mod n` of line `q = l / n`. Its voxel is
// `((q / stride) n + j) stride + q mod stride`, and its key is period_line_random of `(q, j)` under the four keys
static int period_make_key(PeriodRecordResident *resident, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    unsigned int fields[PERIOD_RECORD_KEY_LIMBS];
    for (unsigned int field = 0u; field < PERIOD_RECORD_KEY_LIMBS; field += 1u)
    {
        fields[field] = exact_record_member_field(&built, 32u, 32u * field);
    }
    const unsigned int extent = exact_record_read_unsigned(&built, fields[0], 0u);
    const unsigned int stride = exact_record_read_unsigned(&built, fields[1], 0u);
    unsigned int keys[PERIOD_ROUNDS];
    for (unsigned int round = 0u; round < PERIOD_ROUNDS; round += 1u)
    {
        keys[round] = exact_record_read_unsigned(&built, fields[2u + round], 0u);
    }
    const unsigned int lane = exact_record_lane_below(&built, 32u);
    const unsigned int line = exact_record_quotient(&built, lane, extent);
    const unsigned int step = exact_record_remainder(&built, lane, extent);
    const unsigned int outer = exact_record_quotient(&built, line, stride);
    const unsigned int inner = exact_record_remainder(&built, line, stride);
    const unsigned int row = exact_record_sum(&built, exact_record_product(&built, outer, extent), step);
    const unsigned int place = exact_record_narrow(
        &built, exact_record_sum(&built, exact_record_product(&built, row, stride), inner), PERIOD_RECORD_PLACE_BITS);
    // the line is below 2^31. Its high word, the second round's, is 0, and the round takes the hash alone
    unsigned int hash =
        period_record_round(&built, exact_record_step(&built, ENGINE_RECORD_XOR, line, keys[0], 0u), keys[1]);
    hash = period_record_round(&built, hash, keys[2]);
    hash = period_record_round(&built, exact_record_step(&built, ENGINE_RECORD_XOR, hash, step, 0u), keys[3]);
    const unsigned int outputs[2] = {place, hash};
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {PERIOD_RECORD_KEY_LIMBS, 0u, 0u};
    int ok = period_program_load(&built, 1u, outputs, 2u, in_limbs, &resident->key, error);
    exact_record_close(&built);
    if (ok != 0)
    {
        resident->key_offset = resident->key.layout.step_table[hash].out_offset;
        ok = PERIOD_CHECK((resident->key.layout.out_limbs == PERIOD_RECORD_KEYED_LIMBS) &&
                              (resident->key.layout.step_table[place].out_offset == 0u) &&
                              (resident->key.layout.step_table[hash].out_bits == (PERIOD_RECORD_KEY_BITS + 1u)),
                          &resident->key, error, ENGINE_ERROR_LOGIC);
    }
    return ok;
}

// one field of member 0, `bits` wide at `offset`, written out: the place through the order, or the voxel through the
// places. The call writes a field that reads as a magnitude below `2^31` in 32 bits, one limb for each record
static int period_make_read(unsigned int bits, unsigned int in_limbs_0, PeriodProgram *program, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, bits, 0u);
    const unsigned int read = exact_record_read_unsigned(&built, field, 0u);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {in_limbs_0, 0u, 0u};
    int ok = period_program_load(&built, 1u, &read, 1u, in_limbs, program, error);
    exact_record_close(&built);
    ok = ok && PERIOD_CHECK((program->layout.out_limbs == 1u) && (program->layout.step_table[read].out_offset == 0u),
                            program, error, ENGINE_ERROR_LOGIC);
    return ok;
}

static int period_record_make(EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    if (resident->made != 0)
    {
        return 1;
    }
    const int ok = period_make_agree(resident, error) && period_make_key(resident, error) &&
                   period_make_read(PERIOD_RECORD_PLACE_BITS, PERIOD_RECORD_KEYED_LIMBS, &resident->place, error) &&
                   period_make_read(PERIOD_RECORD_VALUE_BITS, 1u, &resident->gather, error);
    if (ok == 0)
    {
        period_program_release(&resident->agree);
        period_program_release(&resident->key);
        period_program_release(&resident->place);
        period_program_release(&resident->gather);
        return 0;
    }
    resident->made = 1;
    return 1;
}

// The bytes of the cells for `voxels`, in the order that the reserve takes them. The order is the lanes, the shuffled
// lanes, the keyed records, the order, the places, the shared record and the histogram
static void period_record_slices(unsigned long long voxels, unsigned long long bytes[PERIOD_RECORD_SLICES])
{
    const unsigned long long limb = sizeof(unsigned int);
    const unsigned long long slices[PERIOD_RECORD_SLICES] = {
        voxels * limb,       voxels * limb, voxels * PERIOD_RECORD_KEYED_LIMBS * limb,
        voxels * limb,       voxels * limb, PERIOD_RECORD_KEY_LIMBS * limb,
        PERIOD_VALUES * limb};
    for (unsigned int at = 0u; at < PERIOD_RECORD_SLICES; at += 1u)
    {
        bytes[at] = slices[at];
    }
}

// the addresses of the cells of the resident, in the order of period_record_slices
static void period_record_cells(PeriodRecordResident *resident, unsigned int **cells[PERIOD_RECORD_SLICES])
{
    unsigned int **const each[PERIOD_RECORD_SLICES] = {&resident->lanes,    &resident->shuffled, &resident->keyed,
                                                       &resident->order,    &resident->places,   &resident->shared,
                                                       &resident->histogram};
    for (unsigned int at = 0u; at < PERIOD_RECORD_SLICES; at += 1u)
    {
        cells[at] = each[at];
    }
}

// every cell of the resident given back to the pool of the device, the newest first
static void period_record_given(PeriodRecordResident *resident)
{
    unsigned int **cells[PERIOD_RECORD_SLICES];
    period_record_cells(resident, cells);
    for (unsigned int at = PERIOD_RECORD_SLICES; at > 0u; at -= 1u)
    {
        tessera_job_given(*cells[at - 1u], PERIOD_PLACE);
        *cells[at - 1u] = NULL;
    }
    resident->voxels = 0ULL;
}

// the cells grown to contain `voxels`; cells that already contain them stay as they are
static int period_record_reserve(unsigned long long voxels, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    if (voxels <= resident->voxels)
    {
        return 1;
    }
    period_record_given(resident);
    unsigned long long bytes[PERIOD_RECORD_SLICES];
    period_record_slices(voxels, bytes);
    unsigned int **cells[PERIOD_RECORD_SLICES];
    period_record_cells(resident, cells);
    int ok = 1;
    for (unsigned int at = 0u; (ok != 0) && (at < PERIOD_RECORD_SLICES); at += 1u)
    {
        ok = PERIOD_CHECK(tessera_job_persistent_taken(bytes[at], PERIOD_PLACE, (void **)cells[at]), cells[at], error,
                          ENGINE_ERROR_RESOURCE);
    }
    resident->voxels = (ok != 0) ? voxels : 0ULL;
    return ok;
}

extern "C" unsigned long long period_record_reserve_bytes(unsigned long long voxels)
{
    if ((voxels == 0ULL) || (voxels > PERIOD_RECORD_VOXELS_MAX))
    {
        return 0ULL;
    }
    unsigned long long bytes[PERIOD_RECORD_SLICES];
    period_record_slices(voxels, bytes);
    DevicePoolPlan plan = {0ULL, 0ULL, 0};
    for (unsigned int at = 0u; at < PERIOD_RECORD_SLICES; at += 1u)
    {
        device_pool_plan_slice(&plan, bytes[at]);
    }
    return device_pool_plan_bytes(&plan);
}

extern "C" void period_record_release(void)
{
    PeriodRecordResident *const resident = &g_period_record;
    period_program_release(&resident->agree);
    period_program_release(&resident->key);
    period_program_release(&resident->place);
    period_program_release(&resident->gather);
    period_record_given(resident);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
}

// the agreement of one axis at every lag, of `data` arranged with `stride` along the axis. Lag L sweeps the lanes that
// have a voxel L strides on. The program writes 1 where the pair agrees and the place along the axis is usable. The
// sum of those is the count
static int period_record_agreement(const unsigned int *data, unsigned long long voxels, unsigned int stride,
                                   unsigned int extent, unsigned long long *same, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned int lags = extent / 2u;
    const unsigned int shared[PERIOD_RECORD_AGREE_LIMBS] = {stride, extent, extent - lags};
    int ok = PERIOD_CHECK(tessera_job_crossed_into(resident->shared, shared, sizeof(shared)), resident->shared, error,
                          ENGINE_ERROR_RESOURCE);
    for (unsigned int lag = 1u; (ok != 0) && (lag <= lags); lag += 1u)
    {
        const unsigned long long reach = (unsigned long long)lag * stride;
        const unsigned long long lanes = voxels - reach;
        CycleRecordRunRequest sweep;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &sweep, .bytes = sizeof(sweep), .val = 0);
        sweep.record = resident->agree.record;
        sweep.device_in[0] = data;
        sweep.device_in[1] = data + reach;
        sweep.device_in[2] = resident->shared;
        sweep.bodies[0] = voxels;
        sweep.bodies[1] = lanes;
        sweep.bodies[2] = 1ULL;
        sweep.count = lanes;
        // the records of the agreement take the room of the keyed records, which no step reads while the program
        // counts the agreement
        sweep.device_out = resident->keyed;
        sweep.error = error;
        unsigned int sum[2] = {0u, 0u};
        const CycleRecordSumRequest total = {resident->keyed,      lanes, lanes, 1u,   resident->agree_offset,
                                             resident->agree_bits, 2u,    sum,   error};
        ok = (cycle_record_run(&sweep) != CYCLE_ERROR) && (cycle_record_sum(&total) != CYCLE_ERROR);
        same[lag - 1u] = ((unsigned long long)sum[1] << 32u) | sum[0];
    }
    return ok;
}

// the null of one axis on the record machine. The program keys each voxel in line-major order, and sorts each line by
// its keys. It gathers the places of the order, and gathers the voxels through them. Thus the shuffled lattice runs
// line by line with stride 1. Its agreement along the axis, and the strongest peak that it reaches, give the height of
// the draw
static int period_record_null(const PeriodRequest *request, const PeriodLattice *lattice, unsigned long long counter,
                              unsigned int axis, unsigned long long *same, PeriodMargin *height, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned long long voxels = lattice->voxels;
    PeriodShuffle shuffle;
    period_shuffle_fill(&shuffle, &request->content, counter);
    const unsigned int shared[PERIOD_RECORD_KEY_LIMBS] = {lattice->extent[axis], lattice->stride[axis],
                                                          shuffle.keys[0],       shuffle.keys[1],
                                                          shuffle.keys[2],       shuffle.keys[3]};
    int ok = PERIOD_CHECK(tessera_job_crossed_into(resident->shared, shared, sizeof(shared)), resident->shared, error,
                          ENGINE_ERROR_RESOURCE);
    CycleRecordRunRequest sweep;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &sweep, .bytes = sizeof(sweep), .val = 0);
    sweep.record = resident->key.record;
    sweep.device_in[0] = resident->shared;
    sweep.bodies[0] = 1ULL;
    sweep.count = voxels;
    sweep.device_out = resident->keyed;
    sweep.error = error;
    ok = ok && (cycle_record_run(&sweep) != CYCLE_ERROR);
    const CycleRecordSortRequest sort = {resident->keyed,       voxels,
                                         lattice->extent[axis], PERIOD_RECORD_KEYED_LIMBS,
                                         resident->key_offset,  PERIOD_RECORD_KEY_BITS,
                                         resident->order,       error};
    ok = ok && (cycle_record_sort(&sort) != CYCLE_ERROR);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &sweep, .bytes = sizeof(sweep), .val = 0);
    sweep.record = resident->place.record;
    sweep.device_in[0] = resident->keyed;
    sweep.bodies[0] = voxels;
    sweep.device_index = resident->order;
    sweep.count = voxels;
    sweep.device_out = resident->places;
    sweep.error = error;
    ok = ok && (cycle_record_run(&sweep) != CYCLE_ERROR);
    sweep.record = resident->gather.record;
    sweep.device_in[0] = resident->lanes;
    sweep.device_index = resident->places;
    sweep.device_out = resident->shuffled;
    ok = ok && (cycle_record_run(&sweep) != CYCLE_ERROR);
    ok = ok && period_record_agreement(resident->shuffled, voxels, 1u, lattice->extent[axis], same, error);
    if (ok != 0)
    {
        PeriodAxis drawn;
        period_axis_open(lattice, axis, &drawn);
        period_strongest(same, &drawn);
        height->numerator = (drawn.candidate != 0ULL) ? drawn.margin.numerator : 0ULL;
        height->denominator = drawn.pairs_per_lag;
    }
    return ok;
}

// each 16-bit lane written into the low half of its limb, the high half 0
__global__ static void period_record_widen_kernel(const unsigned short *lanes, unsigned long long voxels,
                                                  unsigned int *limbs)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; voxel < voxels;
         voxel += jump)
    {
        limbs[voxel] = (unsigned int)lanes[voxel];
    }
}

// the request's lattice, the programs and the cells, and the lanes widened into one limb each on the device
static int period_record_open(const PeriodRequest *request, PeriodLattice *lattice, unsigned long long *voxels,
                              EngineError *error)
{
    if ((period_lattice_fill(request, lattice, voxels) == 0) ||
        !PERIOD_CHECK(*voxels <= PERIOD_RECORD_VOXELS_MAX, &request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    PeriodRecordResident *const resident = &g_period_record;
    int ok = period_record_make(error) && period_record_reserve(*voxels, error);
    if (ok != 0)
    {
        const unsigned long long needed = (*voxels + PERIOD_THREADS - 1ULL) / PERIOD_THREADS;
        // Explicit cast takes a count of blocks below 4096
        const unsigned int blocks = (unsigned int)((needed < 4096ULL) ? needed : 4096ULL);
        period_record_widen_kernel<<<blocks, PERIOD_THREADS>>>(request->device_lanes, *voxels, resident->lanes);
        ok = PERIOD_STATUS_CHECK(cudaGetLastError(), resident->lanes, error);
    }
    return ok;
}

extern "C" long period_record_read(const PeriodRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return PERIOD_ERROR;
    }
    EngineError *const error = request->error;
    if (!PERIOD_CHECK((request->device_lanes != NULL) && (request->measurement != NULL), request, error,
                      ENGINE_ERROR_REQUEST))
    {
        return PERIOD_ERROR;
    }
    PeriodLattice lattice;
    unsigned long long voxels = 0ULL;
    if ((period_record_open(request, &lattice, &voxels, error) == 0) ||
        (period_request_valid(request, lattice.lag_total) == 0))
    {
        return PERIOD_ERROR;
    }
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned long long entries = lattice.lag_total;
    const unsigned long long draws = request->draws;
    const size_t agreement_entries = (size_t)((entries != 0ULL) ? entries : 1ULL);
    unsigned int *const histogram = (unsigned int *)platform_cell_taken(
        (unsigned long long)(PERIOD_VALUES * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned long long *const agreement = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)(agreement_entries), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_engine_owner, 0);
    unsigned long long *const shuffled_agreement = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)(agreement_entries), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_engine_owner, 0);
    const size_t band_count = (size_t)((draws != 0ULL) ? (draws * lattice.rank) : 1ULL);
    PeriodMargin *const bands = (PeriodMargin *)platform_cell_cleared((unsigned long long)(band_count),
                                                                      (unsigned long long)(sizeof(PeriodMargin)),
                                                                      (unsigned int)platform_place_g_engine_owner, 0);
    unsigned long long band_counts[ENGINE_ARRAY_RANK] = {0ULL};
    int ok = PERIOD_CHECK((histogram != NULL) && (agreement != NULL) && (shuffled_agreement != NULL) && (bands != NULL),
                          request, error, ENGINE_ERROR_RESOURCE) &&
             PERIOD_CHECK(tessera_job_cleared(resident->histogram, PERIOD_VALUES * sizeof(unsigned int)),
                          resident->histogram, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        const unsigned long long needed = (voxels + PERIOD_THREADS - 1ULL) / PERIOD_THREADS;
        // the block count has a bound below the block limit of the record machine, far below `2^31`
        const unsigned int blocks = (unsigned int)((needed < 4096ULL) ? needed : 4096ULL);
        period_histogram_kernel<<<blocks, PERIOD_THREADS>>>(request->device_lanes, voxels, resident->histogram);
        ok = PERIOD_STATUS_CHECK(cudaGetLastError(), resident->histogram, error) &&
             PERIOD_CHECK(tessera_job_crossed_out(histogram, resident->histogram, PERIOD_VALUES * sizeof(unsigned int)),
                          histogram, error, ENGINE_ERROR_RESOURCE);
    }
    unsigned long long collisions = 0ULL;
    if (ok != 0)
    {
        unsigned long long counted = 0ULL;
        for (unsigned int value = 0u; value < PERIOD_VALUES; value += 1u)
        {
            counted += histogram[value];
            collisions += (unsigned long long)histogram[value] * histogram[value];
        }
        ok = PERIOD_CHECK(counted == voxels, histogram, error, ENGINE_ERROR_LOGIC);
    }
    for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
    {
        ok = period_record_agreement(resident->lanes, voxels, lattice.stride[axis], lattice.extent[axis],
                                     &agreement[lattice.first[axis]], error);
    }
    PeriodMeasurement *const measurement = request->measurement;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = measurement, .bytes = sizeof(*measurement), .val = 0);
    measurement->rank = lattice.rank;
    measurement->voxels = voxels;
    measurement->collisions = collisions;
    measurement->draws = draws;
    for (unsigned long long draw = 0ULL; (ok != 0) && (voxels >= 2ULL) && (draw < draws); draw += 1ULL)
    {
        for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
        {
            PeriodMargin height;
            ok = period_record_null(request, &lattice, (draw * lattice.rank) + axis, axis,
                                    &shuffled_agreement[lattice.first[axis]], &height, error);
            if ((ok != 0) && (height.numerator != 0ULL))
            {
                bands[(axis * draws) + band_counts[axis]] = height;
                band_counts[axis] += 1ULL;
            }
        }
    }
    for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
    {
        period_select(&agreement[lattice.first[axis]], &lattice, axis, &bands[axis * draws], band_counts[axis],
                      (request->null_top != NULL) ? &request->null_top[axis] : NULL, &measurement->axis[axis]);
    }
    if ((ok != 0) && (request->agreement != NULL) && (entries != 0ULL))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->agreement, .src = agreement,
                   .bytes = (size_t)entries * sizeof(unsigned long long));
    }
    if ((ok != 0) && (request->band != NULL))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->band, .src = bands,
                   .bytes = (size_t)(draws * lattice.rank) * sizeof(PeriodMargin));
    }
    platform_cell_released(histogram, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(agreement, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(shuffled_agreement, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(bands, (unsigned int)platform_place_g_engine_owner);
    return (ok != 0) ? 0L : PERIOD_ERROR;
}

extern "C" long period_record_draw(const PeriodRequest *request, unsigned long long draw, PeriodMargin *heights)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return PERIOD_ERROR;
    }
    EngineError *const error = request->error;
    if (!PERIOD_CHECK((request->device_lanes != NULL) && (heights != NULL), request, error, ENGINE_ERROR_REQUEST))
    {
        return PERIOD_ERROR;
    }
    PeriodLattice lattice;
    unsigned long long voxels = 0ULL;
    if (period_record_open(request, &lattice, &voxels, error) == 0)
    {
        return PERIOD_ERROR;
    }
    const size_t agreement_entries = (size_t)((lattice.lag_total != 0ULL) ? lattice.lag_total : 1ULL);
    unsigned long long *const shuffled_agreement = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)(agreement_entries), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_engine_owner, 0);
    int ok = PERIOD_CHECK(shuffled_agreement != NULL, request, error, ENGINE_ERROR_RESOURCE);
    for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
    {
        PeriodAxis open;
        period_axis_open(&lattice, axis, &open);
        heights[axis].numerator = 0ULL;
        heights[axis].denominator = open.pairs_per_lag;
        if (voxels >= 2ULL)
        {
            ok = period_record_null(request, &lattice, (draw * lattice.rank) + axis, axis,
                                    &shuffled_agreement[lattice.first[axis]], &heights[axis], error);
        }
    }
    platform_cell_released(shuffled_agreement, (unsigned int)platform_place_g_engine_owner);
    return (ok != 0) ? 0L : PERIOD_ERROR;
}
