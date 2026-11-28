// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file scan.cu
 * @brief The steering scan on a CUDA device, one alignment per thread over the whole object.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * THE SAME QUESTION EVERY SCAN ARM ANSWERS, MAPPED OVER THE DEVICE. Fixed at one needle offset, count
 * the alignments still located whose corpus byte equals the needle byte. The vector arms widen one
 * compare; this one gives one alignment to each thread and adds a one where that alignment agrees and
 * is. The reduction is a single atomic increment per agreeing thread, what the exact
 * device arm does and needs no tuning at this size.
 *
 * WHAT THIS ARM IS NOT ALLOWED TO DO. It returns the portable arm's count or it has a defect. The
 * value is an integer count of alignments and the two arms agree exactly or one is wrong. That is
 * the contract OriorSteerEngine carries, the same one the exact arms carry in no_rounding.
 *
 * @note NOT SHARED SOURCE WITH THE HOST ARMS. A device function carries __global__ or __device__ on
 *       every declaration. The kernel cannot be the portable loop compiled twice, and the two can
 *       drift. The GPU build grades every count this arm returns against the portable host arm on
 *       the same data, what catches a drift.
 * @note THE WHOLE OBJECT CROSSES THE BUS PER CALL. The corpus and the survivor vector are copied to
 *       the device. This arm pays a transfer a host arm does not. It wins only where the object is
 *       large enough to amortize that. It is graded and timed and not placed in
 *       orior_steer_best_engine.
 * @note A device error falls back to a host count instead of returning a sentinel into a table of
 *       counts, matching arm.cu. A driver comparing arms would otherwise read the sentinel as a
 *       disagreement and blame the scan for what was an allocation failure.
 */

#include "../../../../../includes/platform/platform.h"
#include "../../runtime/daemon/tessera_job.h"
#include "orior.h"

#include <cuda_runtime.h>

/** @brief Threads per block. 256 suits every device this is built for and needs no tuning here. */
#define ORIOR_STEER_GPU_BLOCK 256u

/**
 * @brief One thread per alignment: add one where it is and its corpus byte is the wanted one.
 *
 * @param[in]     corpus     Bytes under examination [BORROWS].
 * @param[in]     alignments How many alignments the object has.
 * @param[in]     alive      One flag per alignment, non-zero where still located [BORROWS].
 * @param[in]     wanted     The needle byte being tested.
 * @param[in]     offset     Needle offset the probe sits at.
 * @param[in,out] located   Where the count is accumulated [BORROWS].
 */
__global__ static void scan_kernel(const unsigned char *corpus, size_t alignments, const unsigned char *alive,
                                   unsigned char wanted, size_t offset, unsigned int *located)
{
    const size_t at = (size_t)blockIdx.x * (size_t)blockDim.x + (size_t)threadIdx.x;
    if (at >= alignments)
    {
        return;
    }
    if (alive[at] == 0u)
    {
        return;
    }
    if (corpus[at + offset] == wanted)
    {
        atomicAdd(located, 1u);
    }
}

extern "C" int orior_steer_cuda_available(void)
{
    int devices = 0;
    if (cudaGetDeviceCount(&devices) != cudaSuccess)
    {
        return 0;
    }
    return (devices > 0) ? 1 : 0;
}

extern "C" int orior_steer_cuda_describe(char *text, size_t capacity)
{
    int devices = 0;
    if ((cudaGetDeviceCount(&devices) != cudaSuccess) || (devices <= 0))
    {
        return 0;
    }
    cudaDeviceProp properties;
    if (cudaGetDeviceProperties(&properties, 0) != cudaSuccess)
    {
        return 0;
    }
    VerbaLinea formed_line_1 = mmgr_line_over(text, capacity);
    mmgr_line_put(&formed_line_1, properties.name);
    mmgr_line_put(&formed_line_1, ", compute ");
    mmgr_line_decimal_signed(&formed_line_1, properties.major);
    mmgr_line_put(&formed_line_1, ".");
    mmgr_line_decimal_signed(&formed_line_1, properties.minor);
    mmgr_line_put(&formed_line_1, ", ");
    mmgr_line_decimal_signed(&formed_line_1, properties.multiProcessorCount);
    mmgr_line_put(&formed_line_1, " SMs");
    (void)mmgr_line_finish(&formed_line_1);
    (void)(int)formed_line_1.at;
    return 1;
}

