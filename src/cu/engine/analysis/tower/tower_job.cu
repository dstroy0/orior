// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_job.cu: the lift of the tower as one job of the device (tower_lift_job). The host crosses the lattice and the
// tables of the edges in, and the job lifts the lattice in one cooperative launch. It takes its scratch on the device,
// syncs the grid between its stages, and leaves the overflow in its answer. The host crosses the coefficients and the
// flag out, and agrees with the flag as tower_lift does.
#include "../../../../platform/platform.h"
#include "../../../../platform/platform_device.h"
#include "../../runtime/daemon/tessera_job.h"
#include "memoria_operor/memoria_operor.h"
#include "tower_internal.h"

#include <cooperative_groups.h>

// one edge as the job reads it: its floor, its index bits and its table in the memory of the device
struct TowerJobEdge
{
    unsigned int floor;
    unsigned int index_bits;
    const unsigned int *table;
};

// What the job reads, which crosses in as one cell: the lattice, the floors, the edges and the cells of its answer
struct TowerLiftJob
{
    const unsigned short *lanes;
    const int *values;
    unsigned long long count;
    unsigned long long extent[4];
    TowerFloors floors;
    const TowerJobEdge *edges;
    unsigned int edge_count;
    int *coefficients;
    unsigned int *flag;
};

// the scratch of the job, which its first thread takes on the device and gives back at the end
static __device__ int *s_tower_scratch;

// The edges of floor `floor_at`, each over the region of that floor, then a sync of the grid
static __device__ void tower_job_edges(const TowerLiftJob *job, unsigned int floor_at,
                                       const cooperative_groups::grid_group &grid)
{
    TowerStep region;
    tower_edge_region(&job->floors, job->extent, floor_at, &region);
    for (unsigned int at = 0u; at < job->edge_count; at += 1u)
    {
        const TowerJobEdge *const edge = &job->edges[at];
        if (edge->floor != floor_at)
        {
            continue;
        }
        tower_edge_span(job->coefficients, region, edge->table, edge->index_bits, grid.thread_rank(), grid.size());
        grid.sync();
    }
}

// The job. It takes the scratch, widens or takes the lattice, and runs the edges of floor 0. Then it runs each level
// along each axis, with the edges of the next floor. Each stage reads what the stage before it wrote, after a sync of
// the grid
__global__ void tower_lift_job_kernel(const TowerLiftJob *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    if (first == 0ULL)
    {
        // Explicit cast takes the address of the cell as its ints
        s_tower_scratch = (int *)platform_device_cell_taken(job->count * sizeof(int), platform_place_g_engine_owner, 1);
        // a refused scratch is a flag of 2, which the host reads as a refused cell
        if (s_tower_scratch == NULL)
        {
            *job->flag = 2u;
        }
    }
    grid.sync();
    if (s_tower_scratch == NULL)
    {
        return;
    }
    if (job->values != NULL)
    {
        tower_take_span(job->values, job->count, job->coefficients, job->flag, first, jump);
    }
    else
    {
        tower_widen_span(job->lanes, job->count, job->coefficients, first, jump);
    }
    grid.sync();
    tower_job_edges(job, 0u, grid);
    for (unsigned int level = 0u; level < job->floors.count; level += 1u)
    {
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            TowerStep step = job->floors.step[level];
            if (step.extent[axis] < 2ULL)
            {
                continue;
            }
            step.axis = axis;
            tower_forward_span(job->coefficients, s_tower_scratch, step, job->flag, first, jump);
            grid.sync();
            tower_copy_span(s_tower_scratch, job->coefficients, step, first, jump);
            grid.sync();
        }
        // the edges of the next floor act on the approximation that this level made
        tower_job_edges(job, level + 1u, grid);
    }
    if (first == 0ULL)
    {
        platform_device_cell_released(s_tower_scratch, platform_place_g_engine_owner);
        s_tower_scratch = NULL;
    }
}

// The cells that a lift crosses in, last first as the temporary tier gives them back, and their count
#define TOWER_JOB_CELLS_MAX (TOWER_FLOORS_MAX + 6u)
struct TowerJobCells
{
    void *cell[TOWER_JOB_CELLS_MAX];
    unsigned int count;
};

// `count` bytes of `bytes` crossed in, the cell kept for its give back. 1, or 0 where the crossing errors
static int tower_job_in(TowerJobCells *cells, const void *bytes, unsigned long long count, void **cell)
{
    if ((cells->count >= TOWER_JOB_CELLS_MAX) ||
        !tessera_job_crossed_in(bytes, count, platform_place_g_engine_owner, cell))
    {
        return 0;
    }
    cells->cell[cells->count] = *cell;
    cells->count += 1u;
    return 1;
}

