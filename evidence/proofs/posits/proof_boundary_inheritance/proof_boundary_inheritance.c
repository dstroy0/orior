// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: PRF-x-010
/**
 * @file proof_boundary_inheritance.c
 * @brief The boundary function, defined by probes, run on the sets of Fefferman, with each inheritance checked.
 *
 * @note This asks the boundary function to define itself. It then runs the function over the sets that the
 *       Navier-Stokes statement of Fefferman names. It checks in exact integer arithmetic how each boundary type
 *       passes through the constructors. It claims nothing about any open problem. It presents the rigor and
 *       stops.
 * @note Run proof_boundary_inheritance.sh, beside this file. It builds this program into build/ and runs it.
 * @note proof_domain_boundaries.py names three types of boundary from a survey, and proves each on a case built
 *       to have it. A survey assigns the type by judgment, and the discipline forbids judgment. Here nobody
 *       assigns the type. Two probes read it off the object, and the object answers:
 *
 *           FORMAT        A higher scale (the format) moves what we reach.
 *           COMPLETENESS  A higher horizon (the computation) moves what we reach.
 *           MEASUREMENT   A gap to the target exists, and neither probe moves it. The floor is not on our side.
 *           NONE          No gap exists: the program reaches the target exactly.
 *
 * @note Thus "measurement" means exactly "unmoved by both of our probes". From this side, "external" means the
 *       same. The verdict is only as good as the probes: a probe that cannot move an object is no probe of it.
 *       This file states that limit, and does not hide it.
 * @note The classifier runs first on four cases of a type known by construction: the positive controls of
 *       proof_domain_boundaries.py. Only then does it run on the Navier-Stokes objects. Those come from the
 *       ring of examples/0_experimental/exact_navier_stokes, the same ring that its two reports read.
 * @note The arithmetic is exact integers of includes/exact_types, and truth values. A value that is not a
 *       decimal is a pair of integers, numerator over denominator, reduced by their common divisor. Where a
 *       decimal report needs pi, pi is a scaled integer by the formula of Machin.
 * @note Their sets, run here. First, the Taylor coefficients of a solution of (1)-(3) on the unit torus, in
 *       decimals and in the exact ring. A coefficient that carries pi has no last digit in decimals. The ring
 *       has no scale at all. Second, an amplitude deposited to six places, a stand-in for a measured datum.
 *       Third, the ratio in which that amplitude cancels.
 * @note Then inheritance: does the chain of constructors datum -> u_1 -> u_2 -> u_3 carry each type forward?
 *       The measurement: the format type is absent at every order in the ring. In a decimal report it is absent
 *       at the rational datum, and present from the first derivative on, because the derivative brings pi in.
 *       The measurement type passes to every order, and only a ratio cancels it. The completeness type passes
 *       forward and grows.
 * @note The last part is the algebra of one constructor. The millennium research paper reads it in its
 *       Navier-Stokes chapter, theory/theory/millennium/chapters/chapter_navier_stokes.tex. That chapter gives
 *       an account of Corollary 10.6 of the 2026 paper that it reviews. The constructor builds a periodic field as a
 * sum of integer translates of a field with compact support, with disjoint supports.
 * @note What that constructor inherits from its pieces rests on one exact fact. The bilinear term of a sum
 *       splits into the bilinear terms of the pieces when the supports are disjoint. This shows that fact on
 *       piecewise polynomials with integer coefficients on a lattice of eighths, by two routes. The case with
 *       overlap is the null. The construction is theirs; this runs only the algebra.
 * @note Prior art and language, named with respect. The problem statement and its numbered conditions are by
 *       Charles Fefferman (Clay Mathematics Institute, read in full). The probes are the ordinary idea of a test
 *       of sensitivity, and not the idea of this work. The Moore closure and the diagonal of Cantor are in
 *       proof_set_theory.py, with their credits. Pi is by the formula of Machin (1706).
 * @note The Taylor coefficients of a solution follow from the derivatives of the equation at t = 0, by the
 *       classical method of power series. No bound: each comparison is an exact equality on integers, on pairs
 *       of integers, or on sets.
 */
#include "../../../../examples/0_experimental/exact_navier_stokes/torus_fields.h"
#include "../../../../includes/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The viscosity, 0.1, as the decimal text reads it. */
static const Viscosity VISCOSITY = {1, 1u};

/** @brief The deposit 1.234567, as a numerator at decimal places. */
static const int64_t DEPOSIT_TOP = 1234567;

/** @brief The places of the deposit. */
static const uint32_t DEPOSIT_PLACES = 6u;

/** @brief The stand-in for the true value that the deposit cuts, 1.2345671234567, as a numerator. */
static const int64_t TRUTH_TOP = 12345671234567;

/** @brief The places of the stand-in. */
static const uint32_t TRUTH_PLACES = 13u;

/* ---------------------------------------------------------------- text, the canonical form of a reach */

/** @brief A text that grows, the canonical form of the value that a probe reaches. */
typedef struct
{
    char *bytes;     /**< The text, with a zero at the end [OWNS]. */
    size_t length;   /**< Bytes before the zero. */
    size_t capacity; /**< Bytes of the buffer. */
} ReachText;

/**
 * @brief An empty text.
 *
 * @param[out] text The text [BORROWS].
 */
static void text_begin(ReachText *text)
{
    text->capacity = 256u;
    text->bytes = (char *)malloc(text->capacity);
    if (text->bytes == NULL)
    {
        fprintf(stderr, "  out of memory\n");
        exit(2);
    }
    text->bytes[0] = '\0';
    text->length = 0u;
}

/**
 * @brief Appends formatted text.
 *
 * @param[in,out] text   The text [BORROWS].
 * @param[in]     format The format [BORROWS].
 */
static void text_put(ReachText *text, const char *format, ...)
{
    for (;;)
    {
        va_list arguments;
        va_start(arguments, format);
        const int wrote = vsnprintf(text->bytes + text->length, text->capacity - text->length, format, arguments);
        va_end(arguments);
        // vsnprintf returns a negative count only on an encoding error, which no format here makes.
        const size_t needed = (wrote < 0) ? 0u : (size_t)wrote;
        if ((text->length + needed) < text->capacity)
        {
            text->length += needed;
            return;
        }
        text->capacity = (text->capacity + needed) * 2u;
        char *const grown = (char *)realloc(text->bytes, text->capacity);
        if (grown == NULL)
        {
            fprintf(stderr, "  out of memory\n");
            exit(2);
        }
        text->bytes = grown;
    }
}

/**
 * @brief Appends the decimal text of an exact integer.
 *
 * @param[in,out] text  The text [BORROWS].
 * @param[in]     value The integer [BORROWS].
 */
static void text_big(ReachText *text, const OriorExactInteger *value)
{
    static char digits[40000];
    big_text(value, digits, sizeof(digits));
    text_put(text, "%s", digits);
}

/**
 * @brief Frees a text.
 *
 * @param[in,out] text The text [BORROWS].
 */
static void text_free(ReachText *text)
{
    free(text->bytes);
    text->bytes = NULL;
    text->length = 0u;
    text->capacity = 0u;
}

/* ---------------------------------------------------------------- exact pairs */

/** @brief A rational number as a reduced pair: the denominator is positive and shares no divisor with the top. */
typedef struct
{
    OriorExactInteger top;   /**< The numerator. */
    OriorExactInteger under; /**< The denominator, positive. */
} ExactPair;

/**
 * @brief The quotient of a division rounded toward minus infinity, as the floor division of Python rounds.
 *
 * @param[in]  numerator The dividend [BORROWS].
 * @param[in]  divisor   The divisor, not zero [BORROWS].
 * @param[out] quotient  The quotient [BORROWS].
 */
static void floor_divide(const OriorExactInteger *numerator, const OriorExactInteger *divisor,
                         OriorExactInteger *quotient)
{
    OriorExactInteger remainder;
    ring_require(orior_exact_divide(numerator, divisor, quotient, &remainder), "division");
    if ((remainder.sign != 0) && ((numerator->sign * divisor->sign) < 0))
    {
        OriorExactInteger one;
        big_set(&one, 1);
        ring_require(orior_exact_subtract(quotient, &one, quotient), "subtract");
    }
}

/**
 * @brief The reduced pair of a numerator and a denominator.
 *
 * @param[in]  top   The numerator [BORROWS].
 * @param[in]  under The denominator, not zero [BORROWS].
 * @param[out] pair  The reduced pair [BORROWS].
 */
