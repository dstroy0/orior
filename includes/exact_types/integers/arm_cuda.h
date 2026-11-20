// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file arm_cuda.h
 * @brief The exact measure on a CUDA device, parallel over positions instead of over limbs.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The vectorized arms widen one comparison. This one does not widen a comparison at all. It
 *       gives one position to each thread, and runs the whole search for that position there.
 * @note The limb arithmetic on the device is the portable arithmetic, transcribed for the device.
 *       The tests check it against the host, in the same way as each other arm.
 * @note A big integer is a fixed width limb array here exactly as it is on the host. A device can
 *       store one for that reason alone. The representation is a transform of the value, and the
 *       device carries the same transform, one warp's worth of limbs at a time.
 * @note The arm passes only `agreement` to the device. A single comparison is far too small for a
 *       bus crossing. The arm's `equal` and `compare` stay on the host and are the portable ones.
 */
#ifndef ORIOR_EXACT_GPU_H
#define ORIOR_EXACT_GPU_H

#include "exact_integer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Whether a usable CUDA device is present.
     *
     * @return 1 where at least one device answered, 0 otherwise.
     * @note Asks at run time. A program built with CUDA still runs on machines with no device. There the
     *       arm must report that it is absent, and not fail inside a launch.
     */
    int orior_exact_cuda_available(void);

    /**
     * @brief The name and compute capability of the device this arm would use.
     *
     * @param[out] text  Where the call writes the description [BORROWS].
     * @param[in]  capacity  How many bytes `text` contains.
     * @return           1 where the call wrote a description, 0 where no device answered.
     */
    int orior_exact_cuda_describe(char *text, size_t capacity);

    /**
     * @brief Counts the places that agree over a sorted run, one position per thread.
     *
     * @param[in] positions Positions, ascending [BORROWS].
     * @param[in] values    The value located at each position [BORROWS].
     * @param[in] count     How many positions.
     * @param[in] lag       The offset to test [BORROWS].
     * @return              How many positions agree with the place one lag above them, or SIZE_MAX
     *                      where the device errored on the work.
     * @note Returns the same count the portable arm returns. Where it does not, one of the two has a
     *       defect and the difference is never a tradeoff.
     */
    size_t orior_exact_agreement_cuda(const OriorExactInteger *positions, const uint64_t *values, size_t count,
                                       const OriorExactInteger *lag);

#ifdef __cplusplus
}
#endif

#endif /* ORIOR_EXACT_GPU_H */
