// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_residual.cu: the residual: keys, planes and their growth
#include "engine_internal.h"
#include "memoria_operor/memoria_linea.h"
#include "runtime/device_pool/device_pool.h"

static EngineResidualResident s_residual_resident;

static EngineResidualPlanesResident s_residual_planes_resident;

static EngineResidualResults s_residual_results;

static int engine_residual_key(const EngineResidualRequest *request, unsigned int limbs)
{
    EngineResidualResident *const resident = &s_residual_resident;
    EngineError *const error = request->error;
    if ((resident->key != NULL) &&
        (mmgr_bytes_compare(resident->smooth_orders, request->smooth_orders, sizeof(resident->smooth_orders)) == 0) &&
        (mmgr_bytes_compare(resident->background_orders, request->background_orders,
                            sizeof(resident->background_orders)) == 0) &&
        (mmgr_bytes_compare(resident->comb, request->comb, sizeof(resident->comb)) == 0) &&
        (mmgr_bytes_compare(resident->smooth_spaced, request->smooth_spaced, sizeof(resident->smooth_spaced)) == 0) &&
        (mmgr_bytes_compare(resident->background_spaced, request->background_spaced,
                            sizeof(resident->background_spaced)) == 0))
    {
        return 1;
    }
    cycle_key_release(resident->key);
    resident->key = NULL;
    EngineStep program[RESIDUAL_STEPS];
    if (!ENGINE_CHECK(residual_program(request, program) != RESIDUAL_ERROR, request->smooth_orders, error,
                      ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    CycleKey *key = NULL;
    const long bits = engine_key_encode(program, RESIDUAL_STEPS, &key, error);
    // bits is checked non-negative before it re-signs to unsigned long
    const int imprinted =
        ENGINE_CHECK(bits >= 0L, program, error, ENGINE_ERROR_REQUEST) &&
        ENGINE_CHECK((unsigned long)bits <= (32UL * limbs), request->background_orders, error, ENGINE_ERROR_REQUEST);
    if (imprinted == 0)
    {
        cycle_key_release(key);
        return 0;
    }
    resident->key = key;
    mmgr_bytes_copy(resident->smooth_orders, request->smooth_orders, sizeof(resident->smooth_orders));
    mmgr_bytes_copy(resident->background_orders, request->background_orders, sizeof(resident->background_orders));
    mmgr_bytes_copy(resident->comb, request->comb, sizeof(resident->comb));
    mmgr_bytes_copy(resident->smooth_spaced, request->smooth_spaced, sizeof(resident->smooth_spaced));
    mmgr_bytes_copy(resident->background_spaced, request->background_spaced, sizeof(resident->background_spaced));
    return 1;
}

static void engine_residual_free(void)
{
    EngineResidualResident *const resident = &s_residual_resident;
    cudaFree(resident->volume);
    cudaFree(resident->residual);
    cudaFree(resident->check);
    resident->volume = NULL;
    resident->residual = NULL;
    resident->check = NULL;
    resident->volume_voxels = 0u;
    resident->residual_words = 0u;
    resident->check_words = 0u;
}

// a buffer retained while it is large enough, and made again where a request needs more
static int engine_residual_grow(void **buffer, size_t *buffer_size, size_t wanted, size_t unit, EngineError *error)
{
    if (wanted <= *buffer_size)
    {
        return 1;
    }
    cudaFree(*buffer);
    *buffer = NULL;
    *buffer_size = 0u;
    const int ok = ENGINE_STATUS_CHECK(cudaMalloc(buffer, wanted * unit), buffer, error);
    *buffer_size = (ok != 0) ? wanted : 0u;
    return ok;
}

// the lanes, the residual in `limbs` limbs a lane, and the check where the residual is proved
static int engine_residual_reserve(size_t voxels, unsigned int limbs, int proving, EngineError *error)
{
    EngineResidualResident *const resident = &s_residual_resident;
    const size_t words = voxels * limbs;
    const int ok = engine_residual_grow((void **)&resident->volume, &resident->volume_voxels, voxels,
                                        sizeof(unsigned short), error) &&
                   engine_residual_grow((void **)&resident->residual, &resident->residual_words, words,
                                        sizeof(unsigned int), error) &&
                   ((proving == 0) || engine_residual_grow((void **)&resident->check, &resident->check_words, words,
                                                           sizeof(unsigned int), error));
    if (ok == 0)
    {
        engine_residual_free();
    }
    return ok;
}

// the unit sweep's request for a residual request, its lanes not yet on the device
static void engine_residual_sweep_of(const EngineResidualRequest *request, UnitSweepRequest *sweep)
{
    mmgr_bytes_set(sweep, 0, sizeof(*sweep));
    sweep->depth = request->depth;
    sweep->height = request->height;
    sweep->width = request->width;
    mmgr_bytes_copy(sweep->smooth_orders, request->smooth_orders, sizeof(sweep->smooth_orders));
    mmgr_bytes_copy(sweep->background_orders, request->background_orders, sizeof(sweep->background_orders));
    mmgr_bytes_copy(sweep->comb, request->comb, sizeof(sweep->comb));
    mmgr_bytes_copy(sweep->smooth_spaced, request->smooth_spaced, sizeof(sweep->smooth_spaced));
    mmgr_bytes_copy(sweep->background_spaced, request->background_spaced, sizeof(sweep->background_spaced));
}

extern "C" unsigned long long engine_residual_limbs(const EngineResidualRequest *request)
{
    UnitSweepRequest sweep;
    engine_residual_sweep_of(request, &sweep);
    return (unit_sweep_bits(&sweep) + 31ULL) / 32ULL;
}

extern "C" unsigned long long engine_residual_bytes(const EngineResidualRequest *request)
{
    const unsigned long long voxels = (unsigned long long)request->depth * request->height * request->width;
    const unsigned long long limbs = (request->limbs != 0u) ? request->limbs : ENGINE_RESIDUAL_LIMBS;
    // each allocation is mapped in whole pages
    const unsigned long long page = DEVICE_POOL_PAGE_BYTES - 1ULL;
    const unsigned long long lanes = ((voxels * sizeof(unsigned short)) + page) & ~page;
    const unsigned long long residual = ((voxels * limbs * sizeof(unsigned int)) + page) & ~page;
    UnitSweepRequest sweep;
    engine_residual_sweep_of(request, &sweep);
    const int proving = (request->unit_sweep == ENGINE_RESIDUAL_BOTH_PROVED) ? 1 : 0;
    const int sweep_runs = (request->unit_sweep != ENGINE_RESIDUAL_BY_KEY) ? 1 : 0;
    return lanes + residual + ((proving != 0) ? residual : 0ULL) +
           ((sweep_runs != 0) ? unit_sweep_bytes(&sweep) : 0ULL);
}

extern "C" void engine_resident_release(void)
{
    EngineResidualResident *const resident = &s_residual_resident;
    engine_residual_free();
    cycle_key_release(resident->key);
    resident->key = NULL;
    EngineResidualPlanesResident *const planes = &s_residual_planes_resident;
    cudaFree(planes->planes);
    cudaFree(planes->residual);
    mmgr_bytes_set(planes, 0, sizeof(*planes));
    unit_sweep_release();
    max_tree_resident_release();
    tower_resident_release();
    compression_resident_release();
    cycle_resident_release();
    cycle_record_sum_release();
}

// the parity of the half voxels both terms move along an axis: the smooth order's, and a comb of n's n - 1
static unsigned int engine_residual_half(const unsigned int smooth_orders[ENGINE_AXES],
                                         const unsigned int comb[ENGINE_AXES], unsigned int axis)
{
    const unsigned int comb_moves = (comb[axis] >= 2u) ? (comb[axis] - 1u) : 0u;
    return (smooth_orders[axis] + comb_moves) & 1u;
}

// the bits a comb adds on its axis, bit_length(n - 1); 0 and 1 add none
static unsigned long long engine_residual_comb_bits(unsigned int length)
{
    unsigned long long bits = 0ULL;
    for (unsigned int value = (length >= 2u) ? (length - 1u) : 0u; value != 0u; value >>= 1u)
    {
        bits += 1ULL;
    }
    return bits;
}

// an odd sum of the smooth order and the comb's n - 1 puts the residual half a voxel before its lane's voxel. It is
// allowed only where the request gives the place to report so.
static int engine_residual_placed(const unsigned int smooth_orders[ENGINE_AXES], const unsigned int comb[ENGINE_AXES],
                                  int *const *offset_halves, EngineError *error)
{
    unsigned int odd = 0u;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        odd |= engine_residual_half(smooth_orders, comb, axis);
    }
    return ENGINE_CHECK((odd == 0u) || (*offset_halves != NULL), offset_halves, error, ENGINE_ERROR_REQUEST);
}

// the residual's place per axis in half voxels, written once the residual is made
static void engine_residual_offset(const unsigned int smooth_orders[ENGINE_AXES], const unsigned int comb[ENGINE_AXES],
                                   int *offset_halves)
{
    for (unsigned int axis = 0u; (offset_halves != NULL) && (axis < ENGINE_AXES); axis += 1u)
    {
        // a parity is 0 or 1, which re-signs to int exactly
        offset_halves[axis] = -(int)engine_residual_half(smooth_orders, comb, axis);
    }
}

extern "C" long engine_residual(const EngineResidualRequest *request, const unsigned int **device_residual)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return ENGINE_ERROR;
    }
    EngineError *const error = request->error;
    if (!ENGINE_CHECK(device_residual != NULL, &device_residual, error, ENGINE_ERROR_REQUEST))
    {
        return ENGINE_ERROR;
    }
    *device_residual = NULL;
    const int valid = ENGINE_CHECK(request->volume != NULL, &request->volume, error, ENGINE_ERROR_REQUEST) &&
                      ENGINE_CHECK((request->depth != 0u) && (request->height != 0u) && (request->width != 0u),
                                   &request->depth, error, ENGINE_ERROR_REQUEST) &&
                      engine_residual_placed(request->smooth_orders, request->comb, &request->offset_halves, error);
    if (valid == 0)
    {
        return ENGINE_ERROR;
    }
    const unsigned long long plane_voxels = (unsigned long long)request->height * request->width;
    const unsigned long long voxels = (plane_voxels <= 0xFFFFFFFFULL) ? (plane_voxels * request->depth) : 0ULL;
    const unsigned int limbs = (request->limbs != 0u) ? request->limbs : ENGINE_RESIDUAL_LIMBS;
    const int proving = (request->unit_sweep == ENGINE_RESIDUAL_BOTH_PROVED) ? 1 : 0;
    const int sized = ENGINE_CHECK((voxels != 0ULL) && (voxels <= (0xFFFFFFFFULL / ENGINE_RESIDUAL_LIMBS)),
                                   &request->depth, error, ENGINE_ERROR_REQUEST) &&
                      ENGINE_CHECK(limbs <= ENGINE_RESIDUAL_LIMBS, &request->limbs, error, ENGINE_ERROR_REQUEST) &&
                      engine_residual_key(request, limbs);
    if (sized == 0)
    {
        return ENGINE_ERROR;
    }
    EngineResidualResident *const resident = &s_residual_resident;
    int steps_succeeded =
        engine_residual_reserve((size_t)voxels, limbs, proving, error) &&
        ENGINE_STATUS_CHECK(cudaMemcpy(resident->volume, request->volume, (size_t)voxels * sizeof(unsigned short),
                                       cudaMemcpyHostToDevice),
                            resident->volume, error);
    Atom atom;
    mmgr_bytes_set(&atom, 0, sizeof(atom));
    atom.lanes = resident->volume;
    atom.depth = request->depth;
    atom.height = request->height;
    atom.width = request->width;
    CycleRunRequest cycle_request;
    mmgr_bytes_set(&cycle_request, 0, sizeof(cycle_request));
    cycle_request.key = resident->key;
    cycle_request.atoms = &atom;
    cycle_request.count = 1ULL;
    cycle_request.limbs = limbs;
    cycle_request.device_out = resident->residual;
    cycle_request.error = error;
    UnitSweepRequest sweep_request;
    engine_residual_sweep_of(request, &sweep_request);
    sweep_request.device_volume = resident->volume;
    sweep_request.limbs = limbs;
    sweep_request.device_out = (proving != 0) ? resident->check : resident->residual;
    sweep_request.error = error;
    const int key_runs = (request->unit_sweep != ENGINE_RESIDUAL_BY_UNIT_SWEEP) ? 1 : 0;
    const int sweep_runs = (request->unit_sweep != ENGINE_RESIDUAL_BY_KEY) ? 1 : 0;
    steps_succeeded = steps_succeeded && ENGINE_STATUS_CHECK(cudaDeviceSynchronize(), resident->volume, error);
    const unsigned long long key_started = engine_clock_microseconds();
    steps_succeeded = steps_succeeded &&
                      ((key_runs == 0) ||
                       (ENGINE_CHECK(cycle_run(&cycle_request) == 1L, resident->key, error, ENGINE_ERROR_RESOURCE) &&
                        ENGINE_STATUS_CHECK(cudaDeviceSynchronize(), resident->residual, error)));
    const unsigned long long sweep_started = engine_clock_microseconds();
    steps_succeeded =
        steps_succeeded &&
        ((sweep_runs == 0) || (ENGINE_CHECK(unit_sweep_residual(&sweep_request) == 0L, sweep_request.device_out, error,
                                            ENGINE_ERROR_RESOURCE) &&
                               ENGINE_STATUS_CHECK(cudaDeviceSynchronize(), sweep_request.device_out, error)));
    const unsigned long long sweep_finished = engine_clock_microseconds();
    unsigned long long differing_lanes = 0ULL;
    const UnitSweepComparison comparison = {
        resident->residual, resident->check, voxels, limbs, &differing_lanes, error};
    steps_succeeded =
        steps_succeeded && ((proving == 0) || ENGINE_CHECK(unit_sweep_lanes_compare(&comparison) == 0L, resident->check,
                                                           error, ENGINE_ERROR_RESOURCE));
    if (steps_succeeded && (proving != 0))
    {
        s_residual_results.proved_frames += 1ULL;
        s_residual_results.differing_frames += (differing_lanes != 0ULL) ? 1ULL : 0ULL;
        s_residual_results.differing_lanes += differing_lanes;
        steps_succeeded = ENGINE_CHECK(differing_lanes == 0ULL, resident->check, error, ENGINE_ERROR_LOGIC);
    }
    if (steps_succeeded == 0)
    {
        engine_residual_free();
        return ENGINE_ERROR;
    }
    s_residual_results.frames += 1ULL;
    s_residual_results.key_microseconds += (key_runs != 0) ? (sweep_started - key_started) : 0ULL;
    s_residual_results.sweep_microseconds += (sweep_runs != 0) ? (sweep_finished - sweep_started) : 0ULL;
    engine_residual_offset(request->smooth_orders, request->comb, request->offset_halves);
    *device_residual = resident->residual;
    return 0L;
}