static void pair_reduced(const OriorExactInteger *top, const OriorExactInteger *under, ExactPair *pair)
{
    OriorExactInteger numerator = *top;
    OriorExactInteger denominator = *under;
    if (denominator.sign < 0)
    {
        numerator.sign = -numerator.sign;
        denominator.sign = 1;
    }
    OriorExactInteger divisor;
    ring_require(orior_exact_gcd(&numerator, &denominator, &divisor), "gcd");
    ring_require(orior_exact_divide_exact(&numerator, &divisor, &pair->top), "exact division");
    ring_require(orior_exact_divide_exact(&denominator, &divisor, &pair->under), "exact division");
}

/**
 * @brief The reduced pair of two machine integers.
 *
 * @param[in]  top   The numerator.
 * @param[in]  under The denominator. It is not zero.
 * @param[out] pair  The reduced pair [BORROWS].
 */
static void pair_small(int64_t top, int64_t under, ExactPair *pair)
{
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    big_set(&numerator, top);
    big_set(&denominator, under);
    pair_reduced(&numerator, &denominator, pair);
}

/**
 * @brief The sum of two reduced pairs.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
static void pair_add(const ExactPair *left, const ExactPair *right, ExactPair *sum)
{
    OriorExactInteger first;
    OriorExactInteger second;
    OriorExactInteger under;
    ring_require(orior_exact_multiply(&left->top, &right->under, &first), "multiply");
    ring_require(orior_exact_multiply(&right->top, &left->under, &second), "multiply");
    ring_require(orior_exact_add(&first, &second, &first), "add");
    ring_require(orior_exact_multiply(&left->under, &right->under, &under), "multiply");
    pair_reduced(&first, &under, sum);
}

/**
 * @brief Appends a pair to a text, as top/under.
 *
 * @param[in,out] text The text [BORROWS].
 * @param[in]     pair The pair [BORROWS].
 */
static void text_pair(ReachText *text, const ExactPair *pair)
{
    text_big(text, &pair->top);
    text_put(text, "/");
    text_big(text, &pair->under);
}

/**
 * @brief An exact integer raised to a power of at least zero.
 *
 * @param[in]  base     The base [BORROWS].
 * @param[in]  exponent The power.
 * @param[out] result   The power of the base [BORROWS].
 */
static void big_power(const OriorExactInteger *base, uint32_t exponent, OriorExactInteger *result)
{
    big_set(result, 1);
    for (uint32_t step = 0u; step < exponent; step += 1u)
    {
        ring_require(orior_exact_multiply(result, base, result), "multiply");
    }
}

/**
 * @brief Ten raised to a power, as an exact integer.
 *
 * @param[in]  power  The power.
 * @param[out] result The value [BORROWS].
 */
static void ten_to(uint32_t power, OriorExactInteger *result)
{
    big_set(result, 1);
    ring_require(orior_exact_scale_by_ten(result, power), "scale");
}

/* ---------------------------------------------------------------- the boundary function, by probes */

/**
 * @brief What a probe reaches at a format and a horizon, as canonical text.
 *
 * @param[in]  context       The object of the probe [BORROWS].
 * @param[in]  format_level  The scale.
 * @param[in]  horizon_level The horizon.
 * @param[out] reached       The canonical text [BORROWS].
 */
typedef void (*ReachFunction)(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached);

/** @brief The verdict of the probes on one object. */
typedef struct
{
    const char *type;     /**< The boundary type. */
    int gap;              /**< Whether a gap to the target exists. */
    int moved_by_format;  /**< Whether a higher scale moves the reach. */
    int moved_by_horizon; /**< Whether a higher horizon moves the reach. */
} Verdict;

/**
 * @brief Reads the type off the object: a gap to the target, and what each probe moves.
 *
 * @param[in] reach         What the object holds at a format and a horizon [BORROWS].
 * @param[in] context       The object [BORROWS].
 * @param[in] format_level  The scale.
 * @param[in] horizon_level The horizon.
 * @param[in] target        The canonical text of the target [BORROWS].
 * @return                  The verdict. A probe raises one setting by one.
 */
static Verdict boundary_type(ReachFunction reach, const void *context, int64_t format_level, int64_t horizon_level,
                             const ReachText *target)
{
    ReachText here;
    ReachText by_format;
    ReachText by_horizon;
    text_begin(&here);
    text_begin(&by_format);
    text_begin(&by_horizon);
    reach(context, format_level, horizon_level, &here);
    reach(context, format_level + 1, horizon_level, &by_format);
    reach(context, format_level, horizon_level + 1, &by_horizon);
    Verdict verdict;
    verdict.gap = !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = here.bytes, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = target->bytes, .ci = EMBED_FALSE);
    verdict.moved_by_format = !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = by_format.bytes,
                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = here.bytes, .ci = EMBED_FALSE);
    verdict.moved_by_horizon = !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = by_horizon.bytes,
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = here.bytes, .ci = EMBED_FALSE);
    if (!verdict.gap)
    {
        verdict.type = "none";
    }
    else if (verdict.moved_by_format && verdict.moved_by_horizon)
    {
        verdict.type = "format and completeness";
    }
    else if (verdict.moved_by_format)
    {
        verdict.type = "format";
    }
    else if (verdict.moved_by_horizon)
    {
        verdict.type = "completeness";
    }
    else
    {
        verdict.type = "measurement";
    }
    text_free(&here);
    text_free(&by_format);
    text_free(&by_horizon);
    return verdict;
}

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief Writes one verdict on one line.
 *
 * @param[in] out     The stream [BORROWS].
 * @param[in] name    The name of the object [BORROWS].
 * @param[in] verdict The verdict.
 */
static void describe(FILE *out, const char *name, Verdict verdict)
{
    fprintf(out, "    %-48s gap %-5s format %-5s horizon %-5s -> %s\n", name, truth(verdict.gap),
            truth(verdict.moved_by_format), truth(verdict.moved_by_horizon), verdict.type);
}

/**
 * @brief Writes a list of truth values as Python prints a list.
 *
 * @param[in] out    The stream [BORROWS].
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 */
static void print_truths(FILE *out, const int *values, size_t count)
{
    fputs("[", out);
    for (size_t at = 0u; at < count; at += 1u)
    {
        fprintf(out, "%s%s", (at > 0u) ? ", " : "", truth(values[at]));
    }
    fputs("]", out);
}

/* ---------------------------------------------------------------- positive controls, type known by construction */

/** @brief A prime for a number-theoretic transform, with the 2-adic order of p - 1 and a primitive root. */
typedef struct
{
    uint64_t prime;     /**< The prime. */
    uint64_t order;     /**< The 2-adic order of p - 1. */
    uint64_t generator; /**< A primitive root. */
} FormatPrime;

/** @brief The two formats: 119 * 2^23 + 1, and 15 * 2^27 + 1. */
static const FormatPrime FORMAT_PRIMES[2] = {{998244353u, 23u, 3u}, {2013265921u, 27u, 31u}};

/**
 * @brief A power modulo a prime below 2^31. Each product is below 2^62, and fits 64 bits.
 *
 * @param[in] base     The base.
 * @param[in] exponent The power.
 * @param[in] prime    The modulus.
 * @return             base^exponent mod prime.
 */
static uint64_t power_mod(uint64_t base, uint64_t exponent, uint64_t prime)
{
    uint64_t result = 1u;
    uint64_t square = base % prime;
    while (exponent > 0u)
    {
        if ((exponent & 1u) != 0u)
        {
            result = (result * square) % prime;
        }
        square = (square * square) % prime;
        exponent >>= 1u;
    }
    return result;
}

/**
 * @brief Whether a prime has a root of unity of an exact order.
 *
 * @param[in] prime     The prime.
 * @param[in] generator A primitive root.
 * @param[in] length    The order.
 * @return              1 where a root of exactly that order exists.
 */
static int has_root_of_order(uint64_t prime, uint64_t generator, uint64_t length)
{
    if (((prime - 1u) % length) != 0u)
    {
        return 0;
    }
    const uint64_t root = power_mod(generator, (prime - 1u) / length, prime);
    return (power_mod(root, length, prime) == 1u) && (power_mod(root, length / 2u, prime) != 1u);
}

