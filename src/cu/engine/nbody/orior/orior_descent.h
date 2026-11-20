// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_descent.h: descent, the recursive plan, probes and counting (orior.h includes the parts in order)
#ifndef ORIOR_DESCENT_H
#define ORIOR_DESCENT_H

#include "orior_field.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Everything that a descent reads, as one argument.
     *
     * ONE STRUCT FOR THE WHOLE FAMILY. The descent that reorders and the descent that spawns test the same question of
     * the same field. They differ only in the source of their candidates. Nine or ten positional parameters, four of
     * them `size_t` in a row, make a call that a reader cannot check and a caller can transpose silently.
     *
     * @note AN OMITTED MEMBER IS ZERO, AND THAT IS PART OF THE CONTRACT:
     *         - `sample_stride` of zero reads as one.
     *         - `force_full_depth` of zero honors the destroy rule.
     *         - `any` of zero takes the byte path.
     *         - `resume` of zero resets the survivors to all located.
     *
     *       A caller that omits all four gets the full sweep, the destroy rule, bytes, and a fresh survivor set.
     *       Most callers want that.
     *
     * ANY SYMBOL TYPE, THROUGH `any`. Set it, and the engine reads the field only through an equality oracle, and
     * never touches `corpus` or `needle`.
     *
     * That is not a convenience wrapper over the byte path. It is the path that the theory describes, and the byte
     * members are the specialization. Soundness uses equality alone, and reads neither order, dimension nor alphabet.
     * An engine that demands a `uint8_t *` is narrower than its own proof. The byte path stays, because it is
     * faster, and because each current caller passes bytes.
     *
     * WHAT `force_full_depth` MAKES TESTABLE. The descent normally stops at a level whose best candidate leaves the
     * truthy population unchanged, and destroys each level below it. The claim is that to stop and to continue are
     * equivalent. This member lets a caller check the claim, and not just believe it.
     *
     * Set it, and THE COUNT FROM THE RESULTING PROBE SET MUST BE IDENTICAL, while the placed probe count CAN be
     * larger. A test that compares the two runs gets two separable failures:
     *   - if the counts differ, the necessary-condition guarantee broke.
     *   - if the probes placed before the stop point differ, the induction broke.
     *
     * One member, with two meanings that a reader can tell apart.
     *
     * @note A member that an omitted initializer leaves zero does this work. A second entry that shares one descent
     *       is the drift that this structure exists to remove.
     * @warning The call BORROWS each pointer here for its duration. The call writes `survivors`, and only
     *          reads `corpus` and `needle`.
     */
    typedef struct
    {
        size_t *offsets;         /**< Where the call writes the chosen offsets [BORROWS]. */
        size_t count;            /**< Offsets to place. At max ORIOR_STEER_ANCHORS. */
        const uint8_t *corpus;   /**< Bytes that the search runs over [BORROWS]. Unread where `any` is non-null. */
        size_t corpus_len;       /**< How many. Unread where `any` is non-null. */
        const uint8_t *needle;   /**< Bytes to find [BORROWS]. Unread where `any` is non-null. */
        size_t needle_len;       /**< How many. Unread where `any` is non-null. */
        uint8_t *survivors;      /**< One byte per alignment, written during the call [BORROWS]. THE
                                  *   OUTPUT OF THE DESCENT, AND NOT A TEMPORARY: it records, per alignment,
                                  *   whether the probes left that alignment located. A caller that wants
                                  *   only the depth can discard it. A caller that wants to know WHICH
                                  *   alignments survived has no other way to learn it. */
        size_t survivors_length; /**< How many. Must reach the alignment count, or the call errors. */
        size_t sample_stride;    /**< Plan on each Nth alignment. Zero reads as one. */
        int force_full_depth;    /**< Non-zero descends each level, and ignores the destroy rule. */
        const OriorField *any;  /**< A field of any symbol type [BORROWS]. Null takes the byte path. */
        int resume;             /**< Non-zero starts the descent from the survivors already in the buffer,
                                 *   and does not reset them to all located. A caller uses it to compose a
                                 *   recursive spawn: descend, then descend again over the survivors that
                                 *   the last descent left. Each child reads only what its parent left
                                 *   located. Zero, the default, resets the buffer, and each current caller
                                 *   gets that. The engine does not check that the set it receives is a valid
                                 *   superset, and leaves that to the caller. The engine verifies a lone
                                 *   survivor against the conditions not yet tested before it finds it. */
    } OriorSteerDescent;

