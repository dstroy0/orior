// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_run.cu: allocation, edges, lifting and lowering
#include "../../../../platform/platform.h"
#include "tower_internal.h"

static int tower_reserve(size_t lanes, EngineError *error)
{
    TowerResident *const resident = &g_tower_resident;
    if (lanes <= resident->lanes)
    {
        return 1;
    }
    // the cells given back, the newest first, then taken again at the new count of lanes
    void **cells[TOWER_SLICES];
    tower_cells(resident, cells);
    for (unsigned int at = TOWER_SLICES; at > 0u; at -= 1u)
    {
        tessera_job_given(*cells[at - 1u], TOWER_PLACE);
        *cells[at - 1u] = NULL;
    }
    resident->lanes = 0u;
    unsigned long long bytes[TOWER_SLICES];
    tower_slices(lanes, bytes);
    int ok = 1;
    for (unsigned int at = 0u; (ok != 0) && (at < TOWER_SLICES); at += 1u)
    {
        ok = TOWER_CHECK(tessera_job_persistent_taken(bytes[at], TOWER_PLACE, cells[at]), cells[at], error,
                         ENGINE_ERROR_RESOURCE);
    }
    resident->lanes = (ok != 0) ? lanes : 0u;
    return ok;
}

extern "C" unsigned long long tower_reserve_bytes(unsigned long long lanes)
{
    // past 2^60 lanes the two int slices alone pass the plan's 2^62 bytes, which spoils it; errored here before the
    // product could wrap
    if ((lanes == 0ULL) || (lanes > (1ULL << 60u)))
    {
        return 0ULL;
    }
    unsigned long long bytes[TOWER_SLICES];
    // a size_t represents 64 bits, which device_pool asserts. The lane count converts exactly
    tower_slices((size_t)lanes, bytes);
    DevicePoolPlan plan = {0ULL, 0ULL, 0};
    for (unsigned int at = 0u; at < TOWER_SLICES; at += 1u)
    {
        device_pool_plan_slice(&plan, bytes[at]);
    }
    return device_pool_plan_bytes(&plan);
}

unsigned long long tower_lanes(const unsigned long long *extent)
{
    unsigned long long lanes = 1ULL;
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        if ((extent[axis] == 0ULL) || (lanes > (0xFFFFFFFFFFFFFFFFULL / extent[axis])))
        {
            return 0ULL;
        }
        lanes *= extent[axis];
    }
    return lanes;
}

