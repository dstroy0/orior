// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// period_record.cu: the agreement and the null on the record machine
#include "period_internal.h"

#include "../cycle/cycle.h"
#include "../key_schedule/key_schedule.h"
#include "../keymath/keymath.h"
#include "../../../types/integers/exact_record/exact_record.h"

// a sweep's index names a voxel by an output one bit wider than its register, 32 bits a limb
#define PERIOD_RECORD_VOXELS_MAX (1ull << 31u)

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
#define PERIOD_RECORD_MIX_FIRST 0x9E3779B1ull

#define PERIOD_RECORD_MIX_SECOND 0x85EBCA77ull

#define PERIOD_RECORD_SLICES 7u

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} PeriodProgram;

// the four programs and the pool, made once a process. The agreement reads a voxel, the voxel a lag on, and the shared
// record; the key writes each voxel's place and key; the place reads a keyed record through the order and writes its
// place; the gather reads a voxel through those places
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
    DevicePool pool;
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
    memset(program, 0, sizeof(*program));
}

// imprints, lays out and loads a built program
static int period_program_load(const ExactRecordProgram *built, unsigned int members, const unsigned int *outputs,
                               unsigned int output_count, const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX],
                               PeriodProgram *program, EngineError *error)
{
    memset(program, 0, sizeof(*program));
    const KeymathRecordRequest encode = {built->steps,  built->count,       built->field_bits, built->fields,
                                         members,       outputs,            output_count,      built->tables,
                                         built->table_count, &program->key, error};
    const KeyScheduleRecordRequest layout = {&program->key, built->field_offset, built->fields, in_limbs, 1,
                                             &program->layout, error};
    return PERIOD_CHECK(built->failed == 0, built, error, ENGINE_ERROR_RESOURCE) &&
           (keymath_record_encode(&encode) != KEYMATH_ERROR) &&
           (key_schedule_record_layout(&layout) != KEY_SCHEDULE_ERROR) &&
           (cycle_record_load(&program->layout, &program->record, error) != CYCLE_ERROR);
}

// [the voxel equals the voxel a lag on, and the pair stands where the axis's usable length holds it]: the lane's place
// along the axis is its quotient by the stride, modulo the extent
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
    const unsigned int held = exact_record_above(&built, usable, along);
    const unsigned int same = exact_record_equal(&built, here, there);
    const unsigned int agree = exact_record_product(&built, same, held);
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

// period_round: the 32-bit word (half xor key) times the first multiplier, xor itself shifted down 15, times the
// second, xor itself shifted down 13, each product kept to its low 32 bits
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

// lane l of a line-major sweep stands at step j = l mod n of line q = l / n; its voxel is
// ((q / stride) n + j) stride + q mod stride, and its key is period_line_random of (q, j) under the four keys
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
    unsigned int hash = period_record_round(
        &built, exact_record_step(&built, ENGINE_RECORD_XOR, line, keys[0], 0u), keys[1]);
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
// places. A field read as a magnitude below 2^31 is written in 32 bits, one limb a record
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

// the pool's slices for `voxels`, in the order they are laid out and taken
static DevicePoolPlan period_record_plan(unsigned long long voxels, EngineError *error,
                                         DevicePoolTakeRequest takes[PERIOD_RECORD_SLICES])
{
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned long long limb = sizeof(unsigned int);
    const DevicePoolTakeRequest requests[PERIOD_RECORD_SLICES] = {
        {&resident->pool, voxels * limb, (void **)&resident->lanes, error},
        {&resident->pool, voxels * limb, (void **)&resident->shuffled, error},
        {&resident->pool, voxels * PERIOD_RECORD_KEYED_LIMBS * limb, (void **)&resident->keyed, error},
        {&resident->pool, voxels * limb, (void **)&resident->order, error},
        {&resident->pool, voxels * limb, (void **)&resident->places, error},
        {&resident->pool, PERIOD_RECORD_KEY_LIMBS * limb, (void **)&resident->shared, error},
        {&resident->pool, PERIOD_VALUES * limb, (void **)&resident->histogram, error}};
    DevicePoolPlan plan = {0ull, 0ull, 0};
    for (unsigned int at = 0u; at < PERIOD_RECORD_SLICES; at += 1u)
    {
        takes[at] = requests[at];
        device_pool_plan_slice(&plan, requests[at].bytes);
    }
    return plan;
}

