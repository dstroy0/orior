// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ranges.c: the distinct slot encodings cut into the ranges the pool reads, as many as the host has processors
#include "msq_internal.h"

// The ranges `words` distinct slot encodings go to the pool in, written to `ranges`, which holds `processors` of them:
// as many ranges as processors, or one an encoding where there are fewer, each of one size but the last, which holds
// what is left. The count written: every encoding stands in one range, and no range is empty
size_t msq_ranges(size_t words, unsigned int processors, MsqWork *ranges)
{
    const size_t most = (words < (size_t)processors) ? words : (size_t)processors;
    const size_t range_size = (most != 0u) ? ((words + most - 1u) / most) : 0u;
    size_t count = 0u;
    for (size_t first = 0u; first < words; first += range_size)
    {
        const size_t size = ((first + range_size) <= words) ? range_size : (words - first);
        ranges[count] = (MsqWork){MSQ_WORK_RANGE, first, size};
        count += 1u;
    }
    return count;
}
