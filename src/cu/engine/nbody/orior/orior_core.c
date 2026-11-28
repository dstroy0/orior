// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_core.c: the sift: naive, in order, free, choice and run
#include "memoria_operor/memoria_operor.h"
#include "orior_internal.h"

#if ORIOR_COUNT_READS

uint64_t orior_probes = 0u;
uint64_t orior_verifications = 0u;

void orior_counters_reset(void)
{
    orior_probes = 0u;
    orior_verifications = 0u;
}

/* One corpus byte read by an anchor probe. */
#define ORIOR_PROBED() (orior_probes += 1u)
/* A fixed number of probe reads, for the engine that issues all of them whatever any one returns. */
#define ORIOR_PROBED_N(count_) (orior_probes += (uint64_t)(count_))
/* One exact compare, which reads at max needle_len bytes and usually far fewer, since a memcmp
 * stops at the first difference. The count is therefore a bound on the verification reads and not
 * a measurement of them, and the bench reports it as a bound. */
#define ORIOR_VERIFIED() (orior_verifications += 1u)

#else

#define ORIOR_PROBED() ((void)0)
#define ORIOR_PROBED_N(count_) ((void)(count_))
#define ORIOR_VERIFIED() ((void)0)

#endif

/**
 * @brief Chooses anchor offsets, one drawn inside each evenly sized cell of the needle.
 *
 * @param[out] offsets    Where the call writes the chosen offsets [BORROWS].
 * @param[in]  wanted     How many to choose, at least one.
 * @param[in]  needle_len Length of the needle that they index.
 * @note One draw for each cell keeps the spread, and gives the anchor set no period of its own.
 *       An even comb shares a period with what the domain carries, the failure that this avoids.
 * @warning A needle of length zero has no offset in range to choose. The clamp below computes
 *          `needle_len - 1u`, and on size_t that wraps to SIZE_MAX, and does not saturate. Every
 *          offset then lands far outside both the corpus and the needle. The engines answer length
 *          zero before they reach here. This also bounds it at the declaration, because the library
 *          exports the engines. The clamp reads just as the guard that would prevent it.
 */
void choose_offsets(size_t *offsets, size_t wanted, size_t needle_len)
{
    if (needle_len == 0u)
    {
        for (size_t at = 0u; at < wanted; at += 1u)
        {
            offsets[at] = 0u;
        }
        return;
    }

    const size_t cell = needle_len / wanted;

    for (size_t at = 0u; at < wanted; at += 1u)
    {
        const size_t inside = (cell > 1u) ? ((at * 7u) % cell) : 0u;
        offsets[at] = (at * cell) + inside;
        if (offsets[at] >= needle_len)
        {
            offsets[at] = needle_len - 1u;
        }
    }
}

size_t orior_naive(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len)
{
    size_t found = 0u;

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        ORIOR_VERIFIED();
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = corpus + at, .other = needle, .bytes = needle_len) == 0)
        {
            found += 1u;
        }
    }
    return found;
}

/**
 * @brief The in order engine with the anchor count supplied.
 *
 * @param[in] corpus     Bytes to search [BORROWS].
 * @param[in] corpus_len How many.
 * @param[in] needle     Bytes to find [BORROWS].
 * @param[in] needle_len How many.
 * @param[in] anchors    How many anchors to place, at least one and at max ORIOR_ANCHORS.
 * @return               How many alignments match exactly.
 * @note Only this engine takes a count. The free order engine gains because it writes its
 *       comparisons out, and they fold into one value with one branch behind them. A loop over a
 *       runtime count gives that back. A corpus whose count needs a decrease is a coherent one, and
 *       a coherent corpus dispatches here in all cases.
 * @note A needle of length zero occurs at every alignment, and the naive engine returns that
 *       count. The call passes that case to it, and does not answer it a second way here. No
 *       anchor fits in a needle with no bytes, and a bound on the offsets is not enough on its
 *       own. At length zero the alignment loop runs one further than the corpus. The last
 *       alignment reads one past its end, and the anchor reads element zero of a needle that has
 *       none. Both are reads outside memory that the caller owns.
 */
