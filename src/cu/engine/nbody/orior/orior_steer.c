// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_steer.c: the census and the steering probe order
#include "memoria_operor/memoria_operor.h"
#include "orior_internal.h"

/* ---- the steering, folded in ---- */

void orior_field_census(const uint8_t *corpus, size_t corpus_len, OriorFieldCensus *census)
{
    if (census == NULL)
    {
        return;
    }

    EMBED_CALL(memor.set, MemoriaCfg, .dst = census, .bytes = sizeof(*census), .val = 0);
    if ((corpus == NULL) || (corpus_len == 0u))
    {
        return;
    }

    for (size_t at = 0u; at < corpus_len; at += 1u)
    {
        census->occurrences[corpus[at]] += 1u;
    }
    census->total = (uint64_t)corpus_len;

    for (unsigned int symbol = 0u; symbol < ORIOR_STEER_SYMBOLS; symbol += 1u)
    {
        if (census->occurrences[symbol] != 0u)
        {
            census->distinct += 1u;
        }
    }
}

uint64_t orior_steer_magnitude(const OriorFieldCensus *census, uint8_t symbol)
{
    if (census == NULL)
    {
        return 0u;
    }
    return census->total - census->occurrences[symbol];
}

void orior_steer_probe_order(size_t *offsets, size_t count, const OriorFieldCensus *census, const uint8_t *needle,
                             size_t needle_len)
{
    if ((offsets == NULL) || (census == NULL) || (needle == NULL) || (count == 0u) || (needle_len == 0u) ||
        (census->total == 0u))
    {
        return;
    }

    // Insertion sort, descending by magnitude, and STABLE. The strict greater-than in the shift
    // test keeps it stable: an equal magnitude does not move the entry that the sort already
    // placed. Anchors that test equally rare symbols keep the spatial spread that choose_offsets
    // gave them.
    for (size_t placed = 1u; placed < count; placed += 1u)
    {
        const size_t moving_offset = offsets[placed];
        if (moving_offset >= needle_len)
        {
            continue;
        }
        const uint64_t moving = orior_steer_magnitude(census, needle[moving_offset]);

        size_t at = placed;
        while (at > 0u)
        {
            const size_t settled_offset = offsets[at - 1u];
            const uint64_t settled =
                (settled_offset < needle_len) ? orior_steer_magnitude(census, needle[settled_offset]) : 0u;
            if (settled > moving)
            {
                break;
            }
            if (settled == moving)
            {
                break;
            }
            offsets[at] = offsets[at - 1u];
            at -= 1u;
        }
        offsets[at] = moving_offset;
    }
}

/**
 * @brief Loads a 64 bit value into the fixed width limb form.
 *
 * @param[out] value Where the call writes the limbs [BORROWS].
 * @param[in]  from  The value to carry.
 * @note Base 2^32, least significant limb first, the layout that exact_integer.h declares.
 *       A direct write of the two limbs reads that declaration, and does not reach around it.
 *       There is no decimal text here to route through orior_exact_from_decimal. A conversion
 *       of a counter to text, to parse it back, would be slower and just as exact.
 * @note limb[1] exists because the assert at the top of this file limits the engine to 8 limbs or
 *       more.
 */
static void steer_exact_from_u64(OriorExactInteger *value, uint64_t from)
{
    orior_exact_zero(value);
    if (from == 0u)
    {
        return;
    }
    value->limb[0] = (uint32_t)(from & 0xFFFFFFFFu);
    value->limb[1] = (uint32_t)(from >> 32);
    value->sign = 1;
}

int orior_steer_prefers_free(const OriorFieldCensus *census)
{
    if ((census == NULL) || (census->total == 0u) || (census->distinct == 0u))
    {
        // An empty field distinguishes nothing. It takes the short circuiting engine, the
        // cheaper of the two on a field with no structure to exploit.
        return 0;
    }

    // Three integers, each built in place, since every exact operation accepts its result aliasing
    // an input. Six would come to 768 KiB at 32768 limbs, and with the multiply's accumulator that
    // passes the 1 MiB stack the MSVC linker gives a main thread.
    OriorExactInteger left;
    OriorExactInteger right;
    OriorExactInteger term;

    // left = 100 * total^2
    steer_exact_from_u64(&left, census->total);
    if (orior_exact_multiply(&left, &left, &left) != ORIOR_EXACT_OK)
    {
        return 0;
    }
    steer_exact_from_u64(&term, 100u);
    if (orior_exact_multiply(&left, &term, &left) != ORIOR_EXACT_OK)
    {
        return 0;
    }

    // right = sum over symbols of count^2, accumulated in the limb form and not in a 64 bit
    // counter. A single count squares to at max `total^2`, which already passes `2^64` on a four
    // gigabyte corpus. The call sums 256 of them on top of that.
    orior_exact_zero(&right);
    for (unsigned int symbol = 0u; symbol < ORIOR_STEER_SYMBOLS; symbol += 1u)
    {
        if (census->occurrences[symbol] == 0u)
        {
            continue;
        }
        steer_exact_from_u64(&term, census->occurrences[symbol]);
        if (orior_exact_multiply(&term, &term, &term) != ORIOR_EXACT_OK)
        {
            return 0;
        }
        if (orior_exact_add(&right, &term, &right) != ORIOR_EXACT_OK)
        {
            return 0;
        }
    }

    // right = 85 * distinct * right
    steer_exact_from_u64(&term, 85u);
    if (orior_exact_multiply(&right, &term, &right) != ORIOR_EXACT_OK)
    {
        return 0;
    }
    steer_exact_from_u64(&term, (uint64_t)census->distinct);
    if (orior_exact_multiply(&right, &term, &right) != ORIOR_EXACT_OK)
    {
        return 0;
    }

    // The rule reads "flat corpora take the short circuiting engine". The free order one is
    // therefore the branch taken when the effective alphabet does NOT reach the threshold.
    return (orior_exact_compare(&left, &right) < 0) ? 1 : 0;
}