// the pool grown to hold `voxels`; a pool that already holds them is kept as it is
static int period_record_reserve(unsigned long long voxels, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    if (voxels <= resident->voxels)
    {
        return 1;
    }
    device_pool_release(&resident->pool);
    resident->voxels = 0ull;
    DevicePoolTakeRequest takes[PERIOD_RECORD_SLICES];
    const DevicePoolPlan plan = period_record_plan(voxels, error, takes);
    const DevicePoolReserveRequest reserve = {&plan, &resident->pool, error};
    int ok = device_pool_reserve(&reserve) == 0L;
    for (unsigned int at = 0u; (ok != 0) && (at < PERIOD_RECORD_SLICES); at += 1u)
    {
        ok = device_pool_take(&takes[at]) == 0L;
    }
    resident->voxels = (ok != 0) ? voxels : 0ull;
    return ok;
}

extern "C" unsigned long long period_record_reserve_bytes(unsigned long long voxels)
{
    if ((voxels == 0ull) || (voxels > PERIOD_RECORD_VOXELS_MAX))
    {
        return 0ull;
    }
    DevicePoolTakeRequest takes[PERIOD_RECORD_SLICES];
    const DevicePoolPlan plan = period_record_plan(voxels, NULL, takes);
    return device_pool_plan_bytes(&plan);
}

extern "C" void period_record_release(void)
{
    PeriodRecordResident *const resident = &g_period_record;
    period_program_release(&resident->agree);
    period_program_release(&resident->key);
    period_program_release(&resident->place);
    period_program_release(&resident->gather);
    device_pool_release(&resident->pool);
    memset(resident, 0, sizeof(*resident));
}

// the agreement of one axis at every lag, of `data` laid with `stride` along the axis: lag L sweeps the lanes that have
// a voxel L strides on, the program writes 1 where the pair agrees and the place along the axis is usable, and the sum
// of those is the count
static int period_record_agreement(const unsigned int *data, unsigned long long voxels, unsigned int stride,
                                   unsigned int extent, unsigned long long *same, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned int lags = extent / 2u;
    const unsigned int shared[PERIOD_RECORD_AGREE_LIMBS] = {stride, extent, extent - lags};
    int ok = PERIOD_STATUS_CHECK(cudaMemcpy(resident->shared, shared, sizeof(shared), cudaMemcpyHostToDevice),
                                 resident->shared, error);
    for (unsigned int lag = 1u; (ok != 0) && (lag <= lags); lag += 1u)
    {
        const unsigned long long reach = (unsigned long long)lag * stride;
        const unsigned long long lanes = voxels - reach;
        CycleRecordRunRequest sweep;
        memset(&sweep, 0, sizeof(sweep));
        sweep.record = resident->agree.record;
        sweep.device_in[0] = data;
        sweep.device_in[1] = data + reach;
        sweep.device_in[2] = resident->shared;
        sweep.bodies[0] = voxels;
        sweep.bodies[1] = lanes;
        sweep.bodies[2] = 1ull;
        sweep.count = lanes;
        // the agreement's records take the keyed records' room, which no step reads while the agreement is counted
        sweep.device_out = resident->keyed;
        sweep.error = error;
        unsigned int sum[2] = {0u, 0u};
        const CycleRecordSumRequest total = {resident->keyed, lanes, lanes, 1u, resident->agree_offset,
                                             resident->agree_bits, 2u, sum, error};
        ok = (cycle_record_run(&sweep) != CYCLE_ERROR) && (cycle_record_sum(&total) != CYCLE_ERROR);
        same[lag - 1u] = ((unsigned long long)sum[1] << 32u) | sum[0];
    }
    return ok;
}