/**
 * @brief The power-of-two exponents up to the horizon whose length has a root modulo the prime of the format.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  The format, which picks the prime.
 * @param[in]  horizon_level The highest exponent.
 * @param[out] reached       The exponents [BORROWS].
 */
static void ntt_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    const FormatPrime *const format = &FORMAT_PRIMES[(format_level < 1) ? 0 : 1];
    text_put(reached, "N:");
    for (int64_t exponent = 1; exponent <= horizon_level; exponent += 1)
    {
        if (has_root_of_order(format->prime, format->generator, (uint64_t)1u << (uint64_t)exponent))
        {
            text_put(reached, "%lld,", (long long)exponent);
        }
    }
}

/**
 * @brief The set {1..count} in the canonical form of ntt_reach.
 *
 * @param[in]  count The largest member.
 * @param[out] text  The text [BORROWS].
 */
static void ntt_target(int64_t count, ReachText *text)
{
    text_put(text, "N:");
    for (int64_t exponent = 1; exponent <= count; exponent += 1)
    {
        text_put(text, "%lld,", (long long)exponent);
    }
}

/**
 * @brief The leaves of a full 3-ary tree of depth 12 within the horizon. The scale plays no part.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  Unused.
 * @param[in]  horizon_level The horizon.
 * @param[out] reached       The count [BORROWS].
 */
static void tree_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    (void)format_level;
    int64_t count = 1;
    for (int64_t depth = 0; depth < ((horizon_level < 12) ? horizon_level : 12); depth += 1)
    {
        count *= 3;
    }
    text_put(reached, "T:%lld", (long long)count);
}

/**
 * @brief The fixed count 3^7, a probe with the horizon cut: the null for the classifier.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  Unused.
 * @param[in]  horizon_level Unused.
 * @param[out] reached       The count [BORROWS].
 */
static void blind_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    (void)format_level;
    (void)horizon_level;
    text_put(reached, "T:%lld", 2187LL);
}

/**
 * @brief The deposit read at a count of decimal places, at least its own six, as a reduced pair.
 *
 * @param[in]  places The places.
 * @param[out] pair   The value [BORROWS].
 */
static void deposit_at(int64_t places, ExactPair *pair)
{
    // The places are small, and fit a uint32_t: a probe raises them by one from six or thirty.
    const uint32_t digits = (places > (int64_t)DEPOSIT_PLACES) ? (uint32_t)places : DEPOSIT_PLACES;
    OriorExactInteger top;
    big_set(&top, DEPOSIT_TOP);
    ring_require(orior_exact_scale_by_ten(&top, digits - DEPOSIT_PLACES), "scale");
    OriorExactInteger under;
    ten_to(digits, &under);
    pair_reduced(&top, &under, pair);
}

/**
 * @brief The stand-in true value, as a reduced pair.
 *
 * @param[out] pair The value [BORROWS].
 */
static void truth_value(ExactPair *pair)
{
    OriorExactInteger top;
    big_set(&top, TRUTH_TOP);
    OriorExactInteger under;
    ten_to(TRUTH_PLACES, &under);
    pair_reduced(&top, &under, pair);
}

/**
 * @brief The deposit at the scale of the format. The horizon plays no part.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  The scale.
 * @param[in]  horizon_level Unused.
 * @param[out] reached       The value [BORROWS].
 */
static void deposit_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    (void)horizon_level;
    ExactPair pair;
    deposit_at(format_level, &pair);
    text_put(reached, "P:");
    text_pair(reached, &pair);
}

/**
 * @brief The rational 22/7, the same at every setting.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  Unused.
 * @param[in]  horizon_level Unused.
 * @param[out] reached       The value [BORROWS].
 */
static void rational_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    (void)format_level;
    (void)horizon_level;
    ExactPair pair;
    pair_small(22, 7, &pair);
    text_put(reached, "P:");
    text_pair(reached, &pair);
}

/**
 * @brief Four objects whose type the construction fixes, and the null that shows the limit of the probes.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where each object has its forced type.
 */
static int report_controls(FILE *out)
{
    fputs("  positive controls: four objects whose type is fixed by construction\n", out);
    ReachText target;
    text_begin(&target);
    ntt_target(25, &target);
    const Verdict ntt = boundary_type(ntt_reach, NULL, 0, 25, &target);
    text_free(&target);

    text_begin(&target);
    text_put(&target, "T:%lld", 531441LL);
    const Verdict tree = boundary_type(tree_reach, NULL, 0, 7, &target);
    const Verdict blind = boundary_type(blind_reach, NULL, 0, 7, &target);
    text_free(&target);

    ExactPair truth_pair;
    truth_value(&truth_pair);
    text_begin(&target);
    text_put(&target, "P:");
    text_pair(&target, &truth_pair);
    const Verdict deposit = boundary_type(deposit_reach, NULL, 6, 0, &target);
    text_free(&target);

    text_begin(&target);
    rational_reach(NULL, 0, 0, &target);
    const Verdict exact_value = boundary_type(rational_reach, NULL, 0, 0, &target);
    text_free(&target);

    describe(out, "NTT length, prime 119*2^23+1, horizon 2^25", ntt);
    describe(out, "3-ary tree count at horizon 7 of 12", tree);
    describe(out, "a deposit of 6 places against its true value", deposit);
    describe(out, "the rational 22/7 against itself", exact_value);
    // The null for the classifier: a probe that cannot move the object reports measurement.
    describe(out, "null: the tree count with the horizon probe cut", blind);
    fputs("    the null shows the limit: measurement means unmoved by OUR probes, and only that\n\n", out);
    return (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = ntt.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "format", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = tree.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "completeness", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = deposit.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "measurement", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = exact_value.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "none", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = blind.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "measurement", .ci = EMBED_FALSE));
}

/* ---------------------------------------------------------------- pi as a scaled integer, Machin 1706 */

/**
 * @brief arctan(1 / whole) times the scale, by its series, each term cut toward zero.
 *
 * @param[in]  whole  The reciprocal of the argument.
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The scaled arctangent [BORROWS].
 */
static void arctan_reciprocal(int64_t whole, const OriorExactInteger *scale, OriorExactInteger *result)
{
    orior_exact_zero(result);
    OriorExactInteger power;
    big_set(&power, whole);
    OriorExactInteger whole_squared;
    big_set(&whole_squared, whole * whole);
    int sign = 1;
    for (int64_t index = 0;; index += 1)
    {
        OriorExactInteger divisor;
        OriorExactInteger odd;
        big_set(&odd, (2 * index) + 1);
        ring_require(orior_exact_multiply(&odd, &power, &divisor), "multiply");
        OriorExactInteger term;
        OriorExactInteger remainder;
        ring_require(orior_exact_divide(scale, &divisor, &term, &remainder), "division");
        if (term.sign == 0)
        {
            return;
        }
        if (sign > 0)
        {
            ring_require(orior_exact_add(result, &term, result), "add");
        }
        else
        {
            ring_require(orior_exact_subtract(result, &term, result), "subtract");
        }
        sign = -sign;
        ring_require(orior_exact_multiply(&power, &whole_squared, &power), "multiply");
    }
}

/**
 * @brief floor(pi * 10^digits), by the formula of Machin: pi = 16 arctan(1/5) - 4 arctan(1/239).
 *
 * @param[in]  digits The decimal places.
 * @param[out] result The scaled pi [BORROWS].
 */
static void pi_scaled(uint32_t digits, OriorExactInteger *result)
{
    // The last value, kept. Every call of a run asks for the same few places.
    static uint32_t cached_digits = 0u;
    static OriorExactInteger cached;
    if (cached_digits == digits)
    {
        *result = cached;
        return;
    }
    const uint32_t guard = 10u;
    OriorExactInteger scale;
    ten_to(digits + guard, &scale);
    OriorExactInteger fifth;
    OriorExactInteger part;
    arctan_reciprocal(5, &scale, &fifth);
    arctan_reciprocal(239, &scale, &part);
    OriorExactInteger sixteen;
    OriorExactInteger four;
    big_set(&sixteen, 16);
    big_set(&four, 4);
    ring_require(orior_exact_multiply(&fifth, &sixteen, &fifth), "multiply");
    ring_require(orior_exact_multiply(&part, &four, &part), "multiply");
    OriorExactInteger value;
    ring_require(orior_exact_subtract(&fifth, &part, &value), "subtract");
    OriorExactInteger cut;
    ten_to(guard, &cut);
    floor_divide(&value, &cut, result);
    cached = *result;
    cached_digits = digits;
}

