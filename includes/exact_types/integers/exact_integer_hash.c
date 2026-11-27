
// exact_integer_hash.c: the hash and the agreement
#include "exact_integer_internal.h"

uint64_t orior_exact_hash(const OriorExactInteger *value)
{
    // FNV-1a over the limbs, with the sign folded in last so two magnitudes that differ only in
    // sign do not take one hash.
    uint64_t hash = 0xCBF29CE484222325u;
    for (size_t at = 0u; at < (size_t)ORIOR_EXACT_LIMBS; at++)
    {
        hash ^= (uint64_t)value->limb[at];
        hash *= 0x100000001B3u;
    }
    hash ^= (uint64_t)(uint32_t)value->sign;
    hash *= 0x100000001B3u;
    return hash;
}

/**
 * @brief The agreement count without an order, for when the allocation of the order fails.
 *
 * @param[in] equal     Whether two integers have the same value [BORROWS].
 * @param[in] positions Positions that carry values, in any order [BORROWS].
 * @param[in] values    The value located at each position [BORROWS].
 * @param[in] count     How many positions.
 * @param[in] lag       The offset to test [BORROWS].
 * @return              The count orior_exact_agreement_using returns with an order.
 * @note Quadratic in `count` and needs no memory. The count takes a position at its last entry only.
 *       It matches a displaced position to the last entry equal to it, which keeps the last value at a
 *       repeated position.
 */
static size_t agreement_without_order(int (*equal)(const OriorExactInteger *left, const OriorExactInteger *right),
                                      const OriorExactInteger *positions, const uint64_t *values, size_t count,
                                      const OriorExactInteger *lag)
{
    EXACT_VALUE(moved);
    if (!EXACT_SCRATCH_VALID(moved))
    {
        return 0u;
    }
    size_t agreed = 0u;
    for (size_t at = 0u; at < count; at++)
    {
        int repeated_later = 0;
        for (size_t later = at + 1u; later < count; later++)
        {
            if (equal(&positions[later], &positions[at]) != 0)
            {
                repeated_later = 1;
                break;
            }
        }
        if (repeated_later != 0)
        {
            continue;
        }

        if (orior_exact_add(&positions[at], lag, moved) != ORIOR_EXACT_OK)
        {
            continue;
        }
        size_t found = count;
        for (size_t other = 0u; other < count; other++)
        {
            if (equal(&positions[other], moved) != 0)
            {
                found = other;
            }
        }
        if ((found < count) && (values[found] == values[at]))
        {
            agreed++;
        }
    }
    EXACT_VALUE_RELEASE(moved);
    return agreed;
}

/** @brief A place of the order: the hash of a position, and its entry, the count where a later entry repeats it. */
typedef struct
{
    uint64_t hash;
    size_t at;
} ExactPlace;

/**
 * @brief The order of two places, by their hash.
 *
 * @param[in] left  A place [BORROWS].
 * @param[in] right A place [BORROWS].
 * @return          Below 0, 0 or above 0.
 */
static int exact_place_order(const void *left, const void *right)
{
    const ExactPlace *const one = (const ExactPlace *)left;
    const ExactPlace *const other = (const ExactPlace *)right;
    return (one->hash < other->hash) ? -1 : ((one->hash > other->hash) ? 1 : 0);
}

/**
 * @brief The first place of the order whose hash is not below `hash`, which a binary search finds.
 *
 * @param[in] order The places, sorted by their hash [BORROWS].
 * @param[in] count How many places.
 * @param[in] hash  The hash to find.
 * @return          The place, `count` where every hash is below `hash`.
 */
