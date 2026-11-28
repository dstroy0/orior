// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the compression_*.cu pieces share: the checks, the bounds of the code, the walks of the kernels, and the host
// cells of the answer
#ifndef COMPRESSION_INTERNAL_H
#define COMPRESSION_INTERNAL_H

#include "compression.h"

#include "../../../../../includes/platform/platform.h"

#include <cuda_runtime.h>

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

// cudaError_t enumerates non-negative codes below INT_MAX: the status converts to int exactly
#define COMPRESSION_STATUS_CHECK(call_, evacaddr_, error_)                                                             \
    engine_status_check((int)(call_), ENGINE_MODULE_COMPRESSION, (unsigned int)__LINE__, (const void *)(evacaddr_),    \
                        (error_))

#define COMPRESSION_CHECK(condition_, evacaddr_, error_, type_)                                                        \
    engine_error_check((condition_), (type_), ENGINE_MODULE_COMPRESSION, (unsigned int)__LINE__,                       \
                       (const void *)(evacaddr_), (error_))

#define COMPRESSION_THREADS 256u

#define COMPRESSION_BLOCK 64ULL

#define COMPRESSION_CHUNK_BLOCKS 64ULL

#define COMPRESSION_CHUNK (COMPRESSION_BLOCK * COMPRESSION_CHUNK_BLOCKS)

#define COMPRESSION_K_BITS 5u

#define COMPRESSION_ESCAPE 24u

// The body of each kernel of compression as a walk of its own: the indexes from `first` on, `jump` apart. The kernel
// walks from the index of its thread in its grid. A job kernel (compression_job.cu) walks from the rank of its thread
// in the whole grid, between the syncs of its stages. The zigzag walks the values, and the others walk the chunks
__device__ void compression_zigzag_span(const int *from, unsigned long long count, unsigned int *to,
                                        unsigned long long first, unsigned long long jump);
__device__ void compression_measure_span(const unsigned int *values, unsigned long long count,
                                         unsigned long long chunks, unsigned long long *chunk_bits,
                                         unsigned long long first, unsigned long long jump);
__device__ void compression_write_span(const unsigned int *values, unsigned long long count, unsigned long long chunks,
                                       const unsigned long long *offsets, unsigned int *stream,
                                       unsigned long long first, unsigned long long jump);
__device__ void compression_read_span(const unsigned int *stream, const unsigned long long *offsets,
                                      unsigned long long count, unsigned long long chunks, int *to,
                                      unsigned long long first, unsigned long long jump);

// The host cells of an encoded answer, which compression keeps until its next encode: room for `chunks` offsets and
// `limbs` words of the stream. 1, or 0 with the error set where the cells cannot grow
int compression_host_room(unsigned long long chunks, unsigned long long limbs, unsigned long long **offsets,
                          unsigned int **stream, EngineError *error);

#endif