// an empty cell of `count` bytes taken for the answer, kept for its give back. 1, or 0 where the take errors
static int tower_job_room(TowerJobCells *cells, unsigned long long count, void **cell)
{
    if ((cells->count >= TOWER_JOB_CELLS_MAX) || !tessera_job_cell_taken(count, platform_place_g_engine_owner, cell))
    {
        return 0;
    }
    cells->cell[cells->count] = *cell;
    cells->count += 1u;
    return 1;
}

// every cell of the lift given back, the newest first
static void tower_job_given(TowerJobCells *cells)
{
    while (cells->count > 0u)
    {
        cells->count -= 1u;
        tessera_job_given(cells->cell[cells->count], platform_place_g_engine_owner);
    }
}

extern "C" long tower_lift_job(const TowerLiftJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK(((request->lanes != NULL) || (request->values != NULL)) && (request->coefficients != NULL) &&
                         (request->floors != NULL) && ((request->edges != NULL) || (request->edge_count == 0u)) &&
                         (request->edge_count <= TOWER_FLOORS_MAX),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    TowerLiftJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.count = tower_lanes(request->extent);
    if (!TOWER_CHECK(job.count != 0ULL, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        job.extent[axis] = request->extent[axis];
    }
    tower_floors(request->extent, &job.floors);
    if (tower_edge_check(request->edges, request->edge_count, job.floors.count, error) == 0)
    {
        return TOWER_ERROR;
    }
    TowerJobCells cells;
    cells.count = 0u;
    TowerJobEdge edges[TOWER_FLOORS_MAX];
    void *cell = NULL;
    int ok = (request->values != NULL)
                 ? tower_job_in(&cells, request->values, job.count * sizeof(int), &cell)
                 : tower_job_in(&cells, request->lanes, job.count * sizeof(unsigned short), &cell);
    // Explicit casts read the cell as the lattice that crossed into it
    job.values = (request->values != NULL) ? (const int *)cell : NULL;
    job.lanes = (request->values != NULL) ? NULL : (const unsigned short *)cell;
    for (unsigned int at = 0u; (ok != 0) && (at < request->edge_count); at += 1u)
    {
        edges[at].floor = request->edges[at].floor;
        edges[at].index_bits = request->edges[at].index_bits;
        ok = tower_job_in(&cells, request->edges[at].forward,
                          (1ULL << request->edges[at].index_bits) * sizeof(unsigned int), &cell);
        // Explicit cast reads the cell as the table that crossed into it
        edges[at].table = (const unsigned int *)cell;
    }
    job.edge_count = request->edge_count;
    ok = ok && ((request->edge_count == 0u) ||
                tower_job_in(&cells, edges, (unsigned long long)request->edge_count * sizeof(TowerJobEdge), &cell));
    // Explicit cast reads the cell as the edges that crossed into it
    job.edges = (request->edge_count != 0u) ? (const TowerJobEdge *)cell : NULL;
    ok = ok && tower_job_room(&cells, job.count * sizeof(int), &cell);
    // Explicit cast reads the cell as the room of the coefficients
    job.coefficients = (int *)cell;
    ok = ok && tower_job_room(&cells, sizeof(unsigned int), &cell);
    // Explicit cast reads the cell as the flag of the job
    job.flag = (unsigned int *)cell;
    void *job_cell = NULL;
    ok = ok && tower_job_in(&cells, &job, sizeof(job), &job_cell);
    void *args[1] = {&job_cell};
    ok = TOWER_CHECK((ok != 0) && tessera_job_ran((const void *)tower_lift_job_kernel, TOWER_THREADS, args), request,
                     error, ENGINE_ERROR_RESOURCE);
    unsigned int flag = 1u;
    ok = ok &&
         TOWER_CHECK(tessera_job_crossed_out(&flag, job.flag, sizeof(flag)), request, error, ENGINE_ERROR_RESOURCE);
    ok = ok && TOWER_CHECK(flag != 2u, request, error, ENGINE_ERROR_RESOURCE) &&
         TOWER_CHECK(flag == 0u, request, error, ENGINE_ERROR_REQUEST) &&
         TOWER_CHECK(tessera_job_crossed_out(request->coefficients, job.coefficients, job.count * sizeof(int)), request,
                     error, ENGINE_ERROR_RESOURCE);
    tower_job_given(&cells);
    if (ok == 0)
    {
        return TOWER_ERROR;
    }
    *request->floors = job.floors.count;
    return 0L;
}
