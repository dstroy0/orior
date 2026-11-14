// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notices_internal.h: a notice as the notices' rings hold it, and the rings' counts, which size their part of the
// rings block (notices.c)
#ifndef CLI_NOTICES_INTERNAL_H
#define CLI_NOTICES_INTERNAL_H
#include "memoria_anularis/memoria_anularis.h"
#include "pool.h"

// the notices the list takes at the most, and as many wait at the most; the segments of a ring, a power of two over
// them; the bytes one ring's notices take; and the two rings' bytes, the notices' part of the rings block, the
// list's ring first and the waiting ring after it
#define CLI_NOTICES_LISTED 200u
#define CLI_NOTICES_RING 256u
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_NOTICES_RING) && (CLI_NOTICES_RING >= CLI_NOTICES_LISTED),
                    "a notices ring's segments are a power of two over the notices listed");

// A notice in one movable cell: its first line and a NUL, the lines after its first and a NUL, then its type and a
// NUL; the cell's bytes, where the lines after the first start and where its type starts; whether it is a failure's;
// and the clock's reading as it came
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long size;
    unsigned long long rest_at;
    unsigned long long type_at;
    int failed;
    unsigned long long at_ms;
} CliNotice;

#define CLI_NOTICES_RING_BYTES ((unsigned long long)CLI_NOTICES_RING * sizeof(CliNotice))
#define CLI_NOTICES_RINGS 2u
#define CLI_NOTICES_PART_BYTES ((unsigned long long)CLI_NOTICES_RINGS * CLI_NOTICES_RING_BYTES)

#endif