/**
 * @brief floor(10^digits * (sum_p c_p pi^p) / den), for the real (part 0) or imaginary (part 1) integers.
 *
 * @param[in]  poly        The polynomial [BORROWS].
 * @param[in]  denominator The denominator [BORROWS].
 * @param[in]  part        0 for the real parts, 1 for the imaginary parts.
 * @param[in]  digits      The decimal places.
 * @param[out] result      The floor [BORROWS].
 * @note Pi enters as a scaled integer with 20 places past `digits`. The floor is exact, unless the true expansion
 *       runs twenty nines at that place. The probe compares values, and a value with pi in it moves.
 */
static void evaluate_part(const PiPoly *poly, const OriorExactInteger *denominator, unsigned part, uint32_t digits,
                          OriorExactInteger *result)
{
    const uint32_t work = digits + 20u;
    OriorExactInteger pi_value;
    pi_scaled(work, &pi_value);
    ExactPair total;
    pair_small(0, 1, &total);
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        const PiTerm *const term = &poly->terms[at];
        const OriorExactInteger *const coefficient = (part == 0u) ? &term->real : &term->imaginary;
        if (coefficient->sign == 0)
        {
            continue;
        }
        // The power is a small count of factors of pi. Its magnitude fits a uint32_t.
        const uint32_t magnitude = (term->power < 0) ? (uint32_t)(-term->power) : (uint32_t)term->power;
        OriorExactInteger pi_power;
        big_power(&pi_value, magnitude, &pi_power);
        OriorExactInteger ten_power;
        ten_to(work * magnitude, &ten_power);
        OriorExactInteger top;
        ExactPair piece;
        if (term->power >= 0)
        {
            ring_require(orior_exact_multiply(coefficient, &pi_power, &top), "multiply");
            pair_reduced(&top, &ten_power, &piece);
        }
        else
        {
            ring_require(orior_exact_multiply(coefficient, &ten_power, &top), "multiply");
            pair_reduced(&top, &pi_power, &piece);
        }
        ExactPair sum;
        pair_add(&total, &piece, &sum);
        total = sum;
    }
    OriorExactInteger numerator = total.top;
    ring_require(orior_exact_scale_by_ten(&numerator, digits), "scale");
    OriorExactInteger under;
    ring_require(orior_exact_multiply(&total.under, denominator, &under), "multiply");
    floor_divide(&numerator, &under, result);
}

/* ---------------------------------------------------------------- their sets */

/**
 * @brief The modes of a field with |k|_inf at most a radius.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[in]  radius The radius.
 * @param[out] result The field in the box [BORROWS].
 */
static void within(const TorusVector *vector, int64_t radius, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_within_box(&vector->axis[axis], radius, &result->axis[axis]);
    }
}

/**
 * @brief The exact rows of a field: axis, mode, denominator and every term of the polynomial, as canonical text.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] rows   The rows [BORROWS].
 */
static void exact_rows(const TorusVector *vector, ReachText *rows)
{
    text_put(rows, "E|");
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const TorusScalar *const scalar = &vector->axis[axis];
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            const ModeEntry *const entry = &scalar->entries[at];
            text_put(rows, "%u(%d,%d,%d)", axis, (int)entry->mode[0], (int)entry->mode[1], (int)entry->mode[2]);
            text_big(rows, &scalar->denominator);
            for (size_t term = 0u; term < entry->poly.count; term += 1u)
            {
                text_put(rows, "[%d:", (int)entry->poly.terms[term].power);
                text_big(rows, &entry->poly.terms[term].real);
                text_put(rows, ",");
                text_big(rows, &entry->poly.terms[term].imaginary);
                text_put(rows, "]");
            }
            text_put(rows, ";");
        }
    }
}

/**
 * @brief The coefficients of a field within the horizon, each read as two decimals cut at the scale.
 *
 * @param[in]  context       The field [BORROWS].
 * @param[in]  format_level  The decimal places.
 * @param[in]  horizon_level The radius.
 * @param[out] reached       The rows [BORROWS].
 * @note A cut decimal is a reduced pair. 1/2 read at one place and at two places is the same value. Only a value
 *       whose expansion goes on moves with the scale.
 */
static void decimal_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    const TorusVector *const vector = (const TorusVector *)context;
    TorusVector held;
    within(vector, horizon_level, &held);
    // The scale is thirty or thirty-one places here, and fits a uint32_t.
    const uint32_t digits = (uint32_t)format_level;
    OriorExactInteger scale;
    ten_to(digits, &scale);
    text_put(reached, "D|");
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const TorusScalar *const scalar = &held.axis[axis];
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            const ModeEntry *const entry = &scalar->entries[at];
            text_put(reached, "%u(%d,%d,%d)", axis, (int)entry->mode[0], (int)entry->mode[1], (int)entry->mode[2]);
            for (unsigned part = 0u; part < 2u; part += 1u)
            {
                OriorExactInteger value;
                evaluate_part(&entry->poly, &scalar->denominator, part, digits, &value);
                ExactPair pair;
                pair_reduced(&value, &scale, &pair);
                text_put(reached, " ");
                text_pair(reached, &pair);
            }
            text_put(reached, ";");
        }
    }
    vec_free(&held);
}

/**
 * @brief The coefficients of a field within the horizon, exact. The scale is not part of the representation.
 *
 * @param[in]  context       The field [BORROWS].
 * @param[in]  format_level  Unused.
 * @param[in]  horizon_level The radius.
 * @param[out] reached       The rows [BORROWS].
 */
static void ring_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)format_level;
    TorusVector held;
    within((const TorusVector *)context, horizon_level, &held);
    exact_rows(&held, reached);
    vec_free(&held);
}

/**
 * @brief The ABC datum with A as a reduced pair, and B = 2, C = 3.
 *
 * @param[in]  amplitude The amplitude A [BORROWS].
 * @param[out] vector    The field [BORROWS].
 */
static void abc_with_amplitude(const ExactPair *amplitude, TorusVector *vector)
{
    // The amplitudes here have at most thirteen places, and the pair fits int64_t.
    const int64_t top = (int64_t)amplitude->top.limb[0] | ((int64_t)amplitude->top.limb[1] << 32);
    const int64_t under = (int64_t)amplitude->under.limb[0] | ((int64_t)amplitude->under.limb[1] << 32);
    abc_field_fraction(amplitude->top.sign * top, under, 2, 3, vector);
}

/**
 * @brief The generic datum with the amplitude of its first sine as a reduced pair.
 *
 * @param[in]  amplitude The amplitude [BORROWS].
 * @param[out] vector    The field [BORROWS].
 */
static void generic_with_amplitude(const ExactPair *amplitude, TorusVector *vector)
{
    // The amplitudes here have at most thirteen places, and the pair fits int64_t.
    const int64_t top = (int64_t)amplitude->top.limb[0] | ((int64_t)amplitude->top.limb[1] << 32);
    const int64_t under = (int64_t)amplitude->under.limb[0] | ((int64_t)amplitude->under.limb[1] << 32);
    generic_field_fraction(amplitude->top.sign * top, under, vector);
}

/**
 * @brief Frees a list of vector fields.
 *
 * @param[in,out] vectors The fields [BORROWS].
 * @param[in]     count   How many.
 */
static void vectors_free(TorusVector *vectors, size_t count)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        vec_free(&vectors[at]);
    }
}

/**
 * @brief u_2 of the ABC datum with A deposited at the scale, within the horizon.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  The decimal places of the deposit.
 * @param[in]  horizon_level The radius.
 * @param[out] reached       The rows [BORROWS].
 */
static void measured_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    ExactPair amplitude;
    deposit_at(format_level, &amplitude);
    TorusVector datum;
    abc_with_amplitude(&amplitude, &datum);
    TorusVector held[3];
    taylor_velocity(&datum, VISCOSITY, 2u, held, NULL);
    TorusVector boxed;
    within(&held[2], horizon_level, &boxed);
    exact_rows(&boxed, reached);
    vec_free(&boxed);
    vectors_free(held, 3u);
    vec_free(&datum);
}

/**
 * @brief Whether u_1 = -4 nu pi^2 u_0 holds per axis, as one row per mode of u_0.
 *
 * @param[in]  coefficients The coefficients u_0 and u_1 [BORROWS].
 * @param[out] rows         The rows [BORROWS].
 */
