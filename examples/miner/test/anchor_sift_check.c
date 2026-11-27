// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file anchor_sift_check.c
 * @brief Checks that the three search arms agree, and that the dispatcher follows its rule.
 *
 * @note The test builds two corpora of 65536 bytes. The uniform corpus draws every byte value with equal
 *       chance. The skewed corpus draws from sixteen symbols with a geometric weight. Needles come from
 *       the corpus itself, at lengths 1 through 64. Thus every needle occurs at least once.
 * @note Every arm must return the same count as the naive arm on every needle. On the uniform corpus, the
 *       dispatcher must choose the in order arm. On the skewed corpus, it must choose the free arm for a
 *       short needle, and the in order arm for a needle above sixteen bytes.
 * @note The test also times the free arm against the in order arm on both corpora and prints the ratio.
 *       The ratio is a measurement on this machine, and the test does not assert it.
 */
#include "anchor_sift.h"
#include "../../../includes/platform/platform.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

#define CORPUS_BYTES 65536u
#define NEEDLE_DRAWS 24u
#define TIMING_PASSES 64u

/** @brief Count of failed checks. */
static unsigned s_failures = 0u;

/**
 * @brief Steps a 64-bit linear congruential generator and returns its high half.
 *
 * @param[in,out] state Generator state [BORROWS].
 * @return              Thirty-two pseudo-random bits.
 */
static uint32_t next_random(uint64_t *state)
{
    *state = (*state * 6364136223846793005ull) + 1442695040888963407ull;
    return (uint32_t)(*state >> 32u);
}

/**
 * @brief Fills a corpus with bytes of equal chance.
 *
 * @param[out] corpus Bytes to fill [BORROWS].
 * @param[in]  length How many.
 */
static void fill_uniform(uint8_t *corpus, size_t length)
{
    uint64_t state = 0x0123456789abcdefull;
    for (size_t at = 0u; at < length; at += 1u)
    {
        corpus[at] = (uint8_t)(next_random(&state) >> 24u);
    }
}

/**
 * @brief Fills a corpus from sixteen symbols, where each symbol has half the chance of the one before it.
 *
 * @param[out] corpus Bytes to fill [BORROWS].
 * @param[in]  length How many.
 */
static void fill_skewed(uint8_t *corpus, size_t length)
{
    uint64_t state = 0xfedcba9876543210ull;
    for (size_t at = 0u; at < length; at += 1u)
    {
        uint32_t draw = next_random(&state);
        uint8_t symbol = 0u;
        // Each set low bit moves one symbol along. Symbol k has chance 2^-(k+1), and symbol 15 takes the rest.
        while (((draw & 1u) != 0u) && (symbol < 15u))
        {
            symbol += 1u;
            draw >>= 1u;
        }
        corpus[at] = (uint8_t)('a' + symbol);
    }
}

/**
 * @brief Builds the plan that the dispatcher reads: the collision entropy and the distinct symbol count.
 *
 * @param[in] corpus     Bytes to measure [BORROWS].
 * @param[in] length     How many.
 * @param[in] needle_len The needle length to put in the plan.
 * @return               The plan.
 */
static AnchorSiftPlan plan_for(const uint8_t *corpus, size_t length, size_t needle_len)
{
    size_t histogram[256] = {0u};
    for (size_t at = 0u; at < length; at += 1u)
    {
        histogram[corpus[at]] += 1u;
    }
    double collision = 0.0;
    size_t distinct = 0u;
    for (size_t value = 0u; value < 256u; value += 1u)
    {
        if (histogram[value] != 0u)
        {
            const double share = (double)histogram[value] / (double)length;
            collision += share * share;
            distinct += 1u;
        }
    }
    const AnchorSiftPlan plan = {-log2(collision), distinct, needle_len};
    return plan;
}

/**
 * @brief Reports one check, and counts it where it fails.
 *
 * @param[in] name   The name of the check [BORROWS].
 * @param[in] passed Nonzero where the check passed.
 */
static void report(const char *name, int passed)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  [");
        mmgr_line_put(output_line, passed ? "PASS" : "FAIL");
        mmgr_line_put(output_line, "] ");
        mmgr_line_put(output_line, name);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (!passed)
    {
        s_failures += 1u;
    }
}

/**
 * @brief Runs every arm on needles drawn from the corpus, and requires every count to match the naive arm.
 *
 * @param[in] label  The name of the corpus [BORROWS].
 * @param[in] corpus Bytes to search [BORROWS].
 * @param[in] seed   Seed for the needle positions.
 */
