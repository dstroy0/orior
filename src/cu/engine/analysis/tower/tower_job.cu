// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_job.cu: the lift and the lower of the tower as stages of a job of the device (tower_job.h), and each as a job
// of its own (tower_lift_job, tower_lower_job). The host crosses the lattice or the crystal and the tables of the edges
// in. Each stage takes its scratch on the device, syncs the grid between its steps, and leaves its flags in its answer.
// The host crosses the answer out, and agrees with the flags as tower_lift and tower_lower do.
#include "tower_job.h"

#include "../../../../platform/platform.h"
#include "../../../../platform/platform_device.h"
#include "memoria_operor/memoria_operor.h"
#include "tower_internal.h"

// One edge as the job reads it: its floor, its index bits and its table in the memory of the device. A lower also reads
// the place of its inverse table in the one cell of the inverse tables, which the lower takes and fills
struct TowerJobEdge
{
    unsigned int floor;
    unsigned int index_bits;
    const unsigned int *table;
    unsigned long long inverse_at;
};

// What the lift reads, which crosses in as one cell: the lattice, the floors, the edges and the cells of its answer
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

// the scratch of a stage, which its first thread takes on the device and gives back at the end
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

// The lift. It takes the scratch, widens or takes the lattice, and runs the edges of floor 0. Then it runs each level
// along each axis, with the edges of the next floor. Each step reads what the step before it wrote, after a sync of
// the grid. Every write of the flag comes before a sync. Each thread thus reads the same flag at the end
__device__ int tower_lift_staged(const void *job_record, const cooperative_groups::grid_group &grid)
{
    // Explicit cast reads the record of the stage as the lift that crossed into it
    const TowerLiftJob *const job = (const TowerLiftJob *)job_record;
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
        return 0;
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
    const int clear = (*job->flag == 0u) ? 1 : 0;
    grid.sync();
    if (first == 0ULL)
    {
        platform_device_cell_released(s_tower_scratch, platform_place_g_engine_owner);
        s_tower_scratch = NULL;
    }
    return clear;
}

__global__ void tower_lift_job_kernel(const void *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    (void)tower_lift_staged(job, grid);
}

