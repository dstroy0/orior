// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the raster_cuda_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef RASTER_CUDA_INTERNAL_H
#define RASTER_CUDA_INTERNAL_H

/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file raster_cuda_internal.h
 * @brief The device arm of the direct renderer. Same configuration, same bytes as the host arm.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * ONE THREAD FOR EACH ALIGNMENT. The render is the search, and the parallel decomposition is that
 * of the search. Every alignment is independent until it reduces into a pixel. Nothing goes in
 * tiles and nothing goes in shared memory, because each thread reads a handful of bytes and writes
 * one value.
 *
 * WHY THIS AGREES WITH THE HOST BYTE FOR BYTE. Three properties bind it together. The value that a
 * thread computes is integer arithmetic on the same inputs, thus no rounding can differ. The cell
 * that a thread targets comes from integer division and a permutation, and neither depends on
 * which thread runs. The reduction is a minimum or a max, both associative and commutative.
 *
 * The scheduler can interleave the atomics in any order and reach the same result. A reduction
 * that selects by arrival would make the device answer depend on the schedule. Then no grade
 * against the host would be possible.
 *
 * @note This file writes the transform and the channel again. MinGW builds the host arm through
 *       CMake, and nvcc builds this with MSVC, and the two cannot link. That is the same split that
 *       utils/maint/engine/build_gpu_arm.sh already documents for the exact arm. Where a device is
 *       present, bench_raster grades the two rasters byte for byte, and a divergence fails a row.
 * @warning A copy is a defect that will occur, and this one is safe only because a grader compares
 *          the outputs on every configuration. Delete that grader, and this file becomes a second
 *          renderer that nobody checks.
 */

#include <cstdio>
#include <cuda_runtime.h>

extern "C"
{
#include "orior_raster.h"
}

/** @brief Death level step in the gray ramp. Matches ORIOR_RASTER_STEP in orior_raster_internal.h. */
#define RASTER_STEP 40u

/** @brief Brightest value a death level may reach. Matches ORIOR_RASTER_CEILING. */
#define RASTER_CEILING 250u

/** @brief Probe as the kernel reads it, arranged to match OriorRasterProbe exactly. */
struct DeviceProbe
{
    unsigned long long origin;
    unsigned long long step;
    unsigned long long length;
};

/** @brief Configuration as the kernel reads it. It carries only the fields that a thread needs. */
struct DeviceConfig
{
    unsigned long long width;
    unsigned long long height;
    int layout;
    int channel;
    int reduce;
    unsigned int gain;
};

__global__ void render_alignments(unsigned int *staging, DeviceConfig config, const unsigned char *corpus,
                                  unsigned long long corpus_len, const unsigned char *needle,
                                  unsigned long long needle_len, const DeviceProbe *probes,
                                  unsigned long long probe_count, const unsigned long long *occurrences,
                                  unsigned long long total);

extern "C" int orior_raster_device_available(void);

/** @brief Volume configuration as the kernel reads it. The layout maps an alignment to a voxel. */
struct DeviceVolumeConfig
{
    unsigned long long width;
    unsigned long long height;
    unsigned long long depth;
    int layout;
    int channel;
    int reduce;
    unsigned int gain;
};

__global__ void render_volume(unsigned int *staging, int *error, DeviceVolumeConfig vconfig,
                              const unsigned char *corpus, unsigned long long corpus_len, const unsigned char *needle,
                              unsigned long long needle_len, const DeviceProbe *probes, unsigned long long probe_count,
                              const unsigned long long *occurrences, unsigned long long total);

#endif