static int engine_residual_planes_grow(unsigned int **words, size_t *capacity, size_t wanted, EngineError *error)
{
    if (wanted <= *capacity)
    {
        return 1;
    }
    cudaFree(*words);
    *words = NULL;
    *capacity = 0u;
    const int ok = ENGINE_STATUS_CHECK(cudaMalloc((void **)words, wanted * sizeof(unsigned int)), words, error);
    *capacity = (ok != 0) ? wanted : 0u;
    return ok;
}

extern "C" long engine_residual_planes(const EngineResidualPlanesRequest *request, const unsigned int **device_residual,
                                       unsigned int *limbs)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return ENGINE_ERROR;
    }
    EngineError *const error = request->error;
    if (!ENGINE_CHECK((device_residual != NULL) && (limbs != NULL), &device_residual, error, ENGINE_ERROR_REQUEST))
    {
        return ENGINE_ERROR;
    }
    *device_residual = NULL;
    *limbs = 0u;
    unsigned long long residual_bits = (unsigned long long)request->input_bits + 1ULL;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        residual_bits += (unsigned long long)request->smooth_orders[axis] + request->background_orders[axis] +
                         engine_residual_comb_bits(request->comb[axis]);
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            residual_bits += 2ULL * ((unsigned long long)request->smooth_spaced[axis][spacing] +
                                     request->background_spaced[axis][spacing]);
        }
    }
    const unsigned long long plane_voxels = (unsigned long long)request->height * request->width;
    const unsigned long long voxels = (plane_voxels <= 0xFFFFFFFFULL) ? (plane_voxels * request->depth) : 0ULL;
    const unsigned long long residual_limbs = (residual_bits + 31ULL) / 32ULL;
    const unsigned long long input_limbs = ((unsigned long long)request->input_bits + 31ULL) / 32ULL;
    const int valid =
        ENGINE_CHECK(request->planes != NULL, &request->planes, error, ENGINE_ERROR_REQUEST) &&
        ENGINE_CHECK(request->input_bits != 0u, &request->input_bits, error, ENGINE_ERROR_REQUEST) &&
        ENGINE_CHECK((voxels != 0ULL) && (voxels <= 0xFFFFFFFFULL), &request->depth, error, ENGINE_ERROR_REQUEST) &&
        ENGINE_CHECK(residual_limbs <= (0xFFFFFFFFULL / 32ULL), request->background_orders, error,
                     ENGINE_ERROR_REQUEST) &&
        engine_residual_placed(request->smooth_orders, request->comb, &request->offset_halves, error);
    EngineResidualPlanesResident *const resident = &s_residual_planes_resident;
    int ok =
        valid &&
        engine_residual_planes_grow(&resident->planes, &resident->plane_words, (size_t)(voxels * input_limbs), error) &&
        engine_residual_planes_grow(&resident->residual, &resident->residual_words, (size_t)(voxels * residual_limbs),
                                    error) &&
        ENGINE_STATUS_CHECK(cudaMemcpy(resident->planes, request->planes,
                                       (size_t)(voxels * input_limbs) * sizeof(unsigned int), cudaMemcpyHostToDevice),
                            resident->planes, error);
    UnitSweepRequest sweep_request;
    mmgr_bytes_set(&sweep_request, 0, sizeof(sweep_request));
    sweep_request.device_planes = resident->planes;
    sweep_request.input_bits = request->input_bits;
    sweep_request.depth = request->depth;
    sweep_request.height = request->height;
    sweep_request.width = request->width;
    mmgr_bytes_copy(sweep_request.smooth_orders, request->smooth_orders, sizeof(sweep_request.smooth_orders));
    mmgr_bytes_copy(sweep_request.background_orders, request->background_orders,
                    sizeof(sweep_request.background_orders));
    mmgr_bytes_copy(sweep_request.comb, request->comb, sizeof(sweep_request.comb));
    mmgr_bytes_copy(sweep_request.smooth_spaced, request->smooth_spaced, sizeof(sweep_request.smooth_spaced));
    mmgr_bytes_copy(sweep_request.background_spaced, request->background_spaced,
                    sizeof(sweep_request.background_spaced));
    // the limbs are bounded at or below 2^32 / 32 above. They narrow to unsigned int exactly
    sweep_request.limbs = (unsigned int)residual_limbs;
    sweep_request.device_out = resident->residual;
    sweep_request.error = error;
    const unsigned long long sweep_started = engine_clock_microseconds();
    ok = ok &&
         ENGINE_CHECK(unit_sweep_residual(&sweep_request) == 0L, sweep_request.device_out, error,
                      ENGINE_ERROR_RESOURCE) &&
         ENGINE_STATUS_CHECK(cudaDeviceSynchronize(), resident->residual, error);
    if (ok == 0)
    {
        return ENGINE_ERROR;
    }
    s_residual_results.frames += 1ULL;
    s_residual_results.sweep_microseconds += engine_clock_microseconds() - sweep_started;
    engine_residual_offset(request->smooth_orders, request->comb, request->offset_halves);
    *device_residual = resident->residual;
    *limbs = sweep_request.limbs;
    return 0L;
}