static void ratio_rows(const TorusVector *coefficients, ReachText *rows)
{
    RingRatio rate;
    decay_rate(VISCOSITY, -4, &rate);
    TorusVector expected;
    vec_times(&coefficients[0], &rate, &expected);
    ratio_free(&rate);
    text_put(rows, "R|");
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const int same = scalar_equal(&coefficients[1].axis[axis], &expected.axis[axis]);
        const TorusScalar *const scalar = &coefficients[0].axis[axis];
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            const int32_t *const mode = scalar->entries[at].mode;
            text_put(rows, "%u(%d,%d,%d)%s;", axis, (int)mode[0], (int)mode[1], (int)mode[2], truth(same));
        }
    }
    vec_free(&expected);
}

/**
 * @brief The ratio rows of the ABC datum with A deposited at the scale.
 *
 * @param[in]  context       Unused [BORROWS].
 * @param[in]  format_level  The decimal places of the deposit.
 * @param[in]  horizon_level Unused.
 * @param[out] reached       The rows [BORROWS].
 */
static void ratio_reach(const void *context, int64_t format_level, int64_t horizon_level, ReachText *reached)
{
    (void)context;
    (void)horizon_level;
    ExactPair amplitude;
    deposit_at(format_level, &amplitude);
    TorusVector datum;
    abc_with_amplitude(&amplitude, &datum);
    TorusVector held[2];
    taylor_velocity(&datum, VISCOSITY, 1u, held, NULL);
    ratio_rows(held, reached);
    vectors_free(held, 2u);
    vec_free(&datum);
}

/**
 * @brief Their sets: the Taylor coefficients of (1)-(3) on the unit torus, read by the probes.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where each object has its forced type.
 */
static int report_their_sets(FILE *out)
{
    fputs("  their sets: Taylor coefficients of (1)-(3) on the unit torus, read by the probes\n", out);
    TorusVector generic;
    generic_field(&generic);
    TorusVector velocities[4];
    taylor_velocity(&generic, VISCOSITY, 3u, velocities, NULL);
    // Its modes reach |k|_inf = 2.
    const TorusVector *const second = &velocities[2];

    ReachText target;
    text_begin(&target);
    exact_rows(second, &target);
    const Verdict decimal_narrow = boundary_type(decimal_reach, second, 30, 1, &target);
    const Verdict decimal_full = boundary_type(decimal_reach, second, 30, 2, &target);
    const Verdict ring_narrow = boundary_type(ring_reach, second, 30, 1, &target);
    const Verdict ring_full = boundary_type(ring_reach, second, 30, 2, &target);
    text_free(&target);
    describe(out, "u_2 in decimals, 30 places, modes |k|_inf <= 1", decimal_narrow);
    describe(out, "u_2 in decimals, 30 places, modes |k|_inf <= 2", decimal_full);
    describe(out, "u_2 in the ring, modes |k|_inf <= 1", ring_narrow);
    describe(out, "u_2 in the ring, modes |k|_inf <= 2", ring_full);

    // A measured amplitude: the ABC datum with A deposited to 6 places, against a stand-in true value.
    ExactPair truth_pair;
    truth_value(&truth_pair);
    TorusVector true_datum;
    abc_with_amplitude(&truth_pair, &true_datum);
    TorusVector true_velocities[3];
    taylor_velocity(&true_datum, VISCOSITY, 2u, true_velocities, NULL);
    text_begin(&target);
    exact_rows(&true_velocities[2], &target);
    const Verdict measured = boundary_type(measured_reach, NULL, 6, 1, &target);
    text_free(&target);
    describe(out, "u_2 from A deposited to 6 places, against true A", measured);

    // The ratio in which the amplitude cancels: u_1 / u_0 mode by mode is -4 nu pi^2 for either A.
    text_begin(&target);
    ratio_rows(true_velocities, &target);
    const Verdict ratio = boundary_type(ratio_reach, NULL, 6, 1, &target);
    const int ratio_all_true =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = target.bytes, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "False", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) == NULL;
    text_free(&target);
    describe(out, "u_1/u_0 = -4 nu pi^2, deposit against true A", ratio);
    fprintf(out, "    the ratio identity holds at every mode for the true amplitude: %s\n\n", truth(ratio_all_true));

    vectors_free(true_velocities, 3u);
    vec_free(&true_datum);
    vectors_free(velocities, 4u);
    vec_free(&generic);
    return (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = decimal_narrow.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "format and completeness", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = decimal_full.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "format", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = ring_narrow.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "completeness", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = ring_full.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "none", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = measured.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "measurement", .ci = EMBED_FALSE)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = ratio.type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "none", .ci = EMBED_FALSE)) &&
           ratio_all_true;
}

/* ---------------------------------------------------------------- inheritance through the chain of constructors */

/**
 * @brief Whether the difference of two fields is zero.
 *
 * @param[in] left  First field [BORROWS].
 * @param[in] right Second field [BORROWS].
 * @return          1 where they differ.
 */
static int differ(const TorusVector *left, const TorusVector *right)
{
    TorusVector difference;
    vec_sub(left, right, &difference);
    const int nonzero = !vec_is_zero(&difference);
    vec_free(&difference);
    return nonzero;
}

/**
 * @brief Which types the chain datum -> u_1 -> u_2 -> u_3 carries forward.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where each inheritance has its forced outcome.
 */
static int report_inheritance(FILE *out)
{
    fputs("  inheritance: which types the chain datum -> u_1 -> u_2 -> u_3 carries forward\n", out);
    enum
    {
        DEPTH = 3
    };
    TorusVector generic;
    generic_field(&generic);
    TorusVector velocities[DEPTH + 1];
    taylor_velocity(&generic, VISCOSITY, DEPTH, velocities, NULL);

    // Format: in the ring, the scale probe is silent at every order. In decimals, it moves once pi enters.
    int ring_silent[DEPTH + 1];
    int decimal_moves[DEPTH + 1];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        const TorusVector *const held = &velocities[step];
        const int64_t radius = vec_mode_radius(held);
        ReachText target;
        text_begin(&target);
        exact_rows(held, &target);
        ring_silent[step] = !boundary_type(ring_reach, held, 30, radius, &target).moved_by_format;
        decimal_moves[step] = boundary_type(decimal_reach, held, 30, radius, &target).moved_by_format;
        text_free(&target);
    }
    int decimal_pattern = !decimal_moves[0];
    for (unsigned step = 1u; step <= DEPTH; step += 1u)
    {
        decimal_pattern = decimal_pattern && decimal_moves[step];
    }
    fprintf(out, "    format, ring:     scale probe silent at orders 0..%d: ", DEPTH);
    print_truths(out, ring_silent, DEPTH + 1u);
    fprintf(out, "\n    format, decimals: scale probe moves at orders 0..%d:  ", DEPTH);
    print_truths(out, decimal_moves, DEPTH + 1u);
    fputs("\n      silent at the datum, whose coefficients are rational, and moving from order 1 on: the\n", out);
    fprintf(out, "      derivative brings pi in, and a coefficient with pi has no last digit: %s\n",
            truth(decimal_pattern));

    // Measurement: the gap of a deposit passes to every order. On the linear ABC solution it passes exactly,
    // gap_m = (-4 nu pi^2)^m gap_0. On the generic datum it is nonzero at every order.
    ExactPair truth_pair;
    truth_value(&truth_pair);
    ExactPair deposit;
    deposit_at(6, &deposit);
    TorusVector datum;
    TorusVector abc_dep[DEPTH + 1];
    TorusVector abc_true[DEPTH + 1];
    abc_with_amplitude(&deposit, &datum);
    taylor_velocity(&datum, VISCOSITY, DEPTH, abc_dep, NULL);
    vec_free(&datum);
    abc_with_amplitude(&truth_pair, &datum);
    taylor_velocity(&datum, VISCOSITY, DEPTH, abc_true, NULL);
    vec_free(&datum);
    RingRatio rate;
    decay_rate(VISCOSITY, -4, &rate);
    TorusVector gap_0;
    vec_sub(&abc_true[0], &abc_dep[0], &gap_0);
    int abc_gaps[DEPTH + 1];
    int abc_exact = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        abc_gaps[step] = differ(&abc_true[step], &abc_dep[step]);
        TorusVector gap;
        vec_sub(&abc_true[step], &abc_dep[step], &gap);
        RingRatio power;
        ratio_power(&rate, step, &power);
        TorusVector expected;
        vec_times(&gap_0, &power, &expected);
        abc_exact = abc_exact && vec_equal(&gap, &expected);
        vec_free(&gap);
        vec_free(&expected);
        ratio_free(&power);
    }
    ratio_free(&rate);
    vec_free(&gap_0);

    TorusVector gen_dep[DEPTH + 1];
    TorusVector gen_true[DEPTH + 1];
    generic_with_amplitude(&deposit, &datum);
    taylor_velocity(&datum, VISCOSITY, DEPTH, gen_dep, NULL);
    vec_free(&datum);
    generic_with_amplitude(&truth_pair, &datum);
    taylor_velocity(&datum, VISCOSITY, DEPTH, gen_true, NULL);
    vec_free(&datum);
    int gen_gaps[DEPTH + 1];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        gen_gaps[step] = differ(&gen_true[step], &gen_dep[step]);
    }
    fprintf(out, "    measurement, ABC:     gap nonzero at orders 0..%d: ", DEPTH);
    print_truths(out, abc_gaps, DEPTH + 1u);
    fprintf(out, " ; gap_m = (-4 nu pi^2)^m gap_0 exactly: %s\n", truth(abc_exact));
    fprintf(out, "    measurement, generic: gap nonzero at orders 0..%d: ", DEPTH);
    print_truths(out, gen_gaps, DEPTH + 1u);
    fputs(" (carried, not lowered by any order)\n", out);

    // Completeness: the modes outside a fixed horizon, per order, pass forward and grow.
    size_t outside[DEPTH + 1];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        outside[step] = vec_modes_outside(&velocities[step], 1);
    }
    int growing = outside[DEPTH] > outside[0];
    for (unsigned step = 0u; step < DEPTH; step += 1u)
    {
        growing = growing && (outside[step + 1u] >= outside[step]);
    }
    fprintf(out, "    completeness: modes outside |k|_inf <= 1 at orders 0..%d: [", DEPTH);
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        fprintf(out, "%s%llu", (step > 0u) ? ", " : "", (unsigned long long)outside[step]);
    }
    fprintf(out, "] ; carried and growing: %s\n\n", truth(growing));

    int all_ring_silent = 1;
    int all_abc_gaps = 1;
    int all_gen_gaps = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        all_ring_silent = all_ring_silent && ring_silent[step];
        all_abc_gaps = all_abc_gaps && abc_gaps[step];
        all_gen_gaps = all_gen_gaps && gen_gaps[step];
    }
    vectors_free(gen_true, DEPTH + 1u);
    vectors_free(gen_dep, DEPTH + 1u);
    vectors_free(abc_true, DEPTH + 1u);
    vectors_free(abc_dep, DEPTH + 1u);
    vectors_free(velocities, DEPTH + 1u);
    vec_free(&generic);
    return all_ring_silent && decimal_pattern && all_abc_gaps && abc_exact && all_gen_gaps && growing;
}

