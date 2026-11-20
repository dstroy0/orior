// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_core.h: counters, the sift's engines, plan and run, the census and steering (orior.h includes the
// parts in order)
#ifndef ORIOR_CORE_H
#define ORIOR_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* The dispatch rule reads the field's census directly. The plan carries one. Reading it directly
 * makes the rule exact: the census stores integer counts and the comparison clears its denominators into
 * integers. */

/** @brief Anchors a sift engine places. The cascade depth log2(N)/H2 sits near five on these corpora. */
#define ORIOR_ANCHORS 4u

/**
 * @brief Set to 1 to build the engines with probe and verification counters.
 *
 * @note The gate defines both arms. #if always has a value, and an unset build is never a silent false.
 * @note At 0, the counting macros expand to nothing, and the object has no counter code. One source then serves both
 *       the timed build and the counted one. A cycle count and a read count cannot come from the same run: a count
 *       disturbs the time that it would sit beside.
 */
#ifndef ORIOR_COUNT_READS
#define ORIOR_COUNT_READS 0
#endif

#if ORIOR_COUNT_READS

    /** @brief Corpus bytes read by an anchor probe since the last reset. */
    extern uint64_t orior_probes;

    /** @brief Exact compares performed since the last reset, each reading at max needle_len bytes. */
    extern uint64_t orior_verifications;

    /**
     * @brief Sets both counters to zero.
     *
     * @note Present only in a counted build. A driver that calls it unconditionally will not link
     *       against a timed one, which is deliberate: the two builds are not interchangeable.
     */
    void orior_counters_reset(void);

#endif

/** @brief Distinct values a byte takes, the width of every census below. */
#define ORIOR_STEER_SYMBOLS 256u

/**
 * @brief Max probes that any planner here will place, defined as ORIOR_ANCHORS so that the two cannot drift.
 *
 * @note THIS CONSTANT IS THE TERMINATION ARGUMENT. Every descent below places one probe per level, and never revisits
 *       one. This value bounds the depth at compile time. The header declares it, because it is part of the contract. A
 *       caller that sizes a probe array needs it. A reader who tests whether a recursion terminates should find its
 *       bound in the header, and not in the source.
 * @note DEFINED FROM ORIOR_ANCHORS, AND NOT COPIED. A definition of one from the other makes the compiler keep the
 *       invariant that the termination argument rests on.
 */