static size_t exact_place_first(const ExactPlace *order, size_t count, uint64_t hash)
{
    size_t low = 0u;
    size_t high = count;
    while (low < high)
    {
        const size_t middle = low + ((high - low) / 2u);
        if (order[middle].hash < hash)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

size_t orior_exact_agreement(const OriorExactInteger *positions, const uint64_t *values, size_t count,
                             const OriorExactInteger *lag)
{
    return orior_exact_agreement_using(orior_exact_equal, positions, values, count, lag);
}

size_t orior_exact_agreement_using(int (*equal)(const OriorExactInteger *left, const OriorExactInteger *right),
                                   const OriorExactInteger *positions, const uint64_t *values, size_t count,
                                   const OriorExactInteger *lag)
{
    // What the measure requires of the set is membership: is there a point exactly one lag away, and
    // does it carry the same value. The order is of the positions' hashes and not of the positions, so
    // it puts no order on the domain. A binary search of a hash finds the places of that hash.
    //
    // The code does not trust the hash on its own. A full comparison confirms a hit. Two different
    // coordinates that read as equal are the error that this whole path prevents.
    //
    // The Python side keeps its `placed` as a dict that each write replaces at its key. It keeps the
    // last value at a repeated position, and stores each position once. The order marks each entry
    // that a later entry repeats. The count walks the entries that stay, which gives the same count.
    if (count == 0u)
    {
        return 0u;
    }

    // The order and the room its sort writes are two places an entry. Past this bound their size in
    // bytes wraps size_t, and the scan below needs no allocation to answer.
    const size_t widest = SIZE_MAX / (2u * sizeof(ExactPlace));
    if (count > widest)
    {
        return agreement_without_order(equal, positions, values, count, lag);
    }

    // Explicit cast takes the cell of the pool as the places that it holds, the order first and the room after it
    ExactPlace *const order =
        (ExactPlace *)g_exact_guard->temporary_buf_alloc(2u * count * sizeof(ExactPlace), g_exact_owner);
    if (order == NULL)
    {
        // No order. The scan answers instead. Slower and correct beats absent.
        return agreement_without_order(equal, positions, values, count, lag);
    }

    // The first pass: each entry's place, the places sorted once by their hash. The sort is stable, so
    // the entries of one hash keep the order they came in. An entry that a later entry of its hash
    // repeats gets the mark `count`, and each position keeps its last entry.
    for (size_t at = 0u; at < count; at++)
    {
        order[at].hash = orior_exact_hash(&positions[at]);
        order[at].at = at;
    }
    (void)memor.sort(&(MemoriaCfg){.dst = order,
                                   .scratch = order + count,
                                   .count = count,
                                   .bytes = sizeof(ExactPlace),
                                   .order = exact_place_order});
    size_t first = 0u;
    while (first < count)
    {
        size_t end = first + 1u;
        while ((end < count) && (order[end].hash == order[first].hash))
        {
            end++;
        }
        for (size_t earlier = first; earlier < end; earlier++)
        {
            for (size_t later = earlier + 1u; later < end; later++)
            {
                if (equal(&positions[order[later].at], &positions[order[earlier].at]) != 0)
                {
                    order[earlier].at = count;
                    break;
                }
            }
        }
        first = end;
    }

    EXACT_VALUE(moved);
    if (!EXACT_SCRATCH_VALID(moved))
    {
        g_exact_guard->persistent_buf_release(order, g_exact_owner);
        return agreement_without_order(equal, positions, values, count, lag);
    }
    // The second pass: each position that stays moved one lag, and found by its hash
    size_t agreed = 0u;
    for (size_t place = 0u; place < count; place++)
    {
        const size_t at = order[place].at;
        if (at == count)
        {
            continue;
        }
        if (orior_exact_add(&positions[at], lag, moved) != ORIOR_EXACT_OK)
        {
            continue;
        }
        const uint64_t hash = orior_exact_hash(moved);
        for (size_t found = exact_place_first(order, count, hash); (found < count) && (order[found].hash == hash);
             found++)
        {
            if ((order[found].at != count) && (equal(&positions[order[found].at], moved) != 0))
            {
                if (values[order[found].at] == values[at])
                {
                    agreed++;
                }
                break;
            }
        }
    }

    EXACT_VALUE_RELEASE(moved);
    g_exact_guard->persistent_buf_release(order, g_exact_owner);
    return agreed;
}
