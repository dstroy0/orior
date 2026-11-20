// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file anchor_sift.h
 * @brief The search arms under test and the dispatcher that chooses between them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note This is the kernel. The bench measures all of it, and nothing here reads a
 *       clock, builds a corpus or prints a row. Those belong to the driver.
 * @note Every arm has the same signature and returns the same count, so that a driver can call them
 *       through one pointer. A disagreement between two of them is a defect.
 */
#ifndef ANCHOR_SIFT_H
#define ANCHOR_SIFT_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stddef.h>
#include <stdint.h>

/** @brief Anchors the sift arms place. The cascade depth log2(N)/H2 sits near five on these corpora. */
#define ANCHOR_SIFT_ANCHORS 4u

    /**
     * @brief One search arm: count exact occurrences of a needle in a corpus.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     */
    typedef size_t (*AnchorSiftArm)(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences with a comparison at every alignment.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note The reference. Every other arm has to agree with it or its measurement is void.
     */
    size_t anchor_sift_naive(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences. It tests anchors in order and stops at the first that refutes.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note Short circuiting makes each probe wait on the one before it. Measured, this wins on a
     *       memoryless corpus, where the first probe rejects almost every alignment on its own.
     */
    size_t anchor_sift_inorder(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences. It tests every anchor unconditionally and combines the results.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note Dependency depth two. Every probe issues at once, and the code takes one branch on the combined
     *       result. Measured, this is 2.8 to 3.1 times faster than the in order arm on a skewed corpus
     *       and 1.6 times slower on a memoryless one.
     */
    size_t anchor_sift_free(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /** @brief What the dispatcher needs to choose an arm, both of it cheap to obtain. */
    typedef struct
    {
        double collision_entropy; /**< H2 of the corpus, one pass over a byte histogram. */
        size_t distinct_symbols;  /**< How many byte values the corpus actually uses. */
        size_t needle_len;        /**< Known at the call. */
    } AnchorSiftPlan;

    /**
     * @brief Returns the arm to run for this corpus and this needle length.
     *
     * @param[in] plan Corpus statistics and the needle length [BORROWS].
     * @return         The arm to call. Never NULL.
     * @note Every arm is sound. Thus the choice costs speed and never correctness. That makes a
     *       wrong dispatch a performance defect instead of a wrong answer.
     * @note The rule comes from the cycle measurements and belongs to the machine that produced them.
     *       Measure again before you trust it on another part.
     */
    AnchorSiftArm anchor_sift_choose(const AnchorSiftPlan *plan);

    /**
     * @brief Names the arm the dispatcher would choose, for a driver that wants to print it.
     *
     * @param[in] arm Arm returned by anchor_sift_choose [BORROWS].
     * @return        A static name, or "unknown" where the pointer is not one of the four.
     */
    const char *anchor_sift_arm_name(AnchorSiftArm arm);

#ifdef __cplusplus
}
#endif

#endif