/* ---------------------------------------------------------------- the constructor of disjoint translates, its algebra
 *
 * Polynomials in y = 8 x with integer coefficients, lowest power first. The unit interval is y in [0, 8]. A
 * piecewise polynomial is a list of (lower, upper, polynomial) on integer cells, and zero off its pieces. */

/** @brief A polynomial in y with exact integer coefficients, lowest power first. */
typedef struct
{
    OriorExactInteger *values; /**< The coefficients [OWNS]. */
    size_t count;              /**< How many coefficients. At least one. */
} LinePoly;

/** @brief One piece of a piecewise polynomial: a polynomial on the cell [lower, upper]. */
typedef struct
{
    int64_t lower; /**< The lower end of the cell. */
    int64_t upper; /**< The upper end of the cell. */
    LinePoly poly; /**< The polynomial on the cell. */
} LinePiece;

/** @brief A piecewise polynomial: pieces on integer cells. */
typedef struct
{
    LinePiece *pieces; /**< The pieces [OWNS]. */
    size_t count;      /**< How many pieces. */
} Piecewise;

/**
 * @brief A polynomial of a count of zero coefficients.
 *
 * @param[in]  count The count, at least one.
 * @param[out] poly  The polynomial [BORROWS].
 */
static void line_zero(size_t count, LinePoly *poly)
{
    poly->values = (OriorExactInteger *)calloc(count, sizeof(OriorExactInteger));
    if (poly->values == NULL)
    {
        fprintf(stderr, "  out of memory\n");
        exit(2);
    }
    poly->count = count;
}

/**
 * @brief A polynomial from machine integers.
 *
 * @param[in]  values The coefficients, lowest power first [BORROWS].
 * @param[in]  count  How many.
 * @param[out] poly   The polynomial [BORROWS].
 */
static void line_from(const int64_t *values, size_t count, LinePoly *poly)
{
    line_zero(count, poly);
    for (size_t at = 0u; at < count; at += 1u)
    {
        big_set(&poly->values[at], values[at]);
    }
}

/**
 * @brief A copy of a polynomial.
 *
 * @param[in]  source The polynomial [BORROWS].
 * @param[out] copy   The copy [BORROWS].
 */
static void line_copy(const LinePoly *source, LinePoly *copy)
{
    line_zero(source->count, copy);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy->values, .src = source->values,
               .bytes = source->count * sizeof(OriorExactInteger));
}

/**
 * @brief Frees a polynomial.
 *
 * @param[in,out] poly The polynomial [BORROWS].
 */
static void line_free(LinePoly *poly)
{
    free(poly->values);
    poly->values = NULL;
    poly->count = 0u;
}

/**
 * @brief The product of two polynomials.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[out] product The product [BORROWS].
 */
static void line_mul(const LinePoly *left, const LinePoly *right, LinePoly *product)
{
    line_zero(left->count + right->count - 1u, product);
    for (size_t left_at = 0u; left_at < left->count; left_at += 1u)
    {
        for (size_t right_at = 0u; right_at < right->count; right_at += 1u)
        {
            OriorExactInteger term;
            ring_require(orior_exact_multiply(&left->values[left_at], &right->values[right_at], &term), "multiply");
            OriorExactInteger *const into = &product->values[left_at + right_at];
            ring_require(orior_exact_add(into, &term, into), "add");
        }
    }
}

/**
 * @brief Adds a polynomial into a running sum.
 *
 * @param[in,out] total The running sum [BORROWS].
 * @param[in]     poly  The polynomial to add [BORROWS].
 */
static void line_accumulate(LinePoly *total, const LinePoly *poly)
{
    if (poly->count > total->count)
    {
        OriorExactInteger *const grown =
            (OriorExactInteger *)realloc(total->values, poly->count * sizeof(OriorExactInteger));
        if (grown == NULL)
        {
            fprintf(stderr, "  out of memory\n");
            exit(2);
        }
        for (size_t at = total->count; at < poly->count; at += 1u)
        {
            orior_exact_zero(&grown[at]);
        }
        total->values = grown;
        total->count = poly->count;
    }
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        ring_require(orior_exact_add(&total->values[at], &poly->values[at], &total->values[at]), "add");
    }
}

/**
 * @brief The derivative d/dx = 8 d/dy, on the lattice of eighths.
 *
 * @param[in]  poly   The polynomial [BORROWS].
 * @param[out] result The derivative [BORROWS].
 */
static void line_derivative(const LinePoly *poly, LinePoly *result)
{
    if (poly->count < 2u)
    {
        line_zero(1u, result);
        return;
    }
    line_zero(poly->count - 1u, result);
    for (size_t at = 1u; at < poly->count; at += 1u)
    {
        OriorExactInteger factor;
        big_set(&factor, 8 * (int64_t)at);
        ring_require(orior_exact_multiply(&poly->values[at], &factor, &result->values[at - 1u]), "multiply");
    }
}

/**
 * @brief Whether every coefficient of a polynomial is zero.
 *
 * @param[in] poly The polynomial [BORROWS].
 * @return         1 where it is zero.
 */
