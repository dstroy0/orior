// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file arm_avx512.c
 * @brief The AVX-512 arm of the exact arithmetic, which compares sixteen 32 bit limbs per instruction.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note No machine in this project has AVX-512, and no test runs this arm on hardware. The build
 *       compiles it for the target.
 * @note utils/maint/engine/verify_arm_asm.sh reads its instructions and confirms zmm registers and
 *       vpcmpeqd against a mask. That rules out a silent fallback to scalar code. It states nothing
 *       about behavior, and the name of the arm says that.
 * @note For that reason the name reads avx512-unrun. A row of results must not show this arm beside an
 *       arm that runs, unless the row itself shows the difference.
 * @note The detection needs AVX-512F and AVX-512BW together. The masked load of the equality tail is a
 *       BW instruction, and a part with F alone faults on it.
 * @note An AVX-512 comparison does not give a vector of lanes of all ones, as AVX2 does. It writes a
 *       mask register, one bit per lane, and the comparison is against 0xFFFF for sixteen lanes.
 * @note That is a different instruction shape, and not a wider form of the AVX2 shape.
 * @note 128 limbs is eight full blocks of sixteen lanes, with no remainder. The default width does not
 *       use the masked tail below.
 * @note The tail runs for a power-of-two width below 512 bits, whose limb count is not a multiple of
 *       sixteen. A masked load handles it, and not a scalar tail. A mask costs nothing on this
 *       instruction set, and else the tail is a fraction of the work.
 */

#include "arm.h"

#if defined(ORIOR_EXACT_HAVE_AVX512) && ORIOR_EXACT_HAVE_AVX512

#include <immintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
/** @brief Set where this translation unit can query the processor about AVX-512. */
#define ORIOR_AVX512_CAN_DETECT 1
#elif defined(__GNUC__) || defined(__clang__)
#define ORIOR_AVX512_CAN_DETECT 1
#else
#define ORIOR_AVX512_CAN_DETECT 0
#endif

/** @brief Limbs compared per AVX-512 instruction. 512 bits contains sixteen 32 bit limbs. */
#define ORIOR_AVX512_LANES 16u

/** @brief Every lane set. A full block of matching limbs compares equal to this. */
#define ORIOR_AVX512_ALL ((__mmask16)0xFFFFu)

/**
 * @brief Whether the processor that runs has the AVX-512 subsets that this arm uses.
 *
 * @return 1 where AVX-512F and AVX-512BW are both present, 0 otherwise.
 * @note Leaf 7 subleaf 0: bit 16 of EBX is AVX-512F and bit 30 is AVX-512BW. The leaf exists only
 *       where leaf 0 reports a max of at least 7. The call checks that before it reads the leaf.
 */
static int avx512_present(void)
{
#if !ORIOR_AVX512_CAN_DETECT
    return 0;
#elif defined(_MSC_VER)
    int leaves[4] = {0, 0, 0, 0};
    __cpuid(leaves, 0);
    if (leaves[0] < 7)
    {
        return 0;
    }
    __cpuidex(leaves, 7, 0);
    const int foundation = (leaves[1] & (1 << 16)) != 0;
    const int byte_word = (leaves[1] & (1 << 30)) != 0;
    return (foundation && byte_word) ? 1 : 0;
#else
    return (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw")) ? 1 : 0;
#endif
}

/**
 * @brief Whether two magnitudes are equal, sixteen limbs at a time.
 *
 * @param[in] left  First magnitude [BORROWS].
 * @param[in] right Second magnitude [BORROWS].
 * @return          1 where every limb matches, 0 otherwise.
 * @note Walks from the top limb down. A value scaled to 1024 decimal digits has hundreds of zero
 *       digits at its end. The low limbs are zero on both sides and hold no information.
 */
static int avx512_magnitude_equal(const uint32_t *left, const uint32_t *right)
{
    size_t at = (size_t)ORIOR_EXACT_LIMBS;
    while (at >= (size_t)ORIOR_AVX512_LANES)
    {
        at -= (size_t)ORIOR_AVX512_LANES;
        const __m512i one = _mm512_loadu_si512((const void *)(left + at));
        const __m512i two = _mm512_loadu_si512((const void *)(right + at));
        if (_mm512_cmpeq_epi32_mask(one, two) != ORIOR_AVX512_ALL)
        {
            return 0;
        }
    }
    if (at > 0u)
    {
        // The remainder, as a masked load. Lanes past the remainder read as zero on both sides and
        // compare equal. The mask goes on the comparison, and not on the load alone.
        const __mmask16 wanted = (__mmask16)((1u << at) - 1u);
        const __m512i one = _mm512_maskz_loadu_epi32(wanted, (const void *)left);
        const __m512i two = _mm512_maskz_loadu_epi32(wanted, (const void *)right);
        if ((_mm512_cmpeq_epi32_mask(one, two) & wanted) != wanted)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Orders two magnitudes, sixteen limbs at a time.
 *
 * @param[in] left  First magnitude [BORROWS].
 * @param[in] right Second magnitude [BORROWS].
 * @return          -1 where left is smaller, 1 where it is larger, 0 where they are equal.
 * @note The mask marks which lanes differ, and not which of them sits highest. The mask finds the block
 *       with the highest difference. The call then walks it back one limb at a time. That is sixteen
 *       scalar steps at max, once per comparison.
 */
static int avx512_magnitude_compare(const uint32_t *left, const uint32_t *right)
{
    size_t at = (size_t)ORIOR_EXACT_LIMBS;
    while (at >= (size_t)ORIOR_AVX512_LANES)
    {
        at -= (size_t)ORIOR_AVX512_LANES;
        const __m512i one = _mm512_loadu_si512((const void *)(left + at));
        const __m512i two = _mm512_loadu_si512((const void *)(right + at));
        if (_mm512_cmpeq_epi32_mask(one, two) != ORIOR_AVX512_ALL)
        {
            size_t back = at + (size_t)ORIOR_AVX512_LANES;
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
    while (at > 0u)
    {
        at--;
        if (left[at] != right[at])
        {
            return (left[at] < right[at]) ? -1 : 1;
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
    return avx512_magnitude_equal(left->limb, right->limb);
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
    const int order = avx512_magnitude_compare(left->limb, right->limb);
    if (left->sign < 0)
    {
        // Both negative. The larger magnitude is the smaller value.
        return -order;
    }
    return order;
}

/**
 * @brief Counts the places that agree over a sorted run, through the AVX-512 equality.
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
static const OriorExactArm AVX512_ARM = {
    "avx512-unrun",
    arm_equal,
    arm_compare,
    arm_agreement,
};

const OriorExactArm *orior_exact_avx512_arm(void)
{
    return avx512_present() ? &AVX512_ARM : NULL;
}

#else

const OriorExactArm *orior_exact_avx512_arm(void)
{
    return NULL;
}

#endif /* ORIOR_EXACT_HAVE_AVX512 */