/* ------------------------------------------------------------------------------------------------
 * The truthy and falsy steer. The signal is the survivor vector, not the symbol histogram.
 *
 * EACH PERMUTATION OF THE ANCHORS IS A NULL, AND EACH NULL IS A STEER. An alignment survives only
 * when every anchor agrees, and a conjunction does not depend on the order of its terms. Every
 * order of a given anchor set returns the same count. Thus the orders form a group of moves that
 * CANNOT change the answer, and this tree calls such a group a null. A steer is the choice of the
 * element of that group to apply.
 *
 * The safety argument for all below rests on this alone, and it is structural, not defensive. A
 * planner that samples badly, ranks wrongly, or is broken still lands on some element of the null
 * group. Every element gives the same count. The planner moves inside the null, and the null
 * has one value. Thus the planner cannot spend correctness, and speed is the only currency that
 * it uses.
 * ---------------------------------------------------------------------------------------------- */

/**
 * @brief How many truthy alignments stay truthy when the call tests `offset`.
 *
 * @param[in] alive  One flag for each alignment, not zero for truthy [BORROWS].
 * @param[in] stride Sample every Nth alignment. The rank is a comparison between candidates.
 *                   A consistent sample ranks them consistently, with no read of them all.
 * @return           Count of survivors, in the sampled population.
 */
size_t steer_truthy_after(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                          const uint8_t *alive, size_t offset, size_t stride, const OriorField *any)
{
    // ANY SYMBOL TYPE TAKES THE SCALAR LOOP AND CANNOT TAKE A WIDE ONE. A vector scan compares
    // bytes against a broadcast byte, which is a statement about the representation. The oracle is
    // a statement about equality, and the engine never learns what it compares. There is nothing
    // to broadcast. This path is slower, and the theory describes it. The byte path below is the
    // special case that can go wide.
    if (any != NULL)
    {
        size_t located_any = 0u;

        for (size_t at = 0u; at < any->alignments; at += stride)
        {
            if (alive[at] == 0u)
            {
                continue;
            }
            if (any->same(any->field, at + offset, offset) != 0)
            {
                located_any += 1u;
            }
        }
        return located_any;
    }

    const size_t alignments = (corpus_len - needle_len) + 1u;
    const uint8_t wanted = needle[offset];

    // THE WIDEST ENGINE THAT THIS MACHINE CARRIES, QUERIED ONCE. The scan is where a planner spends
    // its time. An engine graded against the portable one but never called is a measurement, not a
    // speedup. The call resolves the engine once and caches it, because a query of the processor on
    // every candidate would cost more than the candidates do.
    //
    // Only at stride one. A sampled scan walks every Nth alignment, and the engines count every one.
    // A sampled sweep given to one would change what the scan counts. A sampled scan falls through
    // to the loop below, the same code that the portable engine runs.
    if (stride == 1u)
    {
        // orior_steer_best_engine resolves it, and holds the one dispatch that every new arm goes
        // into. A copy of the #if ladder here would be a second copy that a new arm could miss. A
        // scan can run the old widest arm while a wider one reports itself present. That is the
        // defect of an unused implementation, which the scan counters exist to catch.
        static const OriorSteerEngine *chosen = NULL;
        static int resolved = 0;
        if (resolved == 0)
        {
            chosen = orior_steer_best_engine();
            resolved = 1;
        }
        if (chosen != NULL)
        {
            return chosen->count(corpus, alignments, alive, wanted, offset);
        }
    }

    size_t located = 0u;
    for (size_t at = 0u; at < alignments; at += stride)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (corpus[at + offset] == wanted)
        {
            located += 1u;
        }
    }
    return located;
}

/**
 * @brief How many alignments are truthy now, in the sampled population.
 *
 * @note The call samples at the same stride that the candidate scores use. The comparison between
 *       "survivors after this probe" and "survivors before it" is between two counts of the same
 *       population. A mix of a full count with a sampled one would make every probe look as if it
 *       pruned.
 */
size_t steer_truthy_total(const uint8_t *alive, size_t alignments, size_t stride)
{
    size_t located = 0u;

    for (size_t at = 0u; at < alignments; at += stride)
    {
        if (alive[at] != 0u)
        {
            located += 1u;
        }
    }
    return located;
}

/**
 * @brief Turns falsy every alignment that disagrees at `offset`, over the whole population.
 *
 * @note The call works at stride one, even where the rank used a sample. The rank can be
 *       approximate, because it only picks between nulls. The survivor set cannot, because the next
 *       level ranks against it, and an approximate survivor set would compound.
 */
void steer_make_falsy(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                      uint8_t *alive, size_t offset, const OriorField *any)
{
    if (any != NULL)
    {
        for (size_t at = 0u; at < any->alignments; at += 1u)
        {
            if (alive[at] == 0u)
            {
                continue;
            }
            if (any->same(any->field, at + offset, offset) == 0)
            {
                alive[at] = 0u;
            }
        }
        return;
    }

    const size_t alignments = (corpus_len - needle_len) + 1u;
    const uint8_t wanted = needle[offset];

    for (size_t at = 0u; at < alignments; at += 1u)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (corpus[at + offset] != wanted)
        {
            alive[at] = 0u;
        }
    }
}