/**
 * @brief Calls an entry with its arguments built at the call site.
 *
 * @param entry_ The entry to call.
 * @param type_  Its argument structure.
 * @note The literal has automatic storage, and lives for the whole call. Each member that the caller does
 *       not name is zero, the contract that each structure above states. The macro names `__VA_ARGS__`
 *       once. The code evaluates an argument with a side effect once.
 */
#define ORIOR_STEER_CALL(entry_, type_, ...) entry_(&(type_){__VA_ARGS__})

    /**
     * @brief Orders the anchors by conditional pruning, one level per anchor, and reports the depth.
     *
     * @warning THIS REORDERS OFFSETS THAT THE CALLER ALREADY PLACED. IT DOES NOT CHOOSE THEM. The call reads
     *          `offsets` on the way in. A caller can leave the array uninitialized, and expect the descent to fill
     *          it. That caller gets the old contents of that memory ranked, and gets it silently.
     *          orior_steer_spawn_coarms is the entry that chooses the positions itself.
     * @warning `count` ABOVE ORIOR_STEER_ANCHORS RETURNS ZERO AND REPORTS NOTHING. The zero is the only report: a
     *          null pointer and a zero needle length also return zero. A caller that reads only the return value
     *          cannot tell which guard failed. Check the bound before the call, because the call does not tell you.
     *
     * @param[in,out] args          The descent [BORROWS], each member as OriorSteerDescent gives it:
     *                              `offsets` reordered in place into evaluation order.
     * @return                      Levels descended, which is AT MAX `count`, and fewer when the destroy rule
     *                              ends the descent early. `count` on a valid call is the ceiling that the
     *                              depth cannot exceed. Read the return to learn the depth reached, and see the
     *                              note below on `force_full_depth`.
     *
     * WHY RECURSION GAINS ANYTHING OVER ONE PASS. orior_steer_probe_order ranks the anchors once, by the MARGINAL
     * rarity of each symbol in the full field. That is the right first question, and the wrong second one.
     *
     * Once the first probe rejects almost all, the alignments still located are no longer a sample of the field.
     * They are the subset that agreed with one specific symbol. Within that subset, the other anchors have a
     * different power to prune than over the field. A rank of the second anchor by its marginal rarity ignores what
     * the first one just showed.
     *
     * This ranks each level against the alignments that survived the levels above it: the CONDITIONAL
     * distribution, and not the marginal one. It also measures survivors directly, and does not infer them from
     * symbol frequency. It accounts for the correlation between positions, and does not assume it away.
     *
     * IT CANNOT FAIL TO TERMINATE, AND NOT BECAUSE ANYBODY CHECKED. The halting problem is about a decision on
     * termination for an ARBITRARY program. This recursion is not arbitrary:
     *
     *   - each level places exactly one anchor, and the descent never reconsiders a placed anchor.
     *   - the unplaced set therefore shrinks by exactly one each level, and never grows.
     *   - the depth is AT MAX `count`, which ORIOR_STEER_ANCHORS bounds. That is a compile-time constant, and the
     *     loop cannot run past it, whatever the corpus contains.
     *
     * A bound from ABOVE therefore holds on the depth before the program runs, and that bound ends it. The unplaced
     * set strictly shrinks under a constant ceiling, the way a `for` loop over a fixed array ends. No runtime guard,
     * iteration cap or watchdog is present, because a bound enforced at compile time does not need one.
     *
     * THE DEPTH CAN CHANGE, THOUGH. Corpus content decides the depth of the descent as well as its choice at a
     * level, unless the caller sets `force_full_depth`. The destroy test reads a survivor count off the corpus, and
     * breaks. On the default path, where the member is zero, the field often ends the descent early. Depth is a
     * truthy and falsy steer bounded above by a constant. The return value exists for a caller to read the depth
     * reached, and not to assume `count`.
     *
     * @note THE PLANNER CAN BE WRONG. Order cannot change which alignments survive. An alignment survives only when
     *       each anchor agrees, and a conjunction is independent of order. A planner that samples, guesses badly, or
     *       is outright defective therefore costs speed, and cannot cost correctness. `sample_stride` is safe for
     *       the same reason: a plan on a subset risks a worse order, and never a wrong count.
     * @note Does nothing and returns 0 where any pointer is null, where `count` is zero, or where `needle_len` is
     *       zero. A zero length needle has no symbol to rank.
     */
    size_t orior_steer_plan_recursive(const OriorSteerDescent *args);

    /**
     * @brief Spawns coarms at the positions that prune most, one per level, and places them in order.
     *
     * @param[in,out] args          The descent [BORROWS], each member as OriorSteerDescent gives it:
     *                              `offsets` written in evaluation order, and `count` the coarms
     *                              wanted, written `wanted` below.
     * @return                      Coarms placed, which is AT MAX `wanted`, and fewer when the destroy rule
     *                              ends the descent early. `wanted` on a valid call is the ceiling that the
     *                              count cannot exceed. Read the return to learn how many the call placed,
     *                              and size any read of `offsets` by the return itself.
     *
     * THE SPAWN. orior_steer_plan_recursive takes anchors that somebody else placed, and decides the order to test
     * them in. This decides WHERE THEY GO.
     * At each level, it tests each position in the needle: how many of the alignments that now survive stay located
     * if a coarm goes there. It puts one at the position that leaves fewest.
     * The arm spawns at the place that the field shows is worth a read. It does not spawn at a place that a spread
     * rule chose before a look at the field.
     *
     * That is the same steer that the rest of this file applies, moved from the order to the placement. The spread
     * rule above answers "where, with no knowledge". This answers "where, given the corpus and what the placed
     * coarms already ruled out".
     *
     * THIS DESCENT IS GREEDY COVERAGE MAXIMIZATION, AND CARRIES ITS GUARANTEE. Each probe rejects a definite set of
     * alignments, and the alignments that a probe SET rejects are the union of those sets.
     * A union of sets is monotone and submodular, because an alignment already rejected adds nothing when rejected
     * again. The candidate that leaves fewest survivors has the largest marginal gain on that union.
     *
     * Nemhauser, Wolsey and Fisher (1978) give the bound. Greedy maximization of a monotone submodular function under
     * a cardinality constraint reaches at least `1 - 1/e` of the best set of the same size. The probes placed here
     * reject at least about 63 percent of what the optimal `wanted` probes reject.
     *
     * @warning THE GUARANTEE IS ON ALIGNMENTS REJECTED, AND NOT ON READS. An early reject of an alignment saves the
     *          reads that a later probe spends on it. Two probe sets that cover the same alignments can cost
     *          different numbers of reads. It assumes exact scores for the marginal gains, which hold only at
     *          `sample_stride` of one. Above one, the score comes from a sample, the oracle is approximate, and
     *          the ratio no longer holds as stated.
     *
     * TERMINATION IS A BOUND FROM ABOVE, AND NOT A FIXED DEPTH. One coarm per level, the descent never reconsiders
     * a placed position, and the depth is AT MAX `wanted`, which the guard bounds at or under ORIOR_STEER_ANCHORS.
     * The loop cannot run longer than that, whatever the corpus contains, and it therefore ends.
     *
     * IT CAN RUN SHORTER, AND CORPUS CONTENT DECIDES WHEN. The destroy test compares the population that survives
     * for the best candidate against the current one, and that count comes off the corpus. Where nothing prunes, the
     * descent breaks early. `force_full_depth` exists to override that, and an omitted member is zero. On the
     * DEFAULT path, the field ends the descent.
     *
     * bench_sigma measures it: with `wanted` fixed at 4 on each row, `placed` comes back 2 at an alphabet of 2^8,
     * and 1 from 2^16 up. A larger alphabet lets the first probe cut far enough that a second gains nothing.
     *
     * Depth is a truthy and falsy steer like all else here, bounded above by a constant and free to come in under
     * it.
     *
     * The return value is there for a caller to read the depth reached, and not to assume `wanted`. That matters
     * where the two can differ.
     *
     * @note FAILS CLOSED ON THE SURVIVOR BUFFER. Returns 0 without a write to `offsets` where `survivors_length`
     *       does not reach the alignment count. The kernel allocates nothing. The buffer belongs to the caller, and
     *       a buffer that is too small is an error, with no workaround. Size it at `corpus_len - needle_len + 1`.
     * @note A planner is free to be wrong here, for the same reason that it is free to be wrong anywhere else in
     *       this file. Placement and order change which probe rejects first, and never which alignments survive.
     *       The verification is a full compare either way.
     * @warning Costs `wanted * needle_len * alignments / sample_stride` byte comparisons to plan. On a long needle,
     *          that exceeds the scan that it plans for. `sample_stride` is the control, and test_steer measures
     *          where the trade turns over, and does not assert a default.
     */
    size_t orior_steer_spawn_coarms(const OriorSteerDescent *args);

    /**
     * @brief One probe placed on the needle. An arm is a point, and an eye is a line.
     *
     * ONE SHAPE SERVES BOTH, the SAME STATEMENT THAT arm-records.md MAKES ABOUT READINGS. An arm is a region integral,
     * and an eye is a line integral. The difference between them lives in the shape of the support, and not in the
     * arithmetic applied to it. Here, that means an arm is an eye whose length is one, and the same test walks both.
     *
     * @note `step` is unread at `length` one. It makes a longer probe a LINE through the needle, and not a run of
     *       adjacent bytes. A step that shares a period with the needle reads the same residue again and again, and
     *       prunes badly. That is a real failure mode, and it is why the sweep measures steps, and does not assume
     *       one.
     * @note Each position that the probe touches must land inside the needle. The function orior_steer_probe_fits is
     *       the test, and the sweep applies it before it scores a shape.
     */
    typedef struct
    {
        size_t origin; /**< First position in the needle that this probe reads. */
        size_t step;   /**< Distance between successive positions. Unread where length is one. */
        size_t length; /**< Positions read. One is an arm, and more is an eye. */
    } OriorProbe;

    /**
     * @brief What the probe sweep reads, as one argument.
     *
     * @note Separate from OriorSteerDescent, because it writes probes and not offsets, and carries a length
     *       ceiling that the offset descents have no use for. One shared struct puts a member in it that half the
     *       callers must leave zero, and the other half must set.
     * @note Declared here and not beside OriorSteerDescent, because it holds an OriorProbe, declared just above. A
     *       structure cannot name a type that the compiler has not seen.
     * @note An omitted member is zero, as it is for OriorSteerDescent. `sample_stride` of zero reads as one.
     *       `max_length` of zero is an error, and does not read as one. A sweep that considers no probe shape is a
     *       caller error, and not a default worth an invention.
     */
    typedef struct
    {
        OriorProbe *probes;     /**< Where the call writes the chosen probes [BORROWS]. */
        size_t count;            /**< Probes to place. At max ORIOR_STEER_ANCHORS. */
        const uint8_t *corpus;   /**< Bytes that the search runs over [BORROWS]. */
        size_t corpus_len;       /**< How many. */
        const uint8_t *needle;   /**< Bytes to find [BORROWS]. */
        size_t needle_len;       /**< How many. */
        size_t max_length;       /**< Longest eye to consider. One restricts the sweep to arms. */
        uint8_t *survivors;      /**< One byte per alignment, written during the call [BORROWS]. THE
                                  *   OUTPUT OF THE SWEEP, AND NOT A TEMPORARY, exactly as for
                                  *   OriorSteerDescent: it records which alignments the probes left
                                  *   located. */
        size_t survivors_length; /**< How many. Must reach the alignment count, or the call errors. */
        size_t sample_stride;    /**< Plan on each Nth alignment. Zero reads as one. */
    } OriorSteerSweep;

    /**
     * @brief Whether every position a probe reads lands inside the needle.
     *
     * @param[in] probe      Probe to test [BORROWS].
     * @param[in] needle_len Length it must fit inside.
     * @return               1 where it fits, 0 otherwise.
     * @note Computed without the last position formed as a sum. A step and length that overflow size_t are an
     *       error, and do not wrap into a position that looks valid.
     */
    int orior_steer_probe_fits(const OriorProbe *probe, size_t needle_len);

    /**
     * @brief Spawns probes anywhere on the needle, sweeping shapes, and orders them by pruning.
     *
     * @param[in,out] args          The sweep [BORROWS], each member as OriorSteerSweep gives it:
     *                              `probes` written in evaluation order, and `count` the probes
     *                              wanted, written `wanted` below.
     * @return                      Probes placed.
     *
     * THE SWEEP TOUCHES ALL THAT IT CAN REACH. At each level, it considers each origin in the needle, each step that
     * keeps the probe inside it, and each length up to `max_length`. It scores each shape by how many alignments
     * that are truthy now stay located, and spawns the one that leaves fewest.
     * The placement inherits nothing from a spread rule, and the code assumes nothing about the shape. A point probe
     * wins where a point probe is best, and a line wins where a line is.
     *
     * AN EYE IS NOT FREE, AND THE SWEEP KNOWS IT. A probe of length L reads up to L bytes per alignment, where an
     * arm reads one. An eye must prune more than L times as hard to be worth a spawn.
     *
     * The score here is survivors, which does not carry that cost. The caller that compares an eye against an arm
     * must compare READS, and not survivors. test_steer does exactly that and reports both. The guide recommends a
     * measurement, and not the longest eye.
     *
     * TERMINATION, the same, and for the same reason. One probe per level, `wanted` levels, bounded by
     * ORIOR_STEER_ANCHORS at compile time. The sweep inside a level is three nested bounded loops over needle_len,
     * needle_len and max_length. Nothing in it is data dependent in its EXTENT.
     *
     * @note Each shape that the sweep can spawn leaves the count unchanged. The whole sweep moves inside the null
     *       group, and can be as wrong as it likes without a cost to an answer.
     * @note Fails closed on the survivor buffer, exactly as orior_steer_spawn_coarms does.
     * @warning The sweep is `wanted * needle_len^2 * max_length^2 * alignments / sample_stride` byte comparisons at
     *          worst. One factor of max_length counts the lengths enumerated. The second comes from the score. A
     *          candidate of length L costs up to L comparisons, and the sum of L from 1 to max_length averages
     *          about max_length/2.
     * @note orior_steer_probe_fits rejects shapes that do not fit, and the real count sits below this figure. It is
     *       still far more than the scan that it plans, on any but a tiny needle. It is a planner for a search run
     *       many times against one needle, and not for a single shot. `sample_stride` makes it affordable.
     */
    size_t orior_steer_sweep_probes(const OriorSteerSweep *args);

    /** @brief Corpus bytes read by an anchor probe since the last reset. */
    extern uint64_t orior_steer_probes;

    /** @brief Sets the probe counter to zero. */
    void orior_steer_probes_reset(void);

    /**
     * @brief Counts occurrences, and steers the probe order off the corpus or leaves it alone.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @param[in] steered    1 to order the probes by rarity, 0 to leave the spatial order.
     * @return               How many alignments match exactly.
     *
     * @note ONE KERNEL AND ONE FLAG. The flag is the only difference between the two routes. Same offsets, same
     *       probe loop, same verification. `steered` decides only the ORDER of the probes. A comparison between two
     *       separate implementations measures the implementations. This measures the order.
     * @note The count is identical for both values of `steered` by construction, and no run is necessary to show
     *       it. An alignment survives only when each anchor agrees. A conjunction does not depend on the order of
     *       its terms, and a full memcmp verifies the survivor either way. The bench grades it at a residual of
     *       exactly zero for that reason, and not against a tolerance.
     * @note `orior_steer_probes` counts the corpus bytes that the probes read, which is where the order pays. Reset
     *       it before a run, and read it after.
     * @warning Delegates to a full compare at `needle_len` zero, and does not probe, because there is no symbol to
     *          probe and no offset that indexes one. That matches the reference engine, which reports an empty
     *          needle as present at each alignment.
     */
    size_t orior_steer_count(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                              int steered);

    /**
     * @brief Counts occurrences with a probe set that the caller supplies, in the order given.
     *
     * @param[in] corpus     Bytes to search [BORROWS].
     * @param[in] corpus_len How many.
     * @param[in] needle     Bytes to find [BORROWS].
     * @param[in] needle_len How many.
     * @param[in] probes     Probes in evaluation order [BORROWS].
     * @param[in] count      How many probes. Zero sends every alignment to the full compare.
     * @return               How many alignments match exactly.
     *
     * @note THE ENTRY THAT A TEST NEEDS AND A CALLER RARELY DOES. All else here chooses its own probes. A steer engine
     *       exists for that, and it also makes the guarantee hard to attack from outside. This takes the
     *       probe set as an argument. A caller can:
     *         - hand over a permutation of one set, and check that the count stays the same.
     *         - hand over a probe built from the census and not the needle, and watch the count break.
     *         - hand over none at all.
     * @note The empty probe set is the identity. Each alignment reaches the full compare, the answer is exactly
     *       right, and the cost is maximal. That is the cheapest total check of the full guarantee, and it is why
     *       the call accepts a `count` of zero.
     * @note `orior_steer_probes` counts the corpus bytes that the probes read, as it does for orior_steer_count.
     *       Reset it before a run, and read it after.
     */
    size_t orior_steer_count_with_probes(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                                          size_t needle_len, const OriorProbe *probes, size_t count);

#ifdef __cplusplus
}
#endif

#endif
