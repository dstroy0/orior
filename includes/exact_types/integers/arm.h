// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file arm.h
 * @brief The one interface of each implementation of the exact measure. A driver can call any of them through it.
 * @author dstroy0 (Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 *
 * @note Each engine returns the same counts. Where two disagree, one of them has a defect, and the
 *       difference is no tradeoff. OriorEngine has the same contract for the search kernel, for the same
 *       reason.
 * @note The portable arm is the reference. It is always present, and uses no intrinsic and no compiler
 *       extension. The tests grade each other arm against it on the same data.
 * @note An arm reports at run time that it is absent where the machine cannot run it. A build with an
 *       AVX2 arm must still ask the processor before it calls the arm. The build machine and the machine
 *       that runs the program are different machines.
 */
#ifndef ORIOR_EXACT_ARM_H
#define ORIOR_EXACT_ARM_H

#include "exact_integer.h"

#ifdef __cplusplus
/* nvcc compiles the GPU arm as C++, and it defines its arm here. Each other arm is C. */
extern "C"
{
#endif

    /**
     * @brief One implementation of the operations a measure requires of an exact integer.
     *
     * @note The measure spends its time in `equal` and `compare`. The arm gives `agreement` whole, because
     *       an arm can work in parallel across positions in place of across limbs. The GPU arm does that,
     *       and neither vectorized arm does.
     */
    typedef struct
    {
        const char *name; /**< What to print in a row. Never null. */
        int (*equal)(const OriorExactInteger *left, const OriorExactInteger *right);
        int (*compare)(const OriorExactInteger *left, const OriorExactInteger *right);
        size_t (*agreement)(const OriorExactInteger *positions, const uint64_t *values, size_t count,
                            const OriorExactInteger *lag);
    } OriorExactArm;

    /**
     * @brief The portable C11 arm, the reference for the grade of each other arm.
     *
     * @return A pointer to the arm. Never null, since this arm runs anywhere a C11 compiler built it.
     */
    const OriorExactArm *orior_exact_portable_arm(void);

#if defined(ORIOR_EXACT_HAVE_AVX2) && ORIOR_EXACT_HAVE_AVX2

    /**
     * @brief The AVX2 arm, which compares eight limbs per instruction.
     *
     * @return A pointer to the arm, or NULL where this processor does not carry AVX2.
     * @note Asks the processor, and does not trust the build. A program compiled where AVX2 is present
     *       runs on machines without it. A call into the arm there raises an illegal instruction.
     */
    const OriorExactArm *orior_exact_avx2_arm(void);

#endif

#if defined(ORIOR_EXACT_HAVE_AVX512) && ORIOR_EXACT_HAVE_AVX512

    /**
     * @brief The AVX-512 arm, which compares sixteen limbs per instruction.
     *
     * @return A pointer to the arm, or NULL where this processor lacks AVX-512F and AVX-512BW.
     * @note No test runs it on hardware, because no machine in this project has AVX-512. The grade of
     *       this arm reads the instructions that it emits, and not an answer of it.
     * @note Its name is avx512-unrun. A row of results then cannot show it beside an arm that runs and
     *       hide the difference.
     */
    const OriorExactArm *orior_exact_avx512_arm(void);

#endif

#if defined(ORIOR_EXACT_HAVE_NEON) && ORIOR_EXACT_HAVE_NEON

    /**
     * @brief The NEON arm, which compares four limbs per instruction.
     *
     * @return A pointer to the arm, or NULL where this processor does not carry NEON.
     */
    const OriorExactArm *orior_exact_neon_arm(void);

#endif

#if defined(ORIOR_EXACT_HAVE_SVE) && ORIOR_EXACT_HAVE_SVE

    /**
     * @brief The SVE arm, which compares at the vector length of the part, whatever that is.
     *
     * @return A pointer to the arm, or NULL where the kernel does not report SVE.
     * @note No test runs it on hardware. The Raspberry Pi 5 is a Cortex-A76, and is NEON only.
     * @note The grade of this arm reads the instructions that it emits. Its name is sve-unrun, for the
     *       same reason as the AVX-512 arm.
     */
    const OriorExactArm *orior_exact_sve_arm(void);

#endif

#if defined(ORIOR_EXACT_HAVE_CUDA) && ORIOR_EXACT_HAVE_CUDA

    /**
     * @brief The CUDA arm, which takes one position per thread and not one limb per lane.
     *
     * @return A pointer to the arm, or NULL where no usable device is present.
     * @note Its `equal` and `compare` run on the host, and are the portable functions. A single comparison
     *       is far too small for a bus crossing, and the arm passes only `agreement` to the device.
     */
    const OriorExactArm *orior_exact_cuda_arm(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* ORIOR_EXACT_ARM_H */