static int line_is_zero(const LinePoly *poly)
{
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        if (poly->values[at].sign != 0)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief The exact integral of a polynomial over [lower, upper] in y, as a reduced pair.
 *
 * @param[in]  poly  The polynomial [BORROWS].
 * @param[in]  lower The lower end.
 * @param[in]  upper The upper end.
 * @param[out] total The integral [BORROWS].
 */
static void line_integral(const LinePoly *poly, int64_t lower, int64_t upper, ExactPair *total)
{
    pair_small(0, 1, total);
    OriorExactInteger low;
    OriorExactInteger high;
    big_set(&low, lower);
    big_set(&high, upper);
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        // The exponent is at + 1, a count of coefficients, and fits a uint32_t.
        OriorExactInteger high_power;
        OriorExactInteger low_power;
        big_power(&high, (uint32_t)(at + 1u), &high_power);
        big_power(&low, (uint32_t)(at + 1u), &low_power);
        OriorExactInteger span;
        ring_require(orior_exact_subtract(&high_power, &low_power, &span), "subtract");
        OriorExactInteger top;
        ring_require(orior_exact_multiply(&poly->values[at], &span, &top), "multiply");
        OriorExactInteger under;
        big_set(&under, (int64_t)at + 1);
        ExactPair piece;
        pair_reduced(&top, &under, &piece);
        ExactPair sum;
        pair_add(total, &piece, &sum);
        *total = sum;
    }
}

/**
 * @brief Frees a piecewise polynomial.
 *
 * @param[in,out] pieces The piecewise polynomial [BORROWS].
 */
static void piecewise_free(Piecewise *pieces)
{
    for (size_t at = 0u; at < pieces->count; at += 1u)
    {
        line_free(&pieces->pieces[at].poly);
    }
    free(pieces->pieces);
    pieces->pieces = NULL;
    pieces->count = 0u;
}

/**
 * @brief Appends a piece, and takes its polynomial.
 *
 * @param[in,out] pieces The piecewise polynomial [BORROWS].
 * @param[in]     lower  The lower end.
 * @param[in]     upper  The upper end.
 * @param[in,out] poly   The polynomial. The pieces own it after the call [OWNS].
 */
static void piecewise_push(Piecewise *pieces, int64_t lower, int64_t upper, LinePoly *poly)
{
    LinePiece *const grown = (LinePiece *)realloc(pieces->pieces, (pieces->count + 1u) * sizeof(LinePiece));
    if (grown == NULL)
    {
        fprintf(stderr, "  out of memory\n");
        exit(2);
    }
    pieces->pieces = grown;
    pieces->pieces[pieces->count].lower = lower;
    pieces->pieces[pieces->count].upper = upper;
    pieces->pieces[pieces->count].poly = *poly;
    poly->values = NULL;
    poly->count = 0u;
    pieces->count += 1u;
}

/**
 * @brief The order of two integers, for qsort.
 *
 * @param[in] left  First integer [BORROWS].
 * @param[in] right Second integer [BORROWS].
 * @return          -1, 0 or 1.
 */
static int point_order(const void *left, const void *right)
{
    const int64_t first = *(const int64_t *)left;
    const int64_t second = *(const int64_t *)right;
    return (first < second) ? -1 : ((first > second) ? 1 : 0);
}

/**
 * @brief Splits on every breakpoint, adds the polynomials that cover each cell, and drops the zero cells.
 *
 * @param[in]  pieces The pieces, in any order [BORROWS].
 * @param[out] result The canonical form [BORROWS].
 */
static void piecewise_canonical(const Piecewise *pieces, Piecewise *result)
{
    result->pieces = NULL;
    result->count = 0u;
    const size_t total = pieces->count * 2u;
    if (total == 0u)
    {
        return;
    }
    int64_t *const points = (int64_t *)malloc(total * sizeof(int64_t));
    if (points == NULL)
    {
        fprintf(stderr, "  out of memory\n");
        exit(2);
    }
    for (size_t at = 0u; at < pieces->count; at += 1u)
    {
        points[2u * at] = pieces->pieces[at].lower;
        points[(2u * at) + 1u] = pieces->pieces[at].upper;
    }
    qsort(points, total, sizeof(int64_t), point_order);
    size_t distinct = 0u;
    for (size_t at = 0u; at < total; at += 1u)
    {
        if ((distinct == 0u) || (points[distinct - 1u] != points[at]))
        {
            points[distinct] = points[at];
            distinct += 1u;
        }
    }
    for (size_t at = 0u; (at + 1u) < distinct; at += 1u)
    {
        const int64_t lower = points[at];
        const int64_t upper = points[at + 1u];
        LinePoly sum;
        line_zero(1u, &sum);
        for (size_t which = 0u; which < pieces->count; which += 1u)
        {
            if ((pieces->pieces[which].lower <= lower) && (upper <= pieces->pieces[which].upper))
            {
                line_accumulate(&sum, &pieces->pieces[which].poly);
            }
        }
        if (line_is_zero(&sum))
        {
            line_free(&sum);
            continue;
        }
        piecewise_push(result, lower, upper, &sum);
    }
    free(points);
}

/**
 * @brief The pieces of two piecewise polynomials, joined into one list, then in canonical form.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
static void piecewise_add(const Piecewise *left, const Piecewise *right, Piecewise *sum)
{
    Piecewise joined = {NULL, 0u};
    for (size_t at = 0u; at < left->count; at += 1u)
    {
        LinePoly copy;
        line_copy(&left->pieces[at].poly, &copy);
        piecewise_push(&joined, left->pieces[at].lower, left->pieces[at].upper, &copy);
    }
    for (size_t at = 0u; at < right->count; at += 1u)
    {
        LinePoly copy;
        line_copy(&right->pieces[at].poly, &copy);
        piecewise_push(&joined, right->pieces[at].lower, right->pieces[at].upper, &copy);
    }
    piecewise_canonical(&joined, sum);
    piecewise_free(&joined);
}

/**
 * @brief The negative of a piecewise polynomial.
 *
 * @param[in]  pieces The piecewise polynomial [BORROWS].
 * @param[out] result The negative [BORROWS].
 */
static void piecewise_neg(const Piecewise *pieces, Piecewise *result)
{
    result->pieces = NULL;
    result->count = 0u;
    for (size_t at = 0u; at < pieces->count; at += 1u)
    {
        LinePoly copy;
        line_copy(&pieces->pieces[at].poly, &copy);
        for (size_t index = 0u; index < copy.count; index += 1u)
        {
            copy.values[index].sign = -copy.values[index].sign;
        }
        piecewise_push(result, pieces->pieces[at].lower, pieces->pieces[at].upper, &copy);
    }
}

/**
 * @brief The product of two piecewise polynomials, cell by cell.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[out] product The product [BORROWS].
 */
static void piecewise_mul(const Piecewise *left, const Piecewise *right, Piecewise *product)
{
    Piecewise raw = {NULL, 0u};
    for (size_t left_at = 0u; left_at < left->count; left_at += 1u)
    {
        for (size_t right_at = 0u; right_at < right->count; right_at += 1u)
        {
            const LinePiece *const one = &left->pieces[left_at];
            const LinePiece *const other = &right->pieces[right_at];
            const int64_t lower = (one->lower > other->lower) ? one->lower : other->lower;
            const int64_t upper = (one->upper < other->upper) ? one->upper : other->upper;
            if (lower < upper)
            {
                LinePoly poly;
                line_mul(&one->poly, &other->poly, &poly);
                piecewise_push(&raw, lower, upper, &poly);
            }
        }
    }
    piecewise_canonical(&raw, product);
    piecewise_free(&raw);
}

/**
 * @brief The derivative of a piecewise polynomial.
 *
 * @param[in]  pieces The piecewise polynomial [BORROWS].
 * @param[out] result The derivative [BORROWS].
 */
static void piecewise_derivative(const Piecewise *pieces, Piecewise *result)
{
    Piecewise raw = {NULL, 0u};
    for (size_t at = 0u; at < pieces->count; at += 1u)
    {
        LinePoly poly;
        line_derivative(&pieces->pieces[at].poly, &poly);
        piecewise_push(&raw, pieces->pieces[at].lower, pieces->pieces[at].upper, &poly);
    }
    piecewise_canonical(&raw, result);
    piecewise_free(&raw);
}

/**
 * @brief Whether a piecewise polynomial is zero.
 *
 * @param[in] pieces The piecewise polynomial [BORROWS].
 * @return           1 where its canonical form has no piece.
 */
static int piecewise_is_zero(const Piecewise *pieces)
{
    Piecewise canonical;
    piecewise_canonical(pieces, &canonical);
    const int zero = canonical.count == 0u;
    piecewise_free(&canonical);
    return zero;
}

/**
 * @brief The exact integral of a piecewise polynomial.
 *
 * @param[in]  pieces The piecewise polynomial [BORROWS].
 * @param[out] total  The integral [BORROWS].
 */