int tower_lift_stage_crossed(const TowerLiftJobRequest *request, TesseraJobCells *cells, TowerStage *stage)
{
    EngineError *const error = request->error;
    if (!TOWER_CHECK(((request->lanes != NULL) || (request->values != NULL)) && (request->floors != NULL) &&
                         ((request->edges != NULL) || (request->edge_count == 0u)) &&
                         (request->edge_count <= TOWER_FLOORS_MAX),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    TowerLiftJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.count = tower_lanes(request->extent);
    if (!TOWER_CHECK(job.count != 0ULL, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        job.extent[axis] = request->extent[axis];
    }
    tower_floors(request->extent, &job.floors);
    if (tower_edge_check(request->edges, request->edge_count, job.floors.count, error) == 0)
    {
        return 0;
    }
    TowerJobEdge edges[TOWER_FLOORS_MAX];
    void *cell = NULL;
    int ok = (request->values != NULL)
                 ? tessera_job_cells_in(cells, request->values, job.count * sizeof(int), &cell)
                 : tessera_job_cells_in(cells, request->lanes, job.count * sizeof(unsigned short), &cell);
    // Explicit casts read the cell as the lattice that crossed into it
    job.values = (request->values != NULL) ? (const int *)cell : NULL;
    job.lanes = (request->values != NULL) ? NULL : (const unsigned short *)cell;
    for (unsigned int at = 0u; (ok != 0) && (at < request->edge_count); at += 1u)
    {
        edges[at].floor = request->edges[at].floor;
        edges[at].index_bits = request->edges[at].index_bits;
        edges[at].inverse_at = 0ULL;
        ok = tessera_job_cells_in(cells, request->edges[at].forward,
                                  (1ULL << request->edges[at].index_bits) * sizeof(unsigned int), &cell);
        // Explicit cast reads the cell as the table that crossed into it
        edges[at].table = (const unsigned int *)cell;
    }
    job.edge_count = request->edge_count;
    ok = ok &&
         ((request->edge_count == 0u) ||
          tessera_job_cells_in(cells, edges, (unsigned long long)request->edge_count * sizeof(TowerJobEdge), &cell));
    // Explicit cast reads the cell as the edges that crossed into it
    job.edges = (request->edge_count != 0u) ? (const TowerJobEdge *)cell : NULL;
    ok = ok && tessera_job_cells_room(cells, job.count * sizeof(int), &cell);
    // Explicit cast reads the cell as the room of the coefficients
    job.coefficients = (int *)cell;
    ok = ok && tessera_job_cells_room(cells, sizeof(unsigned int), &cell);
    // Explicit cast reads the cell as the flag of the lift
    job.flag = (unsigned int *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(cells, &job, sizeof(job), &job_cell);
    stage->job = job_cell;
    stage->coefficients = job.coefficients;
    stage->answer = job.flag;
    stage->rebuilt = NULL;
    stage->count = job.count;
    stage->floors = job.floors.count;
    return TOWER_CHECK(ok != 0, request, error, ENGINE_ERROR_RESOURCE);
}

int tower_lift_stage_agreed(const TowerLiftJobRequest *request, const TowerStage *stage)
{
    EngineError *const error = request->error;
    unsigned int flag = 1u;
    const int ok =
        TOWER_CHECK(tessera_job_crossed_out(&flag, stage->answer, sizeof(flag)), request, error,
                    ENGINE_ERROR_RESOURCE) &&
        TOWER_CHECK(flag != 2u, request, error, ENGINE_ERROR_RESOURCE) &&
        TOWER_CHECK(flag == 0u, request, error, ENGINE_ERROR_REQUEST) &&
        ((request->coefficients == NULL) ||
         TOWER_CHECK(tessera_job_crossed_out(request->coefficients, stage->coefficients, stage->count * sizeof(int)),
                     request, error, ENGINE_ERROR_RESOURCE));
    if (ok != 0)
    {
        *request->floors = stage->floors;
    }
    return ok;
}

extern "C" long tower_lift_job(const TowerLiftJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK(request->coefficients != NULL, request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    TowerStage stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
    int ok = tower_lift_stage_crossed(request, &cells, &stage);
    const void *job_cell = stage.job;
    // Explicit cast gives the address of the record of the stage as the one argument of the kernel
    void *args[1] = {(void *)&job_cell};
    ok = ok &&
         TOWER_CHECK(tessera_job_ran((const void *)tower_lift_job_kernel, TOWER_THREADS, args), request, error,
                     ENGINE_ERROR_RESOURCE) &&
         tower_lift_stage_agreed(request, &stage);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : TOWER_ERROR;
}

// What the lower answers. The first is the count of the samples that differ from the lanes. The second is a flag of 1
// where a rebuilt value is outside 16 bits, or 2 where the device refuses a cell of the job
struct TowerLowerAnswer
{
    unsigned long long mismatches;
    unsigned int outside;
};

// What the lower reads, which crosses in as one cell. It holds the crystal, the lanes to compare, the floors and the
// edges. It also holds the words of the inverse tables and the cells of the answer
struct TowerLowerJob
{
    int *coefficients;
    const unsigned short *lanes;
    unsigned long long count;
    unsigned long long extent[4];
    TowerFloors floors;
    const TowerJobEdge *edges;
    unsigned int edge_count;
    unsigned long long inverse_entries;
    unsigned short *rebuilt;
    TowerLowerAnswer *answer;
};

// the one cell of the inverse tables of the lower, which its first thread takes after the scratch
static __device__ unsigned int *s_tower_inverse;

// The edges of floor `floor_at` undone, the last edge first, each through its inverse table, then a sync of the grid
static __device__ void tower_job_edges_undone(const TowerLowerJob *job, unsigned int floor_at,
                                              const cooperative_groups::grid_group &grid)
{
    TowerStep region;
    tower_edge_region(&job->floors, job->extent, floor_at, &region);
    for (unsigned int scan = 0u; scan < job->edge_count; scan += 1u)
    {
        const TowerJobEdge *const edge = &job->edges[job->edge_count - 1u - scan];
        if (edge->floor != floor_at)
        {
            continue;
        }
        tower_edge_span(job->coefficients, region, s_tower_inverse + edge->inverse_at, edge->index_bits,
                        grid.thread_rank(), grid.size());
        grid.sync();
    }
}

// The lower. It takes the scratch and the cell of the inverse tables, and fills each inverse table from its forward
// one. It undoes the edges of the last floor, then each level along each axis in reverse with the edges before it.
// Then it counts the samples that differ from the lanes and narrows the crystal into the rebuilt samples
__device__ int tower_lower_staged(const void *job_record, const cooperative_groups::grid_group &grid)
{
    // Explicit cast reads the record of the stage as the lower that crossed into it
    const TowerLowerJob *const job = (const TowerLowerJob *)job_record;
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    if (first == 0ULL)
    {
        // Explicit casts take the addresses of the cells as their ints and words
        s_tower_scratch = (int *)platform_device_cell_taken(job->count * sizeof(int), platform_place_g_engine_owner, 1);
        s_tower_inverse = (unsigned int *)platform_device_cell_taken(job->inverse_entries * sizeof(unsigned int),
                                                                     platform_place_g_engine_owner, 1);
        if ((s_tower_scratch == NULL) || ((s_tower_inverse == NULL) && (job->inverse_entries != 0ULL)))
        {
            job->answer->outside = 2u;
        }
    }
    grid.sync();
    if (job->answer->outside == 2u)
    {
        return 0;
    }
    // each inverse table, as many words as its forward one, the index that each word of the forward table names
    for (unsigned int at = 0u; at < job->edge_count; at += 1u)
    {
        const TowerJobEdge *const edge = &job->edges[at];
        const unsigned long long entries = 1ULL << edge->index_bits;
        for (unsigned long long index = first; index < entries; index += jump)
        {
            // Explicit cast takes the index below 2^TOWER_EDGE_INDEX_BITS_MAX, which a word holds
            s_tower_inverse[edge->inverse_at + edge->table[index]] = (unsigned int)index;
        }
    }
    grid.sync();
    tower_job_edges_undone(job, job->floors.count, grid);
    for (unsigned int level = job->floors.count; level > 0u; level -= 1u)
    {
        for (unsigned int axis = 4u; axis > 0u; axis -= 1u)
        {
            TowerStep step = job->floors.step[level - 1u];
            if (step.extent[axis - 1u] < 2ULL)
            {
                continue;
            }
            step.axis = axis - 1u;
            tower_inverse_span(job->coefficients, s_tower_scratch, step, first, jump);
            grid.sync();
            tower_copy_span(s_tower_scratch, job->coefficients, step, first, jump);
            grid.sync();
        }
        // the edges that the lift ran before the lifting of this level
        tower_job_edges_undone(job, level - 1u, grid);
    }
    if (job->lanes != NULL)
    {
        tower_differ_span(job->coefficients, job->lanes, job->count, &job->answer->mismatches, first, jump);
    }
    tower_narrow_span(job->coefficients, job->count, job->rebuilt, &job->answer->outside, first, jump);
    grid.sync();
    const int clear = (job->answer->outside == 0u) ? 1 : 0;
    grid.sync();
    if (first == 0ULL)
    {
        platform_device_cell_released(s_tower_inverse, platform_place_g_engine_owner);
        platform_device_cell_released(s_tower_scratch, platform_place_g_engine_owner);
        s_tower_inverse = NULL;
        s_tower_scratch = NULL;
    }
    return clear;
}

__global__ void tower_lower_job_kernel(const void *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    (void)tower_lower_staged(job, grid);
}

int tower_lower_stage_crossed(const TowerLowerJobRequest *request, TesseraJobCells *cells, TowerStage *stage)
{
    EngineError *const error = request->error;
    if (!TOWER_CHECK((request->mismatches != NULL) && (request->rebuilt != NULL) &&
                         ((request->edges != NULL) || (request->edge_count == 0u)) &&
                         (request->edge_count <= TOWER_FLOORS_MAX),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    TowerLowerJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.count = tower_lanes(request->extent);
    if (!TOWER_CHECK(job.count != 0ULL, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        job.extent[axis] = request->extent[axis];
    }
    tower_floors(request->extent, &job.floors);
    if (tower_edge_check(request->edges, request->edge_count, job.floors.count, error) == 0)
    {
        return 0;
    }
    TowerJobEdge edges[TOWER_FLOORS_MAX];
    void *cell = NULL;
    // the crystal crosses in, or is a room that an earlier stage writes
    int ok = (request->coefficients != NULL)
                 ? tessera_job_cells_in(cells, request->coefficients, job.count * sizeof(int), &cell)
                 : tessera_job_cells_room(cells, job.count * sizeof(int), &cell);
    // Explicit cast reads the cell as the crystal, which the lower works on in place
    job.coefficients = (int *)cell;
    ok = ok && ((request->lanes == NULL) ||
                tessera_job_cells_in(cells, request->lanes, job.count * sizeof(unsigned short), &cell));
    // Explicit cast reads the cell as the lanes that crossed into it
    job.lanes = (request->lanes != NULL) ? (const unsigned short *)cell : NULL;
    for (unsigned int at = 0u; (ok != 0) && (at < request->edge_count); at += 1u)
    {
        const unsigned long long entries = 1ULL << request->edges[at].index_bits;
        edges[at].floor = request->edges[at].floor;
        edges[at].index_bits = request->edges[at].index_bits;
        edges[at].inverse_at = job.inverse_entries;
        job.inverse_entries += entries;
        ok = tessera_job_cells_in(cells, request->edges[at].forward, entries * sizeof(unsigned int), &cell);
        // Explicit cast reads the cell as the table that crossed into it
        edges[at].table = (const unsigned int *)cell;
    }
    job.edge_count = request->edge_count;
    ok = ok &&
         ((request->edge_count == 0u) ||
          tessera_job_cells_in(cells, edges, (unsigned long long)request->edge_count * sizeof(TowerJobEdge), &cell));
    // Explicit cast reads the cell as the edges that crossed into it
    job.edges = (request->edge_count != 0u) ? (const TowerJobEdge *)cell : NULL;
    ok = ok && tessera_job_cells_room(cells, job.count * sizeof(unsigned short), &cell);
    // Explicit cast reads the cell as the room of the rebuilt samples
    job.rebuilt = (unsigned short *)cell;
    ok = ok && tessera_job_cells_room(cells, sizeof(TowerLowerAnswer), &cell);
    // Explicit cast reads the cell as the answer of the lower
    job.answer = (TowerLowerAnswer *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(cells, &job, sizeof(job), &job_cell);
    stage->job = job_cell;
    stage->coefficients = job.coefficients;
    stage->answer = job.answer;
    stage->rebuilt = job.rebuilt;
    stage->count = job.count;
    stage->floors = job.floors.count;
    return TOWER_CHECK(ok != 0, request, error, ENGINE_ERROR_RESOURCE);
}

int tower_lower_stage_agreed(const TowerLowerJobRequest *request, const TowerStage *stage)
{
    EngineError *const error = request->error;
    TowerLowerAnswer answer = {0ULL, 1u};
    const int ok =
        TOWER_CHECK(tessera_job_crossed_out(&answer, stage->answer, sizeof(answer)), request, error,
                    ENGINE_ERROR_RESOURCE) &&
        TOWER_CHECK(answer.outside != 2u, request, error, ENGINE_ERROR_RESOURCE) &&
        TOWER_CHECK(answer.outside == 0u, request, error, ENGINE_ERROR_LOGIC) &&
        TOWER_CHECK(tessera_job_crossed_out(request->rebuilt, stage->rebuilt, stage->count * sizeof(unsigned short)),
                    request, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        *request->mismatches = answer.mismatches;
    }
    return ok;
}

extern "C" long tower_lower_job(const TowerLowerJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK(request->coefficients != NULL, request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    TowerStage stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
    int ok = tower_lower_stage_crossed(request, &cells, &stage);
    const void *job_cell = stage.job;
    // Explicit cast gives the address of the record of the stage as the one argument of the kernel
    void *args[1] = {(void *)&job_cell};
    ok = ok &&
         TOWER_CHECK(tessera_job_ran((const void *)tower_lower_job_kernel, TOWER_THREADS, args), request, error,
                     ENGINE_ERROR_RESOURCE) &&
         tower_lower_stage_agreed(request, &stage);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : TOWER_ERROR;
}