static void check_agreement(const char *label, const uint8_t *corpus, uint64_t seed)
{
    static const size_t lengths[] = {1u, 2u, 3u, 4u, 5u, 8u, 13u, 16u, 17u, 32u, 64u};
    const AnchorSiftArm arms[] = {anchor_sift_inorder, anchor_sift_free};
    unsigned disagreements = 0u;
    uint64_t state = seed;

    for (size_t which = 0u; which < (sizeof(lengths) / sizeof(lengths[0])); which += 1u)
    {
        const size_t needle_len = lengths[which];
        for (unsigned draw = 0u; draw < NEEDLE_DRAWS; draw += 1u)
        {
            const size_t start = next_random(&state) % (CORPUS_BYTES - needle_len);
            const uint8_t *needle = corpus + start;
            const size_t expected = anchor_sift_naive(corpus, CORPUS_BYTES, needle, needle_len);
            for (size_t arm = 0u; arm < (sizeof(arms) / sizeof(arms[0])); arm += 1u)
            {
                const size_t count = arms[arm](corpus, CORPUS_BYTES, needle, needle_len);
                if ((count != expected) || (expected == 0u))
                {
                    disagreements += 1u;
                }
            }
        }
    }
    char name[96];
    snprintf(name, sizeof(name), "%s: the in order and free arms match the naive arm on every needle", label);
    report(name, disagreements == 0u);
}

/**
 * @brief Times one arm over repeated passes of one needle.
 *
 * @param[in] arm        The arm to time [BORROWS].
 * @param[in] corpus     Bytes to search [BORROWS].
 * @param[in] needle     Bytes to find [BORROWS].
 * @param[in] needle_len How many.
 * @return               Seconds of processor time.
 */
static double time_arm(AnchorSiftArm arm, const uint8_t *corpus, const uint8_t *needle, size_t needle_len)
{
    volatile size_t sink = 0u;
    const clock_t start = clock();
    for (unsigned pass = 0u; pass < TIMING_PASSES; pass += 1u)
    {
        sink = sink + arm(corpus, CORPUS_BYTES, needle, needle_len);
    }
    return (double)(clock() - start) / (double)CLOCKS_PER_SEC;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static uint8_t uniform[CORPUS_BYTES];
    static uint8_t skewed[CORPUS_BYTES];
    fill_uniform(uniform, CORPUS_BYTES);
    fill_skewed(skewed, CORPUS_BYTES);

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "anchor_sift checks");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    check_agreement("uniform", uniform, 11u);
    check_agreement("skewed", skewed, 29u);

    const AnchorSiftPlan uniform_short = plan_for(uniform, CORPUS_BYTES, 8u);
    const AnchorSiftPlan skewed_short = plan_for(skewed, CORPUS_BYTES, 8u);
    const AnchorSiftPlan skewed_long = plan_for(skewed, CORPUS_BYTES, 32u);
    const AnchorSiftPlan empty_needle = plan_for(skewed, CORPUS_BYTES, 0u);
    // MinGW printf has no %zu. The cast widens a symbol count of at most 256 and loses nothing.
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  uniform H2 ");
        mmgr_line_fixed(output_line, (double)(uniform_short.collision_entropy), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " over ");
        mmgr_line_decimal(output_line, (unsigned long long)uniform_short.distinct_symbols, 1u);
        mmgr_line_put(output_line, " symbols, skewed H2 ");
        mmgr_line_fixed(output_line, (double)(skewed_short.collision_entropy), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " over ");
        mmgr_line_decimal(output_line, (unsigned long long)skewed_short.distinct_symbols, 1u);
        mmgr_line_put(output_line, " symbols");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    report("uniform corpus, needle 8: the in order arm", anchor_sift_choose(&uniform_short) == anchor_sift_inorder);
    report("skewed corpus, needle 8: the free arm", anchor_sift_choose(&skewed_short) == anchor_sift_free);
    report("skewed corpus, needle 32: the in order arm", anchor_sift_choose(&skewed_long) == anchor_sift_inorder);
    report("empty needle: the naive arm", anchor_sift_choose(&empty_needle) == anchor_sift_naive);
    report("no plan: the naive arm", anchor_sift_choose(NULL) == anchor_sift_naive);

    const double uniform_ratio = time_arm(anchor_sift_free, uniform, uniform + 1000u, 8u) /
                                 time_arm(anchor_sift_inorder, uniform, uniform + 1000u, 8u);
    const double skewed_ratio = time_arm(anchor_sift_inorder, skewed, skewed + 1000u, 8u) /
                                time_arm(anchor_sift_free, skewed, skewed + 1000u, 8u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  measured: on the uniform corpus the free arm takes ");
        mmgr_line_fixed(output_line, (double)(uniform_ratio), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " times the in order arm");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  measured: on the skewed corpus the in order arm takes ");
        mmgr_line_fixed(output_line, (double)(skewed_ratio), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " times the free arm");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, s_failures, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (s_failures == 0u) ? 0 : 1;
}