static void piecewise_integral(const Piecewise *pieces, ExactPair *total)
{
    pair_small(0, 1, total);
    for (size_t at = 0u; at < pieces->count; at += 1u)
    {
        ExactPair piece;
        line_integral(&pieces->pieces[at].poly, pieces->pieces[at].lower, pieces->pieces[at].upper, &piece);
        ExactPair sum;
        pair_add(total, &piece, &sum);
        *total = sum;
    }
}

/**
 * @brief left * right', the shape of the transport term (u . grad) v in one variable.
 *
 * @param[in]  left   First field [BORROWS].
 * @param[in]  right  Second field [BORROWS].
 * @param[out] result The product [BORROWS].
 */
static void bilinear(const Piecewise *left, const Piecewise *right, Piecewise *result)
{
    Piecewise slope;
    piecewise_derivative(right, &slope);
    piecewise_mul(left, &slope, result);
    piecewise_free(&slope);
}

/**
 * @brief (y - lower)^2 (upper - y)^2 on [lower, upper]: a piece with compact support and integer coefficients.
 *
 * @param[in]  lower  The lower end.
 * @param[in]  upper  The upper end.
 * @param[out] pieces The piece [BORROWS].
 */
static void bump(int64_t lower, int64_t upper, Piecewise *pieces)
{
    const int64_t rising_values[2] = {-lower, 1};
    const int64_t falling_values[2] = {upper, -1};
    LinePoly rising;
    LinePoly falling;
    line_from(rising_values, 2u, &rising);
    line_from(falling_values, 2u, &falling);
    LinePoly rising_square;
    LinePoly falling_square;
    line_mul(&rising, &rising, &rising_square);
    line_mul(&falling, &falling, &falling_square);
    LinePoly poly;
    line_mul(&rising_square, &falling_square, &poly);
    line_free(&rising);
    line_free(&falling);
    line_free(&rising_square);
    line_free(&falling_square);
    pieces->pieces = NULL;
    pieces->count = 0u;
    piecewise_push(pieces, lower, upper, &poly);
}

/** @brief The three outcomes of the split test on one pair of pieces. */
typedef struct
{
    int by_coefficients;  /**< Whether every coefficient of the difference is zero. */
    int by_integral;      /**< Whether the integral of the square of the difference is zero. */
    ExactPair cross_mass; /**< The integral of the square of the cross terms. */
} SplitTest;

/**
 * @brief The split test: the bilinear term of a sum, against the sum of the bilinear terms of the pieces.
 *
 * @param[in] first  First piece [BORROWS].
 * @param[in] second Second piece [BORROWS].
 * @return           The outcomes by two routes, and the mass of the cross terms.
 */
static SplitTest split_holds(const Piecewise *first, const Piecewise *second)
{
    Piecewise whole;
    piecewise_add(first, second, &whole);
    Piecewise of_sum;
    bilinear(&whole, &whole, &of_sum);
    Piecewise first_term;
    Piecewise second_term;
    bilinear(first, first, &first_term);
    bilinear(second, second, &second_term);
    Piecewise of_parts;
    piecewise_add(&first_term, &second_term, &of_parts);
    Piecewise negative;
    piecewise_neg(&of_parts, &negative);
    Piecewise difference;
    piecewise_add(&of_sum, &negative, &difference);

    SplitTest test;
    // Route one: every coefficient on every cell is zero. Route two: the exact integral of the square is zero.
    test.by_coefficients = piecewise_is_zero(&difference);
    Piecewise square;
    piecewise_mul(&difference, &difference, &square);
    ExactPair mass;
    piecewise_integral(&square, &mass);
    test.by_integral = mass.top.sign == 0;
    piecewise_free(&square);

    Piecewise forward;
    Piecewise backward;
    bilinear(first, second, &forward);
    bilinear(second, first, &backward);
    Piecewise cross;
    piecewise_add(&forward, &backward, &cross);
    piecewise_mul(&cross, &cross, &square);
    piecewise_integral(&square, &test.cross_mass);

    piecewise_free(&square);
    piecewise_free(&cross);
    piecewise_free(&forward);
    piecewise_free(&backward);
    piecewise_free(&difference);
    piecewise_free(&negative);
    piecewise_free(&of_parts);
    piecewise_free(&first_term);
    piecewise_free(&second_term);
    piecewise_free(&of_sum);
    piecewise_free(&whole);
    return test;
}

/**
 * @brief The constructor of disjoint translates: the bilinear term of a sum splits exactly with disjoint supports.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where each outcome is the forced one.
 */
static int report_constructor(FILE *out)
{
    fputs("  the disjoint-translate constructor: the bilinear term of a sum splits exactly when supports are "
          "disjoint\n",
          out);
    Piecewise piece;
    Piecewise apart;
    Piecewise overlapping;
    // In eighths: [0, 1/4], then [1/2, 3/4], then [1/8, 3/8].
    bump(0, 2, &piece);
    bump(4, 6, &apart);
    bump(1, 3, &overlapping);

    const SplitTest disjoint = split_holds(&piece, &apart);
    const SplitTest overlap = split_holds(&piece, &overlapping);

    Piecewise sum;
    piecewise_add(&piece, &apart, &sum);
    Piecewise sum_slope;
    piecewise_derivative(&sum, &sum_slope);
    Piecewise piece_slope;
    Piecewise apart_slope;
    piecewise_derivative(&piece, &piece_slope);
    piecewise_derivative(&apart, &apart_slope);
    Piecewise slopes;
    piecewise_add(&piece_slope, &apart_slope, &slopes);
    Piecewise negative;
    piecewise_neg(&slopes, &negative);
    Piecewise difference;
    piecewise_add(&sum_slope, &negative, &difference);
    const int linear_inherits = piecewise_is_zero(&difference);

    fputs("    pieces in eighths: (y-a)^2 (b-y)^2 on [0,2] and on [4,6] (disjoint), on [1,3] (overlapping)\n", out);
    ReachText line;
    text_begin(&line);
    text_put(&line, "    disjoint: split holds by coefficients %s, by integral %s ; int (cross)^2 dy = ",
             truth(disjoint.by_coefficients), truth(disjoint.by_integral));
    text_pair(&line, &disjoint.cross_mass);
    fprintf(out, "%s\n", line.bytes);
    text_free(&line);
    text_begin(&line);
    text_put(&line, "    overlap (the null): split holds by coefficients %s, by integral %s ; int (cross)^2 dy = ",
             truth(overlap.by_coefficients), truth(overlap.by_integral));
    text_pair(&line, &overlap.cross_mass);
    fprintf(out, "%s\n", line.bytes);
    text_free(&line);
    fprintf(out, "    the linear terms split regardless (derivative of the sum): %s\n", truth(linear_inherits));
    fputs("   , a sum of disjoint translates inherits the equation from its pieces through the bilinear\n", out);
    fputs("    term only because the cross terms vanish; the construction is theirs, this is its algebra.\n\n", out);

    piecewise_free(&difference);
    piecewise_free(&negative);
    piecewise_free(&slopes);
    piecewise_free(&piece_slope);
    piecewise_free(&apart_slope);
    piecewise_free(&sum_slope);
    piecewise_free(&sum);
    piecewise_free(&piece);
    piecewise_free(&apart);
    piecewise_free(&overlapping);
    return disjoint.by_coefficients && disjoint.by_integral && (disjoint.cross_mass.top.sign == 0) &&
           !overlap.by_coefficients && !overlap.by_integral && (overlap.cross_mass.top.sign > 0) && linear_inherits;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    fputs("  PROOF: the boundary function defined by probes, run on their sets, inheritance checked\n\n", out);
    const int controls = report_controls(out);
    const int sets = report_their_sets(out);
    const int inheritance = report_inheritance(out);
    const int constructor = report_constructor(out);
    const int all = controls && sets && inheritance && constructor;
    if (all)
    {
        fputs("  every part lands: the probes recover the known types, read their sets, and the chain carries\n", out);
        fputs("  the measurement and completeness types forward while the ring carries no format type at all.\n", out);
        fputs("  nothing here bears on whether (A), (B), (C) or (D) holds.\n", out);
    }
    else
    {
        fputs("  a part missed its forced outcome: refuted as stated.\n", out);
    }
    fflush(out);
    return all ? 0 : 1;
}