int tower_edge_check(const TowerEdge *edges, unsigned int edge_count, unsigned int levels, EngineError *error)
{
    for (unsigned int at = 0u; at < edge_count; at += 1u)
    {
        const TowerEdge *const edge = &edges[at];
        if (!TOWER_CHECK((edge->forward != NULL) && (edge->index_bits >= 1u) &&
                             (edge->index_bits <= TOWER_EDGE_INDEX_BITS_MAX) && (edge->floor <= levels),
                         edge, error, ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
        const unsigned long long entries = 1ULL << edge->index_bits;
        unsigned char *const seen = (unsigned char *)platform_cell_cleared(
            (unsigned long long)((size_t)entries), (unsigned long long)(sizeof(unsigned char)),
            (unsigned int)platform_place_g_engine_owner, 0);
        if (!TOWER_CHECK(seen != NULL, edge, error, ENGINE_ERROR_RESOURCE))
        {
            return 0;
        }
        int permutation = 1;
        for (unsigned long long index = 0ULL; (permutation != 0) && (index < entries); index += 1ULL)
        {
            const unsigned int value = edge->forward[index];
            if ((value >= entries) || (seen[value] != 0u))
            {
                permutation = 0;
            }
            else
            {
                seen[value] = 1u;
            }
        }
        platform_cell_released(seen, (unsigned int)platform_place_g_engine_owner);
        if (!TOWER_CHECK(permutation != 0, edge, error, ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
    }
    return 1;
}

static int tower_edge_run(const TowerEdge *edges, unsigned int edge_count, const TowerFloors &floors,
                          const unsigned long long *extent, unsigned int floor_at, int inverse, EngineError *error)
{
    TowerResident *const resident = &g_tower_resident;
    TowerStep region;
    tower_edge_region(&floors, extent, floor_at, &region);
    int ok = 1;
    // a lift walks a floor's edges in array order, a lower walks them in reverse. A floor inverts as a stack
    for (unsigned int scan = 0u; (ok != 0) && (scan < edge_count); scan += 1u)
    {
        const unsigned int at = (inverse != 0) ? (edge_count - 1u - scan) : scan;
        const TowerEdge *const edge = &edges[at];
        if (edge->floor != floor_at)
        {
            continue;
        }
        const unsigned long long entries = 1ULL << edge->index_bits;
        if (tower_edge_reserve((unsigned int)entries, error) == 0)
        {
            return 0;
        }
        const unsigned int *upload = edge->forward;
        unsigned int *inverted = NULL;
        if (inverse != 0)
        {
            inverted = (unsigned int *)platform_cell_taken((unsigned long long)((size_t)entries * sizeof(unsigned int)),
                                                           (unsigned int)platform_place_g_engine_owner, 0);
            if (!TOWER_CHECK(inverted != NULL, edge, error, ENGINE_ERROR_RESOURCE))
            {
                return 0;
            }
            for (unsigned long long index = 0ULL; index < entries; index += 1ULL)
            {
                inverted[edge->forward[index]] = (unsigned int)index;
            }
            upload = inverted;
        }
        ok = TOWER_CHECK(
            tessera_job_crossed_into(resident->edge_table, upload, (unsigned long long)entries * sizeof(unsigned int)),
            resident->edge_table, error, ENGINE_ERROR_RESOURCE);
        if (ok != 0)
        {
            tower_edge_kernel<<<tower_blocks(region.count), TOWER_THREADS>>>(resident->coefficients, region,
                                                                             resident->edge_table, edge->index_bits);
            ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->coefficients, error);
        }
        platform_cell_released(inverted, (unsigned int)platform_place_g_engine_owner);
    }
    return ok;
}

extern "C" long tower_lift(const TowerLiftRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK(((request->device_lanes != NULL) || (request->device_values != NULL)) &&
                         (request->coefficients != NULL) && (request->scratch != NULL) && (request->floors != NULL),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    const unsigned long long lanes = tower_lanes(request->extent);
    TowerResident *const resident = &g_tower_resident;
    if (!TOWER_CHECK(lanes != 0ULL, request->extent, error, ENGINE_ERROR_REQUEST) ||
        (tower_reserve((size_t)lanes, error) == 0))
    {
        return TOWER_ERROR;
    }
    TowerFloors floors;
    tower_floors(request->extent, &floors);
    if (tower_edge_check(request->edges, request->edge_count, (unsigned int)floors.count, error) == 0)
    {
        return TOWER_ERROR;
    }
    int ok = TOWER_CHECK(tessera_job_cleared(resident->flag, sizeof(unsigned int)), resident->flag, error,
                         ENGINE_ERROR_RESOURCE);
    if ((ok != 0) && (request->device_values != NULL))
    {
        tower_take_kernel<<<tower_blocks(lanes), TOWER_THREADS>>>(request->device_values, lanes, resident->coefficients,
                                                                  resident->flag);
        ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->coefficients, error);
    }
    else if (ok != 0)
    {
        tower_widen_kernel<<<tower_blocks(lanes), TOWER_THREADS>>>(request->device_lanes, lanes,
                                                                   resident->coefficients);
        ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->coefficients, error);
    }
    // floor 0 is before the first lifting level
    ok = ok && tower_edge_run(request->edges, request->edge_count, floors, request->extent, 0u, 0, error);
    for (size_t level = 0u; (ok != 0) && (level < floors.count); level += 1u)
    {
        for (unsigned int axis = 0u; (ok != 0) && (axis < 4u); axis += 1u)
        {
            TowerStep step = floors.step[level];
            if (step.extent[axis] < 2ULL)
            {
                continue;
            }
            step.axis = axis;
            tower_forward_kernel<<<tower_blocks(step.count), TOWER_THREADS>>>(resident->coefficients, resident->scratch,
                                                                              step, resident->flag);
            tower_copy_kernel<<<tower_blocks(step.count), TOWER_THREADS>>>(resident->scratch, resident->coefficients,
                                                                           step);
            ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->coefficients, error);
        }
        // The edges of the next floor act on the approximation that this level made
        ok = ok && tower_edge_run(request->edges, request->edge_count, floors, request->extent,
                                  (unsigned int)(level + 1u), 0, error);
    }
    unsigned int overflow = 1u;
    ok = ok &&
         TOWER_CHECK(tessera_job_crossed_out(&overflow, resident->flag, sizeof(unsigned int)), &overflow, error,
                     ENGINE_ERROR_RESOURCE) &&
         TOWER_CHECK(overflow == 0u, &overflow, error, ENGINE_ERROR_REQUEST);
    if (ok == 0)
    {
        return TOWER_ERROR;
    }
    *request->coefficients = resident->coefficients;
    *request->scratch = (unsigned int *)resident->scratch;
    *request->floors = (unsigned int)floors.count;
    return 0L;
}

extern "C" long tower_capacity(const unsigned long long extent[4], int **coefficients, EngineError *error)
{
    if (error == NULL)
    {
        return TOWER_ERROR;
    }
    if (!TOWER_CHECK((extent != NULL) && (coefficients != NULL), &coefficients, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    const unsigned long long lanes = tower_lanes(extent);
    if (!TOWER_CHECK(lanes != 0ULL, extent, error, ENGINE_ERROR_REQUEST) || (tower_reserve((size_t)lanes, error) == 0))
    {
        return TOWER_ERROR;
    }
    *coefficients = g_tower_resident.coefficients;
    return 0L;
}

extern "C" long tower_lower(const TowerLowerRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK((request->mismatches != NULL) && (request->device_rebuilt != NULL), request, error,
                     ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    const unsigned long long lanes = tower_lanes(request->extent);
    TowerResident *const resident = &g_tower_resident;
    if (!TOWER_CHECK((lanes != 0ULL) && (lanes <= resident->lanes), request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    TowerFloors floors;
    tower_floors(request->extent, &floors);
    if (tower_edge_check(request->edges, request->edge_count, (unsigned int)floors.count, error) == 0)
    {
        return TOWER_ERROR;
    }
    // the collapsed floor's edge is undone first, then each level and the edge that preceded it, in reverse
    int ok = tower_edge_run(request->edges, request->edge_count, floors, request->extent, (unsigned int)floors.count, 1,
                            error);
    for (size_t level = floors.count; (ok != 0) && (level > 0u); level -= 1u)
    {
        for (unsigned int axis = 4u; (ok != 0) && (axis > 0u); axis -= 1u)
        {
            TowerStep step = floors.step[level - 1u];
            if (step.extent[axis - 1u] < 2ULL)
            {
                continue;
            }
            step.axis = axis - 1u;
            tower_inverse_kernel<<<tower_blocks(step.count), TOWER_THREADS>>>(resident->coefficients, resident->scratch,
                                                                              step);
            tower_copy_kernel<<<tower_blocks(step.count), TOWER_THREADS>>>(resident->scratch, resident->coefficients,
                                                                           step);
            ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->coefficients, error);
        }
        // undo the edge that was before this level's lifting on the lift pass
        ok = ok && tower_edge_run(request->edges, request->edge_count, floors, request->extent,
                                  (unsigned int)(level - 1u), 1, error);
    }
    ok = ok && TOWER_CHECK(tessera_job_cleared(resident->mismatches, sizeof(unsigned long long)), resident->mismatches,
                           error, ENGINE_ERROR_RESOURCE);
    if ((ok != 0) && (request->device_lanes != NULL))
    {
        tower_differ_kernel<<<tower_blocks(lanes), TOWER_THREADS>>>(resident->coefficients, request->device_lanes,
                                                                    lanes, resident->mismatches);
        ok = TOWER_STATUS_CHECK(cudaGetLastError(), resident->mismatches, error);
    }
    ok = ok &&
         TOWER_CHECK(tessera_job_crossed_out(request->mismatches, resident->mismatches, sizeof(unsigned long long)),
                     request->mismatches, error, ENGINE_ERROR_RESOURCE);
    // the scratch contains lanes ints. Its first half contains the lanes' 16-bit narrowing with room to spare
    unsigned short *const narrowed = (unsigned short *)resident->scratch;
    unsigned int outside = 1u;
    ok = ok && TOWER_CHECK(tessera_job_cleared(resident->flag, sizeof(unsigned int)), resident->flag, error,
                           ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        tower_narrow_kernel<<<tower_blocks(lanes), TOWER_THREADS>>>(resident->coefficients, lanes, narrowed,
                                                                    resident->flag);
        ok = TOWER_STATUS_CHECK(cudaGetLastError(), narrowed, error);
    }
    ok = ok &&
         TOWER_CHECK(tessera_job_crossed_out(&outside, resident->flag, sizeof(unsigned int)), &outside, error,
                     ENGINE_ERROR_RESOURCE) &&
         TOWER_CHECK(outside == 0u, &outside, error, ENGINE_ERROR_LOGIC);
    if ((ok != 0) && (request->rebuilt != NULL))
    {
        ok = TOWER_CHECK(
            tessera_job_crossed_out(request->rebuilt, narrowed, (unsigned long long)lanes * sizeof(unsigned short)),
            request->rebuilt, error, ENGINE_ERROR_RESOURCE);
    }
    if (ok == 0)
    {
        return TOWER_ERROR;
    }
    *request->device_rebuilt = narrowed;
    return 0L;
}