#define ORIOR_STEER_ANCHORS ORIOR_ANCHORS

    /**
     * @brief What one pass over a corpus records about the field that it is.
     *
     * @note Only this steers the engine. The census comes off the corpus alone, and the steering is therefore the
     *       field's own, and not a parameter.
     * @note `total` is the byte count, and not the alignment count. A census describes the field, and not the search
     *       that will run over it. It does not know the needle length.
     */
    typedef struct
    {
        uint64_t occurrences[ORIOR_STEER_SYMBOLS]; /**< How often each byte value appears. */
        uint64_t total;                            /**< Bytes counted, the sum of the row above. */
        uint32_t distinct;                         /**< Byte values with a non-zero count. */
    } OriorFieldCensus;

    /**
     * @brief One search engine: count exact occurrences of a needle in a corpus.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     */
    typedef size_t (*OriorEngine)(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences with a compare at every alignment.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note The reference. Every other engine has to agree with it or its measurement is void.
     */
    size_t orior_naive(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences, and tests anchors in order until the first one that refutes.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note Short circuiting makes each probe wait on the one before it. Measured, this is faster on a
     *       memoryless corpus, where the first probe rejects almost every alignment on its own.
     */
    size_t orior_inorder(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief Counts occurrences, tests every anchor unconditionally, and combines the results.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note Dependency depth two. Every probe issues at once, and the code takes one branch on the combined result.
     *       Over 65536 bytes this runs 2.95 times faster than the in order engine on a skewed corpus, and 3.42 times on
     *       its widest row. It runs 1.63 times slower on a memoryless one. The advantage holds at every needle length
     *       from 4 to 256, and the rule that chooses between the two therefore reads no needle length.
     */
    size_t orior_free(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);

    /**
     * @brief What the dispatcher needs to choose an engine, all of it cheap to obtain.
     *
     * THE FIELD'S CENSUS, AND NOT A NUMBER DERIVED FROM IT. The rule that this plan feeds tests whether the effective
     * alphabet reaches a share of the symbols actually used. Both quantities come out of one pass over the corpus. With
     * the census in the plan, the rule reads them exactly, in integers, and the engine stores no floating point value
     * anywhere.
     *
     * @note `distinct_symbols` IS NOT A FIELD HERE. The census computes that number authoritatively. A public structure
     *       with a second copy would let a caller hand over two values that disagree, with nothing to catch it. An
     *       unread field is untidy. A field that can contradict the truth beside it is a defect that waits for a caller
     *       to fill in both.
     * @note `needle_len` is in the plan and the rule does not read it, and that is a different case. No ceiling on it
     *       beats none over the values that the bench measures. The rule does not consult it. It duplicates nothing. It
     *       stays.
     * @warning The plan BORROWS `census` for the duration of every call that takes it. A plan holds a pointer to it,
     *          because OriorFieldCensus is about two kilobytes, and a plan goes by pointer on a hot path.
     */
    typedef struct
    {
        const OriorFieldCensus *census; /**< The field's own census, which the rule reads [BORROWS]. */
        size_t needle_len;              /**< Known at the call. The rule does not read it. */
        size_t period;                  /**< Lag the corpus agrees with itself at, or zero where none. */
    } OriorPlan;

    /**
     * @brief How many anchors this corpus is worth.
     *
     * @param[in] plan Corpus statistics [BORROWS].
     * @return         ORIOR_ANCHORS where the corpus repeats at no lag, and 1 where it does.
     * @note A corpus that repeats at some period is one orbit under translation at that period. Position p carries a
     *       value that p modulo the period fixes. A needle taken at offset s therefore has needle[o] fixed by (s + o).
     *       An alignment at `at` matches that anchor when (at + o) and (s + o) agree modulo the period. The offset
     *       cancels, and every anchor tests the same congruence, whatever offset it sits at. The anchors after the
     *       first refute nothing that the first did not already refute.
     * @note Measured on a corpus of period sixteen: four anchors read 1.1875 bytes per alignment, and one anchor reads
     *       1.0000, for the same survivor rate of exactly 1/16. The reads of the extra three anchors are their only
     *       contribution.
     * @warning A period found is not a period that the whole corpus repeats. This returns 1 on any corpus whose period
     *          search cleared its floor, and a partially coherent corpus would want a count between the two. Nothing
     *          here measures that case.
     */
    size_t orior_anchors_for(const OriorPlan *plan);

    /**
     * @brief Counts occurrences with the engine and the anchor count that this plan calls for.
     *
     * @param[in] plan       Corpus statistics and the needle length [BORROWS].
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @return               How many alignments match exactly.
     * @note The entry a caller with a corpus should use. The three engines above stay public because a
     *       bench has to be able to time one of them against another with nothing chosen in between.
     */
    size_t orior_run(const OriorPlan *plan, const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                     size_t needle_len);

    /**
     * @brief Returns the engine to run for this corpus.
     *
     * @param[in] plan Corpus statistics and the needle length [BORROWS].
     * @return         The engine to call. Never NULL.
     * @note Every engine is sound. The choice costs speed, and never correctness. A wrong dispatch is therefore a
     *       performance defect, and not a wrong answer.
     * @note One term, which orior_steer_prefers_free decides in exact integer arithmetic. A flat corpus, whose
     *       effective alphabet 2^H2 reaches 85 percent of the symbols that it uses, takes the short circuiting
     *       engine. Every other corpus takes the free order one. 2^H2 is total squared over the sum of squared counts.
     *       The comparison therefore clears its denominators into 100*total^2 >= 85*distinct*sum(count^2), where true
     *       means the short circuiting engine. It stores no floating point value anywhere.
     *
     *       bench_dispatch scores it against the clock. A figure from it belongs to the toolchain that produced it: run
     *       the bench again before you quote one.
     * @note THE NEEDLE LENGTH TERM CHANGES NO ANSWER ON THIS DATA. A score of flatness alone ties this rule exactly,
     *       with the same rows and the same cycles, on every row that bench_dispatch scores. The term stays, because a
     *       tunable with no reader is an integration point. This note names it, so that nobody concludes from the code
     *       that it carries weight. Find a row where it pays, or leave it inert.
     * @note The rule comes off the cycle measurements, and belongs to the machine that produced them. Run
     *       bench_dispatch again before you trust it on another part. It sweeps both thresholds, and assumes neither.
     *       It prints a recommendation to act on, and not a confirmation.
     */
    OriorEngine orior_choose(const OriorPlan *plan);

    /**
     * @brief Names the engine the dispatcher would choose, for a driver that wants to print it.
     *
     * @param[in] engine Engine returned by orior_choose [BORROWS].
     * @return           A static name, or "unknown" where the pointer is not one of the four.
     */
    const char *orior_engine_name(OriorEngine engine);

    /* ---- the steering, folded in ---- */

    /**
     * @brief Counts the content of the corpus, in one pass.
     *
     * @param[in]  corpus     Bytes to census [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[out] census     Where the counts go [BORROWS].
     * @note Safe on a zero length corpus and on a null pointer, both of which produce an empty census whose `total` and
     *       `distinct` are zero. Every function below works on an empty census.
     */
    void orior_field_census(const uint8_t *corpus, size_t corpus_len, OriorFieldCensus *census);

    /**
     * @brief Steering magnitude of one symbol, as an integer, where larger means rarer.
     *
     * @param[in] census Field census [BORROWS].
     * @param[in] symbol Byte value to weigh.
     * @return           `census->total` minus the symbol's own count.
     *
     * @note THIS IS THE INFORMATION WEIGHT WITHOUT THE LOGARITHM. Rarity orders by P ascending, and P is count over a
     *       shared total. `total - count` orders identically to -log P, and stays an exact integer. It is a magnitude
     *       for order and comparison, and it is not an entropy in bits. Anything that wants bits must take the
     *       logarithm itself, and would bring in a double that this engine does not carry.
     * @note A symbol absent from the corpus returns the largest magnitude available, which is correct. An anchor that
     *       tests a symbol that the field never produces rejects every alignment immediately.
     */
    uint64_t orior_steer_magnitude(const OriorFieldCensus *census, uint8_t symbol);

    /**
     * @brief Orders anchor offsets so that the needle's rarest symbol gets the first test.
     *
     * @param[in,out] offsets    Anchor offsets into the needle, reordered in place [BORROWS].
     * @param[in]     count      How many offsets.
     * @param[in]     census     Field census that supplies the magnitudes [BORROWS].
     * @param[in]     needle     Bytes the offsets index [BORROWS].
     * @param[in]     needle_len How many.
     *
     * @note Insertion sort by descending magnitude. The count is at max ORIOR_ANCHORS, which is four. An insertion sort
     *       is fewer instructions than the setup of anything cleverer, and is the right choice and not a concession.
     * @note STABLE, and that carries weight, and is not incidental. Two anchors that test equally rare symbols keep the
     *       order that choose_offsets placed them in. The spatial spread that the rule exists to produce survives
     *       wherever rarity does not distinguish. An unstable sort would quietly discard the spread on a flat corpus,
     *       the corpus where the spread is all there is.
     * @note Does nothing where any argument is null, where `count` is zero, or where the census is empty. An engine
     *       with nothing to steer by keeps the order that it got.
     */
    void orior_steer_probe_order(size_t *offsets, size_t count, const OriorFieldCensus *census, const uint8_t *needle,
                                 size_t needle_len);

    /**
     * @brief Whether this field wants the free order engine, decided in exact integer arithmetic.
     *
     * @param[in] census Field census [BORROWS].
     * @return           1 for the free order engine, 0 for the short circuiting engine.
     *
     * @note THE DISPATCH RULE IN EXACT INTEGERS, WITH NO FLOATING POINT. The rule tests whether the effective alphabet
     *       2^H2 sits within 85 percent of the symbols actually used. With H2 the collision entropy, 2^H2 is exactly
     *       total^2 over the sum of the squared counts. The test
     *
     *           total^2 / sum(count^2)  >=  (85/100) * distinct
     *
     *       clears its denominators into
     *
     *           100 * total^2  >=  85 * distinct * sum(count^2)
     *
     *       which is a comparison between two exact integers. Where it is true, the corpus is flat and takes the short
     *       circuiting engine (0). Where it fails, the free order engine (1) runs. The rule takes no logarithm, and
     *       approximates no power of two by a series. The threshold is the exact rational 85/100, and not the nearest
     *       double to 0.85.
     * @note Both sides outgrow 64 bits on a corpus of any size, because total^2 passes 2^64 at a four gigabyte corpus.
     *       The sum of squares also runs over 256 terms. The code carries both in OriorExactInteger for that reason,
     *       the fixed width limb form that the rest of the engine already measures in.
     * @note A sweep set the threshold, and orior_core.c records the sweep against the constant. The cleared
     *       denominators do not reopen that: 85/100 is the same value that the sweep scored, carried exactly instead of
     *       rounded.
     */
    int orior_steer_prefers_free(const OriorFieldCensus *census);

#ifdef __cplusplus
}
#endif

#endif
