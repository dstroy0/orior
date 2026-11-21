
// exact_integer_hash.c: the hash and the agreement
#include "exact_integer_internal.h"

uint64_t orior_exact_hash(const OriorExactInteger *value)
{
    // FNV-1a over the limbs, with the sign folded in last so two magnitudes that differ only in
    // sign do not land on one bucket.
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
 * @brief The agreement count without a table, for when the allocation of the table fails.
 *
 * @param[in] equal     Whether two integers have the same value [BORROWS].
 * @param[in] positions Positions that carry values, in any order [BORROWS].
 * @param[in] values    The value located at each position [BORROWS].
 * @param[in] count     How many positions.
 * @param[in] lag       The offset to test [BORROWS].
 * @return              The count orior_exact_agreement_using returns with a table.
 * @note Quadratic in `count` and needs no memory. The count takes a position at its last entry only.
 *       It matches a displaced position to the last entry equal to it, which keeps the last value at a
 *       repeated position.
 */
static size_t agreement_without_table(int (*equal)(const OriorExactInteger *left, const OriorExactInteger *right),
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
    // does it carry the same value. A set has no order. A search that walks one puts on the domain a
    // structure that it does not have. It also pays a full limb comparison at each step.
    //
    // An open addressed table keyed on the hash answers the same question in one probe on average.
    // The code does not trust the hash on its own. A full comparison confirms a hit. Two different
    // coordinates that read as equal are the error that this whole path prevents.
    //
    // The Python side does the same. Its `placed` is a dict that each write replaces at its key. It
    // keeps the last value at a repeated position, and stores each position once. The table below
    // keeps the last entry, and the count walks the table, which gives the same count.
    if (count == 0u)
    {
        return 0u;
    }

    // The bucket count is the power of two at or above twice `count`, which is below four times
    // `count`. Past this bound that product or its size in bytes wraps size_t, and the scan below
    // needs no allocation to answer.
    const size_t widest = SIZE_MAX / (4u * sizeof(size_t));
    if (count > widest)
    {
        return agreement_without_table(equal, positions, values, count, lag);
    }

    size_t slots = 1u;
    while (slots < (count * 2u))
    {
        slots <<= 1u;
    }

    // Explicit cast takes the cell of the pool as the buckets that it holds
    size_t *const table = (size_t *)g_exact_guard->temporary_buf_alloc(slots * sizeof(size_t), g_exact_owner);
    if (table == NULL)
    {
        // No table. The scan answers instead. Slower and correct beats absent.
        return agreement_without_table(equal, positions, values, count, lag);
    }

    for (size_t at = 0u; at < slots; at++)
    {
        table[at] = count;
    }

    const size_t mask = slots - 1u;
    for (size_t at = 0u; at < count; at++)
    {
        size_t slot = (size_t)(orior_exact_hash(&positions[at]) & (uint64_t)mask);
        while ((table[slot] != count) && (equal(&positions[table[slot]], &positions[at]) == 0))
        {
            slot = (slot + 1u) & mask;
        }
        // An empty bucket takes the entry. A bucket that already holds this position takes it too. The
        // table then holds the last entry for each position.
        table[slot] = at;
    }

    EXACT_VALUE(moved);
    if (!EXACT_SCRATCH_VALID(moved))
    {
        g_exact_guard->persistent_buf_release(table, g_exact_owner);
        return agreement_without_table(equal, positions, values, count, lag);
    }
    size_t agreed = 0u;
    for (size_t table_slot = 0u; table_slot < slots; table_slot++)
    {
        const size_t at = table[table_slot];
        if (at == count)
        {
            continue;
        }
        if (orior_exact_add(&positions[at], lag, moved) != ORIOR_EXACT_OK)
        {
            continue;
        }
        size_t slot = (size_t)(orior_exact_hash(moved) & (uint64_t)mask);
        while (table[slot] != count)
        {
            if (equal(&positions[table[slot]], moved) != 0)
            {
                if (values[table[slot]] == values[at])
                {
                    agreed++;
                }
                break;
            }
            slot = (slot + 1u) & mask;
        }
    }

    EXACT_VALUE_RELEASE(moved);
    g_exact_guard->persistent_buf_release(table, g_exact_owner);
    return agreed;
}
