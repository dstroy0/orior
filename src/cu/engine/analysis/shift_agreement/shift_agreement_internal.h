// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the shift_agreement_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef SHIFT_AGREEMENT_INTERNAL_H
#define SHIFT_AGREEMENT_INTERNAL_H

#include "shift_agreement.h"

#include "../../runtime/daemon/tessera_job.h"
#include "../../runtime/device_pool/device_pool.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

#define SHIFT_AGREEMENT_BLOCK 256u

static_assert(sizeof(unsigned int) == 4u, "shift_agreement: unsigned int must be 32 bits, a residue");
static_assert(sizeof(unsigned long long) == 8u, "shift_agreement: unsigned long long must be 64 bits, a product");

struct AgreementLayout
{
    unsigned int axes;
    unsigned int extents[SHIFT_AGREEMENT_AXES];
    unsigned int padded[SHIFT_AGREEMENT_AXES];
    unsigned int padded_strides[SHIFT_AGREEMENT_AXES];
    unsigned int weights[SHIFT_AGREEMENT_AXES];
    unsigned int table_offsets[SHIFT_AGREEMENT_AXES];
    unsigned int voxels;
    unsigned int total;
};

__global__ void choice_kernel(unsigned int total, unsigned int *choice);

__global__ void tournament_kernel(const unsigned int *counts, unsigned int *choice, unsigned int stride,
                                  unsigned int pairs, AgreementLayout layout);

static_assert(SHIFT_AGREEMENT_PRIME < (1u << 30u),
              "shift_agreement: Barrett reduction here needs the prime below 2^30");

__global__ void scatter_kernel(const unsigned long long *before, const unsigned long long *after, unsigned int reflect,
                               AgreementLayout layout, unsigned int *reflected, unsigned int *moved);

__global__ void negate_kernel(const unsigned int *source, const unsigned int *negation, AgreementLayout layout,
                              unsigned int *destination);

__global__ void forward_kernel(unsigned int *values, const unsigned int *roots, unsigned int level, unsigned int levels,
                               unsigned int length, unsigned int stride, unsigned int total);

__global__ void inverse_kernel(unsigned int *values, const unsigned int *inverse_roots, unsigned int level,
                               unsigned int levels, unsigned int length, unsigned int stride, unsigned int total);

__global__ void multiply_kernel(unsigned int *values, const unsigned int *other, unsigned int factor,
                                unsigned int total);

unsigned int device_agreement_power(unsigned int base, unsigned long long exponent);

int agreement_launched(void);

// the owner that the cells of the agreement in the pool of the device count to
#define SHIFT_AGREEMENT_PLACE ((unsigned int)platform_place_g_engine_owner)

// A cell of the device that the agreement holds across its calls, a persistent cell of the pool of the device, every
// byte 0. 1, or 0 where the pool cannot meet the bytes
int agreement_cell_taken(void **cell, unsigned long long bytes);

// the cell given back to the pool of the device and the address set to NULL. The call passes over a NULL cell
void agreement_cell_given(void **cell);

struct AxisTables
{
    unsigned int length;
    unsigned int *roots[2];
    unsigned int *negation;
};

const AxisTables *axis_tables(unsigned int length);

#define SHIFT_AGREEMENT_VOLUMES 4u

// The before and after words and the four transform volumes are persistent cells of the pool of the device. They hold
// one total and one word count at a time
struct VolumesResident
{
    size_t total;
    size_t words;
    unsigned long long *before;
    unsigned long long *after;
    unsigned int *volumes[SHIFT_AGREEMENT_VOLUMES];
    unsigned int cached;
    unsigned long long *cached_words;
    unsigned int cached_axes;
    unsigned int cached_extents[SHIFT_AGREEMENT_AXES];
};

static_assert(SHIFT_AGREEMENT_VOLUMES == 4u,
              "shift_agreement: the reserve plans one slice for each of the four volumes");

struct NegationResident
{
    unsigned int axes;
    unsigned int padded[SHIFT_AGREEMENT_AXES];
    unsigned int *negation;
};

#endif