static EngineBodiesResults s_bodies_results;

extern "C" long engine_frame_bodies(const EngineBodiesRequest *request, EngineLeaves *leaves)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return ENGINE_ERROR;
    }
    EngineError *const error = request->error;
    if (ENGINE_CHECK(leaves != NULL, &leaves, error, ENGINE_ERROR_REQUEST) == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    mmgr_bytes_set(leaves, 0, sizeof(*leaves));
    const unsigned int *device_residual = NULL;
    EngineResidualRequest residual = request->residual;
    residual.error = error;
    if (ENGINE_CHECK(engine_residual(&residual, &device_residual) == 0L, &request->residual, error,
                     ENGINE_ERROR_REQUEST) == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    unsigned int level_code = 0u;
    unsigned int proven = 0u;
    MaxTreeObjectsRequest objects;
    mmgr_bytes_set(&objects, 0, sizeof(objects));
    objects.error = error;
    objects.device_residual = device_residual;
    objects.depth = request->residual.depth;
    objects.height = request->residual.height;
    objects.width = request->residual.width;
    objects.capacity = request->capacity;
    objects.bodies = request->bodies;
    objects.labels = request->labels;
    objects.positive_words = request->positive_words;
    objects.level_code = &level_code;
    objects.proof_count = &proven;
    const long count = max_tree_objects(&objects);
    const int grown =
        (count >= 0L) && ENGINE_CHECK(grow_leaves(request->bodies, (unsigned int)count, leaves, error) != GROW_ERROR,
                                      leaves, error, ENGINE_ERROR_REQUEST);
    if (grown == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    s_bodies_results.frames += 1ULL;
    s_bodies_results.proven += (unsigned long long)proven;
    s_bodies_results.bodies += (unsigned long long)count;
    s_bodies_results.levels += (unsigned long long)level_code;
    return count;
}

extern "C" void engine_bodies_results(EngineBodiesResults *results)
{
    *results = s_bodies_results;
}

extern "C" void engine_residual_results(EngineResidualResults *results)
{
    *results = s_residual_results;
}

extern "C" void engine_group_voxels(const EngineGroupRequest *request)
{
    grow_group_voxels(request);
}
