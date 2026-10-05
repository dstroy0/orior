// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kcr.h: the crystal's kind and its head, the words after the k-file head in their order
#ifndef KCR_H
#define KCR_H

#define KREP_KIND_CRYSTAL "KCR\0"

#define KREP_CRYSTAL_HEAD_WORDS 12u

typedef enum
{
    KREP_HEAD_EXTENT = 0,
    KREP_HEAD_CHUNKS = 4,
    KREP_HEAD_BITS = 5,
    KREP_HEAD_LANE_OFFSET = 6,
    KREP_HEAD_LEAVES = 7,
    KREP_HEAD_SIDE_BYTES = 8,
    KREP_HEAD_PACKED_BYTES = 9,
    KREP_HEAD_NAMES_BYTES = 10,
    KREP_HEAD_LANE_NODES = 11
} KrepCrystalHead;

#endif