// one axis's null on the record machine: each voxel keyed in line-major order, each line sorted by its keys, the
// order's places gathered, and the voxels gathered through them, so that the shuffled lattice runs line by line with
// stride 1. Its agreement along the axis and the strongest peak it reaches give the draw's height
static int period_record_null(const PeriodRequest *request, const PeriodLattice *lattice, unsigned long long counter,
                              unsigned int axis, unsigned long long *same, PeriodMargin *height, EngineError *error)
{
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned long long voxels = lattice->voxels;
    PeriodShuffle shuffle;
    period_shuffle_fill(&shuffle, &request->content, counter);
    const unsigned int shared[PERIOD_RECORD_KEY_LIMBS] = {lattice->extent[axis], lattice->stride[axis],
                                                          shuffle.keys[0], shuffle.keys[1], shuffle.keys[2],
                                                          shuffle.keys[3]};
    int ok = PERIOD_STATUS_CHECK(cudaMemcpy(resident->shared, shared, sizeof(shared), cudaMemcpyHostToDevice),
                                 resident->shared, error);
    CycleRecordRunRequest sweep;
    memset(&sweep, 0, sizeof(sweep));
    sweep.record = resident->key.record;
    sweep.device_in[0] = resident->shared;
    sweep.bodies[0] = 1ull;
    sweep.count = voxels;
    sweep.device_out = resident->keyed;
    sweep.error = error;
    ok = ok && (cycle_record_run(&sweep) != CYCLE_ERROR);
    const CycleRecordSortRequest sort = {resident->keyed, voxels, lattice->extent[axis], PERIOD_RECORD_KEYED_LIMBS,
                                         resident->key_offset, PERIOD_RECORD_KEY_BITS, resident->order, error};
    ok = ok && (cycle_record_sort(&sort) != CYCLE_ERROR);
    memset(&sweep, 0, sizeof(sweep));
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
        height->numerator = (drawn.candidate != 0ull) ? drawn.margin.numerator : 0ull;
        height->denominator = drawn.pairs_per_lag;
    }
    return ok;
}