static size_t sift_inorder_n(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                             size_t anchors)
{
    if (needle_len == 0u)
    {
        return orior_naive(corpus, corpus_len, needle, needle_len);
    }

    size_t offsets[ORIOR_ANCHORS];
    size_t found = 0u;

    choose_offsets(offsets, anchors, needle_len);

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        size_t index = 0u;
        while (index < anchors)
        {
            ORIOR_PROBED();
            if (corpus[at + offsets[index]] != needle[offsets[index]])
            {
                break;
            }
            index += 1u;
        }
        if (index == anchors)
        {
            ORIOR_VERIFIED();
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = corpus + at, .other = needle, .bytes = needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

size_t orior_inorder(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len)
{
    return sift_inorder_n(corpus, corpus_len, needle, needle_len, ORIOR_ANCHORS);
}

size_t orior_free(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len)
{
    // Length zero goes to the naive engine, for the reason on sift_inorder_n. This engine carries
    // the test on its own, because the library exports it, and a caller reaches it with no pass
    // through the dispatcher. It is also the engine where a bound on the offsets alone is not
    // enough. The read below occurs before the alignment loop. A zero offset still takes element
    // zero of a needle that has none.
    if (needle_len == 0u)
    {
        return orior_naive(corpus, corpus_len, needle, needle_len);
    }

    size_t offsets[ORIOR_ANCHORS];
    uint8_t wanted[ORIOR_ANCHORS];
    size_t found = 0u;

    choose_offsets(offsets, ORIOR_ANCHORS, needle_len);
    for (size_t index = 0u; index < ORIOR_ANCHORS; index += 1u)
    {
        wanted[index] = needle[offsets[index]];
    }

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        // No short circuit. Four loads issue together, the comparisons fold into one value, and the
        // branch occurs once. This is the arrangement of dependency depth two.
        ORIOR_PROBED_N(ORIOR_ANCHORS);
        const unsigned agree =
            (unsigned)(corpus[at + offsets[0]] == wanted[0]) & (unsigned)(corpus[at + offsets[1]] == wanted[1]) &
            (unsigned)(corpus[at + offsets[2]] == wanted[2]) & (unsigned)(corpus[at + offsets[3]] == wanted[3]);
        if (agree != 0u)
        {
            ORIOR_VERIFIED();
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = corpus + at, .other = needle, .bytes = needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

OriorEngine orior_choose(const OriorPlan *plan)
{
    // No plan is no statistics, and the engine that needs none is the naive one. THIS FUNCTION
    // TESTS BOTH POINTERS HERE, and gives neither test to the callee. orior_steer_prefers_free
    // returns 0 on a null census. A delegate would work, but this function would then read as if
    // it dereferences an unchecked pointer. The next reader could not tell that it is safe without
    // a look at another file.
    if ((plan == NULL) || (plan->census == NULL))
    {
        return orior_naive;
    }

    // A flat corpus refutes almost every alignment on the first probe. A short circuit reads one
    // byte where the free order engine reads four. A structured one refutes on the first probe
    // often enough to make the trip count of the loop vary. A trip count that varies costs a
    // mispredicted branch for each alignment. The branchless engine exists to prevent that.
    //
    // THE RULE IS AN EXACT INTEGER COMPARISON. It tests whether the effective alphabet `2^H2`
    // reaches 85/100 of the symbols used. That is how close it must sit to the symbols in use
    // before a corpus counts as memoryless. The form of `2^H2` as `total^2` over the sum of
    // squared counts clears both denominators, and leaves
    //
    //     100 * total^2  >=  85 * distinct * sum(count^2)
    //
    // which is a comparison between two exact integers, carried in the limb form where it grows
    // past 64 bits. The rule takes no logarithm and evaluates no series, and the threshold is the
    // exact rational 85/100. Every threshold from 0.34 to 0.96 scores the same on the corpora. They
    // read 0.96, 0.33 and 1.00, with nothing between. orior_steer_prefers_free holds the
    // comparison, and the sweep in test_steer grades it.
    return orior_steer_prefers_free(plan->census) ? orior_free : orior_inorder;
}

size_t orior_anchors_for(const OriorPlan *plan)
{
    // No plan is no known period, and an unknown period is the case that the full anchor set is
    // for. A return of the max reads more than a known period would need, and can lose nothing.
    if (plan == NULL)
    {
        return ORIOR_ANCHORS;
    }

    // Every anchor after the first tests the congruence the first one already tested. The reads
    // they perform are their only contribution.
    if (plan->period != 0u)
    {
        return 1u;
    }
    return ORIOR_ANCHORS;
}

size_t orior_run(const OriorPlan *plan, const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                 size_t needle_len)
{
    const OriorEngine chosen = orior_choose(plan);

    // The call stores and dispatches the choice, and does not test it again, because the choice is
    // three ways and not two. A null plan that reached the tail would run the in order engine with
    // the full anchor set. The guard in orior_choose prevents that outcome.
    if (chosen == orior_naive)
    {
        return orior_naive(corpus, corpus_len, needle, needle_len);
    }
    if (chosen == orior_free)
    {
        return orior_free(corpus, corpus_len, needle, needle_len);
    }
    return sift_inorder_n(corpus, corpus_len, needle, needle_len, orior_anchors_for(plan));
}

const char *orior_engine_name(OriorEngine engine)
{
    if (engine == orior_naive)
    {
        return "naive";
    }
    if (engine == orior_inorder)
    {
        return "orior_inorder";
    }
    if (engine == orior_free)
    {
        return "orior_free";
    }
    return "unknown";
}
