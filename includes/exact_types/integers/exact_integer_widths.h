
// exact_integer_widths.h: the width, limbs and the thresholds that choose each arm (exact_integer.h includes the parts
// in order)
#ifndef EXACT_INTEGER_WIDTHS_H
#define EXACT_INTEGER_WIDTHS_H

#include "embed_types.h"

#ifdef __cplusplus
/* nvcc compiles the GPU arm as C++, and the arm calls these functions, which the C compiler compiles.
 * Without this block, the C++ side looks for mangled names that no C translation unit emits. */
extern "C"
{
#endif

/**
 * @brief Limbs per exact integer, at 32 bits each, a power of two for a power-of-two width.
 *
 * @note A build gives the width as limbs here or as bits in ORIOR_EXACT_BITS. The other follows from it,
 *       and where a build gives both, they must agree.
 * @note The widths have no casts, and #if can then weigh them.
 * @note 128 limbs is 4096 bits, which holds 1024 decimal digits with room over. representation/exact.py
 *       reads at that scale.
 * @note The two forms must agree on the same values. Without that, a cross check between them means
 *       nothing.
 * @note A limb array holds the expansion sum(limb[i] * (2^32)^i). The top coefficient of the expansion
 *       is the width 2^ORIOR_EXACT_BITS.
 * @note A power of two limb count makes that width a power of two. The top magnitude bit then sits at a
 *       fixed position.
 * @note The width scales by a factor of two, 4096 to 8192 to 16384 bits, with the position fixed at each.
 *       The guard below gives an error on a width that is not a power of two.
 * @note A build can select any power of two from 1 limb up, 32 bits up, with no ceiling.
 * @note utils/maint/engine/check_exact_widths.sh grades each arm from 1 limb to 32768.
 *       utils/test/includes/exact_types/integers/exact_transform_test.sh grades the portable reference to
 *       4194304 bits.
 * @note A width below 4096 bits cannot hold the floor of 1024 digits. A build that selects one declares
 *       its own ORIOR_EXACT_DIGITS.
 * @note Where it does not, the floor assert below gives an error on it by name.
 * @note Both arms define the width. #if then always has a value, and an unset build is never a silent
 *       false.
 */
#if defined(ORIOR_EXACT_BITS) && !defined(ORIOR_EXACT_LIMBS)
#define ORIOR_EXACT_LIMBS ((ORIOR_EXACT_BITS) / 32ULL)
#endif

#ifndef ORIOR_EXACT_LIMBS
#define ORIOR_EXACT_LIMBS 128u
#endif

/**
 * @brief Bits that the fixed width holds. Each size comes from this value or from the limbs.
 */
#ifndef ORIOR_EXACT_BITS
#define ORIOR_EXACT_BITS ((ORIOR_EXACT_LIMBS) * 32ULL)
#endif

/**
 * @brief Decimal digits that the fixed width always holds, the same as representation.exact.
 *
 * @note A declared floor and never the capacity. 128 limbs is 4096 bits, which holds 1232 decimal digits.
 *       This constant does not promise the 208 digits of headroom.
 * @note The floor counts each digit of the stored integer, the integer part of the value included.
 * @note The width stores a value at d decimal places as the value times 10^d. At d = 1024 the width holds
 *       a magnitude below 2^4096 / 10^1024, about 1e209.
 * @note Above that magnitude, the width gives an error.
 * @note The CODATA 2022 kilogram-hertz relationship, 1.35639248965e50, needs 3569 bits at 1024 places.
 *       This width holds it.
 * @warning Never size a buffer from this. A width from digits falls short as soon as a build raises the
 *          floor toward the real capacity.
 * @warning A device allocation with that size then falls short by an amount that no check sees. Size
 *          from ORIOR_EXACT_LIMBS or from sizeof(OriorExactInteger), which always match the array.
 * @note Both arms define the floor, as they define the limb count. A build can then raise the floor, and
 *       the assert below decides whether the width holds it.
 * @note A setting that the build cannot change has a guard that no build tests.
 */
#ifndef ORIOR_EXACT_DIGITS
#define ORIOR_EXACT_DIGITS 1024u
#endif

/* The width is a power of two of at least one limb. The top coefficient of the expansion,
 * 2^ORIOR_EXACT_BITS, is then a power of two.
 *
 * The top magnitude bit then sits at a fixed position at each size that the build selects. The width
 * scales by a factor of two in the limb count: 128, 256, 512, which hold 4096, 8192, 16384 bits.
 *
 * A width that is not a power of two stops the compilation here. The power of two test alone passes a
 * width of zero, because 0 & (0 - 1) is 0. The test of the bottom of the range stops that width. */
/* The declared floor must fit the width, and the build can decide that. A decimal digit needs log2(10)
 * bits, which is 3.3219.
 *
 * This test uses 3322 parts in a thousand and rounds up, so that it is never optimistic. A floor above
 * the width stops the compilation at this line. */
/* Three arms, and this block defines each one. The arms key on what the LANGUAGE gives, and not on the
 * compiler.
 *
 * C++ calls the assert static_assert, and C11 calls it _Static_assert. A C compiler older than C11 has
 * neither, and there a negative array width stops the compilation.
 *
 * A vendor name here only moves the gap to the next toolchain that is not from that vendor.
 *
 * Without this guard nvcc does not compile the header, and no GPU arm gets built. An old bench program
 * with no CUDA symbols then shows a "cuda" arm that is the portable arm under another name. */
#if defined(__cplusplus)
    static_assert(((unsigned long long)(ORIOR_EXACT_BITS) >= 32ULL) &&
                      (((unsigned long long)(ORIOR_EXACT_BITS) & ((unsigned long long)(ORIOR_EXACT_BITS)-1ULL)) ==
                       0ULL),
                  "ORIOR_EXACT_BITS must be a power of two of at least 32, so the width scales by doubling");
    static_assert((unsigned long long)(ORIOR_EXACT_BITS) == ((unsigned long long)(ORIOR_EXACT_LIMBS) * 32ULL),
                  "ORIOR_EXACT_BITS and ORIOR_EXACT_LIMBS must name the same width");
    static_assert(((((unsigned long long)(ORIOR_EXACT_DIGITS) * 3322ULL) / 1000ULL) + 1ULL) <=
                      (unsigned long long)(ORIOR_EXACT_BITS),
                  "ORIOR_EXACT_DIGITS declares more decimal digits than ORIOR_EXACT_LIMBS represents");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(((unsigned long long)(ORIOR_EXACT_BITS) >= 32ULL) &&
                   (((unsigned long long)(ORIOR_EXACT_BITS) & ((unsigned long long)(ORIOR_EXACT_BITS)-1ULL)) == 0ULL),
               "ORIOR_EXACT_BITS must be a power of two of at least 32, so the width scales by doubling");
_Static_assert((unsigned long long)(ORIOR_EXACT_BITS) == ((unsigned long long)(ORIOR_EXACT_LIMBS) * 32ULL),
               "ORIOR_EXACT_BITS and ORIOR_EXACT_LIMBS must name the same width");
_Static_assert(((((unsigned long long)(ORIOR_EXACT_DIGITS) * 3322ULL) / 1000ULL) + 1ULL) <=
                   (unsigned long long)(ORIOR_EXACT_BITS),
               "ORIOR_EXACT_DIGITS declares more decimal digits than ORIOR_EXACT_LIMBS represents");
#else
typedef char orior_exact_bits_are_a_power_of_two[(((unsigned long long)(ORIOR_EXACT_BITS) >= 32ULL) &&
                                                   (((unsigned long long)(ORIOR_EXACT_BITS) &
                                                     ((unsigned long long)(ORIOR_EXACT_BITS)-1ULL)) == 0ULL))
                                                      ? 1
                                                      : -1];
typedef char orior_exact_bits_and_limbs_agree
    [((unsigned long long)(ORIOR_EXACT_BITS) == ((unsigned long long)(ORIOR_EXACT_LIMBS) * 32ULL)) ? 1 : -1];
typedef char orior_exact_digits_fit_the_width[(((((unsigned long long)(ORIOR_EXACT_DIGITS) * 3322ULL) / 1000ULL) +
                                                 1ULL) <= (unsigned long long)(ORIOR_EXACT_BITS))
                                                   ? 1
                                                   : -1];
#endif

/**
 * @brief The widest width, in limbs, whose work copies the stack holds.
 *
 * @note Past it, each work copy of the width comes from the heap. 4096 limbs is 16 KiB a copy, and the
 *       widest call, the gcd, uses nine of them.
 */
#ifndef ORIOR_EXACT_STACK_LIMBS
#define ORIOR_EXACT_STACK_LIMBS 4096u
#endif

/**
 * @brief The limbs of the shorter factor from which a product uses Karatsuba and not long
 *        multiplication.
 *
 * @note Karatsuba splits each factor in halves and recurses into three half products. Its cost
 *       grows as n^1.585 against the long multiplication's n^2.
 */
#ifndef ORIOR_EXACT_KARATSUBA_LIMBS
#define ORIOR_EXACT_KARATSUBA_LIMBS 32u
#endif

/**
 * @brief The limbs of the shorter factor from which a product uses the Schonhage-Strassen transform.
 *
 * @note The transform works in the ring of integers modulo 2^n + 1. There a power of two is a root of
 *       unity, and each twiddle is a shift.
 * @note For a negacyclic product, the transform weights by the square root of that root. An integer
 *       cost model selects its depth.
 * @note The threshold is the crossover of Karatsuba and the transform on an x86-64 host, near 8192
 *       limbs. Above it the transform leads.
 */
#ifndef ORIOR_EXACT_TRANSFORM_LIMBS
#define ORIOR_EXACT_TRANSFORM_LIMBS 8192u
#endif

/**
 * @brief The limbs that the divisor and the quotient must both reach before a division uses Newton's
 *        reciprocal and not long division.
 *
 * @note The call grows Newton's reciprocal at half precision, recursively. It then takes one Newton step
 *       and corrects the result exactly. The quotient is then one product on the ladder.
 * @note The threshold is the crossover of long division and Newton on an x86-64 host, for a 2n-limb
 *       numerator by an n-limb divisor, near 8192 limbs.
 */
#ifndef ORIOR_EXACT_NEWTON_LIMBS
#define ORIOR_EXACT_NEWTON_LIMBS 8192u
#endif

#if defined(__cplusplus)
    static_assert((ORIOR_EXACT_NEWTON_LIMBS) >= 1u, "Newton's division needs a divisor of at least one limb");
    static_assert((ORIOR_EXACT_KARATSUBA_LIMBS) >= 4u, "Karatsuba splits in halves of at least two limbs");
    static_assert((ORIOR_EXACT_TRANSFORM_LIMBS) >= (ORIOR_EXACT_KARATSUBA_LIMBS),
                  "the transform's rung sits at or above Karatsuba's");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert((ORIOR_EXACT_NEWTON_LIMBS) >= 1u, "Newton's division needs a divisor of at least one limb");
_Static_assert((ORIOR_EXACT_KARATSUBA_LIMBS) >= 4u, "Karatsuba splits in halves of at least two limbs");
_Static_assert((ORIOR_EXACT_TRANSFORM_LIMBS) >= (ORIOR_EXACT_KARATSUBA_LIMBS),
               "the transform's rung sits at or above Karatsuba's");
#else
typedef char orior_exact_newton_has_a_divisor[((ORIOR_EXACT_NEWTON_LIMBS) >= 1u) ? 1 : -1];
typedef char orior_exact_karatsuba_splits[((ORIOR_EXACT_KARATSUBA_LIMBS) >= 4u) ? 1 : -1];
typedef char orior_exact_rungs_in_order[((ORIOR_EXACT_TRANSFORM_LIMBS) >= (ORIOR_EXACT_KARATSUBA_LIMBS)) ? 1 : -1];
#endif

#ifdef __cplusplus
}
#endif

#endif