// the request's lattice, the programs and the pool, and the lanes widened into one limb each: the copy lays each
// 16-bit lane at the low half of its limb, the high halves cleared before
static int period_record_open(const PeriodRequest *request, PeriodLattice *lattice, unsigned long long *voxels,
                              EngineError *error)
{
    if ((period_lattice_fill(request, lattice, voxels) == 0) ||
        !PERIOD_CHECK(*voxels <= PERIOD_RECORD_VOXELS_MAX, &request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    PeriodRecordResident *const resident = &g_period_record;
    return period_record_make(error) && period_record_reserve(*voxels, error) &&
           PERIOD_STATUS_CHECK(cudaMemset(resident->lanes, 0, (size_t)*voxels * sizeof(unsigned int)), resident->lanes,
                               error) &&
           PERIOD_STATUS_CHECK(cudaMemcpy2D(resident->lanes, sizeof(unsigned int), request->device_lanes,
                                            sizeof(unsigned short), sizeof(unsigned short), (size_t)*voxels,
                                            cudaMemcpyDeviceToDevice),
                               resident->lanes, error);
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
    unsigned long long voxels = 0ull;
    if ((period_record_open(request, &lattice, &voxels, error) == 0) ||
        (period_request_valid(request, lattice.lag_total) == 0))
    {
        return PERIOD_ERROR;
    }
    PeriodRecordResident *const resident = &g_period_record;
    const unsigned long long entries = lattice.lag_total;
    const unsigned long long draws = request->draws;
    const size_t agreement_entries = (size_t)((entries != 0ull) ? entries : 1ull);
    unsigned int *const histogram = (unsigned int *)malloc(PERIOD_VALUES * sizeof(unsigned int));
    unsigned long long *const agreement = (unsigned long long *)calloc(agreement_entries, sizeof(unsigned long long));
    unsigned long long *const shuffled_agreement =
        (unsigned long long *)calloc(agreement_entries, sizeof(unsigned long long));
    const size_t band_slots = (size_t)((draws != 0ull) ? (draws * lattice.rank) : 1ull);
    PeriodMargin *const bands = (PeriodMargin *)calloc(band_slots, sizeof(PeriodMargin));
    unsigned long long band_counts[ENGINE_ARRAY_RANK] = {0ull};
    int ok = PERIOD_CHECK((histogram != NULL) && (agreement != NULL) && (shuffled_agreement != NULL) && (bands != NULL),
                          request, error, ENGINE_ERROR_RESOURCE) &&
             PERIOD_STATUS_CHECK(cudaMemset(resident->histogram, 0, PERIOD_VALUES * sizeof(unsigned int)),
                                 resident->histogram, error);
    if (ok != 0)
    {
        const unsigned long long needed = (voxels + PERIOD_THREADS - 1ull) / PERIOD_THREADS;
        // the block count is held below the record machine's block limit, far below 2^31
        const unsigned int blocks = (unsigned int)((needed < 4096ull) ? needed : 4096ull);
        period_histogram_kernel<<<blocks, PERIOD_THREADS>>>(request->device_lanes, voxels, resident->histogram);
        ok = PERIOD_STATUS_CHECK(cudaGetLastError(), resident->histogram, error) &&
             PERIOD_STATUS_CHECK(cudaMemcpy(histogram, resident->histogram, PERIOD_VALUES * sizeof(unsigned int),
                                            cudaMemcpyDeviceToHost),
                                 histogram, error);
    }
    unsigned long long collisions = 0ull;
    if (ok != 0)
    {
        unsigned long long counted = 0ull;
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
    memset(measurement, 0, sizeof(*measurement));
    measurement->rank = lattice.rank;
    measurement->voxels = voxels;
    measurement->collisions = collisions;
    measurement->draws = draws;
    for (unsigned long long draw = 0ull; (ok != 0) && (voxels >= 2ull) && (draw < draws); draw += 1ull)
    {
        for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
        {
            PeriodMargin height;
            ok = period_record_null(request, &lattice, (draw * lattice.rank) + axis, axis,
                                    &shuffled_agreement[lattice.first[axis]], &height, error);
            if ((ok != 0) && (height.numerator != 0ull))
            {
                bands[(axis * draws) + band_counts[axis]] = height;
                band_counts[axis] += 1ull;
            }
        }
    }
    for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
    {
        period_select(&agreement[lattice.first[axis]], &lattice, axis, &bands[axis * draws], band_counts[axis],
                      (request->null_top != NULL) ? &request->null_top[axis] : NULL, &measurement->axis[axis]);
    }
    if ((ok != 0) && (request->agreement != NULL) && (entries != 0ull))
    {
        memcpy(request->agreement, agreement, (size_t)entries * sizeof(unsigned long long));
    }
    if ((ok != 0) && (request->band != NULL))
    {
        memcpy(request->band, bands, (size_t)(draws * lattice.rank) * sizeof(PeriodMargin));
    }
    free(histogram);
    free(agreement);
    free(shuffled_agreement);
    free(bands);
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
    unsigned long long voxels = 0ull;
    if (period_record_open(request, &lattice, &voxels, error) == 0)
    {
        return PERIOD_ERROR;
    }
    const size_t agreement_entries = (size_t)((lattice.lag_total != 0ull) ? lattice.lag_total : 1ull);
    unsigned long long *const shuffled_agreement =
        (unsigned long long *)calloc(agreement_entries, sizeof(unsigned long long));
    int ok = PERIOD_CHECK(shuffled_agreement != NULL, request, error, ENGINE_ERROR_RESOURCE);
    for (unsigned int axis = 0u; (ok != 0) && (axis < lattice.rank); axis += 1u)
    {
        PeriodAxis open;
        period_axis_open(&lattice, axis, &open);
        heights[axis].numerator = 0ull;
        heights[axis].denominator = open.pairs_per_lag;
        if (voxels >= 2ull)
        {
            ok = period_record_null(request, &lattice, (draw * lattice.rank) + axis, axis,
                                    &shuffled_agreement[lattice.first[axis]], &heights[axis], error);
        }
    }
    free(shuffled_agreement);
    return (ok != 0) ? 0L : PERIOD_ERROR;
}