/**
 * @brief Counts on the host, without touching the scan counters, for the device fallback.
 *
 * @note orior_steer_truthy_after_cuda has already counted the call. The fallback must not reach
 *       orior_steer_truthy_after_portable, which would count it a second time. The loop is the same
 *       one the portable arm runs.
 */
static size_t scan_host_fallback(const unsigned char *corpus, size_t alignments, const unsigned char *alive,
                                 unsigned char wanted, size_t offset)
{
    size_t located = 0u;
    for (size_t at = 0u; at < alignments; at += 1u)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (corpus[at + offset] == wanted)
        {
            located += 1u;
        }
    }
    return located;
}

extern "C" size_t orior_steer_truthy_after_cuda(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                                uint8_t wanted, size_t offset)
{
    /* Counted before the argument check. A caller passing nothing still records that this arm was
     * the one requested. The claim the counters carry is which arm RAN and not what it returned. */
    orior_steer_scan_calls += 1u;
    orior_steer_wide_calls += 1u;

    if ((corpus == NULL) || (alive == NULL) || (alignments == 0u))
    {
        return 0u;
    }

    const unsigned char *host_corpus = (const unsigned char *)corpus;
    const unsigned char *host_alive = (const unsigned char *)alive;
    const size_t corpus_bytes = alignments + offset;

    // The corpus and the survivors cross in. The count is a temporary cell of the pool of the device, 0 at the take.
    // The call gives the cells back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, (unsigned int)platform_place_g_engine_owner);
    void *cell[3] = {NULL, NULL, NULL};
    int ok = tessera_job_cells_in(&cells, host_corpus, (unsigned long long)corpus_bytes, &cell[0]) &&
             tessera_job_cells_in(&cells, host_alive, (unsigned long long)alignments, &cell[1]) &&
             tessera_job_cells_room(&cells, sizeof(unsigned int), &cell[2]);
    // Explicit casts read each cell as the bytes and the count that it holds
    const unsigned char *const device_corpus = (const unsigned char *)cell[0];
    const unsigned char *const device_alive = (const unsigned char *)cell[1];
    unsigned int *const device_located = (unsigned int *)cell[2];
    if (ok != 0)
    {
        const unsigned int blocks =
            (unsigned int)((alignments + (size_t)ORIOR_STEER_GPU_BLOCK - 1u) / (size_t)ORIOR_STEER_GPU_BLOCK);
        scan_kernel<<<blocks, ORIOR_STEER_GPU_BLOCK>>>(device_corpus, alignments, device_alive, (unsigned char)wanted,
                                                       offset, device_located);
        ok = (cudaGetLastError() == cudaSuccess) && (cudaDeviceSynchronize() == cudaSuccess);
    }
    unsigned int host_count = 0u;
    ok = ok && tessera_job_crossed_out(&host_count, device_located, sizeof(unsigned int));
    tessera_job_cells_given(&cells);
    const size_t answer = (ok != 0) ? (size_t)host_count : (size_t)-1;

    if (answer == (size_t)-1)
    {
        return scan_host_fallback(host_corpus, alignments, host_alive, (unsigned char)wanted, offset);
    }
    return answer;
}

extern "C" const OriorSteerEngine *orior_steer_cuda_engine(void)
{
    static const OriorSteerEngine engine = {"cuda", orior_steer_truthy_after_cuda};

    return orior_steer_cuda_available() ? &engine : NULL;
}
