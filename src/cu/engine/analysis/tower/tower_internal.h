// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the tower_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef TOWER_INTERNAL_H
#define TOWER_INTERNAL_H

#include "tower.h"

#include "../../../../platform/platform.h"
#include "../../runtime/daemon/tessera_job.h"
#include "../../runtime/device_pool/device_pool.h"

#include <cuda_runtime.h>

#include <stdlib.h>

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

// cudaError_t enumerates non-negative codes below INT_MAX. The status converts to int exactly
#define TOWER_STATUS_CHECK(call_, evacaddr_, error_)                                                                   \
    engine_status_check((int)(call_), ENGINE_MODULE_TOWER, (unsigned int)__LINE__, (const void *)(evacaddr_), (error_))

#define TOWER_CHECK(condition_, evacaddr_, error_, type_)                                                              \
    engine_error_check((condition_), (type_), ENGINE_MODULE_TOWER, (unsigned int)__LINE__, (const void *)(evacaddr_),  \
                       (error_))

#define TOWER_THREADS 256u

#define TOWER_LIMIT ENGINE_COEFFICIENT_LIMIT

struct TowerStep
{
    unsigned long long extent[4];
    unsigned long long stride[4];
    unsigned long long count;
    unsigned int axis;
};

__global__ void tower_forward_kernel(const int *from, int *to, TowerStep step, unsigned int *overflow);

__global__ void tower_inverse_kernel(const int *from, int *to, TowerStep step);

__global__ void tower_copy_kernel(const int *from, int *to, TowerStep step);

__global__ void tower_edge_kernel(int *state, TowerStep region, const unsigned int *table, unsigned int index_bits);

__global__ void tower_widen_kernel(const unsigned short *lanes, unsigned long long count, int *to);

__global__ void tower_take_kernel(const int *values, unsigned long long count, int *to, unsigned int *overflow);

__global__ void tower_differ_kernel(const int *rebuilt, const unsigned short *lanes, unsigned long long count,
                                    unsigned long long *mismatches);

__global__ void tower_narrow_kernel(const int *rebuilt, unsigned long long count, unsigned short *to,
                                    unsigned int *outside);

unsigned int tower_blocks(unsigned long long count);

// The region that the edges of floor `floor_at` act on: the extent of that floor, over the strides of the whole extent.
// Those strides stay as they are across the levels that the lift works in place. The host and a job kernel both read
// the region
__host__ __device__ void tower_edge_region(const struct TowerFloors *floors, const unsigned long long *extent,
                                           unsigned int floor_at, TowerStep *region);

// 1 where each edge is a permutation of its index bits on a floor that the lattice has, else 0 with the error set
int tower_edge_check(const TowerEdge *edges, unsigned int edge_count, unsigned int levels, EngineError *error);

// The body of each kernel above as a walk of its own: the indexes from `first` on, `jump` apart. The kernel walks from
// the index of its thread in its grid. A job kernel (tower_job.cu) walks from the rank of its thread in the whole grid,
// between the syncs of its stages
__device__ void tower_forward_span(const int *from, int *to, TowerStep step, unsigned int *overflow,
                                   unsigned long long first, unsigned long long jump);
__device__ void tower_inverse_span(const int *from, int *to, TowerStep step, unsigned long long first,
                                   unsigned long long jump);
__device__ void tower_copy_span(const int *from, int *to, TowerStep step, unsigned long long first,
                                unsigned long long jump);
__device__ void tower_edge_span(int *state, TowerStep region, const unsigned int *table, unsigned int index_bits,
                                unsigned long long first, unsigned long long jump);
__device__ void tower_widen_span(const unsigned short *lanes, unsigned long long count, int *to,
                                 unsigned long long first, unsigned long long jump);
__device__ void tower_take_span(const int *values, unsigned long long count, int *to, unsigned int *overflow,
                                unsigned long long first, unsigned long long jump);
__device__ void tower_differ_span(const int *rebuilt, const unsigned short *lanes, unsigned long long count,
                                  unsigned long long *mismatches, unsigned long long first, unsigned long long jump);
__device__ void tower_narrow_span(const int *rebuilt, unsigned long long count, unsigned short *to,
                                  unsigned int *outside, unsigned long long first, unsigned long long jump);

// the floors that an extent takes at max. Each floor halves every extent. An extent below 2^64 reaches 1 in 64
#define TOWER_FLOORS_MAX 65u

// the floors of a tower, from the whole extent up, and their count
struct TowerFloors
{
    TowerStep step[TOWER_FLOORS_MAX];
    unsigned int count;
};

void tower_floors(const unsigned long long *extent, TowerFloors *floors);

// The coefficients, the scratch, the flag and the mismatch count are persistent cells of the pool of the device. They
// hold the most lanes that a call asked for. The edge table grows apart, and only where a call puts an edge
struct TowerResident
{
    int *coefficients;
    int *scratch;
    size_t lanes;
    unsigned int *flag;
    unsigned long long *mismatches;
    unsigned int *edge_table;
    size_t edge_entries;
};

extern TowerResident g_tower_resident;

int tower_edge_reserve(unsigned int entries, EngineError *error);

#define TOWER_SLICES 4u

// the owner that the cells of the tower in the pool of the device count to
#define TOWER_PLACE ((unsigned int)platform_place_g_engine_owner)

// The bytes of the cells for `lanes`, in the order that the reserve takes them: the coefficients, the scratch, the flag
// and the mismatch count
void tower_slices(size_t lanes, unsigned long long bytes[TOWER_SLICES]);

// the addresses of the cells of the resident, in the order of tower_slices
void tower_cells(TowerResident *resident, void **cells[TOWER_SLICES]);

unsigned long long tower_lanes(const unsigned long long *extent);

// a constant of the record floors: its value and the register of the step that holds it
struct TowerConstant
{
    unsigned long long value;
    unsigned int step;
};

// The writer of the record floors. It counts every step, and writes a step only where the program of the caller has
// room. It puts each constant once, where a step first reads it, and keeps its register beside its value. `constants`
// holds a TowerConstant for each
struct TowerRecordBuilder
{
    EngineRecordStep *steps;
    unsigned long long count;
    PlatformArray constants;
    int failed;
};

#endif
