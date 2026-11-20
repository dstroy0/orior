// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file arm_sve.c
 * @brief The SVE arm of the exact arithmetic, at whatever vector length the part turns out to have.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note This arm is for server class ARM: Graviton, Ampere Altra, Grace, the Neoverse cores. No part in
 *       this project has SVE. The Raspberry Pi 5 is a Cortex-A76, which is NEON only.
 * @note No test runs this arm on hardware. The build compiles it for armv8.2-a+sve.
 *       utils/maint/engine/verify_arm_asm.sh reads its instructions and confirms the predicated forms.
 * @note That shows that the intrinsics became SVE instructions and not a scalar fallback. It states
 *       nothing about behavior, and the name of the arm says that.
 * @note SVE has no fixed vector length. A part can be 128, 256, 512 bits or more, and the same program
 *       runs on all of them.
 * @note svcntw() gives how many 32 bit lanes this part has, and a predicate controls the loop in place of a
 *       constant. For that reason no lane count shows in this file, and the tail needs no scalar loop.
 * @note The detection reads the hardware capability word that Linux gives. It never runs an SVE
 *       instruction to see whether it faults.
 * @note There is no cpuid on ARM, and the features of a part come from the kernel. On any system other
 *       than Linux, this arm reports that it is absent, and does not guess.
 */

#include "arm.h"

#if defined(ORIOR_EXACT_HAVE_SVE) && ORIOR_EXACT_HAVE_SVE

#include <arm_sve.h>

#if defined(__linux__)
#include <sys/auxv.h>
/** @brief Set where this translation unit can query the kernel about SVE. */
#define ORIOR_SVE_CAN_DETECT 1
#ifndef HWCAP_SVE
/** @brief The SVE bit in AT_HWCAP, defined here where the running headers predate it. */
#define HWCAP_SVE (1 << 22)
#endif
#else
#define ORIOR_SVE_CAN_DETECT 0
#endif

/**
 * @brief Whether this part carries SVE.
 *
 * @return 1 where the kernel reports SVE, 0 where it does not or where the call cannot ask it.
 */
static int sve_present(void)
{
#if ORIOR_SVE_CAN_DETECT
    return ((getauxval(AT_HWCAP) & (unsigned long)HWCAP_SVE) != 0UL) ? 1 : 0;
#else
    return 0;
#endif
}

/**
 * @brief Whether two magnitudes are equal, a vector's worth of limbs at a time.
 *
 * @param[in] left  First magnitude [BORROWS].
 * @param[in] right Second magnitude [BORROWS].
 * @return          1 where every limb matches, 0 otherwise.
 * @note Walks up here and not down. svwhilelt_b32 builds the predicate for a forward run, and it has
 *       no reversed form. On this instruction set the forward direction costs nothing.
 * @note Equality has no early information in the high limbs, as order does. The walk up loses
 *       nothing.
 */
static int sve_magnitude_equal(const uint32_t *left, const uint32_t *right)
{
    for (uint64_t at = 0u; at < (uint64_t)ORIOR_EXACT_LIMBS; at += svcntw())
    {
        // The predicate covers only the lanes that exist. The tail needs no separate loop.
        const svbool_t live = svwhilelt_b32(at, (uint64_t)ORIOR_EXACT_LIMBS);
        const svuint32_t one = svld1_u32(live, left + at);
        const svuint32_t two = svld1_u32(live, right + at);
        if (svptest_any(live, svcmpne_u32(live, one, two)))
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Orders two magnitudes.
 *
 * @param[in] left  First magnitude [BORROWS].
 * @param[in] right Second magnitude [BORROWS].
 * @return          -1 where left is smaller, 1 where it is larger, 0 where they are equal.
 * @note The highest limb that differs settles the order. This walks blocks from the top down, and then
 *       walks the block that differs back.
 * @note The block stride is the vector length, which the arm learns only at run time. The arm finds the
 *       top block by a count down from the limb count, and not by a constant.
 */
static int sve_magnitude_compare(const uint32_t *left, const uint32_t *right)
{
    const uint64_t lanes = svcntw();
    uint64_t at = (uint64_t)ORIOR_EXACT_LIMBS;
    while (at > 0u)
    {
        const uint64_t span = (at >= lanes) ? lanes : at;
        at -= span;
        const svbool_t live = svwhilelt_b32(at, at + span);
        const svuint32_t one = svld1_u32(live, left + at);
        const svuint32_t two = svld1_u32(live, right + at);
        if (svptest_any(live, svcmpne_u32(live, one, two)))
        {
            uint64_t back = at + span;
            while (back > at)
            {
                back--;
                if (left[back] != right[back])
                {
                    return (left[back] < right[back]) ? -1 : 1;
                }
            }
        }
    }
    return 0;
}

/**
 * @brief Whether two integers have the same value, sign included.
 *
 * @param[in] left  First integer [BORROWS].
 * @param[in] right Second integer [BORROWS].
 * @return          1 where the values are equal, 0 otherwise.
 */
static int arm_equal(const OriorExactInteger *left, const OriorExactInteger *right)
{
    if (left->sign != right->sign)
    {
        return 0;
    }
    return sve_magnitude_equal(left->limb, right->limb);
}

/**
 * @brief Orders two integers, sign included.
 *
 * @param[in] left  First integer [BORROWS].
 * @param[in] right Second integer [BORROWS].
 * @return          -1, 1 or 0, matching orior_exact_compare exactly.
 */
static int arm_compare(const OriorExactInteger *left, const OriorExactInteger *right)
{
    if (left->sign != right->sign)
    {
        return (left->sign < right->sign) ? -1 : 1;
    }
    const int order = sve_magnitude_compare(left->limb, right->limb);
    if (left->sign < 0)
    {
        // Both negative. The larger magnitude is the smaller value.
        return -order;
    }
    return order;
}

/**
 * @brief Counts the places that agree over a sorted run, through the SVE equality.
 *
 * @param[in] positions Positions, ascending [BORROWS].
 * @param[in] values    The value located at each position [BORROWS].
 * @param[in] count     How many positions.
 * @param[in] lag       The offset to test [BORROWS].
 * @return              How many positions agree with the place one lag above them.
 */
static size_t arm_agreement(const OriorExactInteger *positions, const uint64_t *values, size_t count,
                            const OriorExactInteger *lag)
{
    // The shared search, with only the equality test swapped. A time of it then measures the instruction
    // set, and not a second algorithm.
    return orior_exact_agreement_using(arm_equal, positions, values, count, lag);
}

/** @brief The arm as a driver sees it. Static storage. A return of its address is safe. */
static const OriorExactArm SVE_ARM = {
    "sve-unrun",
    arm_equal,
    arm_compare,
    arm_agreement,
};

const OriorExactArm *orior_exact_sve_arm(void)
{
    return sve_present() ? &SVE_ARM : NULL;
}

#else

const OriorExactArm *orior_exact_sve_arm(void)
{
    return NULL;
}

#endif /* ORIOR_EXACT_HAVE_SVE */
